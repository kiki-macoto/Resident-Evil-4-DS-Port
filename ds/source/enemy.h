// ガナード：配置、簡単な接近の動き、攻撃（振り、踏み込んでつかむ）、被弾、
// 被弾の反応、倒れる。
#ifndef ENEMY_H__
#define ENEMY_H__

#include "anim.h"
#include "ground.h"
#include "model.h"
#include "objects.h"
#include "stages.h"

#define ENEMY_MAX 12

// ガナードが持つもの（Zeebo 版の村：鎌、手斧、ピッチフォーク、素手もいる）。
// 武器を持つ敵はつかまずに武器を振る。
// 城の教徒はさらに（GM_Data_ChParam::Get_WepMdlId）カテゴリ15のもの（eqp_wep_all #12、
// 振る：ch_jya_w01）、ボウガン（16、#13：撃つ、ch_jya_w02）、杖（18、#15：持つだけで、
// 攻撃は素手のもの）を持つ。
enum { WEAPON_NONE, WEAPON_SICKLE, WEAPON_AXE, WEAPON_FORK, WEAPON_BOARD, WEAPON_XBOW, WEAPON_STAFF, WEAPON_KINDS };

// ENEMY_HIT 以降の状態は攻撃できない。ENEMY_DYING 以降は戦闘から外れている。
enum { ENEMY_IDLE, ENEMY_WALK, ENEMY_SWIPE, ENEMY_LUNGE, ENEMY_GRAB, ENEMY_HIT, ENEMY_DYING, ENEMY_DEAD };

typedef struct {
    int pos[3];       // モデルの原点、f32（pos[1] = 足元 - モデルの bbox_min.y）
    int feet;         // 足元の床の高さ、f32
    int yaw;          // 向き（モデルの +Z）、libnds の角度
    int state;
    int frame;        // 状態のアニメーション内の位置、f32
    int hp;           // 原作の単位（ハンドガンが胴に当たって 100）
    const FoeKind *kind;   // 部位ごとの体力、ダメージ、ひるみ（原作の ch_param の表）
    int flinch;       // 最後によろめいてからの、弾によるひるみの合計（kind->flinch_max でよろめく）
    int attack;       // 進行中の攻撃（enemy.c の attacks[]）
    int enmity;       // 原作の AI の敵意 × 3：ENMITY_MAX で攻撃できる
    int shove;        // 押し戻されている残りフレーム数（レオンが振りほどいた）
    int cooldown;     // 次の攻撃を始められるまでのフレーム数
    int struck;       // 今の振りはもうレオンへの当たりを調べた
    int spotted;      // レオンを一度は見つけた（彼の方へ歩いた）
    int spot_told;    // …それを知らせた（ev.spotted、叫び声のため）
    int active;       // ステージにいる：待機中のものはまだ出ていない、古い死体は消えている
    int dead_frames;  // 死んで横たわっているフレーム数（しばらくすると死体は消える）
    int falling, fall_to, fall_vy;   // 段や屋根から落ちている：fall_to まで、速さ fall_vy（f32）
    int weapon;       // WEAPON_*
    int boss;         // チェーンソー男（ch_cha）：専用のモデルと動き、ずっと強い
    int drop;         // ミッションファイルの指定する落とし物（地点の2バイト目）、-1 = 指定なし
    int thrower;      // 近づかずにその場で投げる（ダイナマイト、燃える石）
    int daze;         // ヘッドショットの後の気絶の残りフレーム数（レオンが蹴れる）、0 = なし
    int stuck;        // ほとんど進まなかった歩きのフレーム数（レオンとの間に壁）
    int detour;       // 回り込みで歩く残りフレーム数、detour_dir の向きへ（f32 単位ベクトル x, z）
    int detour_dir[2];
    int detour_side;  // 前回回り込んだ側（+1 / -1）：次はそちらから試す
    int climb;        // はしごの登りの残りフレーム数（0：なし）、climb_from から climb_to まで（f32 の足元の点）
    int climb_from[3], climb_to[3];
    int pose[MODEL_MAX_NODES][12];   // 最後に計算したポーズ（20Hz のアニメーション速度で更新）
    int pose_dirty;
    int plaga;        // 首からプラーガが伸び、頭が無い（種類の WithPlgFlg）：enemies_set_plaga() 参照
    int plg_clip, plg_frame;   // プラーガのクリップと、その中の位置（f32 のフレーム）
} Enemy;

