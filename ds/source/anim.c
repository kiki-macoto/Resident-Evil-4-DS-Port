#include <math.h>
#include <string.h>

#include "anim.h"

#define KEY_SIZE 16   // u16 frame, u16 0, s32 x, y, z

static u16 rd16(const u8 *p) { return p[0] | p[1] << 8; }
static s32 rd32(const u8 *p) { return (s32)(p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24); }

int anim_load(AnimSet *a, const void *data, size_t size)
{
    const u8 *base = data, *end = base + size;
    memset(a, 0, sizeof(*a));
    if (size < 8 || memcmp(base, "RDA1", 4) != 0)
        return -1;
    a->count = rd16(base + 4);
    a->nodes = rd16(base + 6);
    if (a->count > ANIM_MAX || a->nodes > MODEL_MAX_NODES || 8 + a->count * 4 > (int)size)
        return -2;
    for (int i = 0; i < a->count; i++)
    {
        const u8 *p = base + rd32(base + 8 + i * 4);
        if (p + 4 > end)
            return -3;
        a->frames[i] = rd16(p);
        p += 4;
        for (int n = 0; n < a->nodes; n++)
        {
            if (p + 4 > end)
                return -3;
            a->node_data[i][n] = p;
            p += 4 + (rd16(p) + rd16(p + 2)) * KEY_SIZE;
            if (p > end)
                return -3;
        }
    }
    return 0;
}

// Linear interpolation of a 3-component channel at frame f (f32).
static void sample(const u8 *keys, int count, int f, int out[3])
{
    int k = 0;
    while (k + 1 < count && inttof32(rd16(keys + (k + 1) * KEY_SIZE)) <= f)
        k++;
    const u8 *a = keys + k * KEY_SIZE;
    if (k + 1 >= count)
    {
        for (int j = 0; j < 3; j++)
            out[j] = rd32(a + 4 + j * 4);
        return;
    }
    const u8 *b = a + KEY_SIZE;
    int f0 = inttof32(rd16(a)), f1 = inttof32(rd16(b));
    int t = f1 > f0 ? divf32(f - f0, f1 - f0) : 0;
    for (int j = 0; j < 3; j++)
    {
        int v0 = rd32(a + 4 + j * 4), v1 = rd32(b + 4 + j * 4);
        out[j] = v0 + mulf32(v1 - v0, t);
    }
}

void anim_sample(const AnimSet *a, int anim, int frame, const Model *m, NodeLocal out[])
{
    // Single-frame poses (length 0 or 1) are sampled as given; others wrap.
    int len = inttof32(a->frames[anim] > 0 ? a->frames[anim] : 1);
    if (a->frames[anim] > 1)
    {
        frame %= len;
        if (frame < 0)
            frame += len;
    }

    for (int n = 0; n < m->node_count; n++)
    {
        NodeLocal *l = &out[n];
        l->t[0] = m->bind_local[n][9];
        l->t[1] = m->bind_local[n][10];
        l->t[2] = m->bind_local[n][11];
        l->r[0] = l->r[1] = l->r[2] = 0;
        l->has_r = 0;
        if (n >= a->nodes)
            continue;
        const u8 *p = a->node_data[anim][n];
        int tkeys = rd16(p), rkeys = rd16(p + 2);
        const u8 *keys = p + 4;
        if (tkeys)
            sample(keys, tkeys, frame, l->t);
        if (rkeys)
        {
            sample(keys + tkeys * KEY_SIZE, rkeys, frame, l->r);
            l->has_r = 1;
        }
    }
}

// Shortest signed difference a - b between two angles (libnds units).
static int angle_diff(int a, int b)
{
    int d = (a - b) % DEGREES_IN_CIRCLE;
    if (d > DEGREES_IN_CIRCLE / 2)
        d -= DEGREES_IN_CIRCLE;
    if (d < -DEGREES_IN_CIRCLE / 2)
        d += DEGREES_IN_CIRCLE;
    return d;
}

void anim_blend(NodeLocal dst[], const NodeLocal src[], int count, int w)
{
    for (int n = 0; n < count; n++)
    {
        for (int j = 0; j < 3; j++)
        {
            dst[n].t[j] += mulf32(src[n].t[j] - dst[n].t[j], w);
            // Missing rotations are the bind rotation, which is identity for every
            // animated node, so they blend as zero angles.
            dst[n].r[j] += mulf32(angle_diff(src[n].r[j], dst[n].r[j]), w);
        }
        dst[n].has_r |= src[n].has_r;
    }
}

