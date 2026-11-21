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
static int bg2_id = -1;
static void layers_16(void)
{
    REG_BG3CNT_SUB = BG_BMP16_256x256 | BG_BMP_BASE(0) | BG_PRIORITY(1);
    if (bg2_id >= 0)
        bgHide(bg2_id);
}
static void layers_8(void)
{
    REG_BG3CNT_SUB = BG_BMP8_256x256 | BG_BMP_BASE(0) | BG_PRIORITY(1);
    REG_BG2CNT_SUB = BG_BMP8_256x256 | BG_BMP_BASE(4) | BG_PRIORITY(0);
    REG_BG2PA_SUB = 256;
    REG_BG2PB_SUB = 0;
    REG_BG2PC_SUB = 0;
    REG_BG2PD_SUB = 256;
    REG_BG2X_SUB = 0;
    REG_BG2Y_SUB = 0;
    if (bg2_id >= 0)
        bgShow(bg2_id);
}
static void bg_plain(void)
{
    layers_16();
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
    bg2_id = bgInitSub(2, BgType_Bmp8, BgSize_B8_256x256, 4, 0);
    bgHide(bg2_id);
    layers_16();
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

#define MAP_X0   10
#define MAP_Y0   69
#define MAP_X1   246
#define MAP_Y1   161
#define MAP_CX   ((MAP_X0 + MAP_X1) / 2)
#define MAP_CY   ((MAP_Y0 + MAP_Y1) / 2)
#define HDR_Y1   32
#define PANEL_Y1 63
#define FOOT_Y0  169
#define FOOT_Y1  191
#define ECG_X1   204
enum { BTN_PLUS, BTN_MINUS, BTN_DIR, BTN_CASE, BTN_MAP, BTN_COUNT };
static const int btn_x[BTN_COUNT][2] = { { 6, 44 }, { 48, 86 }, { 90, 128 }, { 134, 190 }, { 194, 250 } };
static const int zoom_px[3] = { 2048, 4096, 8192 };
static const char *const zoom_name[3] = { "x1", "x2", "x4" };
static int map_zoom = 1, map_north, map_whole;
static int map_stage = -1;
static int map_k, map_x0, map_z0, map_tw, map_th;
static u8 *const mapb = (u8 *)bg;
static u16 *const comp = fb;
static u16 *const comp_static = bg;
static u8 ovb[W * H];
static u16 bin_idx[4096];
static int pal_n;
static u16 *note_gfx[6], *leon_gfx, *ash_gfx;
static u16 note_px[192 * 16];
static int sprites_ready;
static int aff[6];
static int leon_sx, leon_sy, leon_rot, ash_sx, ash_sy, ash_show;
static int hdr_drawn, hdr_ash, hdr_ammo, hdr_spare, hdr_ptas, hdr_fps, hdr_medals = -1, hdr_hit = -1;
static int note_shown = -1;

enum { PI_WHITE = 5, PI_GRAY, PI_GOLD, PI_PINK, PI_GREEN, PI_YELLOW, PI_RED, PI_BLACK };
static const u16 fixed_pal[8] = { RGB15(31, 31, 31), RGB15(18, 17, 15), RGB15(31, 26, 10), RGB15(31, 18, 24),
                                  RGB15(10, 31, 12), RGB15(31, 26, 6), RGB15(31, 8, 6), RGB15(0, 0, 0) };

static int bin12(u16 c) { return ((c >> 1) & 15) | (((c >> 6) & 15) << 4) | (((c >> 11) & 15) << 8); }

static void palette_build(void)
{
    static u16 hist[4096];
    memset(hist, 0, sizeof(hist));
    for (int i = 0; i < W * H; i++)
        if (comp[i] & 0x8000)
        {
            int b = bin12(comp[i]);
            if (hist[b] < 65535)
                hist[b]++;
        }
    for (int i = 0; i < 4096; i++)
        bin_idx[i] = 0xFFFF;
    BG_PALETTE_SUB[0] = RGB15(3, 3, 2);
    BG_PALETTE_SUB[1] = RGB15(17, 15, 10);
    BG_PALETTE_SUB[2] = RGB15(23, 21, 15);
    BG_PALETTE_SUB[3] = RGB15(10, 9, 6);
    BG_PALETTE_SUB[4] = RGB15(8, 7, 5);
    for (int i = 0; i < 8; i++)
        BG_PALETTE_SUB[PI_WHITE + i] = fixed_pal[i];
    pal_n = 16;
    while (pal_n < 256)
    {
        int best = -1;
        u16 bc = 0;
        for (int b = 0; b < 4096; b++)
            if (hist[b] > bc)
            {
                bc = hist[b];
                best = b;
            }
        if (best < 0)
            break;
        hist[best] = 0;
        BG_PALETTE_SUB[pal_n] = RGB15((best & 15) * 2 + 1, ((best >> 4) & 15) * 2 + 1, ((best >> 8) & 15) * 2 + 1);
        bin_idx[best] = pal_n++;
    }
}

static u8 to_idx(u16 c)
{
    int b = bin12(c);
    if (bin_idx[b] == 0xFFFF)
    {
        int r = c & 31, g = (c >> 5) & 31, bl = (c >> 10) & 31, best = PI_WHITE, bd = 1 << 30;
        for (int i = PI_WHITE; i < pal_n; i++)
        {
            if (i > PI_BLACK && i < 16)
                continue;
            u16 p = BG_PALETTE_SUB[i];
            int dr = r - (p & 31), dg = g - ((p >> 5) & 31), db = bl - ((p >> 10) & 31);
            int d = dr * dr + dg * dg + db * db;
            if (d < bd)
            {
                bd = d;
                best = i;
            }
        }
        bin_idx[b] = best;
    }
    return bin_idx[b];
}

static void ov_render(int x0, int y0, int x1, int y1)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
        {
            u16 c = comp[y * W + x];
            ovb[y * W + x] = (c & 0x8000) ? to_idx(c) : 0;
        }
}

