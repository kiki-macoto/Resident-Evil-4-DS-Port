#include "enemy.h"

// ganado_anim.bin のアニメーションの順（re4zeebo -DsAnim ...#6,7,12,21,9,8,10）
#define ANIM_IDLE  0
#define ANIM_WALK  1
#define ANIM_HIT   2
#define ANIM_DEATH 3
#define ANIM_SWIPE 4   // #9：両腕を前に振る（届くのは約8単位、フレーム9-12）
#define ANIM_LUNGE 5   // #8：踏み込んでレオンにつかみかかる（腕を伸ばすのはフレーム9-21）
#define ANIM_HOLD  6   // #10：つかんで噛みつく、ループ
static const u8 state_anim[] = { ANIM_IDLE, ANIM_WALK, ANIM_SWIPE, ANIM_LUNGE, ANIM_HOLD,
                                 ANIM_HIT, ANIM_DEATH, ANIM_DEATH };

#define LEGS_TO       floattof32(7.5)    // 足元からこれより低い弾は脚に当たる
#define FAR_STEP      inttof32(100)      // レオンからこれより遠く歩いているときは、半分の頻度で動かす
#define HAND_NODE     14                 // 右手首（骨格はレオンと同じ並び）
// プラーガ（eqp_plg_all #0、WithPlgFlg を持つ種類）：ガナードの頭（ノード19）を描かず、
// 首（ノード18）からプラーガが伸びる（原作の3面の絵のとおり）。
#define PLG_NECK_NODE 18
#define PLG_HEAD_NODE 19
#ifndef PLG_IDLE
#define PLG_IDLE      0                  // そのクリップ、eqp_plg_all.mcas #0-4（plaga_step() 参照）
#define PLG_ATTACK    2
#define PLG_HIT       1
#define PLG_DEATH     4
#endif
#define PLG_TOP       inttof32(26)       // 足元からこの高さまでの弾が当たる：プラーガは背が高い
#define SIGHT         inttof32(120)      // この距離以内でレオンの方へ歩き始める
#define STOP          inttof32(7)        // …そしてこれだけ近づいたら止まる
#define WALK_SPEED    (inttof32(90) / 768)   // 原作の 20Hz の1ティックあたり 90、1/256 単位（ch_param_2 の種類の MoveSpd、docs/NOTES.md）
#define TURN_SPEED    (500 / 6)          // 原作の RotSpd 1ティックあたり 500（1回転 65536）：約55度/秒（ch_param_2。チェーンソー男も同じ）
#define RADIUS        floattof32(2.2)    // 敵同士と弾に対する体の円柱
#define WALL_RADIUS   floattof32(3.2)    // …壁に対して：肩や振った腕が壁に入らないように
#define BODY_RADIUS   inttof32(6)        // 倒れる（後ろ向きに）体と横たわる体：壁からこれだけ離す
#define HEIGHT        inttof32(17)
#define HEAD_FROM     floattof32(14.5)   // 足元からこれより高い弾は頭に当たる
#define STEP_UP       inttof32(3)
#define ANIM_STEP     (inttof32(20) / 60)   // アニメーションは 20フレーム/秒

// 攻撃（原作のアニメーションスクリプトのとおり：ch_param_2.bin、docs/NOTES.md の
// 「Where a blow lands」）。それぞれクリップのフレーム（1秒に20、原作のティック）で動く：
// ガナードはレオンの方を向き（スクリプトのコマンド100）、踏み込み（コマンド4：1ティックあたりの
// ゲーム単位。スクリプトの指定する所では歩きの MoveSpd 90 を足す）、指定のフレームで打撃
// （コマンド103）かつかみ（コマンド106、ホールド）をする。どちらも、床に沿って距離 + レオンの幅
// （MoveCol_R 1000）以内、体が高さの範囲に重なり、角度の範囲内なら当たる。攻撃は、モーション表の
// 条件の距離（その最初の値、移植版の単位）以内で始まる（踏み込みつかみは35度以内も必要）。
typedef struct { u8 from, to; s16 speed; } AtkStep;                // speed < 0：後ろへ
typedef struct { u8 from, to; u16 dist; s16 lo, hi, ang; } AtkHit; // ゲーム単位。ang：1回転 65536
typedef struct {
    u8 start, start_ang;        // start 単位以内（と start_ang 度以内、0：どの角度でも）で始まる
    u8 turn_from, turn_to; s16 turn;   // レオンの方を向く（1ティックの RotSpd。0：ガナード自身の値）
    u8 grab;                    // 当たりは打撃ではなくつかみ（ホールド）
    AtkHit hit;
    u8 steps; AtkStep step[3];
} Attack;
enum { ATK_QUICK_GRAB, ATK_LUNGE, ATK_HATCHET, ATK_FORK, ATK_CULT_GRAB, ATK_CULT_LUNGE, ATK_CULT_SICKLE, ATK_CULT_BOARD, ATK_XBOW };
static const Attack attacks[] = {
    // 素手、近距離（スクリプト41、クリップ #9。モーション7、条件70：12）。
    { 12, 0, 1, 11, 400, 1, { 14, 16, 2500, 3500, 4000, 1000 }, 2, { { 11, 16, 250 }, { 17, 20, 90 } } },
    // 素手、踏み込み（スクリプト42、クリップ #8。モーション8-10、条件12：30、+-6400）。
    { 30, 35, 1, 16, 300, 1, { 14, 30, 2500, 3500, 4000, 1000 }, 3, { { 0, 9, 50 }, { 10, 34, 320 }, { 35, 38, 150 } } },
    // 手斧（カテゴリ13。スクリプト62、ch_gna_w00 #4。モーション80、条件25：12）。
    { 12, 0, 1, 6, 0, 0, { 13, 13, 3200, 1000, 5600, 3200 }, 3, { { 4, 9, 90 }, { 10, 16, 270 }, { 47, 56, -90 } } },
    // ピッチフォーク（カテゴリ14。スクリプト75、ch_gna_w01 #2。モーション94、条件18：22）。
    { 22, 0, 1, 8, 0, 0, { 21, 21, 5120, 2000, 4000, 1500 }, 1, { { 16, 31, 220 } } },
    // 教徒（ch_param_3.bin。MoveSpd は 120）：素手はガナードと同じ（スクリプト41、42。
    // 条件70：12、12：32）。鎌（カテゴリ17。スクリプト65、ch_jya_w00 #4。モーション88、
    // 条件9：16。フレーム20と21の2回の打撃を1つにまとめる：4300、高さ500-5000、-5000..3000）。
    { 12, 0, 1, 11, 500, 1, { 14, 16, 2500, 3500, 4000, 1000 }, 2, { { 11, 16, 300 }, { 17, 20, 120 } } },
    { 32, 35, 1, 16, 400, 1, { 14, 30, 2500, 3500, 4000, 1000 }, 3, { { 0, 9, 80 }, { 10, 34, 400 }, { 35, 38, 180 } } },
    { 16, 0, 1, 12, 0, 0, { 20, 21, 4300, 500, 5000, 5000 }, 1, { { 16, 23, 400 } } },
    // カテゴリ15（eqp_wep_all #12。スクリプト76、ch_jya_w01 #5。モーション59-61、条件12：20）：
    // フレーム1-10で1ティック150、12-29で450踏み込み、20-22で打撃：1800、高さ1000-3500、+-1000。
    { 20, 0, 1, 10, 0, 0, { 20, 22, 1800, 1000, 3500, 2000 }, 2, { { 1, 10, 150 }, { 12, 29, 450 } } },
    // ボウガン（カテゴリ16。スクリプト93-94、ch_jya_w02 #5）：射撃のみで打撃なし（XBOW_* 参照）。
    { 0, 0, 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, 0, { { 0, 0, 0 } } },
};
#define LEON_WIDTH    1000               // レオンの MoveCol_R（ch_param_0）、ゲーム単位
#define GAME_UNITS(v) ((v) * 16)         // ゲーム単位（移植版の 1/256）を f32 に
// 攻撃するタイミング（GM_CtrlSub_ChAi::_Set_UpDateParam）：各敵の敵意は1ティックごとに種類の
// 集中力だけ増え、1000 まで。ただし誰かが攻撃中は先に duty だけ減る。1000 で攻撃できる
// （モーション表の条件8、EnmityMax：999 超）。ここでは3倍で持つ（60Hz の1フレームは1ティックの
// 3分の1）：ガナード（8）は最後の攻撃から6.25秒で攻撃できる。
#define ENMITY_MAX    3000
// 振りほどかれたとき：3000 押し戻される（Main_State_HoldAttack の Set_StaHitBack）、数フレームかけて。
#define RELEASE_PUSH  (inttof32(3000) / 256)
#define SHOVE_FRAMES  10
// ホールド（スクリプト45、クリップ #10）：フレーム6と10で50ずつ噛む。レオンが振りほどくまで
// ループし続ける（Main_State_HoldAttack はそのときだけ放す）。
#define BITE_A        6
#define BITE_B        10

