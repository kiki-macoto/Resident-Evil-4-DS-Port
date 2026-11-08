#include <string.h>

#include "missile.h"

#define FLIGHT       75                  // frames from the hand to the ground
#define GRAVITY      floattof32(0.035)   // per frame, per frame
#define BOOM_FRAMES  36

void missiles_init(Missiles *s, const Sheet *fx, int fire)
{
    memset(s, 0, sizeof(*s));
    s->fx = fx;
    s->fire = fire;
    for (int i = 0; i < MISSILES_MAX; i++)
        s->m[i].t = s->m[i].boom = -1;
}

void missiles_throw(Missiles *s, const int from[3], const int to[3])
{
    for (int i = 0; i < MISSILES_MAX; i++)
    {
        Missile *m = &s->m[i];
        if (m->t >= 0 || m->boom >= 0)
            continue;
        memcpy(m->pos, from, sizeof(m->pos));
        // Straight across in FLIGHT frames; up enough that gravity brings it down there.
        m->vel[0] = (to[0] - from[0]) / FLIGHT;
        m->vel[2] = (to[2] - from[2]) / FLIGHT;
        m->vel[1] = (to[1] - from[1]) / FLIGHT + GRAVITY * FLIGHT / 2;
        m->land = to[1];
        m->t = 0;
        m->boom = -1;
        return;
    }
}

int missiles_update(Missiles *s, const Ground *g)
{
    int went = -1;
    for (int i = 0; i < MISSILES_MAX; i++)
    {
        Missile *m = &s->m[i];
        if (m->boom >= 0 && ++m->boom >= BOOM_FRAMES)
            m->boom = -1;
        if (m->t < 0)
            continue;
        m->t++;
        m->pos[0] += m->vel[0];
        m->pos[1] += m->vel[1];
        m->pos[2] += m->vel[2];
        m->vel[1] -= GRAVITY;
        // Lands: down at the height it was thrown at (not on a wall top it passes over), or after
        // twice its flight time.
        int floor = m->land;
        int on = m->vel[1] < 0 && m->pos[1] <= m->land + inttof32(1);
        if (on)
            ground_height(g, m->pos[0], m->pos[2], m->land + inttof32(4), inttof32(4), &floor);
        if (on || m->t > FLIGHT * 2)
        {
            if (on)
                m->pos[1] = floor + inttof32(1);
            m->t = -1;
            m->boom = 0;
            went = i;
        }
    }
    return went;
}

void missiles_draw(Missiles *s)
{
    for (int i = 0; i < MISSILES_MAX; i++)
    {
        Missile *m = &s->m[i];
        if (m->t >= 0)
        {
            // In flight: a burning rock / a lit stick of dynamite, spinning.
            u16 col = s->fire ? RGB15(31, 18, 4) : RGB15(31, 28, 16);
            billboard(s->fx, 0, 0, 64, 64, m->pos, inttof32(4), inttof32(4), m->t * 900, 0, col, 31, 40 + i, 0);
        }
        if (m->boom >= 0)
        {
            // Going off: a growing, fading flash.
            int size = inttof32(10) + m->boom * floattof32(0.8);
            int alpha = 31 - m->boom * 30 / BOOM_FRAMES;
            int p[3] = { m->pos[0], m->pos[1] + inttof32(5), m->pos[2] };
            billboard(s->fx, 0, 0, 64, 64, p, size, size, m->boom * 500, 0, RGB15(31, 20, 6), alpha, 46 + i % 4, 0);
        }
    }
}
