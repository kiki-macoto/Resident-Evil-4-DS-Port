#include <stdio.h>
#include <string.h>

#include "stage_maps.h"
#include "text.h"
#include "vital.h"

#define W 256
#define H 192

typedef struct {
    int w, h;
    const u16 *px;
} Bitmap;

static Bitmap panels, icons;
static u16 bg[W * H];
static u16 fb[W * H];
static u16 *vram;

#define SLOT_Y 98
#define DOUBLE_TAP 24
static int tap_slot = -1, tap_tick;

static int mode = -1;
static int sprites_ready_g, map_redraw_g;
static void bg_plain(void)
{
    REG_BG3PA_SUB = 256;
    REG_BG3PB_SUB = 0;
    REG_BG3PC_SUB = 0;
    REG_BG3PD_SUB = 256;
    REG_BG3X_SUB = 0;
    REG_BG3Y_SUB = 0;
    if (sprites_ready_g)
    {
        oamClear(&oamSub, 0, 0);
        oamUpdate(&oamSub);
    }
    map_redraw_g = 1;
}
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
    blit(dst, &panels, 0, 180, 240, 32, 0, 0, W, H, 12);
    blit(dst, &panels, 0, 0, 240, 30, 8, 2, 240, 30, 31);
}

void vital_idle(const char *caption, const char *subtitle)
{
    if (mode == 0)
        return;
    mode = 0;
    bg_plain();
    backdrop(bg);
    text(bg, (W - 7 * (int)strlen(caption)) / 2, 9, RGB15(31, 26, 10), 1, caption);
    text(bg, (W - 7 * (int)strlen(subtitle)) / 2, 40, RGB15(25, 25, 25), 1, subtitle);
    memcpy(fb, bg, sizeof(fb));
    flush(0, 0, W, H);
}

void vital_cutin(const void *picture, size_t size, const char *who, const char *lines)
{
    if (mode != 2)
        bg_plain();
    mode = 2;
    for (int i = 0; i < W * H; i++)
        fb[i] = 0x8000;
    const u8 *p = picture;
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

#define MAP_CX     150
#define MAP_CY     118
#define MAP_ZOOM   inttof32(1)
#define COL_X      4
#define COL_Y      6
#define SLOT_H     42
static int map_stage = -1;
static int map_k;
static int map_x0, map_z0;
static u16 col_px[32 * 128];
static u16 note_px[192 * 16];
static u16 *col_gfx[2], *note_gfx[6], *leon_gfx, *ash_gfx;
static int sprites_ready;
static int aff[6];
static int ash_sx, ash_sy, ash_show;

static void tri_fill256(u16 *dst, int x0, int y0, int x1, int y1, int x2, int y2, u16 c)
{
    int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2), maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2), maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx > 255) maxx = 255;
    if (maxy > 255) maxy = 255;
    int area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (area == 0)
        return;
    for (int y = miny; y <= maxy; y++)
        for (int x = minx; x <= maxx; x++)
        {
            int w0 = (x1 - x) * (y2 - y) - (x2 - x) * (y1 - y);
            int w1 = (x2 - x) * (y0 - y) - (x0 - x) * (y2 - y);
            int w2 = (x0 - x) * (y1 - y) - (x1 - x) * (y0 - y);
            if ((area > 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) || (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0))
                dst[y * 256 + x] = c;
        }
}

static void map_draw(int stage)
{
    map_stage = stage;
    dmaFillHalfWords(0, vram, 256 * 256 * 2);
    if (stage < 0 || stage >= 12 || !stage_maps[stage].count)
        return;
    const StageMap *m = &stage_maps[stage];
    int x0 = 32767, z0 = 32767, x1 = -32768, z1 = -32768;
    for (int i = 0; i < m->count; i++)
        for (int k = 0; k < 6; k += 2)
        {
            int x = m->tri[i].p[k], z = m->tri[i].p[k + 1];
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (z < z0) z0 = z;
            if (z > z1) z1 = z;
        }
    int span = (x1 - x0 > z1 - z0 ? x1 - x0 : z1 - z0) + 1;
    map_k = 240 * 4096 / span;
    map_x0 = x0 - 8 * 4096 / map_k;
    map_z0 = z0 - 8 * 4096 / map_k;
    static const u16 shade[4] = { 0x8000 | RGB15(13, 14, 15), 0x8000 | RGB15(20, 18, 12), 0x8000 | RGB15(7, 8, 9), 0x8000 | RGB15(16, 11, 6) };
    for (int i = 0; i < m->count; i++)
    {
        const MapTri *t = &m->tri[i];
        int px[3], py[3];
        for (int k = 0; k < 3; k++)
        {
            px[k] = (t->p[k * 2] - map_x0) * map_k / 4096;
            py[k] = (t->p[k * 2 + 1] - map_z0) * map_k / 4096;
        }
        tri_fill256(vram, px[0], py[0], px[1], py[1], px[2], py[2], shade[t->group & 3]);
    }
}

