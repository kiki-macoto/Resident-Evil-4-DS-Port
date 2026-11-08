#include <math.h>
#include <string.h>

#include "objects.h"

// Placement (world units). From the stage data, scaled by 1/256:
// - the three gates are the exit quads of stg_data_01.bin's collision data (vertical quads
//   across the roads out of the village; their midpoints are door points b0..b2);
// - the house door is door point b4, on the wall facing +z (the Ganado spawn e7 of
//   mis_data_s01.bin stands 16 units in front of it);
// - the ladders stand at action points a6 and a7, at the edge of the walkways (height ~31.7)
//   found next to them in the stage geometry;
// - the crates are action points a10 and a11 (on the ground against house walls, as the Zeebo
//   version shows them).
// The Zeebo version's stage 1 (all three missions) has no explosive barrels and no blue
// medallions, and its gates and house doors are part of the stage model: the gate leaves and
// the door here only block the way and are not drawn.
static const float gates[3][5] = {   // x0, z0, x1, z1, height
    { 19.8f, 352.8f, 36.3f, 392.1f, 26.6f },
    { 219.0f, 532.4f, 261.7f, 532.1f, 35.5f },
    { 612.7f, 15.9f, 627.3f, 56.0f, 34.9f },
};
static const float door_b4[5] = { 445.6f, 199.3f, 432.6f, 199.3f, 23.0f };
static const float ladders[2][5] = {   // base x, z, direction to the platform x, z, platform height
    { 439.5f, 60.5f, -1.0f, 0.0f, 31.7f },
    { 317.3f, 75.5f, 1.0f, 0.0f, 32.2f },
};
static const float crates[][2] = { { 397.9f, 322.6f }, { 457.7f, 277.6f } };
static const float plaza[2] = { 380.0f, 280.0f };   // the gates swing towards the village

#define CRATE_R      floattof32(3.5)
#define CRATE_H      inttof32(6)
#define BARREL_R     floattof32(3.5)
#define BARREL_H     inttof32(12)
#define MEDAL_R      inttof32(2)
#define MEDAL_DROP   inttof32(9)          // the disc hangs this far below its hang point
#define LEAF_R       floattof32(1.2)      // half the thickness of a gate leaf, for collision
#define SWING_STEP   (inttof32(1) / 14)   // a kicked gate swings open in 14 frames
#define FUSE_FRAMES  20
#define CHAIN_RANGE  inttof32(22)         // barrels this close to a blast go off too
#define BOOM_FRAMES  40
#define PUFF_FRAMES  20
#define DRAW_RANGE   inttof32(220)
#define DOOR_W       inttof32(13)         // the door model (fix_all #1): its leaf, hinge to edge
#define DOOR_H       inttof32(23)
#define DOOR_SWING   (DEGREES_IN_CIRCLE / 4)   // open: a quarter turn (the game's: about 93 degrees)
#define DOOR_STEP    (inttof32(1) / 24)   // it swings open or shut in 24 frames
#define DOOR_HOLD    240                  // ...and stays open 4 s (longer while Leon is in its way)
#define BELL_R       floattof32(3.5)
#define BELL_LEN     inttof32(9)          // the bell model (fix_all #9) hangs this far below its point
#define RING_FRAMES  120
#define HANG_R       inttof32(3)          // the canister (fix_all #5, around its point)
#define HANG_LOW     floattof32(6.6)      // ...reaches this far below its point
#define HANG_G       floattof32(0.08)     // falling, per frame

static int F(float v) { return (int)(v * 4096.0f); }

static void add_leaf(Objects *s, int kind, float hx, float hz, float ex, float ez, float height, int pair)
{
    Obj *o = &s->o[s->count++];
    memset(o, 0, sizeof(*o));
    o->kind = kind;
    o->hinge[0] = F(hx); o->hinge[1] = F(hz);
    o->end[0] = F(ex);   o->end[1] = F(ez);
    o->top = F(height);
    o->pair = pair;
    // Swing the way that takes the free end towards the village (or, for the door, outwards).
    float tx = kind == OBJ_DOOR ? hx + (ex - hx) * 0.5f : plaza[0], tz = kind == OBJ_DOOR ? hz + 10.0f : plaza[1];
    float ux = ex - hx, uz = ez - hz;   // rotating +90 degrees (yaw) turns (ux, uz) into (uz, -ux)... see leaf_dir()
    float px = hx + uz, pz = hz - ux;
    float nx = hx - uz, nz = hz + ux;
    o->swing = (px - tx) * (px - tx) + (pz - tz) * (pz - tz) < (nx - tx) * (nx - tx) + (nz - tz) * (nz - tz) ? 1 : -1;
    o->alive = 1;
    o->fuse = -1;
}

