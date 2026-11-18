// re4zeebo/DsExport.cs が書き出す RDA1 形式のキーフレームアニメーション。
#ifndef ANIM_H__
#define ANIM_H__

#include "model.h"

#define ANIM_MAX 16

typedef struct {
    int count, nodes;
    int frames[ANIM_MAX];
    // アニメーションとノードごとに：移動キー、回転キーと、その始まりの位置。
    const u8 *node_data[ANIM_MAX][MODEL_MAX_NODES];
} AnimSet;

// 行列にする前のノードのローカル変換（ポーズ同士を補間できるように）。
typedef struct {
    int t[3];     // 移動量、f32（アニメーションしないときはバインド時の移動量）
    int r[3];     // オイラー角 X, Y, Z（libnds 単位）、X から順に掛ける
    int has_r;    // 0：バインドの回転のまま（補間するどのポーズでも回らないノード）
} NodeLocal;

// 成功で0を返す。ノード数はモデルの骨格と同じでなければならない。
int anim_load(AnimSet *a, const void *data, size_t size);

// モデル `m` の全ノードの、アニメーション `anim` のフレーム `frame`（f32、長さで
// ループ）でのローカル変換。
void anim_sample(const AnimSet *a, int anim, int frame, const Model *m, NodeLocal out[]);

// 全ノードで dst = dst + (src - dst) * w（w は f32、0..1）。角度は近い方向に回す。
void anim_blend(NodeLocal dst[], const NodeLocal src[], int count, int w);

// 全ノードで dst += (a - b) * w：ポーズ b から a への変化（例：最初のフレームからの反動）を
// 別のポーズの上に足す。角度は近い方向に回す。
void anim_add_delta(NodeLocal dst[], const NodeLocal a[], const NodeLocal b[], int count, int w);
// 同じことを、各関節の b から a への回転を回転として（b^-1 a）、dst の回転の後に掛けて行う。
// オイラー角を足すのでは合わない大きな動き（ショットガンのポンプ）用。
void anim_add_delta_rot(NodeLocal dst[], const NodeLocal a[], const NodeLocal b[], int count, int w);

// ローカル変換からノードのグローバル行列（f32 4x3）を作る。
void anim_compose(const Model *m, const NodeLocal loc[], int out[][12]);

// anim_sample() + anim_compose()。
void anim_pose(const AnimSet *a, int anim, int frame, const Model *m, int out[][12]);

#endif
