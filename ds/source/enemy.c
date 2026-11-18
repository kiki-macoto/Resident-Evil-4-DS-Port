#include "enemy.h"

#define ANIM_IDLE  0
#define ANIM_WALK  1
#define ANIM_HIT   2
#define ANIM_DEATH 3
#define ANIM_SWIPE 4
#define ANIM_LUNGE 5
#define ANIM_HOLD  6
static const u8 state_anim[] = { ANIM_IDLE, ANIM_WALK, ANIM_SWIPE, ANIM_LUNGE, ANIM_HOLD,
                                 ANIM_HIT, ANIM_DEATH, ANIM_DEATH };

#define LEGS_TO       floattof32(7.5)
#define FAR_STEP      inttof32(100)
#define HAND_NODE     14
#define PLG_NECK_NODE 18
#define PLG_HEAD_NODE 19
#ifndef PLG_IDLE
#define PLG_IDLE      0
#define PLG_ATTACK    2
#define PLG_HIT       1
#define PLG_DEATH     4
#endif
#define PLG_TOP       inttof32(26)
#define SIGHT         inttof32(120)
#define STOP          inttof32(7)
#define WALK_SPEED    (inttof32(90) / 768)
#define TURN_SPEED    (500 / 6)
#define RADIUS        floattof32(2.2)
#define WALL_RADIUS   floattof32(3.2)
#define BODY_RADIUS   inttof32(6)
#define HEIGHT        inttof32(17)
#define HEAD_FROM     floattof32(14.5)
#define STEP_UP       inttof32(3)
#define ANIM_STEP     (inttof32(20) / 60)

typedef struct { u8 from, to; s16 speed; } AtkStep;
typedef struct { u8 from, to; u16 dist; s16 lo, hi, ang; } AtkHit;
typedef struct {
    u8 start, start_ang;
    u8 turn_from, turn_to; s16 turn;
    u8 grab;
    AtkHit hit;
    u8 steps; AtkStep step[3];
} Attack;
enum { ATK_QUICK_GRAB, ATK_LUNGE, ATK_HATCHET, ATK_FORK, ATK_CULT_GRAB, ATK_CULT_LUNGE, ATK_CULT_SICKLE, ATK_CULT_BOARD, ATK_XBOW };
static const Attack attacks[] = {
    { 12, 0, 1, 11, 400, 1, { 14, 16, 2500, 3500, 4000, 1000 }, 2, { { 11, 16, 250 }, { 17, 20, 90 } } },
    { 30, 35, 1, 16, 300, 1, { 14, 30, 2500, 3500, 4000, 1000 }, 3, { { 0, 9, 50 }, { 10, 34, 320 }, { 35, 38, 150 } } },
    { 12, 0, 1, 6, 0, 0, { 13, 13, 3200, 1000, 5600, 3200 }, 3, { { 4, 9, 90 }, { 10, 16, 270 }, { 47, 56, -90 } } },
    { 22, 0, 1, 8, 0, 0, { 21, 21, 5120, 2000, 4000, 1500 }, 1, { { 16, 31, 220 } } },
    { 12, 0, 1, 11, 500, 1, { 14, 16, 2500, 3500, 4000, 1000 }, 2, { { 11, 16, 300 }, { 17, 20, 120 } } },
    { 32, 35, 1, 16, 400, 1, { 14, 30, 2500, 3500, 4000, 1000 }, 3, { { 0, 9, 80 }, { 10, 34, 400 }, { 35, 38, 180 } } },
    { 16, 0, 1, 12, 0, 0, { 20, 21, 4300, 500, 5000, 5000 }, 1, { { 16, 23, 400 } } },
    { 20, 0, 1, 10, 0, 0, { 20, 22, 1800, 1000, 3500, 2000 }, 2, { { 1, 10, 150 }, { 12, 29, 450 } } },
    { 0, 0, 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, 0, { { 0, 0, 0 } } },
};
#define LEON_WIDTH    1000
#define GAME_UNITS(v) ((v) * 16)
#define ENMITY_MAX    3000
#define RELEASE_PUSH  (inttof32(3000) / 256)
#define SHOVE_FRAMES  10
#define BITE_A        6
#define BITE_B        10

typedef struct { int set, idle, walk, swing; } WeaponClips;
static const WeaponClips weapon_clips[WEAPON_KINDS] = {
    { -1, 0, 0, 0 }, { 0, 0, 1, 4 }, { 0, 0, 1, 4 }, { 1, 0, 1, 2 }, { 2, 0, 1, 5 }, { 3, 0, 1, 5 }, { -1, 0, 0, 0 } };
#define XBOW_RANGE    inttof32(80)
#define XBOW_EVERY    200
#define XBOW_LOOSE    inttof32(4)
#define XBOW_KEEP     inttof32(25)
#define DAZE_FRAMES   90
#define KICK_RANGE    inttof32(10)
#define KICK_PUSH     inttof32(4)
#define THROW_RANGE   inttof32(90)
#define THROW_EVERY   200

#ifndef BOSS_IDLE
#define BOSS_IDLE     0
#define BOSS_WALK     1
#define BOSS_SWING    4
#endif
#define BOSS_SPEED    (inttof32(130) / 768)
#define BOSS_REACH    floattof32(12.5)

static int set_len(const AnimSet *a, int anim)
{
    return inttof32(a->frames[anim] > 0 ? a->frames[anim] : 1);
}

static int anim_len(const Enemies *s, int anim)
{
    return set_len(s->anims, anim);
}

