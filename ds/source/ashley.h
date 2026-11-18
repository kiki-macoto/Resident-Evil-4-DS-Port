// アシュリー（5、6面）：レオンについて来て、レオンがこちらに銃を向けるとしゃがむ。
// 近い方であればガナードが彼女を狙う。モデルとアニメーション（5面のアーカイブの ch_ash）は、
// 彼女の出るステージを遊んでいる間だけ NitroFS から読み込む。
#ifndef ASHLEY_H__
#define ASHLEY_H__

#include <nds.h>

#include "anim.h"
#include "ground.h"
#include "model.h"

enum { ASH_FOLLOW, ASH_WAIT, ASH_HURT, ASH_DOWN };

typedef struct {
    Model model;
    AnimSet anims;
    void *model_buf, *anim_buf;
    int loaded;
    int on;                   // 今遊んでいるステージにいる
    int pos[3], feet, yaw;    // pos：モデルの原点（f32）。feet：床の高さ
    int state, frame;         // ASH_*。クリップ内の位置（f32）
    int clip;                 // 再生中のクリップ（変わったら最初から再生するため）
    int hp;
    int duck;                 // 呼び出し側が設定：レオンがこちらを狙っている
    int wait;                 // レオンが来るまで、ミッションの指定した場所で待つ
    // レオンが使ったはしご：彼女はその足元（か上）へ行き、後から登る。
    int ladder;               // 0 なし、LADDER_GOING、LADDER_ON
    int lad_foot[3], lad_dir[2], lad_top, lad_yaw, lad_up, lad_t;
    int pose[MODEL_MAX_NODES][12];
} Ashley;

// モデルとアニメーションを読み込む（1回だけ）。成功で0を返す。
int ashley_load(Ashley *a);
void ashley_unload(Ashley *a);

// (x, z)（f32）の、ref_y より下で一番近い床に、yaw の向きで置く。体力は満タン。
void ashley_place(Ashley *a, const Ground *g, int x, int z, int ref_y, int yaw);

// 60Hz の1フレーム分：レオンの左、少し後ろにつく（歩く。追いつくには走る）。
void ashley_update(Ashley *a, const Ground *g, const int leon_pos[3], int leon_yaw);

// レオンがはしごを上った（up = 1）か下りた：foot（f32、その床）、足場への向き dir
// （f32 の単位ベクトル、x z）、top（足場の高さ）、yaw（はしごでの向き、レオンと同じ）。彼女も後に続く。
void ashley_use_ladder(Ashley *a, const int foot[3], const int dir[2], int top, int yaw, int up);

// 攻撃を受けた（ガナードの振りか、流れ弾）。それで倒れたら1を返す。
int ashley_hit(Ashley *a, int damage);

// 光線 p + d*t（0 < t < *t_best）で彼女に当たる一番近い点（enemies_raycast() と同じ）。
int ashley_raycast(const Ashley *a, const int p[3], const int d[3], int *t_best);

// 点 (x, z) を彼女から押し出す（点のまわり半径 r）。
void ashley_push(const Ashley *a, int *x, int *z, int r);

void ashley_draw(Ashley *a, u32 poly_fmt);

#define ASHLEY_HP 100
enum { LADDER_GOING = 1, LADDER_ON };

#endif