static void sprites_init(void)
{
    if (sprites_ready)
        return;
    vramSetBankI(VRAM_I_SUB_SPRITE);
    oamInit(&oamSub, SpriteMapping_Bmp_1D_128, false);
    for (int i = 0; i < 2; i++)
        col_gfx[i] = oamAllocateGfx(&oamSub, SpriteSize_32x64, SpriteColorFormat_Bmp);
    for (int i = 0; i < 6; i++)
        note_gfx[i] = oamAllocateGfx(&oamSub, SpriteSize_32x16, SpriteColorFormat_Bmp);
    leon_gfx = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_Bmp);
    ash_gfx = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_Bmp);
    static u16 arrow[16 * 16], dot[8 * 8];
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
        {
            int half = y / 2;
            int in = y >= 1 && y <= 14 && x >= 8 - half && x <= 7 + half;
            int edge = in && (y == 14 || x == 8 - half || x == 7 + half);
            arrow[y * 16 + x] = !in ? 0 : edge ? 0x8000 | RGB15(6, 4, 0) : 0x8000 | RGB15(31, 28, 4);
        }
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
        {
            int dx = 2 * x - 7, dy = 2 * y - 7;
            dot[y * 8 + x] = dx * dx + dy * dy <= 40 ? 0x8000 | RGB15(31, 12, 20) : 0;
        }
    DC_FlushRange(arrow, sizeof(arrow));
    DC_FlushRange(dot, sizeof(dot));
    dmaCopy(arrow, leon_gfx, sizeof(arrow));
    dmaCopy(dot, ash_gfx, sizeof(dot));
    sprites_ready = 1;
    sprites_ready_g = 1;
}

static void column_draw(const Vital *v)
{
    for (int i = 0; i < 32 * 128; i++)
        col_px[i] = 0;
    const int icon_xy[VITAL_SLOTS][2] = { { v->gun_icon[0], v->gun_icon[1] }, { v->ammo_icon[0], v->ammo_icon[1] }, { 64, 48 } };
    const int counts[VITAL_SLOTS] = { v->ammo, v->spare, v->herbs };
    for (int i = 0; i < VITAL_SLOTS; i++)
    {
        int y0 = i * SLOT_H;
        for (int y = y0; y < y0 + SLOT_H - 2 && y < 128; y++)
            for (int x = 0; x < 32; x++)
                col_px[y * 32 + x] = (y == y0 || y == y0 + SLOT_H - 3 || x == 0 || x == 31) ? 0x8000 | RGB15(16, 13, 8) : 0x8000 | RGB15(2, 2, 3);
        int empty = i == 2 && v->herbs == 0;
        for (int y = 0; y < 24; y++)
            for (int x = 0; x < 30; x++)
            {
                u16 c = icons.px[(icon_xy[i][1] + y) * icons.w + icon_xy[i][0] + 1 + x];
                if (!(c & 0x8000))
                    continue;
                if (empty)
                    c = 0x8000 | RGB15((c & 31) / 3, ((c >> 5) & 31) / 3, ((c >> 10) & 31) / 3);
                col_px[(y0 + 2 + y) * 32 + 1 + x] = c;
            }
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", counts[i]);
        text_draw16(col_px, 32, 32, 128, 30 - 7 * (int)strlen(buf), y0 + 25, empty ? RGB15(17, 17, 17) : RGB15(31, 31, 31), 1, buf);
    }
    DC_FlushRange(col_px, sizeof(col_px));
    for (int i = 0; i < 2; i++)
        dmaCopy(col_px + i * 32 * 64, col_gfx[i], 32 * 64 * 2);
}