static const AnimSet *clip_of(const Enemies *s, const Enemy *e, int *anim)
{
    if (e->boss && e->state <= ENEMY_SWIPE)
    {
        *anim = e->state == ENEMY_IDLE ? BOSS_IDLE : e->state == ENEMY_WALK ? BOSS_WALK : BOSS_SWING;
        return s->boss_anims;
    }
    const WeaponClips *w = &weapon_clips[e->weapon];
    if (e->weapon && w->set >= 0 && s->weapon_anims[w->set] && e->state <= ENEMY_SWIPE)
    {
        *anim = e->state == ENEMY_IDLE ? w->idle : e->state == ENEMY_WALK ? w->walk : w->swing;
        return s->weapon_anims[w->set];
    }
    *anim = state_anim[e->state];
    return s->anims;
}

static int state_len(const Enemies *s, const Enemy *e)
{
    int anim;
    const AnimSet *a = clip_of(s, e, &anim);
    return set_len(a, anim);
}

static u32 rnd(Enemies *s)
{
    s->rng = s->rng * 1664525 + 1013904223;
    return s->rng >> 16;
}

#define CORPSE_FRAMES 300
#define FALL_MIN      inttof32(6)
#define GRAVITY       floattof32(0.06)

void enemies_init(Enemies *s, const Model *m, const AnimSet *a, const Ground *g, const Objects *o,
                  const int (*xyz)[3], int count, int yaw)
{
    s->model = m;
    s->anims = a;
    s->ground = g;
    s->objects = o;
    s->count = count > ENEMY_MAX ? ENEMY_MAX : count;
    s->rng = 12345;
    for (int i = 0; i < s->count; i++)
    {
        enemies_respawn(s, i, xyz[i], yaw);
        s->e[i].active = 0;
        s->e[i].frame = inttof32(i * 7);
        s->e[i].drop = -1;
    }
    s->attacker = -1;
    s->rest = 0;
    s->kills = 0;
    s->tick = 0;
    s->sight = SIGHT;
    s->watch = 0;
    s->noise = 0;
    s->ally = NULL;
}

void enemies_perch(Enemies *s, int i, int y)
{
    Enemy *e = &s->e[i];
    e->feet = inttof32(y);
    e->pos[1] = e->feet - s->model->bbox_min[1];
}

static int sees(const Enemies *s, const Enemy *e, const int t[3])
{
    int from[3] = { e->pos[0], e->feet + inttof32(14), e->pos[2] };
    int to[3] = { t[0], t[1] + inttof32(6), t[2] };
    int d[3] = { to[0] - from[0], to[1] - from[1], to[2] - from[2] };
    int len = (int)sqrt64((u64)((s64)d[0] * d[0] + (s64)d[1] * d[1] + (s64)d[2] * d[2]));
    if (len <= 0)
        return 1;
    for (int k = 0; k < 3; k++)
        d[k] = divf32(d[k], len);
    int hit[3];
    return !ground_raycast(s->ground, from, d, len - inttof32(3), hit, RAY_SHOT);
}

void enemies_make_thrower(Enemies *s, int i)
{
    s->e[i].thrower = 1;
    s->e[i].weapon = WEAPON_NONE;
    s->e[i].cooldown = 120 + i * 30;
}

void enemies_set_look(Enemies *s, const Model *m, const AnimSet *a, const AnimSet *one_hand)
{
    s->model = m;
    s->anims = a;
    s->weapon_anims[0] = one_hand;
    for (int i = 0; i < s->count; i++)
        s->e[i].pose_dirty = 1;
}

void enemies_spawn_boss(Enemies *s, int i, const int xyz[3], int yaw, const Model *m, const AnimSet *a)
{
    s->boss_model = m;
    s->boss_anims = a;
    enemies_respawn(s, i, xyz, yaw);
    Enemy *e = &s->e[i];
    e->boss = 1;
    e->weapon = WEAPON_NONE;
    enemies_set_kind(s, i, &foe_kind_chainsaw[0]);
    e->pose_dirty = 1;
}

static int is_cultist(const Enemy *e);

void enemies_arm_cultists(Enemies *s,const Model *board, const Model *xbow, const Model *staff, const AnimSet *w1, const AnimSet *w2)
{
    s->weapon_model[WEAPON_BOARD] = board;
    s->weapon_model[WEAPON_XBOW] = xbow;
    s->weapon_model[WEAPON_STAFF] = staff;
    s->weapon_anims[2] = w1;
    s->weapon_anims[3] = w2;
    for (int i = 0; i < s->count; i++)
        s->e[i].pose_dirty = 1;
}

int enemies_blow(const Enemy *e)
{
    if (e->attack == ATK_CULT_BOARD)
        return 480;
    if (e->attack == ATK_XBOW)
        return 240;
    return is_cultist(e) ? 800 : 380;
}

void enemies_arm(Enemies *s, const Model *const models[WEAPON_KINDS], const AnimSet *one_hand, const AnimSet *fork)
{
    for (int k = 0; k < WEAPON_STAFF + 1 && k < WEAPON_KINDS; k++)
        if (k <= WEAPON_FORK)
            s->weapon_model[k] = models[k];
    s->weapon_anims[0] = one_hand;
    s->weapon_anims[1] = fork;
}

void enemies_respawn(Enemies *s, int i, const int xyz[3], int yaw)
{
    Enemy *e = &s->e[i];
    e->pos[0] = inttof32(xyz[0]);
    e->pos[2] = inttof32(xyz[2]);
    e->feet = 0;
    ground_height(s->ground, e->pos[0], e->pos[2], inttof32(xyz[1]), STEP_UP, &e->feet);
    e->pos[1] = e->feet - s->model->bbox_min[1];
    e->active = 1;
    e->dead_frames = 0;
    e->falling = 0;
    e->yaw = yaw;
    e->state = ENEMY_IDLE;
    e->frame = 0;
    e->boss = 0;
    e->thrower = 0;
    enemies_set_kind(s, i, &foe_kind_ganado[0]);
    e->attack = 0;
    e->enmity = 0;
    e->shove = 0;
    e->cooldown = 0;
    e->struck = 0;
    e->spotted = 0;
    e->spot_told = 0;
    e->daze = 0;
    e->boss = 0;
    e->thrower = 0;
    e->stuck = e->detour = 0;
    e->climb = 0;
    e->detour_side = (i & 1) ? 1 : -1;
    e->pose_dirty = 1;
}

