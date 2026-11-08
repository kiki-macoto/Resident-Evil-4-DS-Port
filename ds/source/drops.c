#include <string.h>

#include "drops.h"

// Translucent polygon IDs (0..63; ones with equal IDs don't blend over each other): one per drop
// and effect layer, clear of the scene's (1..4), shots' (10..14) and HUD's (20..24).
#define ID_BURST 30
#define ID_BEAM  38
#define ID_STAR  46   // the four star quarters share one: they sit side by side
#define ID_DOT   54
#define PICK_RADIUS  inttof32(10)
#define PICK_HEIGHT  inttof32(14)          // and this close in height (walkways above don't count)
#define ALOFT_ABOVE  inttof32(6)           // a treasure this far above its floor is out of reach
#define HIT_RADIUS   inttof32(7)            // a treasure up high, for shots
#define BURST_FRAMES 24
#define POLY_FX      (POLY_FOG)

void drops_init(Drops *s, const Sheet *star, const Sheet *beam, const Sheet *spark, const Sheet *items)
{
    memset(s, 0, sizeof(*s));
    s->star = star;
    s->beam = beam;
    s->spark = spark;
    s->items = items;
}

void drops_clear(Drops *s)
{
    for (int i = 0; i < DROPS_MAX; i++)
        s->d[i].active = s->d[i].burst = 0;
}

void drops_spawn(Drops *s, const int ground[3], int type, int delay)
{
    drops_spawn_value(s, ground, type, delay, 0);
}

void drops_spawn_value(Drops *s, const int ground[3], int type, int delay, int value)
{
    for (int i = 0; i < DROPS_MAX; i++)
    {
        Drop *d = &s->d[i];
        if (d->active || d->burst)
            continue;
        memcpy(d->pos, ground, sizeof(d->pos));
        d->type = type;
        d->value = value;
        d->delay = delay;
        d->age = 0;
        memcpy(d->land, ground, sizeof(d->land));
        d->aloft = 0;
        d->active = 1;
        return;
    }
}

void drops_place_treasure(Drops *s, const int pos[3], const int land[3])
{
    drops_spawn(s, pos, DROP_TREASURE, 0);
    for (int i = 0; i < DROPS_MAX; i++)
    {
        Drop *d = &s->d[i];
        if (d->active && d->type == DROP_TREASURE && d->pos[0] == pos[0] && d->pos[2] == pos[2])
        {
            memcpy(d->land, land, sizeof(d->land));
            d->aloft = pos[1] - land[1] > ALOFT_ABOVE;
            if (!d->aloft)
                memcpy(d->pos, land, sizeof(d->pos));
        }
    }
}

int drops_raycast(const Drops *s, const int p[3], const int d[3], int *t_best)
{
    int found = -1;
    for (int i = 0; i < DROPS_MAX; i++)
    {
        const Drop *t = &s->d[i];
        if (!t->active || !t->aloft)
            continue;
        // Sphere round the glint (3 above the point).
        s64 fx = p[0] - t->pos[0], fy = p[1] - (t->pos[1] + inttof32(3)), fz = p[2] - t->pos[2];
        s64 b = (fx * d[0] + fy * d[1] + fz * d[2]) >> 12;
        s64 c = ((fx * fx + fy * fy + fz * fz) >> 12) - (((s64)HIT_RADIUS * HIT_RADIUS) >> 12);
        s64 disc = ((b * b) >> 12) - c;
        if (disc < 0)
            continue;
        s64 tt = -b - (s64)sqrt64((u64)(disc << 12));
        if (tt <= 0 || tt >= *t_best)
            continue;
        *t_best = (int)tt;
        found = i;
    }
    return found;
}

void drops_knock(Drops *s, int i)
{
    Drop *d = &s->d[i];
    if (!d->active || !d->aloft)
        return;
    d->burst = BURST_FRAMES / 2;   // a puff of sparkles where it was hit...
    memcpy(d->pos, d->land, sizeof(d->pos));   // ...and it lies on the floor below
    d->aloft = 0;
    d->age = 0;
}