// 武器を持ったガナードは、立ち・歩き・振りに武器のアニメーションセットを使う
// （ch_gna_w00：片手の鎌・手斧、ch_gna_w01：両手で持つピッチフォーク）。被弾と倒れるのは基本セットから。
typedef struct { int set, idle, walk, swing; } WeaponClips;
// （手斧の振りは、スクリプト62のとおり ch_gna_w00 #4：78フレーム、打撃は13）
// 教徒のカテゴリ15（ch_jya_w01：#0 立ち、#1 歩き、#5 振り、スクリプト76）とボウガン
// （ch_jya_w02：#0、#1、#5 射撃。スクリプト84、85、93）。杖は基本のクリップのまま持つ。
static const WeaponClips weapon_clips[WEAPON_KINDS] = {
    { -1, 0, 0, 0 }, { 0, 0, 1, 4 }, { 0, 0, 1, 4 }, { 1, 0, 1, 2 }, { 2, 0, 1, 5 }, { 3, 0, 1, 5 }, { -1, 0, 0, 0 } };
// ボウガンは、見えているレオンが XBOW_RANGE 以内なら、その場から約 XBOW_EVERY フレームごとに
// 撃つ。矢は射撃のクリップ（12フレーム）の XBOW_LOOSE で放たれる。
#define XBOW_RANGE    inttof32(80)
#define XBOW_EVERY    200                // （投げる敵の THROW_EVERY と同じ）
#define XBOW_LOOSE    inttof32(4)
#define XBOW_KEEP     inttof32(25)       // …そしてこれだけ離れている（移植版独自）
#define DAZE_FRAMES   90                 // ヘッドショットでこれだけの間ひるむ（蹴れる）
#define KICK_RANGE    inttof32(10)       // レオンの蹴りは前方にこれだけ届く
#define KICK_PUSH     inttof32(4)
#define THROW_RANGE   inttof32(90)       // 投げる敵は、見えているレオンがこの距離以内なら投げる…
#define THROW_EVERY   200                // …3.3秒ごと（+ 最大1.5秒）

// チェーンソー男：立ち、歩き、チェーンソーの振りは ch_cha.mcas のクリップ。被弾と倒れるのは
// ガナードのもの（骨格が同じ）。ハンドガンはほとんど効かず、村人より速く歩く。
#ifndef BOSS_IDLE
#define BOSS_IDLE     0
#define BOSS_WALK     1
#define BOSS_SWING    4
#endif
#define BOSS_SPEED    (inttof32(130) / 768)  // チェーンソー男の 130（ch_param_4）
#define BOSS_REACH    floattof32(12.5)

static int set_len(const AnimSet *a, int anim)
{
    return inttof32(a->frames[anim] > 0 ? a->frames[anim] : 1);
}

static int anim_len(const Enemies *s, int anim)
{
    return set_len(s->anims, anim);
}

// 敵 e が今の状態で再生するアニメーションセットとクリップ。
static const AnimSet *clip_of(const Enemies *s, const Enemy *e, int *anim)
{
    if (e->boss && e->state <= ENEMY_SWIPE)
    {
        *anim = e->state == ENEMY_IDLE ? BOSS_IDLE : e->state == ENEMY_WALK ? BOSS_WALK : BOSS_SWING;
        return s->boss_anims;
    }
    const WeaponClips *w = &weapon_clips[e->weapon];
    if (e->weapon && w->set >= 0 && s->weapon_anims[w->set] && e->state <= ENEMY_SWIPE)
    {
        *anim = e->state == ENEMY_IDLE ? w->idle : e->state == ENEMY_WALK ? w->walk : w->swing;
        return s->weapon_anims[w->set];
    }
    *anim = state_anim[e->state];
    return s->anims;
}

static int state_len(const Enemies *s, const Enemy *e)
{
    int anim;
    const AnimSet *a = clip_of(s, e, &anim);
    return set_len(a, anim);
}

static u32 rnd(Enemies *s)
{
    s->rng = s->rng * 1664525 + 1013904223;
    return s->rng >> 16;
}

#define CORPSE_FRAMES 300                 // 死体はこれだけ横たわってから消える（ポリゴン数の節約）
#define FALL_MIN      inttof32(6)         // 足元からこれより低い床：そこへ落ちる
#define GRAVITY       floattof32(0.06)    // 1フレームあたり、1フレームあたり

void enemies_init(Enemies *s, const Model *m, const AnimSet *a, const Ground *g, const Objects *o,
                  const int (*xyz)[3], int count, int yaw)
{
    s->model = m;
    s->anims = a;
    s->ground = g;
    s->objects = o;
    s->count = count > ENEMY_MAX ? ENEMY_MAX : count;
    s->rng = 12345;
    for (int i = 0; i < s->count; i++)
    {
        enemies_respawn(s, i, xyz[i], yaw);
        s->e[i].active = 0;
        s->e[i].frame = inttof32(i * 7);   // 互いにタイミングをずらす
        s->e[i].drop = -1;
    }
    s->attacker = -1;
    s->rest = 0;   // （未使用：ENMITY_MAX 参照）
    s->kills = 0;
    s->tick = 0;
    s->sight = SIGHT;
    s->watch = 0;
    s->noise = 0;
    s->ally = NULL;
}

void enemies_perch(Enemies *s, int i, int y)
{
    Enemy *e = &s->e[i];
    e->feet = inttof32(y);
    e->pos[1] = e->feet - s->model->bbox_min[1];
}

// 投げる敵にレオンが見えている：手からレオンの胸までに壁も床も無い（なければ待つ。
// そうしないと、レオンがどこにいてもステージ中の壁を越えて投げ物が飛んでくる）。
static int sees(const Enemies *s, const Enemy *e, const int t[3])
{
    int from[3] = { e->pos[0], e->feet + inttof32(14), e->pos[2] };
    int to[3] = { t[0], t[1] + inttof32(6), t[2] };
    int d[3] = { to[0] - from[0], to[1] - from[1], to[2] - from[2] };
    int len = (int)sqrt64((u64)((s64)d[0] * d[0] + (s64)d[1] * d[1] + (s64)d[2] * d[2]));
    if (len <= 0)
        return 1;
    for (int k = 0; k < 3; k++)
        d[k] = divf32(d[k], len);
    int hit[3];
    return !ground_raycast(s->ground, from, d, len - inttof32(3), hit, RAY_SHOT);
}

void enemies_make_thrower(Enemies *s, int i)
{
    s->e[i].thrower = 1;
    s->e[i].weapon = WEAPON_NONE;   // （投げ物を手に持っている）
    s->e[i].cooldown = 120 + i * 30;
}