static void ov_flush(int y0, int y1)
{
    DC_FlushRange(ovb + y0 * W, (y1 - y0) * W);
    dmaCopy(ovb + y0 * W, (u8 *)vram + 65536 + y0 * 256, (y1 - y0) * 256);
}

static void comp_restore(int x0, int y0, int x1, int y1)
{
    for (int y = y0; y < y1; y++)
        memcpy(&comp[y * W + x0], &comp_static[y * W + x0], (x1 - x0) * 2);
}

static void slice9(int sx, int sy, int sw, int sh, int k, int dx, int dy, int dw, int dh, int center)
{
    const int sxs[3] = { sx, sx + k, sx + sw - k }, sws[3] = { k, sw - 2 * k, k };
    const int sys[3] = { sy, sy + k, sy + sh - k }, shs[3] = { k, sh - 2 * k, k };
    const int dxs[3] = { dx, dx + k, dx + dw - k }, dws[3] = { k, dw - 2 * k, k };
    const int dys[3] = { dy, dy + k, dy + dh - k }, dhs[3] = { k, dh - 2 * k, k };
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++)
            if ((i != 1 || j != 1 || center) && dws[i] > 0 && dhs[j] > 0)
                blit(comp, &panels, sxs[i], sys[j], sws[i], shs[j], dxs[i], dys[j], dws[i], dhs[j], 31);
}

static void comp_text_center(int x0, int x1, int y, u16 color, const char *s)
{
    text(comp, (x0 + x1 - 7 * (int)strlen(s)) / 2, y, color, 1, s);
}

