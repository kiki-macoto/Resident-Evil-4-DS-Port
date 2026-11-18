// ガナードが落とすアイテム（ハンドガンの弾、緑ハーブ）：原作の光の効果（光の柱 fix_all #7、
// きらめく星 fix_all #0）とアイテムのアイコン（2d_itemic_00）で表示する。
// 手の届く範囲で A を押すと拾い（原作のアクションボタンと同じ）、きらめき（eff_all）が出る。
#ifndef DROPS_H__
#define DROPS_H__

#include <nds.h>

#include "sheet.h"

#define DROPS_MAX 8

enum { DROP_AMMO, DROP_HERB, ITEM_GUN, DROP_TREASURE, DROP_MONEY };   // ITEM_GUN：アイコンだけ（アイテム画面用）

typedef struct {
    int pos[3];    // f32、地面の上
    int type;
    int delay;     // 現れるまでのフレーム数（ガナードがまだ倒れている途中）
    int age;       // 現れてからのフレーム数
    int active;
    int burst;     // 拾ったときのきらめきの残りフレーム数（拾った後）
    int land[3];   // 高い所の宝：撃つと落ちる先（f32、下の床）
    int aloft;     // 撃ち落とすまで、高くて手が届かない
    int value;     // DROP_MONEY、DROP_TREASURE：PTAS。DROP_AMMO：0（多機能弾。拾ったときに数える）
} Drop;

typedef struct {
    Drop d[DROPS_MAX];
    int picked_value;   // drops_update() が最後に返したアイテムの値
    int ammo_gun;       // 多機能弾はこの銃の弾箱で表示（手に持っている銃。ゲーム側が設定）
    const Sheet *star, *beam, *spark, *items;
} Drops;

void drops_init(Drops *s, const Sheet *star, const Sheet *beam, const Sheet *spark, const Sheet *items);
void drops_clear(Drops *s);

// `delay` フレーム後に、地面の点にアイテムを落とす。
void drops_spawn(Drops *s, const int ground[3], int type, int delay);
// 値つきで同じことをする（DROP_MONEY：PTAS）。
void drops_spawn_value(Drops *s, const int ground[3], int type, int delay, int value);

// pos（f32）の宝。落ちる先は land（下の床、壁から離れた所）：高い所のものは撃ち落として
// からでないと拾えない。
void drops_place_treasure(Drops *s, const int pos[3], const int land[3], int value);

// 弾の光線 p + d*t（0 < t < *t_best）：高い所の宝に当たれば *t_best を更新してその番号を、
// でなければ -1 を返す。drops_knock() で床に落とす。
int drops_raycast(const Drops *s, const int p[3], const int d[3], int *t_best);
void drops_knock(Drops *s, int i);

// レオン（pos：x、足元、z、f32）の手が届く一番近いドロップ、なければ -1。
int drops_in_reach(const Drops *s, const int leon[3]);
// 効果を進める。take が立っていれば、レオン（pos、f32）が手の届くドロップを拾う：
// その種類（値は drops.picked_value）か -1 を返す。leon が NULL：何もしない（死んでいる）。
int drops_update(Drops *s, const int leon[3], int take);

// 見えているドロップと拾ったときのきらめきを描く（billboard_begin() の後で）。
void drops_draw(Drops *s);

// アイテムのシート（32 x 24）でのアイコンの位置：x, y。
void drops_icon(int type, int value, int *sx, int *sy);

#endif
