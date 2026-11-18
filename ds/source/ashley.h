#ifndef ASHLEY_H__
#define ASHLEY_H__

#include <nds.h>

#include "anim.h"
#include "ground.h"
#include "model.h"

enum { ASH_FOLLOW, ASH_WAIT, ASH_HURT, ASH_DOWN };

typedef struct {
    Model model;
    AnimSet anims;
    void *model_buf, *anim_buf;
    int loaded;
    int on;
    int pos[3], feet, yaw;
    int state, frame;
    int clip;
    int hp;
    int duck;
    int wait;
    int ladder;
    int lad_foot[3], lad_dir[2], lad_top, lad_yaw, lad_up, lad_t;
    int pose[MODEL_MAX_NODES][12];
} Ashley;

int ashley_load(Ashley *a);
void ashley_unload(Ashley *a);

void ashley_place(Ashley *a, const Ground *g, int x, int z, int ref_y, int yaw);

void ashley_update(Ashley *a, const Ground *g, const int leon_pos[3], int leon_yaw);

void ashley_use_ladder(Ashley *a, const int foot[3], const int dir[2], int top, int yaw, int up);

int ashley_hit(Ashley *a, int damage);

int ashley_raycast(const Ashley *a, const int p[3], const int d[3], int *t_best);

void ashley_push(const Ashley *a, int *x, int *z, int r);

void ashley_draw(Ashley *a, u32 poly_fmt);

#define ASHLEY_HP 100
enum { LADDER_GOING = 1, LADDER_ON };

#endif