static void comp_button(int b)
{
    int x0 = btn_x[b][0], x1 = btn_x[b][1], cx = (x0 + x1) / 2, cy = (FOOT_Y0 + FOOT_Y1) / 2;
    slice9(0, 79, 32, 17, 4, x0, FOOT_Y0, x1 - x0, FOOT_Y1 - FOOT_Y0, 1);
    switch (b)
    {
        case BTN_PLUS: blit(comp, &panels, 39, 128, 10, 10, cx - 5, cy - 5, 10, 10, 31); break;
        case BTN_MINUS: blit(comp, &panels, 50, 128, 10, 10, cx - 5, cy - 5, 10, 10, 31); break;
        case BTN_DIR:
            if (map_north)
                comp_text_center(x0, x1, cy - 7, RGB15(31, 26, 10), "N");
            else
            {
                blit(comp, &panels, 39, 139, 10, 10, cx - 11, cy - 5, 10, 10, 31);
                blit(comp, &panels, 50, 139, 10, 10, cx + 1, cy - 5, 10, 10, 31);
            }
            break;
        case BTN_CASE: comp_text_center(x0, x1, cy - 7, RGB15(31, 31, 31), T_BS_CASE); break;
        case BTN_MAP: comp_text_center(x0, x1, cy - 7, map_whole ? RGB15(31, 26, 10) : RGB15(31, 31, 31), T_BS_MAP); break;
    }
}

static void comp_layout(void)
{
    for (int i = 0; i < W * H; i++)
        comp[i] = 0;
    blit(comp, &panels, 0, 180, 240, 32, 0, 0, W, H, 12);
    blit(comp, &panels, 0, 0, 240, 30, 8, 1, 240, 30, 31);
    blit(comp, &panels, 0, 30, 240, 34, 8, 32, 240, 31, 31);
    slice9(8, 96, 50, 30, 7, MAP_X0 - 7, MAP_Y0 - 6, MAP_X1 - MAP_X0 + 14, MAP_Y1 - MAP_Y0 + 12, 0);
    for (int y = MAP_Y0; y < MAP_Y1; y++)
        for (int x = MAP_X0; x < MAP_X1; x++)
            comp[y * W + x] = 0;
    for (int b = 0; b < BTN_COUNT; b++)
        comp_button(b);
}

static int ecg(int t);
static int condition(const Vital *v);

static void comp_header(const Vital *v)
{
    comp_restore(0, 0, W, PANEL_Y1);
    text(comp, 18, 9, RGB15(30, 30, 29), 1, T_BS_LEON);
    if (v->ash_on)
        text(comp, 53, 9, RGB15(31, 18, 24), 1, T_BS_ASHLEY);
    char buf[24];
    snprintf(buf, sizeof(buf), "%d", v->fps);
    text(comp, 236 - 7 * (int)strlen(buf), 9, RGB15(31, 26, 10), 1, buf);
    text(comp, 28, 40, RGB15(18, 17, 15), 1, T_BS_AMMO);
    snprintf(buf, sizeof(buf), "%d / %d", v->ammo, v->spare);
    text(comp, 63, 40, RGB15(31, 31, 31), 1, buf);
    text(comp, 122, 40, RGB15(18, 17, 15), 1, T_BS_PTAS);
    snprintf(buf, sizeof(buf), "%d", v->ptas);
    text(comp, 226 - 7 * (int)strlen(buf), 40, RGB15(31, 31, 31), 1, buf);
    ov_render(0, 0, W, PANEL_Y1);
    hdr_ash = v->ash_on;
    hdr_ammo = v->ammo;
    hdr_spare = v->spare;
    hdr_ptas = v->ptas;
    hdr_fps = v->fps;
    hdr_drawn = 1;
}

static void ov_ecg(const Vital *v, int tick)
{
    int x0 = v->ash_on ? 104 : 56;
    ov_render(x0, 6, ECG_X1 + 1, 25);
    int cond = condition(v), speed = cond + 1, base = 18;
    u8 c = PI_GREEN + cond;
    int prev = base - ecg((x0 + tick * speed) & 63) / 2;
    for (int x = x0 + 1; x <= ECG_X1; x++)
    {
        int y = base - ecg((x + tick * speed) & 63) / 2;
        int a = prev < y ? prev : y, b = prev < y ? y : prev;
        for (int yy = a; yy <= b; yy++)
            ovb[yy * W + x] = c;
        prev = y;
    }
}