void enemies_set_look(Enemies *s, const Model *m, const AnimSet *a, const AnimSet *one_hand)
{
    s->model = m;
    s->anims = a;
    s->weapon_anims[0] = one_hand;
    for (int i = 0; i < s->count; i++)
        s->e[i].pose_dirty = 1;
}

void enemies_spawn_boss(Enemies *s, int i, const int xyz[3], int yaw, const Model *m, const AnimSet *a)
{
    s->boss_model = m;
    s->boss_anims = a;
    enemies_respawn(s, i, xyz, yaw);
    Enemy *e = &s->e[i];
    e->boss = 1;
    e->weapon = WEAPON_NONE;   // チェーンソーはモデルの一部
    enemies_set_kind(s, i, &foe_kind_chainsaw[0]);   // （1面のレコード：バリエーション0）
    e->pose_dirty = 1;
}

static int is_cultist(const Enemy *e);

void enemies_arm_cultists(Enemies *s,const Model *board, const Model *xbow, const Model *staff, const AnimSet *w1, const AnimSet *w2)
{
    s->weapon_model[WEAPON_BOARD] = board;
    s->weapon_model[WEAPON_XBOW] = xbow;
    s->weapon_model[WEAPON_STAFF] = staff;
    s->weapon_anims[2] = w1;
    s->weapon_anims[3] = w2;
    for (int i = 0; i < s->count; i++)
        s->e[i].pose_dirty = 1;
}

// 打撃のダメージ（ch_param_2 / _3 のスクリプト、攻撃コマンド103の値6）：手斧とピッチフォーク
// 380、鎌 800（スクリプト65）、カテゴリ15 480（スクリプト76）。ボウガンの矢：スクリプトに
// ダメージは無かった（射撃のコマンド105に値が無い）：移植版の 240。
int enemies_blow(const Enemy *e)
{
    if (e->attack == ATK_CULT_BOARD)
        return 480;
    if (e->attack == ATK_XBOW)
        return 240;   // （移植版の投げるダイナマイトや石と同じ：レオンの100のうち20）
    return is_cultist(e) ? 800 : 380;
}

void enemies_arm(Enemies *s, const Model *const models[WEAPON_KINDS], const AnimSet *one_hand, const AnimSet *fork)
{
    for (int k = 0; k < WEAPON_STAFF + 1 && k < WEAPON_KINDS; k++)
        if (k <= WEAPON_FORK)
            s->weapon_model[k] = models[k];
    s->weapon_anims[0] = one_hand;
    s->weapon_anims[1] = fork;
}

void enemies_respawn(Enemies *s, int i, const int xyz[3], int yaw)
{
    Enemy *e = &s->e[i];
    e->pos[0] = inttof32(xyz[0]);
    e->pos[2] = inttof32(xyz[2]);
    e->feet = 0;
    // 出現する高さの床（地面、通路、屋根）。
    ground_height(s->ground, e->pos[0], e->pos[2], inttof32(xyz[1]), STEP_UP, &e->feet);
    e->pos[1] = e->feet - s->model->bbox_min[1];
    e->active = 1;
    e->dead_frames = 0;
    e->falling = 0;
    e->yaw = yaw;
    e->state = ENEMY_IDLE;
    e->frame = 0;
    e->boss = 0;
    e->thrower = 0;
    enemies_set_kind(s, i, &foe_kind_ganado[0]);   // （ミッションのレコードが指定するまで）
    e->attack = 0;
    e->enmity = 0;
    e->shove = 0;
    e->cooldown = 0;
    e->struck = 0;
    e->spotted = 0;
    e->spot_told = 0;
    e->daze = 0;
    e->boss = 0;
    e->thrower = 0;
    e->stuck = e->detour = 0;
    e->climb = 0;
    e->detour_side = (i & 1) ? 1 : -1;
    e->pose_dirty = 1;
}

static void set_state(Enemies *s, int i, int state)
{
    if (state == ENEMY_DYING && s->e[i].state < ENEMY_DYING)
        s->kills++;
    Enemy *e = &s->e[i];
    if (state == ENEMY_DYING && !e->thrower)
    {
        // 体はその長さだけ後ろへ倒れる：先に壁から離して、壁に半分めり込まず床に横たわるようにする。
        ground_collide_n(s->ground, &e->pos[0], &e->pos[2], e->feet + STEP_UP, e->feet + HEIGHT, BODY_RADIUS, 2);
        ground_height(s->ground, e->pos[0], e->pos[2], e->feet + STEP_UP, STEP_UP * 2, &e->feet);
        e->pos[1] = e->feet - s->model->bbox_min[1];
    }
    if (s->attacker == i && state != ENEMY_SWIPE && state != ENEMY_LUNGE && state != ENEMY_GRAB)
        s->attacker = -1;   // 攻撃が終わった：（敵意がたまれば）別の敵が攻撃してよい
    if (state != ENEMY_HIT)
        e->daze = 0;
    e->state = state;
    e->frame = 0;
    e->struck = 0;
    e->pose_dirty = 1;
}

// アニメーションは 20フレーム/秒なので、ポーズの計算は 60Hz の3フレームに1回でよい。
// 負荷を分散するため敵ごとに順番（i + tick）。状態が変わったときはすぐ計算する。
static void update_pose(Enemies *s, Enemy *e)
{
    int anim;
    const AnimSet *a = clip_of(s, e, &anim);
    int frame = e->frame;
    if (e->state == ENEMY_DEAD)
        frame = anim_len(s, ANIM_DEATH) - 1;   // 最後のフレーム（終わりを過ぎるとループしてしまう）
    anim_pose(a, anim, frame, s->model, e->pose);
    if (e->plaga)   // 頭なし：そのノードの回転の行を0にして、頭の頂点を1点に縮める
        for (int k = 0; k < 9; k++)
            e->pose[PLG_HEAD_NODE][k] = 0;
    e->pose_dirty = 0;
}

// プラーガのクリップ（eqp_plg_all.mcas #0-4）を体の状態で選ぶ：立ち・歩きでは揺れ、攻撃中は
// 打ちつけ、被弾したらのたうち、倒れたらぐったりする。どれも 20Hz のアニメーション速度で、
// 揺れと打ちつけはループする。
static void plaga_step(Enemies *s, Enemy *e)
{
    int clip = e->state == ENEMY_SWIPE || e->state == ENEMY_LUNGE || e->state == ENEMY_GRAB ? PLG_ATTACK
               : e->state == ENEMY_HIT ? PLG_HIT : e->state >= ENEMY_DYING ? PLG_DEATH : PLG_IDLE;
#ifdef TEST_PLG_CLIP   // テスト用ビルド：常にこのクリップ
    clip = TEST_PLG_CLIP;
#endif
    if (clip != e->plg_clip)
    {
        e->plg_clip = clip;
        e->plg_frame = 0;
    }
    if (!s->plaga_anims)
        return;
    int len = inttof32(s->plaga_anims->frames[clip]);
    e->plg_frame += ANIM_STEP;
    if (e->plg_frame >= len)
        e->plg_frame = clip == PLG_DEATH ? len - inttof32(1) : e->plg_frame - len;
}

void enemies_set_plaga(Enemies *s, const Model *m, const AnimSet *a)
{
    s->plaga_model = m;
    s->plaga_anims = m && a && a->nodes == m->node_count && a->count > PLG_DEATH ? a : NULL;
    if (!s->plaga_anims)
        s->plaga_model = NULL;
}

static void move(Enemies *s, Enemy *e, int dx, int dz)
{
    e->pos[0] += dx;
    e->pos[2] += dz;
    // （2回：隅では、片方の壁の押し出しがもう片方に入れてしまうことがある）
    ground_collide_n(s->ground, &e->pos[0], &e->pos[2], e->feet + STEP_UP, e->feet + HEIGHT, WALL_RADIUS, 2);
    if (s->objects)
        objects_collide(s->objects, &e->pos[0], &e->pos[2], e->feet, RADIUS);
    if (e->falling)
        return;
    int floor = e->feet;
    ground_height(s->ground, e->pos[0], e->pos[2], e->feet, STEP_UP, &floor);
    if (floor < e->feet - FALL_MIN)
    {
        // 段や屋根から歩いて外れた：落ちる（enemies_update() 参照）。
        e->falling = 1;
        e->fall_to = floor;
        e->fall_vy = 0;
        return;
    }
    e->feet = floor;
    e->pos[1] = e->feet - s->model->bbox_min[1];
}

