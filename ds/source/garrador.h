#ifndef GARRADOR_H__
#define GARRADOR_H__

#include <nds.h>

#include "anim.h"
#include "ground.h"
#include "model.h"

enum { GAL_LISTEN, GAL_CHARGE, GAL_SLASH, GAL_STUCK, GAL_HIT, GAL_DEAD };

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
    int flinch;
    int target[3];
    int timer;
    int struck;
    int pose[MODEL_MAX_NODES][12];
} Garrador;

typedef struct {
    int hit_leon;
    int hit_ally;
    int roar;
    int down;
} GarradorEvents;

int garrador_load(Garrador *g);
void garrador_unload(Garrador *g);
void garrador_share(Garrador *g, const Garrador *from);
void garrador_place(Garrador *g, const Ground *gr, int x, int z, int ref_y, int yaw);
void garrador_update(Garrador *g, const Ground *gr, const int leon_pos[3], const int *ally_pos, int vulnerable, GarradorEvents *ev);

void garrador_hear(Garrador *g, const int pos[3], int range);

int garrador_raycast(const Garrador *g, const int p[3], const int d[3], int *t_best, int *back);
void garrador_shot(Garrador *g, int power, int back);

void garrador_push(const Garrador *g, int *x, int *z, int r);

void garrador_draw(Garrador *g, u32 poly_fmt);

#endif