static void comp_map_labels(const Vital *v)
{
    comp_restore(MAP_X0, MAP_Y1 - 16, MAP_X0 + 150, MAP_Y1);
    comp_restore(MAP_X1 - 24, MAP_Y0, MAP_X1, MAP_Y0 + 16);
    if (v->medals > 0)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), T_BS_MEDALS, v->medals_hit, v->medals);
        text(comp, MAP_X0 + 3, MAP_Y1 - 16, RGB15(31, 31, 31), 1, buf);
    }
    const char *z = map_whole ? "--" : zoom_name[map_zoom];
    text(comp, MAP_X1 - 4 - 7 * (int)strlen(z), MAP_Y0 + 2, RGB15(31, 26, 10), 1, z);
    ov_render(MAP_X0, MAP_Y1 - 16, MAP_X0 + 150, MAP_Y1);
    ov_render(MAP_X1 - 24, MAP_Y0, MAP_X1, MAP_Y0 + 16);
    hdr_medals = v->medals;
    hdr_hit = v->medals_hit;
}

static void ui_full(const Vital *v)
{
    comp_layout();
    palette_build();
    memcpy(comp_static, comp, W * H * 2);
    ov_render(0, 0, W, H);
    comp_header(v);
    comp_map_labels(v);
    ov_flush(0, H);
}

static void tri_fill256(u8 *dst, int x0, int y0, int x1, int y1, int x2, int y2, u8 c)
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
    memset(mapb, 0, 65536);
    map_tw = map_th = 0;
    if (stage >= 0 && stage < 12 && stage_maps[stage].count)
    {
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
        map_tw = (x1 - x0) * map_k / 4096;
        map_th = (z1 - z0) * map_k / 4096;
        for (int i = 0; i < m->count; i++)
        {
            const MapTri *t = &m->tri[i];
            int px[3], py[3];
            for (int k = 0; k < 3; k++)
            {
                px[k] = (t->p[k * 2] - map_x0) * map_k / 4096;
                py[k] = (t->p[k * 2 + 1] - map_z0) * map_k / 4096;
            }
            tri_fill256(mapb, px[0], py[0], px[1], py[1], px[2], py[2], 1 + (t->group & 3));
        }
    }
    DC_FlushRange(mapb, 65536);
    dmaCopy(mapb, vram, 65536);
}

static void sprites_init(void)
{
    if (sprites_ready)
        return;
    vramSetBankI(VRAM_I_SUB_SPRITE);
    oamInit(&oamSub, SpriteMapping_Bmp_1D_128, false);
    for (int i = 0; i < 6; i++)
        note_gfx[i] = oamAllocateGfx(&oamSub, SpriteSize_32x16, SpriteColorFormat_Bmp);
    leon_gfx = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_Bmp);
    ash_gfx = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_Bmp);
    static u16 dot[8 * 8];
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
        {
            int dx = 2 * x - 7, dy = 2 * y - 7;
            dot[y * 8 + x] = dx * dx + dy * dy <= 40 ? 0x8000 | RGB15(31, 12, 20) : 0;
        }
    DC_FlushRange(dot, sizeof(dot));
    dmaCopy(dot, ash_gfx, sizeof(dot));
    sprites_ready = 1;
    sprites_ready_g = 1;
}

