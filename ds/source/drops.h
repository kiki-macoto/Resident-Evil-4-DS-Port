#ifndef DROPS_H__
#define DROPS_H__

#include <nds.h>

#include "sheet.h"

#define DROPS_MAX 8

enum { DROP_AMMO, DROP_HERB, ITEM_GUN, DROP_TREASURE, DROP_MONEY };

typedef struct {
    int pos[3];
    int type;
    int delay;
    int age;
    int active;
    int burst;
    int land[3];
    int aloft;
    int value;
} Drop;

typedef struct {
    Drop d[DROPS_MAX];
    int picked_value;
    int ammo_gun;
    const Sheet *star, *beam, *spark, *items;
} Drops;

void drops_init(Drops *s, const Sheet *star, const Sheet *beam, const Sheet *spark, const Sheet *items);
void drops_clear(Drops *s);

void drops_spawn(Drops *s, const int ground[3], int type, int delay);
void drops_spawn_value(Drops *s, const int ground[3], int type, int delay, int value);

void drops_place_treasure(Drops *s, const int pos[3], const int land[3], int value);

int drops_raycast(const Drops *s, const int p[3], const int d[3], int *t_best);
void drops_knock(Drops *s, int i);

int drops_in_reach(const Drops *s, const int leon[3]);
int drops_update(Drops *s, const int leon[3], int take);

void drops_draw(Drops *s);

void drops_icon(int type, int value, int *sx, int *sy);

#endif
