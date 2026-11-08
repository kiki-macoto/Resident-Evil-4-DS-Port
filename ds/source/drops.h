// Items dropped by Ganados (handgun ammo, green herbs): shown with the original's glow effects
// (light column fix_all #7, twinkling star fix_all #0) and the item's icon (2d_itemic_00),
// picked up by walking over them, with a burst of sparkles (eff_all).
#ifndef DROPS_H__
#define DROPS_H__

#include <nds.h>

#include "sheet.h"

#define DROPS_MAX 8

enum { DROP_AMMO, DROP_HERB, ITEM_GUN, DROP_TREASURE, DROP_MONEY };   // ITEM_GUN: only an icon (the item screen)

typedef struct {
    int pos[3];    // f32, on the ground
    int type;
    int delay;     // frames until it appears (the Ganado is still falling)
    int age;       // frames since it appeared
    int active;
    int burst;     // frames left of the pickup sparkle (after it was taken)
    int land[3];   // a treasure up high: where a shot knocks it (f32, on the floor below)
    int aloft;     // up out of reach until shot down
    int value;     // DROP_MONEY: PTAS
} Drop;

typedef struct {
    Drop d[DROPS_MAX];
    int picked_value;   // the value of the item drops_update() last returned
    const Sheet *star, *beam, *spark, *items;
} Drops;

void drops_init(Drops *s, const Sheet *star, const Sheet *beam, const Sheet *spark, const Sheet *items);
void drops_clear(Drops *s);

// Drops an item at a ground point after `delay` frames.
void drops_spawn(Drops *s, const int ground[3], int type, int delay);
// The same with a value (DROP_MONEY: PTAS).
void drops_spawn_value(Drops *s, const int ground[3], int type, int delay, int value);

// A treasure at pos (f32), landing at land (the floor below, clear of walls): one up high is taken only
// after a shot knocks it down.
void drops_place_treasure(Drops *s, const int pos[3], const int land[3]);

// Shot ray p + d*t, 0 < t < *t_best: a treasure up high hit updates *t_best and returns its
// index, else -1. drops_knock() brings it down to the floor.
int drops_raycast(const Drops *s, const int p[3], const int d[3], int *t_best);
void drops_knock(Drops *s, int i);

// Advances the effects; returns the type of an item Leon (at pos, f32) walked over, or -1.
// leon NULL: nothing is taken (he's dead).
int drops_update(Drops *s, const int leon[3]);

// Draws the visible drops and pickup bursts (after billboard_begin()).
void drops_draw(Drops *s);

// Icon of an item in the items sheet (32 x 24): x, y.
void drops_icon(int type, int *sx, int *sy);

#endif
