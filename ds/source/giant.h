// エルヒガンテ（4面と11面）：4面のアーカイブの ch_elg を、そのステージの間だけ NitroFS から読む。
// レオンへ歩いて攻撃する。弾で弱らせるとひざをつき、背中から寄生体が出る：そのとき近くで A を
// 押すとレオンが斬る。3回斬ると倒れる。
#ifndef GIANT_H__
#define GIANT_H__

#include <nds.h>

#include "anim.h"
#include "ground.h"
#include "model.h"

enum { GIANT_WALK, GIANT_ATTACK, GIANT_HIT, GIANT_KNEEL, GIANT_RISE, GIANT_DEAD };

typedef struct {
    Model model;
    AnimSet anims;
    void *model_buf, *anim_buf;
    int loaded;
    int shared;        // モデルとクリップをほかから写したもの（giant_share）
    int on;
    int pos[3], feet, yaw;
    int state, frame, clip;
    int hp;            // 原作のひるみの合計：しきい値でひざをつく
    int cuts;          // これまでに寄生体を斬った回数
    int life;          // 体力、原作の単位（ch_param_6 種類0：4500）：0 で倒れる
    int held;          // 呼び出し側が設定：レオンが登っている
    int timer;         // 今の状態のフレーム数
    int struck;        // この攻撃はもうレオンへの当たりを調べた
    int pose[MODEL_MAX_NODES][12];
} Giant;

typedef struct {
    int hit_leon;      // このフレームにレオンへ与えたダメージ（0 = なし）
    int stomp;         // 攻撃が地面に当たった（画面を揺らす）
    int down;          // このフレームで完全に倒れた
} GiantEvents;

int giant_load(Giant *g);
void giant_unload(Giant *g);
// 1体目のモデルとクリップで描く2体目の巨人（読み込みは1回）。
void giant_share(Giant *g, const Giant *from);
void giant_place(Giant *g, const Ground *gr, int x, int z, int ref_y, int yaw);
void giant_update(Giant *g, const Ground *gr, const int leon_pos[3], int vulnerable, GiantEvents *ev);

// 弾の光線 p + d*t（0 < t < *t_best）：体に当たれば *t_best を更新して1を返す。
int giant_raycast(const Giant *g, const int p[3], const int d[3], int *t_best);
// power：原作の単位の弾の火力（ハンドガン 100）。
void giant_shot(Giant *g, int power);

// pos にいて yaw を向いたレオンが寄生体を斬れる（ひざをついていて、レオンが背後にいる）。
int giant_cuttable(const Giant *g, const int pos[3]);
void giant_cut(Giant *g);
// ロケットの爆発：倒れて死ぬ。まだ立っていたら1を返す。
int giant_blast(Giant *g);

// (x, z) を彼の体から押し出す。
void giant_push(const Giant *g, int *x, int *z, int r);

void giant_draw(Giant *g, u32 poly_fmt);

#endif
