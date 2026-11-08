// Verdugo (stage 12): ch_ver from the stage 12 archive, read from NitroFS for that stage. He
// stalks Leon, fast, and strikes with his tail; now and then he leaps up out of sight and drops
// down by Leon. Bullets barely hurt him, unless he is frozen: a liquid nitrogen tank shot
// open near him (main.c) freezes him stiff for a few seconds, and then shots tell.
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
    int timer;         // frames in the current state
    int struck;        // this tail strike has been checked against Leon
    int next_leap;     // frames of stalking before he leaps up again
    int pose[MODEL_MAX_NODES][12];
} Verdugo;

typedef struct {
    int hit_leon;      // damage dealt to Leon this frame (0 = none)
    int landed;        // he dropped down from above this frame
    int thawed;        // the ice gave way this frame
} VerdugoEvents;

int verdugo_load(Verdugo *v);
void verdugo_unload(Verdugo *v);
void verdugo_place(Verdugo *v, const Ground *gr, int x, int z, int ref_y, int yaw);
void verdugo_update(Verdugo *v, const Ground *gr, const int leon_pos[3], int leon_feet, int vulnerable, VerdugoEvents *ev);

// A cold blast at pos (f32): he freezes if within range. Returns 1 if he did.
int verdugo_freeze(Verdugo *v, const int pos[3], int range);

// Shot ray p + d*t, 0 < t < *t_best: a hit on his body updates *t_best and returns 1.
int verdugo_raycast(const Verdugo *v, const int p[3], const int d[3], int *t_best);
void verdugo_shot(Verdugo *v, int damage);

void verdugo_push(const Verdugo *v, int *x, int *z, int r);

void verdugo_draw(Verdugo *v, u32 poly_fmt);
void verdugo_draw_fx(Verdugo *v, const Sheet *fx);   // the frost on him (after billboard_begin())

#endif
