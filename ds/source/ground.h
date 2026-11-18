// re4zeebo/DsExport.cs が書き出す RDG2 の地面ファイルでの、床の高さの検索と壁の当たり判定。
#ifndef GROUND_H__
#define GROUND_H__

#include <nds.h>

typedef struct {
    int cells_x, cells_z;
    int origin_x, origin_z, cell_size;   // f32
    u32 triangles;
    const u32 *cell_start;
    const u16 *index;
    const s32 *tri;                      // 三角形ごとに9ワード：x0 z0 x1 z1 x2 z2 a b c
    u32 walls;
    const u32 *wall_cell_start;
    const u16 *wall_index;
    const s32 *wall;                     // 壁ごとに6ワード：x0 z0 x1 z1 y_min y_max
    // 光線が当たるもの（原作のカメラ用と弾用のグループ）、同じ格子で。None：ファイルに無い。
    u32 ray_tris, ray_walls;
    const u32 *ray_cell_start, *ray_wall_cell_start;
    const u16 *ray_index, *ray_wall_index;
    const s32 *ray_tri, *ray_wall;
    const u8 *ray_tri_mask, *ray_wall_mask;   // RAY_CAMERA、RAY_SHOT
} Ground;

enum { RAY_CAMERA = 1, RAY_SHOT = 2 };

// 成功で0を返す。
int ground_load(Ground *g, const void *data, size_t size);

// 戸口：この箱の中では壁に当たらない（中心 x, z。扉の板に沿った単位ベクトル。長さの半分。
// 奥行きの半分、f32）。部屋の当たり判定の輪郭は、自分の戸口をまたいで引かれていることがある
// （アシュリーが待つ5面の部屋）：そのままでは扉を開けても閉じたままになる。
void ground_clear_gaps(void);
void ground_add_gap(int cx, int cz, int ux, int uz, int half_len, int half_depth);

// (x, z) の下の床の高さ（f32）：ref_y + step_up 以下で一番高い床。上の屋根は無視し、階段や坂は
// たどる。そこにそういう床が無ければ0を返す（*y はそのまま）。
int ground_height(const Ground *g, int x, int z, int ref_y, int step_up, int *y);

// 縦の円柱（中心 *x, *z、半径、高さ y_lo..y_hi）を、重なっている壁から押し出す。
// 押し出した回数を返す。
int ground_collide(const Ground *g, int *x, int *z, int y_lo, int y_hi, int radius);
// 回数を指定して同じことをする（1回の方が軽い：敵用）。
int ground_collide_n(const Ground *g, int *x, int *z, int y_lo, int y_hi, int radius, int passes);

// p（f32）から単位ベクトル d（f32）の向きへ、マスク（RAY_CAMERA、RAY_SHOT）の壁と床に対して
// max_dist（f32）まで光線を飛ばす。当たれば1を返し、一番近い当たりの点を hit[3]（f32）に入れる。
// 当たらなければ0。
int ground_raycast(const Ground *g, const int p[3], const int d[3], int max_dist, int hit[3], int mask);

#endif
