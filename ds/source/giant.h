#ifndef GIANT_H__
#define GIANT_H__

#include <nds.h>

#include "anim.h"
#include "ground.h"
#include "model.h"

enum { GIANT_WALK, GIANT_ATTACK, GIANT_HIT, GIANT_KNEEL, GIANT_RISE, GIANT_DEAD };

typedef struct {
    Model model;
    AnimSet anims;
    void *model_buf, *anim_buf;
    int loaded;
    int shared;
    int on;
    int pos[3], feet, yaw;
    int state, frame, clip;
    int hp;
    int cuts;
    int life;
    int held;
    int timer;
    int struck;
    int pose[MODEL_MAX_NODES][12];
} Giant;

typedef struct {
    int hit_leon;
    int stomp;
    int down;
} GiantEvents;

int giant_load(Giant *g);
void giant_unload(Giant *g);
void giant_share(Giant *g, const Giant *from);
void giant_place(Giant *g, const Ground *gr, int x, int z, int ref_y, int yaw);
void giant_update(Giant *g, const Ground *gr, const int leon_pos[3], int vulnerable, GiantEvents *ev);

int giant_raycast(const Giant *g, const int p[3], const int d[3], int *t_best);
void giant_shot(Giant *g, int power);

int giant_cuttable(const Giant *g, const int pos[3]);
void giant_cut(Giant *g);
int giant_blast(Giant *g);

void giant_push(const Giant *g, int *x, int *z, int r);

void giant_draw(Giant *g, u32 poly_fmt);

#endif