static void set_state(Enemies *s, int i, int state)
{
    if (state == ENEMY_DYING && s->e[i].state < ENEMY_DYING)
        s->kills++;
    Enemy *e = &s->e[i];
    if (state == ENEMY_DYING && !e->thrower)
    {
        ground_collide_n(s->ground, &e->pos[0], &e->pos[2], e->feet + STEP_UP, e->feet + HEIGHT, BODY_RADIUS, 2);
        ground_height(s->ground, e->pos[0], e->pos[2], e->feet + STEP_UP, STEP_UP * 2, &e->feet);
        e->pos[1] = e->feet - s->model->bbox_min[1];
    }
    if (s->attacker == i && state != ENEMY_SWIPE && state != ENEMY_LUNGE && state != ENEMY_GRAB)
        s->attacker = -1;
    if (state != ENEMY_HIT)
        e->daze = 0;
    e->state = state;
    e->frame = 0;
    e->struck = 0;
    e->pose_dirty = 1;
}

static void update_pose(Enemies *s, Enemy *e)
{
    int anim;
    const AnimSet *a = clip_of(s, e, &anim);
    int frame = e->frame;
    if (e->state == ENEMY_DEAD)
        frame = anim_len(s, ANIM_DEATH) - 1;
    anim_pose(a, anim, frame, s->model, e->pose);
    if (e->plaga)
        for (int k = 0; k < 9; k++)
            e->pose[PLG_HEAD_NODE][k] = 0;
    e->pose_dirty = 0;
}

static void plaga_step(Enemies *s, Enemy *e)
{
    int clip = e->state == ENEMY_SWIPE || e->state == ENEMY_LUNGE || e->state == ENEMY_GRAB ? PLG_ATTACK
               : e->state == ENEMY_HIT ? PLG_HIT : e->state >= ENEMY_DYING ? PLG_DEATH : PLG_IDLE;
#ifdef TEST_PLG_CLIP
    clip = TEST_PLG_CLIP;
#endif
    if (clip != e->plg_clip)
    {
        e->plg_clip = clip;
        e->plg_frame = 0;
    }
    if (!s->plaga_anims)
        return;
    int len = inttof32(s->plaga_anims->frames[clip]);
    e->plg_frame += ANIM_STEP;
    if (e->plg_frame >= len)
        e->plg_frame = clip == PLG_DEATH ? len - inttof32(1) : e->plg_frame - len;
}

void enemies_set_plaga(Enemies *s, const Model *m, const AnimSet *a)
{
    s->plaga_model = m;
    s->plaga_anims = m && a && a->nodes == m->node_count && a->count > PLG_DEATH ? a : NULL;
    if (!s->plaga_anims)
        s->plaga_model = NULL;
}

static void move(Enemies *s, Enemy *e, int dx, int dz)
{
    e->pos[0] += dx;
    e->pos[2] += dz;
    ground_collide_n(s->ground, &e->pos[0], &e->pos[2], e->feet + STEP_UP, e->feet + HEIGHT, WALL_RADIUS, 2);
    if (s->objects)
        objects_collide(s->objects, &e->pos[0], &e->pos[2], e->feet, RADIUS);
    if (e->falling)
        return;
    int floor = e->feet;
    ground_height(s->ground, e->pos[0], e->pos[2], e->feet, STEP_UP, &floor);
    if (floor < e->feet - FALL_MIN)
    {
        e->falling = 1;
        e->fall_to = floor;
        e->fall_vy = 0;
        return;
    }
    e->feet = floor;
    e->pos[1] = e->feet - s->model->bbox_min[1];
}

#define STUCK_FRAMES  20
#define DETOUR_FRAMES 70
#define PROBE         inttof32(8)
#define LADDER_BACK   inttof32(16)
#define LADDER_NEAR   inttof32(3)
#define LADDER_SEEK   inttof32(120)
#define LADDER_FRAMES 75
#define LADDER_LEVEL  inttof32(10)
static int ladder_for(const Enemies *s, const Enemy *e, int up, int at[3], int other[3])
{
    if (!s->objects)
        return 0;
    int best = -1;
    s64 best_d = (s64)LADDER_SEEK * LADDER_SEEK;
    for (int i = 0; i < s->objects->count; i++)
    {
        const Obj *o = &s->objects->o[i];
        if (o->kind != OBJ_LADDER)
            continue;
        int foot[3] = { o->pos[0], o->pos[1], o->pos[2] };
        int top[3] = { o->pos[0] + mulf32(o->dir[0], LADDER_BACK), o->top, o->pos[2] + mulf32(o->dir[1], LADDER_BACK) };
        const int *from = up ? foot : top, *to = up ? top : foot;
        if (e->feet - from[1] > LADDER_LEVEL || from[1] - e->feet > LADDER_LEVEL)
            continue;
        s64 dx = from[0] - e->pos[0], dz = from[2] - e->pos[2], d = dx * dx + dz * dz;
        if (d < best_d)
        {
            best_d = d;
            best = i;
            for (int k = 0; k < 3; k++)
            {
                at[k] = from[k];
                other[k] = to[k];
            }
        }
    }
    return best >= 0;
}

