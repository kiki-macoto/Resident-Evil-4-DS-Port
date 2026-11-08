#include <string.h>

#include "ground.h"

static u16 rd16(const u8 *p) { return p[0] | p[1] << 8; }
static u32 rd32(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }

int ground_load(Ground *g, const void *data, size_t size)
{
    const u8 *p = data, *end = p + size;
    memset(g, 0, sizeof(*g));
    if (size < 24 || memcmp(p, "RDG2", 4) != 0)
        return -1;
    g->cells_x = rd16(p + 4);
    g->cells_z = rd16(p + 6);
    g->origin_x = (int)rd32(p + 8);
    g->origin_z = (int)rd32(p + 12);
    g->cell_size = (int)rd32(p + 16);
    g->triangles = rd32(p + 20);
    if (g->cell_size <= 0)
        return -2;

    u32 cells = g->cells_x * g->cells_z;
    g->cell_start = (const u32 *)(p + 24);
    u32 refs = g->cell_start[cells];
    const u8 *idx = p + 24 + (cells + 1) * 4;
    g->index = (const u16 *)idx;
    g->tri = (const s32 *)(idx + ((refs * 2 + 3) & ~3u));
    const u8 *w = (const u8 *)(g->tri + g->triangles * 9);
    if (w + 4 + (cells + 1) * 4 > end)
        return -3;

    g->walls = rd32(w);
    g->wall_cell_start = (const u32 *)(w + 4);
    u32 wrefs = g->wall_cell_start[cells];
    const u8 *widx = w + 4 + (cells + 1) * 4;
    g->wall_index = (const u16 *)widx;
    g->wall = (const s32 *)(widx + ((wrefs * 2 + 3) & ~3u));
    if ((const u8 *)(g->wall + g->walls * 6) > end)
        return -4;

    const u8 *r = (const u8 *)(g->wall + g->walls * 6);
    if (r == end)
        return 0;
    g->ray_tris = rd32(r);
    g->ray_cell_start = (const u32 *)(r + 4);
    refs = g->ray_cell_start[cells];
    idx = r + 4 + (cells + 1) * 4;
    g->ray_index = (const u16 *)idx;
    g->ray_tri = (const s32 *)(idx + ((refs * 2 + 3) & ~3u));
    r = (const u8 *)(g->ray_tri + g->ray_tris * 9);
    g->ray_walls = rd32(r);
    g->ray_wall_cell_start = (const u32 *)(r + 4);
    wrefs = g->ray_wall_cell_start[cells];
    widx = r + 4 + (cells + 1) * 4;
    g->ray_wall_index = (const u16 *)widx;
    g->ray_wall = (const s32 *)(widx + ((wrefs * 2 + 3) & ~3u));
    g->ray_tri_mask = (const u8 *)(g->ray_wall + g->ray_walls * 6);
    g->ray_wall_mask = g->ray_tri_mask + g->ray_tris;
    if (g->ray_wall_mask + g->ray_walls > end)
        return -5;
    return 0;
}

// Sign of the 2D cross product (b - a) x (p - a), in 64 bits (coordinates are f32).
static int side(int ax, int az, int bx, int bz, int px, int pz)
{
    s64 c = (s64)(bx - ax) * (pz - az) - (s64)(bz - az) * (px - ax);
    return c > 0 ? 1 : c < 0 ? -1 : 0;
}

int ground_height(const Ground *g, int x, int z, int ref_y, int step_up, int *y)
{
    int cx = (x - g->origin_x) / g->cell_size, cz = (z - g->origin_z) / g->cell_size;
    if (x < g->origin_x || z < g->origin_z || cx >= g->cells_x || cz >= g->cells_z)
        return 0;

    int cell = cz * g->cells_x + cx, found = 0, best = 0;
    for (u32 i = g->cell_start[cell]; i < g->cell_start[cell + 1]; i++)
    {
        const s32 *t = g->tri + g->index[i] * 9;
        int s0 = side(t[0], t[1], t[2], t[3], x, z);
        int s1 = side(t[2], t[3], t[4], t[5], x, z);
        int s2 = side(t[4], t[5], t[0], t[1], x, z);
        if ((s0 < 0 || s1 < 0 || s2 < 0) && (s0 > 0 || s1 > 0 || s2 > 0))
            continue;   // outside (edges count as inside, for either winding)
        int h = mulf32(t[6], x) + mulf32(t[7], z) + t[8];
        if (h > ref_y + step_up)
            continue;
        if (!found || h > best)
            best = h;
        found = 1;
    }
    if (found)
        *y = best;
    return found;
}

