// El Gigante (stages 4 and 11): ch_elg from the stage 4 archive, read from NitroFS while one
// of its stages is played. Walks at Leon and attacks; shots wear him down until he kneels
// with the parasite out of his back: then A, close by, has Leon cut it. Three cuts fell him.
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
    int shared;        // model and clips copied from another one (giant_share)
    int on;
    int pos[3], feet, yaw;
    int state, frame, clip;
    int hp;            // shots left before he kneels
    int cuts;          // parasite cuts so far
    int life;          // what shots and cuts wear down: at 0 he falls for good
    int held;          // set by the caller: Leon is climbing him
    int timer;         // frames in the current state
    int struck;        // this attack has been checked against Leon
    int pose[MODEL_MAX_NODES][12];
} Giant;

typedef struct {
    int hit_leon;      // damage dealt to Leon this frame (0 = none)
    int stomp;         // an attack landed on the ground (shake the view)
    int down;          // he fell for good this frame
} GiantEvents;

int giant_load(Giant *g);
void giant_unload(Giant *g);
// A second giant drawn with the first one's model and clips (loaded once).
void giant_share(Giant *g, const Giant *from);
void giant_place(Giant *g, const Ground *gr, int x, int z, int ref_y, int yaw);
void giant_update(Giant *g, const Ground *gr, const int leon_pos[3], int vulnerable, GiantEvents *ev);

// Shot ray p + d*t, 0 < t < *t_best: a hit on his body updates *t_best and returns 1.
int giant_raycast(const Giant *g, const int p[3], const int d[3], int *t_best);
void giant_shot(Giant *g, int damage);

// Leon at pos facing yaw can cut the parasite (he's kneeling and Leon is at his back).
int giant_cuttable(const Giant *g, const int pos[3]);
void giant_cut(Giant *g);
// A rocket's blast: he falls dead. Returns 1 if he was still up.
int giant_blast(Giant *g);

// Pushes (x, z) out of his body.
void giant_push(const Giant *g, int *x, int *z, int r);

void giant_draw(Giant *g, u32 poly_fmt);

#endif
