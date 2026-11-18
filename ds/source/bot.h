#ifndef BOT_H
#define BOT_H

#ifndef DOOR_W
#define DOOR_W inttof32(13)
#endif
#define BOT_LEAVE_GAL 1
#define BN_CELL   inttof32(2)
#define BN_R      180
#define BN_W      (BN_R * 2 + 1)
#define BN_LV     16
#define BN_LEVEL(y) ((((y) >> 12) + 48) / 12)
#define BN_MAX    20000
#define BN_PATH   800
enum { BN_WALK, BN_LADDER_UP, BN_LADDER_DOWN, BN_JUMP };
typedef struct { s32 x, z, feet, parent, g; s16 ci, cj; u8 kind, arg; } BnNode;
static BnNode bn_node[BN_MAX];
static s32 bn_heap[BN_MAX], bn_key[BN_MAX];
static int bn_heap_n;
static u8 bn_seen[(BN_W * BN_W * BN_LV + 7) / 8];
static s32 bn_path[BN_PATH];
static int bn_path_n, bn_truncated, bn_n_last;
#define BN_BLOCKS 24
static int bn_block[BN_BLOCKS][3], bn_block_t[BN_BLOCKS];

static int bn_mark(int ci, int cj, int feet)
{
    int lv = BN_LEVEL(feet);
    if (ci < -BN_R || ci > BN_R || cj < -BN_R || cj > BN_R || lv < 0 || lv >= BN_LV)
        return 0;
    u32 b = ((u32)(cj + BN_R) * BN_W + (ci + BN_R)) * BN_LV + lv;
    if (bn_seen[b >> 3] & (1 << (b & 7)))
        return 0;
    bn_seen[b >> 3] |= 1 << (b & 7);
    return 1;
}
static void bn_push(int i, int key)
{
    int k = bn_heap_n++;
    bn_key[i] = key;
    while (k > 0 && bn_key[bn_heap[(k - 1) / 2]] > key)
    {
        bn_heap[k] = bn_heap[(k - 1) / 2];
        k = (k - 1) / 2;
    }
    bn_heap[k] = i;
}
static int bn_pop(void)
{
    int top = bn_heap[0], last = bn_heap[--bn_heap_n], k = 0;
    for (;;)
    {
        int c = k * 2 + 1;
        if (c >= bn_heap_n)
            break;
        if (c + 1 < bn_heap_n && bn_key[bn_heap[c + 1]] < bn_key[bn_heap[c]])
            c++;
        if (bn_key[bn_heap[c]] >= bn_key[last])
            break;
        bn_heap[k] = bn_heap[c];
        k = c;
    }
    if (bn_heap_n > 0)
        bn_heap[k] = last;
    return top;
}

