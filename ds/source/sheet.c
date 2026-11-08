#include <string.h>

#include "model.h"
#include "sheet.h"

static u16 rd16(const u8 *p) { return p[0] | p[1] << 8; }
static u32 rd32(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }

int sheet_load(Sheet *s, const void *data, size_t size)
{
    const u8 *p = data;
    memset(s, 0, sizeof(*s));
    if (size < 20 || memcmp(p, "RD2D", 4) != 0)
        return -1;
    s->w = rd16(p + 4);
    s->h = rd16(p + 6);
    p += 8;
    // Texture record as in RDS1 models (see re4zeebo/Ds2D.cs).
    int w = rd16(p), h = rd16(p + 2);
    GL_TEXTURE_TYPE_ENUM type = p[4];
    int transparent = p[5] & 1;
    int pal_colors = rd16(p + 6);
    u32 bytes = rd32(p + 8);
    const u8 *texels = p + 12;
    if (12 + 8 + bytes + pal_colors * 2 > size)
        return -2;
    // No wrapping: neighbouring parts of the picture must not bleed in at the edges.
    int param = TEXGEN_TEXCOORD | (transparent ? GL_TEXTURE_COLOR0_TRANSPARENT : 0);
    glGenTextures(1, &s->tex);
    glBindTexture(0, s->tex);
    if (!glTexImageNtr2D(type, w, h, param, texels, NULL))
        return -3;
    if (!glColorTableNtr(pal_colors, (const u16 *)(texels + bytes)))
        return -4;
    return 0;
}

void sheet_free(Sheet *s)
{
    if (s->tex)
        glDeleteTextures(1, &s->tex);
    s->tex = 0;
}

static int bb_right[3], bb_normal[3];

void billboard_begin(const int eye[3], const int at[3])
{
    model_sync();   // a character's list may still be streaming to the geometry FIFO
    // Horizontal direction from the camera; the picture plane faces back along it.
    int fx = at[0] - eye[0], fz = at[2] - eye[2];
    int len = (int)sqrt64((u64)((s64)fx * fx + (s64)fz * fz));
    if (len == 0)
        len = 1;
    bb_normal[0] = -divf32(fx, len);
    bb_normal[1] = 0;
    bb_normal[2] = -divf32(fz, len);
    bb_right[0] = bb_normal[2];   // up x normal
    bb_right[1] = 0;
    bb_right[2] = -bb_normal[0];
}

void billboard(const Sheet *s, int sx, int sy, int sw, int sh, const int pos[3], int w, int h,
               int angle, int flip, u16 color, int alpha, int id, u32 poly_fmt)
{
    const int half = inttof32(1) / 2;
    billboard_rect(s, sx, sy, sw, sh, pos, w, h, -half, half, half, -half, angle, flip, color, alpha, id, poly_fmt);
}

void billboard_rect(const Sheet *s, int sx, int sy, int sw, int sh, const int pos[3], int w, int h,
                    int x0, int y0, int x1, int y1, int angle, int flip, u16 color, int alpha, int id, u32 poly_fmt)
{
    glPushMatrix();
    // Rows: the picture's x axis, y axis, normal, then the position (hardware row-vector order).
    m4x3 m = { {
        bb_right[0], bb_right[1], bb_right[2],
        0, inttof32(1), 0,
        bb_normal[0], bb_normal[1], bb_normal[2],
        pos[0], pos[1], pos[2],
    } };
    glMultMatrix4x3(&m);
    if (angle)
        glRotateZi(angle);
    glScalef32(w, h, inttof32(1));
    int u0 = sx, u1 = sx + sw, v0 = sy, v1 = sy + sh;
    if (flip & FLIP_X) { u0 = sx + sw; u1 = sx; }
    if (flip & FLIP_Y) { v0 = sy + sh; v1 = sy; }
    glBindTexture(0, s->tex);
    glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(id) | poly_fmt);
    glBegin(GL_QUADS);
    glColor(color);
    glTexCoord2t16(inttot16(u0), inttot16(v0));
    glVertex3v16(x0, y0, 0);
    glTexCoord2t16(inttot16(u1), inttot16(v0));
    glVertex3v16(x1, y0, 0);
    glTexCoord2t16(inttot16(u1), inttot16(v1));
    glVertex3v16(x1, y1, 0);
    glTexCoord2t16(inttot16(u0), inttot16(v1));
    glVertex3v16(x0, y1, 0);
    glEnd();
    glPopMatrix(1);
}