static int pick_detour(Enemies *s, Enemy *e, int dx, int dz, int dist)
{
    if (dist <= 0)
        return 0;
    int ux = divf32(dx, dist), uz = divf32(dz, dist);
    static const int turn[] = { 45, 90, 135, 180 };
    for (int k = 0; k < 4; k++)
        for (int j = 0; j < 2; j++)
        {
            int side = j == 0 ? e->detour_side : -e->detour_side;
            if (turn[k] == 180 && j)
                break;
            int a = degreesToAngle(turn[k]) * side;
            int ca = cosLerp(a), sa = sinLerp(a);
            int vx = mulf32(ux, ca) + mulf32(uz, sa), vz = mulf32(uz, ca) - mulf32(ux, sa);
            int x = e->pos[0] + mulf32(vx, PROBE), z = e->pos[2] + mulf32(vz, PROBE);
            ground_collide_n(s->ground, &x, &z, e->feet + STEP_UP, e->feet + HEIGHT, WALL_RADIUS, 2);
            int got = mulf32(x - e->pos[0], vx) + mulf32(z - e->pos[2], vz);
            if (got < PROBE * 3 / 4)
                continue;
            int floor = e->feet;
            if (ground_height(s->ground, x, z, e->feet, STEP_UP, &floor) && floor < e->feet - FALL_MIN)
                continue;
            e->detour_dir[0] = vx;
            e->detour_dir[1] = vz;
            e->detour_side = side;
            e->detour = DETOUR_FRAMES;
            return 1;
        }
    e->detour_side = -e->detour_side;
    return 0;
}

void enemies_activate(Enemies *s, int i, const int *at)
{
    Enemy *e = &s->e[i];
    if (at)
    {
        e->pos[0] = at[0];
        e->pos[2] = at[1];
        ground_height(s->ground, e->pos[0], e->pos[2], e->feet, STEP_UP, &e->feet);
        e->pos[1] = e->feet - s->model->bbox_min[1];
    }
    e->active = 1;
    e->pose_dirty = 1;
}

int enemies_active_standing(const Enemies *s)
{
    int n = 0;
    for (int i = 0; i < s->count; i++)
        n += s->e[i].active && s->e[i].state < ENEMY_DYING;
    return n;
}

int enemies_left(const Enemies *s)
{
    int n = 0;
    for (int i = 0; i < s->count; i++)
        n += s->e[i].state < ENEMY_DYING;
    return n;
}

static int within_angle(int dist, s64 dot, int rot)
{
    if (rot >= 32767)
        return 1;
    return dot >= (((s64)dist * cosLerp(rot / 2)) >> 12);
}
static int notices(const Enemies *s, const Enemy *e, int dist, s64 dot)
{
    const FoeKind *k = e->kind;
    int gap = dist - GAME_UNITS(k->width + LEON_WIDTH + 300);
    if (gap <= 0)
        return 1;
    if (k->look_dis > 0 && gap <= GAME_UNITS(k->look_dis) && within_angle(dist, dot, k->look_rot))
        return 1;
    return s->noise && gap <= GAME_UNITS(k->search_dis) && within_angle(dist, dot, k->search_rot);
}

static int is_cultist(const Enemy *e)
{
    return e->kind >= foe_kind_cultist && e->kind < foe_kind_cultist + foe_kind_cultist_count;
}

static int pick_attack(const Enemy *e, int dist, int on_ally)
{
    if (is_cultist(e))
    {
        if (e->weapon == WEAPON_XBOW)
            return -1;
        if (e->weapon == WEAPON_BOARD)
            return ATK_CULT_BOARD;
        return e->weapon && e->weapon != WEAPON_STAFF ? ATK_CULT_SICKLE
               : dist < inttof32(attacks[ATK_CULT_GRAB].start) || on_ally ? ATK_CULT_GRAB : ATK_CULT_LUNGE;
    }
    if (e->weapon == WEAPON_FORK)
        return ATK_FORK;
    if (e->weapon)
        return ATK_HATCHET;
    return dist < inttof32(attacks[ATK_QUICK_GRAB].start) || on_ally ? ATK_QUICK_GRAB : ATK_LUNGE;
}

static int in_area(const Enemy *e, const int t[3], int dist, s64 cross, s64 dot, const AtkHit *h)
{
    if (dist >= GAME_UNITS(h->dist + LEON_WIDTH))
        return 0;
    int dy = t[1] - e->pos[1];
    if (GAME_UNITS(h->lo) >= dy + GAME_UNITS(4800) || GAME_UNITS(h->hi) <= dy)
        return 0;
    if (h->ang < 16384 && dot <= 0)
        return 0;
    s64 lim = (((s64)dist * sinLerp(h->ang >= 16384 ? 8192 : h->ang / 2)) >> 12) + GAME_UNITS(LEON_WIDTH);
    return cross < lim && -cross < lim;
}

