#ifndef MUSIC_H__
#define MUSIC_H__

#include <nds.h>

enum { MUSIC_TITLE, MUSIC_VILLAGE, MUSIC_COUNT };

int music_init(const void *data, size_t size);

void music_play(int track, int volume);

void music_volume(int volume);
int music_asked(void);
void music_level(int percent);
void music_stop(void);

#endif
