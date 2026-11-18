#ifndef SHEET_H__
#define SHEET_H__

#include <nds.h>

typedef struct {
    int w, h;
    int tex;
} Sheet;

int sheet_load(Sheet *s, const void *data, size_t size);

void sheet_free(Sheet *s);

void billboard_begin(const int eye[3], const int at[3]);
void billboard(const Sheet *s, int sx, int sy, int sw, int sh, const int pos[3], int w, int h,
               int angle, int flip, u16 color, int alpha, int id, u32 poly_fmt);
#define FLIP_X 1
#define FLIP_Y 2
void billboard_rect(const Sheet *s, int sx, int sy, int sw, int sh, const int pos[3], int w, int h,
                    int x0, int y0, int x1, int y1, int angle, int flip, u16 color, int alpha, int id, u32 poly_fmt);

void overlay_begin(void);
void overlay_far(void);
void overlay_tri(int x0, int y0, int x1, int y1, int x2, int y2, u16 color, int alpha, int id);
void overlay_end(void);
void overlay_keep_depth(int keep, int shift_y);

void overlay_rect(int x0, int y0, int x1, int y1, u16 color, int alpha, int id);

void overlay_line(int x0, int y0, int x1, int y1, u16 color, int id);

void overlay_sheet(const Sheet *s, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                   u16 color, int alpha, int id);

#endif
