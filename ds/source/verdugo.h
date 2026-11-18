// ヴェルデューゴ（12面）：12面のアーカイブの ch_ver、そのステージの間だけ NitroFS から読む。彼は
// 素早くレオンに忍び寄り、尾で打つ。時々跳び上がって見えなくなり、レオンのそばに降りてくる。
// 弾はほとんど効かないが、凍っていれば別：彼の近くで液体窒素のタンクを撃って開けると（main.c）
// 数秒固まり、そのとき弾が効く。
#ifndef VERDUGO_H__
#define VERDUGO_H__

#include <nds.h>

#include "anim.h"
#include "ground.h"
#include "model.h"
#include "sheet.h"

enum { VER_STALK, VER_TAIL, VER_HIT, VER_FROZEN, VER_UP, VER_GONE, VER_DROP, VER_DEAD };

typedef struct {
    Model model;
    AnimSet anims;
    void *model_buf, *anim_buf;
    int loaded;
    int on;
    int pos[3], feet, yaw;
    int state, frame, clip;
    int hp;            // 原作の単位（ch_param_7 の種類0：20000）
    int flinch;        // 原作のひるみの合計
    int timer;         // 今の状態のフレーム数
    int struck;        // この尾の打撃はもうレオンへの当たりを調べた
    int next_leap;     // また跳び上がるまでの忍び寄りのフレーム数
    int pose[MODEL_MAX_NODES][12];
} Verdugo;

typedef struct {
    int hit_leon;      // このフレームにレオンへ与えたダメージ（0 = なし）
    int landed;        // このフレームに上から降りてきた
    int thawed;        // このフレームに氷がとけた
} VerdugoEvents;

int verdugo_load(Verdugo *v);
void verdugo_unload(Verdugo *v);
void verdugo_place(Verdugo *v, const Ground *gr, int x, int z, int ref_y, int yaw);
void verdugo_update(Verdugo *v, const Ground *gr, const int leon_pos[3], int leon_feet, int vulnerable, VerdugoEvents *ev);

// pos（f32）での冷気の爆発：範囲内なら凍る。凍ったら1を返す。
int verdugo_freeze(Verdugo *v, const int pos[3], int range);

// 弾の光線 p + d*t、0 < t < *t_best：体に当たれば *t_best を更新して1を返す。
int verdugo_raycast(const Verdugo *v, const int p[3], const int d[3], int *t_best);
// power：原作の単位の弾の火力（ハンドガン 100）。
void verdugo_shot(Verdugo *v, int power);

void verdugo_push(const Verdugo *v, int *x, int *z, int r);

void verdugo_draw(Verdugo *v, u32 poly_fmt);
void verdugo_draw_fx(Verdugo *v, const Sheet *fx);   // 彼の上の霜（billboard_begin() の後に）

#endif
