// 1面の仕掛け：村の門（蹴り開ける）、ガナードが出てくる家の扉、通路へのはしご、木箱（壊れる：
// アイテムが出ることも）、爆発する樽、青いメダル。場所は原作のステージのデータから（stg_data_01.bin：
// 扉 / 門の地点、アクションの地点、当たり判定のデータの出口の四角形）。objects.c を参照。
#ifndef OBJECTS_H__
#define OBJECTS_H__

#include <nds.h>

#include "ground.h"
#include "model.h"
#include "sheet.h"

#define OBJECTS_MAX 24

// OBJ_SWING：ミッションファイルの扉（種類01）、描画して、開いてまた閉じる。
// OBJ_BELL：ミッションファイルの鐘（種類09）、その地点から吊るされ、撃つと鳴る。
enum { OBJ_GATE_LEAF, OBJ_DOOR, OBJ_LADDER, OBJ_CRATE, OBJ_BARREL, OBJ_MEDAL, OBJ_SWING, OBJ_BELL, OBJ_HANG };

// 物のそばでレオンができること（構えていないときの A）。
enum { ACT_NONE, ACT_KICK_GATE, ACT_KICK_CRATE, ACT_LADDER_UP, ACT_LADDER_DOWN, ACT_OPEN_DOOR };

typedef struct {
    int kind;
    int hinge[2], end[2];   // 門の扉板 / 扉：閉じているときのちょうつがいと自由な端（f32 の x, z）
    int pos[3];             // 木箱 / 樽 / メダル（吊るし点）/ はしごの根元、f32
    int top;                // はしご：足場の高さ（f32）。門 / 扉：扉板の高さ
    int dir[2];             // はしご：根元から足場への単位ベクトル（f32）
    int yaw;                // はしご：登るときのレオンの向き（libnds の角度）
    int swing;              // 門 / 扉：+1 / -1、扉板の開く向き
    int open;               // 門 / 扉：0（閉）.. 4096（開）
    int opening;
    int pair;               // 門の扉板：もう一方の扉板（両方一緒に開く）、なければ -1
    int alive;              // 木箱 / 樽 / メダル：まだ壊れていない
    int fuse;               // 樽：爆発までのフレーム数、-1 = 火がついていない。揺れる扉：
                            // 開いたままでいるフレーム数。鐘：揺れの残りフレーム数
    int drop;               // ミッションファイルの木箱：そのドロップコード（main.c を参照）、-1 = ランダムなアイテム
    int drawn;              // はしご：もうステージのモデルの一部
    int locked;             // ミッションの扉：後ろに壁があり、決して開かない
    int keyed;              // 鍵をまだ使っていないミッションの扉：閉じたまま、「A: Open」も出さない（そのイベントの A が言う）
} Obj;

#define BOOMS_MAX 4
typedef struct { int pos[3]; int t; } Boom;   // 爆発：爆発してからのフレーム数（-1 = 空き）
typedef struct { int pos[3]; int t; } Puff;   // 壊れていく木箱

typedef struct {
    Obj o[OBJECTS_MAX];
    int count;
    const Model *door, *ladder, *barrel, *crate, *medal;
    const Model *bell, *hang;   // （objects_init() の後に設定）
    const int *leon;        // レオンの位置（f32）：扉は彼の上には閉じない（ゲームが設定）
    const Sheet *boom_fx, *spark_fx;
    const Ground *ground;
    Boom boom[BOOMS_MAX];
    Puff puff[BOOMS_MAX];
    int medals, medals_hit;
    int cold;               // 樽は液体窒素のタンク（12面）：白い破裂
    u32 tick;
} Objects;

// このフレームで起きたこと（位置は f32。何も無ければ -1 / なし）。
typedef struct {
    int exploded;           // このフレームで explode_pos で樽が爆発した
    int explode_pos[3];
    int small;              // …そしてそれは樽でなく吊るされた容器の爆発だった
    int door;               // 揺れる扉が開き / 閉じ始めた（番号）、-1 = なし
} ObjEvents;

void objects_init(Objects *s, const Ground *g, const Model *door, const Model *ladder, const Model *barrel,
                  const Model *crate, const Model *medal, const Sheet *boom_fx, const Sheet *spark_fx);
void objects_reset(Objects *s);   // 門を閉め、全部元の位置に

// ほかのステージ：仕掛けなし、それから樽（タンク）を1つずつ足す。ref_y から見つけた (x, z) の床に
// 立てる。そこに空きか床が無ければ0を返す。
void objects_clear(Objects *s);
int objects_add_barrel(Objects *s, int x, int z, int ref_y);
// ちょうど (x, y, z) に立つ樽、床があってもなくても（7面の城壁の上のもの）。
int objects_add_barrel_at(Objects *s, int x, int y, int z);
// ドロップコード `drop` のアイテムが入った木箱（-1：1面と同じく、ランダムなアイテムが出ることも）。
int objects_add_crate(Objects *s, int x, int z, int ref_y, int drop);
// ちょうつがい (x, z) の扉、yaw を向く（扉のモデルの扉板はその -x に沿って13単位）、ref_y から
// 見つけた床の上。鐘は (x, y, z) から吊るす。
int objects_add_door(Objects *s, int x, int z, int ref_y, int yaw);
int objects_add_bell(Objects *s, int x, int y, int z);
// 根元 (x, z) のはしご（種類02）、yaw を向き、背を足場に向ける（その CLIMB_BACK 後ろの床、根元から
// 40以内の高さ）。そこに足場が無ければ0を返す。
int objects_add_ladder(Objects *s, int x, int z, int ref_y, int yaw);
// 鎖に吊るされた容器（種類05）、その地点は (x, y, z)：撃つと落ち、下の床で爆発する。
int objects_add_hang(Objects *s, int x, int y, int z, int yaw);

// 揺れる扉 i を `from`（f32 の x, z、開ける者）から離れる向きに開く。
void objects_open_door(Objects *s, int i, const int from[3]);
// 扉板が (x, z) から `reach` 以内にある閉じた揺れる扉、なければ -1。
int objects_door_at(const Objects *s, int x, int z, int reach);

// 揺れる扉板、樽の導火線、エフェクトを進める。
void objects_update(Objects *s, ObjEvents *ev);
// pos での爆発の火と煙（ロケットのもの）。
void objects_boom(Objects *s, const int pos[3]);

// 体（x, z、足の高さでの半径）を、閉じた門 / 扉、木箱、樽から押し出す。
void objects_collide(const Objects *s, int *x, int *z, int feet, int radius);

void objects_draw(Objects *s, const int eye[3], u32 poly_fmt);
void objects_draw_fx(Objects *s);   // 爆発、破片（ビルボード。billboard_begin() の後に）

// 光線 p + d*t、t < *t_best で当たる一番近い木箱 / 樽 / メダル。*t_best を更新する。
int objects_raycast(const Objects *s, const int p[3], const int d[3], int *t_best);

// 物 i に射撃か蹴りが当たった。壊れた（木箱 / メダル）か火がついた（樽）なら1を返す。
int objects_hit(Objects *s, int i);

// (pos、足元、向き yaw) でレオンができる動作と、その対象の物。
int objects_action(const Objects *s, const int pos[3], int feet, int yaw, int *which);

// 門（両方の扉板）か扉を開ける、揺れながら。
void objects_open(Objects *s, int i);

// 家の扉（その番号。ガナードがそこから出てくる）。
int objects_door(const Objects *s);

#endif