// 壁を回り込む。レオンへまっすぐ歩いていて、道を壁がふさいでいる敵は、踏み出したぶん押し戻され、
// その場で足踏みしていた（レオンが近づけば攻撃はできた）。STUCK_FRAMES そうしていると、レオンへの
// 線から45度ずつずらした向きを、近い順・前回の回り込みと同じ側から試す。壁に向かって（段から落ちない
// ように）試しの一歩を出し、最初に抜けられた向きへ DETOUR_FRAMES 歩く。また引っかかれば選び直す
// （そうして建物を回り込んでいく）。
#define STUCK_FRAMES  20
#define DETOUR_FRAMES 70
#define PROBE         inttof32(8)
// はしご（ステージの OBJ_LADDER の物）：レオンが別の階にいるとき、彼を狙っている敵は、自分の階と
// レオンの階をつなぐ一番近いはしごへ歩いて登る（原作の AI と同じ：GM_CtrlSub_ChAi::Check_Ladder）。
// はしごの足元はその点、上はそこから LADDER_BACK 奥の足場の上。登りは LADDER_FRAMES かかる
// （移植版独自：ここではガナードに登りのクリップが無く、歩きのまま上がる）。
#define LADDER_BACK   inttof32(16)
#define LADDER_NEAR   inttof32(3)       // 足元（か上）に着いた：登り始める
#define LADDER_SEEK   inttof32(120)     // これより遠いはしごは無視する
#define LADDER_FRAMES 75
#define LADDER_LEVEL  inttof32(10)      // これだけ離れた床は別の階とみなす
static int ladder_for(const Enemies *s, const Enemy *e, int up, int at[3], int other[3])
{
    if (!s->objects)
        return 0;
    int best = -1;
    s64 best_d = (s64)LADDER_SEEK * LADDER_SEEK;
    for (int i = 0; i < s->objects->count; i++)
    {
        const Obj *o = &s->objects->o[i];
        if (o->kind != OBJ_LADDER)
            continue;
        int foot[3] = { o->pos[0], o->pos[1], o->pos[2] };
        int top[3] = { o->pos[0] + mulf32(o->dir[0], LADDER_BACK), o->top, o->pos[2] + mulf32(o->dir[1], LADDER_BACK) };
        const int *from = up ? foot : top, *to = up ? top : foot;
        if (e->feet - from[1] > LADDER_LEVEL || from[1] - e->feet > LADDER_LEVEL)
            continue;   // （この敵の階に無い）
        s64 dx = from[0] - e->pos[0], dz = from[2] - e->pos[2], d = dx * dx + dz * dz;
        if (d < best_d)
        {
            best_d = d;
            best = i;
            for (int k = 0; k < 3; k++)
            {
                at[k] = from[k];
                other[k] = to[k];
            }
        }
    }
    return best >= 0;
}

static int pick_detour(Enemies *s, Enemy *e, int dx, int dz, int dist)
{
    if (dist <= 0)
        return 0;
    int ux = divf32(dx, dist), uz = divf32(dz, dist);
    static const int turn[] = { 45, 90, 135, 180 };
    for (int k = 0; k < 4; k++)
        for (int j = 0; j < 2; j++)
        {
            int side = j == 0 ? e->detour_side : -e->detour_side;
            if (turn[k] == 180 && j)
                break;
            int a = degreesToAngle(turn[k]) * side;
            int ca = cosLerp(a), sa = sinLerp(a);
            // （yaw と同じだけ回した u：f(yaw + a) = (ux cos a + uz sin a, uz cos a - ux sin a)）
            int vx = mulf32(ux, ca) + mulf32(uz, sa), vz = mulf32(uz, ca) - mulf32(ux, sa);
            int x = e->pos[0] + mulf32(vx, PROBE), z = e->pos[2] + mulf32(vz, PROBE);
            ground_collide_n(s->ground, &x, &z, e->feet + STEP_UP, e->feet + HEIGHT, WALL_RADIUS, 2);
            int got = mulf32(x - e->pos[0], vx) + mulf32(z - e->pos[2], vz);
            if (got < PROBE * 3 / 4)
                continue;
            int floor = e->feet;
            if (ground_height(s->ground, x, z, e->feet, STEP_UP, &floor) && floor < e->feet - FALL_MIN)
                continue;   // （戻ってこられない段から落ちない）
            e->detour_dir[0] = vx;
            e->detour_dir[1] = vz;
            e->detour_side = side;
            e->detour = DETOUR_FRAMES;
            return 1;
        }
    e->detour_side = -e->detour_side;   // 囲まれた：次は反対側から
    return 0;
}

void enemies_activate(Enemies *s, int i, const int *at)
{
    Enemy *e = &s->e[i];
    if (at)
    {
        e->pos[0] = at[0];
        e->pos[2] = at[1];
        ground_height(s->ground, e->pos[0], e->pos[2], e->feet, STEP_UP, &e->feet);
        e->pos[1] = e->feet - s->model->bbox_min[1];
    }
    e->active = 1;
    e->pose_dirty = 1;
}

int enemies_active_standing(const Enemies *s)
{
    int n = 0;
    for (int i = 0; i < s->count; i++)
        n += s->e[i].active && s->e[i].state < ENEMY_DYING;
    return n;
}

int enemies_left(const Enemies *s)
{
    int n = 0;
    for (int i = 0; i < s->count; i++)
        n += s->e[i].state < ENEMY_DYING;
    return n;
}

// 見張っている敵は、原作の AI と同じように気づく（GM_CtrlSub_ChAi::_Get_SearchParam と探索の
// 条件）：床に沿ったレオンとの距離から両方の体の幅と300（ゲーム単位）を引いたもの、向きからの
// 角度（1回転 65536）。見る：視界の距離と角度以内（ガナード：63760、+-1280、前方の狭い7度。
// _SearchCond_Look）。聞く：レオンが走るか撃つ間、探索の距離と角度以内（17920、+-28550。
// _SearchCond_Area_Run、_Area_Shot）。触れる：すき間がまったく無い（_SearchCond_Touch）。
// 撃たれても気づく（_SearchCond_Damage）。
static int within_angle(int dist, s64 dot, int rot)
{
    if (rot >= 32767)
        return 1;
    return dot >= (((s64)dist * cosLerp(rot / 2)) >> 12);   // （|角度| <= rot：libnds の半回転は 32768）
}
static int notices(const Enemies *s, const Enemy *e, int dist, s64 dot)
{
    const FoeKind *k = e->kind;
    int gap = dist - GAME_UNITS(k->width + LEON_WIDTH + 300);
    if (gap <= 0)
        return 1;
    if (k->look_dis > 0 && gap <= GAME_UNITS(k->look_dis) && within_angle(dist, dot, k->look_rot))
        return 1;
    return s->noise && gap <= GAME_UNITS(k->search_dis) && within_angle(dist, dot, k->search_rot);
}

static int is_cultist(const Enemy *e)
{
    return e->kind >= foe_kind_cultist && e->kind < foe_kind_cultist + foe_kind_cultist_count;
}

