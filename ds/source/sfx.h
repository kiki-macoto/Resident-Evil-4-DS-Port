#ifndef SFX_H__
#define SFX_H__

#include <nds.h>

enum {
    SFX_SHOT,
    SFX_EMPTY,
    SFX_RELOAD,
    SFX_WALL,
    SFX_GANADO_HIT,
    SFX_GANADO_SPOT,
    SFX_GANADO_ATTACK,
    SFX_SWING,
    SFX_GANADO_DOWN,
    SFX_LEON_HURT,
    SFX_LEON_HURT2,
    SFX_BREAK_FREE,
    SFX_MENU,
    SFX_ITEM_GET,
    SFX_BELL,
    SFX_DOOR_KICK,
    SFX_BREAK,
    SFX_EXPLODE,
    SFX_KNIFE,
    SFX_COUNT
};

int sfx_init(const void *bank, size_t size);

int sfx_play(int id, int volume, int pan);
void sfx_level(int percent);

int sfx_play_at(int id, int volume, const int pos[3], const int ear[3], int yaw);

void sfx_stop(int channel);

#endif
