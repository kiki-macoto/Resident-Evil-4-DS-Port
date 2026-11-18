#ifndef VERDUGO_H__
#define VERDUGO_H__

#include <nds.h>

#include "anim.h"
#include "ground.h"
#include "model.h"
#include "sheet.h"

enum { VER_STALK, VER_TAIL, VER_HIT, VER_FROZEN, VER_UP, VER_GONE, VER_DROP, VER_DEAD };

typedef struct {
    Model model;
    AnimSet anims;
    void *model_buf, *anim_buf;
    int loaded;
    int on;
    int pos[3], feet, yaw;
    int state, frame, clip;
    int hp;
    int flinch;
    int timer;
    int struck;
    int next_leap;
    int pose[MODEL_MAX_NODES][12];
} Verdugo;

typedef struct {
    int hit_leon;
    int landed;
    int thawed;
} VerdugoEvents;

int verdugo_load(Verdugo *v);
void verdugo_unload(Verdugo *v);
void verdugo_place(Verdugo *v, const Ground *gr, int x, int z, int ref_y, int yaw);
void verdugo_update(Verdugo *v, const Ground *gr, const int leon_pos[3], int leon_feet, int vulnerable, VerdugoEvents *ev);

int verdugo_freeze(Verdugo *v, const int pos[3], int range);

int verdugo_raycast(const Verdugo *v, const int p[3], const int d[3], int *t_best);
void verdugo_shot(Verdugo *v, int power);

void verdugo_push(const Verdugo *v, int *x, int *z, int r);

void verdugo_draw(Verdugo *v, u32 poly_fmt);
void verdugo_draw_fx(Verdugo *v, const Sheet *fx);

#endif