static const int *bn_flee;
#define BN_FLEE_NODES 2500
static int bn_add(int n, int at, int x, int z, int f, int ci, int cj, int kind, int arg, int cost, const int goal[3])
{
    if (n >= BN_MAX || !bn_mark(ci, cj, f))
        return n;
    int g = bn_node[at].g + cost;
    bn_node[n] = (BnNode){ x, z, f, at, g, ci, cj, kind, arg };
    int h = 0;
    if (!bn_flee)
    {
        s64 dx = x - goal[0], dz = z - goal[2];
        h = (int)sqrt64((u64)(dx * dx + dz * dz)) + abs(f - goal[1]) * 2;
    }
    bn_push(n, g + h * 2);
    return n + 1;
}
static int bn_gate_n, bn_gate[8][4];
static u8 bn_door_bad[OBJECTS_MAX];
static void bn_gates(int stage)
{
    const StageFoes *sf = &stage_foes[stage];
    bn_gate_n = 0;
    for (int i = 0; i < objs.count && bn_gate_n < 8; i++)
    {
        const Obj *o = &objs.o[i];
        if (o->kind != OBJ_SWING || o->open || o->opening)
            continue;
        int mx = (o->hinge[0] + (o->hinge[0] - mulf32(cosLerp(o->yaw), DOOR_W))) / 2, mz = (o->hinge[1] + (o->hinge[1] + mulf32(sinLerp(o->yaw), DOOR_W))) / 2;
        int gated = o->locked || bn_door_bad[i];
        for (int e = 0; e < sf->events && e < 64 && !gated; e++)
        {
            const StageEvent *ev = &sf->event[e];
            if (ev->ctg != EV_POINT || ev->get < GET_BUTTON || ev_bit(ev_deleted, e) || (ev_bit(ev_played, e) && !ev->again))
                continue;
            int dp[3] = { mx, o->pos[1], mz };
            if (ev->cond != COND_NONE && !ev_cond(sf, ev, dp))
                continue;
            int px = inttof32(sf->point[ev->trg][0]), pz = inttof32(sf->point[ev->trg][2]);
            gated = abs(px - mx) < inttof32(14) && abs(pz - mz) < inttof32(14);
        }
        if (gated)
        {
            bn_gate[bn_gate_n][0] = o->hinge[0];
            bn_gate[bn_gate_n][1] = o->hinge[1];
            bn_gate[bn_gate_n][2] = o->hinge[0] - mulf32(cosLerp(o->yaw), DOOR_W);
            bn_gate[bn_gate_n][3] = o->hinge[1] + mulf32(sinLerp(o->yaw), DOOR_W);
            bn_gate_n++;
        }
    }
}
static int bn_gated(int x, int z)
{
    for (int i = 0; i < bn_gate_n; i++)
    {
        s64 ax = bn_gate[i][0], az = bn_gate[i][1], dx = bn_gate[i][2] - ax, dz = bn_gate[i][3] - az;
        s64 l2 = dx * dx + dz * dz, t = l2 ? ((x - ax) * dx + (z - az) * dz) * 4096 / l2 : 0;
        if (t < 0) t = 0;
        if (t > 4096) t = 4096;
        s64 ex = x - (ax + dx * t / 4096), ez = z - (az + dz * t / 4096);
        if (ex * ex + ez * ez < (s64)inttof32(3) * inttof32(3))
            return 1;
    }
    return 0;
}
static int bn_plan(int stage, const int from[3], const int goal[3], int reach, int dyok)
{
    const Ground *g = &stage_ground;
    bn_gates(stage);
    memset(bn_seen, 0, sizeof(bn_seen));
    int n = 1, best = 0, found = -1;
    s64 best_d = 0;
    bn_heap_n = 0;
    bn_node[0] = (BnNode){ from[0], from[2], from[1], -1, 0, 0, 0, BN_WALK, 0 };
    bn_mark(0, 0, from[1]);
    bn_push(0, 0);
    bn_truncated = 0;
    static const s8 dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
    while (bn_heap_n > 0)
    {
        int at = bn_pop();
        BnNode s = bn_node[at];
        s64 dx = s.x - goal[0], dz = s.z - goal[2], d2 = dx * dx + dz * dz;
        int dy = s.feet - goal[1];
        if (!bn_flee && d2 <= (s64)reach * reach && dy <= dyok && dy >= -dyok)
        {
            found = at;
            break;
        }
        s64 score = d2 + (s64)(dy > 0 ? dy : -dy) * (dy > 0 ? dy : -dy) * 4;
        if (bn_flee)
        {
            s64 fx = s.x - bn_flee[0], fz = s.z - bn_flee[2];
            int ox = s.x, oz = s.z;
            ground_collide(g, &ox, &oz, s.feet + STEP_UP, s.feet + LEON_HEIGHT, inttof32(10));
            objects_collide(&objs, &ox, &oz, s.feet, inttof32(8));
            score = (ox != s.x || oz != s.z) ? 0 : -(fx * fx + fz * fz);
        }
        if (at == 0 || score < best_d)
        {
            best_d = score;
            best = at;
        }
        if (n >= BN_MAX - 12 || (bn_flee && n >= BN_FLEE_NODES))
        {
            bn_truncated = 1;
            break;
        }
        for (int i = 0; i < objs.count; i++)
        {
            const Obj *o = &objs.o[i];
            if (o->kind != OBJ_LADDER)
                continue;
            int lx = s.x - o->pos[0], lz = s.z - o->pos[2];
            int along = mulf32(lx, o->dir[0]) + mulf32(lz, o->dir[1]);
            int across = abs(mulf32(lx, o->dir[1]) - mulf32(lz, o->dir[0]));
            if (across < inttof32(3) && along > -inttof32(6) && along < o->end[0] && s.feet < o->pos[1] + inttof32(4))
            {
                int x = o->pos[0] + mulf32(o->dir[0], CLIMB_BACK), z = o->pos[2] + mulf32(o->dir[1], CLIMB_BACK), f = o->top;
                ground_height(g, x, z, f, STEP_UP, &f);
                n = bn_add(n, at, x, z, f, (x - from[0]) / BN_CELL, (z - from[2]) / BN_CELL, BN_LADDER_UP, i, inttof32(25), goal);
            }
            if (across < inttof32(3) && along > o->end[0] && along < o->end[0] + inttof32(12) && s.feet > o->top - inttof32(4))
                n = bn_add(n, at, o->pos[0], o->pos[2], o->pos[1], (o->pos[0] - from[0]) / BN_CELL, (o->pos[2] - from[2]) / BN_CELL, BN_LADDER_DOWN, i, inttof32(25), goal);
        }
        for (unsigned j = 0; j < sizeof(jumps) / sizeof(jumps[0]); j++)
        {
            const JumpSpot *js = &jumps[j];
            int jx = s.x - inttof32(js->x), jz = s.z - inttof32(js->z), jy = s.feet - inttof32(js->y);
            if (js->stage == stage && (s64)jx * jx + (s64)jz * jz < (s64)(JUMP_NEAR / 2) * (JUMP_NEAR / 2) && jy < inttof32(6) && jy > -inttof32(6))
            {
                int f = inttof32(js->to_y);
                ground_height(g, inttof32(js->to_x), inttof32(js->to_z), f + STEP_UP, STEP_UP * 2, &f);
                n = bn_add(n, at, inttof32(js->to_x), inttof32(js->to_z), f, (inttof32(js->to_x) - from[0]) / BN_CELL, (inttof32(js->to_z) - from[2]) / BN_CELL, BN_JUMP, j, inttof32(15), goal);
            }
        }
        for (int d = 0; d < 8; d++)
        {
            int ci = s.ci + dirs[d][0], cj = s.cj + dirs[d][1];
            int tx = from[0] + ci * BN_CELL, tz = from[2] + cj * BN_CELL, x = tx, z = tz;
            int blocked = 0;
            for (int b = 0; b < BN_BLOCKS && !blocked; b++)
                blocked = bn_block_t[b] > 0 && abs(tx - bn_block[b][0]) < inttof32(3) && abs(tz - bn_block[b][2]) < inttof32(3)
                          && abs(s.feet - bn_block[b][1]) < inttof32(8);
            if (blocked || bn_gated(tx, tz))
                continue;
            ground_collide(g, &x, &z, s.feet + STEP_UP, s.feet + LEON_HEIGHT, LEON_RADIUS);
            if (abs(x - tx) > BN_CELL / 2 || abs(z - tz) > BN_CELL / 2)
                continue;
            int mx = (s.x + tx) / 2, mz = (s.z + tz) / 2, qx = mx, qz = mz;
            ground_collide(g, &qx, &qz, s.feet + STEP_UP, s.feet + LEON_HEIGHT, LEON_RADIUS);
            if (abs(qx - mx) > BN_CELL / 2 || abs(qz - mz) > BN_CELL / 2)
                continue;
            int floor = s.feet;
            if (!ground_height(g, x, z, s.feet, STEP_UP, &floor))
                continue;
            if (floor < s.feet - DROP_MIN && stage == ASH_STAGE_E)
                continue;
            n = bn_add(n, at, x, z, floor, ci, cj, BN_WALK, 0, d < 4 ? BN_CELL : BN_CELL * 17 / 12, goal);
        }
    }
    bn_n_last = n;
    int end = found >= 0 ? found : best;
    bn_path_n = 0;
    for (int k = end; k >= 0 && bn_path_n < BN_PATH; k = bn_node[k].parent)
        bn_path[bn_path_n++] = k;
    for (int a = 0, b = bn_path_n - 1; a < b; a++, b--)
    {
        s32 t = bn_path[a];
        bn_path[a] = bn_path[b];
        bn_path[b] = t;
    }
    return found >= 0 ? 1 : bn_path_n > 1 ? 0 : -1;
}
static int bot_yaw_to(const int from[3], const int to[3])
{
    return (int)(atan2f((float)(to[0] - from[0]), (float)(to[2] - from[2])) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
}
static int bot_angle_diff(int a, int b)
{
    int d = (a - b) & (DEGREES_IN_CIRCLE - 1);
    return d >= DEGREES_IN_CIRCLE / 2 ? d - DEGREES_IN_CIRCLE : d;
}
static int bot_dist(const int a[3], const int b[3])
{
    s64 dx = a[0] - b[0], dz = a[2] - b[2];
    return (int)sqrt64((u64)(dx * dx + dz * dz));
}
static int bot_sees(const int eye[3], const int to[3])
{
    int d[3] = { to[0] - eye[0], to[1] - eye[1], to[2] - eye[2] };
    s64 l2 = (s64)d[0] * d[0] + (s64)d[1] * d[1] + (s64)d[2] * d[2];
    int len = (int)sqrt64((u64)l2);
    if (len <= 0)
        return 1;
    for (int k = 0; k < 3; k++)
        d[k] = divf32(d[k], len);
    int hit[3];
    return !ground_raycast(&stage_ground, eye, d, len - inttof32(2), hit, RAY_SHOT);
}

enum { BG_NONE, BG_DROP, BG_ASHLEY, BG_EVENT, BG_CRATE, BG_HUNT, BG_GIANT };
typedef struct { int kind, idx, pos[3], reach, dyok, press; } BotGoal;
static u16 bot_skip_t[8][64];
static u8 bot_done[64];
#endif