// A ladder's top rests on the platform's edge: the first floor at least 10 above its foot,
// going from the foot towards the platform. end[0]: how far that is (f32); top: its height.
static void ladder_edge(Obj *o, const Ground *g)
{
    o->end[0] = inttof32(16);
    for (int s = 4; s <= 30; s++)
    {
        int h;
        if (ground_height(g, o->pos[0] + o->dir[0] * s, o->pos[2] + o->dir[1] * s, o->pos[1] + inttof32(60), 0, &h)
            && h > o->pos[1] + inttof32(10))
        {
            o->end[0] = inttof32(s);
            o->top = h;
            return;
        }
    }
}

static int ground_at(const Ground *g, int x, int z, int ref)
{
    int y = 0;
    ground_height(g, x, z, ref, inttof32(3), &y);
    return y;
}

void objects_init(Objects *s, const Ground *g, const Model *door, const Model *ladder, const Model *barrel,
                  const Model *crate, const Model *medal, const Sheet *boom_fx, const Sheet *spark_fx)
{
    memset(s, 0, sizeof(*s));
    ground_clear_gaps();
    s->ground = g;
    s->door = door; s->ladder = ladder; s->barrel = barrel; s->crate = crate; s->medal = medal;
    s->boom_fx = boom_fx; s->spark_fx = spark_fx;
    for (int i = 0; i < 3; i++)
    {
        const float *q = gates[i];
        float mx = (q[0] + q[2]) / 2, mz = (q[1] + q[3]) / 2;
        int a = s->count;
        add_leaf(s, OBJ_GATE_LEAF, q[0], q[1], mx, mz, q[4], a + 1);
        add_leaf(s, OBJ_GATE_LEAF, q[2], q[3], mx, mz, q[4], a);
    }
    add_leaf(s, OBJ_DOOR, door_b4[0], door_b4[1], door_b4[2], door_b4[3], door_b4[4], -1);
    for (int i = 0; i < 2; i++)
    {
        Obj *o = &s->o[s->count++];
        memset(o, 0, sizeof(*o));
        const float *l = ladders[i];
        o->kind = OBJ_LADDER;
        o->pos[0] = F(l[0]); o->pos[2] = F(l[1]);
        o->pos[1] = ground_at(g, o->pos[0], o->pos[2], inttof32(2));
        o->dir[0] = F(l[2]); o->dir[1] = F(l[3]);
        o->top = F(l[4]);
        ladder_edge(o, g);
        // The climb (ch_leon_base #3) carries Leon 16 units back along his facing: he
        // faces away from the platform so that "back" leads onto it.
        o->yaw = (int)(atan2f(-l[2], -l[3]) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
        o->pair = -1;
        o->fuse = -1;
    }
    for (unsigned i = 0; i < sizeof(crates) / sizeof(crates[0]); i++)
    {
        Obj *o = &s->o[s->count++];
        memset(o, 0, sizeof(*o));
        o->kind = OBJ_CRATE;
        o->pos[0] = F(crates[i][0]); o->pos[2] = F(crates[i][1]);
        o->pos[1] = ground_at(g, o->pos[0], o->pos[2], inttof32(2));
        o->pair = -1; o->fuse = -1;
        o->drop = -1;
    }
    objects_reset(s);
}

void objects_reset(Objects *s)
{
    for (int i = 0; i < s->count; i++)
    {
        Obj *o = &s->o[i];
        o->open = o->opening = 0;
        o->alive = 1;
        o->fuse = -1;
    }
    for (int i = 0; i < BOOMS_MAX; i++)
        s->boom[i].t = s->puff[i].t = -1;
    s->medals_hit = 0;
}

void objects_clear(Objects *s)
{
    ground_clear_gaps();   // (the last stage's doorways)
    s->count = 0;
    s->cold = 0;
    for (int i = 0; i < BOOMS_MAX; i++)
        s->boom[i].t = s->puff[i].t = -1;
}

int objects_add_barrel(Objects *s, int x, int z, int ref_y)
{
    int y = ref_y - inttof32(6);   // (no floor just there, as under a door on the map's edge: its own height)
    if (s->count >= OBJECTS_MAX)
        return 0;
    ground_height(s->ground, x, z, ref_y, inttof32(3), &y);
    Obj *o = &s->o[s->count++];
    memset(o, 0, sizeof(*o));
    o->kind = OBJ_BARREL;
    o->pos[0] = x; o->pos[1] = y; o->pos[2] = z;
    o->pair = -1;
    o->alive = 1;
    o->fuse = -1;
    o->drop = -1;
    return 1;
}

int objects_add_barrel_at(Objects *s, int x, int y, int z)
{
    if (!objects_add_bell(s, x, y, z))
        return 0;
    Obj *o = &s->o[s->count - 1];
    o->kind = OBJ_BARREL;
    o->fuse = -1;
    return 1;
}

int objects_add_door(Objects *s, int x, int z, int ref_y, int yaw)
{
    if (!objects_add_barrel(s, x, z, ref_y))
        return 0;
    Obj *o = &s->o[s->count - 1];
    o->kind = OBJ_SWING;
    o->hinge[0] = x;
    o->hinge[1] = z;
    o->yaw = yaw;
    o->top = DOOR_H;
    o->swing = 1;
    // A wall right across the doorway: it leads nowhere (the door Leon came in by). It stays shut.
    int mx = x - mulf32(cosLerp(yaw), DOOR_W / 2), mz = z + mulf32(sinLerp(yaw), DOOR_W / 2);
    int px = mx, pz = mz;
    ground_collide(s->ground, &px, &pz, o->pos[1] + inttof32(3), o->pos[1] + inttof32(16), inttof32(5));
    o->locked = px != mx || pz != mz;
    int gap_mx = mx, gap_mz = mz;
    // ...unless both sides have floor clear of walls a little way out: a door set in a narrow
    // doorway (stage 12's, whose frames come within a few units of its middle) leads on.
    if (o->locked)
    {
        int open = 1, nx = sinLerp(yaw), nz = cosLerp(yaw);
        for (int side = -1; side <= 1; side += 2)
        {
            int qx = mx + side * mulf32(nx, inttof32(9)), qz = mz + side * mulf32(nz, inttof32(9)), y, cx, cz;
            if (!ground_height(s->ground, qx, qz, o->pos[1] + inttof32(4), inttof32(4), &y) || y < o->pos[1] - inttof32(6))
                open = 0;
            else
            {
                cx = qx; cz = qz;
                ground_collide(s->ground, &cx, &cz, y + inttof32(3), y + inttof32(16), inttof32(1));
                if (cx != qx || cz != qz)
                    open = 0;
            }
        }
        if (open)
            o->locked = 0;
    }
    // A door that leads on: the walls across its doorway (a room's outline) are left out there.
    if (!o->locked)
        ground_add_gap(gap_mx, gap_mz, -cosLerp(yaw), sinLerp(yaw), DOOR_W / 2 - inttof32(1), inttof32(7));
    return 1;
}

int objects_add_bell(Objects *s, int x, int y, int z)
{
    if (s->count >= OBJECTS_MAX)
        return 0;
    Obj *o = &s->o[s->count++];
    memset(o, 0, sizeof(*o));
    o->kind = OBJ_BELL;
    o->pos[0] = x; o->pos[1] = y; o->pos[2] = z;
    o->pair = -1;
    o->alive = 1;
    o->fuse = 0;
    o->drop = -1;
    return 1;
}

int objects_add_ladder(Objects *s, int x, int z, int ref_y, int yaw)
{
    int y, top;
    int dx = -sinLerp(yaw), dz = -cosLerp(yaw);   // (towards the platform)
    if (s->count >= OBJECTS_MAX || !ground_height(s->ground, x, z, ref_y, inttof32(3), &y)
        || !ground_height(s->ground, x + dx * 16, z + dz * 16, y + inttof32(60), 0, &top) || top < y + inttof32(10))
        return 0;
    Obj *o = &s->o[s->count++];
    memset(o, 0, sizeof(*o));
    o->kind = OBJ_LADDER;
    o->pos[0] = x; o->pos[1] = y; o->pos[2] = z;
    o->dir[0] = dx; o->dir[1] = dz;
    o->top = top;
    o->yaw = yaw;   // climbing, he faces away from the platform (see objects_init())
    o->pair = -1;
    o->fuse = -1;
    o->drop = -1;
    ladder_edge(o, s->ground);
    return 1;
}

int objects_add_hang(Objects *s, int x, int y, int z, int yaw)
{
    if (!objects_add_bell(s, x, y, z))
        return 0;
    Obj *o = &s->o[s->count - 1];
    o->kind = OBJ_HANG;
    o->yaw = yaw;
    // The floor it lands on (end[1]), the first one below it; end[0]: its falling speed.
    o->end[1] = y - inttof32(60);
    ground_height(s->ground, x, z, y - inttof32(8), 0, &o->end[1]);
    return 1;
}

// A swinging door's free edge, where it stands now.
static void door_end(const Obj *o, int *x, int *z)
{
    int a = o->yaw + o->swing * mulf32(o->open, DOOR_SWING);
    *x = o->hinge[0] - mulf32(cosLerp(a), DOOR_W);
    *z = o->hinge[1] + mulf32(sinLerp(a), DOOR_W);
}

void objects_open_door(Objects *s, int i, const int from[3])
{
    Obj *o = &s->o[i];
    if (o->kind != OBJ_SWING || o->open || o->opening)
        return;
    // Turning by +yaw takes the edge towards the door's front (its +z): open away from `from`.
    int fx = sinLerp(o->yaw), fz = cosLerp(o->yaw);
    s64 side = (s64)(from[0] - o->hinge[0]) * fx + (s64)(from[2] - o->hinge[1]) * fz;
    o->swing = side > 0 ? -1 : 1;
    o->opening = 1;
    o->fuse = -1;
}

static int dist_to_leaf(const Obj *o, int x, int z)
{
    int ex, ez;
    door_end(o, &ex, &ez);
    s64 sx = ex - o->hinge[0], sz = ez - o->hinge[1], len2 = sx * sx + sz * sz;
    s64 t = len2 ? (((s64)(x - o->hinge[0]) * sx + (s64)(z - o->hinge[1]) * sz) << 12) / len2 : 0;
    if (t < 0) t = 0;
    if (t > 4096) t = 4096;
    int dx = o->hinge[0] + (int)((sx * t) >> 12) - x, dz = o->hinge[1] + (int)((sz * t) >> 12) - z;
    return (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
}

int objects_door_at(const Objects *s, int x, int z, int reach)
{
    for (int i = 0; i < s->count; i++)
    {
        const Obj *o = &s->o[i];
        if (o->kind == OBJ_SWING && !o->locked && !o->open && !o->opening && dist_to_leaf(o, x, z) < reach)
            return i;
    }
    return -1;
}

int objects_add_crate(Objects *s, int x, int z, int ref_y, int drop)
{
    if (!objects_add_barrel(s, x, z, ref_y))
        return 0;
    Obj *o = &s->o[s->count - 1];
    o->kind = OBJ_CRATE;
    o->drop = drop;
    // On a shelf or a table (stage 10's: furniture without a floor of its own in the ground
    // data): where the mission puts it, not dropped through the furniture to the floor below.
    if (o->pos[1] < ref_y - inttof32(6 + 4))
        o->pos[1] = ref_y - inttof32(6);
    return 1;
}

static void start_fx(void *list, int stride, const int pos[3], int lift)
{
    for (int i = 0; i < BOOMS_MAX; i++)
    {
        int *e = (int *)((u8 *)list + i * stride);   // { pos[3], t }
        if (e[3] >= 0)
            continue;
        e[0] = pos[0]; e[1] = pos[1] + lift; e[2] = pos[2];
        e[3] = 0;
        return;
    }
}

void objects_boom(Objects *s, const int pos[3])
{
    start_fx(s->boom, sizeof(Boom), pos, inttof32(6));
}

void objects_update(Objects *s, ObjEvents *ev)
{
    ev->exploded = 0;
    ev->door = -1;
    ev->small = 0;
    s->tick++;
    for (int i = 0; i < s->count; i++)
    {
        Obj *o = &s->o[i];
        if (o->kind == OBJ_BELL)
        {
            if (o->fuse > 0)
                o->fuse--;
            continue;
        }
        if (o->kind == OBJ_HANG)
        {
            // Shot: it falls, chains and all (the game pushes the whole model down, harder
            // each frame), and blows up where it lands.
            if (o->alive && o->opening)
            {
                o->end[0] += HANG_G;
                o->pos[1] -= o->end[0];
                if (o->pos[1] - HANG_LOW <= o->end[1] && !ev->exploded)
                {
                    o->alive = 0;
                    o->opening = 0;
                    ev->exploded = 1;
                    ev->small = 1;
                    ev->explode_pos[0] = o->pos[0];
                    ev->explode_pos[1] = o->end[1];
                    ev->explode_pos[2] = o->pos[2];
                    start_fx(s->boom, sizeof(Boom), ev->explode_pos, inttof32(6));
                }
            }
            continue;
        }
        if (o->kind == OBJ_SWING)
        {
            // Opening; open for a while; then shutting again (not on Leon).
            if (o->opening == 1 && o->open == 0)
                ev->door = i;
            if (o->opening == 1 && (o->open += DOOR_STEP) >= inttof32(1))
            {
                o->open = inttof32(1);
                o->opening = 0;
                o->fuse = DOOR_HOLD;
            }
            else if (o->fuse > 0 && --o->fuse == 0)
            {
                if (s->leon && o->pos[1] - s->leon[1] < DOOR_H && s->leon[1] - o->pos[1] < DOOR_H
                    && dist_to_leaf(o, s->leon[0], s->leon[2]) < DOOR_W + inttof32(4))
                    o->fuse = 30;   // (he is in its way: later)
                else
                {
                    o->opening = -1;
                    ev->door = i;
                }
            }
            else if (o->opening == -1 && (o->open -= DOOR_STEP) <= 0)
            {
                o->open = 0;
                o->opening = 0;
            }
            continue;
        }
        if (o->opening && (o->open += SWING_STEP) >= inttof32(1))
        {
            o->open = inttof32(1);
            o->opening = 0;
        }
        if (o->kind == OBJ_BARREL && o->alive && o->fuse >= 0 && --o->fuse < 0 && !ev->exploded)
        {
            o->alive = 0;
            ev->exploded = 1;
            memcpy(ev->explode_pos, o->pos, sizeof(ev->explode_pos));
            start_fx(s->boom, sizeof(Boom), o->pos, inttof32(6));
            // Neighbours catch it.
            for (int j = 0; j < s->count; j++)
            {
                Obj *b = &s->o[j];
                int dx = b->pos[0] - o->pos[0], dz = b->pos[2] - o->pos[2];
                if (b->kind == OBJ_BARREL && b->alive && b->fuse < 0 && !s->cold   // (tanks: one at a time)
                    && (s64)dx * dx + (s64)dz * dz < (s64)CHAIN_RANGE * CHAIN_RANGE)
                    b->fuse = 12;
            }
        }
    }
    for (int i = 0; i < BOOMS_MAX; i++)
    {
        if (s->boom[i].t >= 0 && ++s->boom[i].t >= BOOM_FRAMES)
            s->boom[i].t = -1;
        if (s->puff[i].t >= 0 && ++s->puff[i].t >= PUFF_FRAMES)
            s->puff[i].t = -1;
    }
}

static void push_segment(int ax, int az, int bx, int bz, int *x, int *z, int radius)
{
    s64 dx = bx - ax, dz = bz - az, len2 = dx * dx + dz * dz;
    if (len2 == 0)
        return;
    s64 t = (((s64)(*x - ax) * dx + (s64)(*z - az) * dz) << 12) / len2;
    if (t < 0) t = 0;
    if (t > 4096) t = 4096;
    int px = ax + (int)((dx * t) >> 12), pz = az + (int)((dz * t) >> 12);
    int ox = *x - px, oz = *z - pz;
    int d = (int)sqrt64((u64)((s64)ox * ox + (s64)oz * oz));
    if (d >= radius)
        return;
    if (d == 0)
    {
        // Exactly on the line: out along the normal.
        int l = (int)sqrt64((u64)len2);
        *x = px + (int)(-dz * radius / l);
        *z = pz + (int)(dx * radius / l);
        return;
    }
    *x = px + (int)((s64)ox * radius / d);
    *z = pz + (int)((s64)oz * radius / d);
}

static void push_circle(int cx, int cz, int r, int *x, int *z)
{
    int ox = *x - cx, oz = *z - cz;
    int d = (int)sqrt64((u64)((s64)ox * ox + (s64)oz * oz));
    if (d >= r || d == 0)
        return;
    *x = cx + (int)((s64)ox * r / d);
    *z = cz + (int)((s64)oz * r / d);
}

void objects_collide(const Objects *s, int *x, int *z, int feet, int radius)
{
    for (int i = 0; i < s->count; i++)
    {
        const Obj *o = &s->o[i];
        if ((o->kind == OBJ_GATE_LEAF || o->kind == OBJ_DOOR) && o->open < inttof32(1) / 2)
            push_segment(o->hinge[0], o->hinge[1], o->end[0], o->end[1], x, z, radius + LEAF_R);
        else if (o->kind == OBJ_SWING && feet < o->pos[1] + DOOR_H && feet > o->pos[1] - DOOR_H)
        {
            int ex, ez;   // (the leaf where it stands, open or shut)
            door_end(o, &ex, &ez);
            push_segment(o->hinge[0], o->hinge[1], ex, ez, x, z, radius + LEAF_R);
        }
        else if ((o->kind == OBJ_CRATE || o->kind == OBJ_BARREL) && o->alive && feet > o->pos[1] - inttof32(8) && feet < o->pos[1] + (o->kind == OBJ_CRATE ? CRATE_H : BARREL_H))
            push_circle(o->pos[0], o->pos[2], radius + (o->kind == OBJ_CRATE ? CRATE_R : BARREL_R), x, z);
    }
}

static int far_from(const int eye[3], const int pos[3])
{
    int dx = pos[0] - eye[0], dz = pos[2] - eye[2];
    return (s64)dx * dx + (s64)dz * dz > (s64)DRAW_RANGE * DRAW_RANGE;
}

void objects_draw(Objects *s, const int eye[3], u32 poly_fmt)
{
    for (int i = 0; i < s->count; i++)
    {
        Obj *o = &s->o[i];
        int m[1][12];
        memset(m, 0, sizeof(m));
        if (o->kind == OBJ_GATE_LEAF || o->kind == OBJ_DOOR)
            continue;   // part of the stage model (see the top of this file)
        if (o->kind == OBJ_SWING)
        {
            if (!far_from(eye, o->pos))
            {
                int p[3] = { o->hinge[0], o->pos[1], o->hinge[1] };
                model_draw(s->door, p, o->yaw + o->swing * mulf32(o->open, DOOR_SWING), poly_fmt, NULL);
            }
            continue;
        }
        if (o->kind == OBJ_HANG)
        {
            if (s->hang && o->alive && !far_from(eye, o->pos))
                model_draw(s->hang, o->pos, o->yaw, poly_fmt, NULL);
            continue;
        }
        if (o->kind == OBJ_BELL)
        {
            if (s->bell && !far_from(eye, o->pos))
            {
                // Rung: it swings about its point, less and less.
                int a = o->fuse > 0 ? mulf32(sinLerp(o->fuse * 600), o->fuse * (DEGREES_IN_CIRCLE / 12) / RING_FRAMES) : 0;
                int c = cosLerp(a), sn = sinLerp(a);
                m[0][0] = inttof32(1);
                m[0][4] = c;  m[0][5] = sn;
                m[0][7] = -sn; m[0][8] = c;
                model_draw(s->bell, o->pos, 0, poly_fmt, (const int (*)[12])m);
            }
            continue;
        }
        if (o->kind == OBJ_LADDER)
        {
            if (o->drawn || far_from(eye, o->pos))
                continue;
            // The ladder model lies along +z (44 long): its foot at the mission's point, leaning
            // onto the platform's edge (the game's climb, ch_leon_base #3, goes up such a ladder:
            // his hands and feet move a third of a unit in for every unit up), a little beyond it.
            int h = o->top - o->pos[1], e = o->end[0];
            int len = (int)sqrt64((u64)((s64)h * h + (s64)e * e));
            int ch = divf32(h, len), ce = divf32(e, len), sc = divf32(len + inttof32(4), inttof32(44));
            int p[3] = { o->pos[0], o->pos[1], o->pos[2] };
            m[0][0] = o->dir[1];  m[0][1] = -mulf32(o->dir[0], ch); m[0][2] = mulf32(mulf32(o->dir[0], ce), sc);
            m[0][3] = 0;          m[0][4] = ce;                      m[0][5] = mulf32(ch, sc);
            m[0][6] = -o->dir[0]; m[0][7] = -mulf32(o->dir[1], ch); m[0][8] = mulf32(mulf32(o->dir[1], ce), sc);
            model_draw(s->ladder, p, 0, (poly_fmt & ~POLY_CULL_NONE) | POLY_CULL_NONE, (const int (*)[12])m);
        }
        else if (o->alive && !far_from(eye, o->pos))
        {
            const Model *md = o->kind == OBJ_CRATE ? s->crate : o->kind == OBJ_BARREL ? s->barrel : s->medal;
            int yaw = o->kind == OBJ_MEDAL ? (int)(s->tick * 200) : i * 5000;   // medals turn on their chain
            int p[3] = { o->pos[0], o->pos[1], o->pos[2] };
            if (o->kind == OBJ_BARREL && o->fuse >= 0 && (o->fuse & 2))
                p[1] += inttof32(1) / 4;   // about to go: it jitters
            model_draw(md, p, yaw, poly_fmt, NULL);
        }
    }
}

void objects_draw_fx(Objects *s)
{
    for (int i = 0; i < BOOMS_MAX; i++)
    {
        Boom *b = &s->boom[i];
        if (b->t >= 0)
        {
            int size = inttof32(8) + b->t * inttof32(1) / 2;
            int alpha = 31 - b->t * 30 / BOOM_FRAMES;
            if (s->cold)   // liquid nitrogen: a white, slow cloud
            {
                billboard(s->spark_fx, 0, 0, 64, 64, b->pos, size * 2, size * 2, b->t * 150, 0, RGB15(26, 30, 31), alpha, 20 + i, 0);
                billboard(s->spark_fx, 0, 0, 64, 64, b->pos, size * 3, size * 3, -b->t * 100, 0, RGB15(20, 26, 31), alpha / 2 + 1, 24 + i, 0);
                continue;
            }
            billboard(s->boom_fx, 0, 0, 32, 32, b->pos, size, size, b->t * 400, 0, RGB15(31, 24, 12), alpha, 20 + i, 0);
            billboard(s->spark_fx, 0, 0, 64, 64, b->pos, size * 3 / 2, size * 3 / 2, -b->t * 300, 0, RGB15(31, 20, 6), alpha / 2 + 1, 24 + i, 0);
        }
        Puff *p = &s->puff[i];
        if (p->t >= 0)
        {
            int size = inttof32(3) + p->t * floattof32(0.3);
            billboard(s->spark_fx, 0, 0, 64, 64, p->pos, size, size, p->t * 500, 0, RGB15(24, 16, 8), 31 - p->t * 30 / PUFF_FRAMES, 28 + i, POLY_FOG);
        }
    }
}

int objects_raycast(const Objects *s, const int p[3], const int d[3], int *t_best)
{
    int found = -1;
    s64 aa = ((s64)d[0] * d[0] + (s64)d[2] * d[2]) >> 12;
    for (int i = 0; i < s->count; i++)
    {
        const Obj *o = &s->o[i];
        if (!o->alive || (o->kind != OBJ_CRATE && o->kind != OBJ_BARREL && o->kind != OBJ_MEDAL && o->kind != OBJ_BELL && !(o->kind == OBJ_HANG && !o->opening)))
            continue;
        int r = o->kind == OBJ_CRATE ? CRATE_R : o->kind == OBJ_BARREL ? BARREL_R : o->kind == OBJ_BELL ? BELL_R : o->kind == OBJ_HANG ? HANG_R : MEDAL_R;
        int y0 = o->kind == OBJ_MEDAL ? o->pos[1] - MEDAL_DROP - MEDAL_R : o->kind == OBJ_BELL ? o->pos[1] - BELL_LEN : o->kind == OBJ_HANG ? o->pos[1] - HANG_LOW : o->pos[1];
        // Crates and barrels are below the gun: their hit box reaches a little above them.
        int y1 = o->kind == OBJ_MEDAL ? o->pos[1] - MEDAL_DROP + MEDAL_R : o->kind == OBJ_BELL || o->kind == OBJ_HANG ? o->pos[1] + inttof32(2)
               : o->pos[1] + (o->kind == OBJ_CRATE ? CRATE_H : BARREL_H) + inttof32(3);
        if (aa == 0)
            continue;
        // Vertical cylinder test, as for the Ganados (scaled by 4096 to stay in 64 bits).
        s64 fx = p[0] - o->pos[0], fz = p[2] - o->pos[2];
        s64 b = (fx * d[0] + fz * d[2]) >> 12;
        s64 c = ((fx * fx + fz * fz) >> 12) - (((s64)r * r) >> 12);
        s64 disc = b * b - aa * c;
        if (disc < 0)
            continue;
        s64 root = (s64)sqrt64(disc);
        s64 t = ((-b - root) << 12) / aa;
        if (t <= 0 && ((-b + root) << 12) / aa > 0)
            t = 1;
        if (t <= 0 || t >= *t_best)
            continue;
        s64 y = p[1] + ((d[1] * t) >> 12);
        if (y < y0 || y > y1)
            continue;
        *t_best = (int)t;
        found = i;
    }
    return found;
}

int objects_hit(Objects *s, int i)
{
    Obj *o = &s->o[i];
    if (!o->alive)
        return 0;
    if (o->kind == OBJ_BARREL)
    {
        if (o->fuse >= 0)
            return 0;
        o->fuse = FUSE_FRAMES;
        return 1;
    }
    if (o->kind == OBJ_CRATE)
    {
        o->alive = 0;
        start_fx(s->puff, sizeof(Puff), o->pos, inttof32(3));
        return 1;
    }
    if (o->kind == OBJ_HANG)
    {
        o->opening = 1;   // down it comes
        return 1;
    }
    if (o->kind == OBJ_BELL)
    {
        o->fuse = RING_FRAMES;   // (it rings: the game makes the noise)
        return 1;
    }
    if (o->kind == OBJ_MEDAL)
    {
        o->alive = 0;
        s->medals_hit++;
        int p[3] = { o->pos[0], o->pos[1] - MEDAL_DROP, o->pos[2] };
        start_fx(s->puff, sizeof(Puff), p, 0);
        return 1;
    }
    return 0;
}

int objects_action(const Objects *s, const int pos[3], int feet, int yaw, int *which)
{
    int fx = sinLerp(yaw), fz = cosLerp(yaw);
    for (int i = 0; i < s->count; i++)
    {
        const Obj *o = &s->o[i];
        if ((o->kind == OBJ_GATE_LEAF) && o->open == 0 && !o->opening)
        {
            // Close to the gate (the nearest point of the leaf) and facing it.
            s64 sx = o->end[0] - o->hinge[0], sz = o->end[1] - o->hinge[1], len2 = sx * sx + sz * sz;
            s64 t = len2 ? (((s64)(pos[0] - o->hinge[0]) * sx + (s64)(pos[2] - o->hinge[1]) * sz) << 12) / len2 : 0;
            if (t < 0) t = 0;
            if (t > 4096) t = 4096;
            int dx = o->hinge[0] + (int)((sx * t) >> 12) - pos[0], dz = o->hinge[1] + (int)((sz * t) >> 12) - pos[2];
            int d = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
            if (d < inttof32(10) && d > 0 && ((s64)fx * dx + (s64)fz * dz) >> 12 > (s64)d / 2)
            {
                *which = i;
                return ACT_KICK_GATE;
            }
        }
        else if (o->kind == OBJ_SWING && !o->open && !o->opening && feet < o->pos[1] + inttof32(4) && feet > o->pos[1] - inttof32(4))
        {
            // Close to the shut door and facing it.
            int mx = o->hinge[0] - mulf32(cosLerp(o->yaw), DOOR_W / 2), mz = o->hinge[1] + mulf32(sinLerp(o->yaw), DOOR_W / 2);
            int dx = mx - pos[0], dz = mz - pos[2];
            int d = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
            if (d < inttof32(12) && d > 0 && ((s64)fx * dx + (s64)fz * dz) >> 12 > (s64)d / 2)
            {
                *which = i;
                return o->locked ? ACT_KICK_GATE : ACT_OPEN_DOOR;
            }
        }
        else if (o->kind == OBJ_CRATE && o->alive)
        {
            int dx = o->pos[0] - pos[0], dz = o->pos[2] - pos[2];
            int d = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
            if (d < inttof32(9) && d > 0 && feet > o->pos[1] - inttof32(8) && feet < o->pos[1] + CRATE_H && ((s64)fx * dx + (s64)fz * dz) >> 12 > (s64)d * 3 / 5)
            {
                *which = i;
                return ACT_KICK_CRATE;
            }
        }
        else if (o->kind == OBJ_LADDER)
        {
            // Bottom: at the base facing the platform. Top: at the landing facing the edge.
            int dx = o->pos[0] - pos[0], dz = o->pos[2] - pos[2];
            int d = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
            int facing = ((s64)fx * o->dir[0] + (s64)fz * o->dir[1]) >> 12;
            if (d < inttof32(7) && feet < o->pos[1] + inttof32(4) && facing > inttof32(1) / 2)
            {
                *which = i;
                return ACT_LADDER_UP;
            }
            int lx = o->pos[0] + o->dir[0] * 16 - pos[0], lz = o->pos[2] + o->dir[1] * 16 - pos[2];
            int ld = (int)sqrt64((u64)((s64)lx * lx + (s64)lz * lz));
            if (ld < inttof32(7) && feet > o->top - inttof32(4) && facing < -inttof32(1) / 3)
            {
                *which = i;
                return ACT_LADDER_DOWN;
            }
        }
    }
    return ACT_NONE;
}

void objects_open(Objects *s, int i)
{
    Obj *o = &s->o[i];
    if (o->open || o->opening)
        return;
    o->opening = 1;
    if (o->pair >= 0)
        s->o[o->pair].opening = 1;
}

int objects_door(const Objects *s)
{
    for (int i = 0; i < s->count; i++)
        if (s->o[i].kind == OBJ_DOOR)
            return i;
    return -1;
}