// Orthographic projection of 4 x 3 units = 256 x 192 pixels (64 v16 units per pixel), y down.
// Depth: z = +1 is the near plane; each quad moves a little nearer than the last.
#define Z_FIRST floattof32(0.5)
#define Z_STEP  8
static int z, keep_depth, shift;

void overlay_begin(void)
{
    model_sync();
    // The characters' DMA'd lists set the texture behind libnds' back, and it skips a bind
    // of the texture it believes is current: start from none, so the next bind really binds.
    glBindTexture(0, 0);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrthof32(0, inttof32(4), inttof32(3), 0, -inttof32(1), inttof32(1));
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    if (!keep_depth)
        z = Z_FIRST;
}

void overlay_tri(int x0, int y0, int x1, int y1, int x2, int y2, u16 color, int alpha, int id)
{
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(id));
    glBegin(GL_TRIANGLES);
    glColor(color);
    glVertex3v16(x0 * 64, (y0 + shift) * 64, z);
    glVertex3v16(x1 * 64, (y1 + shift) * 64, z);
    glVertex3v16(x2 * 64, (y2 + shift) * 64, z);
    glEnd();
    z += Z_STEP;
}

void overlay_far(void)
{
    z = -floattof32(0.95);
}

void overlay_keep_depth(int keep, int shift_y)
{
    keep_depth = keep;
    shift = shift_y;
}

void overlay_end(void)
{
    glPopMatrix(1);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix(1);
    glMatrixMode(GL_MODELVIEW);
}

void overlay_rect(int x0, int y0, int x1, int y1, u16 color, int alpha, int id)
{
    y0 += shift;
    y1 += shift;
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(id));
    glBegin(GL_QUADS);
    glColor(color);
    glVertex3v16(x0 * 64, y0 * 64, z);
    glVertex3v16(x1 * 64, y0 * 64, z);
    glVertex3v16(x1 * 64, y1 * 64, z);
    glVertex3v16(x0 * 64, y1 * 64, z);
    glEnd();
    z += Z_STEP;
}

void overlay_line(int x0, int y0, int x1, int y1, u16 color, int id)
{
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(id));
    glBegin(GL_TRIANGLES);
    glColor(color);
    glVertex3v16(x0 * 64, y0 * 64, z);
    glVertex3v16(x1 * 64, y1 * 64, z);
    glVertex3v16(x1 * 64, y1 * 64, z);
    glEnd();
    z += Z_STEP;
}

void overlay_sheet(const Sheet *s, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                   u16 color, int alpha, int id)
{
    dy += shift;
    glBindTexture(0, s->tex);
    glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(id));
    glBegin(GL_QUADS);
    glColor(color);
    glTexCoord2t16(inttot16(sx), inttot16(sy));
    glVertex3v16(dx * 64, dy * 64, z);
    glTexCoord2t16(inttot16(sx + sw), inttot16(sy));
    glVertex3v16((dx + dw) * 64, dy * 64, z);
    glTexCoord2t16(inttot16(sx + sw), inttot16(sy + sh));
    glVertex3v16((dx + dw) * 64, (dy + dh) * 64, z);
    glTexCoord2t16(inttot16(sx), inttot16(sy + sh));
    glVertex3v16(dx * 64, (dy + dh) * 64, z);
    glEnd();
    z += Z_STEP;
}
