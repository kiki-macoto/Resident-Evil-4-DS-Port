// Garrador (stage 8): ch_gal from the stage 8 archive, read from NitroFS for that stage. He is
// blind: he stands listening, and charges at where he heard a noise (a shot, Leon running near
// him, Leon right beside him), slashing with his claws if Leon is there. A charge that ends on
// a wall sticks his claws in it for a while. Shots in the back (the parasite) hurt him most.
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
    int shared;        // model and clips copied from another one (garrador_share)
    int on;
    int pos[3], feet, yaw;
    int state, frame, clip;
    int hp;
    int target[3];     // where he heard the noise he is charging at
    int timer;         // frames in the current state
    int struck;        // this slash has been checked against Leon
    int pose[MODEL_MAX_NODES][12];
} Garrador;

typedef struct {
    int hit_leon;      // damage dealt to Leon this frame (0 = none)
    int hit_ally;      // damage dealt to Ashley this frame (0 = none)
    int roar;          // he heard something and charges (a cue for his cry)
    int down;          // he fell for good this frame
} GarradorEvents;

int garrador_load(Garrador *g);
void garrador_unload(Garrador *g);
// A second Garrador drawn with the first one's model and clips (loaded once).
void garrador_share(Garrador *g, const Garrador *from);
void garrador_place(Garrador *g, const Ground *gr, int x, int z, int ref_y, int yaw);
// ally_pos: Ashley (NULL if she isn't there); his claws catch her too if she is in front.
void garrador_update(Garrador *g, const Ground *gr, const int leon_pos[3], const int *ally_pos, int vulnerable, GarradorEvents *ev);

// A noise at pos (f32) that carries `range` (f32): he charges at it if he is within range.
void garrador_hear(Garrador *g, const int pos[3], int range);

// Shot ray p + d*t, 0 < t < *t_best: a hit on his body updates *t_best and returns 1, and
// *back says whether it came from behind him (the parasite).
int garrador_raycast(const Garrador *g, const int p[3], const int d[3], int *t_best, int *back);
void garrador_shot(Garrador *g, int damage, int back);

// Pushes (x, z) out of his body.
void garrador_push(const Garrador *g, int *x, int *z, int r);

void garrador_draw(Garrador *g, u32 poly_fmt);

#endif