// モーション表が与える攻撃：武器があればその武器のもの。素手なら距離以内で近距離のホールド、
// でなければ踏み込みつかみ。
static int pick_attack(const Enemy *e, int dist, int on_ally)
{
    if (is_cultist(e))
    {
        if (e->weapon == WEAPON_XBOW)
            return -1;   // （撃つ：XBOW_* 参照）
        if (e->weapon == WEAPON_BOARD)
            return ATK_CULT_BOARD;
        return e->weapon && e->weapon != WEAPON_STAFF ? ATK_CULT_SICKLE
               : dist < inttof32(attacks[ATK_CULT_GRAB].start) || on_ally ? ATK_CULT_GRAB : ATK_CULT_LUNGE;
    }
    if (e->weapon == WEAPON_FORK)
        return ATK_FORK;
    if (e->weapon)
        return ATK_HATCHET;
    return dist < inttof32(attacks[ATK_QUICK_GRAB].start) || on_ally ? ATK_QUICK_GRAB : ATK_LUNGE;
}

// 標的 t（dist 離れている。cross/dot：dist × 向きからの角度の sin/cos）が攻撃の範囲にいる：
// 床に沿って距離 + 標的の幅以内、体（高さ4800、レオンの MoveCol_H）が高さの範囲に重なり、
// 左右の角度以内（幅も含めて）。
static int in_area(const Enemy *e, const int t[3], int dist, s64 cross, s64 dot, const AtkHit *h)
{
    if (dist >= GAME_UNITS(h->dist + LEON_WIDTH))
        return 0;
    int dy = t[1] - e->pos[1];
    if (GAME_UNITS(h->lo) >= dy + GAME_UNITS(4800) || GAME_UNITS(h->hi) <= dy)
        return 0;
    if (h->ang < 16384 && dot <= 0)
        return 0;
    s64 lim = (((s64)dist * sinLerp(h->ang >= 16384 ? 8192 : h->ang / 2)) >> 12) + GAME_UNITS(LEON_WIDTH);
    return cross < lim && -cross < lim;
}