static void note_draw(const char *s)
{
    for (int i = 0; i < 192 * 16; i++)
        note_px[i] = s ? 0x8000 | RGB15(2, 2, 3) : 0;
    if (s)
        text_draw16(note_px, 192, 192, 16, 4, 2, RGB15(31, 26, 10), 1, s);
    static u16 part[32 * 16];
    for (int k = 0; k < 6; k++)
    {
        for (int y = 0; y < 16; y++)
            memcpy(&part[y * 32], &note_px[y * 192 + k * 32], 32 * 2);
        DC_FlushRange(part, sizeof(part));
        dmaCopy(part, note_gfx[k], sizeof(part));
    }
}

static Vital col_last;
static int col_drawn, note_shown = -1;

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
    note_shown = -1;
    last_sel = -1;
}

static int map_update(const Vital *v, u32 down, int tick, int touch)
{
    int result = VITAL_NONE;
    sprites_init();
    if (mode != 3 || map_stage != v->stage || map_redraw_g)
    {
        mode = 3;
        map_redraw_g = 0;
        BG_PALETTE_SUB[0] = RGB15(1, 1, 2);
        map_draw(v->stage);
        col_drawn = 0;
        note_shown = -1;
    }

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
#ifdef TEST_SHOW_TAPS
        {
            char b[24];
            snprintf(b, sizeof(b), "tap %d %d t%d", t.px, t.py, tick);
            vital_message(b, 60);
        }
#endif
        for (int i = 0; i < VITAL_SLOTS; i++)
            if (t.px >= COL_X - 4 && t.px < COL_X + 36 && t.py >= COL_Y + i * SLOT_H - 2 && t.py < COL_Y + (i + 1) * SLOT_H)
            {
                if (i == tap_slot && tick - tap_tick <= DOUBLE_TAP)
                {
                    if (i == 2)
                        result = VITAL_USE;
                    tap_slot = -1;
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
    fake_x = -1;
    if (msg_frames > 0)
        msg_frames--;

    int s = sinLerp(v->leon_yaw), c = cosLerp(v->leon_yaw);
    int k = (int)(((s64)map_k << 12) / MAP_ZOOM);
    int pa = -mulf32(c, k), pb = -mulf32(s, k), pc = mulf32(s, k), pd = -mulf32(c, k);
    int lx = (int)(((s64)(v->leon_x - inttof32(map_x0)) * map_k) >> 12);
    int lz = (int)(((s64)(v->leon_z - inttof32(map_z0)) * map_k) >> 12);
    aff[0] = pa >> 4;
    aff[1] = pb >> 4;
    aff[2] = pc >> 4;
    aff[3] = pd >> 4;
    aff[4] = (lx - pa * MAP_CX - pb * MAP_CY) >> 4;
    aff[5] = (lz - pc * MAP_CX - pd * MAP_CY) >> 4;
    ash_show = 0;
    if (v->ash_on)
    {
        int dx = (int)(((s64)(v->ash_x - v->leon_x) * MAP_ZOOM) >> 12), dz = (int)(((s64)(v->ash_z - v->leon_z) * MAP_ZOOM) >> 12);
        ash_sx = MAP_CX + f32toint(mulf32(-c, dx) + mulf32(s, dz));
        ash_sy = MAP_CY + f32toint(mulf32(-s, dx) - mulf32(c, dz));
        ash_show = ash_sx > 40 && ash_sx < 256 && ash_sy >= 0 && ash_sy < 192;
    }

    if (!col_drawn || v->ammo != col_last.ammo || v->spare != col_last.spare || v->herbs != col_last.herbs
        || v->gun_icon[0] != col_last.gun_icon[0] || v->gun_icon[1] != col_last.gun_icon[1] || v->ammo_icon[0] != col_last.ammo_icon[0])
    {
        column_draw(v);
        col_last = *v;
        col_drawn = 1;
    }
    if ((msg_frames > 0) != note_shown)
    {
        note_shown = msg_frames > 0;
        note_draw(note_shown ? msg : NULL);
    }
    (void)tick;
    return result;
}

static int slot_x(int i) { return 24 + i * 76; }

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

static void layout_game(void)
{
    backdrop(bg);
    text(bg, 18, 9, RGB15(31, 26, 10), 1, T_ITEMS);
    blit(bg, &panels, 0, 30, 240, 34, 8, 36, 240, 54, 31);
    for (int i = 0; i < VITAL_SLOTS; i++)
        blit(bg, &panels, 8, 96, 50, 30, slot_x(i), SLOT_Y, 64, 40, 31);
    blit(bg, &panels, 0, 180, 240, 32, 8, 141, 240, 32, 31);
    text(bg, 10, 177, RGB15(17, 17, 17), 1, T_TOUCH_HINT);
}

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
        fill(fb, x0, y0, x0 + 1, y0 + 2, c);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

static int condition(const Vital *v) { return v->hp <= v->hp_max / 3 ? 2 : v->hp <= v->hp_max * 2 / 3 ? 1 : 0; }
static const u16 cond_color[3] = { 0x8000 | RGB15(10, 31, 12), 0x8000 | RGB15(31, 26, 6), 0x8000 | RGB15(31, 8, 6) };

static int items_update(const Vital *v, u32 down, int tick, int touch)
{
    int result = VITAL_NONE;
    if (mode != 1)
    {
        if (mode == 3)
            bg_plain();
        mode = 1;
        layout_game();
        memcpy(fb, bg, sizeof(fb));
        flush(0, 0, W, H);
        last_sel = last_msg = last_fps = drawn_hp = -1;
    }

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
                if (i == tap_slot && tick - tap_tick <= DOUBLE_TAP)
                {
                    if (i == 2)
                        result = VITAL_USE;
                    tap_slot = -1;
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
    fake_x = -1;
    if (msg_frames > 0)
        msg_frames--;

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

    restore(112, 42, 240, 86);
    int speed = cond + 1, base = 72, prev = base - ecg((112 + tick * speed) & 63);
    for (int x = 114; x <= 236; x += 2)
    {
        int y = base - ecg((x + tick * speed) & 63);
        line(x - 2, prev, x, y, cond_color[cond]);
        prev = y;
    }
    flush(112, 42, 240, 86);

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
            const int icon_xy[VITAL_SLOTS][2] = { { v->gun_icon[0], v->gun_icon[1] }, { v->ammo_icon[0], v->ammo_icon[1] }, { 64, 48 } };
            int empty = i == 2 && v->herbs == 0;
            blit(fb, &icons, icon_xy[i][0], icon_xy[i][1], 32, 24, x + 11, SLOT_Y + 5, 42, 30, empty ? 9 : 31);
            snprintf(buf, sizeof(buf), "%d", counts[i]);
            text(fb, x + 60 - 7 * (int)strlen(buf), SLOT_Y + 25, empty ? RGB15(17, 17, 17) : RGB15(31, 31, 31), 1, buf);
        }
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

int vital_update(const Vital *v, u32 down, int tick, int touch)
{
    return v->items_view ? items_update(v, down, tick, touch) : map_update(v, down, tick, touch);
}

void vital_commit(void)
{
    if (mode != 3 || !sprites_ready)
        return;
    REG_BG3PA_SUB = aff[0];
    REG_BG3PB_SUB = aff[1];
    REG_BG3PC_SUB = aff[2];
    REG_BG3PD_SUB = aff[3];
    REG_BG3X_SUB = aff[4];
    REG_BG3Y_SUB = aff[5];
    int n = 0;
    for (int i = 0; i < 2; i++)
        oamSet(&oamSub, n++, COL_X, COL_Y + i * 64, 0, 15, SpriteSize_32x64, SpriteColorFormat_Bmp, col_gfx[i], -1, false, false, false, false, false);
    oamSet(&oamSub, n++, MAP_CX - 8, MAP_CY - 8, 0, 15, SpriteSize_16x16, SpriteColorFormat_Bmp, leon_gfx, -1, false, false, false, false, false);
    oamSet(&oamSub, n++, ash_sx - 4, ash_sy - 4, 0, 15, SpriteSize_8x8, SpriteColorFormat_Bmp, ash_gfx, -1, false, !ash_show, false, false, false);
    for (int i = 0; i < 6; i++)
        oamSet(&oamSub, n++, 56 + i * 32, 172, 0, 15, SpriteSize_32x16, SpriteColorFormat_Bmp, note_gfx[i], -1, false, !note_shown, false, false, false);
    oamUpdate(&oamSub);
}