static int arrow_dir = -1;
static void arrow_draw(int fx, int fy)
{
    static u16 arrow[16 * 16];
    static u8 in[16 * 16];
    int c = inttof32(15) / 2, px = -fy, py = fx;
    int vx[3] = { c + fx * 7, c - fx * 6 + px * 4, c - fx * 6 - px * 4 };
    int vy[3] = { c + fy * 7, c - fy * 6 + py * 4, c - fy * 6 - py * 4 };
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
        {
            int sx = inttof32(x), sy = inttof32(y), pos = 0, neg = 0;
            for (int k = 0; k < 3; k++)
            {
                int j = (k + 1) % 3;
                s64 e = (s64)(vx[j] - vx[k]) * (sy - vy[k]) - (s64)(vy[j] - vy[k]) * (sx - vx[k]);
                if (e >= 0) pos++;
                if (e <= 0) neg++;
            }
            in[y * 16 + x] = pos == 3 || neg == 3;
        }
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
        {
            int i = y * 16 + x;
            int edge = in[i] && (x == 0 || y == 0 || x == 15 || y == 15 || !in[i - 1] || !in[i + 1] || !in[i - 16] || !in[i + 16]);
            arrow[i] = !in[i] ? 0 : edge ? 0x8000 | RGB15(6, 4, 0) : 0x8000 | RGB15(31, 28, 4);
        }
    DC_FlushRange(arrow, sizeof(arrow));
    dmaCopy(arrow, leon_gfx, sizeof(arrow));
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

static int in_rect(const touchPosition *t, int x0, int y0, int x1, int y1)
{
    return t->px >= x0 && t->px < x1 && t->py >= y0 && t->py < y1;
}

static int inside_map(int x, int y)
{
    return x >= MAP_X0 && x < MAP_X1 && y >= MAP_Y0 && y < MAP_Y1;
}

static int map_update(const Vital *v, u32 down, int tick, int touch)
{
    int result = VITAL_NONE;
    sprites_init();
    if (mode != 3 || map_stage != v->stage || map_redraw_g)
    {
        mode = 3;
        map_redraw_g = 0;
        layers_8();
        map_draw(v->stage);
        ui_full(v);
        note_shown = -1;
    }

    int controls = 0;
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
        int hit = -1;
        for (int b = 0; b < BTN_COUNT; b++)
            if (in_rect(&t, btn_x[b][0] - 1, FOOT_Y0 - 3, btn_x[b][1] + 2, H))
                hit = b;
        if (hit == BTN_PLUS || hit == BTN_MINUS)
        {
            if (hit == BTN_PLUS && map_zoom < 2)
                map_zoom++;
            if (hit == BTN_MINUS && map_zoom > 0)
                map_zoom--;
            map_whole = 0;
            controls = 1;
        }
        else if (hit == BTN_DIR)
        {
            map_north = !map_north;
            map_whole = 0;
            controls = 1;
        }
        else if (hit == BTN_CASE)
            result = VITAL_CASE;
        else if (hit == BTN_MAP)
        {
            map_whole = !map_whole;
            controls = 1;
        }
    }
    fake_x = -1;
    if (controls)
    {
        ui_full(v);
        result = VITAL_SELECT;
    }
    if (msg_frames > 0)
        msg_frames--;

    int s = sinLerp(v->leon_yaw), c = cosLerp(v->leon_yaw);
    int lx = (int)(((s64)(v->leon_x - inttof32(map_x0)) * map_k) >> 12);
    int lz = (int)(((s64)(v->leon_z - inttof32(map_z0)) * map_k) >> 12);
    int pa, pb, pc, pd, tx = lx, tz = lz, k;
    if (map_whole)
    {
        int kw = map_tw * 4096 / (MAP_X1 - MAP_X0 - 12), kh = map_th * 4096 / (MAP_Y1 - MAP_Y0 - 12);
        k = kw > kh ? kw : kh;
        if (k < 1024)
            k = 1024;
        tx = inttof32(8) + inttof32(map_tw) / 2;
        tz = inttof32(8) + inttof32(map_th) / 2;
        pa = pd = -k;
        pb = pc = 0;
    }
    else
    {
        k = (int)(((s64)map_k << 12) / zoom_px[map_zoom]);
        if (map_north)
        {
            pa = pd = -k;
            pb = pc = 0;
        }
        else
        {
            pa = -mulf32(c, k);
            pb = -mulf32(s, k);
            pc = mulf32(s, k);
            pd = -mulf32(c, k);
        }
    }
    aff[0] = pa >> 4;
    aff[1] = pb >> 4;
    aff[2] = pc >> 4;
    aff[3] = pd >> 4;
    aff[4] = (tx - pa * MAP_CX - pb * MAP_CY) >> 4;
    aff[5] = (tz - pc * MAP_CX - pd * MAP_CY) >> 4;

    if (map_whole)
    {
        leon_sx = MAP_CX - f32toint(divf32(lx - tx, k));
        leon_sy = MAP_CY - f32toint(divf32(lz - tz, k));
    }
    else
    {
        leon_sx = MAP_CX;
        leon_sy = MAP_CY;
    }
    leon_rot = map_whole || map_north;
    {
        int key = leon_rot ? (v->leon_yaw >> 9) & 63 : 64;
        if (key != arrow_dir)
        {
            arrow_dir = key;
            if (leon_rot)
                arrow_draw(-s, -c);
            else
                arrow_draw(0, -inttof32(1));
        }
    }
    ash_show = 0;
    if (v->ash_on)
    {
        if (map_whole)
        {
            int ax = (int)(((s64)(v->ash_x - inttof32(map_x0)) * map_k) >> 12);
            int az = (int)(((s64)(v->ash_z - inttof32(map_z0)) * map_k) >> 12);
            ash_sx = MAP_CX - f32toint(divf32(ax - tx, k));
            ash_sy = MAP_CY - f32toint(divf32(az - tz, k));
        }
        else
        {
            int z = zoom_px[map_zoom];
            int dx = mulf32(v->ash_x - v->leon_x, z), dz = mulf32(v->ash_z - v->leon_z, z);
            if (map_north)
            {
                ash_sx = MAP_CX - f32toint(dx);
                ash_sy = MAP_CY - f32toint(dz);
            }
            else
            {
                ash_sx = MAP_CX + f32toint(mulf32(-c, dx) + mulf32(s, dz));
                ash_sy = MAP_CY + f32toint(mulf32(-s, dx) - mulf32(c, dz));
            }
        }
        ash_show = inside_map(ash_sx - 3, ash_sy - 3) && inside_map(ash_sx + 3, ash_sy + 3);
    }

    int hdr = !hdr_drawn || v->ash_on != hdr_ash || v->ammo != hdr_ammo || v->spare != hdr_spare || v->ptas != hdr_ptas || v->fps != hdr_fps;
    if (hdr)
    {
        comp_header(v);
        ov_ecg(v, tick);
        ov_flush(0, PANEL_Y1);
    }
    else
    {
        ov_ecg(v, tick);
        ov_flush(6, 25);
    }
    if (v->medals != hdr_medals || v->medals_hit != hdr_hit)
    {
        comp_map_labels(v);
        ov_flush(MAP_Y0, MAP_Y1);
    }
    if ((msg_frames > 0) != note_shown)
    {
        note_shown = msg_frames > 0;
        note_draw(note_shown ? msg : NULL);
    }
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
    oamSet(&oamSub, n++, leon_sx - 8, leon_sy - 8, 1, 15, SpriteSize_16x16, SpriteColorFormat_Bmp, leon_gfx, -1, false,
           !inside_map(leon_sx, leon_sy), false, false, false);
    oamSet(&oamSub, n++, ash_sx - 4, ash_sy - 4, 1, 15, SpriteSize_8x8, SpriteColorFormat_Bmp, ash_gfx, -1, false, !ash_show, false, false, false);
    for (int i = 0; i < 6; i++)
        oamSet(&oamSub, n++, MAP_X0 + 2 + i * 32, MAP_Y0 + 2, 0, 15, SpriteSize_32x16, SpriteColorFormat_Bmp, note_gfx[i], -1, false, !note_shown, false, false, false);
    oamUpdate(&oamSub);
}