void enemies_update(Enemies *s, const int target[3], int vulnerable, EnemyEvents *ev)
{
    ev->swiped_by = ev->grabbed_by = ev->bite = ev->spotted = ev->attacking = ev->swing = ev->ally_hit = ev->thrown = ev->shot = -1;
    if (s->rest > 0)
        s->rest--;
    for (int i = 0; i < s->count; i++)
    {
        Enemy *e = &s->e[i];
        if (!e->active)
            continue;
        if (e->plaga)
            plaga_step(s, e);
        if (e->state == ENEMY_DEAD)
        {
            if (++e->dead_frames >= CORPSE_FRAMES)
                e->active = 0;
            continue;
        }
        if (e->cooldown > 0)
            e->cooldown--;
        if (s->attacker >= 0 && (e->enmity -= e->kind->duty) < 0)
            e->enmity = 0;
        if ((e->enmity += e->kind->conc) > ENMITY_MAX)
            e->enmity = ENMITY_MAX;
        if (e->falling)
        {
            e->fall_vy += GRAVITY;
            e->feet -= e->fall_vy;
            if (e->feet <= e->fall_to)
            {
                e->feet = e->fall_to;
                e->falling = 0;
            }
            e->pos[1] = e->feet - s->model->bbox_min[1];
        }

        int dx = target[0] - e->pos[0], dz = target[2] - e->pos[2];
        int dist = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
        const int *t = target;
        int on_ally = 0;
        if (s->ally)
        {
            int ax = s->ally[0] - e->pos[0], az = s->ally[2] - e->pos[2];
            int ad = (int)sqrt64((u64)((s64)ax * ax + (s64)az * az));
            if (ad < dist)
            {
                dx = ax;
                dz = az;
                dist = ad;
                t = s->ally;
                on_ally = 1;
            }
        }
        if (e->climb > 0)
        {
            int k = LADDER_FRAMES - e->climb + 1;
            e->pos[0] = e->climb_from[0] + (e->climb_to[0] - e->climb_from[0]) * k / LADDER_FRAMES;
            e->pos[2] = e->climb_from[2] + (e->climb_to[2] - e->climb_from[2]) * k / LADDER_FRAMES;
            e->feet = e->climb_from[1] + (e->climb_to[1] - e->climb_from[1]) * k / LADDER_FRAMES;
            e->pos[1] = e->feet - s->model->bbox_min[1];
            e->frame += ANIM_STEP;
            e->climb--;
            e->pose_dirty = 1;
            continue;
        }
        if (e->spotted && !e->thrower && !e->boss && !e->falling && (e->state == ENEMY_IDLE || e->state == ENEMY_WALK))
        {
            int tlevel = t[1] - e->pos[1], at[3], other[3];
            if ((tlevel > LADDER_LEVEL || tlevel < -LADDER_LEVEL) && ladder_for(s, e, tlevel > 0, at, other))
            {
                int lx = at[0] - e->pos[0], lz = at[2] - e->pos[2];
                int ld = (int)sqrt64((u64)((s64)lx * lx + (s64)lz * lz));
                if (ld < LADDER_NEAR)
                {
                    for (int c = 0; c < 3; c++)
                    {
                        e->climb_from[c] = c == 1 ? e->feet : e->pos[c];
                        e->climb_to[c] = other[c];
                    }
                    e->climb = LADDER_FRAMES;
                    e->detour = e->stuck = 0;
                    if (e->state != ENEMY_WALK)
                        set_state(s, i, ENEMY_WALK);
                    continue;
                }
                dx = lx;
                dz = lz;
                dist = ld > STOP + inttof32(1) ? ld : STOP + inttof32(1);
            }
        }
        int fx = sinLerp(e->yaw), fz = cosLerp(e->yaw);
        s64 cross = ((s64)fx * dz - (s64)fz * dx) >> 12;
        s64 dot = ((s64)fx * dx + (s64)fz * dz) >> 12;
        int level = t[1] - e->pos[1];
        int same_level = !e->falling && level < inttof32(8) && level > -inttof32(8);
        int in_reach = e->boss && dist < BOSS_REACH && dot * 4 > (s64)dist * 3 && same_level;

        if (e->thrower && e->state <= ENEMY_SWIPE)
        {
            if (dist > 0 && (cross < -(s64)dist / 28 || (dot < 0 && cross <= 0)))
                e->yaw += TURN_SPEED;
            else if (dist > 0 && (cross > (s64)dist / 28 || dot < 0))
                e->yaw -= TURN_SPEED;
            e->frame += ANIM_STEP;
            if (e->state == ENEMY_SWIPE)
            {
                int len = state_len(s, e);
                if (!e->struck && e->frame >= len / 2)
                {
                    e->struck = 1;
                    ev->thrown = i;
                }
                if (e->frame >= len)
                    set_state(s, i, ENEMY_IDLE);
            }
            else if (e->cooldown == 0 && dist < THROW_RANGE && dot > 0 && sees(s, e, t))
            {
                set_state(s, i, ENEMY_SWIPE);
                e->cooldown = THROW_EVERY + rnd(s) % 90;
            }
        }
        else if (e->state == ENEMY_IDLE || e->state == ENEMY_WALK)
        {
            int chase = dist < s->sight;
            if (s->watch)
            {
                if (!e->spotted && notices(s, e, dist, dot))
                    enemies_alert(s, i);
                chase = e->spotted;
            }
            int stop = e->weapon == WEAPON_XBOW && dist < XBOW_KEEP + inttof32(2) && sees(s, e, t) ? XBOW_KEEP : STOP;
            int want = chase && dist > stop ? ENEMY_WALK : ENEMY_IDLE;
            if (want != e->state)
                set_state(s, i, want);
            if (want == ENEMY_WALK && !e->spotted)
                e->spotted = 1;
            if (e->spotted && !e->spot_told)
            {
                e->spot_told = 1;
                ev->spotted = i;
            }
            if (e->detour && (e->state != ENEMY_WALK || !chase))
                e->detour = e->stuck = 0;
            if (e->detour)
            {
                int dc = (int)(((s64)fx * e->detour_dir[1] - (s64)fz * e->detour_dir[0]) >> 12);
                int dd = (int)(((s64)fx * e->detour_dir[0] + (s64)fz * e->detour_dir[1]) >> 12);
                if (dc < -inttof32(1) / 16 || (dd < 0 && dc <= 0))
                    e->yaw += TURN_SPEED * 3;
                else if (dc > inttof32(1) / 16 || dd < 0)
                    e->yaw -= TURN_SPEED * 3;
            }
            else if (chase && dist > 0)
            {
                if (cross < -(s64)dist / 28 || (dot < 0 && cross <= 0))
                    e->yaw += TURN_SPEED;
                else if (cross > (s64)dist / 28 || dot < 0)
                    e->yaw -= TURN_SPEED;
            }
            int atk = e->boss ? -1 : pick_attack(e, dist, on_ally);
            int go = 0;
            if (vulnerable && same_level && s->attacker < 0 && e->enmity >= ENMITY_MAX && dot > 0)
            {
                if (e->boss)
                    go = dist < BOSS_REACH - floattof32(0.5) && cross * 4 < dist && -cross * 4 < dist;
                else if (atk >= 0)
                {
                    const Attack *a = &attacks[atk];
                    go = dist < inttof32(a->start);
                    if (go && a->start_ang)
                    {
                        s64 lim = ((s64)dist * sinLerp(degreesToAngle(a->start_ang))) >> 12;
                        go = cross < lim && -cross < lim;
                    }
                }
            }
            int shoot = !go && e->weapon == WEAPON_XBOW && e->spotted && e->cooldown == 0
                        && dist < XBOW_RANGE && dot > 0 && cross * 3 < dist && -cross * 3 < dist && sees(s, e, t);
            if (shoot)
            {
                e->attack = ATK_XBOW;
                set_state(s, i, ENEMY_SWIPE);
                e->cooldown = XBOW_EVERY + rnd(s) % 90;
                ev->attacking = i;
            }
            else if (go)
            {
                e->attack = atk;
                set_state(s, i, atk == ATK_LUNGE || atk == ATK_CULT_LUNGE ? ENEMY_LUNGE : ENEMY_SWIPE);
                s->attacker = i;
                ev->attacking = i;
            }
            else
            {
                if (e->state == ENEMY_WALK)
                {
                    int sp = e->boss ? BOSS_SPEED : WALK_SPEED, far = dist > FAR_STEP;
                    if (!far || ((s->tick + i) & 1))
                    {
                        int step = sp << far, mx = e->detour ? e->detour_dir[0] : fx, mz = e->detour ? e->detour_dir[1] : fz;
                        int x0 = e->pos[0], z0 = e->pos[2];
                        move(s, e, mulf32(mx, step), mulf32(mz, step));
                        int got = mulf32(e->pos[0] - x0, mx) + mulf32(e->pos[2] - z0, mz);
                        if (e->falling || got * 3 >= step)
                            e->stuck = e->stuck > 2 ? e->stuck - 2 : 0;
                        else if (++e->stuck >= STUCK_FRAMES >> far)
                        {
                            e->stuck = 0;
#ifndef TEST_NO_DETOUR
                            pick_detour(s, e, dx, dz, dist);
#endif
                        }
                    }
                    if (e->detour)
                        e->detour--;
                }
                e->frame += ANIM_STEP;
            }
        }
        else if (e->state == ENEMY_SWIPE && e->attack == ATK_XBOW)
        {
            e->frame += ANIM_STEP;
            if (dist > 0 && (cross < -(s64)dist / 28 || (dot < 0 && cross <= 0)))
                e->yaw += TURN_SPEED;
            else if (dist > 0 && (cross > (s64)dist / 28 || dot < 0))
                e->yaw -= TURN_SPEED;
            if (!e->struck && e->frame >= XBOW_LOOSE)
            {
                e->struck = 1;
                ev->shot = i;
                ev->shot_at[0] = t[0];
                ev->shot_at[1] = t[1];
                ev->shot_at[2] = t[2];
            }
            if (e->frame >= state_len(s, e))
                set_state(s, i, ENEMY_IDLE);
        }
        else if ((e->state == ENEMY_SWIPE || e->state == ENEMY_LUNGE) && e->boss)
        {
            e->frame += ANIM_STEP;
            int len = state_len(s, e);
            if (!e->struck && e->frame >= len * 2 / 5 && e->frame < len * 3 / 5)
            {
                e->struck = 1;
                ev->swing = i;
                if (in_reach && on_ally)
                    ev->ally_hit = i;
                else if (vulnerable && in_reach)
                    ev->swiped_by = i;
            }
            if (e->frame >= len)
                set_state(s, i, ENEMY_IDLE);
        }
        else if (e->state == ENEMY_SWIPE || e->state == ENEMY_LUNGE)
        {
            e->frame += ANIM_STEP;
            const Attack *a = &attacks[e->attack];
            int tick = e->frame >> 12;
            if (tick >= a->turn_from && tick <= a->turn_to && dist > 0)
            {
                int rate = (a->turn ? a->turn : is_cultist(e) ? 550 : 500) / 6;
                if (cross < -(s64)dist / 28 || (dot < 0 && cross <= 0))
                    e->yaw += rate;
                else if (cross > (s64)dist / 28 || dot < 0)
                    e->yaw -= rate;
            }
            for (int k = 0; k < a->steps; k++)
                if (tick >= a->step[k].from && tick <= a->step[k].to)
                {
                    int sp = GAME_UNITS(a->step[k].speed) / 3;
                    move(s, e, mulf32(fx, sp), mulf32(fz, sp));
                }
            if (tick >= a->hit.from && tick <= a->hit.to && !(e->struck & 2))
            {
                if (!a->grab && !(e->struck & 1))
                {
                    e->struck |= 1;
                    ev->swing = i;
                }
                if (same_level && in_area(e, t, dist, cross, dot, &a->hit))
                {
                    e->struck |= 2;
                    if (a->grab)
                    {
                        if (vulnerable && !on_ally)
                        {
                            set_state(s, i, ENEMY_GRAB);
                            ev->grabbed_by = i;
                        }
                    }
                    else if (on_ally)
                        ev->ally_hit = i;
                    else if (vulnerable)
                        ev->swiped_by = i;
                }
            }
            if ((e->state == ENEMY_SWIPE || e->state == ENEMY_LUNGE) && e->frame >= state_len(s, e))
                set_state(s, i, ENEMY_IDLE);
        }
        else if (e->state == ENEMY_GRAB)
        {
            int was = e->frame >> 12;
            e->frame += ANIM_STEP;
            int now = e->frame >> 12;
            if (now != was && (now == BITE_A || now == BITE_B))
                ev->bite = i;
            if (e->frame >= anim_len(s, ANIM_HOLD))
                e->frame -= anim_len(s, ANIM_HOLD);
        }
        else
        {
            e->frame += ANIM_STEP;
            if (e->shove > 0)
            {
                e->shove--;
                move(s, e, -mulf32(sinLerp(e->yaw), RELEASE_PUSH / SHOVE_FRAMES), -mulf32(cosLerp(e->yaw), RELEASE_PUSH / SHOVE_FRAMES));
            }
            if (e->daze > 0)
            {
                e->daze--;
                if (e->frame > anim_len(s, ANIM_HIT) * 2 / 3)
                    e->frame = anim_len(s, ANIM_HIT) * 2 / 3;
            }
            else if (e->frame >= anim_len(s, state_anim[e->state]))
                set_state(s, i, e->state == ENEMY_HIT ? ENEMY_IDLE : ENEMY_DEAD);
        }

        for (int j = 0; j < s->count; j++)
        {
            Enemy *o = &s->e[j];
            if (j == i || !o->active || o->state >= ENEMY_DYING || e->state >= ENEMY_DYING || e->state == ENEMY_GRAB || e->thrower)
                continue;
            int ox = e->pos[0] - o->pos[0], oz = e->pos[2] - o->pos[2];
            int d = (int)sqrt64((u64)((s64)ox * ox + (s64)oz * oz));
            if (d > 0 && d < 2 * RADIUS)
            {
                int push = (2 * RADIUS - d) / 2;
                move(s, e, mulf32(divf32(ox, d), push), mulf32(divf32(oz, d), push));
            }
        }

        if (e->pose_dirty || (e->state != ENEMY_DEAD && (s->tick + i) % 3 == 0))
            update_pose(s, e);
    }
    s->tick++;
}