void enemies_update(Enemies *s, const int target[3], int vulnerable, EnemyEvents *ev)
{
    ev->swiped_by = ev->grabbed_by = ev->bite = ev->spotted = ev->attacking = ev->swing = ev->ally_hit = ev->thrown = ev->shot = -1;
    if (s->rest > 0)
        s->rest--;
    for (int i = 0; i < s->count; i++)
    {
        Enemy *e = &s->e[i];
        if (!e->active)
            continue;
        if (e->plaga)
            plaga_step(s, e);
        if (e->state == ENEMY_DEAD)
        {
            if (++e->dead_frames >= CORPSE_FRAMES)
                e->active = 0;   // 死体が消える
            continue;
        }
        if (e->cooldown > 0)
            e->cooldown--;   // （投げる敵）
        if (s->attacker >= 0 && (e->enmity -= e->kind->duty) < 0)
            e->enmity = 0;
        if ((e->enmity += e->kind->conc) > ENMITY_MAX)
            e->enmity = ENMITY_MAX;
        if (e->falling)
        {
            // 落下中：下の床まで重力で落ち、その間は向きを変えない。
            e->fall_vy += GRAVITY;
            e->feet -= e->fall_vy;
            if (e->feet <= e->fall_to)
            {
                e->feet = e->fall_to;
                e->falling = 0;
            }
            e->pos[1] = e->feet - s->model->bbox_min[1];
        }

        // 狙う相手：レオン。アシュリー（s->ally）の方が近ければ彼女。
        int dx = target[0] - e->pos[0], dz = target[2] - e->pos[2];
        int dist = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));   // f32（64ビット：遠く離れると32ビットではあふれる）
        const int *t = target;
        int on_ally = 0;
        if (s->ally)
        {
            int ax = s->ally[0] - e->pos[0], az = s->ally[2] - e->pos[2];
            int ad = (int)sqrt64((u64)((s64)ax * ax + (s64)az * az));
            if (ad < dist)
            {
                dx = ax;
                dz = az;
                dist = ad;
                t = s->ally;
                on_ally = 1;
            }
        }
        // はしごの上：上り下りしてから、また追いかける。
        if (e->climb > 0)
        {
            int k = LADDER_FRAMES - e->climb + 1;
            e->pos[0] = e->climb_from[0] + (e->climb_to[0] - e->climb_from[0]) * k / LADDER_FRAMES;
            e->pos[2] = e->climb_from[2] + (e->climb_to[2] - e->climb_from[2]) * k / LADDER_FRAMES;
            e->feet = e->climb_from[1] + (e->climb_to[1] - e->climb_from[1]) * k / LADDER_FRAMES;
            e->pos[1] = e->feet - s->model->bbox_min[1];
            e->frame += ANIM_STEP;
            e->climb--;
            e->pose_dirty = 1;
            continue;
        }
        // レオンが別の階：間のはしごへ向かい、着いたら登る。
        if (e->spotted && !e->thrower && !e->boss && !e->falling && (e->state == ENEMY_IDLE || e->state == ENEMY_WALK))
        {
            int tlevel = t[1] - e->pos[1], at[3], other[3];
            if ((tlevel > LADDER_LEVEL || tlevel < -LADDER_LEVEL) && ladder_for(s, e, tlevel > 0, at, other))
            {
                int lx = at[0] - e->pos[0], lz = at[2] - e->pos[2];
                int ld = (int)sqrt64((u64)((s64)lx * lx + (s64)lz * lz));
                if (ld < LADDER_NEAR)
                {
                    for (int c = 0; c < 3; c++)
                    {
                        e->climb_from[c] = c == 1 ? e->feet : e->pos[c];
                        e->climb_to[c] = other[c];
                    }
                    e->climb = LADDER_FRAMES;
                    e->detour = e->stuck = 0;
                    if (e->state != ENEMY_WALK)
                        set_state(s, i, ENEMY_WALK);
                    continue;
                }
                dx = lx;   // （歩いて向かうのははしご）
                dz = lz;
                dist = ld > STOP + inttof32(1) ? ld : STOP + inttof32(1);
            }
        }
        // 向き f = (sin yaw, cos yaw)。yaw を増やすと f は (cos yaw, -sin yaw) の方へ回る。
        // cross(f, to) と dot(f, to) は、dist × レオンへの角度の sin/cos。
        int fx = sinLerp(e->yaw), fz = cosLerp(e->yaw);
        s64 cross = ((s64)fx * dz - (s64)fz * dx) >> 12;
        s64 dot = ((s64)fx * dx + (s64)fz * dz) >> 12;
        // レオンが手の届く範囲、前方（約40度以内）、同じ高さ（この敵が下にいて相手が通路の上、
        // または落下中、ではない）にいる。
        int level = t[1] - e->pos[1];
        int same_level = !e->falling && level < inttof32(8) && level > -inttof32(8);
        // （チェーンソー男は移植版独自の間合いと振りのまま。ほかは attacks[] 参照）
        int in_reach = e->boss && dist < BOSS_REACH && dot * 4 > (s64)dist * 3 && same_level;

        if (e->thrower && e->state <= ENEMY_SWIPE)
        {
            // 投げる敵：その場からレオンの方を向き、時々投げる。投げ物は振りの途中で手を離れる。
            if (dist > 0 && (cross < -(s64)dist / 28 || (dot < 0 && cross <= 0)))
                e->yaw += TURN_SPEED;
            else if (dist > 0 && (cross > (s64)dist / 28 || dot < 0))
                e->yaw -= TURN_SPEED;
            e->frame += ANIM_STEP;
            if (e->state == ENEMY_SWIPE)
            {
                int len = state_len(s, e);
                if (!e->struck && e->frame >= len / 2)
                {
                    e->struck = 1;
                    ev->thrown = i;
                }
                if (e->frame >= len)
                    set_state(s, i, ENEMY_IDLE);
            }
            else if (e->cooldown == 0 && dist < THROW_RANGE && dot > 0 && sees(s, e, t))   // （爆発は外れることがある：かまわず投げる）
            {
                set_state(s, i, ENEMY_SWIPE);
                e->cooldown = THROW_EVERY + rnd(s) % 90;
            }
        }
        else if (e->state == ENEMY_IDLE || e->state == ENEMY_WALK)
        {
            int chase = dist < s->sight;
            if (s->watch)
            {
                // 見張り中：原作どおりに気づく（notices()）。気づいたら見失わない。
                if (!e->spotted && notices(s, e, dist, dot))
                    enemies_alert(s, i);
                chase = e->spotted;
            }
            // （ボウガンは、レオンが見えている間は XBOW_KEEP 離れて止まり、そこから撃つ）
            int stop = e->weapon == WEAPON_XBOW && dist < XBOW_KEEP + inttof32(2) && sees(s, e, t) ? XBOW_KEEP : STOP;
            int want = chase && dist > stop ? ENEMY_WALK : ENEMY_IDLE;
            if (want != e->state)
                set_state(s, i, want);
            if (want == ENEMY_WALK && !e->spotted)
                e->spotted = 1;
            if (e->spotted && !e->spot_told)
            {
                e->spot_told = 1;
                ev->spotted = i;
            }
            if (e->detour && (e->state != ENEMY_WALK || !chase))
                e->detour = e->stuck = 0;
            if (e->detour)
            {
                // 壁を回り込む：回り込む向きへすばやく向く（その間もそちらへ歩く）。
                int dc = (int)(((s64)fx * e->detour_dir[1] - (s64)fz * e->detour_dir[0]) >> 12);
                int dd = (int)(((s64)fx * e->detour_dir[0] + (s64)fz * e->detour_dir[1]) >> 12);
                if (dc < -inttof32(1) / 16 || (dd < 0 && dc <= 0))
                    e->yaw += TURN_SPEED * 3;
                else if (dc > inttof32(1) / 16 || dd < 0)
                    e->yaw -= TURN_SPEED * 3;
            }
            else if (chase && dist > 0)
            {
                // レオンの方を向く（約2度以上ずれているか、後ろにいる間）。
                if (cross < -(s64)dist / 28 || (dot < 0 && cross <= 0))
                    e->yaw += TURN_SPEED;
                else if (cross > (s64)dist / 28 || dot < 0)
                    e->yaw -= TURN_SPEED;
            }
            // 近く、ほぼ正面（約15度以内）で、誰も攻撃していない：踏み込みつかみか振りで襲いかかる。
            int atk = e->boss ? -1 : pick_attack(e, dist, on_ally);
            int go = 0;
            if (vulnerable && same_level && s->attacker < 0 && e->enmity >= ENMITY_MAX && dot > 0)
            {
                if (e->boss)
                    go = dist < BOSS_REACH - floattof32(0.5) && cross * 4 < dist && -cross * 4 < dist;
                else if (atk >= 0)
                {
                    const Attack *a = &attacks[atk];
                    go = dist < inttof32(a->start);
                    if (go && a->start_ang)   // （角度以内：|cross| < dist * sin）
                    {
                        s64 lim = ((s64)dist * sinLerp(degreesToAngle(a->start_ang))) >> 12;
                        go = cross < lim && -cross < lim;
                    }
                }
            }
            // ボウガン：レオンが見えていて、射程内で、ほぼ正面：止まって撃つ。
            int shoot = !go && e->weapon == WEAPON_XBOW && e->spotted && e->cooldown == 0
                        && dist < XBOW_RANGE && dot > 0 && cross * 3 < dist && -cross * 3 < dist && sees(s, e, t);
            if (shoot)
            {
                e->attack = ATK_XBOW;
                set_state(s, i, ENEMY_SWIPE);
                e->cooldown = XBOW_EVERY + rnd(s) % 90;
                ev->attacking = i;
            }
            else if (go)
            {
                // 武器を持つ敵は振る。素手はつかみにいくか、踏み込む。
                e->attack = atk;
                set_state(s, i, atk == ATK_LUNGE || atk == ATK_CULT_LUNGE ? ENEMY_LUNGE : ENEMY_SWIPE);
                s->attacker = i;
                ev->attacking = i;
            }
            else
            {
                if (e->state == ENEMY_WALK)
                {
                    // 遠く（カメラに映らない）：1フレームおきに2倍の歩幅。
                    int sp = e->boss ? BOSS_SPEED : WALK_SPEED, far = dist > FAR_STEP;
                    if (!far || ((s->tick + i) & 1))
                    {
                        int step = sp << far, mx = e->detour ? e->detour_dir[0] : fx, mz = e->detour ? e->detour_dir[1] : fz;
                        int x0 = e->pos[0], z0 = e->pos[2];
                        move(s, e, mulf32(mx, step), mulf32(mz, step));
                        // ほとんど進んでいない（壁か人混みに押し戻された）：数える。
                        int got = mulf32(e->pos[0] - x0, mx) + mulf32(e->pos[2] - z0, mz);
                        if (e->falling || got * 3 >= step)
                            e->stuck = e->stuck > 2 ? e->stuck - 2 : 0;
                        else if (++e->stuck >= STUCK_FRAMES >> far)
                        {
                            e->stuck = 0;
#ifndef TEST_NO_DETOUR   // テスト用ビルド：比べるための、昔のまっすぐ歩くだけの動き
                            pick_detour(s, e, dx, dz, dist);
#endif
                        }
                    }
                    if (e->detour)
                        e->detour--;
                }
                e->frame += ANIM_STEP;
            }
        }
        else if (e->state == ENEMY_SWIPE && e->attack == ATK_XBOW)
        {
            // ボウガンの射撃：レオンの方を向いたまま、クリップの早い所で矢を放つ。
            e->frame += ANIM_STEP;
            if (dist > 0 && (cross < -(s64)dist / 28 || (dot < 0 && cross <= 0)))
                e->yaw += TURN_SPEED;
            else if (dist > 0 && (cross > (s64)dist / 28 || dot < 0))
                e->yaw -= TURN_SPEED;
            if (!e->struck && e->frame >= XBOW_LOOSE)
            {
                e->struck = 1;
                ev->shot = i;
                ev->shot_at[0] = t[0];
                ev->shot_at[1] = t[1];
                ev->shot_at[2] = t[2];
            }
            if (e->frame >= state_len(s, e))
                set_state(s, i, ENEMY_IDLE);
        }
        else if ((e->state == ENEMY_SWIPE || e->state == ENEMY_LUNGE) && e->boss)
        {
            // チェーンソー男（移植版独自）：クリップの中ほどで刃が下りるときに1回判定する。
            e->frame += ANIM_STEP;
            int len = state_len(s, e);
            if (!e->struck && e->frame >= len * 2 / 5 && e->frame < len * 3 / 5)
            {
                e->struck = 1;
                ev->swing = i;
                if (in_reach && on_ally)
                    ev->ally_hit = i;
                else if (vulnerable && in_reach)
                    ev->swiped_by = i;
            }
            if (e->frame >= len)
                set_state(s, i, ENEMY_IDLE);
        }
        else if (e->state == ENEMY_SWIPE || e->state == ENEMY_LUNGE)
        {
            // 攻撃のスクリプトを、20Hz の1ティックずつ進める（attacks[] 参照）。
            e->frame += ANIM_STEP;
            const Attack *a = &attacks[e->attack];
            int tick = e->frame >> 12;
            if (tick >= a->turn_from && tick <= a->turn_to && dist > 0)
            {
                int rate = (a->turn ? a->turn : is_cultist(e) ? 550 : 500) / 6;   // 1ティックの RotSpd を1フレーム分に
                if (cross < -(s64)dist / 28 || (dot < 0 && cross <= 0))
                    e->yaw += rate;
                else if (cross > (s64)dist / 28 || dot < 0)
                    e->yaw -= rate;
            }
            for (int k = 0; k < a->steps; k++)
                if (tick >= a->step[k].from && tick <= a->step[k].to)
                {
                    int sp = GAME_UNITS(a->step[k].speed) / 3;   // 1ティックの歩幅を3フレームに分ける
                    move(s, e, mulf32(fx, sp), mulf32(fz, sp));
                }
            if (tick >= a->hit.from && tick <= a->hit.to && !(e->struck & 2))
            {
                if (!a->grab && !(e->struck & 1))
                {
                    e->struck |= 1;
                    ev->swing = i;
                }
                if (same_level && in_area(e, t, dist, cross, dot, &a->hit))
                {
                    e->struck |= 2;
                    if (a->grab)
                    {
                        if (vulnerable && !on_ally)
                        {
                            set_state(s, i, ENEMY_GRAB);
                            ev->grabbed_by = i;
                        }
                    }
                    else if (on_ally)
                        ev->ally_hit = i;
                    else if (vulnerable)
                        ev->swiped_by = i;
                }
            }
            if ((e->state == ENEMY_SWIPE || e->state == ENEMY_LUNGE) && e->frame >= state_len(s, e))
                set_state(s, i, ENEMY_IDLE);
        }
        else if (e->state == ENEMY_GRAB)
        {
            // 放されるまでつかみ続け、ホールドの各ループの2つのフレームで噛む。
            int was = e->frame >> 12;
            e->frame += ANIM_STEP;
            int now = e->frame >> 12;
            if (now != was && (now == BITE_A || now == BITE_B))
                ev->bite = i;
            if (e->frame >= anim_len(s, ANIM_HOLD))
                e->frame -= anim_len(s, ANIM_HOLD);
        }
        else
        {
            // 被弾の反応と倒れるのは1回だけ再生。ひるんでいる間（ヘッドショットの後）は、反応の終わり近くで
            // かがんだまま、ひるみが解けるまで止まる。
            e->frame += ANIM_STEP;
            if (e->shove > 0)
            {
                e->shove--;
                move(s, e, -mulf32(sinLerp(e->yaw), RELEASE_PUSH / SHOVE_FRAMES), -mulf32(cosLerp(e->yaw), RELEASE_PUSH / SHOVE_FRAMES));
            }
            if (e->daze > 0)
            {
                e->daze--;
                if (e->frame > anim_len(s, ANIM_HIT) * 2 / 3)
                    e->frame = anim_len(s, ANIM_HIT) * 2 / 3;
            }
            else if (e->frame >= anim_len(s, state_anim[e->state]))
                set_state(s, i, e->state == ENEMY_HIT ? ENEMY_IDLE : ENEMY_DEAD);
        }

        // 生きている敵同士を離す。
        for (int j = 0; j < s->count; j++)
        {
            Enemy *o = &s->e[j];
            if (j == i || !o->active || o->state >= ENEMY_DYING || e->state >= ENEMY_DYING || e->state == ENEMY_GRAB || e->thrower)
                continue;
            int ox = e->pos[0] - o->pos[0], oz = e->pos[2] - o->pos[2];
            int d = (int)sqrt64((u64)((s64)ox * ox + (s64)oz * oz));
            if (d > 0 && d < 2 * RADIUS)
            {
                int push = (2 * RADIUS - d) / 2;
                move(s, e, mulf32(divf32(ox, d), push), mulf32(divf32(oz, d), push));
            }
        }

        if (e->pose_dirty || (e->state != ENEMY_DEAD && (s->tick + i) % 3 == 0))
            update_pose(s, e);
    }
    s->tick++;
}

