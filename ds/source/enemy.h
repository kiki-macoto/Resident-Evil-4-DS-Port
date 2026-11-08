// Ganados: placement, a simple approach behaviour, attacks (swipe, lunge into a grab),
// shot hits, hit reactions, going down.
#ifndef ENEMY_H__
#define ENEMY_H__

#include "anim.h"
#include "ground.h"
#include "model.h"
#include "objects.h"

#define ENEMY_MAX 12

// What a Ganado carries (the Zeebo version's village: sickles, hatchets, pitchforks, and some
// bare-handed). Armed ones swing their weapon instead of grabbing.
enum { WEAPON_NONE, WEAPON_SICKLE, WEAPON_AXE, WEAPON_FORK, WEAPON_KINDS };

// States from ENEMY_HIT on can't attack; from ENEMY_DYING on they're out of the fight.
enum { ENEMY_IDLE, ENEMY_WALK, ENEMY_SWIPE, ENEMY_LUNGE, ENEMY_GRAB, ENEMY_HIT, ENEMY_DYING, ENEMY_DEAD };

typedef struct {
    int pos[3];       // model origin, f32 (pos[1] = feet - model bbox_min.y)
    int feet;         // floor height under the feet, f32
    int yaw;          // facing (model +Z), libnds angle
    int state;
    int frame;        // f32 position in the state's animation
    int hp;
    int cooldown;     // frames until the next attack may start
    int struck;       // the current swipe has already been checked against Leon
    int spotted;      // has seen Leon (walked at him) at least once
    int spot_told;    // ...and that was reported (ev.spotted, for the cry)
    int active;       // in the stage: waiting ones haven't appeared yet, old bodies are cleared
    int dead_frames;  // frames lying dead (the body goes after a while)
    int falling, fall_to, fall_vy;   // dropping off a ledge or roof: to fall_to, speed fall_vy (f32)
    int weapon;       // WEAPON_*
    int boss;         // the chainsaw man (ch_cha): his own model and moves, far tougher
    int drop;         // what the mission file says he leaves (byte 2 of his point), -1 = not set
    int thrower;      // stays put and throws (dynamite, burning rocks) instead of closing in
    int daze;         // frames left stunned after a head shot (Leon can kick it), 0 = not
    int pose[MODEL_MAX_NODES][12];   // last computed pose (refreshed at the 20 Hz animation rate)
    int pose_dirty;
} Enemy;

typedef struct {
    const Model *model;
    const AnimSet *anims;     // order: idle, walk, hit, death, swipe, lunge, grab hold
                              // (re4zeebo -DsAnim ...#6,7,12,21,9,8,10)
    const Ground *ground;
    const Objects *objects;   // shut gates and crates block them too (may be NULL)
    const Model *weapon_model[WEAPON_KINDS];   // drawn in the right hand (node 14); [0] unused
    const AnimSet *weapon_anims[2];            // ch_gna_w00 (one-handed), ch_gna_w01 (pitchfork)
    const Model *boss_model;                   // the chainsaw man (same skeleton as a Ganado)
    const AnimSet *boss_anims;                 // ch_cha.mcas
    Enemy e[ENEMY_MAX];
    int count;
    int attacker;             // the one enemy allowed to attack right now, -1 = none
    int rest;                 // frames until the next attack may start (after any attack)
    int kills;                // enemies that have gone down since enemies_init()
    int sight;                // they notice Leon (and walk at him) within this distance, f32
    int hide_below;           // not drawn with their feet under this (f32; 0: all are), see main.c's galleries
    int watch;                // stages 2-12: they stand facing their way until they notice him
                              // (close by, in front within sight, shot, or a neighbour noticed);
                              // then they keep after him wherever he goes
    const int *ally;          // Ashley's position (f32), or NULL: they go for her when she is nearer
    u32 tick, rng;
} Enemies;

// What happened to Leon during enemies_update(); -1 = nothing.
typedef struct {
    int swiped_by;    // a swipe connected
    int grabbed_by;   // a lunge caught him: the enemy is now holding him (ENEMY_GRAB)
    int bite;         // the holding enemy bit him (once per two loops of the hold)
    int spotted;      // an enemy saw him for the first time (starts walking at him)
    int attacking;    // an enemy started an attack
    int swing;        // a swipe's arms went out (whether or not it connected)
    int ally_hit;     // a swipe caught Ashley
    int thrown;       // a thrower let go of its missile (index), -1 = none
} EnemyEvents;

