#include <string.h>

#include "music.h"

typedef struct {
    const u8 *data;
    u32 bytes;
} Track;

static Track tracks[MUSIC_COUNT];
static int count, rate;
static int playing = -1, channel = -1, current_volume = -1;
static int level = 100, asked = 0;   // master volume (percent) and the volume last asked for

static u16 rd16(const u8 *p) { return p[0] | p[1] << 8; }
static u32 rd32(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }

int music_init(const void *data, size_t size)
{
    const u8 *p = data, *end = p + size;
    if (size < 8 || memcmp(p, "RMU1", 4) != 0)
        return -1;
    count = rd16(p + 4);
    rate = rd16(p + 6);
    if (count > MUSIC_COUNT)
        count = MUSIC_COUNT;
    p += 8;
    for (int i = 0; i < count; i++)
    {
        if (p + 4 > end)
            return -2;
        tracks[i].bytes = rd32(p);
        tracks[i].data = p + 4;
        p += 4 + tracks[i].bytes;
        if (p > end)
            return -2;
    }
    return 0;
}

void music_play(int track, int volume)
{
    if (track == playing)
    {
        music_volume(volume);
        return;
    }
    music_stop();
    if (track < 0 || track >= count)
        return;
    // Loop from word 1, right after the ADPCM header: the hardware keeps the decoder state it
    // had there and restores it on every repeat. The length excludes that header word.
    const Track *t = &tracks[track];
    asked = volume;
    volume = volume * level / 100;
    channel = soundPlaySample(t->data, SoundFormat_ADPCM, t->bytes - 4, rate, volume, 64, true, 1);
    playing = channel >= 0 ? track : -1;
    current_volume = volume;
}

void music_volume(int volume)
{
    asked = volume;
    volume = volume * level / 100;
    if (channel >= 0 && volume != current_volume)
    {
        soundSetVolume(channel, volume);
        current_volume = volume;
    }
}

void music_level(int percent)
{
    level = percent;
    music_volume(asked);
}

void music_stop(void)
{
    if (channel >= 0)
        soundKill(channel);
    channel = playing = -1;
}
