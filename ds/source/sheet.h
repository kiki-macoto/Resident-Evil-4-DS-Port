// 2D の絵（re4zeebo -Ds2D の RD2D シート）を、3D エンジンで画面空間の四角形として、場面の手前に
// 描く：HUD の部品、タイトルの絵。
#ifndef SHEET_H__
#define SHEET_H__

#include <nds.h>

typedef struct {
    int w, h;    // 絵の大きさ、ピクセル（テクスチャはもっと大きく、右 / 下に余白があることがある）
    int tex;     // テクスチャの名前
} Sheet;

// シートを解析してテクスチャを転送する。成功で0を返す。
int sheet_load(Sheet *s, const void *data, size_t size);

// VRAM のシートのテクスチャ（とパレット）を解放する。
void sheet_free(Sheet *s);

// ビルボード：カメラの方を向けた 3D の場面の中の絵（縦の軸のまわりだけなので、光の柱は立ったまま）。
// billboard_begin() がそのフレームのカメラを受け取る。
void billboard_begin(const int eye[3], const int at[3]);
// 部分 (sx, sy, sw, sh) を pos を中心に w x h ワールド単位（f32）で、絵の面の中で angle だけ回して
// 描く。flip_x/flip_y で反転。poly_fmt を OR する（たとえば POLY_FOG）。
void billboard(const Sheet *s, int sx, int sy, int sw, int sh, const int pos[3], int w, int h,
               int angle, int flip, u16 color, int alpha, int id, u32 poly_fmt);
#define FLIP_X 1
#define FLIP_Y 2
// 同じもの、絵をローカルの長方形 (x0, y0) 左上 .. (x1, y1) 右下に広げて（f32、w と h の単位、
// pos のまわり。billboard() は -0.5..0.5 を使う）。
void billboard_rect(const Sheet *s, int sx, int sy, int sw, int sh, const int pos[3], int w, int h,
                    int x0, int y0, int x1, int y1, int angle, int flip, u16 color, int alpha, int id, u32 poly_fmt);

// overlay_begin() と overlay_end() の間の画面空間の描画：座標はピクセル（0..256 x 0..192、y は
// 下向き）で、各四角形は前のものの手前に来る。
void overlay_begin(void);
// 次の四角形は深度の範囲の奥に（その後に描く 3D の後ろ）、次の overlay_begin() まで。
void overlay_far(void);
// 平らな三角形（画面のピクセル）。
void overlay_tri(int x0, int y0, int x1, int y1, int x2, int y2, u16 color, int alpha, int id);
void overlay_end(void);
// keep 1：次の overlay_begin() は新しく始めず、前の回の手前に続ける。
// shift_y は描くもの全部をそのピクセル数だけ動かす（後で 0, 0 に）。
void overlay_keep_depth(int keep, int shift_y);

// テクスチャなしの長方形（アルファ 1..31）。
void overlay_rect(int x0, int y0, int x1, int y1, u16 color, int alpha, int id);

// 1ピクセルの線（ハードウェアは2つの角が同じ三角形を線として描く）。
void overlay_line(int x0, int y0, int x1, int y1, u16 color, int id);

// 絵の部分 (sx, sy, sw, sh) を (dx, dy) に dw x dh に引き伸ばし、color で色付けして描く。
void overlay_sheet(const Sheet *s, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                   u16 color, int alpha, int id);

#endif
