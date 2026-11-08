#include <string.h>

#include "text.h"

// A line is a row of 32x16 sprites (4bpp, 1D mapping, 8 tiles each): 8 for a wide line (2 KB),
// 4 for a narrow one. 12 wide + 8 narrow lines fill VRAM F + G (32 KB) and all 128 sprites.
#define MAX_SPRITES      8
#define MAX_CHARS        48

static int line_sprites(int i) { return i < TEXT_WIDE_LINES ? 8 : 4; }
static int line_first_sprite(int i) { return i < TEXT_WIDE_LINES ? i * 8 : TEXT_WIDE_LINES * 8 + (i - TEXT_WIDE_LINES) * 4; }

typedef struct {
    u16 code;
    u8 width;
    const u32 *rows;   // TEXT_HEIGHT rows, 2 bits per pixel
} Glyph;

static Glyph glyphs[512];
static int glyph_count;

typedef struct {
    char str[MAX_CHARS * 3 + 1];
    int x, y, color, visible, width, big;
} Line;
static Line lines[TEXT_LINES];
static u32 raster[MAX_SPRITES * 8 * 8];   // one line's tiles, built in RAM then copied to VRAM
static int raster_width;                  // pixels of the line being built

static u16 rd16(const u8 *p) { return p[0] | p[1] << 8; }

int text_init(const void *font, size_t size)
{
    const u8 *p = font;
    if (size < 8 || memcmp(p, "RDF1", 4) != 0 || rd16(p + 6) != TEXT_HEIGHT)
        return -1;
    glyph_count = rd16(p + 4);
    if (glyph_count > (int)(sizeof(glyphs) / sizeof(glyphs[0])) || 8 + glyph_count * (4 + 4 * TEXT_HEIGHT) > (int)size)
        return -2;
    for (int i = 0; i < glyph_count; i++)
    {
        const u8 *g = p + 8 + i * (4 + 4 * TEXT_HEIGHT);
        glyphs[i].code = rd16(g);
        glyphs[i].width = g[2];
        glyphs[i].rows = (const u32 *)(g + 4);
    }

    vramSetBankF(VRAM_F_MAIN_SPRITE_0x06400000);
    vramSetBankG(VRAM_G_MAIN_SPRITE_0x06404000);
    oamInit(&oamMain, SpriteMapping_1D_32, false);
    // Palettes: colours 1..3 = the glyph's dark/mid/light pixels, 4 = drop shadow.
    // (pink: the pause menu's help line, the game's FFB4FF)
    static const u8 tint[6][3] = { { 31, 31, 31 }, { 31, 26, 10 }, { 31, 8, 6 }, { 17, 17, 17 }, { 10, 31, 12 }, { 31, 22, 31 } };
    for (int c = 0; c < 6; c++)
    {
        u16 *pal = SPRITE_PALETTE + c * 16;
        for (int l = 1; l <= 3; l++)
            pal[l] = RGB15(tint[c][0] * (l + 1) / 4, tint[c][1] * (l + 1) / 4, tint[c][2] * (l + 1) / 4);
        pal[4] = RGB15(1, 1, 2);
    }
    text_clear();
    return 0;
}

static const Glyph *find(u32 code)
{
    int lo = 0, hi = glyph_count - 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        if (glyphs[mid].code == code)
            return &glyphs[mid];
        if (glyphs[mid].code < code)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return NULL;
}

// Next UTF-8 code point (BMP only) of *s.
static u32 next_code(const char **s)
{
    const u8 *p = (const u8 *)*s;
    u32 c = *p++;
    if (c >= 0xE0 && p[0] && p[1])
    {
        c = (c & 0x0F) << 12 | (p[0] & 0x3F) << 6 | (p[1] & 0x3F);
        p += 2;
    }
    else if (c >= 0xC0 && p[0])
    {
        c = (c & 0x1F) << 6 | (p[0] & 0x3F);
        p += 1;
    }
    *s = (const char *)p;
    return c;
}

static int advance(u32 code, const Glyph *g)
{
    if (g)
        return g->width;
    return code < 0x80 ? TEXT_HEIGHT / 2 : TEXT_HEIGHT;   // spaces, missing glyphs
}

int text_width(const char *s)
{
    int w = 0;
    while (*s)
    {
        u32 c = next_code(&s);
        w += advance(c, find(c));
    }
    return w;
}

static void put(int x, int y, int v)
{
    if (x < 0 || x >= raster_width || y < 0 || y >= 16)
        return;
    // Sprite (x / 32), tile row (y / 8), tile column within the sprite; 8 pixels per u32.
    int tile = (x / 32) * 8 + (y / 8) * 4 + (x % 32) / 8;
    u32 *row = &raster[tile * 8 + y % 8];
    int shift = (x % 8) * 4;
    *row = (*row & ~(0xFu << shift)) | (u32)v << shift;
}