// Places enemies at world points (x, y, z in world units; y picks the floor: ground, walkway,
// roof) facing `yaw`. All start waiting (inactive): enemies_activate() brings them in.
void enemies_init(Enemies *s, const Model *m, const AnimSet *a, const Ground *g, const Objects *o,
                  const int (*xyz)[3], int count, int yaw);

// Puts enemy i back in the stage fresh (full health, standing) at world point xyz (units),
// facing `yaw`: a new enemy for the timed missions, in the slot of one whose body has gone.
void enemies_respawn(Enemies *s, int i, const int xyz[3], int yaw);

// Brings waiting enemy i into the stage (at its place, or at `at` (f32 x, z) if not NULL).
void enemies_activate(Enemies *s, int i, const int *at);

// In the stage and still standing (not down) / not yet killed at all (waiting ones count).
int enemies_active_standing(const Enemies *s);
int enemies_left(const Enemies *s);

// Moves/animates every enemy for one 60 Hz frame. target is Leon's position (f32);
// attacks only start, and only connect, while `vulnerable` is set.
void enemies_update(Enemies *s, const int target[3], int vulnerable, EnemyEvents *ev);

// Puts the holding enemy i (ENEMY_GRAB) right in front of a target at `pos`: the enemy
// keeps its facing and the target should face it (target yaw = enemy yaw + 180 degrees).
void enemies_hold_at(Enemies *s, int i, const int pos[3], int dist);

// Lets go: enemy i staggers back (Leon broke free or the hold ran out).
void enemies_release(Enemies *s, int i);

// Pushes the point (x, z) out of every standing enemy (bodies of radius `radius` and the
// enemies' own). Returns how many pushed.
int enemies_push(const Enemies *s, int *x, int *z, int radius);

// Draws every enemy (skinned on the matrix stack, like Leon).
// The nearest max of them to eye (f32), nearest first.
void enemies_draw(Enemies *s, u32 poly_fmt, const int eye[3], int max);

// Nearest enemy hit by the ray p + d*t (d unit, f32) with 0 < t < *t_best. On a hit, updates
// *t_best and returns the enemy index, with *head set for hits above the shoulders; else -1.
int enemies_raycast(const Enemies *s, const int p[3], const int d[3], int *t_best, int *head);

// Applies a shot to enemy i: damage, then the hit reaction or going down. Interrupts an attack.
// A head shot that doesn't fell it leaves it dazed: open to a kick.
void enemies_damage(Enemies *s, int i, int damage);

// Enemy i has noticed Leon (watch mode): so do the others near it.
void enemies_alert(Enemies *s, int i);
void enemies_daze(Enemies *s, int i);

// The dazed enemy (standing in kicking range in front of pos/yaw), or -1.
int enemies_kickable(const Enemies *s, const int pos[3], int yaw);

// Leon's kick lands: every standing enemy within range in front of him takes damage and is
// knocked back. Returns how many it hit.
int enemies_kick(Enemies *s, const int pos[3], int yaw, int damage);

// Makes enemy i a thrower (see Enemy.thrower).
void enemies_make_thrower(Enemies *s, int i);
// Keeps enemy i at height y (units), whatever floor is below: on a wall top with none.
void enemies_perch(Enemies *s, int i, int y);

// Swaps the look of the ordinary enemies (the castle's cultists use the Ganados' skeleton and
// clip order): model, base animations and the one-handed weapon set.
void enemies_set_look(Enemies *s, const Model *m, const AnimSet *a, const AnimSet *one_hand);

// Puts the chainsaw man in slot i at world point xyz (units), facing yaw. His model must stay
// loaded while he is on the stage.
void enemies_spawn_boss(Enemies *s, int i, const int xyz[3], int yaw, const Model *m, const AnimSet *a);

// A knife slash: the nearest standing enemy within range in front of pos/yaw takes damage
// (a flinch, no knock-back). Returns its index, or -1.
int enemies_slash(Enemies *s, const int pos[3], int yaw, int range, int damage);

// Gives the enemies their weapons' models and animation sets (call after enemies_init()).
void enemies_arm(Enemies *s, const Model *const models[WEAPON_KINDS], const AnimSet *one_hand, const AnimSet *fork);

// Enemies still standing.
int enemies_alive(const Enemies *s);

#endif
