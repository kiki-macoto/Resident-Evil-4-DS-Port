#ifndef MISSILE_H__
#define MISSILE_H__

#include <nds.h>

#include "ground.h"
#include "sheet.h"

#define MISSILES_MAX 6

typedef struct {
    int pos[3], vel[3];
    int t;
    int boom;
    int land;
} Missile;

typedef struct {
    Missile m[MISSILES_MAX];
    const Sheet *fx;
    int fire;
} Missiles;

void missiles_init(Missiles *s, const Sheet *fx, int fire);

void missiles_throw(Missiles *s, const int from[3], const int to[3]);

int missiles_update(Missiles *s, const Ground *g);

void missiles_draw(Missiles *s);

#endif