void anim_add_delta(NodeLocal dst[], const NodeLocal a[], const NodeLocal b[], int count, int w)
{
    for (int n = 0; n < count; n++)
    {
        for (int j = 0; j < 3; j++)
        {
            dst[n].t[j] += mulf32(a[n].t[j] - b[n].t[j], w);
            dst[n].r[j] += mulf32(angle_diff(a[n].r[j], b[n].r[j]), w);
        }
        dst[n].has_r |= a[n].has_r;
    }
}

#define TURN_F 32768.0f   // libnds angle units per turn
static void euler_f(float m[9], const int r[3])
{
    float ax = r[0] * (6.2831853f / TURN_F), ay = r[1] * (6.2831853f / TURN_F), az = r[2] * (6.2831853f / TURN_F);
    float sx = sinf(ax), cx = cosf(ax), sy = sinf(ay), cy = cosf(ay), sz = sinf(az), cz = cosf(az);
    m[0] = cz * cy; m[1] = cz * sy * sx - sz * cx; m[2] = cz * sy * cx + sz * sx;
    m[3] = sz * cy; m[4] = sz * sy * sx + cz * cx; m[5] = sz * sy * cx - cz * sx;
    m[6] = -sy;     m[7] = cy * sx;                m[8] = cy * cx;
}

void anim_add_delta_rot(NodeLocal dst[], const NodeLocal a[], const NodeLocal b[], int count, int w)
{
    static const int zero[3] = { 0, 0, 0 };
    for (int n = 0; n < count; n++)
    {
        for (int j = 0; j < 3; j++)
            dst[n].t[j] += mulf32(a[n].t[j] - b[n].t[j], w);
        if (!a[n].has_r || (b[n].has_r && a[n].r[0] == b[n].r[0] && a[n].r[1] == b[n].r[1] && a[n].r[2] == b[n].r[2]))
            continue;   // (this joint doesn't turn: nothing to add)
        float ma[9], mb[9], md[9], d[9], r[9];
        euler_f(ma, a[n].r);
        euler_f(mb, b[n].has_r ? b[n].r : zero);
        euler_f(md, dst[n].has_r ? dst[n].r : zero);
        for (int i = 0; i < 3; i++)       // d = mb^T ma: the turn from b to a, in the joint's frame
            for (int j = 0; j < 3; j++)
                d[i * 3 + j] = mb[i] * ma[j] + mb[3 + i] * ma[3 + j] + mb[6 + i] * ma[6 + j];
        for (int i = 0; i < 3; i++)       // r = md d
            for (int j = 0; j < 3; j++)
                r[i * 3 + j] = md[i * 3] * d[j] + md[i * 3 + 1] * d[3 + j] + md[i * 3 + 2] * d[6 + j];
        float sy = -r[6];
        sy = sy > 1 ? 1 : sy < -1 ? -1 : sy;
        int out[3] = { (int)(atan2f(r[7], r[8]) * (TURN_F / 6.2831853f)), (int)(asinf(sy) * (TURN_F / 6.2831853f)),
                       (int)(atan2f(r[3], r[0]) * (TURN_F / 6.2831853f)) };
        int base[3];
        for (int j = 0; j < 3; j++)
            base[j] = dst[n].has_r ? dst[n].r[j] : 0;
        for (int j = 0; j < 3; j++)
            dst[n].r[j] = base[j] + mulf32(angle_diff(out[j], base[j]), w);
        dst[n].has_r = 1;
    }
}

void anim_compose(const Model *m, const NodeLocal loc[], int out[][12])
{
    // Parents usually come before their children, but not always (the giant's arms hang off
    // shoulder nodes listed after them): nodes whose parent isn't done yet wait for a later pass.
    u32 done = 0, all = m->node_count >= 32 ? ~0u : (1u << m->node_count) - 1;
    for (int pass = 0; pass < m->node_count && done != all; pass++)
        for (int n = 0; n < m->node_count; n++)
        {
            int p = m->parent[n];
            if ((done & (1u << n)) || (p >= 0 && !(done & (1u << p))))
                continue;
            int local[12];
            memcpy(local, m->bind_local[n], sizeof(local));
            local[9] = loc[n].t[0];
            local[10] = loc[n].t[1];
            local[11] = loc[n].t[2];
            if (loc[n].has_r)
                mtx43_euler(local, loc[n].r[0], loc[n].r[1], loc[n].r[2]);
            if (p < 0)
                memcpy(out[n], local, sizeof(local));
            else
                mtx43_mul(out[n], out[p], local);
            done |= 1u << n;
        }
}

void anim_pose(const AnimSet *a, int anim, int frame, const Model *m, int out[][12])
{
    NodeLocal loc[MODEL_MAX_NODES];
    anim_sample(a, anim, frame, m, loc);
    anim_compose(m, loc, out);
}
