// Ashley (stages 5 and 6): follows Leon, crouches when he aims her way, Ganados go for her
// when she is the nearer. Model and animations (ch_ash, from the stage 5 archive) are read
// from NitroFS while one of her stages is played.
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
    int on;                   // in the stage being played
    int pos[3], feet, yaw;    // pos: model origin (f32); feet: floor height
    int state, frame;         // ASH_*; f32 position in the clip
    int clip;                 // clip playing (to restart it on changes)
    int hp;
    int duck;                 // set by the caller: Leon is aiming her way
    int wait;                 // stays where the mission puts her until Leon reaches her
    // A ladder Leon has used: she goes to its foot (or its top) and climbs it after him.
    int ladder;               // 0 none, LADDER_GOING, LADDER_ON
    int lad_foot[3], lad_dir[2], lad_top, lad_yaw, lad_up, lad_t;
    int pose[MODEL_MAX_NODES][12];
} Ashley;

// Reads her model and animations (once). Returns 0 on success.
int ashley_load(Ashley *a);
void ashley_unload(Ashley *a);

// Puts her at (x, z) (f32) on the floor nearest below ref_y, facing yaw; full health.
void ashley_place(Ashley *a, const Ground *g, int x, int z, int ref_y, int yaw);

// One 60 Hz frame: keeps to Leon's left, a little behind (walks, runs to catch up).
void ashley_update(Ashley *a, const Ground *g, const int leon_pos[3], int leon_yaw);

// Leon has gone up (up = 1) or down a ladder: foot (f32, its floor), dir towards the platform
// (f32 unit, x z), top (the platform's height), yaw (facing on it, as Leon's). She follows.
void ashley_use_ladder(Ashley *a, const int foot[3], const int dir[2], int top, int yaw, int up);

// A hit (a Ganado's swing, or a stray shot). Returns 1 if it took her down.
int ashley_hit(Ashley *a, int damage);

// Nearest hit on her by the ray p + d*t, 0 < t < *t_best (as enemies_raycast()).
int ashley_raycast(const Ashley *a, const int p[3], const int d[3], int *t_best);

// Pushes (x, z) out of her (radius r around the point).
void ashley_push(const Ashley *a, int *x, int *z, int r);

void ashley_draw(Ashley *a, u32 poly_fmt);

#define ASHLEY_HP 100
enum { LADDER_GOING = 1, LADDER_ON };

#endif
