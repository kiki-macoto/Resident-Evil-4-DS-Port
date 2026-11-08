// Thrown missiles: dynamite (village) and burning rocks (castle catapults). Each flies in an
// arc to where Leon stood when it was thrown and blows up on landing.
#ifndef MISSILE_H__
#define MISSILE_H__

#include <nds.h>

#include "ground.h"
#include "sheet.h"

#define MISSILES_MAX 6

typedef struct {
    int pos[3], vel[3];   // f32, per frame
    int t;                // frames in flight; -1 = free
    int boom;             // frames since it went off; -1 = still flying (or free)
    int land;             // height it comes down to (f32)
} Missile;

typedef struct {
    Missile m[MISSILES_MAX];
    const Sheet *fx;
    int fire;             // castle: burning rocks (orange), else dynamite (sparks)
} Missiles;

void missiles_init(Missiles *s, const Sheet *fx, int fire);

// Throws one from `from` (f32) to land at `to` (f32), arcing over FLIGHT frames.
void missiles_throw(Missiles *s, const int from[3], const int to[3]);

// One frame. Returns the index of one that went off this frame (its pos is where), or -1.
int missiles_update(Missiles *s, const Ground *g);

void missiles_draw(Missiles *s);   // billboards: call after billboard_begin()

#endif