void enemies_hold_at(Enemies *s, int i, const int pos[3], int dist)
{
    Enemy *e = &s->e[i];
    e->pos[0] = pos[0] - mulf32(sinLerp(e->yaw), dist);
    e->pos[2] = pos[2] - mulf32(cosLerp(e->yaw), dist);
    ground_collide_n(s->ground, &e->pos[0], &e->pos[2], e->feet + STEP_UP, e->feet + HEIGHT, WALL_RADIUS, 2);
    ground_height(s->ground, e->pos[0], e->pos[2], e->feet, STEP_UP, &e->feet);
    e->pos[1] = e->feet - s->model->bbox_min[1];
}

void enemies_release(Enemies *s, int i)
{
    Enemy *e = &s->e[i];
    if (e->state != ENEMY_GRAB)
        return;
    set_state(s, i, ENEMY_HIT);
    e->shove = SHOVE_FRAMES;
}

int enemies_push(const Enemies *s, int *x, int *z, int radius)
{
    int pushed = 0;
    for (int i = 0; i < s->count; i++)
    {
        const Enemy *e = &s->e[i];
        if (!e->active || e->state >= ENEMY_DYING)
            continue;
        int ox = *x - e->pos[0], oz = *z - e->pos[2], min = radius + RADIUS;
        int d = (int)sqrt64((u64)((s64)ox * ox + (s64)oz * oz));
        if (d > 0 && d < min)
        {
            *x = e->pos[0] + mulf32(divf32(ox, d), min);
            *z = e->pos[2] + mulf32(divf32(oz, d), min);
            pushed++;
        }
    }
    return pushed;
}