int drops_update(Drops *s, const int leon[3])
{
    int picked = -1;
    for (int i = 0; i < DROPS_MAX; i++)
    {
        Drop *d = &s->d[i];
        if (d->burst > 0)
            d->burst--;
        if (!d->active)
            continue;
        if (d->delay > 0)
        {
            d->delay--;
            continue;
        }
        d->age++;
        if (!leon)
            continue;
        int dx = leon[0] - d->pos[0], dz = leon[2] - d->pos[2], dy = leon[1] - d->pos[1];
        if (picked < 0 && !d->aloft && dy < PICK_HEIGHT && dy > -PICK_HEIGHT
            && (s64)dx * dx + (s64)dz * dz < (s64)PICK_RADIUS * PICK_RADIUS)
        {
            picked = d->type;
            s->picked_value = d->value;
            d->active = 0;
            d->burst = BURST_FRAMES;
        }
    }
    return picked;
}

void drops_icon(int type, int *sx, int *sy)
{
    // 2d_itemic_00 rows of 24 pixels: handgun (row 0), handgun ammo box (row 1), green herb (row 2,
    // 3rd); row 3 holds the treasures (pearl, amethyst, ruby: the ruby; money shows the pearl).
    *sx = type == DROP_HERB || type == DROP_TREASURE ? 64 : 0;
    *sy = type == DROP_TREASURE || type == DROP_MONEY ? 72 : type == DROP_HERB ? 48 : type == DROP_AMMO ? 24 : 0;
}

void drops_draw(Drops *s)
{
    for (int i = 0; i < DROPS_MAX; i++)
    {
        Drop *d = &s->d[i];
        int p[3] = { d->pos[0], d->pos[1], d->pos[2] };
        if (d->burst > 0)
        {
            // Pickup: yellow sparkles swelling and fading (eff_all's sparkle picture).
            int t = BURST_FRAMES - d->burst;
            p[1] += inttof32(3) + t * floattof32(0.15);
            int size = inttof32(3) + t * floattof32(0.25);
            billboard(s->spark, 0, 0, 64, 64, p, size, size, t * 300, 0, RGB15(31, 28, 8), 1 + d->burst * 30 / BURST_FRAMES, ID_BURST + i, POLY_FX);
        }
        if (!d->active || d->delay > 0)
            continue;
        int grow = d->age < 20 ? d->age * inttof32(1) / 20 : inttof32(1);   // fades/grows in
        int pulse = sinLerp(d->age * 300);                                 // -1..1, ~1.4 s period

        // Light column (fix_all #7): a tall soft beam standing on the drop.
        int b[3] = { p[0], p[1] + inttof32(6), p[2] };
        billboard(s->beam, 0, 0, 32, 64, b, mulf32(inttof32(6), grow), inttof32(14), 0, 0, RGB15(31, 30, 20),
                  20 + (pulse > 0 ? pulse : 0) * 10 / 4096, ID_BEAM + i, POLY_FX);

        // The item itself, floating and bobbing inside the column.
        int ix, iy;
        drops_icon(d->type, &ix, &iy);
        int c[3] = { p[0], p[1] + inttof32(3) + pulse / 4, p[2] };
        billboard(s->items, ix, iy, 32, 24, c, mulf32(inttof32(4), grow), mulf32(inttof32(3), grow), 0, 0,
                  RGB15(31, 31, 31), 31, ID_STAR + i, POLY_FX);

        // Twinkling star (fix_all #0): its top-left quarter mirrored four ways around the
        // centre, as the original's model does, turning and pulsing.
        int st[3] = { p[0], c[1] + inttof32(2), p[2] };
        int size = mulf32(inttof32(2) + pulse / 2, grow), a = 22 + pulse * 8 / 4096;
        int ang = d->age * 120;
        for (int q = 0; q < 4; q++)
        {
            // Quarter q (left/right, top/bottom): the picture's inner corner (32, 32) sits on
            // the centre, mirrored outwards.
            int one = inttof32(1);
            int x0 = q & 1 ? one : -one, y0 = q & 2 ? -one : one;
            billboard_rect(s->star, 0, 0, 32, 32, st, size, size, x0, y0, 0, 0, ang, 0,
                           RGB15(31, 31, 26), a, ID_STAR + i, POLY_FX);
        }
        // Soft glow dot under the star (fix_all #0, bottom-right quarter).
        billboard(s->star, 32, 32, 32, 32, st, mulf32(inttof32(3), grow), mulf32(inttof32(3), grow), 0, 0,
                  RGB15(31, 31, 24), 12 + (pulse > 0 ? pulse : 0) * 8 / 4096, ID_DOT + i, POLY_FX);
    }
}
