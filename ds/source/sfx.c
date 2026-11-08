#include <string.h>

#include "sfx.h"

#define HEAR_RANGE inttof32(160)   // sounds fade out towards this distance
#define MIN_VOLUME 16              // ...but stay faintly audible inside it

typedef struct {
    const s8 *data;
    u32 bytes;
    u16 rate;
} Sound;

static Sound sounds[SFX_COUNT];
static int count;

static u16 rd16(const u8 *p) { return p[0] | p[1] << 8; }
static u32 rd32(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }

int sfx_init(const void *bank, size_t size)
{
    const u8 *p = bank, *end = p + size;
    if (size < 8 || memcmp(p, "RSN1", 4) != 0)
        return -1;
    count = rd16(p + 4);
    if (count > SFX_COUNT)
        count = SFX_COUNT;
    p += 8;
    for (int i = 0; i < count; i++)
    {
        if (p + 8 > end)
            return -2;
        sounds[i].rate = rd16(p);
        sounds[i].bytes = rd32(p + 4);
        sounds[i].data = (const s8 *)(p + 8);
        p += 8 + sounds[i].bytes;
        if (p > end)
            return -2;
    }
    soundEnable();
    return 0;
}

static int level = 100;   // master volume, percent (the pause menu's Sound setting)

void sfx_level(int percent)
{
    level = percent;
}

int sfx_play(int id, int volume, int pan)
{
    volume = volume * level / 100;
    if (id < 0 || id >= count || volume <= 0)
        return -1;
    const Sound *s = &sounds[id];
    // The samples are part of the program image in main RAM, which the ARM7 reads directly.
    return soundPlaySample(s->data, SoundFormat_8Bit, s->bytes, s->rate, volume > 127 ? 127 : volume,
                           pan < 0 ? 0 : pan > 127 ? 127 : pan, false, 0);
}

int sfx_play_at(int id, int volume, const int pos[3], const int ear[3], int yaw)
{
    int dx = pos[0] - ear[0], dz = pos[2] - ear[2];
    int dist = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
    if (dist >= HEAR_RANGE)
        return -1;
    int v = volume * (HEAR_RANGE - dist) / HEAR_RANGE;
    if (v < MIN_VOLUME)
        v = MIN_VOLUME;
    // The listener's right is (-cos yaw, sin yaw) for facing (sin yaw, cos yaw).
    int pan = 64;
    if (dist > 0)
    {
        s64 side = ((s64)-cosLerp(yaw) * dx + (s64)sinLerp(yaw) * dz) >> 12;   // f32
        pan = 64 + (int)(side * 56 / dist);
    }
    return sfx_play(id, v, pan);
}

void sfx_stop(int channel)
{
    if (channel >= 0)
        soundKill(channel);
}
