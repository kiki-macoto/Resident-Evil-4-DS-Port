#ifndef ENEMY_H__
#define ENEMY_H__

#include "anim.h"
#include "ground.h"
#include "model.h"
#include "objects.h"
#include "stages.h"

#define ENEMY_MAX 12

enum { WEAPON_NONE, WEAPON_SICKLE, WEAPON_AXE, WEAPON_FORK, WEAPON_BOARD, WEAPON_XBOW, WEAPON_STAFF, WEAPON_KINDS };

enum { ENEMY_IDLE, ENEMY_WALK, ENEMY_SWIPE, ENEMY_LUNGE, ENEMY_GRAB, ENEMY_HIT, ENEMY_DYING, ENEMY_DEAD };

typedef struct {
    int pos[3];
    int feet;
    int yaw;
    int state;
    int frame;
    int hp;
    const FoeKind *kind;
    int flinch;
    int attack;
    int enmity;
    int shove;
    int cooldown;
    int struck;
    int spotted;
    int spot_told;
    int active;
    int dead_frames;
    int falling, fall_to, fall_vy;
    int weapon;
    int boss;
    int drop;
    int thrower;
    int daze;
    int stuck;
    int detour;
    int detour_dir[2];
    int detour_side;
    int climb;
    int climb_from[3], climb_to[3];
    int pose[MODEL_MAX_NODES][12];
    int pose_dirty;
    int plaga;
    int plg_clip, plg_frame;
} Enemy;

typedef struct {
    const Model *model;
    const AnimSet *anims;
    const Ground *ground;
    const Objects *objects;
    const Model *weapon_model[WEAPON_KINDS];
    const AnimSet *weapon_anims[4];
    const Model *boss_model;
    const AnimSet *boss_anims;
    const Model *plaga_model;
    const AnimSet *plaga_anims;
    Enemy e[ENEMY_MAX];
    int count;
    int attacker;
    int rest;
    int kills;
    int sight;
    int hide_below;
    int watch;
    const int *ally;
    int noise;
    int crit_hit;
    u32 tick, rng;
} Enemies;

typedef struct {
    int swiped_by;
    int grabbed_by;
    int bite;
    int spotted;
    int attacking;
    int swing;
    int ally_hit;
    int thrown;
    int shot;
    int shot_at[3];
} EnemyEvents;

void enemies_init(Enemies *s, const Model *m, const AnimSet *a, const Ground *g, const Objects *o,
                  const int (*xyz)[3], int count, int yaw);

void enemies_respawn(Enemies *s, int i, const int xyz[3], int yaw);

void enemies_activate(Enemies *s, int i, const int *at);

int enemies_active_standing(const Enemies *s);
int enemies_left(const Enemies *s);

void enemies_update(Enemies *s, const int target[3], int vulnerable, EnemyEvents *ev);

void enemies_hold_at(Enemies *s, int i, const int pos[3], int dist);

void enemies_release(Enemies *s, int i);

int enemies_push(const Enemies *s, int *x, int *z, int radius);

void enemies_draw(Enemies *s, u32 poly_fmt, const int eye[3], int max);

int enemies_raycast(const Enemies *s, const int p[3], const int d[3], int *t_best, int *part);

int enemies_shot(Enemies *s, int i, int power, int part, int crit_rate, int crit_wep);

void enemies_damage(Enemies *s, int i, int damage);

void enemies_set_kind(Enemies *s, int i, const FoeKind *kind);

void enemies_alert(Enemies *s, int i);
void enemies_daze(Enemies *s, int i);

int enemies_kickable(const Enemies *s, const int pos[3], int yaw);

int enemies_kick(Enemies *s, const int pos[3], int yaw, int damage);

void enemies_make_thrower(Enemies *s, int i);
void enemies_perch(Enemies *s, int i, int y);

void enemies_set_look(Enemies *s, const Model *m, const AnimSet *a, const AnimSet *one_hand);

void enemies_spawn_boss(Enemies *s, int i, const int xyz[3], int yaw, const Model *m, const AnimSet *a);

int enemies_slash(Enemies *s, const int pos[3], int yaw, int range, int damage);

void enemies_arm(Enemies *s, const Model *const models[WEAPON_KINDS], const AnimSet *one_hand, const AnimSet *fork);

void enemies_arm_cultists(Enemies *s, const Model *board, const Model *xbow, const Model *staff, const AnimSet *w1, const AnimSet *w2);
int enemies_blow(const Enemy *e);

void enemies_set_plaga(Enemies *s, const Model *m, const AnimSet *a);

int enemies_alive(const Enemies *s);

#endif
