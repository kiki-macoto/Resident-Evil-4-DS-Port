#ifndef VITAL_H__
#define VITAL_H__

#include <nds.h>

#define VITAL_SLOTS 3

typedef struct {
    int hp, hp_max;
    int ammo, mag;
    int spare;
    int herbs, heal;
    int fps;
    const char *gun_name;
    int gun_icon[2], ammo_icon[2];
    int stage;
    int leon_x, leon_z, leon_yaw;
    int ash_on, ash_x, ash_z;
    int items_view;
} Vital;

enum { VITAL_NONE, VITAL_SELECT, VITAL_USE };

int vital_init(const void *panels, size_t panels_size, const void *icons, size_t icons_size);

void vital_idle(const char *caption, const char *subtitle);

int vital_update(const Vital *v, u32 down, int tick, int touch);
void vital_commit(void);

void vital_cutin(const void *picture, size_t size, const char *who, const char *text);

void vital_message(const char *s, int frames);

void vital_fake_touch(int x, int y);

#endif
