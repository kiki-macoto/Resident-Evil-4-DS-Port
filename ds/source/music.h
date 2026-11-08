// Background music: looping IMA-ADPCM tracks (RMU1 file from re4zeebo -DsMusic), each played
// on one hardware channel. Needs sfx_init() first (it switches sound on).
#ifndef MUSIC_H__
#define MUSIC_H__

#include <nds.h>

// Order of the tracks (re4zeebo -DsMusic 'music=common/snd:st001,st003').
enum { MUSIC_TITLE, MUSIC_VILLAGE, MUSIC_COUNT };

// Parses the file. Returns 0 on success.
int music_init(const void *data, size_t size);

// Starts a track looping at volume 0..127; a track already playing just takes the volume.
void music_play(int track, int volume);

void music_volume(int volume);
// Master volume, percent (the pause menu's Sound setting); applies at once.
void music_level(int percent);
void music_stop(void);

#endif
