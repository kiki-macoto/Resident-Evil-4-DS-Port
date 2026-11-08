// 2D pictures (RD2D sheets from re4zeebo -Ds2D) drawn as screen-space quads with the 3D
// engine, in front of the scene: HUD parts, the title art.
#ifndef SHEET_H__
#define SHEET_H__

#include <nds.h>

typedef struct {
    int w, h;    // picture size in pixels (the texture may be larger, padded right/down)
    int tex;     // texture name
} Sheet;

// Parses the sheet and uploads its texture. Returns 0 on success.
int sheet_load(Sheet *s, const void *data, size_t size);

// Frees the sheet's texture (and palette) in VRAM.
void sheet_free(Sheet *s);

// Billboards: pictures in the 3D scene turned to face the camera (about the vertical axis
// only, so light columns stay upright). billboard_begin() takes the camera for the frame.
void billboard_begin(const int eye[3], const int at[3]);
// Part (sx, sy, sw, sh) drawn w x h world units (f32) centred on pos, turned by angle in the
// picture plane; flip_x/flip_y mirror it. poly_fmt is OR-ed in (e.g. POLY_FOG).
void billboard(const Sheet *s, int sx, int sy, int sw, int sh, const int pos[3], int w, int h,
               int angle, int flip, u16 color, int alpha, int id, u32 poly_fmt);
#define FLIP_X 1
#define FLIP_Y 2
// The same with the picture spread over the local rectangle (x0, y0) top-left .. (x1, y1)
// bottom-right (f32, in units of w and h, around pos; billboard() uses -0.5..0.5).
void billboard_rect(const Sheet *s, int sx, int sy, int sw, int sh, const int pos[3], int w, int h,
                    int x0, int y0, int x1, int y1, int angle, int flip, u16 color, int alpha, int id, u32 poly_fmt);

// Screen-space drawing between overlay_begin() and overlay_end(): coordinates are pixels
// (0..256 x 0..192, y down) and each quad lands in front of the previous one.
void overlay_begin(void);
// The next quads at the back of the depth range (behind any 3D drawn after them), until the next overlay_begin().
void overlay_far(void);
// A flat triangle (screen pixels).
void overlay_tri(int x0, int y0, int x1, int y1, int x2, int y2, u16 color, int alpha, int id);
void overlay_end(void);
// keep 1: the next overlay_begin() goes on in front of the last pass instead of starting afresh;
// shift_y moves all that is drawn by so many pixels (0, 0 afterwards).
void overlay_keep_depth(int keep, int shift_y);

// Untextured rectangle (alpha 1..31).
void overlay_rect(int x0, int y0, int x1, int y1, u16 color, int alpha, int id);

// 1-pixel line (the hardware draws a triangle with two equal corners as a line).
void overlay_line(int x0, int y0, int x1, int y1, u16 color, int id);

// Picture part (sx, sy, sw, sh) drawn at (dx, dy) stretched to dw x dh, tinted by color.
void overlay_sheet(const Sheet *s, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                   u16 color, int alpha, int id);

#endif