void enemies_draw(Enemies *s, u32 poly_fmt, const int eye[3], int max)
{
    int order[ENEMY_MAX], dist[ENEMY_MAX], n = 0;
    for (int i = 0; i < s->count; i++)
    {
        Enemy *e = &s->e[i];
        if (!e->active || !view_visible(e->pos, inttof32(10)) || (s->hide_below && e->feet < s->hide_below))
            continue;
        int dx = (e->pos[0] - eye[0]) >> 4, dz = (e->pos[2] - eye[2]) >> 4, d = dx * dx + dz * dz;
        int k = n++;
        while (k > 0 && dist[k - 1] > d)
        {
            order[k] = order[k - 1];
            dist[k] = dist[k - 1];
            k--;
        }
        order[k] = i;
        dist[k] = d;
    }
    for (int j = 0; j < n && j < max; j++)
    {
        Enemy *e = &s->e[order[j]];
        if (e->pose_dirty)
            update_pose(s, e);
        model_draw(e->boss ? s->boss_model : s->model, e->pos, e->yaw, poly_fmt, (const int (*)[12])e->pose);
        if (!e->boss && e->weapon && s->weapon_model[e->weapon])
            model_draw_held(s->weapon_model[e->weapon], e->pos, e->yaw, poly_fmt, e->pose[HAND_NODE]);
        if (e->plaga && s->plaga_model && e->plg_clip >= 0)
        {
            static int local[MODEL_MAX_NODES][12];
            anim_pose(s->plaga_anims, e->plg_clip, e->plg_frame, s->plaga_model, local);
            model_draw_on(s->plaga_model, e->pos, e->yaw, poly_fmt, e->pose[PLG_NECK_NODE], (const int (*)[12])local);
        }
    }
}

int enemies_raycast(const Enemies *s, const int p[3], const int d[3], int *t_best, int *part)
{
    int found = -1;
    s64 aa = ((s64)d[0] * d[0] + (s64)d[2] * d[2]) >> 12;
    if (aa == 0)
        return -1;
    for (int i = 0; i < s->count; i++)
    {
        const Enemy *e = &s->e[i];
        if (!e->active || e->state >= ENEMY_DYING)
            continue;
        s64 fx = p[0] - e->pos[0], fz = p[2] - e->pos[2];
        s64 b = (fx * d[0] + fz * d[2]) >> 12;
        s64 c = ((fx * fx + fz * fz) >> 12) - (((s64)RADIUS * RADIUS) >> 12);
        s64 disc = b * b - aa * c;
        if (disc < 0)
            continue;
        s64 root = (s64)sqrt64(disc);
        s64 t = ((-b - root) << 12) / aa;
        if (t <= 0 && ((-b + root) << 12) / aa > 0)
            t = 1;
        if (t <= 0 || t >= *t_best)
            continue;
        s64 y = p[1] + ((d[1] * t) >> 12) - e->feet;
        if (y < 0 || y > (e->plaga ? PLG_TOP : HEIGHT))
            continue;
        *t_best = (int)t;
        *part = y > HEAD_FROM ? PART_HEAD : y < LEGS_TO ? PART_LEGS : PART_BODY;
        found = i;
    }
    return found;
}