typedef struct {
    const Model *model;
    const AnimSet *anims;     // 順：待機、歩き、被弾、死亡、振り、踏み込み、つかみ
                              // （re4zeebo -DsAnim ...#6,7,12,21,9,8,10）
    const Ground *ground;
    const Objects *objects;   // 閉じた門や木箱にもふさがれる（NULL 可）
    const Model *weapon_model[WEAPON_KINDS];   // 右手（ノード14）に描く。[0] は未使用
    const AnimSet *weapon_anims[4];            // ch_gna_w00（片手）、ch_gna_w01（ピッチフォーク）、
                                               // 教徒の ch_jya_w01（カテゴリ15）、ch_jya_w02（ボウガン）
    const Model *boss_model;                   // チェーンソー男（骨格はガナードと同じ）
    const AnimSet *boss_anims;                 // ch_cha.mcas
    const Model *plaga_model;                  // eqp_plg_all #0（NULL：読み込んでいない。プラーガは描かない）
    const AnimSet *plaga_anims;                // そのクリップ（eqp_plg_all.mcas #0-4）
    Enemy e[ENEMY_MAX];
    int count;
    int attacker;             // 今攻撃してよい1体、-1 = なし
    int rest;                 // 次の攻撃を始められるまでのフレーム数（どれかの攻撃の後）
    int kills;                // enemies_init() から倒れた敵の数
    int sight;                // この距離以内でレオンに気づく（そして歩いてくる）、f32
    int hide_below;           // 足元がこれより低いものは描かない（f32。0：全部描く）。main.c の回廊を参照
    int watch;                // 2-12面：気づくまでは向いている方を向いて立っている
                              // （近く、前方の見える範囲、撃たれた、近くの敵が気づいた）。
                              // その後は、どこへ行っても追い続ける
    const int *ally;          // アシュリーの位置（f32）か NULL：彼女の方が近ければ彼女を狙う
    int noise;                // 呼び出し側が毎フレーム設定：レオンが走ったか撃った（聞こえるかもしれない）
    int crit_hit;             // enemies_shot() がクリティカルを出した（呼び出し側が消す）
    u32 tick, rng;
} Enemies;

// enemies_update() の間にレオンに起きたこと。-1 = なし。
typedef struct {
    int swiped_by;    // 振りが当たった
    int grabbed_by;   // 踏み込みでつかまれた：敵がつかんでいる（ENEMY_GRAB）
    int bite;         // つかんでいる敵が噛んだ（ホールドの2ループに1回）
    int spotted;      // 敵が初めてレオンを見つけた（彼の方へ歩き始める）
    int attacking;    // 敵が攻撃を始めた
    int swing;        // 振りの腕が伸びた（当たったかどうかに関係なく）
    int ally_hit;     // 振りがアシュリーに当たった
    int thrown;       // 投げる敵が投げ物を放した（番号）、-1 = なし
    int shot;         // ボウガンが矢を放った（番号）、-1 = なし。狙いは shot_at（f32：狙っている相手の足元）
    int shot_at[3];
} EnemyEvents;

// ワールドの点（x, y, z はワールド単位。y で床を選ぶ：地面、通路、屋根）に yaw の向きで敵を
// 置く。全員待機（非アクティブ）で始まる：enemies_activate() で登場させる。
void enemies_init(Enemies *s, const Model *m, const AnimSet *a, const Ground *g, const Objects *o,
                  const int (*xyz)[3], int count, int yaw);

// 敵 i を新品の状態（体力満タン、立ち）でワールドの点 xyz（単位）に、yaw の向きで戻す：
// 時間制のミッションの新しい敵として、死体が消えた枠に入れる。
void enemies_respawn(Enemies *s, int i, const int xyz[3], int yaw);

// 待機中の敵 i をステージに登場させる（その場所に。at（f32 x, z）が NULL でなければそこに）。
void enemies_activate(Enemies *s, int i, const int *at);

// ステージにいてまだ立っている（倒れていない）数 / まだ一度も倒されていない数（待機中も数える）。
int enemies_active_standing(const Enemies *s);
int enemies_left(const Enemies *s);

// 全部の敵を 60Hz の1フレーム分動かす・アニメーションさせる。target はレオンの位置（f32）。
// 攻撃は `vulnerable` が立っている間だけ始まり、当たる。
void enemies_update(Enemies *s, const int target[3], int vulnerable, EnemyEvents *ev);

// つかんでいる敵 i（ENEMY_GRAB）を、`pos` にいる標的のすぐ前に置く：敵は向きを変えず、標的は
// 敵の方を向く（標的の yaw = 敵の yaw + 180度）。
void enemies_hold_at(Enemies *s, int i, const int pos[3], int dist);

// 放す：敵 i は後ろへよろめく（レオンが振りほどいたか、ホールドの時間が切れた）。
void enemies_release(Enemies *s, int i);