void enemies_hold_at(Enemies *s, int i, const int pos[3], int dist)
{
    Enemy *e = &s->e[i];
    e->pos[0] = pos[0] - mulf32(sinLerp(e->yaw), dist);
    e->pos[2] = pos[2] - mulf32(cosLerp(e->yaw), dist);
    ground_collide_n(s->ground, &e->pos[0], &e->pos[2], e->feet + STEP_UP, e->feet + HEIGHT, WALL_RADIUS, 2);   // （レオンの背中が壁）
    ground_height(s->ground, e->pos[0], e->pos[2], e->feet, STEP_UP, &e->feet);
    e->pos[1] = e->feet - s->model->bbox_min[1];
}

void enemies_release(Enemies *s, int i)
{
    Enemy *e = &s->e[i];
    if (e->state != ENEMY_GRAB)
        return;
    set_state(s, i, ENEMY_HIT);
    e->shove = SHOVE_FRAMES;
}

int enemies_push(const Enemies *s, int *x, int *z, int radius)
{
    int pushed = 0;
    for (int i = 0; i < s->count; i++)
    {
        const Enemy *e = &s->e[i];
        if (!e->active || e->state >= ENEMY_DYING)
            continue;
        int ox = *x - e->pos[0], oz = *z - e->pos[2], min = radius + RADIUS;
        int d = (int)sqrt64((u64)((s64)ox * ox + (s64)oz * oz));
        if (d > 0 && d < min)
        {
            *x = e->pos[0] + mulf32(divf32(ox, d), min);
            *z = e->pos[2] + mulf32(divf32(oz, d), min);
            pushed++;
        }
    }
    return pushed;
}

// 近い順（DS は1フレームに2048ポリゴンまでで、それを超えた後のものは捨てる：消えるのは
// レオンの目の前ではなく霧の中のもの）、最大 max 体まで。
void enemies_draw(Enemies *s, u32 poly_fmt, const int eye[3], int max)
{
    int order[ENEMY_MAX], dist[ENEMY_MAX], n = 0;
    for (int i = 0; i < s->count; i++)
    {
        Enemy *e = &s->e[i];
        if (!e->active || !view_visible(e->pos, inttof32(10)) || (s->hide_below && e->feet < s->hide_below))
            continue;   // （視界の外：ジオメトリエンジンに送らない）
        int dx = (e->pos[0] - eye[0]) >> 4, dz = (e->pos[2] - eye[2]) >> 4, d = dx * dx + dz * dz;
        int k = n++;
        while (k > 0 && dist[k - 1] > d)
        {
            order[k] = order[k - 1];
            dist[k] = dist[k - 1];
            k--;
        }
        order[k] = i;
        dist[k] = d;
    }
    for (int j = 0; j < n && j < max; j++)
    {
        Enemy *e = &s->e[order[j]];
        if (e->pose_dirty)
            update_pose(s, e);
        model_draw(e->boss ? s->boss_model : s->model, e->pos, e->yaw, poly_fmt, (const int (*)[12])e->pose);
        if (!e->boss && e->weapon && s->weapon_model[e->weapon])
            model_draw_held(s->weapon_model[e->weapon], e->pos, e->yaw, poly_fmt, e->pose[HAND_NODE]);
        if (e->plaga && s->plaga_model && e->plg_clip >= 0)
        {
            static int local[MODEL_MAX_NODES][12];
            anim_pose(s->plaga_anims, e->plg_clip, e->plg_frame, s->plaga_model, local);
            model_draw_on(s->plaga_model, e->pos, e->yaw, poly_fmt, e->pose[PLG_NECK_NODE], (const int (*)[12])local);
        }
    }
}

int enemies_raycast(const Enemies *s, const int p[3], const int d[3], int *t_best, int *part)
{
    int found = -1;
    // 円との判定に使う光線の水平成分。ステージのどの距離でも2乗が64ビットに収まるように、
    // 以下はすべて一度 4096 で割っておく。
    s64 aa = ((s64)d[0] * d[0] + (s64)d[2] * d[2]) >> 12;   // |d.xz|^2
    if (aa == 0)
        return -1;   // 真上か真下
    for (int i = 0; i < s->count; i++)
    {
        const Enemy *e = &s->e[i];
        if (!e->active || e->state >= ENEMY_DYING)
            continue;
        // 入る点 t を |(p - c).xz + d.xz * t| = RADIUS で解く。
        s64 fx = p[0] - e->pos[0], fz = p[2] - e->pos[2];
        s64 b = (fx * d[0] + fz * d[2]) >> 12;           // f32^2 / 4096
        s64 c = ((fx * fx + fz * fz) >> 12) - (((s64)RADIUS * RADIUS) >> 12);
        s64 disc = b * b - aa * c;
        if (disc < 0)
            continue;
        s64 root = (s64)sqrt64(disc);
        s64 t = ((-b - root) << 12) / aa;   // f32
        if (t <= 0 && ((-b + root) << 12) / aa > 0)
            t = 1;   // 銃口が体の中（至近距離）：その場で当たり
        if (t <= 0 || t >= *t_best)
            continue;
        s64 y = p[1] + ((d[1] * t) >> 12) - e->feet;
        if (y < 0 || y > (e->plaga ? PLG_TOP : HEIGHT))
            continue;
        *t_best = (int)t;
        *part = y > HEAD_FROM ? PART_HEAD : y < LEGS_TO ? PART_LEGS : PART_BODY;
        found = i;
    }
    return found;
}