void enemies_damage(Enemies *s, int i, int damage)
{
    Enemy *e = &s->e[i];
    if (!e->active || e->state >= ENEMY_DYING)
        return;
    e->hp -= damage;
    e->flinch = 0;
    set_state(s, i, e->hp <= 0 ? ENEMY_DYING : ENEMY_HIT);
    if (s->watch)
        enemies_alert(s, i);
}

int enemies_shot(Enemies *s, int i, int power, int part, int crit_rate, int crit_wep)
{
    Enemy *e = &s->e[i];
    if (!e->active || e->state >= ENEMY_DYING)
        return 0;
    if (s->watch)
        enemies_alert(s, i);
    int chance = part == PART_HEAD && crit_wep < CRIT_NONE ? e->kind->crit * crit_rate / 100 : 0;
    if (chance > 0 && (int)(rnd(s) % 100) < chance)
    {
        int d = power * e->kind->crit_mul[crit_wep];
        e->hp -= d > 30000 ? 30000 : d;
        s->crit_hit = 1;
    }
    else
        e->hp -= power * e->kind->dmg[part] / 100;
    e->flinch += power * e->kind->flinch[part] / 100;
    if (e->hp > 0 && e->flinch < e->kind->flinch_max)
        return 0;
    e->flinch = 0;
    set_state(s, i, e->hp <= 0 ? ENEMY_DYING : ENEMY_HIT);
    return 1;
}

void enemies_set_kind(Enemies *s, int i, const FoeKind *kind)
{
    Enemy *e = &s->e[i];
    e->kind = kind;
    e->hp = kind->hp;
    e->flinch = 0;
    if (e->boss)
        e->weapon = WEAPON_NONE;
    else if (is_cultist(e))
        e->weapon = kind->ctg == 15 ? WEAPON_BOARD : kind->ctg == 16 ? WEAPON_XBOW : kind->ctg == 18 ? WEAPON_STAFF
                    : kind->ctg ? WEAPON_SICKLE : WEAPON_NONE;
    else
        e->weapon = kind->ctg == 14 ? WEAPON_FORK : kind->ctg ? WEAPON_AXE : WEAPON_NONE;
#ifdef TEST_WEAPON
    if (!e->boss)
        e->weapon = TEST_WEAPON;
#endif
    if (e->thrower)
        e->weapon = WEAPON_NONE;
    e->plaga = !e->boss && kind->plaga;
    if (e->plaga)
        e->weapon = WEAPON_NONE;
    e->plg_clip = -1;
    e->pose_dirty = 1;
}

void enemies_alert(Enemies *s, int i)
{
    if (s->e[i].spotted)
        return;
    s->e[i].spotted = 1;
    if (!s->e[i].kind || !s->e[i].kind->leader)
        return;
    for (int j = 0; j < s->count; j++)
        if (j != i && s->e[j].active && s->e[j].state < ENEMY_DYING)
            s->e[j].spotted = 1;
}

int enemies_alive(const Enemies *s)
{
    int n = 0;
    for (int i = 0; i < s->count; i++)
        n += s->e[i].active && s->e[i].state < ENEMY_DYING;
    return n;
}

void enemies_daze(Enemies *s, int i)
{
    Enemy *e = &s->e[i];
    if (e->active && e->state == ENEMY_HIT)
        e->daze = DAZE_FRAMES;
}

static int in_front(const Enemy *e, const int pos[3], int yaw)
{
    int dx = e->pos[0] - pos[0], dz = e->pos[2] - pos[2];
    int d = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
    s64 dot = ((s64)sinLerp(yaw) * dx + (s64)cosLerp(yaw) * dz) >> 12;
    return d > 0 && dot * 10 > (s64)d * 7 ? d : -1;
}

int enemies_kickable(const Enemies *s, const int pos[3], int yaw)
{
    for (int i = 0; i < s->count; i++)
    {
        const Enemy *e = &s->e[i];
        int d;
        if (e->active && e->state == ENEMY_HIT && e->daze > 0 && (d = in_front(e, pos, yaw)) >= 0 && d < KICK_RANGE)
            return i;
    }
    return -1;
}

int enemies_kick(Enemies *s, const int pos[3], int yaw, int damage)
{
    int n = 0;
    for (int i = 0; i < s->count; i++)
    {
        Enemy *e = &s->e[i];
        int d;
        if (!e->active || e->state >= ENEMY_DYING || (d = in_front(e, pos, yaw)) < 0 || d >= KICK_RANGE + RADIUS)
            continue;
        enemies_damage(s, i, damage);
        int dx = e->pos[0] - pos[0], dz = e->pos[2] - pos[2];
        move(s, e, mulf32(divf32(dx, d), KICK_PUSH), mulf32(divf32(dz, d), KICK_PUSH));
        n++;
    }
    return n;
}
int enemies_slash(Enemies *s, const int pos[3], int yaw, int range, int damage)
{
    int best = -1, best_d = range + RADIUS;
    for (int i = 0; i < s->count; i++)
    {
        const Enemy *e = &s->e[i];
        int d;
        if (e->active && e->state < ENEMY_DYING && (d = in_front(e, pos, yaw)) >= 0 && d < best_d)
        {
            best = i;
            best_d = d;
        }
    }
    if (best >= 0)
        enemies_damage(s, best, damage);
    return best;
}
