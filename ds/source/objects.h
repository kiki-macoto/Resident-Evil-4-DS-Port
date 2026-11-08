// Stage 1 gimmicks: the village gates (kicked open), a house door Ganados come out of, ladders
// up to the walkways, crates (break: maybe an item), explosive barrels and blue medallions.
// Where they are comes from the game's stage data (stg_data_01.bin: the door/gate points, the
// action points, the exit quads of its collision data); see objects.c.
#ifndef OBJECTS_H__
#define OBJECTS_H__

#include <nds.h>

#include "ground.h"
#include "model.h"
#include "sheet.h"

#define OBJECTS_MAX 24

// OBJ_SWING: a mission file's door (kind 01), drawn, swinging open and shut again;
// OBJ_BELL: a mission file's bell (kind 09), hanging from its point, rung by a shot.
enum { OBJ_GATE_LEAF, OBJ_DOOR, OBJ_LADDER, OBJ_CRATE, OBJ_BARREL, OBJ_MEDAL, OBJ_SWING, OBJ_BELL, OBJ_HANG };

// What Leon can do at an object (A when not aiming).
enum { ACT_NONE, ACT_KICK_GATE, ACT_KICK_CRATE, ACT_LADDER_UP, ACT_LADDER_DOWN, ACT_OPEN_DOOR };

typedef struct {
    int kind;
    int hinge[2], end[2];   // gate leaves / door: hinge and free end when closed (f32 x, z)
    int pos[3];             // crate / barrel / medal (hang point) / ladder base, f32
    int top;                // ladder: platform height (f32); gate / door: leaf height
    int dir[2];             // ladder: unit vector from the base towards the platform (f32)
    int yaw;                // ladder: Leon's facing while climbing up (libnds angle)
    int swing;              // gate / door: +1 / -1, the way the leaf opens
    int open;               // gate / door: 0 (closed) .. 4096 (open)
    int opening;
    int pair;               // gate leaf: the other leaf (both open together), else -1
    int alive;              // crate / barrel / medal: not broken yet
    int fuse;               // barrel: frames until it blows up, -1 = not lit; swinging door:
                            // frames it stays open; bell: frames left of its swing
    int drop;               // crate from a mission file: its drop code (see main.c), -1 = a random item
    int drawn;              // ladder: already part of the stage model
    int locked;             // mission door: a wall behind it, it never opens
} Obj;

#define BOOMS_MAX 4
typedef struct { int pos[3]; int t; } Boom;   // explosion: frames since it went off (-1 = free)
typedef struct { int pos[3]; int t; } Puff;   // a crate breaking

typedef struct {
    Obj o[OBJECTS_MAX];
    int count;
    const Model *door, *ladder, *barrel, *crate, *medal;
    const Model *bell, *hang;   // (set after objects_init())
    const int *leon;        // Leon's position (f32): a door doesn't shut on him (set by the game)
    const Sheet *boom_fx, *spark_fx;
    const Ground *ground;
    Boom boom[BOOMS_MAX];
    Puff puff[BOOMS_MAX];
    int medals, medals_hit;
    int cold;               // the barrels are liquid nitrogen tanks (stage 12): a white burst
    u32 tick;
} Objects;

// What happened this frame (positions f32; -1 / none when nothing).
typedef struct {
    int exploded;           // a barrel went off at explode_pos this frame
    int explode_pos[3];
    int small;              // ...and it was a hanging canister's blast, not a barrel's
    int door;               // a swinging door started opening or shutting (index), -1 = none
} ObjEvents;

void objects_init(Objects *s, const Ground *g, const Model *door, const Model *ladder, const Model *barrel,
                  const Model *crate, const Model *medal, const Sheet *boom_fx, const Sheet *spark_fx);
void objects_reset(Objects *s);   // gates shut, everything back in place

// Other stages: no gimmicks, then barrels (tanks) added one by one, standing on the floor
// at (x, z) found from ref_y. Returns 0 if there is no room or no floor there.
void objects_clear(Objects *s);
int objects_add_barrel(Objects *s, int x, int z, int ref_y);
// A barrel standing exactly at (x, y, z), floor or not (stage 7's up on the walls).
int objects_add_barrel_at(Objects *s, int x, int y, int z);
// A crate holding the item of drop code `drop` (-1: maybe a random item, as in stage 1).
int objects_add_crate(Objects *s, int x, int z, int ref_y, int drop);
// A door on its hinge (x, z), facing yaw (the door model's leaf runs 13 units along its -x),
// on the floor found from ref_y; a bell hanging from (x, y, z).
int objects_add_door(Objects *s, int x, int z, int ref_y, int yaw);
int objects_add_bell(Objects *s, int x, int y, int z);
// A ladder (kind 02) at its foot (x, z), facing yaw with its back to the platform (the floor
// CLIMB_BACK behind it, within 40 above the foot). Returns 0 if there is no platform there.
int objects_add_ladder(Objects *s, int x, int z, int ref_y, int yaw);
// A canister on chains (kind 05), its point at (x, y, z): a shot drops it, and it blows up on
// the floor below.
int objects_add_hang(Objects *s, int x, int y, int z, int yaw);

// Swinging door i opens away from `from` (f32 x, z, the one opening it).
void objects_open_door(Objects *s, int i, const int from[3]);
// The shut swinging door whose leaf is within `reach` of (x, z), or -1.
int objects_door_at(const Objects *s, int x, int z, int reach);

// Advances swinging leaves, barrel fuses and effects.
void objects_update(Objects *s, ObjEvents *ev);
// An explosion's fire and smoke at pos (a rocket's).
void objects_boom(Objects *s, const int pos[3]);

// Pushes a body (x, z, radius at feet height) out of shut gates / doors, crates and barrels.
void objects_collide(const Objects *s, int *x, int *z, int feet, int radius);

void objects_draw(Objects *s, const int eye[3], u32 poly_fmt);
void objects_draw_fx(Objects *s);   // explosions, splinters (billboards; after billboard_begin())

// Nearest crate / barrel / medal hit by the ray p + d*t with t < *t_best; updates *t_best.
int objects_raycast(const Objects *s, const int p[3], const int d[3], int *t_best);

// A shot or kick landed on object i. Returns 1 if it broke (crate / medal) or was lit (barrel).
int objects_hit(Objects *s, int i);

// The action Leon can take at (pos, feet, facing yaw), and on which object.
int objects_action(const Objects *s, const int pos[3], int feet, int yaw, int *which);

// Opens a gate (both leaves) or the door, swinging.
void objects_open(Objects *s, int i);

// The house door (its index; Ganados come out of it).
int objects_door(const Objects *s);

#endif
