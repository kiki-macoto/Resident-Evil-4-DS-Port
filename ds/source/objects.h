#ifndef OBJECTS_H__
#define OBJECTS_H__

#include <nds.h>

#include "ground.h"
#include "model.h"
#include "sheet.h"

#define OBJECTS_MAX 24

enum { OBJ_GATE_LEAF, OBJ_DOOR, OBJ_LADDER, OBJ_CRATE, OBJ_BARREL, OBJ_MEDAL, OBJ_SWING, OBJ_BELL, OBJ_HANG };

enum { ACT_NONE, ACT_KICK_GATE, ACT_KICK_CRATE, ACT_LADDER_UP, ACT_LADDER_DOWN, ACT_OPEN_DOOR };

typedef struct {
    int kind;
    int hinge[2], end[2];
    int pos[3];
    int top;
    int dir[2];
    int yaw;
    int swing;
    int open;
    int opening;
    int pair;
    int alive;
    int fuse;
    int drop;
    int drawn;
    int locked;
    int keyed;
} Obj;

#define BOOMS_MAX 4
typedef struct { int pos[3]; int t; } Boom;
typedef struct { int pos[3]; int t; } Puff;

typedef struct {
    Obj o[OBJECTS_MAX];
    int count;
    const Model *door, *ladder, *barrel, *crate, *medal;
    const Model *bell, *hang;
    const int *leon;
    const Sheet *boom_fx, *spark_fx;
    const Ground *ground;
    Boom boom[BOOMS_MAX];
    Puff puff[BOOMS_MAX];
    int medals, medals_hit;
    int cold;
    u32 tick;
} Objects;

typedef struct {
    int exploded;
    int explode_pos[3];
    int small;
    int door;
} ObjEvents;

void objects_init(Objects *s, const Ground *g, const Model *door, const Model *ladder, const Model *barrel,
                  const Model *crate, const Model *medal, const Sheet *boom_fx, const Sheet *spark_fx);
void objects_reset(Objects *s);

void objects_clear(Objects *s);
int objects_add_barrel(Objects *s, int x, int z, int ref_y);
int objects_add_barrel_at(Objects *s, int x, int y, int z);
int objects_add_crate(Objects *s, int x, int z, int ref_y, int drop);
int objects_add_door(Objects *s, int x, int z, int ref_y, int yaw);
int objects_add_bell(Objects *s, int x, int y, int z);
int objects_add_ladder(Objects *s, int x, int z, int ref_y, int yaw);
int objects_add_hang(Objects *s, int x, int y, int z, int yaw);

void objects_open_door(Objects *s, int i, const int from[3]);
int objects_door_at(const Objects *s, int x, int z, int reach);

void objects_update(Objects *s, ObjEvents *ev);
void objects_boom(Objects *s, const int pos[3]);

void objects_collide(const Objects *s, int *x, int *z, int feet, int radius);

void objects_draw(Objects *s, const int eye[3], u32 poly_fmt);
void objects_draw_fx(Objects *s);

int objects_raycast(const Objects *s, const int p[3], const int d[3], int *t_best);

int objects_hit(Objects *s, int i);

int objects_action(const Objects *s, const int pos[3], int feet, int yaw, int *which);

void objects_open(Objects *s, int i);

int objects_door(const Objects *s);

#endif