#define GAPS_MAX 8
static int gap_n, gaps[GAPS_MAX][6];
void ground_clear_gaps(void) { gap_n = 0; }
void ground_add_gap(int cx, int cz, int ux, int uz, int half_len, int half_depth)
{
    if (gap_n >= GAPS_MAX)
        return;
    int *p = gaps[gap_n++];
    p[0] = cx; p[1] = cz; p[2] = ux; p[3] = uz; p[4] = half_len; p[5] = half_depth;
}
static int in_gap(int x, int z)
{
    for (int i = 0; i < gap_n; i++)
    {
        const int *p = gaps[i];
        int dx = x - p[0], dz = z - p[1];
        int along = mulf32(dx, p[2]) + mulf32(dz, p[3]), across = mulf32(dx, -p[3]) + mulf32(dz, p[2]);
        if (along > -p[4] && along < p[4] && across > -p[5] && across < p[5])
            return 1;
    }
    return 0;
}

// Pushes (*x, *z) out of one wall segment if the circle overlaps it.
static int push_out(const s32 *w, int *x, int *z, int radius)
{
    // Quick reject (no 64-bit division): the circle is off the segment's bounding box.
    int lo = w[0] < w[2] ? w[0] : w[2], hi = w[0] < w[2] ? w[2] : w[0];
    if (*x + radius < lo || *x - radius > hi)
        return 0;
    lo = w[1] < w[3] ? w[1] : w[3];
    hi = w[1] < w[3] ? w[3] : w[1];
    if (*z + radius < lo || *z - radius > hi)
        return 0;
    s64 dx = w[2] - w[0], dz = w[3] - w[1];
    s64 px = *x - w[0], pz = *z - w[1];
    s64 len2 = dx * dx + dz * dz;
    if (len2 == 0)
        return 0;
    // Closest point on the segment: a + d * t, t clamped to 0..1 (t in 16.16, so that
    // d * t stays well inside 64 bits even for long walls).
    s64 num = px * dx + pz * dz;
    if (num < 0)
        num = 0;
    if (num > len2)
        num = len2;
    s64 t = (num << 16) / len2;
    s64 ox = px - ((dx * t) >> 16), oz = pz - ((dz * t) >> 16);   // closest point -> centre
    s64 dist2 = ox * ox + oz * oz;
    if (dist2 >= (s64)radius * radius)
        return 0;
    if (gap_n && in_gap((int)(*x - ox), (int)(*z - oz)))
        return 0;   // (that part of the wall is a doorway)

    s64 dist = sqrt64(dist2);
    if (dist == 0)
    {
        // Centre exactly on the wall: push along the segment's normal.
        s64 len = sqrt64(len2);
        ox = -dz * 4096 / len;
        oz = dx * 4096 / len;
        dist = 4096;
        *x += (int)(ox * radius / dist);
        *z += (int)(oz * radius / dist);
        return 1;
    }
    s64 push = radius - dist;
    *x += (int)(ox * push / dist);
    *z += (int)(oz * push / dist);
    return 1;
}

// Ray tests. t is the distance along the ray (f32, d is a unit vector); a hit is kept if
// 0 < t < *best. The ARM9 has no 64-bit divide, so candidates are rejected with
// multiplications first and only the survivors pay for a division.

static int ray_wall(const s32 *w, const int p[3], const int d[3], s64 *best)
{
    s64 ex = w[2] - w[0], ez = w[3] - w[1];
    s64 den = (s64)d[0] * ez - (s64)d[2] * ex;   // cross(d.xz, e)
    if (den == 0)
        return 0;
    s64 qx = w[0] - p[0], qz = w[1] - p[2];
    s64 tn = qx * ez - qz * ex;                    // t = tn / den (x 4096 for f32)
    s64 un = qx * d[2] - qz * d[0];                // u = un / den, along the segment
    if (den < 0)
    {
        den = -den;
        tn = -tn;
        un = -un;
    }
    if (tn <= 0 || un < 0 || un > den || (tn << 12) >= *best * den)
        return 0;
    s64 t = (tn << 12) / den;
    s64 y = p[1] + ((d[1] * t) >> 12);
    if (y < w[4] || y > w[5])
        return 0;
    *best = t;
    return 1;
}

static int ray_floor(const s32 *tr, const int p[3], const int d[3], s64 *best)
{
    // Plane y = a*x + b*z + c.
    s64 den = d[1] - (((s64)tr[6] * d[0] + (s64)tr[7] * d[2]) >> 12);
    s64 num = (((s64)tr[6] * p[0] + (s64)tr[7] * p[2]) >> 12) + tr[8] - p[1];
    if (den < 0)
    {
        den = -den;
        num = -num;
    }
    if (den == 0 || num <= 0 || (num << 12) >= *best * den)
        return 0;
    s64 t = (num << 12) / den;
    int x = p[0] + (int)((d[0] * t) >> 12), z = p[2] + (int)((d[2] * t) >> 12);
    int s0 = side(tr[0], tr[1], tr[2], tr[3], x, z);
    int s1 = side(tr[2], tr[3], tr[4], tr[5], x, z);
    int s2 = side(tr[4], tr[5], tr[0], tr[1], x, z);
    if ((s0 < 0 || s1 < 0 || s2 < 0) && (s0 > 0 || s1 > 0 || s2 > 0))
        return 0;
    *best = t;
    return 1;
}

