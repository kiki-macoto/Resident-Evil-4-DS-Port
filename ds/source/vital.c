#include <stdio.h>
#include <string.h>

#include "text.h"
#include "vital.h"

#define W 256
#define H 192

typedef struct {
    int w, h;
    const u16 *px;   // RGB15, bit 15 = opaque
} Bitmap;

static Bitmap panels, icons;
static u16 bg[W * H];    // the static layout
static u16 fb[W * H];    // bg + what changes, copied to VRAM by regions
static u16 *vram;

// Layout. Panel bitmap (vitalbmp.bin): 2d_bks_01's frames at y 0 (header bar 240x30 at 0,0;
// spring panel 240x34 at 0,30; small frame 32x16 at 0,80), 2d_bks_00's slot frame (50x30 at
// 8,96), a wood strip (240x32 at 0,180).
#define SLOT_Y 98
static int slot_x(int i) { return 24 + i * 76; }
#define DOUBLE_TAP 24   // frames: two taps on a slot this close together use the item
static int tap_slot = -1, tap_tick;

static int mode = -1;   // 0 idle, 1 game
static int sel, msg_frames;
static char msg[48];
static Vital last;
static int last_sel = -1, last_msg = -1, last_fps = -1, drawn_hp = -1;

static int load_bitmap(Bitmap *b, const void *data, size_t size)
{
    const u8 *p = data;
    if (size < 8 || memcmp(p, "RD2B", 4) != 0)
        return -1;
    b->w = p[4] | p[5] << 8;
    b->h = p[6] | p[7] << 8;
    if (8 + (size_t)b->w * b->h * 2 > size)
        return -2;
    b->px = (const u16 *)(p + 8);
    return 0;
}

int vital_init(const void *panel_data, size_t panels_size, const void *icon_data, size_t icons_size)
{
    if (load_bitmap(&panels, panel_data, panels_size) || load_bitmap(&icons, icon_data, icons_size))
        return -1;
    videoSetModeSub(MODE_5_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    int id = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    vram = bgGetGfxPtr(id);
    return 0;
}

// Copies part of a bitmap scaled (nearest) to the buffer, skipping transparent pixels;
// shade 31 = as is, lower = darker.
static void blit(u16 *dst, const Bitmap *b, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, int shade)
{
    for (int y = 0; y < dh; y++)
    {
        int py = dy + y;
        if (py < 0 || py >= H)
            continue;
        const u16 *row = b->px + (sy + y * sh / dh) * b->w + sx;
        for (int x = 0; x < dw; x++)
        {
            int px = dx + x;
            if (px < 0 || px >= W)
                continue;
            u16 c = row[x * sw / dw];
            if (!(c & 0x8000))
                continue;
            if (shade < 31)
                c = 0x8000 | RGB15((c & 31) * shade / 31, ((c >> 5) & 31) * shade / 31, ((c >> 10) & 31) * shade / 31);
            dst[py * W + px] = c;
        }
    }
}

static void fill(u16 *dst, int x0, int y0, int x1, int y1, u16 c)
{
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < H; y++)
        for (int x = x0 < 0 ? 0 : x0; x < x1 && x < W; x++)
            dst[y * W + x] = c;
}

static void restore(int x0, int y0, int x1, int y1)
{
    for (int y = y0; y < y1; y++)
        memcpy(&fb[y * W + x0], &bg[y * W + x0], (x1 - x0) * 2);
}

static void flush(int x0, int y0, int x1, int y1)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            vram[y * 256 + x] = fb[y * W + x];
}

static int text(u16 *dst, int x, int y, u16 color, int scale, const char *s)
{
    return text_draw16(dst, W, W, H, x, y, color, scale, s);
}

static void backdrop(u16 *dst)
{
    blit(dst, &panels, 0, 180, 240, 32, 0, 0, W, H, 12);   // the wood strip, darkened, as the backdrop
    blit(dst, &panels, 0, 0, 240, 30, 8, 2, 240, 30, 31);   // header bar
}

void vital_idle(const char *caption, const char *subtitle)
{
    if (mode == 0)
        return;
    mode = 0;
    backdrop(bg);
    // Caption centred in the header bar, subtitle centred under it (7-pixel ASCII cells).
    text(bg, (W - 7 * (int)strlen(caption)) / 2, 9, RGB15(31, 26, 10), 1, caption);
    text(bg, (W - 7 * (int)strlen(subtitle)) / 2, 40, RGB15(25, 25, 25), 1, subtitle);
    memcpy(fb, bg, sizeof(fb));
    flush(0, 0, W, H);
}

