// 投げ物：ダイナマイト（村）と燃える岩（城のカタパルト）。それぞれ投げたときにレオンが立っていた
// 所へ弧を描いて飛び、着地で爆発する。
#ifndef MISSILE_H__
#define MISSILE_H__

#include <nds.h>

#include "ground.h"
#include "sheet.h"

#define MISSILES_MAX 6

typedef struct {
    int pos[3], vel[3];   // f32、1フレームあたり
    int t;                // 飛んでいるフレーム数。-1 = 空き
    int boom;             // 爆発してからのフレーム数。-1 = まだ飛んでいる（か空き）
    int land;             // 落ちてくる高さ（f32）
} Missile;

typedef struct {
    Missile m[MISSILES_MAX];
    const Sheet *fx;
    int fire;             // 城：燃える岩（オレンジ）、それ以外はダイナマイト（火花）
} Missiles;

void missiles_init(Missiles *s, const Sheet *fx, int fire);

// `from`（f32）から1つ投げ、`to`（f32）に着地させる。FLIGHT フレームで弧を描く。
void missiles_throw(Missiles *s, const int from[3], const int to[3]);

// 1フレーム。このフレームで爆発したものの番号（その pos が場所）、なければ -1 を返す。
int missiles_update(Missiles *s, const Ground *g);

void missiles_draw(Missiles *s);   // ビルボード：billboard_begin() の後に呼ぶ

#endif