// Per-primitive "already tested in this cast" stamps (a primitive can sit in many cells).
#define STAMP_MAX 4096
static u16 wall_stamp[STAMP_MAX], tri_stamp[STAMP_MAX];
static u16 cast_id;

int ground_raycast(const Ground *g, const int p[3], const int d[3], int max_dist, int hit[3], int mask)
{
    s64 best = max_dist;
    int found = 0, own = g->ray_tri_mask != NULL;   // (none in the file: rays hit what stops walkers)
    if (++cast_id == 0)
    {
        memset(wall_stamp, 0, sizeof(wall_stamp));
        memset(tri_stamp, 0, sizeof(tri_stamp));
        cast_id = 1;
    }

    // March along the ray in quarter-cell steps, testing each grid cell's walls and floors
    // once, nearest cells first. Once a hit is closer than the cells still ahead, stop.
    int step = g->cell_size / 4, last_cell = -1;
    for (int s = 0; s <= max_dist; s += step)
    {
        if (found && best + g->cell_size < s)
            break;
        int x = p[0] + mulf32(d[0], s), z = p[2] + mulf32(d[2], s);
        if (x < g->origin_x || z < g->origin_z)
            continue;
        int cx = (x - g->origin_x) / g->cell_size, cz = (z - g->origin_z) / g->cell_size;
        if (cx >= g->cells_x || cz >= g->cells_z)
            continue;
        int cell = cz * g->cells_x + cx;
        if (cell == last_cell)
            continue;
        last_cell = cell;

        const u32 *wc = own ? g->ray_wall_cell_start : g->wall_cell_start, *tc = own ? g->ray_cell_start : g->cell_start;
        const u16 *wi = own ? g->ray_wall_index : g->wall_index, *ti = own ? g->ray_index : g->index;
        const s32 *wl = own ? g->ray_wall : g->wall, *tr = own ? g->ray_tri : g->tri;
        for (u32 i = wc[cell]; i < wc[cell + 1]; i++)
        {
            u16 w = wi[i];
            if (own && !(g->ray_wall_mask[w] & mask))
                continue;
            if (w < STAMP_MAX)
            {
                if (wall_stamp[w] == cast_id)
                    continue;
                wall_stamp[w] = cast_id;
            }
            found |= ray_wall(wl + w * 6, p, d, &best);
        }
        for (u32 i = tc[cell]; i < tc[cell + 1]; i++)
        {
            u16 t = ti[i];
            if (own && !(g->ray_tri_mask[t] & mask))
                continue;
            if (t < STAMP_MAX)
            {
                if (tri_stamp[t] == cast_id)
                    continue;
                tri_stamp[t] = cast_id;
            }
            found |= ray_floor(tr + t * 9, p, d, &best);
        }
    }

    if (found)
        for (int k = 0; k < 3; k++)
            hit[k] = p[k] + (int)((d[k] * best) >> 12);
    return found;
}

int ground_collide(const Ground *g, int *x, int *z, int y_lo, int y_hi, int radius)
{
    return ground_collide_n(g, x, z, y_lo, y_hi, radius, 2);
}

int ground_collide_n(const Ground *g, int *x, int *z, int y_lo, int y_hi, int radius, int passes)
{
    int pushes = 0;
    // Two passes settle corners where two walls push against each other.
    for (int pass = 0; pass < passes; pass++)
    {
        int cx0 = (*x - radius - g->origin_x) / g->cell_size, cx1 = (*x + radius - g->origin_x) / g->cell_size;
        int cz0 = (*z - radius - g->origin_z) / g->cell_size, cz1 = (*z + radius - g->origin_z) / g->cell_size;
        if (*x - radius < g->origin_x) cx0 = -1;
        if (*z - radius < g->origin_z) cz0 = -1;
        for (int cz = cz0 < 0 ? 0 : cz0; cz <= cz1 && cz < g->cells_z; cz++)
            for (int cx = cx0 < 0 ? 0 : cx0; cx <= cx1 && cx < g->cells_x; cx++)
            {
                int cell = cz * g->cells_x + cx;
                for (u32 i = g->wall_cell_start[cell]; i < g->wall_cell_start[cell + 1]; i++)
                {
                    const s32 *w = g->wall + g->wall_index[i] * 6;
                    if (w[5] <= y_lo || w[4] >= y_hi)
                        continue;   // entirely below the step height or above the head
                    pushes += push_out(w, x, z, radius);
                }
            }
    }
    return pushes;
}