void vital_cutin(const void *picture, size_t size, const char *who, const char *lines)
{
    mode = 2;
    for (int i = 0; i < W * H; i++)
        fb[i] = 0x8000;
    const u8 *p = picture;
    // RD2D: magic, w, h; then the texture record: w, h, type, flags, palette colours, bytes, texels, palette.
    if (p && size >= 20 && memcmp(p, "RD2D", 4) == 0)
    {
        int tw = p[8] | p[9] << 8, th = p[10] | p[11] << 8, type = p[12], colors = p[14] | p[15] << 8;
        u32 bytes = p[16] | p[17] << 8 | p[18] << 16 | (u32)p[19] << 24;
        const u8 *tex = p + 20;
        const u8 *pal = tex + bytes;
        if (type == GL_RGB256 && 20 + bytes + colors * 2 <= size && tw == 256)
            for (int y = 0; y < th && y < 128; y++)
                for (int x = 0; x < 256; x++)
                {
                    int k = tex[y * tw + x];
                    u16 c = k < colors ? (pal[k * 2] | pal[k * 2 + 1] << 8) : 0;
                    fb[(4 + y) * W + x] = 0x8000 | c;
                }
    }
    int y = 141;
    if (who)
    {
        text(fb, 16, 134, RGB15(31, 26, 10), 1, who);
        y = 149;
    }
    for (int i = 0; i < 3 && lines && *lines; i++)
    {
        char line[48];
        int n = 0;
        while (*lines && *lines != '\n' && n < (int)sizeof(line) - 1)
            line[n++] = *lines++;
        line[n] = 0;
        if (*lines == '\n')
            lines++;
        text(fb, 16, y + i * 14, RGB15(31, 31, 31), 1, line);
    }
    flush(0, 0, W, H);
}

static void layout_game(void)
{
    backdrop(bg);
    text(bg, 18, 9, RGB15(31, 26, 10), 1, T_ITEMS);
    blit(bg, &panels, 0, 30, 240, 34, 8, 36, 240, 54, 31);      // vital panel
    for (int i = 0; i < VITAL_SLOTS; i++)
        blit(bg, &panels, 8, 96, 50, 30, slot_x(i), SLOT_Y, 64, 40, 31);
    blit(bg, &panels, 0, 180, 240, 32, 8, 141, 240, 32, 31);    // description panel
    text(bg, 10, 177, RGB15(17, 17, 17), 1, T_TOUCH_HINT);
}

// Heartbeat trace height at phase 0..63 (pixels up from the baseline): P wave, QRS, T wave.
static int ecg(int t)
{
    if (t >= 6 && t < 12) return t < 9 ? t - 5 : 12 - t;
    if (t == 16) return -2;
    if (t >= 17 && t < 21) return t < 19 ? (t - 16) * 6 : (21 - t) * 6;
    if (t == 21) return -4;
    if (t >= 28 && t < 38) return t < 33 ? (t - 27) / 2 + 1 : (38 - t) / 2 + 1;
    return 0;
}

