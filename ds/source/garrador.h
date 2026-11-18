// ガラドール（8面）：8面のアーカイブの ch_gal を、そのステージの間だけ NitroFS から読む。
// 目が見えない：聞き耳を立てて立ち、物音（銃声、近くで走るレオン、すぐ横のレオン）のした方へ
// 突進し、そこにレオンがいれば爪で斬る。壁で終わった突進は、しばらく爪が壁に刺さる。
// 背中（寄生体）への弾が一番効く。
#ifndef GARRADOR_H__
#define GARRADOR_H__

#include <nds.h>

#include "anim.h"
#include "ground.h"
#include "model.h"

enum { GAL_LISTEN, GAL_CHARGE, GAL_SLASH, GAL_STUCK, GAL_HIT, GAL_DEAD };

typedef struct {
    Model model;
    AnimSet anims;
    void *model_buf, *anim_buf;
    int loaded;
    int shared;        // モデルとクリップをほかから写したもの（garrador_share）
    int on;
    int pos[3], feet, yaw;
    int state, frame, clip;
    int hp;            // 原作の単位（ch_param_5 種類0：3500）
    int flinch;        // 原作のひるみの合計：しきい値でよろめく
    int target[3];     // 突進している物音の場所
    int timer;         // 今の状態のフレーム数
    int struck;        // この斬りはもうレオンへの当たりを調べた
    int pose[MODEL_MAX_NODES][12];
} Garrador;

typedef struct {
    int hit_leon;      // このフレームにレオンへ与えたダメージ（0 = なし）
    int hit_ally;      // このフレームにアシュリーへ与えたダメージ（0 = なし）
    int roar;          // 何かを聞いて突進する（叫び声の合図）
    int down;          // このフレームで完全に倒れた
} GarradorEvents;

int garrador_load(Garrador *g);
void garrador_unload(Garrador *g);
// 1体目のモデルとクリップで描く2体目のガラドール（読み込みは1回）。
void garrador_share(Garrador *g, const Garrador *from);
void garrador_place(Garrador *g, const Ground *gr, int x, int z, int ref_y, int yaw);
// ally_pos：アシュリー（いなければ NULL）。前方にいれば爪は彼女にも当たる。
void garrador_update(Garrador *g, const Ground *gr, const int leon_pos[3], const int *ally_pos, int vulnerable, GarradorEvents *ev);

// pos（f32）での、`range`（f32）まで届く物音：範囲内にいればそちらへ突進する。
void garrador_hear(Garrador *g, const int pos[3], int range);

// 弾の光線 p + d*t（0 < t < *t_best）：体に当たれば *t_best を更新して1を返し、
// *back に背後から（寄生体）かどうかを入れる。
int garrador_raycast(const Garrador *g, const int p[3], const int d[3], int *t_best, int *back);
// power：原作の単位の弾の火力（ハンドガン 100）。
void garrador_shot(Garrador *g, int power, int back);

// (x, z) を彼の体から押し出す。
void garrador_push(const Garrador *g, int *x, int *z, int r);

void garrador_draw(Garrador *g, u32 poly_fmt);

#endif