void enemies_damage(Enemies *s, int i, int damage)
{
    Enemy *e = &s->e[i];
    if (!e->active || e->state >= ENEMY_DYING)
        return;
    e->hp -= damage;
    e->flinch = 0;
    set_state(s, i, e->hp <= 0 ? ENEMY_DYING : ENEMY_HIT);
    if (s->watch)
        enemies_alert(s, i);   // 撃たれた：レオンの居場所がわかり、まわりの敵にもわかる
}

int enemies_shot(Enemies *s, int i, int power, int part, int crit_rate, int crit_wep)
{
    Enemy *e = &s->e[i];
    if (!e->active || e->state >= ENEMY_DYING)
        return 0;
    if (s->watch)
        enemies_alert(s, i);
    // クリティカルなヘッドショット（bio4_brew.mod 0x22be0）：確率は種類の頭の値 × 武器の
    // クリティカル率 / 100。当たれば、部位のダメージ率に関係なく、火力 × その武器に対する種類の
    // 倍率（最大30000）。
    int chance = part == PART_HEAD && crit_wep < CRIT_NONE ? e->kind->crit * crit_rate / 100 : 0;
    if (chance > 0 && (int)(rnd(s) % 100) < chance)
    {
        int d = power * e->kind->crit_mul[crit_wep];
        e->hp -= d > 30000 ? 30000 : d;
        s->crit_hit = 1;
    }
    else
        e->hp -= power * e->kind->dmg[part] / 100;
    e->flinch += power * e->kind->flinch[part] / 100;
    if (e->hp > 0 && e->flinch < e->kind->flinch_max)
        return 0;   // （当たっても向かってくる）
    e->flinch = 0;
    set_state(s, i, e->hp <= 0 ? ENEMY_DYING : ENEMY_HIT);
    return 1;
}

void enemies_set_kind(Enemies *s, int i, const FoeKind *kind)
{
    Enemy *e = &s->e[i];
    e->kind = kind;
    e->hp = kind->hp;
    e->flinch = 0;
    // 武器は種類の装備カテゴリで決まる（GM_Data_ChParam::Get_WepMdlId：13 は eqp_wep_all #10
    // の手斧、14 は #11 のピッチフォーク、教徒の15は #12、16は #13 のボウガン、17は #14 の鎌、
    // 18は #15 の杖）。教徒のそれ以外のもの（21、7面の投げる敵）は鎌で代用する。
    if (e->boss)
        e->weapon = WEAPON_NONE;
    else if (is_cultist(e))
        e->weapon = kind->ctg == 15 ? WEAPON_BOARD : kind->ctg == 16 ? WEAPON_XBOW : kind->ctg == 18 ? WEAPON_STAFF
                    : kind->ctg ? WEAPON_SICKLE : WEAPON_NONE;
    else
        e->weapon = kind->ctg == 14 ? WEAPON_FORK : kind->ctg ? WEAPON_AXE : WEAPON_NONE;
#ifdef TEST_WEAPON   // テスト用ビルド：全員がこれを持つ
    if (!e->boss)
        e->weapon = TEST_WEAPON;
#endif
    if (e->thrower)
        e->weapon = WEAPON_NONE;   // （投げ物を手に持っている）
    // プラーガつき（3面のもの：種類18、カテゴリ19）：頭も武器も無く、プラーガが打ちつける。
    e->plaga = !e->boss && kind->plaga;
    if (e->plaga)
        e->weapon = WEAPON_NONE;
    e->plg_clip = -1;
    e->pose_dirty = 1;
}

// 1体がレオンの居場所を知った。原作では、ほかの敵に知らせるのはリーダーだけ（種類のリーダー
// フラグ、GM_Data_ChP_Info::Get_Idv_LeaderFlg：ガナードの種類6-8、教徒の種類5-8）：標的を
// 持つと彼を指さし（GM_Hdl_Ch_Normal::_Check_Yubisashi、モーション53）、ステージ中の全部の敵が
// どれだけ遠くても彼を標的にする（GM_CtrlMain_Enemy::Set_AllTrgFind）。ほかの敵は知らせない
// （それぞれ自分の目と耳で気づくか、撃たれて気づく）。
void enemies_alert(Enemies *s, int i)
{
    if (s->e[i].spotted)
        return;
    s->e[i].spotted = 1;
    if (!s->e[i].kind || !s->e[i].kind->leader)
        return;
    for (int j = 0; j < s->count; j++)
        if (j != i && s->e[j].active && s->e[j].state < ENEMY_DYING)
            s->e[j].spotted = 1;
}

int enemies_alive(const Enemies *s)
{
    int n = 0;
    for (int i = 0; i < s->count; i++)
        n += s->e[i].active && s->e[i].state < ENEMY_DYING;
    return n;
}

void enemies_daze(Enemies *s, int i)
{
    Enemy *e = &s->e[i];
    if (e->active && e->state == ENEMY_HIT)
        e->daze = DAZE_FRAMES;
}

// 敵 e が前方（約45度以内）にいれば (pos, yaw) からの距離、でなければ -1。
static int in_front(const Enemy *e, const int pos[3], int yaw)
{
    int dx = e->pos[0] - pos[0], dz = e->pos[2] - pos[2];
    int d = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
    s64 dot = ((s64)sinLerp(yaw) * dx + (s64)cosLerp(yaw) * dz) >> 12;
    return d > 0 && dot * 10 > (s64)d * 7 ? d : -1;
}

int enemies_kickable(const Enemies *s, const int pos[3], int yaw)
{
    for (int i = 0; i < s->count; i++)
    {
        const Enemy *e = &s->e[i];
        int d;
        if (e->active && e->state == ENEMY_HIT && e->daze > 0 && (d = in_front(e, pos, yaw)) >= 0 && d < KICK_RANGE)
            return i;
    }
    return -1;
}

int enemies_kick(Enemies *s, const int pos[3], int yaw, int damage)
{
    int n = 0;
    for (int i = 0; i < s->count; i++)
    {
        Enemy *e = &s->e[i];
        int d;
        if (!e->active || e->state >= ENEMY_DYING || (d = in_front(e, pos, yaw)) < 0 || d >= KICK_RANGE + RADIUS)
            continue;
        enemies_damage(s, i, damage);
        int dx = e->pos[0] - pos[0], dz = e->pos[2] - pos[2];
        move(s, e, mulf32(divf32(dx, d), KICK_PUSH), mulf32(divf32(dz, d), KICK_PUSH));
        n++;
    }
    return n;
}
int enemies_slash(Enemies *s, const int pos[3], int yaw, int range, int damage)
{
    int best = -1, best_d = range + RADIUS;
    for (int i = 0; i < s->count; i++)
    {
        const Enemy *e = &s->e[i];
        int d;
        if (e->active && e->state < ENEMY_DYING && (d = in_front(e, pos, yaw)) >= 0 && d < best_d)
        {
            best = i;
            best_d = d;
        }
    }
    if (best >= 0)
        enemies_damage(s, best, damage);
    return best;
}