static void line(int x0, int y0, int x1, int y1, u16 c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx - dy;
    while (1)
    {
        fill(fb, x0, y0, x0 + 1, y0 + 2, c);   // 2 pixels tall: easier to see
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

static int condition(const Vital *v) { return v->hp <= v->hp_max / 3 ? 2 : v->hp <= v->hp_max * 2 / 3 ? 1 : 0; }
static const u16 cond_color[3] = { 0x8000 | RGB15(10, 31, 12), 0x8000 | RGB15(31, 26, 6), 0x8000 | RGB15(31, 8, 6) };

static int fake_x = -1, fake_y;

void vital_fake_touch(int x, int y)
{
    fake_x = x;
    fake_y = y;
}

void vital_message(const char *s, int frames)
{
    snprintf(msg, sizeof(msg), "%s", s);
    msg_frames = frames;
    last_sel = -1;   // redraw the description even if one was already showing
}

int vital_update(const Vital *v, u32 down, int tick, int touch)
{
    int result = VITAL_NONE;
    if (mode != 1)
    {
        mode = 1;
        layout_game();
        memcpy(fb, bg, sizeof(fb));
        flush(0, 0, W, H);
        last_sel = last_msg = last_fps = drawn_hp = -1;
    }

    // Touch: the slots.
    if (touch && ((down & KEY_TOUCH) || fake_x >= 0))
    {
        touchPosition t;
        if (fake_x >= 0)
        {
            t.px = fake_x;
            t.py = fake_y;
            fake_x = -1;
        }
        else
            touchRead(&t);
        for (int i = 0; i < VITAL_SLOTS; i++)
            if (t.px >= slot_x(i) - 4 && t.px < slot_x(i) + 68 && t.py >= SLOT_Y - 4 && t.py < SLOT_Y + 44)
            {
                // A second tap on the same slot soon after the first is a double tap: use
                // the item (only the herbs can be used).
                if (i == tap_slot && tick - tap_tick <= DOUBLE_TAP)
                {
                    if (i == 2)
                        result = VITAL_USE;
                    tap_slot = -1;   // a third tap starts over
                }
                else
                {
                    if (i != sel)
                        result = VITAL_SELECT;
                    sel = i;
                    tap_slot = i;
                    tap_tick = tick;
                }
            }
    }
    if (msg_frames > 0)
        msg_frames--;

    // Condition and health.
    int cond = condition(v);
    if (v->hp != drawn_hp)
    {
        static const char *const words[3] = { "FINE", "CAUTION", "DANGER" };
        char buf[32];
        restore(12, 40, 112, 88);
        text(fb, 20, 42, cond_color[cond], 2, words[cond]);
        snprintf(buf, sizeof(buf), T_HP, v->hp, v->hp_max);
        text(fb, 22, 70, RGB15(31, 31, 31), 1, buf);
        flush(12, 40, 112, 88);
        drawn_hp = v->hp;
    }

    // Heartbeat: faster and redder as the condition worsens.
    restore(112, 42, 240, 86);
    int speed = cond + 1, base = 72, prev = base - ecg((112 + tick * speed) & 63);
    for (int x = 114; x <= 236; x += 2)
    {
        int y = base - ecg((x + tick * speed) & 63);
        line(x - 2, prev, x, y, cond_color[cond]);
        prev = y;
    }
    flush(112, 42, 240, 86);

    // Slots and description when something they show changed.
    int counts[VITAL_SLOTS] = { v->ammo, v->spare, v->herbs };
    int changed = sel != last_sel || (msg_frames > 0) != last_msg || v->ammo != last.ammo || v->spare != last.spare || v->herbs != last.herbs
                  || v->gun_icon[0] != last.gun_icon[0] || v->gun_icon[1] != last.gun_icon[1] || v->gun_name != last.gun_name || v->mag != last.mag;
    if (changed)
    {
        char buf[48];
        restore(14, SLOT_Y - 6, 248, SLOT_Y + 46);
        for (int i = 0; i < VITAL_SLOTS; i++)
        {
            int x = slot_x(i);
            // the gun in hand and its rounds (as the game's item icons), the herb
            const int icon_xy[VITAL_SLOTS][2] = { { v->gun_icon[0], v->gun_icon[1] }, { v->ammo_icon[0], v->ammo_icon[1] }, { 64, 48 } };
            int empty = i == 2 && v->herbs == 0;
            blit(fb, &icons, icon_xy[i][0], icon_xy[i][1], 32, 24, x + 11, SLOT_Y + 5, 42, 30, empty ? 9 : 31);
            snprintf(buf, sizeof(buf), "%d", counts[i]);
            text(fb, x + 60 - 7 * (int)strlen(buf), SLOT_Y + 25, empty ? RGB15(17, 17, 17) : RGB15(31, 31, 31), 1, buf);
        }
        // Red frame around the chosen slot (the colour of the original's selection box).
        int x = slot_x(sel);
        u16 red = 0x8000 | RGB15(31, 6, 4);
        fill(fb, x - 4, SLOT_Y - 4, x + 68, SLOT_Y - 2, red);
        fill(fb, x - 4, SLOT_Y + 42, x + 68, SLOT_Y + 44, red);
        fill(fb, x - 4, SLOT_Y - 2, x - 2, SLOT_Y + 42, red);
        fill(fb, x + 66, SLOT_Y - 2, x + 68, SLOT_Y + 42, red);
        flush(14, SLOT_Y - 6, 248, SLOT_Y + 46);

        const char *const names[VITAL_SLOTS] = { v->gun_name ? v->gun_name : T_ITEM_GUN, T_ITEM_AMMO, T_ITEM_HERB };
        restore(8, 141, 248, 173);
        text(fb, 16, 143, RGB15(31, 26, 10), 1, names[sel]);
        if (msg_frames > 0)
            snprintf(buf, sizeof(buf), "%s", msg);
        else if (sel == 0)
            snprintf(buf, sizeof(buf), T_DESC_GUN, v->ammo, v->mag);
        else if (sel == 1)
            snprintf(buf, sizeof(buf), T_DESC_AMMO, v->spare);
        else
            snprintf(buf, sizeof(buf), T_DESC_HERB, v->heal, v->herbs);
        text(fb, 16, 157, msg_frames > 0 ? RGB15(31, 26, 10) : RGB15(31, 31, 31), 1, buf);
        flush(8, 141, 248, 173);


        last = *v;
        last_sel = sel;
        last_msg = msg_frames > 0;
    }

    if (v->fps != last_fps)
    {
        char buf[16];
        restore(196, 6, 246, 28);
        snprintf(buf, sizeof(buf), "%dfps", v->fps);
        text(fb, 240 - 7 * (int)strlen(buf), 9, RGB15(17, 17, 17), 1, buf);
        flush(196, 6, 246, 28);
        last_fps = v->fps;
    }
    return result;
}
