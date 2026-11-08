// Sound effects from the game (RSN1 bank from re4zeebo -DsSound), played on the DS's
// hardware channels through libnds' sound API.
#ifndef SFX_H__
#define SFX_H__

#include <nds.h>

// Order of the bank (re4zeebo -DsSound 'sfx=common/snd:wep15_shot5,fn57_kara,...').
enum {
    SFX_SHOT,           // wep15_shot5: handgun shot
    SFX_EMPTY,          // fn57_kara: dry fire, empty magazine
    SFX_RELOAD,         // wep15_reload_re
    SFX_WALL,           // bogan_kabe: bullet into a wall
    SFX_GANADO_HIT,     // v_damege03: villager hurt
    SFX_GANADO_SPOT,    // v_fd1: villager shout on spotting Leon
    SFX_GANADO_ATTACK,  // ikaku01: shout as an attack starts
    SFX_SWING,          // kazekiri: arms swinging (swipe)
    SFX_GANADO_DOWN,    // zea_c_down: going down
    SFX_LEON_HURT,      // leon_dam1
    SFX_LEON_HURT2,     // leon_dam2: bitten / falling dead
    SFX_BREAK_FREE,     // escape_hit: kick out of a grab
    SFX_MENU,           // sub_kettei2: menu choice made
    SFX_ITEM_GET,       // itemget_fixeq2: picking up an item
    SFX_BELL,           // demo_kane: the church bell (stage clear)
    SFX_DOOR_KICK,      // door_w01_kick: a gate kicked open
    SFX_BREAK,          // tate_break1: wood breaking (crates, medallions)
    SFX_EXPLODE,        // handbomb: a barrel going off
    SFX_KNIFE,          // knife: a knife slash
    SFX_COUNT
};

// Parses the bank and switches sound on. Returns 0 on success.
int sfx_init(const void *bank, size_t size);

// Plays a sound (volume 0..127, pan 0 left .. 64 centre .. 127 right). Returns the channel,
// or -1 (no sound).
int sfx_play(int id, int volume, int pan);
// Master volume for every sound, percent (100 = as given).
void sfx_level(int percent);

// Plays a sound coming from world point `pos` (f32) as heard by a listener at `ear` facing
// `yaw` (libnds angle): quieter with distance, panned to its side.
int sfx_play_at(int id, int volume, const int pos[3], const int ear[3], int yaw);

// Stops a sound started by sfx_play*() (ignored for -1).
void sfx_stop(int channel);

#endif
