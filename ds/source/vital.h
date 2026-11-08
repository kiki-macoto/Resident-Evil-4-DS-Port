// Bottom screen: the item select / vital screen, always shown in the game and worked by touch.
// Drawn into a 16-bit bitmap background (VRAM C) from the original's case panels (2d_bks_01 /
// 2d_bks_00), item icons (2d_itemic_00) and the game font.
#ifndef VITAL_H__
#define VITAL_H__

#include <nds.h>

#define VITAL_SLOTS 3   // the gun in hand, its rounds (multi ammo), green herbs

typedef struct {
    int hp, hp_max;
    int ammo, mag;      // rounds loaded / magazine size
    int spare;          // rounds carried
    int herbs, heal;    // herbs carried, health one gives back
    int fps;
    const char *gun_name;   // the gun in hand (NULL: the handgun's)
    int gun_icon[2], ammo_icon[2];   // their icons in the icon sheet (2d_itemic_00, laid out as the items sheet)
} Vital;

enum { VITAL_NONE, VITAL_SELECT, VITAL_USE };

// Sets up the sub screen (16-bit bitmap on BG3, VRAM C). Returns 0 on success.
int vital_init(const void *panels, size_t panels_size, const void *icons, size_t icons_size);

// Menus: just the backdrop, a caption in the header bar and an (ASCII) subtitle under it.
void vital_idle(const char *caption, const char *subtitle);

// In the game: redraws what changed (the heartbeat every frame) and handles touches (when
// `touch` is set): tapping a slot selects it (VITAL_SELECT); a double tap on the herb slot
// returns VITAL_USE.
int vital_update(const Vital *v, u32 down, int tick, int touch);

// A story scene's page on the sub screen's bitmap (shown on top: the caller swaps the screens,
// so it needs no texture memory): the picture (an RD2D sheet, 256 x 128, 8-bit paletted; NULL
// for none) at the top over black, who speaks (NULL: no one) and up to 3 lines ('\n') below.
// The next vital_update() / vital_idle() lays its own screen out again.
void vital_cutin(const void *picture, size_t size, const char *who, const char *text);

// A note in the description panel for `frames` frames (e.g. "health is full").
void vital_message(const char *s, int frames);

// Test builds: the next vital_update() sees a tap at (x, y) (an emulator can't be touched
// from a script).
void vital_fake_touch(int x, int y);

#endif