// 点 (x, z) を、立っている全部の敵から押し出す（半径 `radius` の体と、敵自身の体）。
// 押し出した数を返す。
int enemies_push(const Enemies *s, int *x, int *z, int radius);

// 全部の敵を描く（レオンと同じく行列スタックでスキニング）。
// eye（f32）に近い max 体まで、近い順。
void enemies_draw(Enemies *s, u32 poly_fmt, const int eye[3], int max);

// 光線 p + d*t（d は単位ベクトル、f32、0 < t < *t_best）に当たる一番近い敵。当たれば
// *t_best を更新して敵の番号を返し、*part に当たった部位を入れる（肩より上は PART_HEAD、
// 腰より下は PART_LEGS、ほかは PART_BODY）。当たらなければ -1。
int enemies_raycast(const Enemies *s, const int p[3], const int d[3], int *t_best, int *part);

// 敵 i の部位 `part` への火力 `power`（原作の値：ハンドガン 100）の弾。原作と同じく：
// 種類のその部位のダメージ率だけ体力が減り、ひるみ率だけひるみの合計に足され、その合計が
// 種類の値に達したときだけよろめく（攻撃が中断される）。よろめいたか倒れたら1を返す。
// crit_rate：武器のクリティカル率（item_weapon_info.bin）。crit_wep：CRIT_*（CRIT_NONE：
// クリティカルなし）、FoeKind 参照。
int enemies_shot(Enemies *s, int i, int power, int part, int crit_rate, int crit_wep);

// 原作の単位のダメージ（蹴り、ナイフ、爆発）：必ず反応するか、倒れる。
void enemies_damage(Enemies *s, int i, int damage);

// 敵 i にミッションのレコードの種類（部位ごとの体力、ダメージ、ひるみ）を与える。
void enemies_set_kind(Enemies *s, int i, const FoeKind *kind);

// 敵 i がレオンに気づいた（見張りモード）：近くのほかの敵も気づく。
void enemies_alert(Enemies *s, int i);
void enemies_daze(Enemies *s, int i);

// 気絶している敵（pos/yaw の前方、蹴りの届く範囲に立っている）、なければ -1。
int enemies_kickable(const Enemies *s, const int pos[3], int yaw);

// レオンの蹴りが当たる：前方の範囲内に立っている敵全員がダメージを受けて吹き飛ぶ。
// 当たった数を返す。
int enemies_kick(Enemies *s, const int pos[3], int yaw, int damage);

// 敵 i を投げる敵にする（Enemy.thrower 参照）。
void enemies_make_thrower(Enemies *s, int i);
// 敵 i を、下の床に関係なく高さ y（単位）に保つ：床の無い壁の上など。
void enemies_perch(Enemies *s, int i, int y);

// 普通の敵の見た目を差し替える（城の教徒はガナードの骨格とクリップの順を使う）：
// モデル、基本のアニメーション、片手武器のセット。
void enemies_set_look(Enemies *s, const Model *m, const AnimSet *a, const AnimSet *one_hand);

// チェーンソー男を枠 i、ワールドの点 xyz（単位）に yaw の向きで置く。彼がステージにいる間は
// モデルを読み込んだままにしておくこと。
void enemies_spawn_boss(Enemies *s, int i, const int xyz[3], int yaw, const Model *m, const AnimSet *a);

// ナイフの一振り：pos/yaw の前方の範囲内に立っている一番近い敵がダメージを受ける
// （ひるむが吹き飛ばない）。その番号か -1 を返す。
int enemies_slash(Enemies *s, const int pos[3], int yaw, int range, int damage);

// 敵に武器のモデルとアニメーションセットを与える（enemies_init() の後に呼ぶ）。
void enemies_arm(Enemies *s, const Model *const models[WEAPON_KINDS], const AnimSet *one_hand, const AnimSet *fork);

// 教徒のほかの武器：WEAPON_BOARD、_XBOW、_STAFF のモデルと w01 / w02 のセット（NULL のものは
// 描かない / 基本のクリップを使う）。
void enemies_arm_cultists(Enemies *s, const Model *board, const Model *xbow, const Model *staff, const AnimSet *w1, const AnimSet *w2);
// 原作の単位（レオンの1200のうち）の打撃のダメージ：敵 e の進行中の攻撃のもの。
int enemies_blow(const Enemy *e);

// 種類にプラーガがある敵に描くプラーガ（NULL：描かない。それでも頭は無い）。
void enemies_set_plaga(Enemies *s, const Model *m, const AnimSet *a);

// まだ立っている敵の数。
int enemies_alive(const Enemies *s);

#endif