static void rasterize(int i)
{
    Line *l = &lines[i];
    int sprites = line_sprites(i);
    raster_width = sprites * 32;
    memset(raster, 0, sizeof(raster));
    int x = 0;
    for (int pass = 0; pass < 2; pass++)   // shadow (1 pixel down-right) first, then the glyphs
    {
        const char *s = l->str;
        x = 0;
        while (*s)
        {
            u32 c = next_code(&s);
            const Glyph *g = find(c);
            if (g)
                for (int y = 0; y < TEXT_HEIGHT; y++)
                    for (int px = 0; px < g->width; px++)
                    {
                        int v = (g->rows[y] >> (2 * px)) & 3;
                        if (v)
                            put(x + px + (pass ? 0 : 1), y + (pass ? 0 : 1), pass ? v : 4);
                    }
            x += advance(c, g);
        }
    }
    l->width = x;
    u32 *dst = (u32 *)((u8 *)SPRITE_GFX + line_first_sprite(i) * 8 * 32);   // VRAM: 32-bit writes only
    for (int k = 0; k < sprites * 8 * 8; k++)
        dst[k] = raster[k];
}

static void plot16(u16 *dst, int stride, int w, int h, int x, int y, int scale, u16 c)
{
    for (int dy = 0; dy < scale; dy++)
        for (int dx = 0; dx < scale; dx++)
        {
            int px = x + dx, py = y + dy;
            if (px >= 0 && py >= 0 && px < w && py < h)
                dst[py * stride + px] = c;
        }
}

int text_draw16(u16 *dst, int stride, int w, int h, int x, int y, u16 color, int scale, const char *s)
{
    // Glyph levels 1..3 = 1/2, 3/4, full brightness of color.
    u16 shade[4] = { 0, 0, 0, 0 };
    for (int l = 1; l <= 3; l++)
    {
        int r = color & 31, g = (color >> 5) & 31, b = (color >> 10) & 31;
        shade[l] = 0x8000 | RGB15(r * (l + 1) / 4, g * (l + 1) / 4, b * (l + 1) / 4);
    }
    int x0 = x;
    for (int pass = 0; pass < 2; pass++)   // shadow first
    {
        const char *p = s;
        x = x0;
        while (*p)
        {
            u32 c = next_code(&p);
            const Glyph *g = find(c);
            if (g)
                for (int gy = 0; gy < TEXT_HEIGHT; gy++)
                    for (int gx = 0; gx < g->width; gx++)
                    {
                        int v = (g->rows[gy] >> (2 * gx)) & 3;
                        if (v)
                            plot16(dst, stride, w, h, x + gx * scale + (pass ? 0 : scale), y + gy * scale + (pass ? 0 : scale),
                                   scale, pass ? shade[v] : 0x8000 | RGB15(1, 1, 2));
                    }
            x += advance(c, g) * scale;
        }
    }
    return x - x0;
}

void text_line(int i, int x, int y, int color, const char *s)
{
    if (i < 0 || i >= TEXT_LINES)
        return;
    Line *l = &lines[i];
    l->x = x;
    l->y = y;
    l->color = color;
    l->visible = 1;
    l->big = 0;
    if (strncmp(l->str, s, sizeof(l->str) - 1) != 0)
    {
        strncpy(l->str, s, sizeof(l->str) - 1);
        l->str[sizeof(l->str) - 1] = 0;
        rasterize(i);
    }
}

void text_big(int i, int x, int y, int color, const char *s)
{
    if (i < 0 || i >= TEXT_WIDE_LINES)
        return;
    text_line(i, x, y, color, s);
    lines[i].big = 1;
}

void text_hide(int i)
{
    if (i >= 0 && i < TEXT_LINES)
        lines[i].visible = 0;
}

void text_clear(void)
{
    for (int i = 0; i < TEXT_LINES; i++)
        lines[i].visible = 0;
}

void text_commit(void)
{
    // Affine matrix 0 doubles the size (8.8 fixed point, inverse scale); a double-size sprite
    // covers 64x32 pixels with its 32x16 picture scaled to fill it.
    oamRotateScale(&oamMain, 0, 0, 1 << 7, 1 << 7);
    for (int i = 0; i < TEXT_LINES; i++)
    {
        Line *l = &lines[i];
        for (int k = 0; k < line_sprites(i); k++)
        {
            int id = line_first_sprite(i) + k;
            int used = l->visible && k * 32 < l->width + 1;
            u16 *gfx = (u16 *)((u8 *)SPRITE_GFX + id * 8 * 32);
            if (l->big)
                oamSet(&oamMain, id, l->x + k * 64, l->y, 0, l->color, SpriteSize_32x16, SpriteColorFormat_16Color,
                       gfx, used ? 0 : -1, used, !used, false, false, false);
            else
                oamSet(&oamMain, id, l->x + k * 32, l->y, 0, l->color, SpriteSize_32x16, SpriteColorFormat_16Color,
                       gfx, -1, false, !used, false, false, false);
        }
    }
    oamUpdate(&oamMain);
}
