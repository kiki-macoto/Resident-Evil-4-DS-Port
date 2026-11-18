// RE4 Zeebo -> DS デモ：1面（村）+ レオン。
//
// 画面：タイトル（メニュー：スタート / 操作説明）-> ゲーム。ゲーム中の START でポーズ（再開 /
// やり直し / タイトルへ）。レオンが死ぬとゲームオーバーのメニュー（コンティニュー / タイトルへ）。
//
// SELECT でステータス画面（武器の一覧）を開く。ゲーム中は2つのモード（L + SELECT で切り替え）：
//
// レオンを操作（既定）、RE4 風のラジコン操作と追従カメラ：
//   上                前進                   B + 上            走る
//   下                後退                   左 / 右           向きを変える
//   R（押したまま）   ハンドガンを構える（レーザーサイト）。上下で狙いの傾き、左右でゆっくり向きを変える
//   A（構え中）       撃つ                   Y                 リロード（弾切れでも自動）
//   X                 ハンドガンを出して構える / ホルスターにしまう
//   タッチのカメラ枠  レオンのまわりを見回す（下画面。離すと戻る）
//   A / B / X / Y     ガナードにつかまれたら連打で振りほどく
//   START             ポーズメニュー         L                 アイテム選択 / バイタル画面
//
// フリーカメラ：
//   十字キー上下      前後に移動             十字キー左右      向きを変える
//   L / R             横に移動               X / B             上下を見る
//   Y / A             下 / 上に移動          タッチしてドラッグ  見回す
//   START             レオン：待機 / 歩き / 走り（円を回る）/ 非表示
//
// 両方：Y + SELECT    カリング：裏 / 表 / なし
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <nds.h>

#include "anim.h"
#include "drops.h"
#include "enemy.h"
#include "objects.h"
#include "ground.h"
#include "model.h"
#include "music.h"
#include "sfx.h"
#include "sheet.h"
#include "text.h"
#include "vital.h"
#include <string.h>

#include "barrel_bin.h"
#include "beam_bin.h"
#include "bell_bin.h"
#include "hang_bin.h"
#include "tank_bin.h"
#include "boom_bin.h"
#include "crate_bin.h"
#include "door_bin.h"
#include "font_bin.h"
#include "ganado_anim_bin.h"
#include "ganado_bin.h"
#include "cha_anim_bin.h"
#include "cha_bin.h"
#include "axe_bin.h"
#include "fork_bin.h"
#include "gna_w0_bin.h"
#include "gna_w1_bin.h"
#include "sickle_bin.h"
#include "gun_bin.h"
#include "hud_bin.h"
#include "iconbmp_bin.h"
#include "items_bin.h"
#include "ladder_bin.h"
#include "vitalbmp_bin.h"
#include "leon_anim_bin.h"
#include "leon_bin.h"
#include "leon_gun_bin.h"
#include "knife_bin.h"
#include "leon_knife_bin.h"
#include "medal_bin.h"
#include "menu_bin.h"
#include "music_bin.h"
#include "result_bin.h"
#include "sfx_bin.h"
#include "spark_bin.h"
#include <fat.h>
#include <filesystem.h>
#include <stddef.h>
#include "stages.h"
#include "ashley.h"
#include "giant.h"
#include "garrador.h"
#include "cutins.h"
#include "verdugo.h"
#include "missile.h"
#include "stagesel_bin.h"
#include "star_bin.h"
#include "title_bin.h"

static Model leon, gun, ganado;
static Sheet hud, items, star, beam, spark;
static Drops drops;

// 全画面の絵（タイトル、ステージセレクト、リザルト）は 64KB のテクスチャで、ステージのテクスチャ
// の横に VRAM の空きは1枚分しかない：この枠を共有する。
enum { PIC_TITLE, PIC_STAGESEL, PIC_RESULT };
static Sheet picture;
static int picture_id = -1;

// 表示中のステージ（0..STAGE_COUNT-1）：そのモデルと地面。NitroFS からバッファに読み、読み込んで
// いる間は確保したままにする（モデルの描画リストと地面がその中を指している）。
static Model stage;
static Ground stage_ground;
static void *stage_buf, *ground_buf;
static int stage_loaded = -1;

static void *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *p = malloc(n);
    if (p && fread(p, 1, n, f) != (size_t)n)
    {
        free(p);
        p = NULL;
    }
    fclose(f);
    return p;
}

static long file_size(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

// ステージ i を読み込む（前のものは解放）。成功で0を返す。
static int load_stage(int i)
{
    if (i == stage_loaded)
        return 0;
    if (stage_loaded >= 0)
    {
        model_free(&stage);
        free(stage_buf);
        free(ground_buf);
        stage_buf = ground_buf = NULL;
    }
    stage_loaded = -1;
    char sp[32], gp[32];
    snprintf(sp, sizeof(sp), "nitro:/stage%02d.bin", i + 1);
    snprintf(gp, sizeof(gp), "nitro:/ground%02d.bin", i + 1);
    stage_buf = read_file(sp);
    ground_buf = read_file(gp);
    if (!stage_buf || !ground_buf)
        return -1;
    memset(&stage, 0, sizeof(stage));
    int err = model_load(&stage, stage_buf, file_size(sp));
    if (!err)
        err = ground_load(&stage_ground, ground_buf, file_size(gp)) * 10;
    if (!err)
        stage_loaded = i;
    return err;
}

// 空と霧の色（もや）、8ビットの RGB。
static void set_haze(const u8 c[3])
{
    glClearColor(c[0] >> 3, c[1] >> 3, c[2] >> 3, 31);
    glFogColor(c[0] >> 3, c[1] >> 3, c[2] >> 3, 31);
}

// チェーンソー男のテクスチャ（32KB）は、メニューの絵とテクスチャ VRAM を共有する：彼がステージ
// にいる間だけ読み込み、プレイ中は絵を捨てる。
static Model cha_model;
static AnimSet cha_anims;
static int cha_loaded;

static void cha_unload(void)
{
    if (cha_loaded)
        model_free(&cha_model);
    cha_loaded = 0;
}

// アシュリー、5、6、7、8面（ASH_STAGE_A..D、0始まり。Zeebo の動画では、カタパルトのある城の
// ステージやガラドールの牢を過ぎるまでレオンのそばにいる）。彼女のモデルはそのときだけ読み込む。
#define ASH_STAGE_A    4
#define ASH_STAGE_B    5
#define ASH_STAGE_C    6
#define ASH_STAGE_D    7
// …と10面（0始まりで9。セレクトでは STAGE 9）：そのミッションでは彼女は下のホールにいて
// （レコードの種類1）、出口は「アシュリーと一緒に」（GET_WITH_ASHLEY）。彼女がいないと
// ステージをクリアできなかった。
#define ASH_STAGE_E    9

// 城（7-12面、CASTLE_FROM から、0始まり）：敵は教徒（6面のアーカイブの ch_jya：ガナードの骨格と
// クリップの順）。そのとき NitroFS から読む。
#define CASTLE_FROM    6   // （ミッション6は村の小屋の籠城戦：ガナード）

// 2-12面：敵が前方のレオンを見る距離（聞くのはもっと近く、どの向きでも）。
#ifndef ENEMY_SIGHT
#define ENEMY_SIGHT    inttof32(150)
#endif
static Model jya_model;
static AnimSet jya_anims, jya_w0;
static void *jya_buf[3];
static int jya_loaded;
// 教徒のほかの武器（カテゴリ15の #12、ボウガン #13、杖 #15）、そのセット（ch_jya_w01、_w02）と
// ボウガンの矢（eff_all #17）：教徒と一緒に読み込む。読み込めない（空きが無い）ときは、その教徒は
// 基本のクリップで手に何も持たず、矢も描かない。
static Model cw_board, cw_xbow, cw_staff, bolt_model;
static AnimSet jya_w1, jya_w2;
static void *cw_buf[6];
static int cw_loaded;
static Enemies ganados;   // （下で定義）

static void jya_unload(void)
{
    enemies_arm_cultists(&ganados, NULL, NULL, NULL, NULL, NULL);
    if (cw_loaded)
    {
        model_free(&cw_board);
        model_free(&cw_xbow);
        model_free(&cw_staff);
        model_free(&bolt_model);
        cw_loaded = 0;
    }
    for (int i = 0; i < 6; i++)
    {
        free(cw_buf[i]);
        cw_buf[i] = NULL;
    }
    if (!jya_loaded)
        return;
    model_free(&jya_model);
    for (int i = 0; i < 3; i++)
    {
        free(jya_buf[i]);
        jya_buf[i] = NULL;
    }
    jya_loaded = 0;
}

// jya_load() で読んだもので教徒に武器を持たせる（教徒の enemies_set_look() の後に呼ぶ）。
static void jya_arm(void)
{
    if (cw_loaded)
        enemies_arm_cultists(&ganados, &cw_board, &cw_xbow, &cw_staff, &jya_w1, &jya_w2);
    else
        enemies_arm_cultists(&ganados, NULL, NULL, NULL, NULL, NULL);
}

static int jya_load(void)
{
    if (jya_loaded)
        return 0;
    static const char *const path[3] = { "nitro:/jya.bin", "nitro:/jya_anim.bin", "nitro:/jya_w0.bin" };
    long n[3];
    for (int i = 0; i < 3; i++)
    {
        n[i] = file_size(path[i]);
        if (!(jya_buf[i] = read_file(path[i])))
            return -1;
    }
    memset(&jya_model, 0, sizeof(jya_model));
    if (model_load(&jya_model, jya_buf[0], n[0]) || anim_load(&jya_anims, jya_buf[1], n[1]) || anim_load(&jya_w0, jya_buf[2], n[2]))
        return -2;
    jya_loaded = 1;
#ifndef TEST_NO_CW   // （テスト用ビルド：教徒のほかの武器なしで）
    {
        static const char *const cpath[6] = { "nitro:/cboard.bin", "nitro:/cxbow.bin", "nitro:/cstaff.bin", "nitro:/bolt.bin",
                                              "nitro:/jya_w1.bin", "nitro:/jya_w2.bin" };
        Model *const cm[4] = { &cw_board, &cw_xbow, &cw_staff, &bolt_model };
        long cn[6];
        int ok = 1;
        for (int i = 0; i < 4; i++)
            memset(cm[i], 0, sizeof(*cm[i]));
        for (int i = 0; i < 6 && ok; i++)
        {
            cn[i] = file_size(cpath[i]);
            ok = (cw_buf[i] = read_file(cpath[i])) != NULL;
        }
        for (int i = 0; i < 4 && ok; i++)
            ok = model_load(cm[i], cw_buf[i], cn[i]) == 0;
        ok = ok && anim_load(&jya_w1, cw_buf[4], cn[4]) == 0 && anim_load(&jya_w2, cw_buf[5], cn[5]) == 0
             && jya_w1.nodes == jya_model.node_count && jya_w2.nodes == jya_model.node_count;
        if (!ok)
            for (int i = 0; i < 4; i++)
                model_free(cm[i]);   // （読み込めた分の VRAM を返す）
        cw_loaded = ok;
    }
#endif
    return 0;
}

// ボウガンの矢（教徒のカテゴリ16）：eff_all #17。ボウガンの高さから、レオンの胸があった所へ
// BOLT_SPEED でまっすぐ飛ぶ。壁に当たると止まり、レオンの体（まわり BOLT_R、身長分）に当たると
// ダメージ。速さは移植版独自（原作の値は調べていない）。
#define BOLTS_MAX     6
#define BOLT_SPEED    floattof32(1.5)    // 1フレームの単位（1秒に90）
#define BOLT_LIFE     120
#define BOLT_R        floattof32(2.2)
#define BOLT_HEIGHT   inttof32(13)       // 放つ高さ（教徒の足元から）
#define BOLT_AIM      inttof32(11)       // 狙う高さ（レオンの足元から）
typedef struct { int pos[3], vel[3], life, damage; } Bolt;
static Bolt bolts[BOLTS_MAX];

static void bolt_fire(const int from[3], const int at[3], int damage)
{
    for (int i = 0; i < BOLTS_MAX; i++)
        if (bolts[i].life <= 0)
        {
            Bolt *b = &bolts[i];
            int d[3] = { at[0] - from[0], at[1] - from[1], at[2] - from[2] };
            int len = (int)sqrt64((u64)((s64)d[0] * d[0] + (s64)d[1] * d[1] + (s64)d[2] * d[2]));
            if (len <= 0)
                return;
            for (int k = 0; k < 3; k++)
            {
                b->pos[k] = from[k];
                b->vel[k] = (int)(((s64)d[k] * BOLT_SPEED) / len);
            }
            b->life = BOLT_LIFE;
            b->damage = damage;
            return;
        }
}

static void bolts_clear(void)
{
    for (int i = 0; i < BOLTS_MAX; i++)
        bolts[i].life = 0;
}

// 矢を1フレーム進める。レオンに届いたもの（足元 at[]、ダメージを受ける状態のとき）は消えて
// そのダメージを返す（0：なし）。アシュリーに届いたもの（足元 ally[]、NULL：いない。身長13）は
// そのダメージを *ally_hurt に入れる。
static int bolts_update(const int at[3], int vulnerable, const int *ally, int *ally_hurt)
{
    int hurt = 0;
    *ally_hurt = 0;
    for (int i = 0; i < BOLTS_MAX; i++)
    {
        Bolt *b = &bolts[i];
        if (b->life <= 0)
            continue;
        b->life--;
        int dir[3] = { divf32(b->vel[0], BOLT_SPEED), divf32(b->vel[1], BOLT_SPEED), divf32(b->vel[2], BOLT_SPEED) }, hit[3];
        int wall = ground_raycast(&stage_ground, b->pos, dir, BOLT_SPEED, hit, RAY_SHOT);
        for (int k = 0; k < 3; k++)
            b->pos[k] += b->vel[k];
        int dx = b->pos[0] - at[0], dz = b->pos[2] - at[2];
        if (!hurt && vulnerable && b->pos[1] > at[1] && b->pos[1] < at[1] + inttof32(17)
            && (s64)dx * dx + (s64)dz * dz < (s64)BOLT_R * BOLT_R)
        {
            hurt = b->damage;
            b->life = 0;
        }
        else if (ally && !*ally_hurt && b->pos[1] > ally[1] && b->pos[1] < ally[1] + inttof32(13)
                 && (s64)(b->pos[0] - ally[0]) * (b->pos[0] - ally[0]) + (s64)(b->pos[2] - ally[2]) * (b->pos[2] - ally[2]) < (s64)BOLT_R * BOLT_R)
        {
            *ally_hurt = b->damage;
            b->life = 0;
        }
        else if (wall)
            b->life = 0;
    }
    return hurt;
}

static void bolts_draw(void)
{
    if (!cw_loaded)
        return;
    for (int i = 0; i < BOLTS_MAX; i++)
        if (bolts[i].life > 0)
        {
            const Bolt *b = &bolts[i];
            // モデルは +Z 向き、先端が前：飛ぶ向きに回す（水平に）。
            int yaw = (int)(atan2f((float)b->vel[0], (float)b->vel[2]) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
            model_draw(&bolt_model, b->pos, yaw, POLY_ALPHA(31) | POLY_CULL_NONE, NULL);
        }
}

// 投げ物（2面のダイナマイト、城の燃える石）。
#define MISSILE_RANGE  inttof32(14)      // この距離以内なら爆発でダメージ
#define MISSILE_DAMAGE 20               // （投げる敵が何人もいる：1回30だと2回の投げで死んでいた）
static Missiles missiles;
#ifdef TEST_SHOW_THROW
static int throws_seen;
#endif
#define ASH_SHOT_DAMAGE 25
static Ashley ash;
// エルヒガンテ：4面に1体、11面に2体（0始まりで3と10。ミッション11「A Fornalha」：
// 「dois El Gigantes」）。
#define GIANT_STAGE_A  3
#define GIANT_STAGE_B  10
#ifndef GIANT_AHEAD
#define GIANT_AHEAD    70                // 1体目はレオンのこれだけ前から始まる（単位）
#endif
#define GIANT_AHEAD_U  inttof32(GIANT_AHEAD)
#define GIANT_MAX      2
#ifndef GIANT2_DEG
#define GIANT2_DEG     40                   // 2体目：レオンの左へこれだけの角度…
#endif
#define GIANT2_FARTHER inttof32(30)         // …そしてこれだけ遠く
#define GIANT_RADIUS   inttof32(9)          // （giant.c の体の半径：互いに離れる）
static Giant giants[GIANT_MAX];
static int giant_count;
#define giant giants[0]   // （テスト用の表示：1体目）
static int giants_down(void)
{
    for (int i = 0; i < giant_count; i++)
        if (giants[i].state != GIANT_DEAD)
            return 0;
    return 1;
}
// レオンが今の位置から寄生体を斬れる巨人、なければ NULL。
static Giant *giant_at_hand(const int pos[3])
{
    for (int i = 0; i < giant_count; i++)
        if (giant_cuttable(&giants[i], pos))
            return &giants[i];
    return NULL;
}
#ifdef TEST_GX
static int gx_poly_peak, gx_vert_peak, gx_overflows, gx_underflows, gx_stage_poly, gx_stage_vert, gx_rdlines_min = 99;
#endif

// ガラドール、8面に1体、9面に2体（0始まりで7、8）：目が見えず、物音に向かう。
// 銃声はどこまでも届く。レオンが走ると GAL_HEAR_RUN 以内、歩きはすぐ横でだけ聞こえる
// （GAL_HEAR_NEAR）。テスト用ビルド：-DGAL_AHEAD=n でレオンの n 単位前に置く。
#define GAL_STAGE      7
#define GAL_STAGE_2    8
#define GAL_MAX        2
#define GAL_HEAR_RUN   inttof32(60)
#define GAL_HEAR_NEAR  inttof32(10)
static Garrador gals[GAL_MAX];
static int gal_count; int garradors_down(void)
{
    for (int i = 0; i < gal_count; i++)
        if (gals[i].state != GAL_DEAD)
            return 0;
    return 1;
}
#define gal gals[0]   // （テスト用の表示：1体目）

// サラザールの右腕（ヴェルデューゴ）、12面（0始まりで11）。レオンのスタート地点のまわりに液体窒素の
// タンク（樽のモデル、そのステージだけ読む）：彼の近くで1発撃って開けると凍る。
#define VER_STAGE      11
#ifndef VER_AHEAD
#define VER_AHEAD      50                // レオンのこれだけ前から始まる（単位）
#endif
#define TANK_RANGE     inttof32(34)      // タンクの冷気がここまで届く（22だと彼がほぼ真上にいる必要があった）
// 12面はゲームで唯一の時間制限のあるストーリーミッション：エレベーターは「約4分」かかり
// （mis_data_s12 の注記）、到着でミッションが終わる。ヴェルデューゴを倒していなくても。
// Zeebo の動画どおり、画面に残り時間は出さない。
#ifndef ELEVATOR_FRAMES                  // テスト用ビルド：-DELEVATOR_FRAMES=n で待ち時間を短く
#define ELEVATOR_FRAMES (240 * 60)
#endif
#define ELEVATOR_NOTE  150               // 「The elevator has arrived.」を出しておくフレーム数
static Verdugo ver;
static Model barrel_model;   // （下のほかの仕掛けのモデルと一緒に）
static int barrel_loaded;

static void barrel_unload(void)
{
    if (barrel_loaded)
        model_free(&barrel_model);
    barrel_loaded = 0;
}

#ifdef TEST_PROF   // テスト用ビルド：フレームの各部分にかかった時間（バイタル画面を参照）
static u32 prof_mark, prof_peak[8];
#define PROF(i) do { u32 t_ = cpuGetTiming(); u32 d_ = (t_ - prof_mark) / 3351; if (d_ > prof_peak[i]) prof_peak[i] = d_; prof_mark = t_; } while (0)
#else
#define PROF(i) ((void)0)
#endif

static void gimmicks_free(void);
static void plaga_unload(void);
static void use_picture(int id)
{
    static const void *const data[] = { title_bin, stagesel_bin, result_bin };
    static const size_t size[] = { title_bin_size, stagesel_bin_size, result_bin_size };
    if (id == picture_id)
        return;
    cha_unload();
    ashley_unload(&ash);
    for (int i = GIANT_MAX - 1; i >= 0; i--)   // （共有のものを先に）
        giant_unload(&giants[i]);
    for (int i = GAL_MAX - 1; i >= 0; i--)
        garrador_unload(&gals[i]);
    verdugo_unload(&ver);
    barrel_unload();
    gimmicks_free();
    jya_unload();
    plaga_unload();
    sheet_free(&picture);
    picture_id = sheet_load(&picture, data[id], size[id]) == 0 ? id : -1;
}
static AnimSet leon_anims, leon_gun, ganado_anims, ganado_w0, ganado_w1;
static Model sickle_model, axe_model, fork_model;   // ガナードの武器（eqp_wep_all #14、#10、#11）
// レオンのナイフ（eqp_wep_all #0）とそのアニメーション（ch_leon_w00.mcas：#2 構え、しゃがみ。
// #3 低い斬り、足元の木箱用。#1 胸の高さの斬り）。銃と同じく視点は肩越しに寄り、上下で傾く：
// 上に傾けると高く斬る。
static Model knife_model;
static AnimSet leon_knife;
#ifndef KNIFE_SLASH
#define KNIFE_SLASH   3
#endif
#define KNIFE_READY   2
#define KNIFE_HIGH    1
#define KNIFE_HIGH_TILT floattof32(0.3)  // これより上に傾けると：高い斬り
#define KNIFE_RANGE   inttof32(8)
#define KNIFE_REACH   inttof32(14)      // 視線に沿った刃の届く距離（胸から）
#define KNIFE_PITCH   degreesToAngle(55) // 上 / 下にいっぱいに傾けたときの視点（と上半身）
#define KNIFE_CHEST   inttof32(13)
#define KNIFE_RAISED  inttof32(5)       // 足元からこれだけ上にある物（テーブル、棚）：胸の高さの斬り
#ifdef TEST_SHOW_KNIFE
static int knife_probe = 9999, v_knife2 = 8888;
#endif
#ifdef TEST_SHOW_HEAP
static int heap_kb;
#endif
// 刃が当たる物：胸からの線上（from[] から dir[] に沿って *t 以内）か、その少し上下にある物
// （刃は線でなく帯を払う：胸と同じ高さの棚の木箱は線のすぐ上にある）。番号か -1 を返し、
// *t にその距離を入れる。
static int knife_object(const Objects *s, const int from[3], const int dir[3], int *t)
{
    static const s8 up[] = { 0, 4, -4, 8 };
    for (unsigned k = 0; k < sizeof(up); k++)
    {
        int f[3] = { from[0], from[1] + inttof32(up[k]), from[2] }, tt = *t;
        int i = objects_raycast(s, f, dir, &tt);
        if (i >= 0)
        {
            *t = tt;
            return i;
        }
    }
    return -1;
}
#define KNIFE_LEAN_UP   degreesToAngle(35)   // いっぱいに傾けたときの上半身（残りは腕の高い斬りで）
#define KNIFE_LEAN_DOWN degreesToAngle(33)
#define KNIFE_CAM_DROP  inttof32(7)          // 上にいっぱいに傾けたとき視点がこれだけ下がる
#define KNIFE_DAMAGE  130               // ナイフの火力、item_weapon_info.bin の武器0（原作の単位）
#ifndef KNIFE_HAND
#define KNIFE_HAND    14                // ナイフは右手（斬る方の手）
#endif
static Enemies ganados;
// 頭の無いガナードの首から出るプラーガ（3面の、WithPlgFlg のある種類）：それがいるステージでだけ
// NitroFS から読む（eqp_plg_all #0 とそのクリップ）。
static Model plg_model;
static AnimSet plg_anims;
static void *plg_buf[2];
static int plg_loaded;

static void plaga_unload(void)
{
    enemies_set_plaga(&ganados, NULL, NULL);
    if (!plg_loaded)
        return;
    model_free(&plg_model);
    for (int i = 0; i < 2; i++)
    {
        free(plg_buf[i]);
        plg_buf[i] = NULL;
    }
    plg_loaded = 0;
}

static int plaga_load(void)
{
    if (!plg_loaded)
    {
        static const char *const path[2] = { "nitro:/plg.bin", "nitro:/plg_anim.bin" };
        long n[2];
        for (int i = 0; i < 2; i++)
        {
            n[i] = file_size(path[i]);
            if (!(plg_buf[i] = read_file(path[i])))
                return -1;
        }
        memset(&plg_model, 0, sizeof(plg_model));
        if (model_load(&plg_model, plg_buf[0], n[0]) || anim_load(&plg_anims, plg_buf[1], n[1]))
            return -2;
        plg_loaded = 1;
    }
    enemies_set_plaga(&ganados, &plg_model, &plg_anims);
    return 0;
}

static Model door_model, ladder_model, barrel_model, crate_model, medal_model;
static Model bell_model, tank_model;   // fix_all #9、#6：ミッションファイルの鐘、12面のタンク
static Model hang_model;               // fix_all #5：鎖に吊るされた容器（種類05、8面）
static int door_loaded, bell_loaded, tank_loaded, hang_loaded;   // （それがあるステージでだけ読む）

// 仕掛けのモデルを読み込む（want）か解放する（!want）。読み込まれているかを返す。
static int gimmick_model(Model *m, int *loaded, int want, const void *bin, size_t size)
{
    if (want && !*loaded)
        *loaded = model_load(m, bin, size) == 0;
    else if (!want && *loaded)
    {
        model_free(m);
        *loaded = 0;
    }
    return *loaded;
}

static void gimmicks_free(void)
{
    gimmick_model(&door_model, &door_loaded, 0, NULL, 0);
    gimmick_model(&bell_model, &bell_loaded, 0, NULL, 0);
    gimmick_model(&tank_model, &tank_loaded, 0, NULL, 0);
    gimmick_model(&hang_model, &hang_loaded, 0, NULL, 0);
}
static Sheet boom_sheet;
static Objects objs;

// ミッション1の村人（ワールドの x, y, z）、レオンのスタート地点の前の広場にひとかたまりで。
// （原作の地点、mis_data_s01.bin は村じゅうに散らばっていて、屋根や通路の上もある。ここの
// ガナードは SIGHT 以内でしかレオンに気づかないので、ほとんどが見つけられなかった。）
// 一度にステージにいるのは最大 GANADO_ACTIVE 人（ポリゴンの予算）。残りはほかが倒れるにつれ、
// この順にそれぞれの地点から出てくる。
static const int ganado_spawn[][3] = {
    { 292, 0, 340 }, { 318, 0, 352 }, { 276, 0, 372 }, { 330, 0, 384 }, { 300, 0, 404 },
    { 262, 0, 350 }, { 344, 0, 362 }, { 284, 0, 420 }, { 316, 0, 428 }, { 300, 0, 320 } };
#define GANADO_COUNT ((int)(sizeof(ganado_spawn) / sizeof(ganado_spawn[0])))

// 1面はゲームの最初のストーリーモードのミッション（mis_data_s01）：村人を全員倒し、次に
// チェーンソー男、それから鐘。結果の後はステージセレクトに戻る。
#ifndef OBJ_FRAMES                       // （テスト用ビルド：キャプチャで捉えるため長く）
#define OBJ_FRAMES     240               // 開始時に画面に出すミッションの目的
#endif
#ifndef BOSS_SURVIVE
#define BOSS_SURVIVE   (90 * 60)         // 彼が来てからこれだけで鐘が鳴る…
#endif
#ifndef BOSS_LINES
#define BOSS_LINES     240               // …登場のせりふはこれだけ出しておく
#endif
static const int boss_spawn[3] = { 300, 0, 380 };   // 村人が立っていた場所
#define MISSION_PTAS   5000              // ミッションの報酬（mis_data_s01 では 5000）
#define TREASURE_REACH inttof32(20)
#define TREASURE_PTAS  3000              // 宝を取った（動画の結果：Joia +3000）
#define GANADO_ACTIVE 5
#define GANADO_FROM_DOOR GANADO_COUNT     // （今は家の扉からは誰も出てこない）

// 2-12面：ミッションファイルの敵（stage_foes.c）を、原作と同じやり方で入れる
// （libbio4af.so の GM_CtrlMain_Enemy）：敵のユニットは FOE_UNITS 個。空いたユニットは、
// ファイルの順で、グループが入場済みで、エントリーが残っていて、そのどれもステージにいない
// 最初のレコードを取り、その次のエントリーをレコードの地点に置く。ユニットは敵の死体が消えると
// また空く。グループ0は最初から入場し、ほかはミッションのイベントで入る（foes_events()）。
#define FOES_MAX 32
#define FOE_UNITS 5     // （GM_CtrlMain::_Make_CtrlUnits：GM_CtrlMain_Enemy::Init(5, 50)）
#define WALL_TOP 80     // これだけ高いところの教徒のレコードは7面の城壁の上：投げる者
// どのステージにもミッションファイルの敵がいる。（-DSTAGE1_ENEMIES_ONLY：1面だけ。ほかは
// 空のまま歩いて回り、クリアしない。床、扉、はしごを確かめる間に使う。）
#ifdef STAGE1_ENEMIES_ONLY
#define FOES_ON(stage) ((stage) == 0)
#else
#define FOES_ON(stage) 1
#endif
static const StageFoe *foe_list[FOES_MAX];   // ガナードと教徒のレコード、ファイルの順で
static int foe_n, foe_slots;                 // …；床にいる者のスロット（0..）
static u8 foe_left[FOES_MAX];                // それぞれからまだ出てくるエントリーの数
static s8 foe_slot[FOES_MAX];                // そのエントリーがいるスロット（立っているか倒れているか）、-1
static s8 foe_fixed[FOES_MAX];               // 投げる者専用のスロット（開始時に作る）、なければ -1
static s8 slot_foe[ENEMY_MAX];               // スロットが持っているレコード、-1
static u16 foe_groups;                       // 入場したグループ（1つにつき1ビット）
static int foe_entered;                      // これまでに出てきたエントリーの数（テスト用の表示）

static __attribute__((unused)) int foes_standing(void)   // （テスト用の表示）
{
    int n = 0;
    for (int f = 0; f < foe_n; f++)
        n += foe_slot[f] >= 0 && ganados.e[foe_slot[f]].state < ENEMY_DYING;
    return n;
}

// レコードは、持っていたエントリーが全員倒れたら倒したことになる（GM_CtrlMain_Enemy::Check_EnDest）。
// recn はミッションファイルでの番号。ガナードでも教徒でもないもの：倒したことにはならない。
static int foe_record_down(int recn)
{
    for (int f = 0; f < foe_n; f++)
        if (foe_list[f]->rec == recn)
            return foe_left[f] == 0 && (foe_slot[f] < 0 || ganados.e[foe_slot[f]].state >= ENEMY_DYING);
    return 0;
}

// 入場したグループのレコードが全部倒された（Check_EntryGlpAllDest）。
static int foe_groups_down(void)
{
    for (int f = 0; f < foe_n; f++)
        if ((foe_groups >> foe_list[f]->wave & 1) && !foe_record_down(foe_list[f]->rec))
            return 0;
    return 1;
}

// 地点がレオンのいる場所にあるもの（8面の最初の教徒は彼のスタート地点に立つ）は、彼がそこから
// 離れるまで待つ。最大 FOE_WAIT フレーム（移植版独自：同じ場所に2つの体）。
#define FOE_CLEAR inttof32(30)
#define FOE_WAIT  (60 * 20)
static int foe_held;

// カリングに使う建物（ステージの描画を参照）：壁の箱（ワールドの x, z）、屋根の高さ、中から
// 外がどれだけ見えるか。
typedef struct { s8 stage; s16 x0, z0, x1, z1, top, yard; } Room;
static const Room rooms[] = {
    { 5, 174, 217, 294, 316, 60, 30 },   // 6面：小屋（はしごは壁の外に立つ）
};
typedef struct { s8 stage; s16 from, below; } Gallery;   // レオンが rom 以上の高さ：elow より下は描かない
static const Gallery galleries[] = {
    { 7, 36, 30 },   // 8面：謁見の間の上の回廊
};
static const Room *room_of(int stage)
{
    for (unsigned i = 0; i < sizeof(rooms) / sizeof(rooms[0]); i++)
        if (rooms[i].stage == stage)
            return &rooms[i];
    return NULL;
}
// ミッションのレコードがガナードや教徒に与える種類（部位ごとの体力、ダメージ、ひるみ）。
static const FoeKind *foe_kind_of(const StageFoe *f)
{
    if (f->type == FOE_CULTIST)
        return &foe_kind_cultist[f->variant < foe_kind_cultist_count ? f->variant : 0];
    return &foe_kind_ganado[f->variant < foe_kind_ganado_count ? f->variant : 0];
}

// ミッションのイベント（libbio4af.so の GM_CtrlMain_Event）。イベントは、依存するもの（敵の
// レコードが倒された、物が壊れた、レオンがイベント地点にいる）が成り立ち、その条件が成り立ち
// （条件ごとの _Check_Cond_*）、レオンが求められたやり方で起こしたとき（_Check_EveGetCond：
// それだけで、触れて、アクションボタンで、アシュリーがそばにいてボタンで）に起きる。起きるのは
// 1回。イベント地点のもので、そう指定されたものは毎回（鍵のかかった扉のせりふ）。その後
// スクリプトのコマンド：アイテムの入手や譲渡、ミッションフラグ（フラグ0：ミッション完了）、
// イベントの削除、グループの入場、そして場面：せりふと絵（cutins.c）。再生は呼び出し側が行う。
static u8 ev_played[8], ev_deleted[8], ev_inside[8];   // （イベント1つにつき1ビット）
static u8 items_held[32];                    // （アイテムIDごとに1ビット）
static u16 mis_flags;
static int mis_frames;                       // ミッション開始からのフレーム数
// イベントの時間条件が読む時間（GM_MisConduct::Get_LimitProgressTime。_Check_Cond_Time が
// 条件の分と比べる）：スクリプトのコマンド205（Set_LimitTime）で始まるまでは0、その後そのコマンド
// の秒数まで数えて止まる。もう一度205が来ると0からやり直す。7面の大砲：アシュリーが準備を始めて
// （「I'll hold them off」）から2分。ミッションの開始からではない（それだとレオンが着く頃には
// 準備ができていて、グループ2の10人が来る前に撃てた）。12面はミッションと同時に始まる
// （ヴェルデューゴの場面）。
// ゲームのフレームではなく実時間（vblank、1秒に60）で数える：重いステージは30で動く。
static int mis_timer, mis_timer_end;
static u32 mis_timer_vb;
static volatile u32 vblank_count;
static int mis_flag_clear;                   // ミッションフラグ0でステージクリア（そうでなければ：敵を全滅）
static s8 thing_obj[24];                     // ステージの各物が objs の何番になったか、-1
static s16 foe_at[FOES_MAX][3];              // 各レコードの最後のエントリーがいた場所（ワールド単位）
static int ev_button = -1;                   // ここでアクションボタンが起こすイベント、-1
static int ev_press;                         // そのためにアクションボタンが押された
static s8 ev_scene[4];                       // 再生する場面（ミッションのスクリプト）、順に
static int ev_scenes;
static s16 ev_got[4];                        // イベントで入手したアイテム、呼び出し側が渡す
static int ev_gots;
static int ev_bit(const u8 *b, int i) { return i >= 0 && i < 64 && (b[i >> 3] >> (i & 7) & 1); }
static void ev_set(u8 *b, int i) { if (i >= 0 && i < 64) b[i >> 3] |= 1 << (i & 7); }
static void ev_clr(u8 *b, int i) { if (i >= 0 && i < 64) b[i >> 3] &= ~(1 << (i & 7)); }
static int item_held(int id) { return id >= 0 && id < 256 && (items_held[id >> 3] >> (id & 7) & 1); }
static void item_set(int id, int on)
{
    if (id >= 0 && id < 256)
        items_held[id >> 3] = on ? items_held[id >> 3] | 1 << (id & 7) : items_held[id >> 3] & ~(1 << (id & 7));
}

// レオン（足元 leon_at、f32）がイベントの壁 w に触れている：その線から体の幅以内で、足元が
// その下端と上端の間（Check_EvHit：彼の球と三角形の判定）。
#define EV_WALL_REACH inttof32(3)
static int ev_wall_touched(const StageFoes *sf, int w, const int leon_at[3])
{
    if (w < 0 || w >= sf->walls)
        return 0;
    const s16 *l = sf->wall[w];
    if (leon_at[1] < inttof32(l[4] - 4) || leon_at[1] > inttof32(l[5]))
        return 0;
    s64 ax = inttof32(l[0]), az = inttof32(l[1]), dx = inttof32(l[2]) - ax, dz = inttof32(l[3]) - az;
    s64 px = leon_at[0] - ax, pz = leon_at[2] - az, len2 = dx * dx + dz * dz;
    s64 t = len2 ? (px * dx + pz * dz) * 4096 / len2 : 0;
    if (t < 0) t = 0;
    if (t > 4096) t = 4096;
    s64 ex = px - dx * t / 4096, ez = pz - dz * t / 4096;
    return ex * ex + ez * ez < (s64)EV_WALL_REACH * EV_WALL_REACH;
}

// レオンが床の上で (x, y, z)（ワールド単位）から reach（原作の単位：1単位 256）以内、同じ階で
// （イベント地点は床から 8-19 単位上。階どうしは30以上離れている）。
static int ev_near(int x, int y, int z, int reach, const int leon_at[3])
{
    if (reach <= 0)
        return 0;
    int dy = leon_at[1] - inttof32(y);
    if (dy < -inttof32(24) || dy > inttof32(8))   // （8面の鍵は額縁の中、床から19上）
        return 0;
    s64 dx = leon_at[0] - inttof32(x), dz = leon_at[2] - inttof32(z), r = (s64)reach * 16;   // （reach / 256 単位、f32）
    return dx * dx + dz * dz <= r * r;
}

static void ev_run(const StageFoes *sf, int i)
{
    const StageEvent *ev = &sf->event[i];
    ev_set(ev_played, i);
    for (int k = 0; k < ev->count; k++)
    {
        const StageOp *o = &sf->op[ev->first + k];
        switch (o->op)
        {
            case OP_ITEM:
                item_set(o->a, 1);
                if (ev_gots < 4)
                    ev_got[ev_gots++] = o->a;
                break;
            case OP_GIVE_ITEM: item_set(o->a, 0); break;
            case OP_FLAG: if (o->a >= 0 && o->a < 16) mis_flags = o->b ? mis_flags | 1 << o->a : mis_flags & ~(1 << o->a); break;
            case OP_DELETE: ev_set(ev_deleted, o->a); break;
            case OP_GROUP: if (o->a >= 0 && o->a < 16) foe_groups |= 1 << o->a; break;
            case OP_TIMER: mis_timer = 0; mis_timer_end = o->a * 60; break;
        }
    }
}

static int ev_cond(const StageFoes *sf, const StageEvent *ev, const int leon_at[3])
{
    switch (ev->cond)
    {
        case COND_NONE: case COND_DESTROYED: return 1;   // （壊された：下のその持ち主を参照）
        case COND_TIME: return mis_timer >= ev->value * 60 * 60;   // （値は分）
        case COND_KILLS: return ev->value ? ganados.kills >= ev->value : foe_groups_down();
        case COND_RECORD_DOWN: return foe_record_down(ev->value);
        case COND_EVENT: return ev_bit(ev_played, ev->value);
        case COND_FLAG: return ev->value < 16 && (mis_flags >> ev->value & 1);
        case COND_ITEM: return item_held(ev->value);
        case COND_WALL: return ev_wall_touched(sf, ev->value, leon_at);
    }
    return 0;
}

// ゲームが自分のきっかけで再生する場面（アシュリーを見つけた、5面。ガラドールに出会った、8面）：
// それを再生するイベントはコマンドだけを行う。
static int ev_scene_own(int stage, int scene)
{
    return (stage == 4 && scene == CUT_ASHLEY) || (stage == 7 && scene == CUT_GARRADOR);
}
// そういう場面の1つを再生した：それを再生するイベントは再生済みにする（コマンドも行う）。
static void foes_scene_played(const StageFoes *sf, int scene)
{
    for (int i = 0; i < sf->events && i < 64; i++)
        if (!ev_bit(ev_played, i) && (sf->event[i].script[0] == scene || sf->event[i].script[1] == scene))
            ev_run(sf, i);
}

// ステージ開始時のイベント（開始のコマンドと、グループ0の入場）。
static void foes_events_start(const StageFoes *sf, int stage)
{
    memset(ev_played, 0, sizeof(ev_played));
    memset(ev_deleted, 0, sizeof(ev_deleted));
    memset(ev_inside, 0, sizeof(ev_inside));
    memset(items_held, 0, sizeof(items_held));
    mis_flags = 0;
    mis_frames = 0;
    mis_timer = mis_timer_end = 0;
    if (stage == 11)   // （場面0は空：タイマーを始めるヴェルデューゴの場面は開始時に再生する）
        mis_timer_end = 240 * 60;
    ev_button = -1;
    ev_press = ev_scenes = ev_gots = 0;
    foe_groups = 1;   // （GM_CtrlMain_Enemy::Set_Start）
    // 12面は独自の終わり方（エレベーター）、1面は独自の流れ。
    mis_flag_clear = 0;
    for (int i = 0; stage != 11 && i < sf->events; i++)
        for (int k = 0; k < sf->event[i].count; k++)
            mis_flag_clear |= sf->op[sf->event[i].first + k].op == OP_FLAG && sf->op[sf->event[i].first + k].a == 0 && sf->op[sf->event[i].first + k].b;
    for (int i = 0; i < sf->events; i++)
        if (sf->event[i].ctg == EV_START)
            ev_run(sf, i);
    ev_gots = 0;
#ifdef TEST_HAVE_ITEM   // テスト用ビルド：最初からこのアイテムを持っている
    item_set(TEST_HAVE_ITEM, 1);
#endif
#ifdef TEST_ITEM   // テスト用ビルド：このアイテム（鍵、紋章）を最初から持っている
    item_set(TEST_ITEM, 1);
#endif
#ifdef TEST_EV_PLAYED   // テスト用ビルド：このイベントを再生済みにする（フック：そこまで歩かずに出口へ）
    ev_set(ev_played, TEST_EV_PLAYED);
#endif
}

// ミッションのイベントがくれるアイテム（item_base_info.bin：ID、カテゴリ、値段。名前は英語）：
// 鍵と紋章（カテゴリ5）、宝（6、値段で売れる：移植版のほかの宝と同じく、取った時点で）、
// ハーブ（2：移植版はグリーンだけ）、ファイル（7）。
enum { EVI_KEY, EVI_TREASURE, EVI_HERB, EVI_FILE };
typedef struct { s16 id; u8 kind; int price; const char *name; } EvItem;
static const EvItem ev_items[] = {
    { 40, EVI_HERB, 0, "Green Herb" }, { 41, EVI_HERB, 0, "Yellow Herb" }, { 42, EVI_HERB, 0, "Red Herb" },
    { 80, EVI_KEY, 0, "Emblem (Right Half)" }, { 81, EVI_KEY, 0, "Emblem (Left Half)" },
    { 82, EVI_KEY, 0, "Hexagonal Emblem" }, { 83, EVI_KEY, 0, "Insignia Key" }, { 84, EVI_KEY, 0, "Prison Key" },
    { 85, EVI_KEY, 0, "Castle Key" }, { 86, EVI_KEY, 0, "Old Key" },
    { 100, EVI_TREASURE, 2000, "Spinel" }, { 101, EVI_TREASURE, 3000, "Velvet Blue" },
    { 102, EVI_TREASURE, 10000, "Ruby" }, { 103, EVI_TREASURE, 10000, "Dirty Pocket Watch" },
    { 104, EVI_TREASURE, 10000, "Antique Pipe" }, { 105, EVI_TREASURE, 20000, "Pearl Bracelet" },
    { 106, EVI_TREASURE, 20000, "Amber Ring" }, { 107, EVI_TREASURE, 20000, "Gold Bangle" },
    { 108, EVI_TREASURE, 3000, "Elegant Mask" }, { 109, EVI_TREASURE, 3000, "Green Gem" },
    { 110, EVI_TREASURE, 3000, "Purple Gem" }, { 111, EVI_TREASURE, 3000, "Orange Gem" },
    { 119, EVI_TREASURE, 9000, "Crown" }, { 120, EVI_TREASURE, 11000, "Crown Jewel" },
    { 121, EVI_TREASURE, 13000, "Royal Insignia" },
    { 200, EVI_FILE, 0, "Game Manual 1" }, { 201, EVI_FILE, 0, "About Ashley" }, { 202, EVI_FILE, 0, "Game Manual 2" },
    { 203, EVI_FILE, 0, "Warning" }, { 204, EVI_FILE, 0, "Anonymous Letter" }, { 205, EVI_FILE, 0, "Closure of the Church" },
    { 206, EVI_FILE, 0, "Game Manual 3" }, { 207, EVI_FILE, 0, "Capture Luis Sera" }, { 208, EVI_FILE, 0, "Luis' Memo" },
    { 209, EVI_FILE, 0, "Luis' Memo 2" }, { 210, EVI_FILE, 0, "Notes on Grenades" }, { 211, EVI_FILE, 0, "Mine Thrower Manual" },
    { 212, EVI_FILE, 0, "Attache Case Manual" },
};
static const EvItem *ev_item(int id)
{
    for (unsigned i = 0; i < sizeof(ev_items) / sizeof(ev_items[0]); i++)
        if (ev_items[i].id == id)
            return &ev_items[i];
    return NULL;
}

// アシュリーがレオンのそば（GET_WITH_ASHLEY）：一緒にいて、近くて、同じ階。
#define EV_ASHLEY_NEAR inttof32(24)
static int ev_ashley_by(const int leon_at[3])
{
    return ash.on && !ash.wait && abs(ash.pos[0] - leon_at[0]) < EV_ASHLEY_NEAR && abs(ash.pos[2] - leon_at[2]) < EV_ASHLEY_NEAR
           && abs(ash.feet - leon_at[1]) < inttof32(12);
}

// 鍵のあるミッションの扉：そのそばのイベント地点に、アイテムを求めるイベントがある（5面の
// チャペルの扉は紋章、8面の地下牢の扉はその鍵）。そのイベントが起きるまで扉は閉じたままで、
// 「A: Open」も出さない（以前は鍵があってもなくても、イベントの範囲より1歩遠くから A で開いて
// いた）。起きたら（鍵を使ったら）扉が開く。
static void door_keys_update(const StageFoes *sf, const int leon_at[3])
{
    for (int i = 0; i < objs.count; i++)
    {
        Obj *o = &objs.o[i];
        if (o->kind != OBJ_SWING || o->locked)
            continue;
        int mx = o->hinge[0] - mulf32(cosLerp(o->yaw), inttof32(13) / 2), mz = o->hinge[1] + mulf32(sinLerp(o->yaw), inttof32(13) / 2);
        int keyed = 0;
        for (int e = 0; e < sf->events && e < 64 && !keyed; e++)
        {
            const StageEvent *ev = &sf->event[e];
            if (ev->ctg != EV_POINT || ev->cond != COND_ITEM || ev->trg < 0 || ev->trg >= sf->points
                || ev_bit(ev_played, e) || ev_bit(ev_deleted, e))
                continue;
            int px = inttof32(sf->point[ev->trg][0]), pz = inttof32(sf->point[ev->trg][2]);
            keyed = abs(px - mx) < inttof32(14) && abs(pz - mz) < inttof32(14);
        }
        if (o->keyed && !keyed)
            objects_open_door(&objs, i, leon_at);   // （鍵を回した：開く）
        o->keyed = keyed;
    }
}

static void foes_events(const StageFoes *sf, int stage, const int leon_at[3])
{
    door_keys_update(sf, leon_at);
    mis_frames++;
    {
        u32 dv = vblank_count - mis_timer_vb;   // （ポーズや場面をはさむ：最大0.5秒）
        mis_timer_vb = vblank_count;
        mis_timer += dv > 30 ? 1 : (int)dv;
        if (mis_timer > mis_timer_end)
            mis_timer = mis_timer_end;
    }
    for (int f = 0; f < foe_n; f++)
        if (foe_slot[f] >= 0)
            for (int k = 0; k < 3; k++)
                foe_at[f][k] = f32toint(k == 1 ? ganados.e[foe_slot[f]].feet : ganados.e[foe_slot[f]].pos[k]);
    int button = -1, pressed = ev_press, press_ctg = -1, press_trg = 0;
    ev_press = 0;
    for (int i = 0; i < sf->events && i < 64; i++)
    {
        const StageEvent *ev = &sf->event[i];
        if (ev_bit(ev_deleted, i) || ev->ctg == EV_START || ev->ctg == EV_END
            || (ev_bit(ev_played, i) && !(ev->again && ev->ctg == EV_POINT)))
            continue;
        int on = 1;
        if (ev->ctg == EV_RECORD)          // （敵が持っている：倒れて、レオンがその死体のところにいる）
        {
            on = foe_record_down(ev->trg);
            for (int f = 0; on && f < foe_n; f++)
                if (foe_list[f]->rec == ev->trg && ev->get >= GET_TOUCH)
                    on = ev_near(foe_at[f][0], foe_at[f][1] + 8, foe_at[f][2], ev->reach, leon_at);
        }
        else if (ev->ctg == EV_OBJECT)     // （物が持っている：壊れて、レオンがその場所にいる）
        {
            const Obj *o = ev->trg >= 0 && ev->trg < (int)sizeof(thing_obj) && thing_obj[ev->trg] >= 0 ? &objs.o[thing_obj[ev->trg]] : NULL;
            on = o && !o->alive && (ev->get < GET_TOUCH || ev_near(f32toint(o->pos[0]), f32toint(o->pos[1]) + 8, f32toint(o->pos[2]), ev->reach, leon_at));
        }
        else if (ev->ctg == EV_POINT)      // （イベント地点で）
        {
            on = ev->trg >= 0 && ev->trg < sf->points && ev_near(sf->point[ev->trg][0], sf->point[ev->trg][1], sf->point[ev->trg][2], ev->reach, leon_at);
            // 10面の地点4、アシュリーが鍵で開けるホールの扉（ホールの床から12上）は、その上のバルコニー
            // （80）からも届く：ホールからは上に戻る道が無い。
            if (!on && stage == ASH_STAGE_E && ev->trg == 4)
                on = ev_near(sf->point[4][0], sf->point[4][1] + 68, sf->point[4][2], ev->reach, leon_at);
        }
        // （中にいることは条件が成り立っている間だけ数える：レオンがすでに中にいるときに条件が成り立つ
        // 触れるイベント、たとえば小屋で2分たった6面のグループ2や、鍵の直後の10面の扉は、そのとき
        // 起きる。もう一度入ったときだけではない。）
        int was_inside = ev_bit(ev_inside, i);
        on = on && ev_cond(sf, ev, leon_at);
        if (on)
            ev_set(ev_inside, i);
        else
            ev_clr(ev_inside, i);
        if (!on)
            continue;
        int fire = 0;
        if (ev->get == GET_AUTO)
            fire = 1;
        else if (ev->get == GET_TOUCH)
            fire = !was_inside;           // （入ったとき）
        else if (ev->get == GET_BUTTON || ev_ashley_by(leon_at))
        {
            if (button < 0)
            {
                button = i;
                fire = pressed;
                if (fire)
                {
                    press_ctg = ev->ctg;
                    press_trg = ev->trg;
                }
            }
            else   // 同じ持ち主の、自分の場面が無い（コマンドだけの）ほかのイベントも
                   // 一緒に起きる：5面のはしごの下では、その場面を再生する押下でグループ4が入る（押下1回に
                   // イベント1つだと、それは決して来なかった）。持ち主のほかのせりふ（鍵のかかった扉のせりふ、
                   // 鍵のせりふの横）は順番を待つ
                fire = ev->ctg == press_ctg && ev->trg == press_trg
                       && !(ev->script[0] >= 0 && cut_find(stage, ev->script[0])) && !(ev->script[1] >= 0 && cut_find(stage, ev->script[1]));
        }
        if (!fire)
            continue;
        ev_run(sf, i);
        for (int k = 0; k < 2; k++)
            if (ev->script[k] >= 0 && ev_scenes < 4 && !ev_scene_own(stage, ev->script[k]) && cut_find(stage, ev->script[k]))
                ev_scene[ev_scenes++] = ev->script[k];
        if (button == i)
            pressed = 0;   // （押下1回にイベント1つ）
    }
    ev_button = button;
}
// まだ入場していないグループが自分から来るか（自分の条件だけのイベントで、倒すこと、時間、
// 入場したレコードがまだ起こせるもの）。レオンを待つグループ（どこかへ行く、何かを壊す）は
// ステージのクリアを止めない。
static int foes_more_coming(const StageFoes *sf)
{
    int could = ganados.kills;
    for (int f = 0; f < foe_n; f++)
        if (foe_groups >> foe_list[f]->wave & 1)
            could += foe_left[f] + (foe_slot[f] >= 0 && ganados.e[foe_slot[f]].state < ENEMY_DYING);
    for (int i = 0; i < sf->events && i < 64; i++)
    {
        const StageEvent *ev = &sf->event[i];
        if (ev->ctg != EV_AUTO || ev_bit(ev_played, i) || ev_bit(ev_deleted, i))
            continue;
        int lets_in = 0;
        for (int k = 0; k < ev->count; k++)
            lets_in |= sf->op[ev->first + k].op == OP_GROUP && !(foe_groups >> sf->op[ev->first + k].a & 1);
        if (!lets_in)
            continue;
        if (ev->cond == COND_TIME || (ev->cond == COND_KILLS && ev->value <= could))
            return 1;
        if (ev->cond == COND_RECORD_DOWN)
            for (int f = 0; f < foe_n; f++)
                if (foe_list[f]->rec == ev->value && (foe_groups >> foe_list[f]->wave & 1))
                    return 1;
    }
    return 0;
}

// ミッションの敵が片付いた：何人か来て、入場した者は全員倒れて動かず、ほかのグループが自分から
// 来ることもない。
static int foes_finished(const StageFoes *sf)
{
    if (!foe_entered)
        return foe_n == 0;
    for (int f = 0; f < foe_n; f++)
        if ((foe_groups >> foe_list[f]->wave & 1) && (foe_left[f] > 0 || (foe_slot[f] >= 0 && ganados.e[foe_slot[f]].state != ENEMY_DEAD)))
            return 0;
    return !foes_more_coming(sf);
}

// 地点が遠い、別の階、あるいはレオンの今いる所から歩いて行けない所にある敵（5面のチャペルの
// 回廊の上、10面の彼のバルコニーの下のホール、9面の長い廊下の先）は決して来ず、ステージが
// クリアできなかった。そういう敵は代わりに彼の近くに、もう彼を追っている状態で出てくる。
// （これは 2026-11-08 のベータにあり、2026-11-10 にミッションのグループのために出現を作り直した
// とき失われた。2026-11-11 に戻した。）
#define FOE_FAR       inttof32(110)
#define FOE_STEP      inttof32(3)            // （STEP_UP、もっと下で定義）
#define FOE_LEVEL     inttof32(16)
// 場所：レオンの足元から床の上を FOE_CELL 刻みで塗りつぶす（上下は最大1段、壁は通らない）ので、
// 廊下や10面の狭いバルコニーに沿う。歩いて FOE_NEAR_MIN..FOE_NEAR_MAX 離れた場所のうち、
// すでに立っている者から一番遠い所を取る（群れでなく、まわりじゅうから来る）。同じなら敵の番号で
// 選ぶ。
#define FOE_CELL      inttof32(4)
#define FOE_BODY      floattof32(2.2)        // （レオンのは2.5：彼が収まる所）
#define FOE_GRID      101                    // 一辺のマス数（真ん中にレオン）
#define FOE_NEAR_MIN  12                     // 歩いた距離のマス数
#define FOE_NEAR_MAX  45
static u8 foe_dist[FOE_GRID * FOE_GRID];
static s32 foe_fy[FOE_GRID * FOE_GRID];     // 床の高さ（f32：整数の単位だと坂が失われる）
static u8 foe_shut[FOE_GRID * FOE_GRID];    // 閉じた扉を通ってしか届かない（または物の向こう）
static int foe_cx, foe_cz;                  // 塗りつぶしの中心（f32 のワールド座標）：レオンがいた所
static int foe_tries, foe_moves, foe_cells, foe_y0;   // （テスト用の表示）
// 塗りつぶし本体：レオンの足元から床の上を、1段ずつ上下しながら、壁は通らずに（体の幅は
// だいたいレオンと同じ：城の戸口は通れ、柵のすき間は通れない）。
static void foe_flood(const int leon_at[3])
{
    static u16 queue[FOE_GRID * FOE_GRID];
    static u16 held[256];   // （閉じた扉 / 物を通る歩み。ほかの後で取る）
    int nheld = 0;
    memset(foe_dist, 0xff, sizeof(foe_dist));
    memset(foe_shut, 0, sizeof(foe_shut));
    foe_cx = leon_at[0];
    foe_cz = leon_at[2];
    // （足元の床：ミッションの開始の高さは床より少し上か下のことがある）
    int y0 = leon_at[1];
    ground_height(&stage_ground, leon_at[0], leon_at[2], leon_at[1] + inttof32(16), 0, &y0);
    int c0 = (FOE_GRID / 2) * FOE_GRID + FOE_GRID / 2, head = 0, tail = 0;
    foe_dist[c0] = 0;
    foe_fy[c0] = y0;
    queue[tail++] = c0;
    for (int pass = 0; pass < 2; pass++)
    {
        if (pass)   // 扉を通って：その奥にも届くが、印を付ける
            for (int h = 0; h < nheld; h++)
            {
                int c = held[h] >> 2, k = held[h] & 3, cx = c % FOE_GRID, cz = c / FOE_GRID;
                int n = (cz + (k == 2) - (k == 3)) * FOE_GRID + cx + (k == 0) - (k == 1), y;
                if (foe_dist[n] != 0xff)
                    continue;
                int px = foe_cx + (n % FOE_GRID - FOE_GRID / 2) * FOE_CELL, pz = foe_cz + (n / FOE_GRID - FOE_GRID / 2) * FOE_CELL;
                ground_height(&stage_ground, px, pz, foe_fy[c] + FOE_STEP, FOE_STEP * 2, &y);
                foe_dist[n] = foe_dist[c] + 1;
                foe_fy[n] = y;
                foe_shut[n] = 1;
                queue[tail++] = n;
            }
    while (head < tail)
    {
        int c = queue[head++], cx = c % FOE_GRID, cz = c / FOE_GRID;
        if (foe_dist[c] >= FOE_NEAR_MAX)
            continue;
        int x = foe_cx + (cx - FOE_GRID / 2) * FOE_CELL, z = foe_cz + (cz - FOE_GRID / 2) * FOE_CELL;
        for (int k = 0; k < 4; k++)
        {
            int nx = cx + (k == 0) - (k == 1), nz = cz + (k == 2) - (k == 3);
            if (nx < 0 || nz < 0 || nx >= FOE_GRID || nz >= FOE_GRID)
                continue;
            int n = nz * FOE_GRID + nx;
            if (foe_dist[n] != 0xff)
                continue;
            int px = x + (nx - cx) * FOE_CELL, pz = z + (nz - cz) * FOE_CELL, y;
            int from = foe_fy[c];
            if (!ground_height(&stage_ground, px, pz, from + FOE_STEP, FOE_STEP * 2, &y) || y < from - FOE_STEP || y > from + FOE_STEP)
                continue;
            // 新しい場所に壁が無く、前の場所との間にも無い（4離れた2つの場所の間の薄い壁は、
            // 中間点から2以内にある：これが無いと塗りつぶしが野原の壁を越えて、敵が壁の外に
            // 出てきた）。
            int qx = px, qz = pz, mx = (x + px) / 2, mz = (z + pz) / 2;
            if (ground_collide_n(&stage_ground, &qx, &qz, y + FOE_STEP, y + inttof32(16), FOE_BODY, 1)
                || ground_collide_n(&stage_ground, &mx, &mz, y + FOE_STEP, y + inttof32(16), FOE_BODY, 1))
                continue;
            // 閉じた扉（または木箱、樽）が途中にある：その奥にも届く（地点がそこにある敵は回り込んで、
            // 扉を開けて来る）が、最後に回し印を付ける：そこには敵を出さない。（5面のアシュリーが待つ
            // 扉の奥の部屋は、ほかから一番遠い場所だったので敵がそこに出ていた。）
            if (!foe_shut[c] && !pass)
            {
                int ox = px, oz = pz, ox2 = mx, oz2 = mz;
                objects_collide(&objs, &ox, &oz, y, FOE_BODY);
                objects_collide(&objs, &ox2, &oz2, y, FOE_BODY);
                if (ox != px || oz != pz || ox2 != mx || oz2 != mz)
                {
                    if (nheld < (int)(sizeof(held) / sizeof(held[0])))
                        held[nheld++] = (u16)(c << 2 | k);
                    continue;
                }
            }
            foe_dist[n] = foe_dist[c] + 1;
            foe_fy[n] = y;
            foe_shut[n] = foe_shut[c];
            queue[tail++] = n;
        }
    }
    }
    foe_cells = tail;
    foe_y0 = f32toint(y0);
}

// 塗りつぶしが (x, z) に、だいたい高さ y（ワールド単位）で届いたか：野原の外（壁の向こう、
// すき間の向こう）のミッション地点は届かない。
static int foe_reachable(int x, int y, int z)
{
    int cx = (inttof32(x) - foe_cx + FOE_CELL / 2) / FOE_CELL + FOE_GRID / 2 - (inttof32(x) < foe_cx ? 1 : 0);
    int cz = (inttof32(z) - foe_cz + FOE_CELL / 2) / FOE_CELL + FOE_GRID / 2 - (inttof32(z) < foe_cz ? 1 : 0);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++)
        {
            int nx = cx + dx, nz = cz + dz;
            if (nx < 0 || nz < 0 || nx >= FOE_GRID || nz >= FOE_GRID)
                continue;
            int n = nz * FOE_GRID + nx, d = f32toint(foe_fy[n]) - y;
            if (foe_dist[n] != 0xff && d < 10 && d > -10)
                return 1;
        }
    return 0;
}

// 塗りつぶしから敵を出す場所（上を参照）。無ければ0を返す。
static int foe_near(int seed, int out[3])
{
    foe_tries++;
    int best = -1, best_score = -1;
    for (int c = 0; c < FOE_GRID * FOE_GRID; c++)
    {
        if (foe_dist[c] == 0xff || foe_dist[c] < FOE_NEAR_MIN || foe_shut[c])
            continue;
        int x = foe_cx + (c % FOE_GRID - FOE_GRID / 2) * FOE_CELL, z = foe_cz + (c / FOE_GRID - FOE_GRID / 2) * FOE_CELL;
        int near = 200;
        for (int i = 0; i < ganados.count; i++)
        {
            const Enemy *e = &ganados.e[i];
            if (!e->active || e->state >= ENEMY_DYING)
                continue;
            int dx = f32toint(e->pos[0] - x), dz = f32toint(e->pos[2] - z);
            int d = (dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz);
            if (d < near)
                near = d;
        }
        int score = near * 64 + ((c * 31 + seed * 17) & 63);
        if (score > best_score)
        {
            best_score = score;
            best = c;
        }
    }
    if (best < 0)
        return 0;
    foe_moves++;
    out[0] = f32toint(foe_cx) + (best % FOE_GRID - FOE_GRID / 2) * f32toint(FOE_CELL);
    out[1] = f32toint(foe_fy[best]) + 2;
    out[2] = f32toint(foe_cz) + (best / FOE_GRID - FOE_GRID / 2) * f32toint(FOE_CELL);
    return 1;
}
static void foes_step(const int leon_at[3])
{
    // 敵がいなくなったユニット（死体が片付いた。投げる者は死んで倒れたら）は空きになる。
    for (int s = 0; s < ganados.count; s++)
    {
        int f = slot_foe[s];
        if (f >= 0 && ganados.e[s].state == ENEMY_DEAD && (!ganados.e[s].active || ganados.e[s].thrower))
        {
            slot_foe[s] = -1;
            foe_slot[f] = -1;
        }
    }
    int units = 0, flooded = 0;   // （塗りつぶしは1回の呼び出しにつき1回、敵が最初に必要としたときに作る）
    for (int s = 0; s < ganados.count; s++)
        units += slot_foe[s] >= 0;
    while (units < FOE_UNITS)
    {
        int f = 0;
        while (f < foe_n && !((foe_groups >> foe_list[f]->wave & 1) && foe_left[f] > 0 && foe_slot[f] < 0))
            f++;
        if (f >= foe_n)
            return;
        const StageFoe *r = foe_list[f];
        int slot = foe_fixed[f];
        if (slot < 0)
            for (int s = 0; s < foe_slots && slot < 0; s++)
                if (slot_foe[s] < 0 && !ganados.e[s].active)
                    slot = s;
        if (slot < 0)
            return;   // （まだ死体は1つも消えていない）
        int brought = 0;   // （レオンの近くに、もう彼を追っている状態で出る）
        if (foe_fixed[f] < 0)
        {
            int ldx = inttof32(r->pos[0]) - leon_at[0], ldz = inttof32(r->pos[2]) - leon_at[2];
            int ldy = inttof32(r->pos[1]) - leon_at[1];   // （彼が立っているバルコニーの下の床：問題ない）
            if ((s64)ldx * ldx + (s64)ldz * ldz < (s64)FOE_CLEAR * FOE_CLEAR && ldy < inttof32(20) && ldy > -inttof32(20)
                && ++foe_held < FOE_WAIT)
                return;
            foe_held = 0;
            // 地点にまだ別の者がいる（地点を共有するレコード）：横に並べる。
            int xyz[3] = { r->pos[0], r->pos[1], r->pos[2] };
            // 地点が遠い、別の階、あるいはレオンの今いる所から歩いて行けない所（野原の外：壁の向こう、
            // すき間の向こう）：代わりに彼の近くに出る。
            if (!flooded)
            {
                foe_flood(leon_at);
                flooded = 1;
            }
            int pfy = inttof32(r->pos[1]);
            ground_height(&stage_ground, inttof32(xyz[0]), inttof32(xyz[2]), inttof32(r->pos[1] + 6), FOE_STEP * 2, &pfy);
            int fdy = inttof32(r->pos[1] - 4) - leon_at[1];
            brought = ((s64)ldx * ldx + (s64)ldz * ldz > (s64)FOE_FAR * FOE_FAR || fdy > FOE_LEVEL || fdy < -FOE_LEVEL
                       || !foe_reachable(xyz[0], f32toint(pfy), xyz[2]))
                      && foe_near(f, xyz);
            for (int k = 0; k < 4 && !brought; k++)
            {
                int busy = 0;
                for (int s = 0; s < foe_slots; s++)
                    busy |= slot_foe[s] >= 0 && abs(ganados.e[s].pos[0] - inttof32(xyz[0])) < inttof32(4)
                            && abs(ganados.e[s].pos[2] - inttof32(xyz[2])) < inttof32(4);
                if (!busy)
                    break;
                xyz[0] = r->pos[0] + (k & 1 ? -6 : 6) * (k / 2 + 1);
            }
            int yaw = r->dir * (DEGREES_IN_CIRCLE / 256);
            if (brought)   // （彼の方を向いて）
                yaw = (int)(atan2f((float)(leon_at[0] - inttof32(xyz[0])), (float)(leon_at[2] - inttof32(xyz[2]))) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
            enemies_respawn(&ganados, slot, xyz, yaw);
            ganados.e[slot].drop = r->drop;
            enemies_set_kind(&ganados, slot, foe_kind_of(r));
#ifdef TEST_DROP_CODE   // テスト用ビルド：全員がこのドロップコードのアイテムを落とす
            ganados.e[slot].drop = TEST_DROP_CODE;
#endif
        }
        enemies_activate(&ganados, slot, NULL);
        // 最初の AI モード（GM_CtrlSub_ChAi::Set_Start：Change_AiMode(FstAiId)）：6と7
        // （_AiMode_Def_Search）はすぐにレオンを標的にする。ほかは彼に気づくまで待つ。
        if (r->ai == 6 || r->ai == 7 || brought)
            ganados.e[slot].spotted = 1;
        foe_left[f]--;
        foe_slot[f] = slot;
        slot_foe[slot] = f;
        foe_entered++;
        units++;
    }
}


// 1面はガナードを自分で置く（ganado_spawn）：n人目は、ミッションファイルの n 番目のガナードの
// レコードの種類になる（そこでは体力 400 か 480）。
static void stage1_kinds(void)
{
    const StageFoes *sf = &stage_foes[0];
    int n = 0;
    for (int i = 0; i < sf->count && n < ganados.count; i++)
        if (sf->foe[i].type == FOE_GANADO)
        {
            ganados.e[n].drop = sf->foe[i].drop;   // （落とすもの：そのレコードのドロップコード）
            enemies_set_kind(&ganados, n++, foe_kind_of(&sf->foe[i]));
        }
}

// その種類の n 体目の敵（ボス：FOE_GARRADOR、FOE_GIANT、FOE_VERDUGO）、なければ NULL。
static const StageFoe *stage_boss(int stage, int type, int n)
{
    const StageFoes *sf = &stage_foes[stage];
    for (int i = 0; (FOES_ON(stage) || type == FOE_GIANT) && i < sf->count; i++)   // （巨人はもう戻っている）
        if (sf->foe[i].type == type && n-- == 0)
            return &sf->foe[i];
    return NULL;
}
static int stage_bosses(int stage, int type)
{
    int n = 0;
    while (stage_boss(stage, type, n))
        n++;
    return n;
}
// レオンは原作と同じ場所から始まる：村の南西の丘の道で、南の門の方を向いて（mis_data_s01.bin）。
#define STAGE_START_X 19.5f
#define STAGE_START_Z 781.2f
#define STAGE_START_YAW 138               // 度
static int leon_pose[MODEL_MAX_NODES][12];
static NodeLocal pose_base[MODEL_MAX_NODES], pose_aim[MODEL_MAX_NODES], pose_tilt[MODEL_MAX_NODES];
static NodeLocal pose_prev[MODEL_MAX_NODES], pose_from[MODEL_MAX_NODES];
static NodeLocal pose_fx[MODEL_MAX_NODES], pose_fx0[MODEL_MAX_NODES];

// leon_anim.bin のアニメーションの順（re4zeebo.ps1 -DsAnim ...#22,23,24,11,20,21,13,3,10,8,16,0,1,2）：
// 待機（1ポーズ）、歩き、走り、次に反応：ひるみ（#11）、ガナードにつかまれる（#20、ループ）、
// 振りほどき（#21）、倒れて死ぬ（#13）。そしてステージの動作：はしごを登る（#3：32単位上へ、
// 16後ろへ）、蹴り（#10）、段差から降りる（#8）、着地（#16）。最後に負傷時の待機、歩き、走り
// （#0、#1、#2）。原作のモーション表（ch_param_0 のモーション1-12）は、体力が1200のうち800以上の
// とき #22-24（条件12、HpMore 800）、それ未満で #0-2（条件11、HpUnder 799）を再生する。後退は
// 歩きの逆再生。ANIM_OFF はフリーカメラのモード（レオン非表示）で、クリップではない。
enum { ANIM_IDLE, ANIM_WALK, ANIM_RUN, ANIM_OFF };
enum { CLIP_FLINCH = 3, CLIP_HELD, CLIP_BREAK_FREE, CLIP_DEATH, CLIP_CLIMB, CLIP_KICK, CLIP_STEP_OFF, CLIP_LAND, CLIP_HURT_IDLE };
#define LEON_HURT_BELOW 67              // この体力未満で負傷時の動き：原作の1200のうち800
#define KICK_HIT      inttof32(18)      // CLIP_KICK で足が当たるフレーム
#define CLIMB_BACK    inttof32(16)      // CLIP_CLIMB で彼がこれだけ後ろへ…
#define CLIMB_UP      inttof32(32)      // …そして上へ
#define DROP_MIN      inttof32(8)       // これより高い段差から歩いて出ると：落ちる
#define DROP_GRAVITY  floattof32(0.06)
#define LAND_FRAMES   inttof32(24)      // 再生する CLIP_LAND の部分
// 樽の爆発（GM_Hdl_Fix_Normal::_Set_Exp、サイズ3：Set_AllRangeAtk は 7680 / 15360 まで、s32/256：
// 30と60単位、威力は近くも遠くも1600で、どのガナードや教徒の体力より多い。サイズ5、吊るされた
// 容器のもの：10 / 20、1200 / 1000、HANG_BLAST を参照）：この近さのガナードを倒し、内側半分の
// 中のレオンを傷つける（移植版の BLAST_DAMAGE）。（移植版は以前24を使っていた。）
#define BLAST_RANGE   inttof32(60)
#define BLAST_DAMAGE  30
#define HANG_BLAST    inttof32(20)      // 吊るされた容器の爆発（0x4a688 サイズ5）
#define MEDAL_REWARD  20                // メダルを全部撃つための弾数

// レオンの体力と、ガナードの攻撃への反応。
#define LEON_HP       100
// 一撃、原作のデータから（ch_param_N.bin のアニメーションスクリプト、攻撃コマンド103の7番目の値。
// 当たりはそれに、受ける者の部位ごとのダメージ%を掛ける：レオンは100、アシュリーは50、体力は彼の
// 半分）。ガナード：手斧（スクリプト62）と農具のフォーク（75）はレオンの1200のうち380。素手では
// 殴らず、つかんで噛む（enemy.c）。教徒：鎌（スクリプト65）は800。（原作の難しい難易度の480は
// Zeebo のものではない。重いガナードの800、カテゴリ19は、ここにモデルが無い。）Android 版の
// ライブラリで読んだ（docs/NOTES.md）。
#define SWIPE_DAMAGE  32                // 380 / 1200
#define CULTIST_DAMAGE 67               // 800 / 1200
#define IS_CULTIST(e) ((e)->kind >= foe_kind_cultist && (e)->kind < foe_kind_cultist + foe_kind_cultist_count)
// （enemies_blow()：攻撃自身の値、レオンの1200のうち。380 -> 32、800 -> 67、カテゴリ15の480 -> 40）
#define HIT_DAMAGE(e) ((enemies_blow(e) * 100 + 600) / 1200)
// ガナードと教徒へのダメージは原作の単位（enemy.h：ハンドガンの胴への当たりが100、ガナードは
// 400-800）。蹴りとナイフは原作の値。振りほどきは移植版独自。
#define KICK_DAMAGE   200               // ふらついたガナード（とその隣の者）へのレオンの蹴り：原作の200（ch_param_0 のスクリプト31と36、攻撃コマンド103）
#define ENEMY_FELL    30000             // 爆発（またはテスト用ビルド）は届いた者を倒す
#define BITE_DAMAGE   4                 // 噛みつき（つかみのループ1回に2回）：原作の1200のうち50
#define GRAB_DIST     floattof32(6.5)   // つかまれている間のガナードからレオンへ（ガナードの手が彼に）
// つかまれた：8回目の押下で振りほどく（GM_CtrlMain_Player::_State_HoldDamage はアクションボタンを
// 数え、7を超えると放す）。それまでガナードは噛みながらつかみ続け（enemy.c）、無傷で突き放される
// （GM_Hdl_ChBase::Main_State_HoldAttack）。
#define GRAB_MASH     8
// 反応の後、この間は攻撃が当たらない。原作には無い：レオンの反応モーション（ch_param_0 の
// モーションタイプ27-39と53）は、その最初のフレームから（スクリプトコマンド118、
// GM_Ctrl_ObjScriptCh::_Cmd_Ch_SetMuteki）終わるまで無敵にし、それ以上ではない。
#define SAFE_FRAMES   0
#define MUSIC_VOLUME  80                // BGM、効果音の下に
#define KICK_FRAME    inttof32(16)     // CLIP_BREAK_FREE で彼の蹴りが当たるフレーム
#define DEAD_TO_MENU  150              // 倒れてからゲームオーバーのメニューまでのフレーム数
#define MENU_DELAY    60                // …これだけの間は入力を受け付けない
enum { REACT_NONE, REACT_FLINCH, REACT_HELD, REACT_BREAK_FREE, REACT_DEAD,
       REACT_KICK, REACT_CLIMB_UP, REACT_CLIMB_DOWN, REACT_DROP, REACT_LAND, REACT_GIANT_UP, REACT_GIANT_CUT };

enum { MODE_TITLE, MODE_HOWTO, MODE_SELECT, MODE_PLAY, MODE_PAUSE, MODE_GAMEOVER, MODE_RESULT, MODE_STATUS, MODE_SHOP, MODE_CUTIN, MODE_MESSAGE };
#define HERBS_MAX     9

// 弾とアイテム。
#ifndef SPARE_START
#define SPARE_START   20                // 開始時にマガジンとは別に持っている弾数
#endif
#define HERB_HEAL     50                // グリーンハーブで戻る体力：原作のレオンの1200のうち600
                                        // （item_healing_info.bin の最初のエントリー。0x10d58：1200 + イエローハーブで120）
#define DROP_PERCENT  100               // （未使用：ガナードが落とすものはそのドロップコードのもの）
#define DROP_DELAY    40                // …倒れたら見える

// ステージクリア：ガナードが全員死んで倒れたら、教会の鐘が3回鳴り（BGM はフェードアウト）、
// STAGE CLEAR が出て、それから結果。
#define BELL_FIRST    30                // 最後の1人が倒れてからのフレーム数
#define BELL_GAP      96
#define CLEAR_TEXT_AT 60
#define CLEAR_END     330
#define RESULT_STEP   24                // 結果の各行が出る間隔のフレーム数
static const u8 react_clip[] = { 0, CLIP_FLINCH, CLIP_HELD, CLIP_BREAK_FREE, CLIP_DEATH,
                                 CLIP_KICK, CLIP_CLIMB, CLIP_CLIMB, CLIP_STEP_OFF, CLIP_LAND, CLIP_CLIMB, CLIP_CLIMB };
// ミッションのストーリー場面（cutins.h：ミッションファイルのスクリプト、その絵とせりふ）：専用の画面で
// （MODE_CUTIN）、その間ゲームは止まる。1ページは黒の上の絵（上に 256 x 128）と、その下の文と
// 話す人。A でページをめくり（CUT_AUTO の後は自動でめくる）、START で残りを飛ばす。再生するのは：
// 各ミッションの始まりとクリア、そしてイベント（チェーンソー男、エルヒガンテがひざをつきレオンが
// 背中に乗る、アシュリー発見、ガラドール、ヴェルデューゴ）。
#ifndef CUT_AUTO   // （テスト用ビルド：短く）
#define CUT_AUTO      (60 * 8)
#endif
#define GAL_CUT_NEAR  inttof32(60)      // ガラドールの場面：レオンが彼にこれだけ近い
enum { CUT_AFTER_NONE, CUT_AFTER_GIANT, CUT_AFTER_JUMP };

// すき間を跳ぶ（Zeebo 版の「Acao: Pular」）：5面のシャンデリア。北の回廊の手すりから身廊を越えて
// 南の回廊へ（mis_data_s05 の場面17：その絵とせりふ、その後レオンはステージの地点13に）、そして
// 戻り。その場所に立ち（JUMP_NEAR 以内、同じ床）、その向き（約50度以内）を向いて A で、場面を
// 再生して向こうに着地する。
typedef struct { s8 stage; s16 x, z, y, yaw, to_x, to_z, to_y, to_yaw; } JumpSpot;
static const JumpSpot jumps[] = {
    { 4, 64, 18, 46, 0, 64, 155, 46, 0 },
    { 4, 64, 150, 46, 180, 64, 17, 46, 180 },
};
#define JUMP_NEAR inttof32(7)
static const JumpSpot *jump_now;   // （作っている途中のもの）
static const JumpSpot *jump_at_hand(int stage, const int pos[3], int feet, int yaw)
{
    for (unsigned i = 0; i < sizeof(jumps) / sizeof(jumps[0]); i++)
    {
        const JumpSpot *j = &jumps[i];
        int dx = pos[0] - inttof32(j->x), dz = pos[2] - inttof32(j->z), dy = feet - inttof32(j->y);
        int face = (int)(((s64)sinLerp(yaw) * sinLerp(degreesToAngle(j->yaw)) + (s64)cosLerp(yaw) * cosLerp(degreesToAngle(j->yaw))) >> 12);
        if (j->stage == stage && (s64)dx * dx + (s64)dz * dz < (s64)JUMP_NEAR * JUMP_NEAR && dy < inttof32(6) && dy > -inttof32(6)
            && face > floattof32(0.64))
            return j;
    }
    return NULL;
}
static const CutScene *cut;
static int cut_page, cut_t, cut_then, cut_after, cut_fade, cut_pic_id = -1;
static const char *const cut_who[] = { "", "Leon", "Ashley", "Luis", "Saddler" };

// ステージの場面を準備する（無い、または文が無い：0）。呼び出し側が MODE_CUTIN に切り替える
// （フェード付き：fade = 1、始まりかクリアで）。終わると then へ（MODE_PLAY：元どおり）。
static int cut_start(int stage, int scene, int then, int after, int fade)
{
    const CutScene *c = cut_find(stage, scene);
    if (!c || !c->count)
        return 0;
    cut = c;
    cut_page = cut_t = 0;
    cut_then = then;
    cut_after = after;
    cut_fade = fade;
    return 1;
}

// ひざをついた巨人の背中に乗る（Zeebo 版：Acao: Escalar）。原作にはレオンのその動きの
// アニメーションが無い：イベントで、その場面（mis_data_s04 / s11 の場面11：エルヒガンテの上の
// レオンの絵と2行のせりふ）。その後レオンは彼の背後で、彼の方を向いている。
#define GIANT_BACK    inttof32(8)       // 登り終えると巨人の中心からこれだけ後ろ
#define GIANT_CUT_HIT inttof32(6)       // 斬りで刃が当たるフレーム
// leon_gun.bin：ch_leon_w01.mcas のハンドガンのアニメーション #0（構え）、#1（歩き）、#2（走り）、
// #8（抜く：構えで終わる）、狙いの水平、上と下（エクスポーターが #6 と #4 から作る。1回転離れて
// いる：中間、90%、10%、各ノードは短い方へ回す）、#5（撃つ、上向き）、#7（撃つ、下向き）、
// #3（リロード）
enum { GUN_READY_POSE, GUN_WALK, GUN_RUN, GUN_DRAW, AIM_FORWARD, AIM_UP, AIM_DOWN,
       FIRE_UP, FIRE_DOWN, GUN_RELOAD };
#define DRAW_FRAMES leon_gun.frames[GUN_DRAW]
#define FIRE_FRAMES leon_gun.frames[FIRE_UP]
#define RELOAD_FRAMES leon_gun.frames[GUN_RELOAD]
#define MAG_SIZE    wep_mag(wep)
// 銃。アイテム、値段、ケースの大きさは item_base_info.bin のもの。火力（x 10）と装弾数は
// item_tuneup_info.bin と item_weapon_info.bin の最初のレベル。箱と弾の値段（1発あたり）は
// item_ammo_info.bin と item_base_info.bin のもの。連射間隔は次の射撃までの原作のティック（アニメの
// フレーム、1秒に20）：item_weapon_info.bin の ReactionSpeed（30バイトのレコードの +14、
// Bio4_WeaponNode::GetReactionSpeed：ハンドガン10、マグナム14、ショットガン30、ライフル40、TMP 2、
// ロケットランチャー47）。モデルとアニメーション（eqp_wep_all #N、ch_leon_w0N、クリップの順は
// ハンドガンと同じ）は装備したときに NitroFS から読む。
typedef struct {
    const char *name, *model, *anim;
    u8 item, mag, refire, tam, box;
    u16 power, ammo_price;
    int price, muzzle[3];
    u8 special;            // WS_SCOPE：構えると視点がスコープになる。WS_ROCKET：ロケット1発、撃つと無くなる
} Weapon;
enum { WS_NONE, WS_SCOPE, WS_ROCKET };
// （銃口、銃自身の座標で：eqp_wep_all のその銃の骨格の銃身の先のノード。）
#define MUZZLE(x, y, z) { floattof32(x), floattof32(y), floattof32(z) }
static const Weapon weapons[] = {
    { T_ITEM_GUN, NULL, NULL, 4, 10, 10, 6, 50, 10, 50, 8000, MUZZLE(-2.92, -0.24, 0.84) },
    { T_ITEM_MAGNUM, "nitro:/gun5.bin", "nitro:/gun5_anim.bin", 5, 6, 14, 8, 10, 130, 500, 38000, MUZZLE(-4.47, -0.26, 1.20) },
    { T_ITEM_SHOTGUN, "nitro:/gun2.bin", "nitro:/gun2_anim.bin", 6, 6, 30, 16, 15, 40, 120, 20000, MUZZLE(-8.30, -0.18, 0.60) },
    { T_ITEM_RIFLE, "nitro:/gun3.bin", "nitro:/gun3_anim.bin", 7, 5, 40, 14, 10, 50, 150, 20000, MUZZLE(-7.79, -0.21, 1.00), WS_SCOPE },
    { T_ITEM_TMP, "nitro:/gun4.bin", "nitro:/gun4_anim.bin", 8, 30, 2, 10, 100, 4, 20, 14000, MUZZLE(-2.96, -0.24, 0.80) },
    { T_ITEM_ROCKET, "nitro:/gun6.bin", "nitro:/gun6_anim.bin", 9, 1, 47, 16, 0, 0, 0, 30000, MUZZLE(-6.31, 0.06, 0.50), WS_ROCKET },
};
#define WEP_LINE 18                      // （プレイ中ほかに使っていない文字の行）
// 多機能弾丸：持っている弾、ロケットランチャー以外のどの銃にも共通の1つの数（ロケットは別）。
// その1箱：MULTI_BOX 発（ハンドガンの箱：店では50発で2500、ケースに2つ）。落ちているもの：
// MULTI_DROP（ハンドガンの10）。
#define MULTI_BOX     50
#define MULTI_DROP    10
#define USES_ROUNDS(w) (weapons[w].box > 0)
enum { WEP_HANDGUN, WEP_MAGNUM, WEP_SHOTGUN, WEP_RIFLE, WEP_TMP, WEP_ROCKET, WEP_COUNT };
#define SCOPE_EYE     inttof32(15)       // （彼の目：足元からこれだけ上）
#define ROCKET_SPEED  inttof32(5)        // ロケットは1フレームにこれだけ飛ぶ…
#define ROCKET_BLAST  inttof32(30)       // …その爆発はこの範囲の全員を倒す（エルヒガンテも）
#define SCOPE_FOV     20                 // ライフルのスコープ：横の度数（通常の視点は70）
#define SCOPE_R       26                 // 画面上のそのレンズ、ピクセル（動画のもの：高さの7分の1）
#define SCOPE_PITCH   degreesToAngle(50) // いっぱいの狙いの傾き：これだけ上か下
#define SHOTGUN_RANGE inttof32(40)       // ショットガンの広がり：この範囲のほかの者にも当たる
#define SHOTGUN_CONE  floattof32(0.95)   // （半角のコサイン）
static int wep;                          // 手に持っている武器
static int wep_ammo[WEP_COUNT];   // （各銃のマガジン。持っている弾は1つの数、多機能弾丸）
// ミッション開始時にレオンが持っていたもの（持っている弾、ハーブ、お金）：やり直しは原作と同じく
// ここから始める。失敗した挑戦の残りからではない（11面の巨人には約60発要る）。
static int mis_rounds, mis_herbs, mis_ptas, mis_kept;
// 店が変えるもの：持っている銃（ビット）、その改造、ケース。
#define WEP_STATS 4
static const u8 wep_stat_id[WEP_STATS] = { 0, 3, 5, 6 };      // （ShopTune.stat）
static const char *const wep_stat_name[WEP_STATS] = { T_STAT_POWER, T_STAT_CAPACITY, T_STAT_RATE, T_STAT_RELOAD };
enum { STAT_POWER, STAT_CAPACITY, STAT_RATE, STAT_RELOAD };
static int wep_owned = 1;
static u8 wep_level[WEP_COUNT][WEP_STATS];
static int case_level;
// 前のミッションのランク（0-4）と、そのときの店の値段、パーセント（libbio4af.so のその表）。
static int last_rank;
static const u8 rank_discount[5] = { 100, 100, 100, 90, 80 };
#define BUY_PRICE(p) ((p) * rank_discount[last_rank] / 100)
static const u8 case_tam[] = { 60, 77, 96, 120 };             // item_ac_info.bin
static const int case_price[] = { 0, 24000, 40000, 73000 };   // item_base_info.bin：大きいケース
// レオンのベスト：原作の防具のレベル（item_armor_info.bin：0、10、20、30。item_base_info.bin の
// ID 0x47-0x49、「Reduz dano em 10%」、30000 / 60000 / 90000 PTAS）。受けるダメージは
// (100 - そのレベルのパーセント)%（Android のライブラリの GM_CtrlParam::Set_Start）。
static int armor_level;
static const u8 armor_cut[] = { 0, 10, 20, 30 };
static const int armor_price[] = { 0, 30000, 60000, 90000 };
static int armored(int damage) { return damage * (100 - armor_cut[armor_level]) / 100; }

static const ShopTune *tune_at(int w, int stat, int level)
{
    for (int i = 0; i < shop_tune_count; i++)
        if (shop_tune[i].item == weapons[w].item && shop_tune[i].stat == wep_stat_id[stat] && shop_tune[i].level == level)
            return &shop_tune[i];
    return NULL;
}

// 銃 w のそのレベルでの性能（x 10）。base：改造前。
static int wep_stat(int w, int stat, int base)
{
    const ShopTune *t = wep_level[w][stat] ? tune_at(w, stat, wep_level[w][stat]) : NULL;
    return t ? t->value : base;
}

static int wep_mag(int w) { return wep_stat(w, STAT_CAPACITY, weapons[w].mag * 10) / 10; }

// 手に持っている銃の原作の単位の火力（ハンドガン 100、item_weapon_info.bin と同じ）：
// ガナードと教徒が受ける値（enemies_shot()）。
static int wep_power(void) { return wep_stat(wep, STAT_POWER, weapons[wep].power) * 10; }
// 銃のクリティカル率（item_weapon_info.bin の最初のレベルの13番目の値：ハンドガン8、マグナム16、
// ライフル56。ショットガン、TMP、ランチャーは無し）と、種類の倍率表でのその行。
static int wep_crit_rate(void) { static const u8 r[WEP_COUNT] = { 8, 16, 0, 56, 0, 0 }; return r[wep]; }
static int wep_crit_kind(void) { return wep == WEP_HANDGUN ? CRIT_HANDGUN : wep == WEP_MAGNUM ? CRIT_MAGNUM : wep == WEP_RIFLE ? CRIT_RIFLE : CRIT_NONE; }
#ifdef TEST_SHOW_HP
static int test_shot_foe = -1, test_shots, test_staggers, test_downs;
#endif


// 次の射撃までのアニメのフレーム数（f32）：銃の連射速度レベルでの ReactionSpeed（変わるのは
// ハンドガンだけ：10、8、6 ティック、店の表示で 0.5、0.4、0.3 秒）。
static int wep_refire(int w)
{
    static const u8 handgun[3] = { 10, 8, 6 };
    int lv = wep_level[w][STAT_RATE];
    return inttof32(w == WEP_HANDGUN && lv < 3 ? handgun[lv] : weapons[w].refire);
}

// リロード、銃のリロード速度レベルごとに：レオンのモーションスクリプトがリロードを終える
// リロードクリップ（#36）のフレーム（ch_param_0 のリロードモーション、タイプ16、武器の
// ReloadSpeed レベルで選ぶ：GM_CtrlSub_ChHandler::_Check_Cond_Wep_ReLoadSpd。そのコマンド12）、
// 1秒に20。移植版はその時間で銃のリロードクリップ全体を再生する（f32 の速さ）。
static const u8 wep_reload_end[WEP_COUNT][5] = {
    { 18, 15, 11 },               // ハンドガン（カテゴリ2）
    { 62, 49, 37, 31 },           // マグナム（3）
    { 44, 35, 18, 10 },           // ショットガン（4）
    { 35, 28, 18 },               // ライフル（5）
    { 32, 28, 23, 18, 14 },       // TMP（6）
    { 0 },                        // ランチャー：リロード無し
};
static int wep_reload_speed(int w)
{
    int lv = wep_level[w][STAT_RELOAD], end = lv < 5 ? wep_reload_end[w][lv] : 0;
    while (lv > 0 && !end)
        end = wep_reload_end[w][--lv];
    return end ? inttof32(RELOAD_FRAMES) / end : inttof32(1);
}
#define RECOIL      inttof32(1)          // 射撃アニメーションの動きのうち狙いに足す割合
#define RELOAD_FADE floattof32(1.5)      // リロードのポーズはこのアニメフレーム数でフェードイン / アウト
#define SHOT_RANGE  inttof32(400)
#define THROWER_REACH inttof32(20)   // 壁で止まった弾も、その奥のこの距離までの投げる者には当たる（7面の胸壁）
#define DRAW_GRAB   floattof32(4.5)      // 手がホルスターに届く抜きのフレーム
#define XFADE_STEP  (inttof32(1) / 6)    // アニメーションの切り替えは6フレームでクロスフェード

enum { GUN_HOLSTERED, GUN_DRAWING, GUN_READY, GUN_HOLSTERING };

#define HAND_NODE   14                   // レオンの右手首：銃のモデルはその座標系にある
#define UPPER_BODY  10                   // 背骨から上のレオンのノード（0-9：ルート、腰、脚）
#define AIM_RAISE   (inttof32(1) / 8)    // 1フレームあたりの狙いのブレンド：8フレームで構える
#define AIM_TILT    (inttof32(1) / 40)   // 上 / 下を押している間の1フレームあたりの傾き
#define AIM_TILT_RANGE floattof32(0.8)   // いっぱいの傾き：上 / 下のポーズまでの80%（約50度）
#define AIM_TURN    degreesToAngle(1)
#define ANIM_FPS 20   // 1秒あたりのアニメのフレーム数（原作は 20Hz で更新）
#define ANIM_STEP (inttof32(ANIM_FPS) / 60)

// 60Hz の1フレームあたりの移動量（レオンの身長は約18単位）。
// Zeebo の動画で測った：ミッション1のスタートから前の家まで（56単位）歩いて3.4秒。ミッション12の
// 最初の水路（約130単位）を走って3.6秒。
// 原作の速さ（ch_param_0 のモーションスクリプト：GM_Ctrl_ObjScript::_Cmd_Move、スクリプトの値に
// レオンの MoveSpd 300 を足したもの、1ティックあたり）：歩き300、走り700、後退400。負傷時（体力が
// 1200のうち800未満）は280、600、380。歩きが上で測ったものになるように縮めた。彼の歩きの
// クリップの足もそれに合う（Android 版の1秒20ティックだと全部1.45倍速くなり、足が滑る）。
// LEON_MOVE(s) は 60Hz の1フレームあたりの速さ s。
#define WALK_SPEED  floattof32(0.27)   // 約16単位/秒
#define LEON_MOVE(s) (WALK_SPEED * (s) / 300)
#define RUN_SPEED   LEON_MOVE(700)     // 約38単位/秒
#define BACK_SPEED  LEON_MOVE(400)
// クリップは自身の1秒20フレームで再生する：接地した足（足首が最低点から0.25以内）で測ると、
// 歩き #23 の足は約15.6単位/秒、走り #24 は約37、負傷時の歩き #1 は約14、走り #2 は約31、
// ハンドガンの歩きは約15、走りは約35で後ろへ動く。上の移動量は（16.2、37.8、15.1、32.4）。
// 走りを1.575倍速で再生すると（以前どおり）足は約58になった。
// クイックターン（モーション13：歩きのクリップ、6ティックで半回転、2ティック目から1ティック
// あたり200前へ）：下と B。
#define QTURN_FRAMES 26
#define QTURN_MOVE_FROM 9
#define QTURN_PAIR 6      // 下と B がこのフレーム数以内に押された：クイックターン
#define TURN_SPEED  degreesToAngle(4)  // 240度/秒
#define STEP_UP     inttof32(3)        // 1フレームで登れる最高の段差。それより高いものは無視
#define LEON_RADIUS floattof32(2.5)    // 壁に対する当たり判定の円柱
#define LEON_HEIGHT inttof32(16)

#ifdef TEST_AUDIT
// テスト用ビルド：このステージでレオンが歩いて行ける所。スタート地点から、床の上を2単位刻みで、
// 彼自身の当たり判定（STEP_UP、LEON_RADIUS、LEON_HEIGHT）で塗りつぶす。段差から降りる（一方通行）、
// はしご（双方向）、ジャンプも含む。その後、すべてのイベント地点（そのイベントの届く範囲と高さの
// 枠内）、木箱、はしご、アシュリーの場所について、彼が行けるかどうか。扉は開いたものとする
// （物は地面に含まれない）。画面に表示する。
#define AU_CELL  inttof32(2)
#define AU_LEVELS 16
#define AU_LEVEL(y) ((((y) >> 12) + 48) / 12)   // 1段12単位、y -48 から
#define AU_Q 16384
#ifdef AU_STEP_U   // （テスト用ビルド：-DAU_STEP_U=n：塗りつぶしが登る段差の高さ、単位）
#define AU_STEP inttof32(AU_STEP_U)
#else
#define AU_STEP STEP_UP
#endif
typedef struct { int x, z, feet; } AuState;
static void audit_stage(int stage, const int start[3])
{
    const Ground *g = &stage_ground;
    const StageFoes *sf = &stage_foes[stage];
    int nx = (g->cells_x * g->cell_size) / AU_CELL + 1, nz = (g->cells_z * g->cell_size) / AU_CELL + 1;
    u32 bits = (u32)nx * nz * AU_LEVELS;
    u8 *seen = calloc(bits / 8 + 1, 1);
    AuState *q = malloc(sizeof(AuState) * AU_Q);
    consoleDemoInit();
    if (!seen || !q)
    {
        printf("audit: out of memory (%dx%d)\n", nx, nz);
        return;
    }
    // 目標：イベント地点（そのイベントの最大の届く範囲）、木箱、はしご（下、上）、アシュリー。
    enum { T_MAX = 48 };
    int tk[T_MAX], ti[T_MAX], tx[T_MAX], ty[T_MAX], tz[T_MAX], tr[T_MAX], hit[T_MAX], tn = 0;
    for (int p = 0; p < sf->points && tn < T_MAX; p++)
    {
        int reach = 0;
        for (int e = 0; e < sf->events; e++)
            if (sf->event[e].ctg == EV_POINT && sf->event[e].trg == p && sf->event[e].reach > reach)
                reach = sf->event[e].reach;
        if (!reach)
            continue;
        tk[tn] = 'P'; ti[tn] = p; tx[tn] = sf->point[p][0]; ty[tn] = sf->point[p][1]; tz[tn] = sf->point[p][2]; tr[tn] = reach; tn++;
    }
    for (int i = 0; i < sf->things && tn < T_MAX; i++)
        if (sf->thing[i].kind == 4 || sf->thing[i].kind == 7)
        {
            tk[tn] = 'C'; ti[tn] = i; tx[tn] = sf->thing[i].pos[0]; ty[tn] = sf->thing[i].pos[1]; tz[tn] = sf->thing[i].pos[2]; tr[tn] = 0; tn++;
        }
    for (int i = 0; i < objs.count && tn + 1 < T_MAX; i++)
        if (objs.o[i].kind == OBJ_LADDER)
        {
            tk[tn] = 'U'; ti[tn] = i; tn++;   // （下の範囲）
            tk[tn] = 'D'; ti[tn] = i; tn++;   // （上の範囲）
        }
    if (sf->ally && tn < T_MAX)
    {
        tk[tn] = 'A'; ti[tn] = 0; tx[tn] = sf->ally_pos[0]; ty[tn] = sf->ally_pos[1]; tz[tn] = sf->ally_pos[2]; tr[tn] = 0; tn++;
    }
    for (int t = 0; t < tn; t++)
        hit[t] = 0;

    int head = 0, tail = 0, count = 0, lost = 0;
    #define AU_PUSH(X, Z, F) do { \
        int cx_ = ((X) - g->origin_x) / AU_CELL, cz_ = ((Z) - g->origin_z) / AU_CELL, lv_ = AU_LEVEL(F); \
        if (cx_ >= 0 && cz_ >= 0 && cx_ < nx && cz_ < nz && lv_ >= 0 && lv_ < AU_LEVELS) { \
            u32 b_ = ((u32)cz_ * nx + cx_) * AU_LEVELS + lv_; \
            if (!(seen[b_ >> 3] & (1 << (b_ & 7)))) { \
                seen[b_ >> 3] |= 1 << (b_ & 7); count++; \
                if ((tail + 1) % AU_Q == head) lost++; \
                else { q[tail].x = (X); q[tail].z = (Z); q[tail].feet = (F); tail = (tail + 1) % AU_Q; } } } } while (0)
    AU_PUSH(start[0], start[2], start[1]);
    static const s8 dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
    while (head != tail)
    {
        AuState s = q[head];
        head = (head + 1) % AU_Q;
        // ここから届く目標。
        for (int t = 0; t < tn; t++)
        {
            if (hit[t])
                continue;
            if (tk[t] == 'P')
            {
                int at[3] = { s.x, s.feet, s.z };
                hit[t] = ev_near(tx[t], ty[t], tz[t], tr[t], at);
            }
            else if (tk[t] == 'C' || tk[t] == 'A')
            {
                int dx = s.x - inttof32(tx[t]), dz = s.z - inttof32(tz[t]), dy = s.feet - inttof32(ty[t]);
                int r = tk[t] == 'C' ? inttof32(9) : inttof32(30);
                hit[t] = (s64)dx * dx + (s64)dz * dz < (s64)r * r && dy > -inttof32(12) && dy < inttof32(8);
            }
            else
            {
                const Obj *o = &objs.o[ti[t]];
                int dx = s.x - o->pos[0], dz = s.z - o->pos[2];
                int along = mulf32(dx, o->dir[0]) + mulf32(dz, o->dir[1]);
                int across = abs(mulf32(dx, o->dir[1]) - mulf32(dz, o->dir[0]));
                if (tk[t] == 'U')
                    hit[t] = across < inttof32(6) && along > -inttof32(8) && along < o->end[0] + inttof32(2) && s.feet < o->pos[1] + inttof32(4);
                else
                    hit[t] = across < inttof32(6) && along > o->end[0] - inttof32(3) && along < o->end[0] + inttof32(14) && s.feet > o->top - inttof32(4);
            }
        }
        // はしご：下の範囲から登り終えた所へ上がり、下へ降りる。
        for (int i = 0; i < objs.count; i++)
        {
            const Obj *o = &objs.o[i];
            if (o->kind != OBJ_LADDER)
                continue;
            int dx = s.x - o->pos[0], dz = s.z - o->pos[2];
            int along = mulf32(dx, o->dir[0]) + mulf32(dz, o->dir[1]);
            int across = abs(mulf32(dx, o->dir[1]) - mulf32(dz, o->dir[0]));
            if (across < inttof32(6) && along > -inttof32(8) && along < o->end[0] + inttof32(2) && s.feet < o->pos[1] + inttof32(4))
            {
                int x = o->pos[0] + mulf32(o->dir[0], CLIMB_BACK), z = o->pos[2] + mulf32(o->dir[1], CLIMB_BACK), f = o->top;
                ground_height(g, x, z, f, STEP_UP, &f);
                AU_PUSH(x, z, f);
            }
            if (across < inttof32(6) && along > o->end[0] - inttof32(3) && along < o->end[0] + inttof32(14) && s.feet > o->top - inttof32(4))
                AU_PUSH(o->pos[0], o->pos[2], o->pos[1]);
        }
        // ジャンプ（5面のシャンデリア）。
        for (unsigned j = 0; j < sizeof(jumps) / sizeof(jumps[0]); j++)
        {
            const JumpSpot *js = &jumps[j];
            int dx = s.x - inttof32(js->x), dz = s.z - inttof32(js->z), dy = s.feet - inttof32(js->y);
            if (js->stage == stage && (s64)dx * dx + (s64)dz * dz < (s64)JUMP_NEAR * JUMP_NEAR && dy < inttof32(6) && dy > -inttof32(6))
            {
                int f = inttof32(js->to_y);
                ground_height(g, inttof32(js->to_x), inttof32(js->to_z), f + STEP_UP, STEP_UP * 2, &f);
                AU_PUSH(inttof32(js->to_x), inttof32(js->to_z), f);
            }
        }
        // 歩きの1歩。
        for (int d = 0; d < 8; d++)
        {
            int x = s.x + dirs[d][0] * AU_CELL, z = s.z + dirs[d][1] * AU_CELL;
            ground_collide(g, &x, &z, s.feet + AU_STEP, s.feet + LEON_HEIGHT, LEON_RADIUS);
            int floor = s.feet;
            if (!ground_height(g, x, z, s.feet, AU_STEP, &floor))
                continue;   // （床が無い：彼は進まない）
#ifdef TEST_AUDIT_SHUT   // （ミッションの扉は閉じている：どれも開けない。鍵の扉は壁のまま）
            {
                int ox = x, oz = z;
                objects_collide(&objs, &ox, &oz, s.feet, LEON_RADIUS);
                if (ox != x || oz != z)
                    continue;
            }
#endif
            if (floor < s.feet - DROP_MIN && stage == ASH_STAGE_E)
                continue;   // （10面：バルコニーの端で止まる）
            AU_PUSH(x, z, floor);   // （ずっと下の床：そこへ落ちる）
        }
    }
    #undef AU_PUSH
    printf("S%d cells %d%s\n", stage + 1, count, lost ? " QUEUE FULL" : "");
    int col = 0;
    for (int t = 0; t < tn; t++)
    {
        char buf[16];
        if (tk[t] == 'U' || tk[t] == 'D')
            snprintf(buf, sizeof(buf), "L%d%c %s", ti[t], tk[t], hit[t] ? "ok" : "NO");
        else
            snprintf(buf, sizeof(buf), "%c%d %s", tk[t], ti[t], hit[t] ? "ok" : "NO");
        printf("%-10s", buf);
        if (++col == 3)
        {
            printf("\n");
            col = 0;
        }
    }
    printf("\n");
    free(seen);
    free(q);
}
#endif


// 追従カメラ、原作がミッション開始時に設定するとおり（GM_Task_Mis_Prepare::_Main_MisCam
// -> GM_Hdl_Cam_Behind::Set_DefCam、ミッションファイルのセクション1のレコードから：DefCamPos +32..40
// （s32 /256：回転の中心点、レオンから：右に2.6、後ろに2.6、上に15.9。巨人のステージでは16.7）、
// DefCamRot +44..52（1回転 65536：600 = 3.3度見下ろす、巨人のステージでは -200、10面では2400。
// 1200 = 彼の右へ6.6度回る）、DefCamDis +56（その点から19.5）。視野角45度）。cam_setup() がそれを
// 移植版の視点と注視点のオフセット（後ろ、右、高さ。注視点は CAM_AHEAD 前）に変える。（移植版は
// 以前、キャプチャから取った独自の値を使っていた：後ろ14.5、右1.5、上16.5、視野角70度。）
static int cam_back = floattof32(14.5), cam_right = floattof32(1.5), cam_height = floattof32(16.5),
           cam_at_height = floattof32(13), cam_at_right = floattof32(1.5);
#define CAM_FOV       45
#define ST_FOV        70                 // ステータス画面の視点（そのためのレオンの配置）
#define CAM_BACK      cam_back           // レオンの後ろ
#define CAM_NEAR      inttof32(9)        // カメラがレオンにこれだけ近い（後ろに壁）と…
#define CAM_NEAR_LIFT floattof32(1.2)    // …1単位近づくごとにこれだけ上がる
#define CAM_FADE      inttof32(5)        // …これより近いと彼が透ける
#define CAM_CLIMB_OUT    inttof32(20)     // はしごの上：壁からこれだけ離れて
#define CAM_CLIMB_SIDE   inttof32(6)      // …彼の右にこれだけ
#define CAM_CLIMB_HEIGHT inttof32(12)     // …足元からこれだけ上
#define CAM_CLIMB_AT     inttof32(9)      // …彼の背中を見る
#define CAM_WALL_GAP  floattof32(1.5)    // カメラは壁からこれだけ手前に保つ
#define CAM_WALL_STEP floattof32(1.5)    // （歩きの壁：彼からこの間隔で調べる…
#define CAM_WALL_PROBE floattof32(0.75)  // …この幅で）
#define CAM_RIGHT     cam_right          // 彼の右へ（彼は中央の少し左に映る）
#define CAM_AT_RIGHT  cam_at_right       // 注視点の、彼の右へ
#define CAM_HEIGHT    cam_height         // 足元から上へ
#define CAM_AHEAD     floattof32(40)     // 彼の前の注視点…
#define CAM_AT_HEIGHT cam_at_height      // …この高さで
// 構えている間、カメラは肩越しに寄り、狙いの傾きに従う。
#define CAM_AIM_BACK    floattof32(9)
#define CAM_AIM_RIGHT   floattof32(3.5)
#define CAM_AIM_HEIGHT  floattof32(15.5)
#define CAM_AIM_TILT    floattof32(25)   // 上 / 下にいっぱいに傾けたときの注視点の高さの変化
#define KNIFE_CAM_BACK   floattof32(12)  // ナイフの視点：彼の真後ろ…
#define KNIFE_CAM_RIGHT  floattof32(0.5)
#define KNIFE_CAM_HEIGHT floattof32(22)  // …頭の上…
#define KNIFE_CAM_AT     floattof32(-4)  // …CAM_AHEAD 前のこの高さの点を見下ろす（約25度）
// 壁がカメラを CAM_NEAR より近くに押す：頭の上に上がる代わりに彼のまわりを回る（最大
// CAM_SWING_MAX 度、空きの多い側へ）。上がると彼が画面から外れ（彼の上越しに前を見ていた）、
// 透けていた。
#define CAM_SWING_MAX   60
#define CAM_SWING_EASE  3                // （ゆっくり：1フレームに残りの1/8、最低でもこの角度の単位）

// レオンの頭と eye[]（f32 のワールド座標）のカメラの間の壁：カメラを最初の壁のすぐ手前まで
// 寄せる。最後に彼から（水平に）どれだけ離れたかを返す。
static int cam_clear(const int head[3], int eye[3], int feet)
{
    int dv[3] = { eye[0] - head[0], eye[1] - head[1], eye[2] - head[2] };
    int len = (int)sqrt64((u64)((s64)dv[0] * dv[0] + (s64)dv[1] * dv[1] + (s64)dv[2] * dv[2]));
    if (len <= CAM_WALL_GAP)
        return len;
    // 壁の裏から見る（壁を背に、向きを変える）と、壁は向こうを向いていて描かれず、無くなった
    // ように見える：彼の頭からカメラへの光線。間に壁があればカメラを寄せる。
    int dir[3] = { divf32(dv[0], len), divf32(dv[1], len), divf32(dv[2], len) }, hit[3];
    if (ground_raycast(&stage_ground, head, dir, len + CAM_WALL_GAP, hit, RAY_CAMERA))
    {
        int hx = hit[0] - head[0], hz = hit[2] - head[2];
        int hd = (int)sqrt64((u64)((s64)hx * hx + (s64)hz * hz)) - CAM_WALL_GAP;
        if (hd < inttof32(2))
            hd = inttof32(2);
        if (hd < len)
            for (int k = 0; k < 3; k += 2)
                eye[k] = head[k] + mulf32(dir[k], hd);
    }
    // レオンを止める壁（柵、手すり：全部がカメラ用のセットにあるわけではない）もカメラを止める：
    // 線に沿って彼から外へ進み、彼の胸とカメラの高さの間で最初にその中に入る場所までしか行かない。
    int hlen = (int)sqrt64((u64)((s64)(eye[0] - head[0]) * (eye[0] - head[0]) + (s64)(eye[2] - head[2]) * (eye[2] - head[2])));
    if (hlen > CAM_WALL_GAP)
    {
        int ux = divf32(eye[0] - head[0], hlen), uz = divf32(eye[2] - head[2], hlen);
        int lo = feet + inttof32(10), hi = eye[1] > lo ? eye[1] : lo + inttof32(1);
        for (int d = CAM_WALL_STEP; d < hlen; d += CAM_WALL_STEP)
        {
            int x = head[0] + mulf32(ux, d), z = head[2] + mulf32(uz, d);
            if (ground_collide_n(&stage_ground, &x, &z, lo, hi, CAM_WALL_PROBE, 1))
            {
                int hd = d - CAM_WALL_STEP;
                if (hd < inttof32(2))
                    hd = inttof32(2);
                eye[0] = head[0] + mulf32(ux, hd);
                eye[2] = head[2] + mulf32(uz, hd);
                return hd;
            }
        }
    }
    return hlen;
}

enum { CTRL_LEON, CTRL_FREECAM };

#ifndef START_CTRL   // -DSTART_CTRL=1 でフリーカメラから始める
#define START_CTRL CTRL_LEON
#endif
#ifndef LEON_START_MODE   // フリーカメラ：-DLEON_START_MODE=1 で円を回って歩き始める
#define LEON_START_MODE ANIM_IDLE
#endif
// フリーカメラの歩きの円（ワールド単位）。既定：ステージの中心のまわりに半径20、平らな地面。
// -DLEON_CIRCLE_X=60 -DLEON_CIRCLE_Z=180 -DLEON_CIRCLE_R=50 で丘を回る。
#ifndef LEON_CIRCLE_R
#define LEON_CIRCLE_R 20
#endif

static void on_vblank(void) { vblank_count++; }

static const u32 cull_modes[] = { POLY_CULL_BACK, POLY_CULL_FRONT, POLY_CULL_NONE };

#ifdef AUTOPILOT
// テスト用ビルド（-DAUTOPILOT）：エミュレーターのウィンドウは外から操作できないので、レオンの
// モードで十字キーの入力を台本どおりに行う。10秒ごとに繰り返す。
static int ap_aim_err;   // （テスト用ビルド：レオンの向きから見た巨人の方角、+ = 左）
static u32 autopilot_keys(u32 frame)
{
#if AUTOPILOT == 2
    // -DAUTOPILOT=2：まっすぐ走り、4秒ごとに少し左へそれる。
    return (frame % 240) < 20 ? KEY_UP | KEY_B | KEY_LEFT : KEY_UP | KEY_B;
#elif AUTOPILOT == 4
    // -DAUTOPILOT=4：狙いのテスト。9秒立ち、3秒構え、上に傾け、下に傾け、下ろし、繰り返す。
    if (frame < 540) return 0;
    u32 a = (frame - 540) % 600;
    if (a < 180) return KEY_R;
    if (a < 240) return KEY_R | KEY_UP;
    if (a < 360) return KEY_R | KEY_DOWN;
    if (a < 420) return KEY_R | KEY_UP;
    return 0;
#elif AUTOPILOT == 5
    // -DAUTOPILOT=5：抜き / しまいのテスト。9秒立ち、抜き（X）、2秒構えて立ち、2秒歩き、
    // 2秒走り、1秒立ち、しまい（X）、立つ。
    if (frame < 540) return 0;
    u32 a = frame - 540;
    if (a < 4) return KEY_X;
    if (a < 160) return 0;
    if (a < 280) return KEY_UP;
    if (a < 400) return KEY_UP | KEY_B;
    if (a < 460) return 0;
    if (a < 464) return KEY_X;
    return 0;
#elif AUTOPILOT == 7
    // -DAUTOPILOT=7：射撃テスト。9秒後、構えて0.4秒ごとに撃つ。11発目でマガジンが空になって
    // リロードし、それから撃ち続ける。-DLEON_START_X=225 -DLEON_START_Z=204 -DLEON_START_YAW=106 で
    // 弾が納屋の壁に当たる。
    if (frame < 540) return 0;
    u32 a = frame - 540, keys = KEY_R;
    if (a % 24 < 2 && a > 20) keys |= KEY_A;
    return keys;
#elif AUTOPILOT == 8
    // -DAUTOPILOT=8：ガナードのテスト。9秒立ち（彼らが歩いて来る）、それから構えて0.5秒ごとに撃つ。
    // 合間に少し左右に向きを変えて次の者を捉える。
    if (frame < 540) return 0;
    u32 a = frame - 540, keys = KEY_R;
    if (a % 30 < 2 && a > 20) keys |= KEY_A;
    if ((a / 90) % 4 == 1 && a % 30 > 10) keys |= KEY_LEFT;
    if ((a / 90) % 4 == 3 && a % 30 > 10) keys |= KEY_RIGHT;
    return keys;
#elif AUTOPILOT == 9
    // -DAUTOPILOT=9：ガナードの攻撃のテスト。4秒群れの中へ歩き、それから立って攻撃させ、1秒に5回
    // A を押す（つかみから約1.6秒で振りほどく）。
    if (frame < 540) return 0;
    u32 a = frame - 540;
    if (a < 240) return KEY_UP;
    return a % 12 < 2 ? KEY_A : 0;
#elif AUTOPILOT == 10
    // -DAUTOPILOT=10：メニューのテスト、タイトルから：操作説明の画面（フレーム560-840）、戻る、
    // ゲーム開始、歩く、ポーズ（1140-1400、「やり直し」へ移動）、やり直し。
    // 次にステージセレクト（900-1100）：右でロックされたステージへ、A（拒否される）、左、A（村へ
    // ズーム）。歩く、ポーズ（1400-1600）、やり直し。
    static const struct { u16 at; u16 key; } script[] = {
        { 540, KEY_DOWN }, { 560, KEY_A }, { 840, KEY_B }, { 870, KEY_DOWN }, { 900, KEY_A },
        { 960, KEY_RIGHT }, { 1000, KEY_A }, { 1040, KEY_LEFT }, { 1100, KEY_A },
        { 1400, KEY_START }, { 1460, KEY_DOWN }, { 1600, KEY_A } };
    for (unsigned i = 0; i < sizeof(script) / sizeof(script[0]); i++)
        if (frame >= script[i].at && frame < script[i].at + 2u)
            return script[i].key;
    return frame > 1200 && frame < 1400 ? KEY_UP : 0;
#elif AUTOPILOT == 12
    // -DAUTOPILOT=12 -DTEST_HURT -DTEST_GOD：バイタル画面、台本どおりのタッチで（main() を参照）。
    (void)frame;
    return 0;
#elif AUTOPILOT == 13
    // -DAUTOPILOT=13 -DSTAGE_START -DLEON_START_X=240 -DLEON_START_Z=565 -DLEON_START_YAW=180：
    // 南の門まで歩き、蹴り開け（A）、村へ歩いて入る。
    if (frame >= 540 && frame < 720) return KEY_UP;   // 門に押し付けられて終わる
    if (frame >= 740 && frame < 742) return KEY_A;
    if (frame >= 840 && frame < 1100) return KEY_UP;
    return 0;
#elif AUTOPILOT == 14
    // -DAUTOPILOT=14 -DSTAGE_START -DLEON_START_X=310 -DLEON_START_Z=76 -DLEON_START_YAW=90：
    // はしごを登り（A）、振り返り、端まで歩いて降りる（A）。通路の別の所から歩き出て落ちる。
    if (frame >= 540 && frame < 560) return KEY_UP;
    if (frame >= 570 && frame < 572) return KEY_A;
    if (frame >= 780 && frame < 800) return KEY_A;     // まっすぐ降りる（彼は端を向いている）
    if (frame >= 1000 && frame < 1002) return KEY_A;   // もう一度登る
    if (frame >= 1200 && frame < 1260) return KEY_LEFT;
    if (frame >= 1260 && frame < 1500) return KEY_UP;  // 通路に沿って、その端から出る
    return 0;
#elif AUTOPILOT == 60
    // -DAUTOPILOT=60 -DTEST_SHOW_POS -DSTAGE_START -DLEON_START_X=298 -DLEON_START_Z=76
    // -DLEON_START_YAW=90（1面の東のはしご、その下から19手前）：はしごの下半分の上に立つまで歩いて
    // 入り（下から10先、以前の7単位の円の外）、A（登る）。（端を向いて終わるので）数歩進んで、
    // A（降りる）。
    if (frame >= 540 && frame < 650) return KEY_UP;
    if (frame >= 660 && frame < 662) return KEY_A;
    if (frame >= 900 && frame < 918) return KEY_UP;
    if (frame >= 930 && frame < 932) return KEY_A;
    return 0;
#elif AUTOPILOT == 15
    // -DAUTOPILOT=15 -DSTAGE_START -DLEON_START_X=340 -DLEON_START_Z=76 -DLEON_START_YAW=270：
    // 通路の上（スタートは一番高い床に着く）で、その端から歩き出て落ちる。
    return frame >= 540 && frame < 700 ? KEY_UP : 0;
#elif AUTOPILOT == 16
    // -DAUTOPILOT=16 -DSTAGE_START -DLEON_START_X=359 -DLEON_START_Z=250 -DLEON_START_YAW=0：
    // 前の樽を狙って撃つ（爆発する）。次に右の木箱。
    // （樽と木箱は銃より低い：先に狙いを少し下に傾ける。）
    if (frame >= 540 && frame < 700) return KEY_R | (frame >= 560 && frame < 563 ? KEY_DOWN : 0) | (frame >= 570 && frame < 577 ? KEY_RIGHT : 0) | (frame >= 620 && frame < 622 ? KEY_A : 0);
    if (frame >= 760 && frame < 960) return KEY_R | (frame >= 770 && frame < 773 ? KEY_DOWN : 0) | (frame >= 800 && frame < 830 ? KEY_LEFT : 0) | (frame >= 900 && frame < 902 ? KEY_A : 0);
    return 0;
#elif AUTOPILOT == 18
    // -DAUTOPILOT=18 -DSTAGE_START -DLEON_START_X=391 -DLEON_START_Z=322 -DLEON_START_YAW=90：
    // 木箱の横：L を押したまま（ナイフを構え）、斬る（A）。
    if (frame >= 540 && frame < 760) return KEY_L | (frame >= 600 && frame < 602 ? KEY_A : 0);
    return 0;
#elif AUTOPILOT == 45
    // -DAUTOPILOT=45 -DGIANT_AHEAD_TEST：撃ってひざをつかせ、また起き上がらせる。次に撃ち倒して
    // 駆け寄って斬る、を何度も。
    static int kneels, last = -1;
    if (frame < 300) return 0;
    if (giants[0].state == GIANT_KNEEL && last != GIANT_KNEEL)
        kneels++;
    last = giants[0].state;
    if (giants[0].state == GIANT_KNEEL)
        return kneels == 1 ? 0 : KEY_UP | KEY_B | (frame % 12 < 2 ? KEY_A : 0);
    if (giants[0].state == GIANT_RISE || giants[0].state == GIANT_DEAD)
        return 0;
    return KEY_R | ((frame % 20) < 2 ? KEY_A : 0);
#elif AUTOPILOT == 46
    // -DAUTOPILOT=46 -DTEST_ALL_WEAPONS -DTEST_WEAPON=n：抜き、L+X を n 回、それから4秒後に構えて
    // 1回撃つ（ランチャーは前の巨人に。ライフルはスコープ越しに、傾けながら）。
    if (frame < 400) return 0;
    if (frame < 403) return KEY_X;
    for (int k = 0; k < TEST_WEAPON; k++)
        if (frame >= 480u + k * 40 && frame < 483u + k * 40) return KEY_L | KEY_X;
    if (frame < 800) return 0;
    if (frame >= 1040 && frame < 1042) return KEY_R | KEY_A;
    if (frame >= 1100 && frame < 1160) return KEY_R | KEY_UP;
    return frame < 1400 ? KEY_R : 0;
#elif AUTOPILOT == 47
    // -DAUTOPILOT=47 -DTEST_ALL_WEAPONS -DGIANT_AHEAD_TEST：ライフル（L+X を3回）、スコープ越しに
    // 巨人を。ひざをついたら駆け寄って A（登って斬る）。倒れるまで繰り返す。
    if (frame < 400) return 0;
    if (frame < 403) return KEY_X;
    for (int k = 0; k < 3; k++)
        if (frame >= 480u + k * 40 && frame < 483u + k * 40) return KEY_L | KEY_X;
    if (frame < 700) return 0;
    if (giants[0].held)
        return frame % 120 < 2 ? KEY_A : 0;
    if (giants[0].state == GIANT_KNEEL)
        return KEY_UP | KEY_B | (frame % 12 < 2 ? KEY_A : 0);
    if (giants[0].state == GIANT_RISE || giants[0].state == GIANT_DEAD)
        return 0;
    // （彼の方へ向きを変える：構えの左右。）
    u32 turn = ap_aim_err > degreesToAngle(2) ? KEY_LEFT : ap_aim_err < -degreesToAngle(2) ? KEY_RIGHT : 0;
    return KEY_R | turn | (!turn && (frame % 30) < 2 ? KEY_A : 0);
#elif AUTOPILOT == 19
    // -DAUTOPILOT=19：5秒から、まっすぐ前を構えて1/3秒ごとに撃つ（巨人のテスト）。
    if (frame < 300) return 0;
    return KEY_R | ((frame % 20) < 2 ? KEY_A : 0);
#elif AUTOPILOT == 17
    // -DAUTOPILOT=17 -DTEST_DAZE -DTEST_GOD：1人歩いて来させ、撃ち（このビルドでは当たるたびに
    // ふらつく）、銃を下ろして蹴る（A）。2秒ごとに繰り返す。
    if (frame < 900) return 0;
    u32 a = (frame - 900) % 120;
    if (a < 40) return KEY_R | (a < 24 ? KEY_LEFT : 0) | (a >= 30 && a < 32 ? KEY_A : 0);   // 向きを変えながら、1人を探す
    return a >= 50 && a < 52 ? KEY_A : 0;
#elif AUTOPILOT == 11
    // -DAUTOPILOT=11 -DTEST_GOD -DTEST_KILL_AT=600：彼らが歩いて来て、フレーム600で全員倒れ、
    // アイテムを落とす（11-14秒に見える）。レオンはその中を前へ歩き（14-18秒）、それから
    // ステージクリア（鐘、STAGE CLEAR、結果）、A でステージセレクトへ進む。
    if (frame >= 840 && frame < 1080) return KEY_UP;
    if (frame >= 1700 && frame % 60 < 2) return KEY_A;
    return 0;
#elif AUTOPILOT == 21
    // -DAUTOPILOT=21：ポーズメニュー：START、下で Quit、A（確認）、上で YES、B（戻る）、
    // 下で Sound、A、上で Medium、A（設定）、下で Controls、A、B、START（ゲームに戻る）。
    static const struct { u16 at; u16 key; } script[] = {
        { 400, KEY_START }, { 520, KEY_DOWN }, { 640, KEY_A }, { 760, KEY_UP }, { 880, KEY_B },
        { 1000, KEY_DOWN }, { 1060, KEY_A }, { 1180, KEY_UP }, { 1300, KEY_A }, { 1420, KEY_DOWN },
        { 1480, KEY_A }, { 1600, KEY_B }, { 1720, KEY_START } };
    for (unsigned i = 0; i < sizeof(script) / sizeof(script[0]); i++)
        if (frame >= script[i].at && frame < script[i].at + 2u)
            return script[i].key;
    return 0;
#elif AUTOPILOT == 23
    // -DAUTOPILOT=23：5秒から、構えて、少し上に傾けるのと撃つのを交互に（吊るされた容器）。
    if (frame < 300) return 0;
    return KEY_R | ((frame % 30) < 5 && frame > 330 ? KEY_UP : 0) | ((frame % 30) >= 15 && (frame % 30) < 17 ? KEY_A : 0);
#elif AUTOPILOT == 22
    // -DAUTOPILOT=22 -DTEST_HURT：ステータス画面：SELECT、ハンドガンで A（そのコマンド）、
    // B、下で弾、A、B、下でハーブ、A、A（使う）、下 x3（スクロール）、SELECT。
    static const struct { u16 at; u16 key; } script[] = {
        { 400, KEY_SELECT }, { 520, KEY_A }, { 640, KEY_B }, { 700, KEY_DOWN }, { 760, KEY_A },
        { 880, KEY_B }, { 940, KEY_DOWN }, { 1000, KEY_A }, { 1120, KEY_A }, { 1240, KEY_DOWN },
        { 1300, KEY_DOWN }, { 1360, KEY_DOWN }, { 1480, KEY_SELECT } };
    for (unsigned i = 0; i < sizeof(script) / sizeof(script[0]); i++)
        if (frame >= script[i].at && frame < script[i].at + 2u)
            return script[i].key;
    return 0;
#elif AUTOPILOT == 31
    // -DAUTOPILOT=31：抜き、それから4秒ごとに次の武器（L+X）、構え（R）、撃つ。
    if (frame < 400) return 0;
    if (frame < 403) return KEY_X;
    if (frame < 480) return 0;
    u32 a = (frame - 480) % 240;
    if (a < 3) return KEY_L | KEY_X;
    if (a < 60) return 0;
    if (a == 130 || a == 131) return KEY_R | KEY_A;
    return a < 200 ? KEY_R : 0;
#elif AUTOPILOT == 32
    // -DAUTOPILOT=32 -DTEST_WEAPON=n：抜き、L+X を n 回、それから構えたまま4秒ごとに撃つ。
    if (frame < 400) return 0;
    if (frame < 403) return KEY_X;
    for (int k = 0; k < TEST_WEAPON; k++)
        if (frame >= 480u + k * 40 && frame < 483u + k * 40) return KEY_L | KEY_X;
    if (frame < 800) return 0;
    return (frame - 1000) % 240 < 2 ? KEY_R | KEY_A : KEY_R;
#elif AUTOPILOT == 33
    // -DAUTOPILOT=33：ステータス画面：SELECT、下 x2（ショットガン）、A、A（Equip）、SELECT、抜き、構え。
    static const struct { u16 at; u16 key; } script33[] = {
        { 400, KEY_SELECT }, { 480, KEY_DOWN }, { 540, KEY_DOWN }, { 620, KEY_A }, { 700, KEY_A }, { 860, KEY_SELECT }, { 960, KEY_X } };
    for (unsigned i = 0; i < sizeof(script33) / sizeof(script33[0]); i++)
        if (frame >= script33[i].at && frame < script33[i].at + 2u)
            return script33[i].key;
    return frame > 1060 ? KEY_R : 0;
#elif AUTOPILOT == 34
    // -DAUTOPILOT=34 -DTEST_SHOP：店：Buy、マグナム、次にその弾。B。Upgrade、ハンドガン、
    // 火力。B、B。Sell、マグナム。B。Start。
    static const struct { u16 at; u16 key; } script34[] = {
        { 300, KEY_A }, { 380, KEY_DOWN }, { 460, KEY_A }, { 560, KEY_DOWN }, { 620, KEY_DOWN }, { 680, KEY_DOWN },
        { 740, KEY_DOWN }, { 800, KEY_DOWN }, { 880, KEY_A }, { 980, KEY_B }, { 1060, KEY_RIGHT }, { 1120, KEY_RIGHT },
        { 1200, KEY_A }, { 1280, KEY_A }, { 1360, KEY_A }, { 1460, KEY_B }, { 1520, KEY_B }, { 1600, KEY_LEFT },
        { 1680, KEY_A }, { 1760, KEY_A }, { 1860, KEY_B }, { 1940, KEY_LEFT }, { 2020, KEY_LEFT }, { 2100, KEY_A } };
    for (unsigned i = 0; i < sizeof(script34) / sizeof(script34[0]); i++)
        if (frame >= script34[i].at && frame < script34[i].at + 2u)
            return script34[i].key;
    return 0;
#elif AUTOPILOT == 63
    // -DAUTOPILOT=63 -DTEST_DROP_CODE=n：6.7秒で20前にドロップが出る。そこまで歩き
    // （届く範囲で「A: Pick up」が出る）、A で拾う。
    if (frame >= 470 && frame < 530) return KEY_UP;
    if (frame >= 620 && frame < 622) return KEY_A;
    return 0;
#elif AUTOPILOT == 69
    // -DAUTOPILOT=69：左に回りながら2秒後退、次に右に2秒。8秒で SELECT（ステータス画面）。
    if (frame >= 180 && frame < 300) return KEY_DOWN | KEY_LEFT;
    if (frame >= 300 && frame < 420) return KEY_DOWN | KEY_RIGHT;
    if (frame >= 480 && frame < 482) return KEY_SELECT;
    return 0;
#elif AUTOPILOT == 68
    // -DAUTOPILOT=68 -DTEST_ALL_WEAPONS -DTEST_SHOW_HP：L+X を2回（ショットガンへ）、4秒から構え（R）、
    // 6秒で A。
    if ((frame >= 60 && frame < 64) || (frame >= 90 && frame < 94)) return KEY_L | KEY_X;
    if (frame >= 120 && frame < 300) return KEY_R | (frame >= 200 && frame < 202 ? KEY_A : 0);
    return 0;
#elif AUTOPILOT == 67
    // -DAUTOPILOT=67：3秒から構え（R）。5秒から7秒まで6フレームごとに A（ハンドガンの連射間隔）。
    if (frame >= 180 && frame < 480) return KEY_R | (frame >= 300 && frame < 420 && frame % 6 == 0 ? KEY_A : 0);
    return 0;
#elif AUTOPILOT == 72
    // -DAUTOPILOT=72：3秒から15秒まで前へ歩き、それから立つ。
    if (frame >= 180 && frame < 900) return KEY_UP;
    return 0;
#elif AUTOPILOT == 71
    // -DAUTOPILOT=71：3秒から後退、4秒で B を押して1秒押したまま（振り向かない：後退し続ける）。
    // 放す。それから9秒で下と B を同時に（クイックターン）。
    if (frame >= 180 && frame < 420) return KEY_DOWN | (frame >= 240 && frame < 300 ? KEY_B : 0);
    if (frame >= 540 && frame < 560) return KEY_DOWN | KEY_B;
    return 0;
#elif AUTOPILOT == 66
    // -DAUTOPILOT=66：5秒で下と B（クイックターン）、それから8秒から2秒後退。
    if (frame >= 300 && frame < 304) return KEY_DOWN | KEY_B;
    if (frame >= 480 && frame < 600) return KEY_DOWN;
    return 0;
#elif AUTOPILOT == 65
    // -DAUTOPILOT=65（LEON_START_X/Z をミッションの出口のそばに）：12秒で A（A: Examine）。
    if (frame >= 720 && frame < 722) return KEY_A;
#ifdef TEST_A_EVERY_S   // …その後1秒ごとに A（せりふのページ、それから出口）
    if (frame >= 720 && frame % 60 == 0) return KEY_A;
#endif
#ifdef TEST_CRATE_PICK   // …それから（木箱を蹴り開けて）16秒でもう一度 A：中身
    if (frame >= 960 && frame < 962) return KEY_A;
#endif
    return 0;
#elif AUTOPILOT == 64
    // -DAUTOPILOT=64 -DTEST_SHOW_FOES -DSTART_STAGE=3：ミッションのスタートから5秒前へ歩く
    // （最初のグループが待っているイベントの壁を通る）。
    if (frame >= 180 && frame < 480) return KEY_UP;
    return 0;
#elif AUTOPILOT == 61
    // -DAUTOPILOT=61：5秒立ち、4秒歩き、4秒走る（素手：元気なときの動き。負傷時の動きは体力67未満）。
    if (frame >= 300 && frame < 540) return KEY_UP;
    if (frame >= 540 && frame < 780) return KEY_UP | KEY_B;
    return 0;
#elif AUTOPILOT == 62
    // -DAUTOPILOT=62 -DTEST_SHOP：Buy、下で最後の行（ベスト）、A：タクティカルベストを買い、その行は
    // アイアンベストになる。
    if (frame == 300 || frame == 1000) return KEY_A;
    if (frame >= 380 && frame < 380 + 9 * 40 && frame % 40 == 20) return KEY_DOWN;
    return 0;
#elif AUTOPILOT == 35
    // -DAUTOPILOT=35：タイトルから：New Game、ステージ、店の Upgrade タブ、Start。
    static const struct { u16 at; u16 key; } script35[] = {
        { 300, KEY_A }, { 420, KEY_A }, { 700, KEY_A }, { 800, KEY_DOWN }, { 900, KEY_B }, { 1000, KEY_LEFT }, { 1080, KEY_A } };
    for (unsigned i = 0; i < sizeof(script35) / sizeof(script35[0]); i++)
        if (frame >= script35[i].at && frame < script35[i].at + 2u)
            return script35[i].key;
    return 0;
#elif AUTOPILOT == 49
    // -DAUTOPILOT=49：構えて何発か撃つ、R はずっと押したまま。それから Y（リロード）と上を3秒押す
    // （リロードしながら歩く）。
    if (frame < 300) return 0;
    u32 k = KEY_R;
    if (frame > 340 && frame < 400 && frame % 18 == 0) k |= KEY_A;
    if (frame == 900) k |= KEY_Y;
    if (frame >= 904 && frame < 1080) k |= KEY_UP;
    return k;
#elif AUTOPILOT == 70
    // -DAUTOPILOT=70：構え、5発、リロード（Y）、それからポーズ、Retry、YES：持っている弾がミッション
    // 開始時の数に戻る。
    if (frame < 240) return 0;
    if (frame < 243) return KEY_X;   // （抜き）
    if (frame < 300) return 0;
    if (frame < 420) return KEY_R | (frame > 320 && frame % 18 == 0 ? KEY_A : 0);
    if (frame == 430) return KEY_Y;
    if (frame == 600) return KEY_START;
    if (frame == 640) return KEY_A;    // Retry
    if (frame == 680) return KEY_UP;   // YES
    if (frame == 720) return KEY_A;
    return 0;
#elif AUTOPILOT == 50
    // -DAUTOPILOT=50：10秒からナイフ（L を押したまま）：上を1秒（見上げる）、斬り、斬り。それから
    // 下を2秒（見下ろす）、斬り、斬り。
    if (frame < 600) return 0;
    u32 k = KEY_L;
    if (frame < 660) k |= KEY_UP;
    if (frame == 720 || frame == 800) k |= KEY_A;
    if (frame >= 880 && frame < 1000) k |= KEY_DOWN;
    if (frame == 1060 || frame == 1140) k |= KEY_A;
    return k;
#elif AUTOPILOT == 51
    // -DAUTOPILOT=51：タイトルから：New Game、ステージセレクトで右 x5（6面、城壁）、A、店の Start。
    // それからその場で回る（42と同じ）。
    static const struct { u16 at; u16 key; } script51[] = {
        { 600, KEY_A }, { 900, KEY_RIGHT }, { 960, KEY_RIGHT }, { 1020, KEY_RIGHT }, { 1080, KEY_RIGHT }, { 1140, KEY_RIGHT },
        { 1260, KEY_A }, { 1500, KEY_LEFT }, { 1600, KEY_A }, { 2000, KEY_START } };
    for (unsigned i = 0; i < sizeof(script51) / sizeof(script51[0]); i++)
        if (frame >= script51[i].at && frame < script51[i].at + 2u)
            return script51[i].key;
    return frame > 2300 && (frame / 90) % 3 == 0 ? KEY_LEFT : 0;
#elif AUTOPILOT == 48
    // -DAUTOPILOT=48：タイトルから：New Game、それからステージセレクトを移動（6面の場所を過ぎて）。
    static const struct { u16 at; u16 key; } script48[] = {
        { 600, KEY_A }, { 900, KEY_RIGHT }, { 1020, KEY_RIGHT }, { 1140, KEY_RIGHT }, { 1260, KEY_RIGHT }, { 1380, KEY_RIGHT }, { 1500, KEY_LEFT } };
    for (unsigned i = 0; i < sizeof(script48) / sizeof(script48[0]); i++)
        if (frame >= script48[i].at && frame < script48[i].at + 2u)
            return script48[i].key;
    return 0;
#elif AUTOPILOT == 36
    // -DAUTOPILOT=36 -DTEST_SHOP：Buy、下でグリーンハーブ、ケースがいっぱいになるまで A を繰り返す。
    if (frame == 300 || frame == 301) return KEY_A;
    for (unsigned i = 0; i < 6; i++)
        if (frame >= 360 + i * 30 && frame < 362 + i * 30) return KEY_DOWN;
    return frame > 600 && frame % 16 < 2 ? KEY_A : 0;
#elif AUTOPILOT == 37
    // -DAUTOPILOT=37 -DTEST_SHOP：Buy、A（持っているハンドガン：何も起きない）、下、A（マグナム）。
    static const struct { u16 at; u16 key; } script37[] = { { 300, KEY_A }, { 400, KEY_DOWN }, { 480, KEY_A } };
    for (unsigned i = 0; i < 3; i++)
        if (frame >= script37[i].at && frame < script37[i].at + 2u)
            return script37[i].key;
    return 0;
#elif AUTOPILOT == 77
    // -DAUTOPILOT=77：12秒からポーズメニューのオプション：START、下 x3（HUD）、A、下（Hide）、A。
    // 下（Bottom Screen）、A、下（Item Select）、A。START（ゲームに戻る）。
    static const struct { u16 at; u16 key; } s77[] = {
        { 720, KEY_START }, { 750, KEY_DOWN }, { 770, KEY_DOWN }, { 790, KEY_DOWN }, { 820, KEY_A }, { 850, KEY_DOWN }, { 880, KEY_A },
        { 910, KEY_DOWN }, { 940, KEY_A }, { 970, KEY_DOWN }, { 1000, KEY_A }, { 1040, KEY_START } };
    for (unsigned i = 0; i < sizeof(s77) / sizeof(s77[0]); i++)
        if (frame >= s77[i].at && frame < s77[i].at + 2u)
            return s77[i].key;
    return 0;
#elif AUTOPILOT == 76
    // -DAUTOPILOT=76：10秒からナイフ（L を押したまま）、水平。12秒と15秒に斬り。
    if (frame < 600) return 0;
    return KEY_L | (frame == 720 || frame == 900 ? KEY_A : 0);
#elif AUTOPILOT == 75
    // -DAUTOPILOT=75（-DTEST_PLG_VIEW と）：14秒まで1秒ごとに A（始まりのせりふ）、それから構え
    // （R）て1秒に2回撃つ（A）。16秒から上を AUTOPILOT_TILT フレーム押して上に傾ける。
#ifndef AUTOPILOT_TILT
#define AUTOPILOT_TILT 0
#endif
    if (frame < 840)
        return frame % 60 < 2 ? KEY_A : 0;
    {
        u32 k = KEY_R;
        if (frame >= 960 && frame < 960 + AUTOPILOT_TILT)
            k |= KEY_UP;
        if (frame >= 900 && frame % 30 < 2)
            k |= KEY_A;
        return k;
    }
#elif AUTOPILOT == 74
    // -DAUTOPILOT=74：抜き（X）、構えて前を撃つ（テーブルの上の木箱）、銃をしまい、
    // それから時々 A（落としたものを拾う）。
    if (frame >= 240 && frame < 243) return KEY_X;
    if (frame >= 300 && frame < 480) return KEY_R | (frame % 20 == 0 ? KEY_A : 0);
    if (frame >= 520 && frame < 523) return KEY_X;
    if (frame >= 700 && frame % 60 < 2) return KEY_A;
    return 0;
#elif AUTOPILOT == 73
    // -DAUTOPILOT=73 -DTEST_CLEAR_AT=300：ミッションが終わる。場面と結果とステージセレクトを A で進み、
    // 店の Buy の一覧へ。それから B、左（Start タブ）、A：次のミッション。
    if (frame > 900 && frame < 2300) return frame % 120 < 2 ? KEY_A : 0;
    if (frame >= 2400 && frame < 2402) return KEY_B;
    if (frame >= 2460 && frame < 2462) return KEY_LEFT;
    if (frame >= 2520 && frame < 2522) return KEY_A;
    if (frame > 2700 && frame < 3300) return frame % 90 < 2 ? KEY_START : 0;   // （始まりの場面を飛ばす）
    if (frame >= 3900 && frame < 4500) return KEY_UP;   // （少し歩く）
    return 0;
#elif AUTOPILOT == 38
    // -DAUTOPILOT=38 -DTEST_CLEAR_AT=500：ミッションの終わりの後、時々 A：結果、ステージセレクト、店。
    return frame > 900 && frame % 120 < 2 ? KEY_A : 0;
#elif AUTOPILOT == 39
    // -DAUTOPILOT=39：ポーズメニューの Quit：START、下、A、上で YES、A。
    static const struct { u16 at; u16 key; } script39[] = { { 400, KEY_START }, { 480, KEY_DOWN }, { 560, KEY_A }, { 640, KEY_UP }, { 720, KEY_A } };
    for (unsigned i = 0; i < 5; i++)
        if (frame >= script39[i].at && frame < script39[i].at + 2u)
            return script39[i].key;
    return 0;
#elif AUTOPILOT == 40
    // -DAUTOPILOT=40 -DTEST_SHOP：マグナムを買い、戻る（セーブ）。再起動：タイトルに Continue が出る。
    static const struct { u16 at; u16 key; } script40[] = {
        { 300, KEY_A }, { 380, KEY_DOWN }, { 460, KEY_A }, { 560, KEY_B }, { 640, KEY_B } };
    for (unsigned i = 0; i < 5; i++)
        if (frame >= script40[i].at && frame < script40[i].at + 2u)
            return script40[i].key;
    return 0;
#elif AUTOPILOT == 41
    // -DAUTOPILOT=41：タイトルから：Continue、ステージ、店の Buy の一覧。
    static const struct { u16 at; u16 key; } script41[] = { { 300, KEY_A }, { 420, KEY_A }, { 700, KEY_A } };
    for (unsigned i = 0; i < 3; i++)
        if (frame >= script41[i].at && frame < script41[i].at + 2u)
            return script41[i].key;
    return 0;
#elif AUTOPILOT == 42
    // -DAUTOPILOT=42：その場で回り、時々歩く（ステージのどこも欠けてはいけない）。
    if (frame < 300) return 0;
    return (frame / 240) % 2 ? KEY_RIGHT : KEY_UP | KEY_B | KEY_LEFT;
#elif AUTOPILOT == 43
    // -DAUTOPILOT=43：A（はしごを登る）、それから彼女がついて来る間立つ。A（降りる）、立つ。
    if (frame == 540 || frame == 541 || frame == 1500 || frame == 1501) return KEY_A;
    return 0;
#elif AUTOPILOT == 44
    // -DAUTOPILOT=44：壁に止められるまで走り、それから背中と横を壁に向けてゆっくり回る
    // （カメラが壁に押される）。それから壁に沿って走る。繰り返す。
    if (frame < 300) return 0;
    u32 a = (frame - 300) % 600;
    if (a < 180) return KEY_UP | KEY_B;
    if (a < 420) return KEY_LEFT;
    return KEY_UP | KEY_B | (a & 64 ? KEY_RIGHT : 0);
#elif AUTOPILOT == 30
    // -DAUTOPILOT=30：A（手元の扉かはしご）、それから歩いて通り抜ける。A、また歩く。
    if (frame < 540) return 0;
    u32 a = (frame - 540) % 420;
    if (a < 3) return KEY_A;
    return a >= 60 && a < 360 ? KEY_UP : 0;
#elif AUTOPILOT == 20
    // -DAUTOPILOT=20（+ 3の納屋の壁のスタート）：壁に走り込み、それから背中を壁に向けてゆっくり回る
    // （カメラは壁の手前にいなければならない）。
    if (frame < 540) return 0;
    if (frame < 660) return KEY_UP | KEY_B;
    return frame < 900 ? KEY_LEFT : 0;
#elif AUTOPILOT == 3
    // -DAUTOPILOT=3：9秒立ち（エミュレーターの起動）、それからまっすぐ走り続ける。
    // -DLEON_START_X=225 -DLEON_START_Z=204 -DLEON_START_YAW=106 で納屋の壁に正面からぶつかる。
    return frame < 540 ? 0 : KEY_UP | KEY_B;
#endif
    u32 t = frame % 600;
    if (t < 120) return KEY_UP;                 // 歩き
    if (t < 180) return KEY_UP | KEY_LEFT;      // 回りながら歩く
    if (t < 330) return KEY_UP | KEY_B;         // 走り
    if (t < 390) return KEY_RIGHT;              // その場で回る
    if (t < 480) return KEY_DOWN;               // 後ろへ歩く
    return 0;                                   // 待機
}
#endif

// 原作のもや（キャプチャから採った：空と遠景は 100,100,78）。各ステージは自分のものを設定する
// （stage_info[].haze、ミッションファイルから）。
#define HAZE_R 12
#define HAZE_G 12
#define HAZE_B 9
#define NEAR_PLANE 1.0f
#ifndef FAR_PLANE
#define FAR_PLANE  700.0f
#endif
#define POLY_HIGH      1940   // フレームがこれだけ埋まった：ステージの送る量を減らす（ハードウェアは2048まで）
#define POLY_LOW       1800   // …これだけ空いた：また多く送る
#define POLY_STAGE_MIN 1200   // （送る三角形、近いものから：これより少なくはしない）
static int stage_budget = -1; // 1フレームに送るステージの三角形（-1：全部）
// ステージの下限に達してもまだフレームが埋まりすぎているときは、代わりに一番遠い敵を省く
// （もやの中。レオンの目の前の壁は残す必要がある）。
#define ENEMY_SHOW_MIN 2
static int enemy_show = ENEMY_MAX;
static int stage_used;   // このフレームでステージが使ったポリゴン
#define TIME_CAP_MIN   1200   // （フレームレートが遅いときのステージの三角形：これより少なくはしない。800だと壁の近くで欠けた）
static int time_cap = 4000, enemy_cap = ENEMY_MAX;   // フレームにかかる時間からの上限（メインループを参照）
#define FOG_START  15.0f   // ワールド単位：これより近くには霧をかけない
#define FOG_SCALE  130.0f  // 濃さ = 1 - exp(-(d - FOG_START) / FOG_SCALE)：80で約40%、200で約75%

// 視点からの距離 d にある点の15ビットの深度バッファの値（Z バッファ、NEAR_PLANE/FAR_PLANE の
// 透視投影）と、その逆。
static float depth_of(float d)
{
    return 32767.0f * FAR_PLANE / (FAR_PLANE - NEAR_PLANE) * (1.0f - NEAR_PLANE / d);
}

static float distance_of(float depth)
{
    float q = depth / 32767.0f * (FAR_PLANE - NEAR_PLANE) / FAR_PLANE;
    return q >= 1.0f ? 1e9f : NEAR_PLANE / (1.0f - q);
}

// ハードウェアの霧：32個の表のエントリーは深度 FOG_OFFSET + (n + 1) * (0x400 >> shift) にある。
// 深度は距離に対して直線からほど遠いので、各エントリーの濃さはその深度に対応する距離から計算する。

static void setup_fog(void)
{
    const int shift = 3;   // 1エントリーあたり深度128：約10から約700単位をカバー
    int offset = (int)depth_of(FOG_START);
    glEnable(GL_FOG);
    glFogShift(shift);
    glFogOffset(offset);
    glFogColor(HAZE_R, HAZE_G, HAZE_B, 31);
    for (int n = 0; n < 32; n++)
    {
        float d = distance_of(offset + (n + 1) * (0x400 >> shift));
        float density = d <= FOG_START ? 0.0f : 1.0f - expf(-(d - FOG_START) / FOG_SCALE);
        int v = (int)(density * 127.0f + 0.5f);
        glFogDensity(n, v > 127 ? 127 : v);
    }
}

// ステージ自身の霧、原作が設定するとおり（GM_CtrlMain_Camera::Set_Fog：線形の霧、タイプ1、
// ミッションファイルの FogDis 0 から FogDis 1 まで。GM_Data_MisParam::Get_FogDis、その色は
// Get_FogCol：stage_info の haze）。遠い平面より先は最後のエントリーのまま。表の最初のエントリーは
// 霧の始まり（その深度）にあり、各エントリーは深度128ずつ先。
static void stage_fog(const s16 fog[2])
{
#ifdef TEST_OLD_FOG   // テスト用ビルド：比較用の、移植版の以前の霧
    (void)fog;
    setup_fog();
    return;
#endif
    float from = fog[0] < 1 ? 1.0f : (float)fog[0], to = fog[1] > FAR_PLANE ? FAR_PLANE : (float)fog[1];
    if (to <= from + 1.0f)
        to = from + 1.0f;
    int offset = (int)depth_of(from);
    // 深度は遠くで詰まる（距離10単位が、近くでは深度約3000、600では約1）：エントリーの刻み
    // （0x400 >> shift）は、32個で from..to をまだ覆える一番細かいもの。
    int span = (int)depth_of(to) - offset, shift = 10;
    while (shift > 0 && (0x400 >> shift) * 32 < span)
        shift--;
    if (offset + 32 * (0x400 >> shift) > 0x7fff)
        offset = 0x7fff - 32 * (0x400 >> shift);
    if (offset < 0)
        offset = 0;   // （11面の霧は0から：深度の全範囲、刻み0x400。以前は -1 になり、アサーションで止まった）
    glFogShift(shift);
    glFogOffset(offset);
    for (int n = 0; n < 32; n++)
    {
        float d = distance_of(offset + (n + 1) * (0x400 >> shift));
        float density = d <= from ? 0.0f : d >= to ? 1.0f : (d - from) / (to - from);
        int v = (int)(density * 127.0f + 0.5f);
        glFogDensity(n, v > 127 ? 127 : v);
    }
}

// マズルフラッシュと着弾用の小さな半透明の「星」（交差した3枚の正方形）。
static void draw_star(const int pos[3], int size, u16 color, int alpha, int id)
{
    model_sync();
    glPushMatrix();
    glTranslatef32(pos[0], pos[1], pos[2]);
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(id));
    glBegin(GL_TRIANGLES);
    glColor(color);
    for (int plane = 0; plane < 3; plane++)
    {
        static const int corner[6][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, -1 }, { 1, 1 }, { -1, 1 } };
        for (int c = 0; c < 6; c++)
        {
            int a = corner[c][0] * size, b = corner[c][1] * size;
            if (plane == 0) glVertex3v16(a, b, 0);
            else if (plane == 1) glVertex3v16(0, a, b);
            else glVertex3v16(a, 0, b);
        }
    }
    glEnd();
    glPopMatrix(1);
}

// 1ピクセルの線（ハードウェアは2つの頂点が同じ三角形を線として描く）。
static void draw_line(const int from[3], const int to[3], u16 color, int id)
{
    model_sync();
    glPushMatrix();
    glTranslatef32(from[0], from[1], from[2]);
    glScalef32(inttof32(64), inttof32(64), inttof32(64));   // 頂点を v16 の範囲内に保つ
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(id));
    glBegin(GL_TRIANGLES);
    glColor(color);
    int dx = (to[0] - from[0]) / 64, dy = (to[1] - from[1]) / 64, dz = (to[2] - from[2]) / 64;
    glVertex3v16(0, 0, 0);
    glVertex3v16(dx, dy, dz);
    glVertex3v16(dx, dy, dz);
    glEnd();
    glPopMatrix(1);
}

// hud.bin の HUD の部品、2d_gms_00.mct の右半分（x 128..255。下の座標はシートの中のもの）：
// 弾数の枠と体力のトラックのあるレオンのプレート、ゲージの中身、弾数の数字。
#define PLATE_SX 29          // プレート：99 x 25
#define PLATE_SY 59
#define PLATE_W  99
#define PLATE_H  25
#define PLATE_X  (256 - PLATE_W - 3)   // 画面上：右下
#define PLATE_Y  (192 - PLATE_H - 4)
#define TRACK_X  1           // プレートの中の体力のトラック：96 x 4
#define TRACK_Y  19
#define TRACK_W  96
#define TRACK_H  4
#define LIFE_BAR_X 120       // レオンの体力バーはここから始まる（動画：画面の中央の下）
#define FILL_SX  90          // 緑と赤のバーの中央の列、トラックに沿って引き伸ばす
#define GREEN_SY 110
#define RED_SY   118
#define DIGIT_SY 0           // 弾数の数字：8 x 15（「1」は幅4）
#define DIGIT_H  15
#define AMMO_X   25          // プレートの弾数の枠（3つのうち2番目と3番目）
#define AMMO_Y   1

static void draw_digit(int d, int x, int y)
{
    int sx = d == 0 ? 24 : d == 1 ? 36 : 40 + (d - 2) * 8, w = d == 1 ? 4 : 8;
    overlay_sheet(&hud, sx, DIGIT_SY, w, DIGIT_H, x + (8 - w) / 2, y, w, DIGIT_H, RGB15(31, 31, 31), 31, 24);
}

// レオンのプレート（右下）と体力ゲージ、緑 FINE / 黄 CAUTION / 赤 DANGER、そしてマガジンの弾数。
// 傷を負うと赤く光り、死ぬと暗い赤のベールがかかる。拾ったばかりのアイテムのアイコン
// （ポップアップの残りフレーム、0 = なし）。
#define POPUP_FRAMES 120
#define SPARE_LINE   11   // ゲームで使う文字の行（メニューが消す）
#define POPUP_LINE   10
#define CLEAR_LINE   9
#define PROMPT_LINE  8
#define MEDAL_LINE   7
#define OBJ_LINE     5
#define OBJ_TITLE_LINE 4
#define OBJ_HINT_LINE 3
// オプション（ポーズメニュー。サウンドと同じくそのセッションの間）：HUD（レオンとアシュリーの
// プレート）の表示、下画面の表示（0 マップ、既定。1 アイテム選択）。
static int hud_on = 1, bottom_items = 0;
static void draw_plate(int hp, int ammo, int dy);

// 原作のボーナス表示（GM_MisConduct::Set_Bounus -> その表示エフェクト。Zeebo の動画では
// Dano Critico!、Tecnico!、Estrategico!、Joia Adquirida!）、英語で左上に：新しいものが上、
// 最大2つ、それぞれ BONUS_FRAMES。
#define BONUS_FRAMES 90
#define BONUS_LINE   12   // （文字の行12と13：幅128ピクセルまで）
static const char *bonus_text[2];
static int bonus_t[2], felled_by_blast;
static void bonus_note(const char *s)
{
    bonus_text[1] = bonus_text[0];
    bonus_t[1] = bonus_t[0];
    bonus_text[0] = s;
    bonus_t[0] = BONUS_FRAMES;
}
static void bonus_draw(void)
{
    for (int k = 0; k < 2; k++)
    {
        if (bonus_t[k] > 0)
        {
            bonus_t[k]--;
            text_line(BONUS_LINE + k, 4, 4 + k * 15, TEXT_GOLD, bonus_text[k]);
        }
        else
            text_hide(BONUS_LINE + k);
    }
}
static void draw_hud(int hp, int ammo, int flash, int dead, int popup_type, int popup_value, int popup)
{
    overlay_begin();
    if (popup > 0)
    {
        // ポップアップの文（文字の行 POPUP_LINE）の左のアイコン、最初の数フレームで上がって来る。
        int ix, iy;
        drops_icon(popup_type, popup_value, &ix, &iy);
        int rise = popup > POPUP_FRAMES - 8 ? (popup - (POPUP_FRAMES - 8)) * 2 : 0;
        int alpha = popup < 16 ? popup * 2 : 31;
        overlay_rect(20, 128 + rise, 236, 156 + rise, RGB15(0, 0, 0), alpha / 2 + 1, 25);
        overlay_sheet(&items, ix, iy, 32, 24, 26, 130 + rise, 32, 24, RGB15(31, 31, 31), alpha, 26);
    }
    if (dead)
        overlay_rect(0, 0, 256, 192, RGB15(10, 0, 0), 14, 20);
    else if (flash > 0)
        overlay_rect(0, 0, 256, 192, RGB15(31, 0, 0), flash, 20);

    if (hud_on)
        draw_plate(hp, ammo, 0);
    overlay_end();
}

// レオンのプレート：体力バーとマガジンの弾数（HUD のオプションでプレイ中は隠れる。
// ステータス画面でも、所持金のバーの上に dy ピクセル高く出す）。
static void draw_plate(int hp, int ammo, int dy)
{
    if (ammo > 99)
        ammo = 99;
    overlay_sheet(&hud, PLATE_SX, PLATE_SY, PLATE_W, PLATE_H, PLATE_X, PLATE_Y - dy, PLATE_W, PLATE_H, RGB15(31, 31, 31), 31, 21);
    // Zeebo 版が描くとおりの体力バー：長く、画面の中央の下からプレートの右端まで、失った分は黒。
    // 左から減る。
    int right = PLATE_X + TRACK_X + TRACK_W, len = right - LIFE_BAR_X, y = PLATE_Y - dy + TRACK_Y;
    overlay_rect(LIFE_BAR_X - 1, y - 1, right + 1, y + TRACK_H + 1, RGB15(0, 0, 0), 31, 21);
    int fill = len * hp / LEON_HP, x = right - fill;
    if (fill > 0)
    {
        // 注意：緑の上に半分重ねた赤いバーで黄土色になる。
        int danger = hp <= LEON_HP / 3, caution = !danger && hp <= LEON_HP * 2 / 3;
        overlay_sheet(&hud, FILL_SX, danger ? RED_SY : GREEN_SY, 4, TRACK_H, x, y, fill, TRACK_H, RGB15(31, 31, 31), 31, 22);
        if (caution)
            overlay_sheet(&hud, FILL_SX, RED_SY, 4, TRACK_H, x, y, fill, TRACK_H, RGB15(31, 31, 31), 16, 23);
    }
    if (ammo >= 10)
        draw_digit(ammo / 10 % 10, PLATE_X + AMMO_X, PLATE_Y - dy + AMMO_Y);
    draw_digit(ammo % 10, PLATE_X + AMMO_X + 8, PLATE_Y - dy + AMMO_Y);
}

// pos (x, z) を (dx, dz) に沿って dist だけ、歩くときと同じように1単位ずつ動かす。高さ y の床での
// 壁から外に保つ（巨人の背中から降りるとき：横の柵を絶対に抜けない）。
static void walk_out(const Ground *g, int pos[3], int dx, int dz, int dist, int y)
{
    for (int d = 0; d < dist; d += inttof32(1))
    {
        int step = dist - d < inttof32(1) ? dist - d : inttof32(1);
        pos[0] += mulf32(dx, step);
        pos[2] += mulf32(dz, step);
        ground_collide(g, &pos[0], &pos[2], y + STEP_UP, y + LEON_HEIGHT, LEON_RADIUS);
    }
}

// ステージの原作の既定のカメラ（CAM_BACK を参照）：そのピッチ（1回転 65536）と、回転の中心点の
// 高さ（1/256 単位）。
static void cam_setup(int stage)
{
    int giants = stage == 3 || stage == 10;
    float pitch = (giants ? -200 : stage == 9 ? 2400 : 600) * 6.2831853f / 65536.0f, yaw = 1200 * 6.2831853f / 65536.0f;
    float pivot_up = (giants ? 4280 : 4080) / 256.0f, pivot_back = 670 / 256.0f, pivot_right = 670 / 256.0f, dis = 5000 / 256.0f;
    float back = pivot_back + dis * cosf(pitch) * cosf(yaw), right = pivot_right + dis * cosf(pitch) * sinf(yaw);
    float up = pivot_up + dis * sinf(pitch), ahead = back + f32tofloat(CAM_AHEAD);
    cam_back = floattof32(back);
    cam_right = floattof32(right);
    cam_height = floattof32(up);
    cam_at_right = floattof32(right - ahead * tanf(yaw));
    cam_at_height = floattof32(up - ahead * tanf(pitch) / cosf(yaw));
}

// 視野の幅（度）：スコープで狭くなる。
static void set_fov(int deg)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspectivef32(degreesToAngle(deg), floattof32(256.0 / 192.0), floattof32(NEAR_PLANE), floattof32(FAR_PLANE));
    glMatrixMode(GL_MODELVIEW);
}

// 動画に映るとおりのライフルのスコープ：中央の澄んだ丸いレンズのまわりを暗くした視界、その黒い縁、
// 赤い十字線。
static void draw_scope(void)
{
    overlay_begin();
    const int cx = 128 * 64, cy = 96 * 64, r = SCOPE_R * 64, rim = (SCOPE_R + 2) * 64, far = 300 * 64;
    glBindTexture(0, 0);
    for (int pass = 0; pass < 2; pass++)
    {
        // ベール（半透明）を画面の端の外まで、それから縁（不透明）。
        int r0 = pass ? r : rim, r1 = pass ? rim : far;
        glPolyFmt(POLY_ALPHA(pass ? 31 : 20) | POLY_CULL_NONE | POLY_ID(40 + pass));
        glBegin(GL_QUADS);
        glColor(pass ? RGB15(1, 1, 1) : RGB15(10, 10, 10));
        for (int i = 0; i < 32; i++)
        {
            int a0 = i * (DEGREES_IN_CIRCLE / 32), a1 = (i + 1) * (DEGREES_IN_CIRCLE / 32);
            int x0 = mulf32(cosLerp(a0), inttof32(1)), y0 = mulf32(sinLerp(a0), inttof32(1));
            int x1 = mulf32(cosLerp(a1), inttof32(1)), y1 = mulf32(sinLerp(a1), inttof32(1));
            glVertex3v16(cx + mulf32(x0, r0), cy + mulf32(y0, r0), 2048);
            glVertex3v16(cx + mulf32(x1, r0), cy + mulf32(y1, r0), 2048);
            glVertex3v16(cx + mulf32(x1, r1), cy + mulf32(y1, r1), 2048);
            glVertex3v16(cx + mulf32(x0, r1), cy + mulf32(y0, r1), 2048);
        }
        glEnd();
    }
    overlay_rect(128 - SCOPE_R + 4, 96, 128 + SCOPE_R - 4, 97, RGB15(31, 2, 2), 31, 42);   // 十字線
    overlay_rect(128, 96 - SCOPE_R + 4, 129, 96 + SCOPE_R - 4, RGB15(31, 2, 2), 31, 43);
    overlay_end();
}

// アシュリーのプレート（2d_gms_00、原作のもの：彼女の名前、体力のトラック、丸い指示のアイコン）、
// 彼女が一緒にいる間はレオンのものの上に：体力は彼と同じくトラックに、円の中には彼について行く
// （または2つ目、待つ）アイコン。
#define ASH_PLATE_SX 24       // （HUD のシート、2d_gms_00 の右半分の中）：104 x 27
#define ASH_PLATE_SY 32
#define ASH_PLATE_W  104
#define ASH_PLATE_H  27
#define ASH_PLATE_X  (256 - ASH_PLATE_W - 3)
#define ASH_PLATE_Y  (PLATE_Y - ASH_PLATE_H + 3)   // （動画のとおり、レオンのもののすぐ上）
#define ASH_TRACK_X  2
#define ASH_TRACK_Y  14
#define ASH_TRACK_W  75
#define ASH_ICON_SX  20       // 指示のアイコン、16 x 16 が横に並ぶ
#define ASH_ICON_SY  16
#define ASH_ICON_X   86       // プレートの中の円の内側
#define ASH_ICON_Y   9

static void draw_ash_hud(int hp, int max, int waiting)
{
    overlay_begin();
    overlay_sheet(&hud, ASH_PLATE_SX, ASH_PLATE_SY, ASH_PLATE_W, ASH_PLATE_H, ASH_PLATE_X, ASH_PLATE_Y, ASH_PLATE_W, ASH_PLATE_H, RGB15(31, 31, 31), 31, 21);
    int fill = hp > 0 ? (ASH_TRACK_W * hp + max - 1) / max : 0, y = ASH_PLATE_Y + ASH_TRACK_Y;
    int x = ASH_PLATE_X + ASH_TRACK_X + ASH_TRACK_W - fill;   // （レオンと同じく左から減る）
    if (fill > 0)
    {
        int danger = hp <= max / 3, caution = !danger && hp <= max * 2 / 3;
        overlay_sheet(&hud, FILL_SX, danger ? RED_SY : GREEN_SY, 4, TRACK_H, x, y, fill, TRACK_H, RGB15(31, 31, 31), 31, 22);
        if (caution)
            overlay_sheet(&hud, FILL_SX, RED_SY, 4, TRACK_H, x, y, fill, TRACK_H, RGB15(31, 31, 31), 16, 23);
    }
    overlay_sheet(&hud, ASH_ICON_SX + (waiting ? 16 : 0), ASH_ICON_SY, 16, 16, ASH_PLATE_X + ASH_ICON_X, ASH_PLATE_Y + ASH_ICON_Y, 16, 16, RGB15(31, 31, 31), 31, 24);
    overlay_end();
}

// ドロップコード（敵のレコードのバイト8、種類04の物のバイト2）が落とすもの。原作のドロップの関数の
// とおり（bio4_brew.mod 0x49f48。コードのアイテムと数は原作のもの、抽選は rand() % n か & 3。
// ff と15より大きいものは何も落とさず、0も同じ）：1 グリーンハーブ、2 救急スプレー、3 手榴弾、
// 4 閃光手榴弾、5 600-1000 PTAS、6 1100-1500 PTAS、7 ハンドガン10 / ショットガン5 / ライフル3 /
// TMP 50発、8 ライフル3 / TMP 50 / マグナム5 / マインダーツ5、9 手に持っている銃の弾、10 なし /
// ハーブ / スプレー、11 なし / 卵、12 なし / ハンドガン10 / ショットガン5 / ライフル3 / 手榴弾 /
// ハーブ、13 なし / TMP 50 / マグナム5 / ダーツ5 / 閃光手榴弾 / ハーブ、14と15 宝（スピネル、
// ブルーベルベット：item_base_info.bin の2000と3000 PTAS）。移植版には手榴弾、マインダーツ、
// スプレー、卵が無い：それらは何も落とさない。ただし回復するスプレーと卵は代わりにグリーンハーブを
// 落とす。
// 原作が落とすどの銃の弾も、移植版では1つのアイテム：多機能弾丸。拾ったときに手に持っている銃の弾
// （今はどの銃も共有する1つの数に MULTI_DROP 発。下のドロップコード自身の数は参考に残す：
// ハンドガン10、マグナム5、ショットガン5、ライフル3、TMP 50。multi_ammo_gun() がアイコンを選ぶ：
// 手に持っている銃の箱、ロケットランチャーのときはハンドガンのもの）。
enum { DROP_ROUNDS_HANDGUN = 10, DROP_ROUNDS_MAGNUM = 5, DROP_ROUNDS_SHOTGUN = 5, DROP_ROUNDS_RIFLE = 3, DROP_ROUNDS_TMP = 50 };
static const u8 drop_rounds[] = { DROP_ROUNDS_HANDGUN, DROP_ROUNDS_MAGNUM, DROP_ROUNDS_SHOTGUN, DROP_ROUNDS_RIFLE, DROP_ROUNDS_TMP };
static int multi_ammo_gun(int w) { return w < (int)sizeof(drop_rounds) ? w : WEP_HANDGUN; }
static void code_drop(const int ground[3], int code)
{
    // （-1：なし、-2：ハーブ、それ以外は銃の弾：多機能弾丸）
    static const s8 c7[4] = { WEP_HANDGUN, WEP_SHOTGUN, WEP_RIFLE, WEP_TMP }, c8[4] = { WEP_RIFLE, WEP_TMP, WEP_MAGNUM, -1 };
    static const s8 c12[6] = { -1, WEP_HANDGUN, WEP_SHOTGUN, WEP_RIFLE, -1, -2 }, c13[6] = { -1, WEP_TMP, WEP_MAGNUM, -1, -1, -2 };
    int type = -1, value = 0, w = -1;
    switch (code)
    {
    case 1: case 2: type = DROP_HERB; break;
    case 5: type = DROP_MONEY; value = 600 + (int)(rand() % 5) * 100; break;
    case 6: type = DROP_MONEY; value = 1100 + (int)(rand() % 5) * 100; break;
    case 7: w = c7[rand() & 3]; break;
    case 8: w = c8[rand() & 3]; break;
    case 9: w = WEP_HANDGUN; break;
    case 10: type = rand() % 3 ? DROP_HERB : -1; break;
    case 11: type = rand() & 3 ? DROP_HERB : -1; break;
    case 12: w = c12[rand() % 6]; break;
    case 13: w = c13[rand() % 6]; break;
    case 14: type = DROP_TREASURE; value = 2000; break;
    case 15: type = DROP_TREASURE; value = 3000; break;
    }
    if (w == -2)
        type = DROP_HERB;
    else if (w >= 0)
        type = DROP_AMMO;
    if (type >= 0)
        drops_spawn_value(&drops, ground, type, DROP_DELAY, value);
}

static void mission_drop(const Enemy *e)
{
    int ground[3] = { e->pos[0], e->feet, e->pos[2] };
    code_drop(ground, e->drop);
}

// 倒れるガナードは、そのレコードのドロップコードが言うものを落とす（なしなら何も）。
static void maybe_drop(const Enemy *e, int leon_hp, int percent)
{
    (void)leon_hp;
    (void)percent;
    if (e->drop >= 0)
        mission_drop(e);
}

// 壊れた木箱は、その地点のバイト2が言うものを落とす（ff なら何も）。
// テーブルや棚の上のもの（地面のデータに自分の床が無い家具）：落としたものは、レオンの側の家具の
// 前の床に落ち、彼が拾える（以前は木箱の高さ、テーブルの天板の中にとどまり、その壁の奥で
// 彼の手が届かなかった）。
static void crate_drop(const Obj *o, const int leon_at[3])
{
    if (o->drop < 0)
        return;
    int floor = o->pos[1];
    ground_height(&stage_ground, o->pos[0], o->pos[2], o->pos[1], 0, &floor);
    if (o->pos[1] - floor <= inttof32(3))
    {
        code_drop(o->pos, o->drop);
        return;
    }
    int dx = leon_at[0] - o->pos[0], dz = leon_at[2] - o->pos[2];
    int len = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
    int at[3] = { leon_at[0], leon_at[1], leon_at[2] };   // （見つからない：彼の足元に）
    if (len > 0)
    {
        int ux = divf32(dx, len), uz = divf32(dz, len);
        for (int d = inttof32(2); d <= inttof32(20); d += inttof32(1))
        {
            int x = o->pos[0] + mulf32(ux, d), z = o->pos[2] + mulf32(uz, d), y = leon_at[1];
            if (!ground_height(&stage_ground, x, z, leon_at[1] + STEP_UP, STEP_UP * 2, &y))
                continue;
            int cx = x, cz = z;
            ground_collide(&stage_ground, &cx, &cz, y + STEP_UP, y + inttof32(10), inttof32(3));
            if (cx != x || cz != z)
                continue;   // （まだ家具に接している）
            at[0] = x; at[1] = y; at[2] = z;
            break;
        }
    }
    code_drop(at, o->drop);
}
// ステータス画面（SELECT）：原作の Status 画面を武器の一覧（Armas）で開いたもの。レイアウトの
// ファイルが 320 x 240 の画面に置くとおりに（Bio4_ImageLayout.bin のレコード285-293と356-402、
// Bio4_StringLayout.bin の444-485）、ここでは中心を基準に0.8倍：2d_gms_01（メニューのシート、
// 開いている間だけ読む）の枠のバーと一覧のパネル、4行のアイテムのアイコン（装備中の武器に E）、
// 名前、弾の印付きの数、ケースでの大きさ（TAM）、選んだ行は赤い枠（2d_gms_00、HUD のシート）、
// 下に所持金とケースの使用量、右に銃を持ったレオン。行で A を押すと小さなウィンドウにその
// コマンドを開く。
#define ST_X(x)   (128 + (x) * 4 / 5)
#define ST_Y(y)   (96 + (y) * 4 / 5)
#define ST_S(n)   ((n) * 4 / 5)
#define ST_ROWS   4
// ケースでの大きさ（item_base_info.bin、値段の次のバイト）：ハンドガン6、弾 / ハーブ2。
enum { ST_GUN, ST_AMMO, ST_HERB };
static const u8 st_tam[] = { 6, 2, 2 };

// アイテムのシート（2d_itemic_00、item_icon_idx.bin の配置）でのアイテムのアイコンの位置：
// 上の行が銃、次が弾。シートの最後の行は最初の行の右半分。
static void item_icon(int item, int *sx, int *sy)
{
    static const struct { u8 item, x, y; } at[] = {
        { 4, 0, 0 }, { 6, 32, 0 }, { 7, 64, 0 }, { 5, 96, 0 }, { 8, 0, 96 },
        { 20, 64, 96 }, { 22, 96, 96 }, { 23, 0, 24 }, { 21, 32, 24 }, { 24, 64, 24 }, { 9, 0, 48 },
        { 71, 96, 72 } };   // （ベスト：3つとも同じ絵、宝の横）
    for (unsigned i = 0; i < sizeof(at) / sizeof(at[0]); i++)
        if (at[i].item == item)
        {
            *sx = at[i].x;
            *sy = at[i].y;
            return;
        }
    *sx = *sy = 0;
}
#define AMMO_ITEM(w) ((w) == WEP_HANDGUN ? 20 : (w) == WEP_MAGNUM ? 21 : (w) == WEP_SHOTGUN ? 22 : (w) == WEP_RIFLE ? 23 : 24)
typedef struct { int type, count, weapon; } StItem;
static Sheet menu_sheet;
static int menu_loaded;

// （ST_S(w) ではなく、拡大縮小した両端から大きさを決める：端と端を並べたパネルがぴったり接する。
// 大きさを個別に丸めると間に1ピクセルの黒い行が残った。）
static void menu_piece(int sx, int sy, int sw, int sh, int x, int y, int w, int h)
{
    overlay_sheet(&menu_sheet, sx, sy, sw, sh, ST_X(x), ST_Y(y), ST_X(x + w) - ST_X(x), ST_Y(y + h) - ST_Y(y), RGB15(31, 31, 31), 31, 25);
}

// 小さなウィンドウ（コマンド）：黒い内側のまわりに 2d_gms_01 の角と辺。左上の角が (x, y)、
// 内側は w x h（レイアウトの単位）。
static void menu_window(int x, int y, int w, int h)
{
    if (!menu_loaded)
    {
        overlay_rect(ST_X(x), ST_Y(y), ST_X(x + w + 32), ST_Y(y + h + 32), RGB15(2, 2, 2), 31, 25);
        return;
    }
    overlay_rect(ST_X(x + 16), ST_Y(y + 16), ST_X(x + 16 + w), ST_Y(y + 16 + h), RGB15(0, 0, 0), 31, 25);
    menu_piece(0, 80, 16, 16, x, y, 16, 16);
    menu_piece(0, 64, 176, 16, x + 16, y, w, 16);
    menu_piece(16, 80, 16, 16, x + 16 + w, y, 16, 16);
    menu_piece(240, 0, 16, 112, x, y + 16, 16, h);
    menu_piece(240, 0, 16, 112, x + 16 + w, y + 16, 16, h);
    menu_piece(32, 80, 16, 16, x, y + 16 + h, 16, 16);
    menu_piece(0, 64, 176, 16, x + 16, y + 16 + h, w, 16);
    menu_piece(48, 80, 16, 16, x + 16 + w, y + 16 + h, 16, 16);
}

#define ST_PLATE_DY 29   // ステータス画面のレオンのプレート：プレイ中の位置よりこれだけ上（所持金のバーの上端に）
// 2D の部分：枠、行（アイコン、印、選んだ行の枠）、矢印、コマンドのウィンドウ。
static void draw_status(const StItem *it, int n, int top, int sel, int cmds)
{
    overlay_begin();
    if (menu_loaded)
    {
        menu_piece(0, 0, 240, 32, -160, -112, 320, 32);       // 上のバー
        menu_piece(80, 96, 156, 26, -160, -85, 208, 26);      // 一覧の見出し
        for (int i = 0; i < 5; i++)
            menu_piece(80, 32, 156, 32, -160, -59 + 32 * i, 208, 32);   // 一覧のパネル
        menu_piece(0, 0, 240, 32, -160, 80, 320, 32);         // 下のバー
        menu_piece(203, 64, 37, 29, 54, 80, 37, 29);          // アタッシュケース
    }
    else   // （メニューのシートの空きが無い：無地のパネル）
    {
        overlay_rect(0, 0, 256, ST_Y(-80), RGB15(4, 3, 2), 31, 25);
        overlay_rect(0, ST_Y(-85), ST_X(48), ST_Y(80), RGB15(3, 3, 4), 31, 25);
        overlay_rect(0, ST_Y(80), 256, 192, RGB15(4, 3, 2), 31, 25);
    }
    for (int r = 0; r < ST_ROWS && top + r < n; r++)
    {
        const StItem *t = &it[top + r];
        int ix, iy, dy = 31 * r;
        if (t->type == ST_HERB)
            drops_icon(DROP_HERB, 0, &ix, &iy);
        else
            item_icon(t->type == ST_GUN ? weapons[t->weapon].item : AMMO_ITEM(t->weapon), &ix, &iy);
        overlay_rect(ST_X(-139), ST_Y(-59 + dy), ST_X(-139) + ST_S(32), ST_Y(-59 + dy) + ST_S(24), RGB15(1, 1, 1), 31, 26);   // （その暗い箱）
        overlay_sheet(&items, ix, iy, 32, 24, ST_X(-139), ST_Y(-59 + dy), ST_S(32), ST_S(24), RGB15(31, 31, 31), 31, 26);
        if (menu_loaded)
        {
            if (t->type == ST_GUN && t->weapon == wep)
                menu_piece(240, 112, 6, 7, -137, -58 + dy, 6, 7);    // E：装備中
        }
        if (top + r == sel)
        {
            // 赤い枠（原作のものは 2d_gms_00 240,84。0.8倍だと消える細い線）。
            int x0 = ST_X(-140), y0 = ST_Y(-61 + dy), x1 = ST_X(38), y1 = y0 + ST_S(31) - 1;
            overlay_line(x0, y0, x1, y0, RGB15(27, 7, 7), 27);
            overlay_line(x0, y1, x1, y1, RGB15(27, 7, 7), 27);
            overlay_line(x0, y0, x0, y1, RGB15(27, 7, 7), 27);
            overlay_line(x1, y0, x1, y1, RGB15(27, 7, 7), 27);
        }
    }
    // 上 / 下にまだ行がある：原作が矢印を置く所に小さな三角形（-51、-77 / 66）。
    for (int k = 0; k < 2; k++)
        if (k == 0 ? top > 0 : top + ST_ROWS < n)
            for (int j = 0; j < 4; j++)
            {
                int y = k == 0 ? ST_Y(-77) + 2 + j : ST_Y(66) + 5 - j;
                overlay_line(ST_X(-51) + 4 - j, y, ST_X(-51) + 4 + j, y, RGB15(31, 27, 8), 28);
            }
    if (cmds)
        menu_window(52, -42, 64, 16 * cmds);
    overlay_end();
}

// 武器 w を持つ：前のものの代わりにそのモデルとアニメーション。各銃はマガジンの中身を保つ。
// 持っている弾はどの銃にも共通の1つの数、多機能弾丸（*spare はそのまま。ロケットランチャーは
// 込める弾が無い）。何か足りなければ：ハンドガンに戻る。
static void *wep_buf[2];

static void weapon_switch(int w, int *ammo, int *spare)
{
    (void)spare;
    wep_ammo[wep] = *ammo;
    model_free(&gun);
    free(wep_buf[0]);
    free(wep_buf[1]);
    wep_buf[0] = wep_buf[1] = NULL;
    int ok = 0;
    if (weapons[w].model && (wep_buf[0] = read_file(weapons[w].model)) && (wep_buf[1] = read_file(weapons[w].anim)))
        ok = model_load(&gun, wep_buf[0], file_size(weapons[w].model)) == 0
             && anim_load(&leon_gun, wep_buf[1], file_size(weapons[w].anim)) == 0 && leon_gun.nodes == leon.node_count;
    if (!ok)
    {
        w = WEP_HANDGUN;
        model_load(&gun, gun_bin, gun_bin_size);
        anim_load(&leon_gun, leon_gun_bin, leon_gun_bin_size);
    }
    wep = w;
    *ammo = wep_ammo[w];
}

// ケース：銃はその大きさ、多機能弾丸の1箱（MULTI_BOX 発、一部だけ入ったものも）かハーブは2。
static int case_used(int spare, int herbs)
{
    int used = herbs * 2 + (spare + MULTI_BOX - 1) / MULTI_BOX * 2;
    for (int w = 0; w < WEP_COUNT; w++)
        if (wep_owned >> w & 1)
            used += weapons[w].tam;
    return used;
}

// 改造の一覧用の性能（x 10 で持っている）：13、1.2。
static void stat_text(char *s, int v)
{
    if (v % 10)
        sprintf(s, "%d.%d", v / 10, v % 10);
    else
        sprintf(s, "%d", v / 10);
}

// セーブファイル、ROM を起動したカードに（ファイルシステムの無いカートリッジには何も：そのときは
// 覚えないだけ）。まず2つ目のファイルに書くので、電源が切れても最後の正常なものは失われない。
#define SAVE_NAME   "re4ds.sav"
#define SAVE_MAGIC  0x53344552    // 「RE4S」
#define SAVE_VER    3             // 2：ベスト（防具）。3：前のランク（店の割引）。古いファイルはそれ無しで読む
typedef struct {
    u32 magic, version;
    int ptas, owned, case_level, herbs, weapon;
    int spare[WEP_COUNT];
    u8 level[WEP_COUNT][WEP_STATS];
    int armor;
    int last_rank;
    u32 check;
} SaveData;
#define SAVE_V1_SIZE offsetof(SaveData, armor)   // （バージョン1：同じ、次にそのチェック）
#define SAVE_V2_SIZE offsetof(SaveData, last_rank)   // （バージョン2：ベストまで、次にそのチェック）
static SaveData save_data;
static int save_ok, have_save;

static u32 save_sum(const SaveData *d, size_t n)
{
    u32 sum = 0x1234;
    const u8 *b = (const u8 *)d;
    for (size_t i = 0; i < n; i++)
        sum = sum * 31 + b[i];
    return sum;
}

static void save_path(char *s, size_t n, const char *name)
{
    snprintf(s, n, "%s%s", fatGetDefaultDrive(), name);
}

static void save_read(void)
{
    char path[32];
    save_ok = fatInitDefault();
    if (!save_ok)
        return;
    save_path(path, sizeof(path), SAVE_NAME);
    FILE *f = fopen(path, "rb");
    if (!f)
        return;
    SaveData d;
    memset(&d, 0, sizeof(d));
    size_t got = fread(&d, 1, sizeof(d), f);
    fclose(f);
    int good = 0;
    if (got == sizeof(d) && d.version == SAVE_VER)
        good = d.check == save_sum(&d, offsetof(SaveData, check));
    else if (got == SAVE_V1_SIZE + sizeof(u32) && d.version == 1)
    {
        u32 check;
        memcpy(&check, (const u8 *)&d + SAVE_V1_SIZE, sizeof(check));
        good = check == save_sum(&d, SAVE_V1_SIZE);
        d.armor = 0;
    }
    else if (got == SAVE_V2_SIZE + sizeof(u32) && d.version == 2)
    {
        u32 check;
        memcpy(&check, (const u8 *)&d + SAVE_V2_SIZE, sizeof(check));
        good = check == save_sum(&d, SAVE_V2_SIZE);
        d.last_rank = 0;
    }
    if (good && d.magic == SAVE_MAGIC && d.armor >= 0 && d.armor <= 3 && d.last_rank >= 0 && d.last_rank <= 4
        && d.case_level >= 0 && d.case_level <= 3 && d.weapon >= 0 && d.weapon < WEP_COUNT
        && (d.owned >> d.weapon & 1) && (d.owned & ~((1 << WEP_COUNT) - 1)) == 0)
    {
        save_data = d;
        have_save = 1;
    }
}

static void save_write(int spare, int herbs, int ptas)
{
    SaveData *d = &save_data;
    memset(d, 0, sizeof(*d));
    d->magic = SAVE_MAGIC;
    d->version = SAVE_VER;
    d->ptas = ptas;
    d->owned = wep_owned;
    d->case_level = case_level;
    d->herbs = herbs;
    d->weapon = wep;
    d->spare[0] = spare;   // （多機能弾丸：1つの数。ほかは0のまま）
    memcpy(d->level, wep_level, sizeof(d->level));
    d->armor = armor_level;
    d->last_rank = last_rank;
    d->check = save_sum(d, offsetof(SaveData, check));
    have_save = 1;
    if (!save_ok)
        return;
    char path[32], tmp[32];
    save_path(path, sizeof(path), SAVE_NAME);
    save_path(tmp, sizeof(tmp), "re4ds.tmp");
    FILE *f = fopen(tmp, "wb");
    if (!f)
        return;
    int put = fwrite(d, 1, sizeof(*d), f) == sizeof(*d);
    fclose(f);
    if (put)
    {
        remove(path);
        rename(tmp, path);
    }
}

// Continue：全部元どおり、手に持っている武器も。
static void save_apply(int *ammo, int *spare, int *herbs, int *ptas)
{
    const SaveData *d = &save_data;
    memcpy(wep_level, d->level, sizeof(wep_level));
    wep_owned = d->owned;
    case_level = d->case_level;
    armor_level = d->armor;
    last_rank = d->last_rank;
    for (int w = 0; w < WEP_COUNT; w++)
        wep_ammo[w] = wep_mag(w);
    if (d->weapon != wep)
        weapon_switch(d->weapon, ammo, spare);
    *ammo = MAG_SIZE;
    // 多機能弾丸：1つの数（以前のセーブで銃ごとに弾があれば：全部合わせる）。
    *spare = 0;
    for (int w = 0; w < WEP_COUNT; w++)
        *spare += d->spare[w] > 0 ? d->spare[w] : 0;
    *herbs = d->herbs;
    *ptas = d->ptas;
}

// ニューゲーム：ハンドガンだけ、何も買っていない、銃1つと弾少しを買えるお金。
#define NEW_GAME_PTAS 30000
static void new_game(int *ammo, int *spare, int *herbs, int *ptas)
{
    if (wep != WEP_HANDGUN)
        weapon_switch(WEP_HANDGUN, ammo, spare);
    memset(wep_level, 0, sizeof(wep_level));
    wep_owned = 1;
    case_level = 0;
    armor_level = 0;
    last_rank = 0;
    for (int w = 0; w < WEP_COUNT; w++)
        wep_ammo[w] = wep_mag(w);
    *ammo = MAG_SIZE;
    *spare = SPARE_START;
    *herbs = 0;
    *ptas = NEW_GAME_PTAS;
}

// 店の一覧：売り物1つにつき1行。枠はステータス画面のもの。
enum { SHOP_ST_TABS, SHOP_ST_LIST };
enum { ROW_WEAPON, ROW_AMMO, ROW_HERB, ROW_CASE, ROW_VEST, ROW_SELL_WEAPON, ROW_SELL_HERB, ROW_UP_WEAPON, ROW_UP_STAT };
typedef struct { int kind, arg, price, ix, iy; char name[26], note[28]; } ShopRow;
#define SHOP_ROWS 12

static void draw_shop(const ShopRow *it, int n, int top, int sel)
{
    overlay_begin();
    if (menu_loaded)
    {
        menu_piece(0, 0, 240, 32, -160, -112, 320, 32);       // 上のバー
        menu_piece(80, 96, 156, 26, -160, -85, 320, 26);      // 一覧の見出し、画面の幅
        for (int i = 0; i < 5; i++)
            menu_piece(80, 32, 156, 32, -160, -59 + 32 * i, 320, 32);   // 一覧のパネル
        menu_piece(0, 0, 240, 32, -160, 80, 320, 32);         // 下のバー
        menu_piece(203, 64, 37, 29, -150, 80, 37, 29);        // アタッシュケース、左に（ケースの大きさをその横に、所持金を右に：原作のとおり）
    }
    else
        overlay_rect(0, 0, 256, 192, RGB15(3, 3, 4), 31, 25);
    // 上のバーの枠（原作のとおり）：選んだ行のアイコンを暗い枠に、その横に商人のせりふ。
    overlay_rect(ST_X(-156), ST_Y(-110), ST_X(-156) + ST_S(36), ST_Y(-110) + ST_S(28), RGB15(1, 1, 1), 31, 26);
    {
        int x0 = ST_X(-156), y0 = ST_Y(-110), x1 = x0 + ST_S(36) - 1, y1 = y0 + ST_S(28) - 1;
        u16 c = RGB15(20, 17, 10);
        overlay_line(x0, y0, x1, y0, c, 27);
        overlay_line(x0, y1, x1, y1, c, 27);
        overlay_line(x0, y0, x0, y1, c, 27);
        overlay_line(x1, y0, x1, y1, c, 27);
    }
    if (n > 0 && sel >= 0 && sel < n)
    {
        if (it[sel].kind == ROW_CASE && menu_loaded)
            menu_piece(203, 64, 37, 29, -154, -108, 32, 24);
        else
            overlay_sheet(&items, it[sel].ix, it[sel].iy, 32, 24, ST_X(-154), ST_Y(-108), ST_S(32), ST_S(24), RGB15(31, 31, 31), 31, 27);
    }
    for (int r = 0; r < ST_ROWS && top + r < n; r++)
    {
        int dy = 31 * r;
        overlay_rect(ST_X(-139), ST_Y(-59 + dy), ST_X(-139) + ST_S(32), ST_Y(-59 + dy) + ST_S(24), RGB15(1, 1, 1), 31, 26);
        if (it[top + r].kind == ROW_CASE && menu_loaded)
            menu_piece(203, 64, 37, 29, -139, -59 + dy, 32, 24);
        else
            overlay_sheet(&items, it[top + r].ix, it[top + r].iy, 32, 24, ST_X(-139), ST_Y(-59 + dy), ST_S(32), ST_S(24), RGB15(31, 31, 31), 31, 26);
        if (top + r == sel)
        {
            int x0 = ST_X(-140), y0 = ST_Y(-61 + dy), x1 = ST_X(152), y1 = y0 + ST_S(31) - 1;
            overlay_line(x0, y0, x1, y0, RGB15(27, 7, 7), 27);
            overlay_line(x0, y1, x1, y1, RGB15(27, 7, 7), 27);
            overlay_line(x0, y0, x0, y1, RGB15(27, 7, 7), 27);
            overlay_line(x1, y0, x1, y1, RGB15(27, 7, 7), 27);
        }
    }
    for (int k = 0; k < 2; k++)
        if (k == 0 ? top > 0 : top + ST_ROWS < n)
            for (int j = 0; j < 4; j++)
            {
                int y = k == 0 ? ST_Y(-77) + 2 + j : ST_Y(66) + 5 - j;
                overlay_line(ST_X(-51) + 4 - j, y, ST_X(-51) + 4 + j, y, RGB15(31, 27, 8), 28);
            }
    overlay_end();
}

// タイトルの絵、全画面。
static void draw_title(void)
{
    use_picture(PIC_TITLE);
    overlay_begin();
    overlay_sheet(&picture, 0, 0, picture.w, picture.h, 0, 0, 256, 192, RGB15(31, 31, 31), 31, 1);
    overlay_end();
}

// ステージセレクト、1度に1ステージ：上にその番号と場所とミッションの名前、赤い矢印の間に
// その絵（原作のもの、2d_stage_00：85 x 56 が 3 x 4、2倍の大きさで）、下にミッションの説明
// （原作のミッションの一覧、stgnfo_mission_ms.bin、英語で）。左 / 右で次へスクロール（絵が横に
// 滑る）。A で決めると（ズーム 0..ZOOM_FRAMES）絵が大きくなって画面を埋める。
#define STAGES       12
#define ZOOM_FRAMES  30
#define SCROLL_FRAMES 10
#define SEL_X  43       // 絵：170 x 112
#define SEL_Y  34
#define SEL_W  170
#define SEL_H  112

// 6面（ミッション6、小屋の籠城戦）はゲームから外している：ステージセレクトに無く、その後の
// ステージは6から続けて番号を振る。
#define STAGE_DROPPED 5
static int stage_no(int i) { return i < STAGE_DROPPED ? i + 1 : i; }
static const char *const stage_names[] = { T_SN_1, T_SN_2, T_SN_3, T_SN_4, T_SN_5, T_SN_6, T_SN_7, T_SN_8, T_SN_9, T_SN_10, T_SN_11, T_SN_12 };
static const char *const mission_names[] = {
    "Bloodthirst", "Luis Sera", "Las Plagas", "The Giant's Cage", "Ashley's Rescue", "The Siege",
    "To the Great Castle", "The Blind Weapon", "The Guardian Pair", "Reunion", "The Furnace", "The Strongest Assassin" };
static const char *const mission_lines[][3] = {
    { "The nightmare begins! Leon", "enters the village. Defeat", "the Ganados!" },
    { "Captured and taken to a", "safe place, Leon meets", "Luis for the first time." },
    { "Leon's body writhes in pain", "as the Plaga parasite", "spreads." },
    { "Leon falls into a trap in", "the mine as the terrible", "El Gigante approaches!" },
    { "Leon gets into the church.", "Where could Ashley be", "held...?" },
    { "The villagers start to", "surround the house Leon", "is in." },
    { "Leon flees to the old", "castle, but what awaits", "him may be worse." },
    { "As Leon looks for a way", "out, the iron guardian,", "Garrador, appears!" },
    { "Standing between Ashley", "and Leon is not one, but", "two Garradors." },
    { "Leon rescues Ashley. But", "she is captured again", "by the Ganados!" },
    { "A trap puts Leon up", "against a real nightmare:", "two El Gigantes!" },
    { "Waiting for Leon at the", "old canal is the deadly", "assassin, Verdugo!" } };

static void select_picture(int i, int x, int y, int w, int h, int alpha, int id)
{
    overlay_sheet(&picture, (i % 3) * 85, (i / 3) * 56, 85, 56, x, y, w, h, RGB15(31, 31, 31), alpha, id);
}

// from：スクロールして離れたステージ（-1：なし）。scroll：スライドの残りフレーム。dir：+1 で
// 新しいものが右から入る。
static void draw_select(int sel, int from, int scroll, int dir, int tick, int zoom)
{
    use_picture(PIC_STAGESEL);
    overlay_begin();
    overlay_rect(0, 0, 256, 192, RGB15(2, 2, 3), 31, 1);
    int x = SEL_X, y = SEL_Y, w = SEL_W, h = SEL_H;
    if (zoom >= 0)
    {
        // 全画面へ向けてイーズアウト。
        int t = inttof32(zoom) / ZOOM_FRAMES;
        t = inttof32(1) - mulf32(inttof32(1) - t, inttof32(1) - t);
        x += mulf32(0 - x, t);
        y += mulf32(0 - y, t);
        w += mulf32(256 - w, t);
        h += mulf32(192 - h, t);
        select_picture(sel, x, y, w, h, 31, 4);
    }
    else
    {
        int off = scroll > 0 ? scroll * 256 / SCROLL_FRAMES : 0;
        if (scroll > 0 && from >= 0)
            select_picture(from, x - dir * (256 - off), y, w, h, 31, 3);
        overlay_rect(x - 2, y - 2, x + w + 2, y + h + 2, RGB15(12, 10, 6), 31, 2);
        select_picture(sel, x + dir * off, y, w, h, 31, 4);
        // 矢印、少し外へ脈打つ。
        int p = 2 + sinLerp(tick * 700) * 2 / 4096, cy = y + h / 2;
        overlay_tri(8 - p, cy, 30 - p, cy - 18, 30 - p, cy + 18, RGB15(31, 0, 0), 31, 5);
        overlay_tri(248 + p, cy, 226 + p, cy - 18, 226 + p, cy + 18, RGB15(31, 0, 0), 31, 6);
    }
    overlay_end();
}
// 結果：「TIME:」と数字は原作のもの（2d_gm_00、シートの y 48 から：時計の絵は y 1、金のバーは
// y 169）。ほかはゲームのフォントの文字。
#define RES_TIME_W  46
#define RES_DIGIT_Y 1
#define RES_DIGIT_H 15
#define RES_BAR_Y   169
static int res_digit(int d, int x, int y)
{
    // （シートの中：0 は48、1 は60で幅4、それから2-9が64から幅8で横に並ぶ）
    int sx = d == 0 ? 48 : d == 1 ? 60 : 64 + (d - 2) * 8, w = d == 1 ? 4 : 8;
    overlay_sheet(&picture, sx, RES_DIGIT_Y, w, RES_DIGIT_H, x + (8 - w) / 2, y, w, RES_DIGIT_H, RGB15(31, 31, 31), 31, 5);
    return x + 9;
}

static void draw_result(int seconds, int step)
{
    use_picture(PIC_RESULT);
    overlay_begin();
    overlay_rect(0, 0, 256, 192, RGB15(3, 1, 1), 31, 1);
    if (step >= 1)
    {
        overlay_sheet(&picture, 1, RES_BAR_Y, 148, 9, 24, 50, 208, 9, RGB15(31, 31, 31), 31, 2);
        // TIME: mm:ss
        int x = 64, y = 66;
        overlay_sheet(&picture, 0, RES_DIGIT_Y, RES_TIME_W, RES_DIGIT_H, x, y, RES_TIME_W, RES_DIGIT_H, RGB15(31, 31, 31), 31, 3);
        x += RES_TIME_W + 6;
        int m = seconds / 60 > 99 ? 99 : seconds / 60, s = seconds % 60;
        x = res_digit(m / 10, x, y);
        x = res_digit(m % 10, x, y);
        overlay_sheet(&picture, 42, RES_DIGIT_Y, 4, RES_DIGIT_H, x, y, 4, RES_DIGIT_H, RGB15(31, 31, 31), 31, 5);   // 「:」
        x += 6;
        x = res_digit(s / 10, x, y);
        res_digit(s % 10, x, y);
    }
    overlay_end();
}

// 縦のメニュー、(x 中心, y 上端) に中央揃え。選んだ項目は金色。
static void show_menu(int first_line, int cx, int y, const char *const *items, int count, int selected)
{
    for (int i = 0; i < count; i++)
        text_line(first_line + i, cx - text_width(items[i]) / 2, y + i * 18, i == selected ? TEXT_GOLD : TEXT_WHITE, items[i]);
}

// 上 / 下で選択を動かす（端で折り返す）。新しい番号を返す。
static int menu_move(u32 down, int selected, int count)
{
    if (down & KEY_UP)
        selected = (selected + count - 1) % count;
    if (down & KEY_DOWN)
        selected = (selected + 1) % count;
    return selected;
}

// メニューの後ろの 3D の層（BG0）を暗くする。スプライト（文字）は明るいまま。0 = なし、16まで。
static void dim_3d(int level)
{
    REG_BLDCNT = level ? BLEND_FADE_BLACK | BLEND_SRC_BG0 : 0;
    REG_BLDY = level;
}

// レオンのモデル空間 -> ワールド（位置 + Y 軸まわりの回転、model_draw() がかけるとおり）。
static void leon_to_world(const int pos[3], int yaw, const int v[3], int out[3])
{
    int s = sinLerp(yaw), c = cosLerp(yaw);
    out[0] = pos[0] + mulf32(v[0], c) + mulf32(v[2], s);
    out[1] = pos[1] + v[1];
    out[2] = pos[2] - mulf32(v[0], s) + mulf32(v[2], c);
}

// `cur` を `want` へ残りの1/8だけ動かす（単純な指数平滑化）。
static void approach(int cur[3], const int want[3])
{
    for (int k = 0; k < 3; k++)
        cur[k] += (want[k] - cur[k]) >> 3;
}

#ifdef TEST_BOT
#include "bot.h"
#endif
static int st_jya_out;   // ステータス画面がその間、教徒のモデルをしまった（MODE_STATUS を参照）

// 本体がスリープから戻った後（ふたを閉じてまた開けた）：3D エンジンの設定、VRAM のバンク、
// ステージのもやと霧を起動時と同じく設定し直す。実機ではスリープの後テクスチャが全部白くなった。
// （フレームの間が0.5秒を超える空きも、スリープでしか起きないので同じ扱い。）
// ふたを閉じた：本体自身のスリープ（その後、実機では後から読んだテクスチャが全部白くなった）の
// 代わりに、ゲームはその場で止まり、両方のバックライトと電源ランプを消し、音楽も止める。ふたが
// また開くまで。（DSi の明るさのレベルではなくバックライトのビット。レベルは DSi が保つ。）
static void lid_rest(void)
{
    model_sync();
    int vol = music_asked();
    music_volume(0);
    fifoSendValue32(FIFO_PM, PM_REQ_OFF | PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
    fifoSendValue32(FIFO_PM, PM_REQ_LED | PM_LED_SLEEP);
    do
    {
        swiWaitForVBlank();
        scanKeys();
    } while (keysHeld() & KEY_LID);
    fifoSendValue32(FIFO_PM, PM_REQ_ON | PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
    fifoSendValue32(FIFO_PM, PM_REQ_LED | PM_LED_ON);
    music_volume(vol);
}
static void gfx_restore(int stage)
{
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankB(VRAM_B_TEXTURE);
    vramSetBankD(VRAM_D_TEXTURE);
    vramSetBankE(VRAM_E_TEX_PALETTE);
    vramSetBankF(VRAM_F_MAIN_SPRITE_0x06400000);
    vramSetBankG(VRAM_G_MAIN_SPRITE_0x06404000);
    vramSetBankC(VRAM_C_SUB_BG);
    REG_DISPCNT |= DISPLAY_BG0_ACTIVE | ENABLE_3D;   // （BG0 は 3D の層）
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ANTIALIAS);
    glEnable(GL_BLEND);
    glClearPolyID(63);
    glClearDepth(0x7FFF);
    glViewport(0, 0, 255, 191);
    if (stage >= 0)
    {
        set_haze(stage_info[stage].haze);
        stage_fog(stage_info[stage].fog);
    }
    else
    {
        glClearColor(HAZE_R, HAZE_G, HAZE_B, 31);
        setup_fog();
    }
}

int main(void)
{
    disableSleep();   // （ふた：代わりに lid_rest()）
    videoSetMode(MODE_0_3D);
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankB(VRAM_B_TEXTURE);
    vramSetBankD(VRAM_D_TEXTURE);
    vramSetBankE(VRAM_E_TEX_PALETTE);
    irqSet(IRQ_VBLANK, on_vblank);

    glInit();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ANTIALIAS);
    glEnable(GL_BLEND);   // 半透明のポリゴン（エフェクト、HUD）は不透明に描かずにブレンドする
    glClearColor(HAZE_R, HAZE_G, HAZE_B, 31);   // 空はもやの色
    setup_fog();
    glClearPolyID(63);
    glClearDepth(0x7FFF);
    glViewport(0, 0, 255, 191);

    int err_stage = nitroFSInit(NULL) ? load_stage(0) : -99;
    save_read();
    int err_leon = model_load(&leon, leon_bin, leon_bin_size);
    int err_anim = anim_load(&leon_anims, leon_anim_bin, leon_anim_bin_size);
    if (!err_anim && leon_anims.nodes != leon.node_count)
        err_anim = -10;
    int err_ground = 0;
    int err_gun = model_load(&gun, gun_bin, gun_bin_size);
    int err_aim = anim_load(&leon_gun, leon_gun_bin, leon_gun_bin_size);
    if (!err_aim && leon_gun.nodes != leon.node_count)
        err_aim = -10;
    if (!err_aim)
        err_aim = anim_load(&leon_knife, leon_knife_bin, leon_knife_bin_size) || leon_knife.nodes != leon.node_count
                  || model_load(&knife_model, knife_bin, knife_bin_size) ? -11 : 0;
    int err_gna = model_load(&ganado, ganado_bin, ganado_bin_size);
    if (!err_gna)
        err_gna = anim_load(&ganado_anims, ganado_anim_bin, ganado_anim_bin_size);
    if (!err_gna && ganado_anims.nodes != ganado.node_count)
        err_gna = -10;
    if (!err_gna)
        err_gna = anim_load(&ganado_w0, gna_w0_bin, gna_w0_bin_size) || anim_load(&ganado_w1, gna_w1_bin, gna_w1_bin_size)
                  || model_load(&sickle_model, sickle_bin, sickle_bin_size) || model_load(&axe_model, axe_bin, axe_bin_size)
                  || model_load(&fork_model, fork_bin, fork_bin_size) ? -20 : 0;
    if (!err_gna)
        err_gna = anim_load(&cha_anims, cha_anim_bin, cha_anim_bin_size) ? -30 : 0;
    int err_2d = sheet_load(&hud, hud_bin, hud_bin_size);
    if (!err_2d)
        err_2d = sheet_load(&items, items_bin, items_bin_size) * 2;
    if (!err_2d)
        err_2d = sheet_load(&star, star_bin, star_bin_size) * 3;
    if (!err_2d)
        err_2d = sheet_load(&beam, beam_bin, beam_bin_size) * 4;
    if (!err_2d)
        err_2d = sheet_load(&spark, spark_bin, spark_bin_size) * 5;
    if (!err_2d)
    {
        use_picture(PIC_TITLE);
        err_2d = picture_id == PIC_TITLE ? 0 : -6;
    }
    if (!err_2d)
        err_2d = text_init(font_bin, font_bin_size) * 100;
    if (!err_2d)
        err_2d = sfx_init(sfx_bin, sfx_bin_size) * 1000;
    if (!err_2d)
        err_2d = music_init(music_bin, music_bin_size) * 10000;
    if (!err_2d)   // 下画面：バイタル画面（VRAM C）
        err_2d = vital_init(vitalbmp_bin, vitalbmp_bin_size, iconbmp_bin, iconbmp_bin_size) * 100000;
    if (!err_2d)   // ステージの仕掛け（fix_all のモデル、32x32 のテクスチャ）
    {
        // 1面にあるのははしごと木箱だけ（門や扉はステージのモデルの中にあり、樽もメダルも無い）：
        // ほかの仕掛けのモデルは読まないままにして、テクスチャ VRAM をガナードの武器に残す。
        err_2d = model_load(&ladder_model, ladder_bin, ladder_bin_size) || model_load(&crate_model, crate_bin, crate_bin_size);
        err_2d *= 1000000;
    }
    if (err_stage || err_leon || err_anim || err_ground || err_gun || err_aim || err_gna || err_2d)
    {
        consoleDemoInit();   // 代わりに下画面に文字
        printf("load failed: stage %d leon %d anim %d\nground %d gun %d aim %d ganado %d\n2d %d\n",
               err_stage, err_leon, err_anim, err_ground, err_gun, err_aim, err_gna, err_2d);
        while (1)
            swiWaitForVBlank();
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspectivef32(degreesToAngle(CAM_FOV), floattof32(256.0 / 192.0), floattof32(NEAR_PLANE), floattof32(FAR_PLANE));

#ifdef LEON_CIRCLE_X
    const int circle_x = inttof32(LEON_CIRCLE_X), circle_z = inttof32(LEON_CIRCLE_Z);
#else
    const int circle_x = stage.ground[0], circle_z = stage.ground[2];
#endif
    const int circle_radius = inttof32(LEON_CIRCLE_R);

    // レオンは村の外の原作のスタート地点から始まる。テスト用ビルドは村の広場の歩きの円の上
    // （+Z 向き）か、LEON_START_X/Z から始まる。足元はバウンディングボックスの最小値。
    int leon_feet = stage.ground[1];
#if defined(LEON_START_X)   // ワールド単位
    int leon_pos[3] = { inttof32(LEON_START_X), 0, inttof32(LEON_START_Z) };
#elif defined(AUTOPILOT) && !defined(STAGE_START)
    int leon_pos[3] = { circle_x, 0, circle_z + circle_radius };
#else
    int leon_pos[3] = { floattof32(STAGE_START_X), 0, floattof32(STAGE_START_Z) };
#endif
    // そこの一番高い床から始める（開けた地面なら問題ない。屋根の下のスタートは屋根の上に乗る）。
    ground_height(&stage_ground, leon_pos[0], leon_pos[2], inttof32(10000), 0, &leon_feet);
    leon_pos[1] = leon_feet - leon.bbox_min[1];
#if defined(LEON_START_YAW)   // 度、0 = +Z 向き
    int leon_yaw = degreesToAngle(LEON_START_YAW);
#elif defined(AUTOPILOT) && !defined(STAGE_START)
    int leon_yaw = 0;
#else
    int leon_yaw = degreesToAngle(STAGE_START_YAW);
#endif
    objects_init(&objs, &stage_ground, &door_model, &ladder_model, &barrel_model, &crate_model, &medal_model, &boom_sheet, &spark);
    objs.bell = &bell_model;
    enemies_init(&ganados, &ganado, &ganado_anims, &stage_ground, &objs, ganado_spawn, GANADO_COUNT, DEGREES_IN_CIRCLE / 2);
    stage1_kinds();
#ifdef TEST_SIGHT
    ganados.sight = inttof32(TEST_SIGHT);
#endif
    {
        const Model *const weapons[WEAPON_KINDS] = { NULL, &sickle_model, &axe_model, &fork_model };
        enemies_arm(&ganados, weapons, &ganado_w0, &ganado_w1);
    }
    const int start_pos[3] = { leon_pos[0], leon_pos[1], leon_pos[2] };
    const int start_feet = leon_feet, start_yaw = leon_yaw;
    int leon_angle = 0, anim_frame = 0;
    int leon_anim = LEON_START_MODE;   // フリーカメラ：START で順に切り替えるもの
    int ctrl = START_CTRL;
    int aim_w = 0;       // f32 0..1：構えのポーズをどれだけブレンドしたか
    int aim_tilt = 0;    // f32 -1（下）.. +1（上）
    int gun_state = GUN_HOLSTERED;
    int draw_frame = 0;  // 抜き / しまいの間の GUN_DRAW での f32 の位置
    int clip_key = -1;   // 再生中のアニメーションを識別し、変わったときにクロスフェードする
    int xfade_w = 0;     // f32 1..0：まだ混ぜている pose_from の重み
    int ammo = MAG_SIZE;
    int wep_note = 0;        // 持ち替えたばかりの武器の名前を出す残りフレーム
    int gclimb = 0, gclimb_f[2] = { 0, 0 }, gclimb_y = 0, gcut_page = 0;   // レオンが登る巨人：どれか、その向き、床
    int rocket_on = 0, rocket_pos[3] = { 0 }, rocket_to[3] = { 0 }, rocket_boom = 0;
    int scoping = 0, scope_was = 0, scope_dir[3] = { 0, 0, inttof32(1) };   // ライフルのスコープ：このフレームで覗いている、前のフレーム
    int leon_fade = 31;      // 彼のアルファ：壁でカメラが彼に押し付けられると透ける
    int shop_screen = SHOP_ST_TABS, shop_tab = 1, shop_sel = 0, shop_top = 0, shop_wep = -1, shop_msg = 0;
    const char *shop_text = "";
    int fire_frame = -1;     // 射撃アニメーションでの f32 の位置、撃っていないときは -1
    int reload_frame = -1;   // GUN_RELOAD での f32 の位置、リロードしていないときは -1
    int reload_ch = -1;      // リロードの音のチャンネル、リロードをやめたら切る
    int flash_timer = 0, impact_timer = 0, cam_kick = 0;
    int impact_pos[3] = { 0 }, muzzle[3] = { 0 }, laser_end[3] = { 0 }, laser_hit = 0;
    int laser_enemy = -1, laser_head = 0, laser_part = PART_BODY, impact_blood = 0, laser_obj = -1, laser_ash = 0, laser_giant = 0, laser_gal = 0, laser_gal_back = 0, laser_ver = 0, laser_drop = -1;
    int ash_lost = 0;               // アシュリーが倒れてからのフレーム数（しばらくしてゲームオーバー）
    u32 hits = 0, headshots = 0;
    u32 shots = 0;
    int leon_hp = LEON_HP;
    int react = REACT_NONE;   // 操作とアニメーションに優先する、攻撃への反応
    int react_frame = 0;      // 反応のクリップでの f32 の位置
    int holder = -1;          // 彼をつかんでいるガナード（REACT_HELD）
    int mash = 0, held_frames = 0, safe_timer = 0, hurt_flash = 0;
    int slash_frame = -1;           // ナイフの斬りでの f32 の位置、-1 = 斬っていない
    int slash_high = 0;             // 今の斬りは高い方
    int knife_w = 0;                // f32 0..1：ナイフのために視点がどれだけ寄ったか
    int act_obj = -1;               // 実行中の動作の門 / 木箱 / はしご
    int drop_to = 0, drop_vy = 0;   // 段差から落ちる：下の床、落ちる速さ（f32）
    int next_ganado = 0;            // 次にステージに入るガナード
#ifndef START_STAGE                // テスト用ビルド：-DSTART_STAGE=n（1..12）と -DSKIP_TITLE
#define START_STAGE 1
#endif
    int cur_stage = START_STAGE - 1;  // プレイ中のステージ（0..STAGE_COUNT-1）
    int obj_t = 0;                 // 目的を画面に出しておくフレーム数
#ifdef TEST_SHOP_PTAS
    int ptas = TEST_SHOP_PTAS;
#elif defined(TEST_SHOP)
    int ptas = 60000;
#else
    int ptas = 0;                   // 稼いだお金（PTAS）
#endif
    int treasures_found = 0;        // このミッションで取った宝
    int boss_phase = 0, boss_t = 0; // ミッション1：0 村人、1 チェーンソー男、2 終わり
    int medal_note = 0;             // メダルの数の表示の残りフレーム
    int elevator_note = 0;          // 12面：エレベーター到着の表示の残りフレーム
    ObjEvents oev;
    EnemyEvents ev;

    // フリーカメラ：位置 + ヨー/ピッチ。追従カメラ：なめらかにした視点と注視点。
    int eye[3] = { leon_pos[0], leon_feet + inttof32(14), leon_pos[2] + inttof32(45) };
    int yaw = 0, pitch = -degreesToAngle(8);
    // 追従の位置から始める（その後は approach() がそこに保つ）。
    int follow_eye[3] = { leon_pos[0] - mulf32(sinLerp(leon_yaw), CAM_BACK) - mulf32(cosLerp(leon_yaw), CAM_RIGHT),
                          leon_feet + CAM_HEIGHT,
                          leon_pos[2] - mulf32(cosLerp(leon_yaw), CAM_BACK) + mulf32(sinLerp(leon_yaw), CAM_RIGHT) };
    int follow_target[3] = { leon_pos[0] + mulf32(sinLerp(leon_yaw), CAM_AHEAD) - mulf32(cosLerp(leon_yaw), CAM_AT_RIGHT),
                             leon_feet + CAM_AT_HEIGHT,
                             leon_pos[2] + mulf32(cosLerp(leon_yaw), CAM_AHEAD) + mulf32(sinLerp(leon_yaw), CAM_AT_RIGHT) };
#ifdef TEST_CULL
    int cull = TEST_CULL;
#else
    int cull = 0;
#endif
    touchPosition last_touch = { 0 };

    u32 frames = 0, fps = 0, fps_mark = 0;
#ifdef AUTOPILOT
    u32 frame_no = 0;
#endif

#if (defined(AUTOPILOT) && AUTOPILOT != 10 && AUTOPILOT != 35 && AUTOPILOT != 41 && AUTOPILOT != 48 && AUTOPILOT != 51) || defined(SKIP_TITLE)   // テスト用ビルドはすぐゲームに入る
    int mode = MODE_PLAY;
#else
    int mode = MODE_TITLE;
#endif
#ifdef TEST_SHOP   // テスト用ビルド：お金を持って店へ
    mode = MODE_SHOP;
#endif
    int menu_sel = 0, dead_timer = 0;
    int reset = mode == MODE_PLAY;   // メニューが設定：ゲームをやり直す（レオン、ガナード、カメラ）
    int pending = -1, fade = 0;   // 黒へのフェードを待っている画面の切り替え、そのレベル 0..16
    int stage_sel = 0, zoom = -1, shake = 0, tick = 0;   // ステージセレクト
    int scroll = 0, scroll_from = -1, scroll_dir = 1;   // …ステージから次へのスライド
    int spare = SPARE_START;      // マガジンとは別に持っている弾
    int herbs = 0;                // 持っているグリーンハーブ（アイテム画面から使う）
    int popup = 0, popup_type = 0, popup_value = 0;
    int qturn = 0;                // クイックターンの残りフレーム
    int cam_yaw = leon_yaw;       // 追従カメラの向き（レオンの後から回る）
    const CutScene *msg = NULL;   // MODE_MESSAGE：イベントのせりふ、表示中のページ、出してからのフレーム数
    int msg_page = 0, msg_t = 0;
    int take_item = 0;              // アイテムのそばで A が押された：拾う（drops_update()）
    int shot_noise = 0;             // このフレームで撃った（ガナードに聞こえるかもしれない）
#ifdef TEST_CUTIN   // テスト用ビルド：ステージの始まりの場面を再生する
    int cut_opening = 1;
#else
    int cut_opening = 0;   // ミッションの始まりの場面を再生する（ステージセレクトから入った）
#endif
    int kneel_seen = 0, gal_seen = 0, ash_waiting = 0, giant_paid = 0;   // このミッションで見たイベント
    int st_sel = 0, st_top = 0, st_cmd = -1, st_msg = 0;   // ステータス画面：選んだ行、表示中の最初の行、コマンド（-1：開いていない）、表示のフレーム数
    int clear_t = -1;             // ステージをクリアしてからのフレーム数、-1 = まだ
    u32 play_frames = 0;          // ステージでの時間（ポーズは数えない）
    int damage_taken = 0, result_t = 0;
    drops_init(&drops, &star, &beam, &spark, &items);
    static const char *const title_items[] = { T_CONTINUE, T_NEW_GAME, T_HOWTO };
    static const char *const over_items[] = { T_CONTINUE, T_TO_TITLE };
    static const char *const howto[] = { T_HOWTO_KEYS };
    int menu_shown = -1;   // どの画面の文字を出しているか
    while (1)
    {
        scanKeys();
        u32 down = keysDown(), held = keysHeld();
        // ふたを閉じた：本体のスリープの代わりにゲームが休む（lid_rest()）。
        if (held & KEY_LID)
        {
            lid_rest();
            gfx_restore(stage_loaded >= 0 ? stage_loaded : -1);
            scanKeys();
            down = 0;
            held = keysHeld();
        }
#ifdef TEST_BOT   // テスト用ビルド：ボットがボタンを押す（bot.h）
        {
            static u32 bot_prev;
#include "bot_frame.inc"
            down = held & ~bot_prev;
            bot_prev = held;
        }
#endif
#ifdef AUTOPILOT
        if (ctrl == CTRL_LEON)
        {
            static u32 prev_held;
            held = autopilot_keys(frame_no);
            down = held & ~prev_held;
            prev_held = held;
        }
        frame_no++;
#if AUTOPILOT == 12
        // バイタル画面のタッチ：弾の枠、ハーブの枠（選ぶ）、もう一度ゆっくりハーブの枠（何も起きない）、
        // それからハーブをダブルタップ（使う、使う：もう満タンで拒否）と弾の枠をダブルタップ（何も起きない）。
        static const struct { u16 at; u8 x, y; } taps[] = {
            { 300, 20, 68 }, { 400, 20, 110 }, { 500, 20, 110 },
            { 600, 20, 110 }, { 610, 20, 110 }, { 700, 20, 110 }, { 710, 20, 110 },
            { 800, 20, 110 }, { 810, 20, 110 }, { 900, 20, 68 }, { 910, 20, 68 } };   // （左に並ぶショートカット：弾68、ハーブ110）
#ifndef TAPS_FROM   // （テスト用ビルド：-DTAPS_FROM=n で n フレーム遅らせる。ミッションの始まりの後へ）
#define TAPS_FROM 0
#endif
        for (unsigned i = 0; i < sizeof(taps) / sizeof(taps[0]); i++)
            if (frame_no == taps[i].at + TAPS_FROM)
                vital_fake_touch(taps[i].x, taps[i].y);
#endif
#endif

        // フェードする画面の切り替え：黒へ（その間は入力なし）、切り替え、戻る。
        if (pending >= 0)
        {
            down = held = 0;
            fade += 2;
            if (fade >= 16)
            {
                fade = 16;
                mode = pending;
                pending = -1;
                menu_shown = -1;
            }
        }
        else if (fade > 0)
            fade -= 2;
        setBrightness(1, -fade);
        tick++;

        // タイトルと操作説明の画面：タイトルの絵とメニュー。ゲームは動かない。
        if (mode == MODE_TITLE || mode == MODE_HOWTO)
        {
            if (menu_shown != mode)
            {
                text_clear();
                menu_sel = menu_shown == MODE_HOWTO ? 1 + have_save : 0;   // 操作説明から戻る：その項目のまま
                menu_shown = mode;
            }
            music_play(MUSIC_TITLE, MUSIC_VOLUME);
            if (mode == MODE_TITLE)
            {
                dim_3d(0);
                menu_sel = menu_move(down, menu_sel, mode == MODE_TITLE ? 2 + have_save : 2);
                show_menu(0, 128, have_save ? 130 : 138, title_items + !have_save, 2 + have_save, menu_sel);
                if (down & (KEY_A | KEY_START))
                {
                    int item = menu_sel + !have_save;   // 0 Continue、1 New Game、2 Controls
                    sfx_play(SFX_MENU, 100, 64);
                    if (item == 0)
                        save_apply(&ammo, &spare, &herbs, &ptas);
                    else if (item == 1)
                        new_game(&ammo, &spare, &herbs, &ptas);
                    mode = item < 2 ? MODE_SELECT : MODE_HOWTO;
                }
            }
            else
            {
                dim_3d(12);
                text_line(0, 128 - text_width(T_HOWTO) / 2, 10, TEXT_GOLD, T_HOWTO);
                for (int i = 0; i < 8; i++)
                {
                    text_line(TEXT_WIDE_LINES + i, 10, 34 + i * 18, TEXT_GOLD, howto[i * 2]);
                    text_line(2 + i, 72, 34 + i * 18, TEXT_WHITE, howto[i * 2 + 1]);
                }
                text_line(1, 246 - text_width(T_BACK), 176, TEXT_GREY, T_BACK);
                if (down & (KEY_B | KEY_A | KEY_START))
                    mode = MODE_TITLE;
            }
            if (mode == MODE_TITLE || mode == MODE_HOWTO)
            {
                draw_title();
                model_sync();   // （最後の文字の一覧を終えた）
                glFlush(0);
            }
            else
            {
                text_clear();   // ステージセレクトへ
                dim_3d(0);
                menu_shown = -1;
            }
            swiWaitForVBlank();
            text_commit();
            vital_idle(T_CAPTION, T_EDITION);
            continue;
        }

        // ステージセレクト（draw_select() を参照）：十字キーで選び、A で決定（絵がズームして画面を埋め、
        // ゲームが始まる）、B でタイトルに戻る。
        if (mode == MODE_SELECT)
        {
            if (menu_shown != MODE_SELECT)
            {
                text_clear();
                dim_3d(0);
                zoom = -1;
                menu_shown = MODE_SELECT;
            }
            music_play(MUSIC_TITLE, MUSIC_VOLUME);
            if (scroll > 0)
                scroll--;
            if (zoom < 0)
            {
                int step = (down & KEY_RIGHT) ? 1 : (down & KEY_LEFT) ? -1 : 0;
                if (step && scroll == 0)
                {
                    scroll_from = stage_sel;
                    do
                        stage_sel = (stage_sel + step + STAGES) % STAGES;
                    while (stage_sel == STAGE_DROPPED);   // （6面は外している）
                    scroll = SCROLL_FRAMES;
                    scroll_dir = step;
                    sfx_play(SFX_MENU, 60, 64);
                }
                if ((down & (KEY_A | KEY_START)) && scroll == 0)
                {
                    sfx_play(SFX_MENU, 100, 64);   // 全ステージが開いている
                    zoom = 0;
                }
                if (down & KEY_B)
                    mode = MODE_TITLE;
            }
            else if (zoom < ZOOM_FRAMES && ++zoom == ZOOM_FRAMES)
            {
                pending = MODE_SHOP;
                cur_stage = stage_sel;
            }
            if (shake > 0)
                shake--;
            // 番号と場所、ミッションの名前、絵の下にその説明。
            char name[32];
            snprintf(name, sizeof(name), T_STAGE_N, stage_no(stage_sel), stage_names[stage_sel]);
            text_hide(0);
            text_line(1, 128 - text_width(name) / 2, 3, TEXT_GOLD, name);
            text_line(2, 128 - text_width(mission_names[stage_sel]) / 2, 17, TEXT_WHITE, mission_names[stage_sel]);
            for (int i = 0; i < 3; i++)
                text_line(3 + i, 128 - text_width(mission_lines[stage_sel][i]) / 2, 150 + i * 13, TEXT_WHITE, mission_lines[stage_sel][i]);
            if (zoom >= 0)
                for (int i = 0; i < 6; i++)
                    text_hide(i);
            if (mode == MODE_SELECT)
            {
                draw_select(stage_sel, scroll_from, scroll, scroll_dir, tick, zoom);
                model_sync();   // （最後の文字の一覧を終えた）
                glFlush(0);
            }
            else
                menu_shown = -1;
            swiWaitForVBlank();
            text_commit();
            vital_idle(T_CAPTION, T_EDITION);
            continue;
        }

        // 店、ステージセレクトとミッションの間（原作の Iniciar、Comprar、Vender、Upgrade のタブと商人の
        // せりふ）。値段と改造は原作のもの。弾の箱は1回押すだけ（原作ではいくつか聞かれる）。売値は半分。
        if (mode == MODE_SHOP)
        {
            static const char *const tabs[] = { T_SHOP_START, T_SHOP_BUY, T_SHOP_SELL, T_SHOP_UPGRADE };
            static const s8 tab_x[] = { -116, -55, 10, 69 };          // （Bio4_StringLayout.bin 41-44）
            static ShopRow rows[SHOP_ROWS];
            if (menu_shown != MODE_SHOP)
            {
                text_clear();
                dim_3d(0);
                menu_shown = MODE_SHOP;
                shop_screen = SHOP_ST_TABS;
                shop_tab = 1;
                shop_wep = -1;
                shop_msg = 0;
                sheet_free(&picture);   // （ステージセレクトのもの：その空きはメニューのシートに回す）
                picture_id = -1;
                if (!menu_loaded)
                    menu_loaded = sheet_load(&menu_sheet, menu_bin, menu_bin_size) == 0;
                glClearColor(0, 0, 0, 31);
            }
            music_play(MUSIC_TITLE, MUSIC_VOLUME);
            int n = 0;
            ShopRow *r = rows;
            #define ROW(k, a, p, it) (r = &rows[n++], *r = (ShopRow){ .kind = k, .arg = a, .price = p }, \
                                      (it) < 0 ? drops_icon(DROP_HERB, 0, &r->ix, &r->iy) : item_icon(it, &r->ix, &r->iy))
            if (shop_tab == 1)
            {
                for (int w = 0; w < WEP_COUNT; w++)
                {
                    ROW(ROW_WEAPON, w, BUY_PRICE(weapons[w].price), weapons[w].item);
                    snprintf(r->name, sizeof(r->name), "%s", weapons[w].name);
                    if (wep_owned >> w & 1)
                        snprintf(r->note, sizeof(r->note), T_SHOP_OWNED);
                    else
                        snprintf(r->note, sizeof(r->note), T_SHOP_PRICE, r->price);
                }
                // 弾：1種類、多機能弾丸（どの銃にも共通の1つの数）：MULTI_BOX 発の箱をハンドガンの値段で
                // （50発で2500）。
                ROW(ROW_AMMO, WEP_HANDGUN, BUY_PRICE(MULTI_BOX * weapons[WEP_HANDGUN].ammo_price), AMMO_ITEM(WEP_HANDGUN));
                snprintf(r->name, sizeof(r->name), T_ITEM_AMMO);
                snprintf(r->note, sizeof(r->note), T_SHOP_BOX, MULTI_BOX, r->price);
                ROW(ROW_HERB, 0, BUY_PRICE(1000), -1);
                snprintf(r->name, sizeof(r->name), T_ITEM_HERB);
                snprintf(r->note, sizeof(r->note), T_SHOP_PRICE, r->price);
                if (case_level < 3)
                {
                    ROW(ROW_CASE, 0, BUY_PRICE(case_price[case_level + 1]), 0);
                    snprintf(r->name, sizeof(r->name), T_SHOP_CASE_NAME);
                    snprintf(r->note, sizeof(r->note), T_SHOP_CASE, case_tam[case_level + 1], r->price);
                }
                if (armor_level < 3)
                {
                    static const char *const vest[] = { T_VEST_1, T_VEST_2, T_VEST_3 };
                    ROW(ROW_VEST, 0, BUY_PRICE(armor_price[armor_level + 1]), 71);
                    snprintf(r->name, sizeof(r->name), "%s", vest[armor_level]);
                    snprintf(r->note, sizeof(r->note), T_SHOP_VEST, armor_cut[armor_level + 1], r->price);
                }
            }
            else if (shop_tab == 2)
            {
                for (int w = 0; w < WEP_COUNT; w++)
                    if ((wep_owned >> w & 1) && w != wep)
                    {
                        ROW(ROW_SELL_WEAPON, w, weapons[w].price / 2, weapons[w].item);
                        snprintf(r->name, sizeof(r->name), "%s", weapons[w].name);
                        snprintf(r->note, sizeof(r->note), T_SHOP_PRICE, r->price);
                    }
                if (herbs > 0)
                {
                    ROW(ROW_SELL_HERB, 0, 500, -1);
                    snprintf(r->name, sizeof(r->name), T_ITEM_HERB);
                    snprintf(r->note, sizeof(r->note), T_SHOP_PRICE, r->price);
                }
            }
            else if (shop_tab == 3 && shop_wep < 0)
            {
                for (int w = 0; w < WEP_COUNT; w++)
                    if (wep_owned >> w & 1)
                    {
                        int done = 0;
                        for (int i = 0; i < WEP_STATS; i++)
                            done += wep_level[w][i];
                        ROW(ROW_UP_WEAPON, w, 0, weapons[w].item);
                        snprintf(r->name, sizeof(r->name), "%s", weapons[w].name);
                        snprintf(r->note, sizeof(r->note), T_SHOP_LEVELS, done);
                    }
            }
            else if (shop_tab == 3)
            {
                for (int i = 0; i < WEP_STATS; i++)
                {
                    if (!tune_at(shop_wep, i, 1))
                        continue;   // （この銃には無い）
                    int base = i == STAT_POWER ? weapons[shop_wep].power : i == STAT_CAPACITY ? weapons[shop_wep].mag * 10
                             : i == STAT_RATE ? 5 : tune_at(shop_wep, i, 1)->value * 5 / 4;
                    int now = wep_stat(shop_wep, i, base);
                    const ShopTune *next = tune_at(shop_wep, i, wep_level[shop_wep][i] + 1);
                    ROW(ROW_UP_STAT, i, next ? next->price : 0, weapons[shop_wep].item);
                    snprintf(r->name, sizeof(r->name), "%s", wep_stat_name[i]);
                    char a[8], b[8];
                    stat_text(a, now);
                    if (next)
                    {
                        stat_text(b, next->value);
                        snprintf(r->note, sizeof(r->note), T_SHOP_NEXT, a, b, r->price);
                    }
                    else
                        snprintf(r->note, sizeof(r->note), T_SHOP_TOP, a);
                }
            }
            #undef ROW
            if (shop_sel >= n)
                shop_sel = n > 0 ? n - 1 : 0;
            if (pending < 0 && shop_screen == SHOP_ST_TABS)
            {
                int was = shop_tab;
                if (down & KEY_LEFT)  shop_tab = (shop_tab + 3) % 4;
                if (down & KEY_RIGHT) shop_tab = (shop_tab + 1) % 4;
                if (shop_tab != was)
                    sfx_play(SFX_MENU, 60, 64);
                if (down & KEY_B)
                {
                    pending = MODE_SELECT;
                    save_write(spare, herbs, ptas);
                    sfx_play(SFX_MENU, 100, 64);
                }
                else if (down & KEY_A)
                {
                    sfx_play(SFX_MENU, 100, 64);
                    if (shop_tab == 0)
                    {
                        pending = MODE_PLAY;
                        reset = 1;
                        cut_opening = 1;
                        save_write(spare, herbs, ptas);
                    }
                    else
                    {
                        shop_screen = SHOP_ST_LIST;
                        shop_sel = shop_top = 0;
                        shop_wep = -1;
                        text_clear();
                    }
                }
            }
            else if (pending < 0)
            {
                if (down & KEY_B)
                {
                    sfx_play(SFX_MENU, 100, 64);
                    if (shop_tab == 3 && shop_wep >= 0)
                    {
                        shop_wep = -1;
                        shop_sel = 0;
                    }
                    else
                        shop_screen = SHOP_ST_TABS;
                    shop_msg = 0;
                    text_clear();
                }
                else if (n > 0)
                {
                    int was = shop_sel;
                    shop_sel = menu_move(down, shop_sel, n);
                    if (shop_sel != was)
                        sfx_play(SFX_MENU, 60, 64);
                    if (down & KEY_A)
                    {
                        const ShopRow *c = &rows[shop_sel];
                        int w = c->arg, got = 1;
                        const char *why = NULL;
                        int room = case_tam[case_level] - case_used(spare, herbs);
                        if (c->kind == ROW_WEAPON)
                        {
                            if (wep_owned >> w & 1)
                                got = 0;
                            else if (ptas < c->price)
                                why = T_SHOP_MONEY;
                            else if (room < weapons[w].tam)
                                why = T_SHOP_SPACE;
                            else
                            {
                                wep_owned |= 1 << w;
                                wep_ammo[w] = wep_mag(w);
                            }
                        }
                        else if (c->kind == ROW_AMMO)
                        {
                            int need = spare % MULTI_BOX ? 0 : 2;   // （一部だけ入った箱はそれ以上場所を取らない）
                            if (ptas < c->price)
                                why = T_SHOP_MONEY;
                            else if (room < need)
                                why = T_SHOP_SPACE;
                            else
                                spare += MULTI_BOX;
                        }
                        else if (c->kind == ROW_HERB)
                        {
                            if (ptas < c->price)
                                why = T_SHOP_MONEY;
                            else if (room < 2)
                                why = T_SHOP_SPACE;
                            else
                                herbs++;
                        }
                        else if (c->kind == ROW_CASE)
                        {
                            if (ptas < c->price)
                                why = T_SHOP_MONEY;
                            else
                                case_level++;
                        }
                        else if (c->kind == ROW_VEST)
                        {
                            if (ptas < c->price)
                                why = T_SHOP_MONEY;
                            else
                                armor_level++;   // （着ている：ケースの場所を取らない）
                        }
                        else if (c->kind == ROW_SELL_WEAPON)
                        {
                            ptas += c->price;
                            wep_owned &= ~(1 << w);
                            memset(wep_level[w], 0, sizeof(wep_level[w]));
                            got = -1;
                        }
                        else if (c->kind == ROW_SELL_HERB)
                        {
                            ptas += c->price;
                            herbs--;
                            got = -1;
                        }
                        else if (c->kind == ROW_UP_WEAPON)
                        {
                            shop_wep = w;
                            shop_sel = shop_top = 0;
                            text_clear();
                            got = 0;
                            sfx_play(SFX_MENU, 100, 64);
                        }
                        else if (c->kind == ROW_UP_STAT)
                        {
                            const ShopTune *next = tune_at(shop_wep, w, wep_level[shop_wep][w] + 1);
                            if (!next)
                                why = T_SHOP_MAX;
                            else if (ptas < next->price)
                                why = T_SHOP_MONEY;
                            else
                            {
                                wep_level[shop_wep][w]++;
                                if (w == STAT_CAPACITY && shop_wep == wep)
                                    ammo = MAG_SIZE;
                                if (w == STAT_CAPACITY)
                                    wep_ammo[shop_wep] = wep_mag(shop_wep);
                            }
                        }
                        if (why)
                        {
                            shop_text = why;
                            shop_msg = 120;
                            sfx_play(SFX_EMPTY, 110, 64);
                        }
                        else if (got)
                        {
                            if (got > 0)
                                ptas -= c->price;
                            sfx_play(SFX_ITEM_GET, 110, 64);
                        }
                    }
                }
            }
            if (shop_sel < shop_top)
                shop_top = shop_sel;
            if (shop_sel >= shop_top + ST_ROWS)
                shop_top = shop_sel - ST_ROWS + 1;

            // 原作のとおりの下のバー：左のケースの横に使っている場所 / その大きさ、右に所持金。
            char m[28];
            snprintf(m, sizeof(m), "%d / %d", case_used(spare, herbs), case_tam[case_level]);
            text_line(14, ST_X(-108), ST_Y(89), TEXT_WHITE, m);
            snprintf(m, sizeof(m), T_SHOP_PRICE, ptas);
            text_line(13, ST_X(150) - text_width(m), ST_Y(89), TEXT_WHITE, m);
            if (shop_screen == SHOP_ST_TABS)
            {
                text_line(0, ST_X(150) - text_width(T_MISSION_SHOP), ST_Y(-103), TEXT_WHITE, T_MISSION_SHOP);
                text_line(1, ST_X(-100), ST_Y(-50), TEXT_WHITE, last_rank >= 3 ? T_SHOP_DISCOUNT_1 : T_SHOP_LINE_1);   // （SLYT_Shop::Set_MerchantStartText：前のランクで）
                text_line(2, ST_X(-100), ST_Y(-34), TEXT_WHITE, last_rank >= 3 ? T_SHOP_DISCOUNT_2 : T_SHOP_LINE_2);
                text_line(3, ST_X(-100), ST_Y(-18), TEXT_WHITE, T_SHOP_LINE_3);
                for (int i = 0; i < 4; i++)
                    text_line(15 + i, ST_X(tab_x[i]), ST_Y(49), i == shop_tab ? TEXT_GOLD : TEXT_GREY, tabs[i]);
                draw_shop(rows, 0, 0, 0);
            }
            else
            {
                // 上のバー：この一覧の商人のせりふ、アイコンの枠の右に（断るときはその場所に少しの間ピンクで）。
                const char *ask = shop_tab == 1 ? T_SHOP_ASK_BUY : shop_tab == 2 ? T_SHOP_ASK_SELL : T_SHOP_ASK_UP;
                if (shop_msg > 0)
                {
                    shop_msg--;
                    ask = shop_text;
                }
                text_line(0, ST_X(150) - text_width(ask), ST_Y(-103), shop_msg > 0 ? TEXT_PINK : TEXT_WHITE, ask);
                for (int i = 0; i < ST_ROWS; i++)
                {
                    int k = shop_top + i;
                    if (k >= n)
                    {
                        text_hide(1 + i * 2);
                        text_hide(2 + i * 2);
                        continue;
                    }
                    text_line(1 + i * 2, ST_X(-106), ST_Y(-54 + 31 * i), TEXT_WHITE, rows[k].name);
                    text_line(2 + i * 2, ST_X(142) - text_width(rows[k].note), ST_Y(-54 + 31 * i), rows[k].kind == ROW_WEAPON && (wep_owned >> rows[k].arg & 1) ? TEXT_GREY : TEXT_GOLD, rows[k].note);
                }
                if (n == 0)
                    text_line(1, ST_X(-106), ST_Y(-54), TEXT_GREY, T_SHOP_EMPTY);
                draw_shop(rows, n, shop_top, shop_sel);
            }
            model_sync();
            glFlush(0);
            if (pending >= 0 && fade + 2 >= 16)   // （画面は黒：次のフレームで消える）
            {
                text_clear();
                if (menu_loaded)
                    sheet_free(&menu_sheet);
                menu_loaded = 0;
            }
            swiWaitForVBlank();
            text_commit();
            vital_idle(T_CAPTION, T_EDITION);
            continue;
        }

        // 結果（英語、draw_result() を参照）：行が1つずつ出て、それから A でステージセレクトへ進む。
        if (mode == MODE_RESULT)
        {
            static char stats[5][24], rank[3];
            static const char *const labels[] = { T_ACCURACY, T_LIFE, T_KILLS, T_HEADSHOTS, T_TREASURE };
            int seconds = (int)(play_frames / 60);
#ifdef TEST_RESULT_SECONDS   // テスト用ビルド：結果にこの時間を出す
            seconds = TEST_RESULT_SECONDS;
#endif
            if (menu_shown != MODE_RESULT)
            {
                text_clear();
                dim_3d(0);
                music_stop();
                result_t = 0;
                menu_shown = MODE_RESULT;
                // 原作の MISSÃO COMPLETA と同じく：命中率、残りの体力、攻撃（ここでは倒した数）、
                // 総合ランク、ミッションの PTAS。
                int kills = ganados.kills;
                int acc = shots ? (int)(hits * 100 / shots) : 0;
                snprintf(stats[0], sizeof(stats[0]), "%d%%", acc);
                snprintf(stats[1], sizeof(stats[1]), "%d", leon_hp);
                snprintf(stats[2], sizeof(stats[2]), "%d", kills);
                snprintf(stats[3], sizeof(stats[3]), "%lu", headshots);
                snprintf(stats[4], sizeof(stats[4]), "%d/%d", treasures_found, stage_info[cur_stage].treasures);
                // 原作のランク（Bio4_GMParam::GetMSRank、3つの点数、それぞれ100）：命中率（当たり x 100 / 撃った数）、
                // 体力（100 - 受けたダメージ / 100。レオンの体力は1200：移植版の100 x 12）、攻撃（倒した敵と
                // ミッションの全部の敵の比、重みはガナードか教徒が1、チェーンソー男が10、ガラドールが50、
                // エルヒガンテかヴェルデューゴが100。GetMSAttackScore）。150、180、210、270を超えると：ランク1-4
                // （文字は移植版のもの）。次の店はランク3で10%、4で20%引き（Bio4_ItemNode::GetBuyPrice）。
                (void)seconds;
                int life = 100 - (damage_taken * 12 + (ash.on ? (ASHLEY_HP - ash.hp) * 6 : 0)) / 100;   // （アシュリーの600：移植版の100 x 6）
                if (life < 0)
                    life = 0;
                int all = 0, felled = kills;
                if (cur_stage == 0)
                {
                    all = GANADO_COUNT + 10;
                    for (int i = 0; i < ganados.count; i++)
                        if (ganados.e[i].boss && ganados.e[i].state >= ENEMY_DYING)
                            felled += 10 - 1;
                }
                else
                {
                    const StageFoes *sf = &stage_foes[cur_stage];
                    for (int i = 0; i < sf->count; i++)
                    {
                        int w = sf->foe[i].type == FOE_CHAINSAW ? 10 : sf->foe[i].type == FOE_GARRADOR ? 50
                              : sf->foe[i].type == FOE_GIANT || sf->foe[i].type == FOE_VERDUGO ? 100 : 1;
                        all += w * sf->foe[i].entries;
                        if (w == 50 && garradors_down())
                            felled += 50;
                        else if (w == 100 && (sf->foe[i].type == FOE_GIANT ? giants_down() : !ver.on || ver.state == VER_DEAD))
                            felled += 100;
                    }
                }
                int attack = all ? felled * 100 / all : 0;
                if (attack > 100)
                    attack = 100;
                int score = acc + life + attack;
                ptas += MISSION_PTAS;
                last_rank = score > 269 ? 4 : score > 209 ? 3 : score > 179 ? 2 : score > 149 ? 1 : 0;
                static const char *const letters[5] = { "D", "C", "B", "A", "S" };
                snprintf(rank, sizeof(rank), "%s", letters[last_rank]);
            }
            result_t++;
            int step = result_t / RESULT_STEP;
            text_big(0, 128 - text_width(T_COMPLETE), 14, TEXT_GOLD, T_COMPLETE);
            for (int i = 0; i < 5; i++)
                if (step >= 2 + i)
                {
                    int y = 84 + i * 14;
                    text_line(i < 4 ? 1 + i : 9, 48, y, TEXT_WHITE, labels[i]);
                    text_line(TEXT_WIDE_LINES + i, 208 - text_width(stats[i]), y, TEXT_GOLD, stats[i]);
                }
            if (step >= 7)
            {
                text_line(5, 48, 160, TEXT_WHITE, T_TOTAL);
                char p[24];
                snprintf(p, sizeof(p), T_PTAS, ptas);
                text_line(8, 128 - text_width(p) / 2, 160, TEXT_GOLD, p);   // TOTAL とランクの間
                text_big(6, 180, 152, last_rank >= 3 ? TEXT_GOLD : TEXT_WHITE, rank);
            }
            if (result_t == RESULT_STEP * 7)
                sfx_play(SFX_ITEM_GET, 110, 64);
            else if (result_t % RESULT_STEP == 0 && step >= 1 && step <= 6)
                sfx_play(SFX_EMPTY, 90, 64);   // 1行ごとにクリック音
            if (step >= 8 && (tick / 30) % 2 == 0)
                text_line(7, 128 - text_width(T_PRESS_A) / 2, 180, TEXT_GREY, T_PRESS_A);
            else
                text_hide(7);
            if (step >= 8 && (down & (KEY_A | KEY_START)))
            {
                sfx_play(SFX_MENU, 100, 64);
                text_clear();
                pending = MODE_SELECT;   // ステージセレクトに戻る
                save_write(spare, herbs, ptas);
            }
            draw_result(seconds, step);
            model_sync();   // （最後の文字の一覧を終えた）
            glFlush(0);
            swiWaitForVBlank();
            text_commit();
            vital_idle(T_CAPTION, T_EDITION);
            continue;
        }

        // ストーリーの場面（cut_start() を参照）：サブ画面のビットマップに描く（vital_cutin()：テクスチャの
        // メモリは要らない。ステージで埋まっていることがある）。画面を入れ替えて上に出す。その間、下になった
        // 3D の画面は黒。
        if (mode == MODE_CUTIN)
        {
            if (menu_shown != MODE_CUTIN)
            {
                text_clear();
                dim_3d(0);
                menu_shown = MODE_CUTIN;
                cut_pic_id = -1;   // （表示中のページ）
                lcdMainOnBottom();
            }
            if (pending < 0 && cut_page < cut->count)
            {
                if ((down & KEY_A) || ++cut_t >= CUT_AUTO)
                {
                    cut_page++;
                    cut_t = 0;
                    if (cut_after == CUT_AFTER_GIANT && cut_page == 1)
                        sfx_play(SFX_KNIFE, 127, 64);   // （触手を斬った）
                }
                if (down & KEY_START)
                    cut_page = cut->count;
                if (cut_page >= cut->count)
                {
                    if (cut_after == CUT_AFTER_GIANT)
                        gcut_page = 2;   // （レオンが彼から降りる）
                    if (cut_fade)
                        pending = cut_then;
                    else
                    {
                        mode = cut_then;
                        menu_shown = -1;
                    }
                    down = 0;   // （それを終えた押下はゲームの方には渡さない）
                }
            }
            if (cut_page < cut->count && cut_page != cut_pic_id)
            {
                const CutPage *pg = &cut->page[cut_page];
                void *buf = NULL;
                size_t size = 0;
                if (pg->pic >= 0)
                {
                    char name[32];
                    snprintf(name, sizeof(name), "nitro:/eve%02d_%d.bin", cut->stage + 1, pg->pic);
                    buf = read_file(name);
                    size = buf ? file_size(name) : 0;
                }
                vital_cutin(buf, size, pg->who && pg->who < sizeof(cut_who) / sizeof(cut_who[0]) ? cut_who[pg->who] : NULL, pg->text);
                free(buf);
                cut_pic_id = cut_page;
            }
            setBrightness(2, -fade);   // （フェード：今は上にあるサブ画面も）
            if (mode != MODE_CUTIN || (pending >= 0 && fade + 2 >= 16))   // （黒：次のフレームは別の画面）
            {
                vital_cutin(NULL, 0, NULL, NULL);   // （黒、下に戻る前に）
                lcdMainOnTop();
                setBrightness(2, 0);
            }
            overlay_begin();
            overlay_rect(0, 0, 256, 192, RGB15(0, 0, 0), 31, 1);
            overlay_end();
            model_sync();
            glFlush(0);
            swiWaitForVBlank();
            text_commit();
            if (mode == MODE_CUTIN)
                continue;
        }
        // ポーズ：新しい 3D のフレームは描かないので、最後のものがメニューの下に暗くなって残る。
        if (mode == MODE_PLAY && ctrl == CTRL_LEON && (down & KEY_START))
        {
            mode = MODE_PAUSE;
            menu_sel = 0;
            down = 0;
        }
        // ポーズメニューは原作の Opções 画面。レイアウトのファイル（Bio4_StringLayout.bin、レコード
        // 288-310）が 320 x 240 の画面に置くとおりに、ここでは中心を基準に0.8倍：タイトル、項目（白、
        // 選んだものは金）、下にピンクで選んだ項目の説明、Retry / Quit には YES / NO の確認（金 / 灰）。
        // Sound は音量を設定（OFF、Low、Medium、High）。Controls は操作説明を出す（原作の操作タイプと
        // 振動は DS では設定するものが無い）。
        if (mode == MODE_PAUSE)
        {
            // （移植版独自の2つ、HUD と Bottom Screen を Controls の前に）
            static const char *const items[] = { T_OPT_RETRY, T_OPT_QUIT, T_OPT_SOUND, T_OPT_HUD, T_OPT_SCREEN, T_OPT_CONTROLS };
            static const u8 item_sub[] = { 1, 2, 3, 5, 6, 4 };   // 各項目のサブ画面
            #define PAUSE_ITEMS 6
            const char *const helps[] = { T_HELP_RETRY, FOES_ON(cur_stage) ? T_HELP_QUIT : T_HELP_LEAVE, T_HELP_SOUND, T_HELP_CONTROLS,
                                          T_HELP_HUD, T_HELP_SCREEN };   // （サブ画面 - 1 で）
            static const char *const vols[] = { T_VOL_OFF, T_VOL_LOW, T_VOL_MID, T_VOL_HIGH };
            static const char *const yes_no[] = { T_YES, T_NO };
            static const char *const on_off[] = { T_HUD_ON, T_HUD_OFF };
            static const char *const views[] = { T_SCR_MAP, T_SCR_ITEMS };
            static const u8 vol_pct[] = { 0, 40, 70, 100 };
            static int sub, sub_sel, sound = 3;   // sub：0 一覧、1 やり直し?、2 終了?、3 サウンド、4 操作説明、5 HUD、6 下画面
            if (menu_shown != MODE_PAUSE)
            {
                text_clear();
                menu_shown = MODE_PAUSE;
                sub = 0;
            }
            dim_3d(sub == 4 ? 12 : 10);
            music_volume(MUSIC_VOLUME / 3);   // ポーズ中は小さく
            int back = (down & KEY_B) != 0, pick = (down & KEY_A) != 0, shown = sub;
            if (sub == 0)
            {
                menu_sel = menu_move(down, menu_sel, PAUSE_ITEMS);
                if (down & KEY_START || back)
                {
                    mode = MODE_PLAY;   // （ゲームに戻る）
                }
                else if (pick)
                {
                    sfx_play(SFX_MENU, 100, 64);
                    sub = item_sub[menu_sel];
                    sub_sel = sub == 3 ? sound : sub == 5 ? !hud_on : sub == 6 ? bottom_items : 1;   // （確認は NO から始まる）
                }
            }
            else if (sub == 5 || sub == 6)
            {
                sub_sel = menu_move(down, sub_sel, 2);
                if (pick)
                {
                    if (sub == 5)
                        hud_on = sub_sel == 0;
                    else
                        bottom_items = sub_sel;
                    sfx_play(SFX_MENU, 100, 64);
                }
                if (pick || back)
                    sub = 0;
            }
            else if (sub == 1 || sub == 2)
            {
                sub_sel = menu_move(down, sub_sel, 2);
                if (back || (pick && sub_sel == 1))
                    sub = 0;
                else if (pick)
                {
                    sfx_play(SFX_MENU, 100, 64);
                    mode = sub != 2 ? MODE_PLAY : FOES_ON(cur_stage) ? MODE_TITLE : MODE_SELECT;   // （空のステージはクリアにならない：見つけたものを持って出る）
                    if (mode == MODE_SELECT)
                        save_write(spare, herbs, ptas);
                    reset = sub == 1;
                }
            }
            else if (sub == 3)
            {
                sub_sel = menu_move(down, sub_sel, 4);
                if (pick)
                {
                    sound = sub_sel;
                    sfx_level(vol_pct[sound]);
                    music_level(vol_pct[sound]);
                    sfx_play(SFX_MENU, 100, 64);
                }
                if (pick || back)
                    sub = 0;
            }
            else if (back || pick || (down & KEY_START))
                sub = 0;
            if (sub != shown)
                text_clear();
            if (mode != MODE_PAUSE)
            {
                text_clear();
                dim_3d(0);
                menu_shown = -1;
            }
            else if (sub == 4)
            {
                // 操作説明、タイトルの操作説明の画面と同じ。
                text_line(0, 128 - text_width(T_OPT_CONTROLS) / 2, 10, TEXT_GOLD, T_OPT_CONTROLS);
                for (int i = 0; i < 8; i++)
                {
                    text_line(TEXT_WIDE_LINES + i, 10, 34 + i * 18, TEXT_GOLD, howto[i * 2]);
                    text_line(2 + i, 72, 34 + i * 18, TEXT_WHITE, howto[i * 2 + 1]);
                }
                text_line(1, 246 - text_width(T_BACK), 176, TEXT_GREY, T_BACK);
            }
            else
            {
                text_line(10, 128 - text_width(T_OPTIONS) / 2, 13, TEXT_WHITE, T_OPTIONS);           // y -104
                if (sub == 1 || sub == 2)
                {
                    const char *q = sub == 1 ? T_ASK_RETRY : FOES_ON(cur_stage) ? T_ASK_QUIT : T_ASK_LEAVE;
                    text_line(4, 128 - text_width(q) / 2, 96, TEXT_WHITE, q);                     // y 4
                    for (int i = 0; i < 2; i++)                                                     // y 28、52
                        text_line(5 + i, 128 - text_width(yes_no[i]) / 2, 115 + i * 19, i == sub_sel ? TEXT_GOLD : TEXT_GREY, yes_no[i]);
                }
                else if (sub == 3)
                    for (int i = 0; i < 4; i++)                                                     // y -44 ...
                        text_line(i, 128 - text_width(vols[i]) / 2, 58 + i * 19, i == sub_sel ? TEXT_GOLD : TEXT_WHITE, vols[i]);
                else if (sub == 5 || sub == 6)
                    for (int i = 0; i < 2; i++)
                    {
                        const char *s = sub == 5 ? on_off[i] : views[i];
                        text_line(i, 128 - text_width(s) / 2, 77 + i * 19, i == sub_sel ? TEXT_GOLD : TEXT_WHITE, s);
                    }
                else
                    for (int i = 0; i < PAUSE_ITEMS; i++)                                           // y -68、-44、...（6は少し近く）
                        text_line(i, 128 - text_width(items[i]) / 2, 34 + i * 18, i == menu_sel ? TEXT_GOLD : TEXT_WHITE, items[i]);
                const char *h = helps[(sub == 0 ? item_sub[menu_sel] : sub) - 1];
                text_line(9, 128 - text_width(h) / 2, 150, TEXT_PINK, h);                          // y 73
            }
            swiWaitForVBlank();
            text_commit();
            continue;
        }

        // 絵の無いミッションのイベントのせりふ（鍵のかかった扉、アイテムの入手：原作はプレイ画面の上の
        // 枠に出し、ゲームは止まる）：最後の 3D のフレームの上に、暗くして、話す人を上に金色で。
        // A（か B）で次のページ。
        if (mode == MODE_MESSAGE)
        {
            static const char *const who_names[] = { "", "Leon", "Ashley", "Luis", "Saddler" };
            if (menu_shown != MODE_MESSAGE)
            {
                text_clear();
                menu_shown = MODE_MESSAGE;
                msg_t = 0;
            }
            dim_3d(7);
            msg_t++;
            if (msg_t > 10 && (down & (KEY_A | KEY_B)))
            {
                msg_page++;
                msg_t = 0;
                text_clear();
                sfx_play(SFX_MENU, 80, 64);
            }
            if (!msg || msg_page >= msg->count)
            {
                text_clear();
                dim_3d(0);
                menu_shown = -1;
                mode = MODE_PLAY;
                down = 0;
            }
            else
            {
                const CutPage *pg = &msg->page[msg_page];
                if (pg->who > 0 && pg->who < 5)
                    text_line(0, 128 - text_width(who_names[pg->who]) / 2, 62, TEXT_GOLD, who_names[pg->who]);
                char line[48];
                const char *s = pg->text;
                for (int k = 0; k < 3 && *s; k++)
                {
                    int n = 0;
                    while (s[n] && s[n] != '\n' && n < (int)sizeof(line) - 1)
                        n++;
                    memcpy(line, s, n);
                    line[n] = 0;
                    text_line(1 + k, 128 - text_width(line) / 2, 82 + k * 16, TEXT_WHITE, line);
                    s += n + (s[n] == '\n');
                }
            }
            swiWaitForVBlank();
            text_commit();
            continue;
        }

        // ステータス画面（SELECT。draw_status() を参照）：その間ゲームは止まる。
        if (mode == MODE_STATUS)
        {
            // その行：ハンドガン（中の弾）、AMMO_STACK までの箱に分けたハンドガンの弾、グリーンハーブ1つずつ。
            StItem list[24];
            int n = 0, used = 0;
            for (int w = 0; w < WEP_COUNT; w++)
                if (wep_owned >> w & 1)
                    list[n++] = (StItem){ ST_GUN, w == wep ? ammo : wep_ammo[w], w };
            for (int s = spare; s > 0 && n < 18; s -= MULTI_BOX)   // （多機能弾丸、MULTI_BOX の箱で、手に持っている銃のアイコンで）
                list[n++] = (StItem){ ST_AMMO, s < MULTI_BOX ? s : MULTI_BOX, multi_ammo_gun(wep) };
            for (int h = 0; h < herbs && n < 24; h++)
                list[n++] = (StItem){ ST_HERB, 1, 0 };
            used = case_used(spare, herbs);
            if (menu_shown != MODE_STATUS)
            {
                text_clear();
                menu_shown = MODE_STATUS;
                if (!menu_loaded)
                    menu_loaded = sheet_load(&menu_sheet, menu_bin, menu_bin_size) == 0;
                // テクスチャ VRAM に空きが無い（7面：壁、教徒、アシュリー、樽で埋まる。画面の枠が欠けて出た）：
                // 画面を出している間は教徒のモデルをしまい、後で読み直す。
                if (!menu_loaded && jya_loaded)
                {
                    jya_unload();
                    st_jya_out = 1;
                    menu_loaded = sheet_load(&menu_sheet, menu_bin, menu_bin_size) == 0;
                }
                glClearColor(0, 0, 0, 31);
                set_fov(ST_FOV);   // （画面のレオンはこの視点用に置く。プレイの視点用ではない）
            }
            if (st_sel >= n)
                st_sel = n - 1;
            // 選んだ行のコマンド（原作のもの：武器は Equipar / Cancelar、弾は Descartar / Cancelar、ハーブは
            // Usar / Descartar / Cancelar。Combinar と、唯一の武器を捨てることは省く）。
            enum { CMD_EQUIP, CMD_USE, CMD_DISCARD, CMD_CANCEL };
            static const u8 gun_cmds[] = { CMD_EQUIP, CMD_CANCEL }, ammo_cmds[] = { CMD_DISCARD, CMD_CANCEL },
                            herb_cmds[] = { CMD_USE, CMD_DISCARD, CMD_CANCEL };
            static const char *const cmd_text[] = { T_ST_EQUIP, T_ST_USE, T_ST_DISCARD, T_ST_CANCEL };
            const StItem *cur = &list[st_sel];
            const u8 *cmds = cur->type == ST_GUN ? gun_cmds : cur->type == ST_AMMO ? ammo_cmds : herb_cmds;
            int ncmd = cur->type == ST_HERB ? 3 : 2;
            if (st_cmd < 0)
            {
                if (down & (KEY_B | KEY_SELECT | KEY_START))
                {
                    // ゲームに戻る。
                    mode = MODE_PLAY;
                    text_clear();
                    menu_shown = -1;
                    if (menu_loaded)
                        sheet_free(&menu_sheet);
                    menu_loaded = 0;
                    if (st_jya_out)
                    {
                        st_jya_out = 0;
                        if (jya_load() == 0)
                        {
                            enemies_set_look(&ganados, &jya_model, &jya_anims, &jya_w0);
                            jya_arm();
                        }
                        else
                            enemies_set_look(&ganados, &ganado, &ganado_anims, &ganado_w0);   // （また空きが無い：ガナードの見た目。解放したモデルは決して使わない）
                    }
                    set_haze(stage_info[cur_stage].haze);
                    stage_fog(stage_info[cur_stage].fog);
                    set_fov(CAM_FOV);   // （またプレイの視点のもの）
                    down = 0;
                }
                else
                {
                    int was = st_sel;
                    st_sel = menu_move(down, st_sel, n);
                    if (st_sel != was)
                        sfx_play(SFX_MENU, 60, 64);
                    if (down & KEY_A)
                    {
                        st_cmd = 0;
                        sfx_play(SFX_MENU, 100, 64);
                    }
                }
            }
            else
            {
                st_cmd = menu_move(down, st_cmd, ncmd);
                if (down & KEY_B)
                    st_cmd = -1;
                else if (down & KEY_A)
                {
                    int c = cmds[st_cmd];
                    if (c == CMD_EQUIP && cur->weapon != wep)
                    {
                        weapon_switch(cur->weapon, &ammo, &spare);
                        fire_frame = reload_frame = -1;
                        clip_key = -1;
                    }
                    if (c == CMD_USE && leon_hp >= LEON_HP)
                        st_msg = 90;   // （回復するものが無い）
                    else if (c == CMD_USE)
                    {
                        herbs--;
                        leon_hp = leon_hp + HERB_HEAL > LEON_HP ? LEON_HP : leon_hp + HERB_HEAL;
                        sfx_play(SFX_ITEM_GET, 110, 64);
                    }
                    else if (c == CMD_DISCARD)
                    {
                        if (cur->type == ST_AMMO)
                            spare -= cur->count;
                        else
                            herbs--;
                    }
                    if (c != CMD_USE)
                        sfx_play(SFX_MENU, 100, 64);
                    st_cmd = -1;
                }
            }
            if (st_sel < st_top)
                st_top = st_sel;
            if (st_sel >= st_top + ST_ROWS)
                st_top = st_sel - ST_ROWS + 1;
            if (mode == MODE_STATUS)
            {
                // 右にレオン、銃を構えて、一覧の方へ少し向いて。
                static NodeLocal st_local[MODEL_MAX_NODES];
                static int st_pose[MODEL_MAX_NODES][12];
                anim_sample(&leon_gun, GUN_READY_POSE, 0, &leon, st_local);
                anim_compose(&leon, st_local, st_pose);
                int h = leon.bbox_max[1] - leon.bbox_min[1];
                int ex = h * 3 / 4, ey = h / 2, ez = h * 5 / 4;
                // 彼の後ろ、一覧のパネルを画面の右端まで続ける（ケースの暗い内側。何も無いと一覧の先に
                // 3D の層の黒が見えた）。
                overlay_begin();
                overlay_far();
                if (menu_loaded)
                    for (int i = 0; i < 6; i++)
                        menu_piece(80, 32, 156, 32, 48, -85 + 32 * i, 112, 32);
                else
                    overlay_rect(ST_X(48), ST_Y(-85), 256, ST_Y(80), RGB15(3, 3, 4), 31, 25);
                overlay_end();
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
                gluLookAtf32(-ex, ey, ez, -ex, ey, 0, 0, inttof32(1), 0);
                glColor(RGB15(31, 31, 31));
                int p[3] = { 0, -leon.bbox_min[1], 0 }, py = -degreesToAngle(20);
                model_draw(&leon, p, py, POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(2), (const int (*)[12])st_pose);
                model_draw_held(&gun, p, py, POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(3), st_pose[HAND_NODE]);
                draw_status(list, n, st_top, st_sel, st_cmd >= 0 ? ncmd : 0);
                // レオンのプレート（体力と弾）を所持金のバーのすぐ上に（ST_Y(80)、y 160）。
                overlay_begin();
                draw_plate(leon_hp, ammo, ST_PLATE_DY);
                overlay_end();

                // 文字（Bio4_StringLayout.bin 444-485）。
                if (st_msg > 0)
                {
                    st_msg--;
                    text_line(0, ST_X(-140), ST_Y(-103), TEXT_PINK, T_FULL);
                }
                else
                    text_line(0, ST_X(-140), ST_Y(-103), TEXT_WHITE, T_WEAPONS);
                static const char *const names[] = { T_ITEM_GUN, T_ITEM_AMMO, T_ITEM_HERB };
                for (int r = 0; r < ST_ROWS; r++)
                {
                    int k = st_top + r, line = 1 + r * 3;
                    if (k >= n)
                    {
                        for (int j = 0; j < 3; j++)
                            text_hide(line + j);
                        continue;
                    }
                    char num[8];
                    text_line(line, ST_X(-106), ST_Y(-54 + 31 * r), TEXT_WHITE, list[k].type == ST_GUN ? weapons[list[k].weapon].name
                              : names[list[k].type]);
                    snprintf(num, sizeof(num), "%d", list[k].count);
                    if (list[k].type == ST_HERB)
                        text_hide(line + 1);
                    else
                        text_line(line + 1, ST_X(4) - text_width(num), ST_Y(-54 + 31 * r), TEXT_WHITE, num);
                    snprintf(num, sizeof(num), "%d", list[k].type == ST_GUN ? weapons[list[k].weapon].tam : st_tam[list[k].type]);
                    text_line(line + 2, ST_X(34) - text_width(num), ST_Y(-54 + 31 * r), TEXT_GREY, num);
                }
                char m[24], lead[16];
                snprintf(lead, sizeof(lead), "%d ", ptas);
                snprintf(m, sizeof(m), "%d PTAS", ptas);
                text_line(13, ST_X(-84) - text_width(lead), ST_Y(89), TEXT_WHITE, m);   // 原作が置く所に「PTAS」
                snprintf(m, sizeof(m), "%03d/%03d", used, case_tam[case_level]);
                text_line(14, ST_X(91), ST_Y(89), TEXT_WHITE, m);
                for (int i = 0; i < 3; i++)
                    if (st_cmd >= 0 && i < ncmd)
                        text_line(15 + i, ST_X(65), ST_Y(-33 + 16 * i), i == st_cmd ? TEXT_GOLD : TEXT_GREY, cmd_text[cmds[i]]);
                    else
                        text_hide(15 + i);
                model_sync();
                glFlush(0);
            }
            swiWaitForVBlank();
            text_commit();
            continue;
        }

        // レオンが生きている間は村のテーマが流れる（ニューゲームか続きから、最初から）。
        // 彼が死ぬと止まる。
        if (reset)
            music_play(MUSIC_VILLAGE, MUSIC_VOLUME);
        else if (clear_t >= BELL_FIRST)
        {
            // ステージクリア：鐘の下で音楽がフェードアウトする。
            int v = MUSIC_VOLUME - (clear_t - BELL_FIRST) * MUSIC_VOLUME / 90;
            if (v > 0)
                music_volume(v);
            else
                music_stop();
        }
        else if (react != REACT_DEAD)
            music_play(MUSIC_VILLAGE, MUSIC_VOLUME);
        else
            music_stop();

        // ゲームオーバー：メニューの後ろで場面は続く。レオンは入力を受けない。
        if (mode == MODE_GAMEOVER)
        {
            if (menu_shown != MODE_GAMEOVER)
            {
                text_clear();
                menu_sel = 0;
                menu_shown = MODE_GAMEOVER;
            }
            // 少しの間ボタンを無視する。連打（振りほどき）で項目を選ばないように。
            if (dead_timer < DEAD_TO_MENU + MENU_DELAY)
            {
                dead_timer++;
                down = 0;
            }
            menu_sel = menu_move(down, menu_sel, mode == MODE_TITLE ? 2 + have_save : 2);
            const char *dead = ash.on && ash.state == ASH_DOWN ? T_ASH_DEAD : T_DEAD;
            text_line(4, 128 - text_width(dead) / 2, 64, TEXT_RED, dead);
            show_menu(0, 128, 100, over_items, 2, menu_sel);
            if (down & (KEY_A | KEY_START))
            {
                sfx_play(SFX_MENU, 100, 64);
                text_clear();
                menu_shown = -1;
                if (menu_sel == 0)
                {
                    mode = MODE_PLAY;
                    reset = 1;
                }
                else
                {
                    mode = MODE_TITLE;
                    continue;
                }
            }
            if (mode == MODE_GAMEOVER)
                held = down = 0;
        }

        if (down & KEY_SELECT)
        {
            if (held & KEY_Y)   // Y + SELECT
                cull = (cull + 1) % 3;
            else if (ctrl == CTRL_LEON && !(held & KEY_L))
            {
                // ステータス画面（原作のボタン3）、レオンが自由に動けるとき。
                if (mode == MODE_PLAY && react == REACT_NONE)
                {
                    mode = MODE_STATUS;
                    st_sel = st_top = 0;
                    st_cmd = -1;
                }
            }
            else if (ctrl == CTRL_LEON)   // L + SELECT
            {
                // 追従カメラの位置から、同じ向きでフリーカメラに切り替える。
                ctrl = CTRL_FREECAM;
                for (int k = 0; k < 3; k++)
                    eye[k] = follow_eye[k];
                yaw = DEGREES_IN_CIRCLE / 2 - leon_yaw;
                pitch = -degreesToAngle(8);
                leon_anim = ANIM_IDLE;
                gun_state = GUN_HOLSTERED;   // フリーカメラのデモは銃なしで歩く
            }
            else
            {
                ctrl = CTRL_LEON;
                anim_frame = 0;
            }
        }

        if (reset)
        {
            // ニューゲーム：スタート地点に戻り、銃はしまい、体力は満タン、ガナードは元の位置に、
            // カメラはレオンのすぐ後ろ。
            reset = 0;
            ctrl = CTRL_LEON;
            for (int k = 0; k < 3; k++)
                leon_pos[k] = start_pos[k];
            leon_feet = start_feet;
            leon_yaw = start_yaw;
            leon_anim = ANIM_IDLE;
            anim_frame = 0;
            gun_state = GUN_HOLSTERED;
            aim_w = aim_tilt = 0;
            fire_frame = reload_frame = -1;
            flash_timer = impact_timer = cam_kick = 0;
            // ミッションに入る（店から）：持ち込むものはそのまま。やり直し：それに戻す。
            if (cut_opening || !mis_kept)
            {
                mis_rounds = spare;
                mis_herbs = herbs;
                mis_ptas = ptas;
                mis_kept = 1;
            }
            else
            {
                spare = mis_rounds;
                herbs = mis_herbs;
                ptas = mis_ptas;
            }
            // 銃は全部装填。弾、ハーブ、お金はミッションからミッションへ持ち越す。
            for (int w = 0; w < WEP_COUNT; w++)
                wep_ammo[w] = wep_mag(w);
            ammo = MAG_SIZE;
#ifdef TEST_ALL_WEAPONS   // テスト用ビルド：全部の銃と、少しの弾
            wep_owned = (1 << WEP_COUNT) - 1;
            spare += 100;
#endif
            leon_hp = LEON_HP;
#ifdef TEST_HURT   // テスト用ビルド：負傷した状態で、ハーブを持って始める
            herbs = 3;
            leon_hp = 30;
#endif
            react = REACT_NONE;
            holder = -1;
            safe_timer = hurt_flash = dead_timer = popup = damage_taken = 0;
            hits = shots = headshots = 0;
            clear_t = -1;
            play_frames = 0;
            drops_clear(&drops);
            frames = 0;   // FPS の表示はゲームのフレームだけを数える：新しく数え始める
            fps = 60;
            fps_mark = vblank_count;
#ifdef TEST_DROPS   // テスト用ビルド：レオンの前に弾の箱とハーブ、見るため
            for (int k = 0; k < 2; k++)
            {
                int p[3] = { leon_pos[0] + inttof32(k ? 9 : -6), leon_feet, leon_pos[2] + inttof32(14) };
                ground_height(&stage_ground, p[0], p[2], leon_feet, STEP_UP, &p[1]);
                drops_spawn(&drops, p, k ? DROP_HERB : DROP_AMMO, 0);
            }
#endif
            objects_reset(&objs);
            // プレイにはメニューの絵は要らない（その VRAM はチェーンソー男が来たら彼に回す）。
            sheet_free(&picture);
            picture_id = -1;
            cha_unload();
            // 選んだステージ：そのモデル、地面、もや。
            stage_budget = -1;   // （新しいステージ：最初は全部）
            enemy_show = ENEMY_MAX;
            time_cap = 4000;
            enemy_cap = ENEMY_MAX;
            if (load_stage(cur_stage))
            {
                consoleDemoInit();
                printf("stage %d: load failed\n", cur_stage + 1);
                while (1)
                    swiWaitForVBlank();
            }
            set_haze(stage_info[cur_stage].haze);
            stage_fog(stage_info[cur_stage].fog);
            cam_setup(cur_stage);
            if (cur_stage == 0)
            {
                objects_init(&objs, &stage_ground, &door_model, &ladder_model, &barrel_model, &crate_model, &medal_model, &boom_sheet, &spark);
                objs.bell = &bell_model;
            }
            else
                objects_clear(&objs);   // 仕掛け（はしご、木箱）は1面でだけ置く
            // ミッションの宝、原作が置く所で光る：床の上のものは上を歩けば取れる。高い所のものは
            // 撃つと下の床に落ちる。
            treasures_found = 0;
            {
                const StageInfo *si = &stage_info[cur_stage];
                for (int i = 0; i < si->treasures; i++)
                {
                    int p[3] = { inttof32(si->treasure[i][0]), inttof32(si->treasure[i][1]), inttof32(si->treasure[i][2]) };
                    // 落ちる場所：それよりずっと下の床（それが乗っている屋根や梁ではない）、壁から押し出して
                    // （軒の上のものは家に接して落ちる）。それほど下に床が無ければ：その場にある。
                    int land[3] = { p[0], p[1], p[2] };
                    ground_height(&stage_ground, p[0], p[2], p[1] - inttof32(8), 0, &land[1]);
                    ground_collide(&stage_ground, &land[0], &land[2], land[1] + STEP_UP, land[1] + LEON_HEIGHT, inttof32(6));
                    ground_height(&stage_ground, land[0], land[2], land[1] + STEP_UP, STEP_UP, &land[1]);
                    if (cur_stage == 3 && i == 1)   // （4面のもの：鉱山の外壁の上。レオンの側に落ちる）
                        land[0] = inttof32(314);
                    // （その価値：イベントがくれるものの値段、item_base_info.bin：スピネル2000、ベルベットブルー3000、
                    // ルビー10000…。移植版は取った時点で売る）
                    const EvItem *ti = ev_item(stage_treasure_item[cur_stage][i < 2 ? i : 1]);
                    drops_place_treasure(&drops, p, land, ti && ti->price ? ti->price : TREASURE_PTAS);
                }
            }
            boss_phase = boss_t = 0;
            obj_t = OBJ_FRAMES;
            elevator_note = 0;
            if (cur_stage == 0)
            {
                enemies_init(&ganados, &ganado, &ganado_anims, &stage_ground, &objs, ganado_spawn, GANADO_COUNT, DEGREES_IN_CIRCLE / 2);
                enemies_set_look(&ganados, &ganado, &ganado_anims, &ganado_w0);   // （下のほかのステージのものを参照）
                stage1_kinds();
#ifdef TEST_SIGHT   // テスト用ビルド：これだけ遠く（単位）からレオンを狙って来る。たとえば建物を回り込んで
                ganados.sight = inttof32(TEST_SIGHT);
#endif
            }
            else
            {
                // ミッション自身の敵（stage_foes.c）、グループごとに入る（foes_step()）：ガナードか教徒、
                // それぞれの出現地点にその向きで。倒れるとレコードが指定するものを落とす（mission_drop()）。
                // 7面の城壁の上の教徒（その注記：上の男たちの合図で、ほかの者が燃える岩を投げる。上には床が無い）は
                // 開始時に作る専用のスロットを持ち、レコードの高さにとどまって投げる。
                const StageFoes *sf = &stage_foes[cur_stage];
                foe_n = 0;
                int throwers = 0, tpts[4][3];
                for (int i = 0; FOES_ON(cur_stage) && i < sf->count && foe_n < FOES_MAX; i++)
                    if (sf->foe[i].type == FOE_GANADO || sf->foe[i].type == FOE_CULTIST)
                    {
                        foe_fixed[foe_n] = -1;
                        if (sf->foe[i].type == FOE_CULTIST && sf->foe[i].pos[1] >= WALL_TOP)
                        {
                            if (throwers == 4)
                                continue;
                            for (int k = 0; k < 3; k++)
                                tpts[throwers][k] = sf->foe[i].pos[k];
                            foe_fixed[foe_n] = throwers++;   // （+ 床のスロット、下で）
                        }
                        foe_left[foe_n] = sf->foe[i].entries;
                        foe_slot[foe_n] = -1;
                        foe_list[foe_n++] = &sf->foe[i];
                    }
                foe_slots = FOE_UNITS;
                foe_entered = 0;
                foe_held = 0;
                int pts[ENEMY_MAX][3], n = 0;
                for (; n < foe_slots; n++)
                    for (int k = 0; k < 3; k++)
                        pts[n][k] = sf->start[k];   // （来たときにそれぞれ自分の地点に置く）
                int first_thrower = n;
                for (int i = 0; i < throwers; i++, n++)
                    for (int k = 0; k < 3; k++)
                        pts[n][k] = tpts[i][k];
                for (int f = 0; f < foe_n; f++)
                    if (foe_fixed[f] >= 0)
                        foe_fixed[f] += first_thrower;
                for (int s = 0; s < ENEMY_MAX; s++)
                    slot_foe[s] = -1;
                enemies_init(&ganados, &ganado, &ganado_anims, &stage_ground, &objs, (const int (*)[3])pts, n, DEGREES_IN_CIRCLE / 2);
                // ガナード自身の片手武器のクリップに戻す：前の城のステージが教徒のもの（jya_w0）を残していて、
                // そのバッファは jya_unload() が解放するので、武器を持ったガナードが解放したメモリからポーズを
                // 取っていた：メッシュが画面に引き伸ばされ、フレームレートが落ちた。
                enemies_set_look(&ganados, &ganado, &ganado_anims, &ganado_w0);
                ganados.sight = ENEMY_SIGHT;
                ganados.watch = 1;
                for (int f = 0; f < foe_n; f++)
                    if (foe_fixed[f] >= 0)
                    {
                        int i = foe_fixed[f];
                        enemies_make_thrower(&ganados, i);
                        enemies_set_kind(&ganados, i, foe_kind_of(foe_list[f]));
                        enemies_perch(&ganados, i, pts[i][1]);
                        ganados.e[i].yaw = foe_list[f]->dir * (DEGREES_IN_CIRCLE / 256);
                        ganados.e[i].drop = foe_list[f]->drop;
                    }
                foes_events_start(sf, cur_stage);
                // ステージの種類にプラーガがある（3面の最初のガナード）：そのモデル。
                {
                    int with_plaga = 0;
                    for (int f = 0; f < foe_n; f++)
                        with_plaga |= foe_kind_of(foe_list[f])->plaga;
                    if (with_plaga)
                        plaga_load();   // （失敗したら、頭が無く、そこには何も描かない）
                    else
                        plaga_unload();
                }

#ifdef TEST_THROWER_AUDIT   // テスト用ビルド：各投げる者を、床の何か所（4単位の格子）から撃てるか
                {
                    consoleDemoInit();
                    for (int i = 0; i < ganados.count; i++)
                    {
                        Enemy *e = &ganados.e[i];
                        if (!e->thrower)
                            continue;
                        e->active = 1;   // （後でグループと一緒に来る）
                        int ok = 0, steep = 0, walled = 0, missed = 0, best_d = 0;
                        for (int gx = -50; gx <= 50; gx++)
                            for (int gz = -50; gz <= 50; gz++)
                            {
                                int x = e->pos[0] + gx * inttof32(4), z = e->pos[2] + gz * inttof32(4), y = 0;
                                if (!ground_height(&stage_ground, x, z, inttof32(60), 0, &y) || y > inttof32(60))
                                    continue;
                                int eye[3] = { x, y + inttof32(14), z }, to[3] = { e->pos[0], e->feet + inttof32(10), e->pos[2] };
                                int d[3] = { to[0] - eye[0], to[1] - eye[1], to[2] - eye[2] };
                                int len = (int)sqrt64((u64)((s64)d[0] * d[0] + (s64)d[1] * d[1] + (s64)d[2] * d[2]));
                                if (len <= 0) continue;
                                int hl = (int)sqrt64((u64)((s64)d[0] * d[0] + (s64)d[2] * d[2]));
                                if (d[1] > mulf32(hl, floattof32(1.19)))   // 50度より上
                                { steep++; continue; }
                                for (int k = 0; k < 3; k++) d[k] = divf32(d[k], len);
                                int hit[3], t_best = len + inttof32(5), part;
                                if (ground_raycast(&stage_ground, eye, d, len, hit, RAY_SHOT))
                                {
                                    s64 hx = hit[0] - eye[0], hy = hit[1] - eye[1], hz = hit[2] - eye[2];
                                    t_best = (int)sqrt64((u64)(hx * hx + hy * hy + hz * hz)) + THROWER_REACH;   // （弾と同じく）
                                    if (t_best < len) { walled++; continue; }
                                }
                                if (enemies_raycast(&ganados, eye, d, &t_best, &part) != i)
                                { missed++; continue; }
                                ok++;
                                if (!best_d || hl < best_d) best_d = hl;
                            }
                        printf("t%d %d,%d,%d ok%d steep%d wall%d miss%d near%d\n", i, f32toint(e->pos[0]), f32toint(e->feet), f32toint(e->pos[2]),
                               ok, steep, walled, missed, f32toint(best_d));
                    }
                    while (1) swiWaitForVBlank();
                }
#endif
                // 城では教徒。
                if (cur_stage >= CASTLE_FROM && foe_n + throwers > 0)
                {
                    if (jya_load())
                    {
                        consoleDemoInit();
                        printf("cultists: load failed\n");
                        while (1)
                            swiWaitForVBlank();
                    }
                    enemies_set_look(&ganados, &jya_model, &jya_anims, &jya_w0);
                    jya_arm();
                }
                else
                    jya_unload();
            }
            if (cur_stage == 0)
                for (next_ganado = 0; next_ganado < GANADO_ACTIVE && next_ganado < GANADO_FROM_DOOR; next_ganado++)
                    enemies_activate(&ganados, next_ganado, NULL);
            else
                {
                // （レオンが始まる所：ミッションのスタート地点）
                const StageFoes *st = &stage_foes[cur_stage];
                int at[3] = { inttof32(st->start[0]), inttof32(st->start[1]), inttof32(st->start[2]) };
                foes_step(at);   // （グループ0の最初、GM_CtrlMain_Enemy::Set_Start がユニットを埋めるとおり）
#ifdef TEST_FOE_DUMP   // テスト用ビルド：最初の者が立つ場所、それから止める
                consoleDemoInit();
                printf("start %d %d %d slots %d n %d\n", f32toint(at[0]), f32toint(at[1]), f32toint(at[2]), foe_slots, foe_n);
                printf("groups %x entered %d events %d\n", foe_groups, foe_entered, stage_foes[cur_stage].events);
                {
                    int nb[3] = { 0, 0, 0 };
                    foe_flood(at);
                    int okp = foe_near(0, nb);
                    printf("near %d: %d %d %d cells %d y0 %d moved %d\n", okp, nb[0], nb[1], nb[2], foe_cells, foe_y0, foe_moves);
                }
                for (int f = 0; f < foe_n; f++)   // レコード：グループ、AI、残り、スロット
                    printf("r%02d g%d a%d %d %2d%s", foe_list[f]->rec, foe_list[f]->wave, foe_list[f]->ai, foe_left[f], foe_slot[f], f % 3 == 2 ? "\n" : " ");
                printf("\n");
                for (int i = 0; i < ganados.count && i < 9; i++)
                    printf("%2d a%d %4d %4d %4d s%d\n", i, ganados.e[i].active, f32toint(ganados.e[i].pos[0]), f32toint(ganados.e[i].feet), f32toint(ganados.e[i].pos[2]), ganados.e[i].spotted);
                while (1)
                    swiWaitForVBlank();
#endif
            }
#if !defined(AUTOPILOT) || defined(STAGE_START)
            // スタート：村では南の門のすぐ内側、柵のある道の先。ほかのステージではミッションファイルが彼を
            // 置く所（タイプ0のレコード）、その向きで。
            if (cur_stage == 0)
            {
                leon_pos[0] = inttof32(240);
                leon_pos[2] = inttof32(500);
                leon_yaw = degreesToAngle(180);
            }
            else
            {
                const StageFoes *sf = &stage_foes[cur_stage];
                leon_pos[0] = inttof32(sf->start[0]);
                leon_pos[2] = inttof32(sf->start[2]);
                leon_yaw = sf->start_dir * (DEGREES_IN_CIRCLE / 256);
            }
#if defined(LEON_START_X)
            leon_pos[0] = inttof32(LEON_START_X);
            leon_pos[2] = inttof32(LEON_START_Z);
#endif
#if defined(LEON_START_YAW)
            leon_yaw = degreesToAngle(LEON_START_YAW);
#endif
            // 地点の高さの床の上（その上の屋根ではない）。
            int ref = inttof32(10000);
            if (cur_stage != 0)
                ref = inttof32(stage_foes[cur_stage].start[1] + 4);
#if defined(LEON_START_Y)
            ref = inttof32(LEON_START_Y);
#endif
            if (!ground_height(&stage_ground, leon_pos[0], leon_pos[2], ref, 0, &leon_feet))
                ground_height(&stage_ground, leon_pos[0], leon_pos[2], inttof32(10000), 0, &leon_feet);   // それほど低いものが無い
            leon_pos[1] = leon_feet - leon.bbox_min[1];
#endif
            // エルヒガンテ：彼のステージに（4面に1体、11面に2体）、ミッションファイルの彼の地点に。
            // （GIANT_AHEAD_TEST のテスト用ビルド：レオンの前、2体目は横のもっと遠く。）
            for (int i = 0; i < GIANT_MAX; i++)
                giants[i].on = 0;
            giant_count = stage_bosses(cur_stage, FOE_GIANT);
            if (giant_count > GIANT_MAX)
                giant_count = GIANT_MAX;
            if (giant_count && giant_load(&giants[0]))
            {
                consoleDemoInit();
                printf("El Gigante: load failed\n");
                while (1)
                    swiWaitForVBlank();
            }
            for (int i = 0; i < giant_count; i++)
            {
                if (i)
                    giant_share(&giants[i], &giants[0]);
#ifdef GIANT_AHEAD_TEST
                int a = leon_yaw + i * degreesToAngle(GIANT2_DEG), r = GIANT_AHEAD_U + i * GIANT2_FARTHER;
                giant_place(&giants[i], &stage_ground, leon_pos[0] + mulf32(sinLerp(a), r),
                            leon_pos[2] + mulf32(cosLerp(a), r), leon_feet + inttof32(6), a + DEGREES_IN_CIRCLE / 2);
#else
                const StageFoe *f = stage_boss(cur_stage, FOE_GIANT, i);
                giant_place(&giants[i], &stage_ground, inttof32(f->pos[0]), inttof32(f->pos[2]),
                            inttof32(f->pos[1] + 6), f->dir * (DEGREES_IN_CIRCLE / 256));
#endif
                giants[i].on = 1;
            }
            if (!giant_count)
                for (int i = GIANT_MAX - 1; i >= 0; i--)
                    giant_unload(&giants[i]);
            // ガラドール：8面に1体、9面に2体（「A Dupla de Guardiões」）、ミッションファイルの彼らの地点で、
            // 聞き耳を立てて。（GAL_AHEAD のテスト用ビルド：1体、レオンの前。）
            for (int i = 0; i < GAL_MAX; i++)
                gals[i].on = 0;
            gal_count = stage_bosses(cur_stage, FOE_GARRADOR);
            if (gal_count > GAL_MAX)
                gal_count = GAL_MAX;
            if (gal_count && garrador_load(&gals[0]))
            {
                consoleDemoInit();
                printf("Garrador: load failed\n");
                while (1)
                    swiWaitForVBlank();
            }
            for (int i = 0; i < gal_count; i++)
            {
                if (i)
                    garrador_share(&gals[i], &gals[0]);
#ifdef GAL_AHEAD
                garrador_place(&gals[i], &stage_ground, leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(GAL_AHEAD + i * 20)),
                               leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(GAL_AHEAD + i * 20)), leon_feet + inttof32(6),
#ifdef TEST_GAL_BACK   // テスト用ビルド：レオンに背を向けて（寄生体が射線上に）
                               leon_yaw);
#else
                               leon_yaw + DEGREES_IN_CIRCLE / 2);
#endif
#else
                const StageFoe *f = stage_boss(cur_stage, FOE_GARRADOR, i);
                garrador_place(&gals[i], &stage_ground, inttof32(f->pos[0]), inttof32(f->pos[2]),
                               inttof32(f->pos[1] + 4), f->dir * (DEGREES_IN_CIRCLE / 256));
#endif
                gals[i].on = 1;
            }
            if (!gal_count)
                for (int i = GAL_MAX - 1; i >= 0; i--)
                    garrador_unload(&gals[i]);
            // ヴェルデューゴ：彼のステージで、レオンの前に。タンクはレオンのまわりの輪の上、彼の高さの床が
            // あって壁が無い所に。
            ver.on = 0;
            if (cur_stage == VER_STAGE && FOES_ON(cur_stage))
            {
                if (verdugo_load(&ver) || (!barrel_loaded && model_load(&barrel_model, barrel_bin, barrel_bin_size)))
                {
                    consoleDemoInit();
                    printf("Verdugo: load failed\n");
                    while (1)
                        swiWaitForVBlank();
                }
                barrel_loaded = 1;
#ifdef VER_AHEAD_TEST
                verdugo_place(&ver, &stage_ground, leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(VER_AHEAD)),
                              leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(VER_AHEAD)), leon_feet + inttof32(6),
                              leon_yaw + DEGREES_IN_CIRCLE / 2);
#else
                const StageFoe *f = stage_boss(cur_stage, FOE_VERDUGO, 0);   // （ミッションファイルの彼の地点）
                verdugo_place(&ver, &stage_ground, inttof32(f->pos[0]), inttof32(f->pos[2]), inttof32(f->pos[1] + 6),
                              f->dir * (DEGREES_IN_CIRCLE / 256));
#endif
                ver.on = 1;
                objects_clear(&objs);
                objs.cold = 1;
                if (gimmick_model(&tank_model, &tank_loaded, 1, tank_bin, tank_bin_size))
                    objs.barrel = &tank_model;   // （原作の液体窒素のボンベ、fix_all #6）
                const StageInfo *si = &stage_info[cur_stage];
                for (int i = 0; i < si->tanks; i++)
                    objects_add_barrel(&objs, inttof32(si->tank[i][0]), inttof32(si->tank[i][2]), inttof32(si->tank[i][1] + 12));
            }
            else
            {
                verdugo_unload(&ver);
                gimmick_model(&tank_model, &tank_loaded, 0, NULL, 0);
                objs.barrel = &barrel_model;
            }
            // ミッションの物：名前のアイテムが入った木箱（種類04）、爆発する樽（床の上の種類03。7面の城壁の
            // 上のものはカタパルト）、扉（01）、鐘（09）。
            if (cur_stage == 0)
                gimmicks_free();
            else
            {
                const StageFoes *sf = &stage_foes[cur_stage];
                int barrels = 0, doors = 0, bells = 0, hangs = 0;
                for (int i = 0; i < sf->things; i++)
                {
                    barrels += sf->thing[i].kind == 3;
                    doors += sf->thing[i].kind == 1;
                    bells += sf->thing[i].kind == 9;
                    hangs += sf->thing[i].kind == 5;
                }
                doors = gimmick_model(&door_model, &door_loaded, doors, door_bin, door_bin_size);
                bells = gimmick_model(&bell_model, &bell_loaded, bells, bell_bin, bell_bin_size);
                hangs = gimmick_model(&hang_model, &hang_loaded, hangs, hang_bin, hang_bin_size);
                objs.hang = &hang_model;
                if (barrels && !barrel_loaded)
                    barrel_loaded = model_load(&barrel_model, barrel_bin, barrel_bin_size) == 0;
                for (int i = 0; i < sf->things; i++)
                {
                    const StageThing *t = &sf->thing[i];
                    int x = inttof32(t->pos[0]), z = inttof32(t->pos[2]), ref = inttof32(t->pos[1] + 6);
                    if (i < (int)sizeof(thing_obj))
                        thing_obj[i] = -1;
                    if (t->kind == 4 && objects_add_crate(&objs, x, z, ref, t->drop) && i < (int)sizeof(thing_obj))
                        thing_obj[i] = objs.count - 1;   // （ミッションのイベントがいくつかに依存する：foes_events()）
                    else if (t->kind == 2)   // はしご、その奥の床まで
                        objects_add_ladder(&objs, x, z, ref, t->dir * (DEGREES_IN_CIRCLE / 256));
                    else if (t->kind == 1 && doors)   // 扉、ちょうつがいで
                        objects_add_door(&objs, x, z, ref, t->dir * (DEGREES_IN_CIRCLE / 256));
                    else if (t->kind == 5 && hangs)   // 鎖に吊るされた容器
                        objects_add_hang(&objs, x, inttof32(t->pos[1]), z, t->dir * (DEGREES_IN_CIRCLE / 256));
                    else if (t->kind == 9 && bells)   // 鐘、その地点から吊るされる
                        objects_add_bell(&objs, x, inttof32(t->pos[1]), z);
                    else if (t->kind == 3 && barrel_loaded && t->pos[1] > 20)   // （7面の城壁の上）
                        objects_add_barrel_at(&objs, x, inttof32(t->pos[1]), z);
                    else if (t->kind == 3 && barrel_loaded)
                        objects_add_barrel(&objs, x, z, ref);
                }
                // 5面のはしごはチャペルのモデルの一部で、ミッションの物ではない。ステージのデータがその下と上に
                // 印を付けている（stg_data のセクション4、地点 b3、b4）。
                if (cur_stage == 4 && objects_add_ladder(&objs, inttof32(14), inttof32(44), inttof32(6), 0))
                    objs.o[objs.count - 1].drawn = 1;
                if (!barrels && cur_stage != VER_STAGE)
                    barrel_unload();
            }
#ifdef TEST_HIGH_CRATE   // テスト用ビルド：宙に浮いた木箱、レオンの9前、13上
            if (objects_add_barrel_at(&objs, leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(9)), leon_feet + inttof32(13),
                                      leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(9))))
            {
                objs.o[objs.count - 1].kind = OBJ_CRATE;
                objs.o[objs.count - 1].drop = -1;
            }
#endif
#ifdef TEST_AUDIT   // テスト用ビルド：レオンが歩いて行ける所（audit_stage()）、それから止める
            {
                int at[3] = { leon_pos[0], leon_feet, leon_pos[2] };
                audit_stage(cur_stage, at);
                while (1)
                    swiWaitForVBlank();
            }
#endif
            // アシュリー：彼女のステージで、レオンのすぐ後ろ。ガナードは彼女も狙う。
            ash_lost = 0;
            ash.on = 0;
            missiles_init(&missiles, &spark, cur_stage >= CASTLE_FROM);
            bolts_clear();
            bonus_t[0] = bonus_t[1] = 0;
            if (cur_stage == ASH_STAGE_A || cur_stage == ASH_STAGE_B || cur_stage == ASH_STAGE_C || cur_stage == ASH_STAGE_D
                || cur_stage == ASH_STAGE_E)
            {
                if (ashley_load(&ash))
                {
                    consoleDemoInit();
                    printf("Ashley: load failed\n");
                    while (1)
                        swiWaitForVBlank();
                }
#ifdef TEST_ASH_VIEW   // テスト用ビルド：彼の前で、彼の方を向いて
                ashley_place(&ash, &stage_ground, leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(9)),
                             leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(9)), leon_feet + inttof32(4), leon_yaw + DEGREES_IN_CIRCLE / 2);
#else
                ashley_place(&ash, &stage_ground, leon_pos[0] - mulf32(sinLerp(leon_yaw), inttof32(5)) + mulf32(cosLerp(leon_yaw), inttof32(7)),
                             leon_pos[2] - mulf32(cosLerp(leon_yaw), inttof32(5)) - mulf32(sinLerp(leon_yaw), inttof32(7)), leon_feet + inttof32(4), leon_yaw);
#endif
#ifndef TEST_ASH_VIEW
                // ミッションが彼女を置く所、彼がまだそこにいなければ：彼が来るまでそこにいる。
                const StageFoes *as = &stage_foes[cur_stage];
#ifdef TEST_ASH_WITH   // テスト用ビルド：彼女は最初から彼と一緒
                if (0)
#else
                if (as->ally)
#endif
                {
                    ashley_place(&ash, &stage_ground, inttof32(as->ally_pos[0]), inttof32(as->ally_pos[2]), inttof32(as->ally_pos[1] + 2), leon_yaw);
                    ash.wait = 1;
                }
#endif
                ash.on = 1;
#ifdef TEST_ASH_HP   // テスト用ビルド：彼女は負傷した状態で始まる
                ash.hp = TEST_ASH_HP;
#endif
                ganados.ally = ash.pos;
            }
            else
                ashley_unload(&ash);
            int s = sinLerp(leon_yaw), c = cosLerp(leon_yaw);
            follow_eye[0] = leon_pos[0] - mulf32(s, CAM_BACK) - mulf32(c, CAM_RIGHT);
            follow_eye[1] = leon_feet + CAM_HEIGHT;
            follow_eye[2] = leon_pos[2] - mulf32(c, CAM_BACK) + mulf32(s, CAM_RIGHT);
            follow_target[0] = leon_pos[0] + mulf32(s, CAM_AHEAD) - mulf32(c, CAM_AT_RIGHT);
            follow_target[1] = leon_feet + CAM_AT_HEIGHT;
            follow_target[2] = leon_pos[2] + mulf32(c, CAM_AHEAD) + mulf32(s, CAM_AT_RIGHT);
            cam_yaw = leon_yaw;
            kneel_seen = gal_seen = giant_paid = 0;
            ash_waiting = ash.on && ash.wait;
            // ミッションの始まりの場面（12面のは空：ヴェルデューゴの場面がすぐに来る）。
#ifdef TEST_OPENING   // テスト用ビルド：直接ステージから始めても始まりの場面を出す
            cut_opening = 1;
#endif
            if (cut_opening && cut_start(cur_stage, cur_stage == VER_STAGE ? CUT_VERDUGO : CUT_START, MODE_PLAY, CUT_AFTER_NONE, 1))
                mode = MODE_CUTIN;
            cut_opening = 0;
        }

        // ガナードへの反応（下で、彼らの更新の後に始める）：操作を止めて最後まで再生する。
        // つかまれた：A/B/X/Y を連打して振りほどく。
        if (react != REACT_NONE)
        {
            react_frame += ANIM_STEP;
            int len = inttof32(leon_anims.frames[react_clip[react]]);
            if (react == REACT_BREAK_FREE && react_frame >= KICK_FRAME && react_frame - ANIM_STEP < KICK_FRAME)
            {
                sfx_play(SFX_BREAK_FREE, 120, 64);   // 蹴りが当たる
                // （突き放し：無傷でよろめく。enemies_release()）
            }
            if (react == REACT_HELD)
            {
                held_frames++;
                if (down & (KEY_A | KEY_B | KEY_X | KEY_Y))
                    mash++;
#ifdef TEST_MASH   // テスト用ビルド：A を連打したことにする（6フレームごと）
                if (held_frames % 6 == 0)
                    mash++;
#endif
                if (mash >= GRAB_MASH || ganados.e[holder].state != ENEMY_GRAB)
                {
                    // 振りほどいた：レオンが突き放す（CLIP_BREAK_FREE）。相手はよろめく。
                    enemies_release(&ganados, holder);
                    holder = -1;
                    react = REACT_BREAK_FREE;
                    react_frame = 0;
                }
            }
            else if (react == REACT_DEAD)
            {
                if (react_frame > len - 1)
                    react_frame = len - 1;   // 倒れたまま（終わりを過ぎたフレームは先頭に戻ってしまう）
                if (mode == MODE_PLAY && ++dead_timer >= DEAD_TO_MENU)
                    mode = MODE_GAMEOVER;
            }
            else if (react == REACT_KICK)
            {
                // 足が当たる：ふらついたガナード（とその横の者）が吹っ飛ぶ…
                if (react_frame >= KICK_HIT && react_frame - ANIM_STEP < KICK_HIT && act_obj < 0)
                {
                    if (enemies_kick(&ganados, leon_pos, leon_yaw, KICK_DAMAGE))
                        sfx_play(SFX_BREAK_FREE, 127, 64);
                    act_obj = -2;   // （着地した）
                }
                // …または門が開く / 木箱が壊れる。
                if (react_frame >= KICK_HIT && react_frame - ANIM_STEP < KICK_HIT && act_obj >= 0)
                {
                    if (objs.o[act_obj].kind == OBJ_GATE_LEAF)
                    {
                        objects_open(&objs, act_obj);
                        sfx_play(SFX_DOOR_KICK, 127, 64);
                    }
                    else if (objects_hit(&objs, act_obj))
                    {
                        sfx_play(SFX_BREAK, 120, 64);
                        crate_drop(&objs.o[act_obj], (const int[3]){ leon_pos[0], leon_feet, leon_pos[2] });
                    }
                    act_obj = -1;
                }
                if (react_frame >= len)
                    react = REACT_NONE;
            }
            else if (react == REACT_GIANT_UP)
            {
                if (react_frame >= len)
                {
                    // 背中の上：彼の頭の方を向いて、ナイフを振り下ろす。
                    leon_pos[0] += mulf32(gclimb_f[0], CLIMB_BACK);
                    leon_pos[2] += mulf32(gclimb_f[1], CLIMB_BACK);
                    leon_feet = gclimb_y + CLIMB_UP;
                    leon_pos[1] = leon_feet - leon.bbox_min[1];
                    leon_yaw = giants[gclimb].yaw;
                    react = REACT_GIANT_CUT;
                    react_frame = 0;
                    clip_key = -1;
                    bonus_note(T_BONUS_TECH);   // （触手：彼の弱点）
                    sfx_play(SFX_KNIFE, 127, 64);
                }
            }
            else if (react == REACT_GIANT_CUT)
            {
                Giant *gc = &giants[gclimb];
                down &= ~KEY_A;
                if (gcut_page >= 2)
                {
                    sfx_play_at(SFX_GANADO_DOWN, 127, gc->pos, leon_pos, leon_yaw);
                    giant_cut(gc);
                    // 彼がレオンを振り落とす：彼の後ろに降り、彼の方を向く。
                    gc->held = 0;
                    walk_out(&stage_ground, leon_pos, -gclimb_f[0], -gclimb_f[1], inttof32(4), gclimb_y);
                    {
                        int tx = gc->pos[0] - leon_pos[0], tz = gc->pos[2] - leon_pos[2];   // （着地して、彼の方を向く）
                        leon_yaw = (int)(atan2f((float)tx, (float)tz) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
                    }
                    ground_height(&stage_ground, leon_pos[0], leon_pos[2], leon_feet + STEP_UP, STEP_UP, &leon_feet);
                    leon_pos[1] = leon_feet - leon.bbox_min[1];
                    react = REACT_NONE;
                    react_frame = 0;
                    clip_key = -1;
                }
            }
            else if (react == REACT_CLIMB_UP || react == REACT_CLIMB_DOWN)
            {
                if (react_frame >= len)
                {
                    // はしごから：通路の上（登りで運ばれた所）か、その下。
                    const Obj *l = &objs.o[act_obj];
                    leon_pos[0] = l->pos[0] + (react == REACT_CLIMB_UP ? mulf32(l->dir[0], CLIMB_BACK) : 0);
                    leon_pos[2] = l->pos[2] + (react == REACT_CLIMB_UP ? mulf32(l->dir[1], CLIMB_BACK) : 0);
                    leon_feet = react == REACT_CLIMB_UP ? l->top : l->pos[1];
                    ground_height(&stage_ground, leon_pos[0], leon_pos[2], leon_feet, STEP_UP, &leon_feet);
                    leon_pos[1] = leon_feet - leon.bbox_min[1];
                    react = REACT_NONE;
                    clip_key = -1;   // 登りの最後のポーズは今立っている所：クロスフェードしない
                    if (ash.on)   // 彼女も後から登る
                    {
                        int foot[3] = { l->pos[0], l->pos[1], l->pos[2] };
                        ashley_use_ladder(&ash, foot, l->dir, l->top, l->yaw, leon_feet > l->pos[1] + inttof32(4));
                    }
                    act_obj = -1;
                }
            }
            else if (react == REACT_DROP)
            {
                // 段差から：少し前へ、それから重力で下の床へ。
                if (react_frame > len - 1)
                    react_frame = len - 1;
                drop_vy += DROP_GRAVITY;
                leon_feet -= drop_vy;
                if (react_frame < inttof32(6))
                {
                    leon_pos[0] += mulf32(sinLerp(leon_yaw), floattof32(0.25));
                    leon_pos[2] += mulf32(cosLerp(leon_yaw), floattof32(0.25));
                }
                if (leon_feet <= drop_to)
                {
                    leon_feet = drop_to;
                    react = REACT_LAND;
                    react_frame = 0;
                    if (drop_vy > floattof32(1.2))
                        sfx_play(SFX_LEON_HURT, 70, 64);   // 強い着地
                }
                leon_pos[1] = leon_feet - leon.bbox_min[1];
            }
            else if (react == REACT_LAND)
            {
                if (react_frame >= LAND_FRAMES)
                    react = REACT_NONE;
            }
            else if (react_frame >= len)
            {
                react = REACT_NONE;
                safe_timer = SAFE_FRAMES;
            }
        }
        else if (safe_timer > 0)
            safe_timer--;
        int free_to_act = react == REACT_NONE;

        int fwd_l[3] = { sinLerp(leon_yaw), 0, cosLerp(leon_yaw) };   // レオンの向き（モデルの +Z）
        int anim_dir = 1;   // -1 でアニメーションを逆再生（後ろ歩き）

        // リロード中、レオンは銃を下ろして歩ける（Zeebo 版のとおり）：R を押したままなら、弾が入ると
        // また構える。
        int aim_held = ctrl == CTRL_LEON && (held & KEY_R) && free_to_act;
        int aiming = aim_held && reload_frame < 0;
        // ナイフ（L を押したまま、Zeebo 版のナイフボタンと同じ）：レオンはナイフを構えて立ち、A で斬る
        // （足元の木箱に届く低さ）。始めた斬りは最後までやる。
        int knifing = (ctrl == CTRL_LEON && (held & KEY_L) && !aiming && free_to_act
                       && gun_state != GUN_DRAWING && gun_state != GUN_HOLSTERING) || slash_frame >= 0;
        knife_w += knifing ? AIM_RAISE : -AIM_RAISE;
        if (knife_w > inttof32(1)) knife_w = inttof32(1);
        if (knife_w < 0) knife_w = 0;
        if (knifing && free_to_act && (down & KEY_A) && slash_frame < 0)
        {
            slash_frame = 0;
            slash_high = aim_tilt > KNIFE_HIGH_TILT;
            // 刃の通り道にテーブルや棚の上の物がある（下に傾けていない）：しゃがむ斬りでなく胸の高さの斬り
            // （以前はかがんでその下を斬っていた）。
            if (!slash_high && aim_tilt > -KNIFE_HIGH_TILT)
            {
                int pitch = mulf32(aim_tilt, KNIFE_PITCH), cp = cosLerp(pitch), kt = KNIFE_REACH;
                int kdir[3] = { mulf32(sinLerp(leon_yaw), cp), sinLerp(pitch), mulf32(cosLerp(leon_yaw), cp) };
                int kfrom[3] = { leon_pos[0], leon_feet + KNIFE_CHEST, leon_pos[2] };
                int k = knife_object(&objs, kfrom, kdir, &kt);
                slash_high = k >= 0 && objs.o[k].pos[1] > leon_feet + KNIFE_RAISED;
#ifdef TEST_SHOW_KNIFE   // テスト用ビルド：FPS の欄 = 探りが当たった物（+1、0 なし）x 1000 + 足元からのその高さ x 10 + 傾きの符号
                knife_probe = (k + 1) * 1000 + (k >= 0 ? f32toint(objs.o[k].pos[1] - leon_feet) * 10 : 0) + (aim_tilt > 0) + 2 * (aim_tilt < 0);
                {   // …そして長いもの：物 x 1000 + 距離 x 10
                    int t2 = inttof32(200), k2 = objects_raycast(&objs, kfrom, kdir, &t2);
                    knife_probe = knife_probe * 100000 / 100000 + 0;
                    v_knife2 = (k2 + 1) * 1000 + (k2 >= 0 ? f32toint(t2) * 10 : 0);
                }
#endif
            }
            sfx_play(SFX_KNIFE, 120, 64);
        }
        if (slash_frame >= 0)
        {
            int len = inttof32(leon_knife.frames[slash_high ? KNIFE_HIGH : KNIFE_SLASH]), hit = len / 2;
            slash_frame += ANIM_STEP;
            if (slash_frame >= hit && slash_frame - ANIM_STEP < hit)
            {
                // 刃は視線に沿って、見ている向きの上か下へ通る（銃の狙いと同じ）：そこにある木箱、樽、鐘、メダルに
                // 当たる（高い所の宝は落ちる）。無ければ足元の木箱が壊れるか、届く範囲で一番近いガナードが斬られる。
                int which, pitch = mulf32(aim_tilt, KNIFE_PITCH), cp = cosLerp(pitch);
                int kdir[3] = { mulf32(sinLerp(leon_yaw), cp), sinLerp(pitch), mulf32(cosLerp(leon_yaw), cp) };
                int kfrom[3] = { leon_pos[0], leon_feet + KNIFE_CHEST, leon_pos[2] };
                int kt = KNIFE_REACH, kend[3];
                if (ground_raycast(&stage_ground, kfrom, kdir, KNIFE_REACH, kend, RAY_SHOT))
                {
                    s64 ex = kend[0] - kfrom[0], ey = kend[1] - kfrom[1], ez = kend[2] - kfrom[2];
                    kt = (int)sqrt64((u64)(ex * ex + ey * ey + ez * ez)) + inttof32(1);   // （壁越しではなく）
                }
                int kobj = knife_object(&objs, kfrom, kdir, &kt);
                int kdrop = drops_raycast(&drops, kfrom, kdir, &kt);
                if (kdrop >= 0)
                {
                    drops_knock(&drops, kdrop);
                    sfx_play(SFX_ITEM_GET, 90, 64);
                    hits++;
                }
                else if (kobj >= 0 && objects_hit(&objs, kobj))
                {
                    Obj *o = &objs.o[kobj];
                    hits++;
                    if (o->kind == OBJ_CRATE)
                    {
                        sfx_play_at(SFX_BREAK, 120, o->pos, leon_pos, leon_yaw);
                        crate_drop(o, (const int[3]){ leon_pos[0], leon_feet, leon_pos[2] });
                    }
                    else if (o->kind == OBJ_BELL)
                    {
                        sfx_play_at(SFX_BELL, 127, o->pos, leon_pos, leon_yaw);
                        for (int g = 0; g < gal_count; g++)
                            garrador_hear(&gals[g], o->pos, inttof32(4000));
                    }
                    else if (o->kind == OBJ_MEDAL)
                    {
                        sfx_play_at(SFX_BREAK, 110, o->pos, leon_pos, leon_yaw);
                        medal_note = 120;
                        if (objs.medals_hit == objs.medals)
                        {
                            spare += MEDAL_REWARD;
                            sfx_play(SFX_ITEM_GET, 120, 64);
                        }
                    }
                }
                else if (aim_tilt <= 0 && objects_action(&objs, leon_pos, leon_feet, leon_yaw, &which) == ACT_KICK_CRATE
                    && objects_hit(&objs, which))
                {
                    sfx_play(SFX_BREAK, 120, 64);
                    crate_drop(&objs.o[which], (const int[3]){ leon_pos[0], leon_feet, leon_pos[2] });
                    hits++;
                }
                else
                {
                    int e = enemies_slash(&ganados, leon_pos, leon_yaw, KNIFE_RANGE, KNIFE_DAMAGE);
                    if (e >= 0)
                    {
                        hits++;
                        sfx_play_at(ganados.e[e].state == ENEMY_DYING ? SFX_GANADO_DOWN : SFX_GANADO_HIT, 120, ganados.e[e].pos, leon_pos, leon_yaw);
                        if (ganados.e[e].state == ENEMY_DYING)
                            maybe_drop(&ganados.e[e], leon_hp, DROP_PERCENT);
                    }
                }
            }
            if (slash_frame >= len || !free_to_act)
                slash_frame = -1;
        }
        aim_w += aiming ? AIM_RAISE : -AIM_RAISE;
        if (aim_w > inttof32(1)) aim_w = inttof32(1);
        if (aim_w < 0) aim_w = 0;

        // X で抜き / しまい（GUN_DRAW を順に、またはしまうときは逆に）。
        // L+X：次の武器（ステータス画面の Equip も同じ）。
        if (ctrl == CTRL_LEON && (down & KEY_X) && (held & KEY_L) && !aiming && free_to_act && reload_frame < 0)
        {
            int next = (wep + 1) % WEP_COUNT;
            while (!(wep_owned >> next & 1))
                next = (next + 1) % WEP_COUNT;
            weapon_switch(next, &ammo, &spare);
            fire_frame = -1;
            clip_key = -1;
            wep_note = 120;
            sfx_play(SFX_MENU, 100, 64);
            down &= ~KEY_X;
        }
        if (ctrl == CTRL_LEON && (down & KEY_X) && !aiming && free_to_act)
        {
            if (gun_state == GUN_HOLSTERED)
            {
                gun_state = GUN_DRAWING;
                draw_frame = 0;
            }
            else if (gun_state == GUN_READY)
            {
                gun_state = GUN_HOLSTERING;
                draw_frame = inttof32(DRAW_FRAMES);
            }
        }
        if (gun_state == GUN_DRAWING && (draw_frame += ANIM_STEP) >= inttof32(DRAW_FRAMES))
            gun_state = GUN_READY;
        if (gun_state == GUN_HOLSTERING && (draw_frame -= ANIM_STEP) <= 0)
            gun_state = GUN_HOLSTERED;
        int drawing = gun_state == GUN_DRAWING || gun_state == GUN_HOLSTERING;

        // 撃つ（A、完全に構えて）とリロード（銃を出して Y、または空で撃つ）。
        int gun_out = gun_state == GUN_READY || aim_held;
        if (ctrl == CTRL_LEON && reload_frame < 0 && !drawing && free_to_act)
        {
            if ((down & KEY_Y) && gun_out && ammo < MAG_SIZE && spare > 0 && USES_ROUNDS(wep))
            {
                reload_frame = 0;
                reload_ch = sfx_play(SFX_RELOAD, 110, 64);
            }
            else if (((down & KEY_A) || (wep == WEP_TMP && (held & KEY_A))) && aiming && aim_w == inttof32(1)
                     && (fire_frame < 0 || fire_frame >= wep_refire(wep)))
            {
                if (ammo == 0)
                {
                    sfx_play(SFX_EMPTY, 110, 64);   // そしてリロード、込めるものがあれば
                    if (spare > 0 && USES_ROUNDS(wep))
                    {
                        reload_frame = 0;
                        reload_ch = sfx_play(SFX_RELOAD, 110, 64);
                    }
                }
                else
                {
                    sfx_play(SFX_SHOT, 127, 64);
                    ammo--;
                    shots++;
                    for (int i = 0; i < gal_count; i++)
                        garrador_hear(&gals[i], leon_pos, inttof32(4000));   // 射撃：どこにいても聞こえる
                    shot_noise = 1;   // （ガナードは探す距離の中で聞こえる）
                    fire_frame = 0;
                    flash_timer = 3;
                    cam_kick = floattof32(1.2);
                    if (laser_hit)
                    {
                        for (int k = 0; k < 3; k++)
                            impact_pos[k] = laser_end[k];
                        impact_timer = 12;
                        impact_blood = laser_enemy >= 0;
                    }
                    if (weapons[wep].special == WS_ROCKET)
                    {
                        // ロケット、照準の指す所へ飛ぶ。そこで破裂する（下を参照）。
                        for (int k = 0; k < 3; k++)
                        {
                            rocket_pos[k] = muzzle[k];
                            rocket_to[k] = laser_end[k];
                        }
                        rocket_on = 1;
                        sfx_play(SFX_EXPLODE, 70, 64);
                        laser_enemy = laser_obj = laser_drop = -1;
                        laser_ash = laser_giant = laser_gal = laser_ver = 0;
                        impact_timer = 0;
                    }
                    if (wep == WEP_SHOTGUN)
                    {
                        // その弾（item_weapon_info.bin：1発に5つ。それぞれ近距離の威力120、遠距離80、6 m から先が遠距離）：
                        // それぞれ狙いのまわりの自分の向きに飛び（原作は銃のモデルのノードから向きを取る、
                        // GM_Obj_Eqp_Weapon::Get_ShotGunVec。ここでは固定の広がり、真ん中1つと4-5度ずれた4つ）、
                        // SHOTGUN_RANGE まで、体から3単位以内を通る一番近いガナードか教徒に当たる。
                        static const s8 spread[5][2] = { { 0, 0 }, { 5, 0 }, { -5, 0 }, { 0, 4 }, { 0, -4 } };
                        int ax = laser_end[0] - leon_pos[0], ay = laser_end[1] - (leon_feet + inttof32(12)), az = laser_end[2] - leon_pos[2];
                        (void)ax; (void)ay; (void)az;   // （照準の先は彼の足元の地面のこともある）
                        int aim_yaw = leon_yaw;
                        if (laser_enemy >= 0)   // （照準が合っている者：広がりは彼を中心に）
                            aim_yaw = (int)(atan2f((float)(ganados.e[laser_enemy].pos[0] - leon_pos[0]), (float)(ganados.e[laser_enemy].pos[2] - leon_pos[2])) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
                        int aim_pitch = mulf32(aim_tilt, degreesToAngle(40));   // （狙いの傾き：だいたい構えのポーズが届く所まで）
                        int pellets_hit = 0;
                        for (int k = 0; k < 5; k++)
                        {
                            int yw = aim_yaw + degreesToAngle(spread[k][0]), pt = aim_pitch + degreesToAngle(spread[k][1]);
                            int dir[3] = { mulf32(sinLerp(yw), cosLerp(pt)), sinLerp(pt), mulf32(cosLerp(yw), cosLerp(pt)) };
                            int best = -1, best_t = SHOTGUN_RANGE;
                            for (int i = 0; i < ganados.count; i++)
                            {
                                const Enemy *e = &ganados.e[i];
                                if (!e->active || e->state >= ENEMY_DYING)
                                    continue;
                                int v[3] = { e->pos[0] - leon_pos[0], e->feet + inttof32(10) - (leon_feet + inttof32(12)), e->pos[2] - leon_pos[2] };   // （彼の胸から：銃口の所にいる者にも当たる）
                                int tt = mulf32(v[0], dir[0]) + mulf32(v[1], dir[1]) + mulf32(v[2], dir[2]);
                                if (tt <= 0 || tt >= best_t)
                                    continue;
                                s64 off2 = 0;
                                for (int c = 0; c < 3; c++)
                                {
                                    s64 o = v[c] - mulf32(dir[c], tt);
                                    off2 += o * o;
                                }
                                s64 r = floattof32(3.0), hh = inttof32(9);   // （彼の体：左右に3、胸から上下に9）
                                int up = v[1] - mulf32(dir[1], tt);
                                if (off2 - (s64)up * up > r * r || up > hh || up < -hh)
                                    continue;
                                best_t = tt;
                                best = i;
                            }
                            if (best < 0)
                                continue;
                            enemies_shot(&ganados, best, best_t < inttof32(60) ? 120 : 80, PART_BODY, 0, CRIT_NONE);
                            pellets_hit++;
#ifdef TEST_SHOW_HP
                            test_shot_foe = best;
                            test_shots++;
#endif
                            if (ganados.e[best].state == ENEMY_DYING)
                                maybe_drop(&ganados.e[best], leon_hp, DROP_PERCENT);
                        }
                        hits += pellets_hit > 0;
                    }
                    if (laser_enemy >= 0 && wep != WEP_SHOTGUN)
                    {
                        int staggered = enemies_shot(&ganados, laser_enemy, wep_power(), laser_part, wep_crit_rate(), wep_crit_kind());
#ifdef TEST_SHOW_HP   // テスト用ビルド：バイタル画面に最後に撃った者を出す（下を参照）
                        test_shot_foe = laser_enemy;
                        test_shots++;
                        test_staggers += staggered && ganados.e[laser_enemy].state == ENEMY_HIT;
                        test_downs += ganados.e[laser_enemy].state == ENEMY_DYING;
#else
                        (void)staggered;
#endif
#ifdef TEST_DAZE   // テスト用ビルド：当たるたびにふらつく
                        laser_head = 1;
#endif
                        if (laser_head)
                            enemies_daze(&ganados, laser_enemy);   // よろめく：蹴れる
                        // （原作のボーナス表示：クリティカル。プラーガへの当たり、弱点）
                        if (ganados.crit_hit)
                            bonus_note(T_BONUS_CRIT);
                        else if (laser_head && ganados.e[laser_enemy].plaga)
                            bonus_note(T_BONUS_TECH);
                        ganados.crit_hit = 0;
                        hits++;
                        headshots += laser_head;
                        const Enemy *e = &ganados.e[laser_enemy];
                        sfx_play_at(e->state == ENEMY_DYING ? SFX_GANADO_DOWN : SFX_GANADO_HIT, 120, e->pos, leon_pos, leon_yaw);
                        if (e->state == ENEMY_DYING)
                            maybe_drop(e, leon_hp, DROP_PERCENT);
                        // 貫通：銃の ThroughRate は1発でさらに何人に当たれるか（GM_Obj_Eqp_Weapon::Set_TrgHitMax。
                        // マグナム2、ほかは0）。弾の線上の後ろの者、近い順に、胴に。
                        int through = wep == WEP_MAGNUM ? 2 : 0;
                        int dir[3], len2 = 0;
                        for (int k = 0; k < 3; k++)
                            dir[k] = laser_end[k] - muzzle[k];
                        len2 = f32toint(dir[0]) * f32toint(dir[0]) + f32toint(dir[2]) * f32toint(dir[2]);
                        int last = laser_enemy;
                        while (through-- > 0 && len2 > 0)
                        {
                            int best = -1, best_t = 0x7fffffff;
                            for (int i = 0; i < ganados.count; i++)
                            {
                                const Enemy *o = &ganados.e[i];
                                if (i == laser_enemy || i == last || !o->active || o->state >= ENEMY_DYING)
                                    continue;
                                int ox = f32toint(o->pos[0] - muzzle[0]), oz = f32toint(o->pos[2] - muzzle[2]);
                                int lt = ox * f32toint(dir[0]) + oz * f32toint(dir[2]);   // （線に沿って、x len）
                                int lx = f32toint(ganados.e[last].pos[0] - muzzle[0]), lz = f32toint(ganados.e[last].pos[2] - muzzle[2]);
                                if (lt <= lx * f32toint(dir[0]) + lz * f32toint(dir[2]) || lt > len2 * 6)
                                    continue;   // （最後に当たった者の後ろではない。弾の届く範囲の外）
                                s64 cross = (s64)ox * f32toint(dir[2]) - (s64)oz * f32toint(dir[0]);
                                if (cross * cross > (s64)3 * 3 * len2)
                                    continue;   // （線から3単位より離れている）
                                if (lt < best_t)
                                {
                                    best_t = lt;
                                    best = i;
                                }
                            }
                            if (best < 0)
                                break;
                            enemies_shot(&ganados, best, wep_power(), PART_BODY, 0, CRIT_NONE);
                            hits++;
                            if (ganados.e[best].state == ENEMY_DYING)
                                maybe_drop(&ganados.e[best], leon_hp, DROP_PERCENT);
                            last = best;
                        }
                    }
                    else if (laser_ver)
                    {
                        int was_frozen = ver.state == VER_FROZEN;
                        verdugo_shot(&ver, wep_power());
                        hits++;
                        headshots += was_frozen;
                        impact_blood = !was_frozen;
                        sfx_play_at(ver.state == VER_DEAD ? SFX_GANADO_DOWN : SFX_GANADO_HIT, 110, ver.pos, leon_pos, leon_yaw);
                    }
                    else if (laser_gal)
                    {
                        Garrador *gh = &gals[laser_gal - 1];
                        garrador_shot(gh, wep_power(), laser_gal_back);
                        if (laser_gal_back)
                            bonus_note(T_BONUS_TECH);   // （背中の寄生体：弱点）
                        hits++;
                        headshots += laser_gal_back;
                        impact_blood = 1;
                        sfx_play_at(gh->state == GAL_DEAD ? SFX_GANADO_DOWN : SFX_GANADO_HIT, 110, gh->pos, leon_pos, leon_yaw);
                    }
                    else if (laser_giant)
                    {
                        Giant *gh = &giants[laser_giant - 1];
                        giant_shot(gh, wep_power());
                        hits++;
                        impact_blood = 1;
                        sfx_play_at(SFX_GANADO_HIT, 100, gh->pos, leon_pos, leon_yaw);
                    }
                    else if (laser_ash)
                    {
                        ashley_hit(&ash, ASH_SHOT_DAMAGE);   // 彼女を撃った
                        sfx_play_at(SFX_LEON_HURT2, 110, ash.pos, leon_pos, leon_yaw);
                    }
                    else if (laser_drop >= 0 && laser_obj < 0)
                    {
                        drops_knock(&drops, laser_drop);   // 高い所の宝：落ちてくる
                        impact_blood = 0;
                        sfx_play(SFX_ITEM_GET, 90, 64);
                    }
                    else if (laser_obj >= 0)
                    {
                        // 木箱は壊れ（アイテムが出ることも）、樽は火がつき、メダルは砕ける。
                        Obj *o = &objs.o[laser_obj];
                        impact_blood = 0;
                        if (objects_hit(&objs, laser_obj))
                        {
                            hits++;
                            if (o->kind == OBJ_CRATE)
                            {
                                sfx_play_at(SFX_BREAK, 120, o->pos, leon_pos, leon_yaw);
                                crate_drop(o, (const int[3]){ leon_pos[0], leon_feet, leon_pos[2] });
                            }
                            else if (o->kind == OBJ_BELL)
                            {
                                // 鐘：ガラドールがどこにいても聞こえる大きな音
                                // （8面の注記：「if you strike it, it makes a loud noise」）。
                                sfx_play_at(SFX_BELL, 127, o->pos, leon_pos, leon_yaw);
                                for (int g = 0; g < gal_count; g++)
                                    garrador_hear(&gals[g], o->pos, inttof32(4000));
                            }
                            else if (o->kind == OBJ_MEDAL)
                            {
                                sfx_play_at(SFX_BREAK, 110, o->pos, leon_pos, leon_yaw);
                                medal_note = 120;
                                if (objs.medals_hit == objs.medals)
                                {
                                    spare += MEDAL_REWARD;
                                    sfx_play(SFX_ITEM_GET, 120, 64);
                                }
                            }
                            else
                                sfx_play_at(SFX_WALL, 70, laser_end, leon_pos, leon_yaw);
                        }
                    }
                    else if (laser_hit)
                        sfx_play_at(SFX_WALL, 70, laser_end, leon_pos, leon_yaw);
                }
            }
        }
        if (fire_frame >= 0 && (fire_frame += ANIM_STEP) >= inttof32(FIRE_FRAMES))
            fire_frame = -1;
        if (reload_frame >= 0 && (reload_frame += mulf32(ANIM_STEP, wep_reload_speed(wep))) >= inttof32(RELOAD_FRAMES))
        {
            reload_frame = -1;
            int load = MAG_SIZE - ammo < spare ? MAG_SIZE - ammo : spare;
            ammo += load;
            spare -= load;
        }
        if (reload_frame >= 0 && (!gun_out || !free_to_act))
        {
            reload_frame = -1;   // 構えをやめた / 銃をしまった / 傷を負った：リロードは中止
            sfx_stop(reload_ch);
        }
        if (reload_frame < 0)
            reload_ch = -1;

        if (ctrl == CTRL_LEON && ((drawing && !aiming) || !free_to_act))
        {
            // 抜き / しまい中、または攻撃への反応中：じっと立つ。
        }
        else if (ctrl == CTRL_LEON && (aiming || knifing))
        {
            // 構え：じっと立ち、ゆっくり向きを変え、上 / 下で狙いを傾ける。
            qturn = 0;
            if (held & KEY_LEFT)  leon_yaw += AIM_TURN;
            if (held & KEY_RIGHT) leon_yaw -= AIM_TURN;
            if (held & KEY_UP)    aim_tilt += AIM_TILT;
            if (held & KEY_DOWN)  aim_tilt -= AIM_TILT;
            if (aim_tilt > inttof32(1))  aim_tilt = inttof32(1);
            if (aim_tilt < -inttof32(1)) aim_tilt = -inttof32(1);
            fwd_l[0] = sinLerp(leon_yaw);
            fwd_l[2] = cosLerp(leon_yaw);
            if (leon_anim != ANIM_IDLE)
            {
                leon_anim = ANIM_IDLE;
                anim_frame = 0;
            }
        }
        else if (ctrl == CTRL_LEON)
        {
            if (!aim_w)
                aim_tilt = 0;   // 次の構えは水平から

            // ラジコン操作：左 / 右で向きを変え、向いている方へ動く。
            // 下と B：クイックターン（半回転、歩きのクリップ数歩分）。2つを一緒に押したときだけ
            // （QTURN_PAIR フレーム以内）：後ろ歩きの途中の B や、B を押したままの下では回らない。
            static int down_t, b_t;   // それぞれを押しているフレーム数
            down_t = (held & KEY_DOWN) ? down_t + 1 : 0;
            b_t = (held & KEY_B) ? b_t + 1 : 0;
            if (qturn == 0 && (held & KEY_DOWN) && (held & KEY_B) && ((down & KEY_B) || (down & KEY_DOWN))
                && down_t <= QTURN_PAIR && b_t <= QTURN_PAIR)
                qturn = QTURN_FRAMES;
            if (qturn == 0)
            {
                if (held & KEY_LEFT)  leon_yaw += TURN_SPEED;
                if (held & KEY_RIGHT) leon_yaw -= TURN_SPEED;
            }
            else
                leon_yaw += (DEGREES_IN_CIRCLE / 2) / QTURN_FRAMES;
            fwd_l[0] = sinLerp(leon_yaw);
            fwd_l[2] = cosLerp(leon_yaw);

            int hurt = leon_hp < LEON_HURT_BELOW;
            int speed = 0, want = ANIM_IDLE;
            if (qturn > 0)
            {
                want = ANIM_WALK;
                speed = QTURN_FRAMES - qturn >= QTURN_MOVE_FROM ? LEON_MOVE(200) : 0;
                qturn--;
            }
            else if (held & KEY_UP)
            {
                want = (held & KEY_B) ? ANIM_RUN : ANIM_WALK;
                speed = want == ANIM_RUN ? (hurt ? LEON_MOVE(600) : RUN_SPEED) : (hurt ? LEON_MOVE(280) : WALK_SPEED);
            }
            else if (held & KEY_DOWN)
            {
                want = ANIM_WALK;
                speed = -(hurt ? LEON_MOVE(380) : BACK_SPEED);
                anim_dir = -1;
            }
            if (want != leon_anim)
            {
                leon_anim = want;
                anim_frame = 0;
            }

            // A（構えていない）：その場の動作：ふらついたガナードを蹴る、木箱を蹴る、はしごを登る。
            // ひざをついた巨人の背中で：A で寄生体を斬る（ナイフの斬りで）。
            if (jump_now && mode == MODE_PLAY)
            {
                // 向こうへ：向こう側で、前を向いて。アシュリーも一緒に、すぐ後ろ。
                leon_pos[0] = inttof32(jump_now->to_x);
                leon_pos[2] = inttof32(jump_now->to_z);
                leon_feet = inttof32(jump_now->to_y);
                ground_height(&stage_ground, leon_pos[0], leon_pos[2], leon_feet + STEP_UP, STEP_UP * 2, &leon_feet);
                leon_pos[1] = leon_feet - leon.bbox_min[1];
                leon_yaw = degreesToAngle(jump_now->to_yaw);
                if (ash.on && !ash.wait)
                    ashley_place(&ash, &stage_ground, leon_pos[0] - mulf32(sinLerp(leon_yaw), inttof32(6)), leon_pos[2] - mulf32(cosLerp(leon_yaw), inttof32(6)),
                                 leon_feet + inttof32(4), leon_yaw);
                clip_key = -1;
                jump_now = NULL;
            }
            const JumpSpot *js = (down & KEY_A) && react == REACT_NONE ? jump_at_hand(cur_stage, leon_pos, leon_feet, leon_yaw) : NULL;
            if (js)
            {
                // まずその場面、あれば（着地は終わるまで待つ）。
                jump_now = js;
                if (cut_start(cur_stage, CUT_CHANDELIER, MODE_PLAY, CUT_AFTER_JUMP, 0))
                    mode = MODE_CUTIN;
                sfx_play(SFX_MENU, 100, 64);
                speed = 0;
                down &= ~KEY_A;
            }
            Giant *gc = (down & KEY_A) ? giant_at_hand(leon_pos) : NULL;
            if (gc)
            {
                // 彼の背中へ、彼と反対を向いて：登りでレオンが後ろへ運ばれ背中に乗る。
                int fx = sinLerp(gc->yaw), fz = cosLerp(gc->yaw);
                gclimb = gc - giants;
                gclimb_f[0] = fx;
                gclimb_f[1] = fz;
                leon_pos[0] = gc->pos[0] - mulf32(fx, GIANT_BACK);
                leon_pos[2] = gc->pos[2] - mulf32(fz, GIANT_BACK);
                walk_out(&stage_ground, leon_pos, -fx, -fz, CLIMB_BACK, gc->feet);   // （壁越しではなく）
                leon_feet = gc->feet;
                ground_height(&stage_ground, leon_pos[0], leon_pos[2], gc->feet + STEP_UP, STEP_UP, &leon_feet);
                leon_pos[1] = leon_feet - leon.bbox_min[1];
                gclimb_y = leon_feet;
                leon_yaw = gc->yaw;
                react = REACT_GIANT_CUT;
                react_frame = 0;
                gcut_page = 0;
                if (cut_start(cur_stage, CUT_GIANT_UP, MODE_PLAY, CUT_AFTER_GIANT, 0))
                    mode = MODE_CUTIN;
                else
                    gcut_page = 2;
                sfx_play(SFX_MENU, 100, 64);
                gun_state = GUN_HOLSTERED;
                clip_key = -1;
                slash_frame = -1;
                gc->held = 1;
                speed = 0;
                down &= ~KEY_A;
            }
            int dazed = (down & KEY_A) ? enemies_kickable(&ganados, leon_pos, leon_yaw) : -1;
            if ((down & KEY_A) && dazed < 0)
            {
                // 届く範囲のアイテム：A で拾う（原作のアクションボタン、GM_CtrlMain_Player::_Check_GetEvent）。
                // そこにある扉や木箱より先に。
                int at[3] = { leon_pos[0], leon_feet, leon_pos[2] };
                if (cur_stage != 0 && ev_button >= 0)   // （ここにミッションのイベント：foes_events()）
                {
                    ev_press = 1;
                    // はしごではそのイベントが登りの場面（5面のチャペル：下がイベント地点、「Leon sets up the
                    // ladder」、毎回）：それを再生し、彼が登る。
                    int lw, la = objects_action(&objs, leon_pos, leon_feet, leon_yaw, &lw);
                    if (la != ACT_LADDER_UP && la != ACT_LADDER_DOWN)
                        down &= ~KEY_A;
                }
                else if (drops_in_reach(&drops, at) >= 0)
                {
                    take_item = 1;
                    down &= ~KEY_A;
                }
            }
            int which = -1, act = (down & KEY_A) && dazed < 0 ? objects_action(&objs, leon_pos, leon_feet, leon_yaw, &which) : ACT_NONE;
            if (dazed >= 0)
            {
                // それの方を向き、それから蹴り（KICK_HIT で当たる）。
                int dx = ganados.e[dazed].pos[0] - leon_pos[0], dz = ganados.e[dazed].pos[2] - leon_pos[2];
                leon_yaw = (int)(atan2f((float)dx, (float)dz) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
                react = REACT_KICK;
                react_frame = 0;
                act_obj = -1;
                speed = 0;
            }
            else if (act == ACT_KICK_CRATE)   // （門は鍵がかかったまま）
            {
                react = REACT_KICK;
                react_frame = 0;
                act_obj = which;
                speed = 0;
            }
            else if (act == ACT_OPEN_DOOR)   // （ミッションの扉：押し開ける）
                objects_open_door(&objs, which, leon_pos);
            else if (act == ACT_LADDER_UP || act == ACT_LADDER_DOWN)
            {
                // はしごの線上で、通路と反対を向いて：登り（#3）で後ろへ運ばれ通路に乗る。降りるときは通路から
                // 逆再生する。
                const Obj *l = &objs.o[which];
                react = act == ACT_LADDER_UP ? REACT_CLIMB_UP : REACT_CLIMB_DOWN;
                react_frame = 0;
                act_obj = which;
                leon_pos[0] = l->pos[0];
                leon_pos[2] = l->pos[2];
                leon_feet = l->pos[1];
                leon_pos[1] = leon_feet - leon.bbox_min[1];
                leon_yaw = l->yaw;
                gun_state = GUN_HOLSTERED;   // 両手を横木に
                clip_key = -1;               // そのまま登りへ（ほかからクロスフェードしない）
                speed = 0;
            }

            if (speed)
            {
                int from_x = leon_pos[0], from_z = leon_pos[2];
                leon_pos[0] += mulf32(fwd_l[0], speed);
                leon_pos[2] += mulf32(fwd_l[2], speed);
                // ガナード、閉じた門、木箱、樽は通れない。最後に壁が決める。
                enemies_push(&ganados, &leon_pos[0], &leon_pos[2], LEON_RADIUS);
                if (ash.on)
                    ashley_push(&ash, &leon_pos[0], &leon_pos[2], LEON_RADIUS);
                for (int i = 0; i < giant_count; i++)
                    giant_push(&giants[i], &leon_pos[0], &leon_pos[2], LEON_RADIUS);
                for (int i = 0; i < gal_count; i++)
                    garrador_push(&gals[i], &leon_pos[0], &leon_pos[2], LEON_RADIUS);
                if (ver.on)
                    verdugo_push(&ver, &leon_pos[0], &leon_pos[2], LEON_RADIUS);
                objects_collide(&objs, &leon_pos[0], &leon_pos[2], leon_feet, LEON_RADIUS);
                // 段差より高く彼の体に届く壁が彼を押し戻す。
                ground_collide(&stage_ground, &leon_pos[0], &leon_pos[2],
                                           leon_feet + STEP_UP, leon_feet + LEON_HEIGHT, LEON_RADIUS);
                // 床に沿う。床が無い所（穴、マップの端）では最後の高さを保つ。
                // ずっと下の床：段差から歩き出たので、落ちる。
                int floor = leon_feet;
                ground_height(&stage_ground, leon_pos[0], leon_pos[2], leon_feet, STEP_UP, &floor);
                if (floor < leon_feet - DROP_MIN)
                {
                    // 歩いて出たのでなく押し出された？狭い橋（7面の大砲への橋）の群れが、彼を細い手すりの向こうまで
                    // 押し、壁がその反対側へ押し出すことがあった。そのときは彼自身の1歩だけにする。
                    int wx = from_x + mulf32(fwd_l[0], speed), wz = from_z + mulf32(fwd_l[2], speed), wfloor = leon_feet;
                    ground_collide(&stage_ground, &wx, &wz, leon_feet + STEP_UP, leon_feet + LEON_HEIGHT, LEON_RADIUS);
                    ground_height(&stage_ground, wx, wz, leon_feet, STEP_UP, &wfloor);
                    if (wfloor >= leon_feet - DROP_MIN)
                    {
                        leon_pos[0] = wx;
                        leon_pos[2] = wz;
                        floor = wfloor;
                    }
                }
                // 10面：バルコニーの下のホールからは上に戻る道が無い（アシュリーが開ける扉へ降りる原作の階段は
                // 当たり判定に無い）ので、落ちてそこで詰まらないように端で止める。
                if (floor < leon_feet - DROP_MIN && cur_stage == ASH_STAGE_E)
                {
                    leon_pos[0] = from_x;
                    leon_pos[2] = from_z;
                    floor = leon_feet;
                }
                if (floor < leon_feet - DROP_MIN)
                {
                    react = REACT_DROP;
                    react_frame = 0;
                    drop_to = floor;
                    drop_vy = 0;
                }
                else
                    leon_feet = floor;
                leon_pos[1] = leon_feet - leon.bbox_min[1];
            }
        }
        else
        {
            if (down & KEY_START)
            {
                leon_anim = (leon_anim + 1) % 4;
                anim_frame = 0;
            }
            if (leon_anim == ANIM_WALK || leon_anim == ANIM_RUN)
            {
                // 円を回り、進む方を向く（上から見て反時計回り）。
                int speed = leon_anim == ANIM_WALK ? WALK_SPEED : RUN_SPEED;
                leon_angle += (int)(((s64)speed * DEGREES_IN_CIRCLE) / (2 * 3.14159265 * circle_radius));
                leon_pos[0] = circle_x + mulf32(sinLerp(leon_angle), circle_radius);
                leon_pos[2] = circle_z + mulf32(cosLerp(leon_angle), circle_radius);
                leon_yaw = leon_angle + DEGREES_IN_CIRCLE / 4;
                ground_height(&stage_ground, leon_pos[0], leon_pos[2], leon_feet, STEP_UP, &leon_feet);
                leon_pos[1] = leon_feet - leon.bbox_min[1];
            }
        }

        // カメラ
        int look_eye[3], look_at[3];
        if (ctrl == CTRL_LEON)
        {
            // レオンの右肩越しに、彼の先を見る（上の CAM_* を参照）。構えると寄って、傾きに従う。
            // 視点は決まった距離で彼の後から回る（cam_yaw が彼の向きへゆっくり近づく）：以前は視点と注視点を
            // 地面の上でゆっくり動かしていて、彼が回ると弧の内側を通り（後ろにいるほどひどく）、後退して
            // 回ると視点が振れた。
            {
                s16 dy = (s16)(leon_yaw - cam_yaw);
                cam_yaw += (aim_w > 0 || knife_w > 0) ? dy / 2 : dy / 8;   // （構え中はぴったりついて行く）
            }
            int fwd_c[3] = { sinLerp(cam_yaw), 0, cosLerp(cam_yaw) };
            int right[3] = { -fwd_c[2], 0, fwd_c[0] };   // カメラの右
            // 銃では視点が右肩越しに寄る。ナイフ（原作の動画のとおり、13:12 の木箱を斬る所）は彼の真後ろ、
            // 頭の上から、彼が斬るものを見下ろす：彼は視界の真ん中に立つ。
            int knife_view = knife_w > aim_w;
            int cam_w = knife_view ? knife_w : aim_w;
            int to_back = knife_view ? KNIFE_CAM_BACK : CAM_AIM_BACK, to_side = knife_view ? KNIFE_CAM_RIGHT : CAM_AIM_RIGHT;
            int to_height = knife_view ? KNIFE_CAM_HEIGHT : CAM_AIM_HEIGHT;
            int back = CAM_BACK + mulf32(to_back - CAM_BACK, cam_w);
            int side = CAM_RIGHT + mulf32(to_side - CAM_RIGHT, cam_w);
            int side_at = CAM_AT_RIGHT + mulf32(to_side - CAM_AT_RIGHT, cam_w);
            int want_eye[3], want_at[3];
            for (int k = 0; k < 3; k += 2)
            {
                want_eye[k] = leon_pos[k] - mulf32(fwd_c[k], back) + mulf32(right[k], side);
                want_at[k] = leon_pos[k] + mulf32(fwd_c[k], CAM_AHEAD) + mulf32(right[k], side_at);
            }
            want_eye[1] = leon_feet + CAM_HEIGHT + mulf32(to_height - CAM_HEIGHT, cam_w);
            want_at[1] = leon_feet + CAM_AT_HEIGHT + mulf32(mulf32(CAM_AIM_TILT, aim_tilt), cam_w);
            if (knife_view)
                want_at[1] += mulf32(KNIFE_CAM_AT - CAM_AT_HEIGHT, knife_w);   // （見下ろす）
            if (knife_w > aim_w && aim_tilt > 0)
            {
                // ナイフで見上げる：視点は彼の後ろで下がり、傾きを抑えるので、のけぞっても彼が映る
                // （以前は頭の上越しに見ていて、彼は画面の下の外だった）。
                want_eye[1] -= mulf32(mulf32(KNIFE_CAM_DROP, aim_tilt), knife_w);
                want_at[1] -= mulf32(mulf32(CAM_AIM_TILT / 2, aim_tilt), knife_w);
            }
            // 射撃で視点が少し跳ね上がる。追従のなめらかさで戻る。
            want_at[1] += cam_kick;
            cam_kick = cam_kick * 3 / 5;
#ifdef CAM_TEST_SIDE
            // テスト用ビルド：代わりにレオンを右横から見る。
            for (int k = 0; k < 3; k += 2)
            {
                want_eye[k] = leon_pos[k] + right[k] * 16 + fwd_l[k] * 4;
                want_at[k] = leon_pos[k] + fwd_l[k] * 4;
            }
            want_eye[1] = leon_feet + inttof32(12);
            want_at[1] = leon_feet + inttof32(11);
#endif
#ifdef CAM_TEST_FRONT
            // テスト用ビルド：近くで、彼の前から（少し左）。
            for (int k = 0; k < 3; k += 2)
            {
                want_eye[k] = leon_pos[k] + fwd_l[k] * 11 - right[k] * 4;
                want_at[k] = leon_pos[k];
            }
            want_eye[1] = leon_feet + inttof32(13);
            want_at[1] = leon_feet + inttof32(10);
#endif
            // はしごの上：彼は壁と反対を向き、体は登り（降り）に合わせて上がるが、位置は下のまま。視点は
            // 開いている側から、少し右から体を追い、彼と壁を見る。
            int on_giant = react == REACT_GIANT_UP;
            int climbing = ((react == REACT_CLIMB_UP || react == REACT_CLIMB_DOWN) && act_obj >= 0) || on_giant;
            if (climbing)
            {
                // はしごの下、入る向き、その上。または巨人のもの：レオンが登り始める所。
                int foot[3], in[2], top;
                if (on_giant)
                {
                    int back = react == REACT_GIANT_CUT ? CLIMB_BACK : 0;   // （上では彼の位置が寄っている）
                    foot[0] = leon_pos[0] - mulf32(gclimb_f[0], back);
                    foot[1] = gclimb_y;
                    foot[2] = leon_pos[2] - mulf32(gclimb_f[1], back);
                    in[0] = gclimb_f[0];
                    in[1] = gclimb_f[1];
                    top = gclimb_y + CLIMB_UP;
                }
                else
                {
                    const Obj *l = &objs.o[act_obj];
                    foot[0] = l->pos[0];
                    foot[1] = l->pos[1];
                    foot[2] = l->pos[2];
                    in[0] = l->dir[0];
                    in[1] = l->dir[1];
                    top = l->top;
                }
                int len = inttof32(leon_anims.frames[react_clip[react]]);
                int t = react == REACT_GIANT_CUT ? inttof32(1) : len > 0 ? divf32(react_frame, len) : inttof32(1);
                t = t > inttof32(1) ? inttof32(1) : t;
                if (react == REACT_CLIMB_DOWN)
                    t = inttof32(1) - t;
                int body_y = foot[1] + mulf32(top - foot[1], t);
                int out[2] = { -in[0], -in[1] };              // 壁（巨人）から離れて
                int sx = -out[1], sz = out[0];                 // 彼の右、外を向いて
                for (int k = 0; k < 2; k++)
                {
                    int base = (k ? foot[2] : foot[0]) + mulf32(k ? in[1] : in[0], mulf32(CLIMB_BACK, t) / 2);
                    want_eye[k * 2] = base + mulf32(out[k], CAM_CLIMB_OUT) + mulf32(k ? sz : sx, CAM_CLIMB_SIDE);
                    want_at[k * 2] = base;
                }
                want_eye[1] = body_y + CAM_CLIMB_HEIGHT;
                want_at[1] = body_y + CAM_CLIMB_AT;
                if (react_frame <= ANIM_STEP)   // （そこへは切り替え：滑って行くと彼を通り抜けてしまう）
                    for (int k = 0; k < 3; k++)
                    {
                        follow_eye[k] = want_eye[k];
                        follow_target[k] = want_at[k];
                    }
            }
            if (climbing)
                approach(follow_eye, want_eye);
            else
            {
                // （地面に沿ってはあるべき所に。上下はまだゆっくり近づく）
                follow_eye[0] = want_eye[0];
                follow_eye[2] = want_eye[2];
                follow_eye[1] += (want_eye[1] - follow_eye[1]) >> 3;
            }
            if (climbing)
                approach(follow_target, want_at);
            else
            {
                follow_target[0] = want_at[0];
                follow_target[2] = want_at[2];
                follow_target[1] += (want_at[1] - follow_target[1]) >> 3;
            }
            // 壁：カメラは壁のレオンの側にとどまる（cam_clear()）。壁がカメラを近くに押すと、空きの多い側へ
            // 彼のまわりを回り（cam_swing、ゆっくり）、彼の後ろに空きができたら戻る。
            {
                static int cam_swing;   // （libnds の角度の単位）
                int head[3] = { leon_pos[0], follow_eye[1], leon_pos[2] };
                if (!climbing)
                {
                    int off[2] = { follow_eye[0] - head[0], follow_eye[2] - head[2] };
                    int e0[3] = { follow_eye[0], follow_eye[1], follow_eye[2] };
                    int best = cam_clear(head, e0, leon_feet), want_swing = 0;
                    if (best < CAM_NEAR && cam_w == 0)   // （構え中は無し：視点は狙いのもの）
                    {
                        static const s8 tries[] = { 30, -30, CAM_SWING_MAX, -CAM_SWING_MAX };
                        for (unsigned k = 0; k < sizeof(tries); k++)
                        {
                            int a = degreesToAngle(tries[k]), s = sinLerp(a), c = cosLerp(a);
                            int e[3] = { head[0] + mulf32(off[0], c) - mulf32(off[1], s), follow_eye[1], head[2] + mulf32(off[0], s) + mulf32(off[1], c) };
                            int d = cam_clear(head, e, leon_feet);
                            if (d > best + inttof32(2))
                            {
                                best = d;
                                want_swing = a;
                            }
                        }
                    }
                    int step = (want_swing - cam_swing) / 8;
                    if (step == 0 || (step < degreesToAngle(CAM_SWING_EASE) && step > -degreesToAngle(CAM_SWING_EASE)))
                        step = want_swing > cam_swing ? degreesToAngle(CAM_SWING_EASE) : -degreesToAngle(CAM_SWING_EASE);
                    if ((want_swing - cam_swing) * (long long)(want_swing - cam_swing - step) <= 0)
                        cam_swing = want_swing;
                    else
                        cam_swing += step;
                    int s = sinLerp(cam_swing), c = cosLerp(cam_swing);
                    follow_eye[0] = head[0] + mulf32(off[0], c) - mulf32(off[1], s);
                    follow_eye[2] = head[2] + mulf32(off[0], s) + mulf32(off[1], c);
                    cam_clear(head, follow_eye, leon_feet);
                    // 回り込むと注視点が彼の方へ寄る（いっぱいに回ると彼の6前まで）：彼は画面の端でなく中に映る。
                    if (cam_swing)
                    {
                        int t = divf32(cam_swing < 0 ? -cam_swing : cam_swing, degreesToAngle(CAM_SWING_MAX));
                        int in[3] = { leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(6)), follow_target[1],
                                      leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(6)) };
                        for (int k = 0; k < 3; k += 2)
                            follow_target[k] += mulf32(in[k] - follow_target[k], t);
                    }
                }
                else
                    cam_swing = 0;
                // まわりじゅうの壁で近くに押されると、カメラは彼の中に入ってしまう：代わりに上がって肩越しに
                // 見下ろし、注視点を寄せて彼を画面に入れる。さらに近いと彼は透けて邪魔にならない。
                int hx = follow_eye[0] - leon_pos[0], hz = follow_eye[2] - leon_pos[2];
                int near = (int)sqrt64((u64)((s64)hx * hx + (s64)hz * hz));
                leon_fade = 31;
                if (near < CAM_NEAR && !climbing)
                {
                    int lift = leon_feet + CAM_HEIGHT + mulf32(CAM_NEAR - near, CAM_NEAR_LIFT);
                    if (follow_eye[1] < lift)
                        follow_eye[1] = lift;
                    int t = divf32(CAM_NEAR - near, CAM_NEAR);   // 0 .. 1：注視点を彼の6前、腰の高さまで寄せる
                    int in[3] = { leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(6)), leon_feet + inttof32(8),
                                  leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(6)) };
                    for (int k = 0; k < 3; k++)
                        follow_target[k] += mulf32(in[k] - follow_target[k], t);
                    if (near < CAM_FADE)
                        leon_fade = 8 + 23 * near / CAM_FADE;
                }
            }            // カメラをその下の床より上に保つ（レオンの後ろの丘。彼の頭上の上の階ではない、
            // 彼がそこから降りたばかりのときに）。
            int cam_floor;
            if (ground_height(&stage_ground, follow_eye[0], follow_eye[2], (follow_eye[1] < want_eye[1] + inttof32(6) ? follow_eye[1] : want_eye[1] + inttof32(6)), 0, &cam_floor)
                && follow_eye[1] < cam_floor + inttof32(3))
                follow_eye[1] = cam_floor + inttof32(3);
            for (int k = 0; k < 3; k++)
            {
                look_eye[k] = follow_eye[k];
                look_at[k] = follow_target[k];
            }
            if (scoping)   // （彼の目からの視点、スコープの狭いもの：set_fov() を参照）
                for (int k = 0; k < 3; k++)
                {
                    look_eye[k] = muzzle[k];
                    look_at[k] = muzzle[k] + scope_dir[k] * 64;
                }
        }
        else
        {
            if (held & KEY_LEFT)  yaw -= degreesToAngle(2);
            if (held & KEY_RIGHT) yaw += degreesToAngle(2);
            if (held & KEY_X)     pitch += degreesToAngle(1);
            if (held & KEY_B)     pitch -= degreesToAngle(1);
            if (held & KEY_TOUCH)
            {
                touchPosition t;
                touchRead(&t);
                if (!(down & KEY_TOUCH))
                {
                    yaw += (t.px - last_touch.px) * degreesToAngle(1) / 2;
                    pitch -= (t.py - last_touch.py) * degreesToAngle(1) / 2;
                }
                last_touch = t;
            }
            if (pitch > degreesToAngle(80))  pitch = degreesToAngle(80);
            if (pitch < -degreesToAngle(80)) pitch = -degreesToAngle(80);

            int sy = sinLerp(yaw), cy = cosLerp(yaw), sp = sinLerp(pitch), cp = cosLerp(pitch);
            int fwd[3] = { mulf32(sy, cp), sp, -mulf32(cy, cp) };
            int right[3] = { cy, 0, sy };

            const int speed = floattof32(1.5);
            int dir = ((held & KEY_UP) ? 1 : 0) - ((held & KEY_DOWN) ? 1 : 0);
            int side = ((held & KEY_R) ? 1 : 0) - ((held & KEY_L) ? 1 : 0);
            int lift = ((held & KEY_A) ? 1 : 0) - ((held & KEY_Y) ? 1 : 0);
            eye[0] += mulf32(sy, speed) * dir + mulf32(right[0], speed) * side;
            eye[2] += -mulf32(cy, speed) * dir + mulf32(right[2], speed) * side;
            eye[1] += speed * lift;
            for (int k = 0; k < 3; k++)
            {
                look_eye[k] = eye[k];
                look_at[k] = eye[k] + fwd[k];
            }
        }

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAtf32(look_eye[0], look_eye[1], look_eye[2], look_at[0], look_at[1], look_at[2], 0, inttof32(1), 0);

        glColor(RGB15(31, 31, 31));
        int origin[3] = { 0, 0, 0 };
#ifdef TEST_ASH_FIRST   // テスト用ビルド：アシュリーを一番先に（ポリゴンの予算の確認）
        if (ash.on)
            ashley_draw(&ash, cull_modes[cull] | POLY_ID(6) | POLY_FOG);
        if (giant.on)
            giant_draw(&giant, cull_modes[cull] | POLY_ID(7) | POLY_FOG);
#endif
#ifdef TEST_PROF
        cpuStartTiming(0);
        prof_mark = 0;
#endif
        model_set_view(look_eye, look_at);
        if (stage.chunked)
        {
            // DS は1フレームに2048ポリゴン（カメラの方を向いて画面内にあるもの）を保ち、その後に来るものは
            // 落とす：最初に送るステージは、前のフレームで余った分を近い順にもらう（フレームの終わりを参照）。
            // だからキャラクターとレーザーサイトはまだ収まる。
            if (stage_budget < 0 || stage_budget > (int)stage.triangles)
                stage_budget = stage.triangles;
            // レオンが出入りする建物（6面の小屋）：外からは中を描かない。中からはその庭より先を何も描かない
            // （ハードウェアの2048ポリゴンが村全体に使われ、小屋の壁と村人が落とされていた）。
            {
                const Room *rm = room_of(cur_stage);
                if (rm)
                {
                    int in = leon_pos[0] > inttof32(rm->x0) && leon_pos[0] < inttof32(rm->x1) && leon_pos[2] > inttof32(rm->z0) && leon_pos[2] < inttof32(rm->z1)
                             && leon_feet < inttof32(rm->top);
                    int box[4] = { inttof32(rm->x0 + (in ? -rm->yard : 4)), inttof32(rm->z0 + (in ? -rm->yard : 4)),
                                   inttof32(rm->x1 - (in ? -rm->yard : 4)), inttof32(rm->z1 - (in ? -rm->yard : 4)) };
                    model_set_cull_box(in ? CULL_OUTSIDE : CULL_INSIDE, box);
                }
                else
                    model_set_cull_box(CULL_NONE, NULL);
                // 回廊：8面の回廊の上からは、下のホール（窓越しに見える）も、そこにいる者も描かない。窓は
                // カーテンのように暗く見える。
                int below = 0;
                for (unsigned i = 0; i < sizeof(galleries) / sizeof(galleries[0]); i++)
                    if (galleries[i].stage == cur_stage && leon_feet >= inttof32(galleries[i].from))
                        below = inttof32(galleries[i].below);
                model_set_cull_below(below);
                ganados.hide_below = below;
            }
            model_draw_near(&stage, look_eye, look_at, stage_budget, cull_modes[cull] | POLY_ID(1) | POLY_FOG);
        }
        else
            model_draw_async(&stage, origin, cull_modes[cull] | POLY_ID(1) | POLY_FOG);

        // ステージを DMA でジオメトリエンジンに流している間に、CPU はレオンのポーズを作り、レーザーを
        // 飛ばし、ガナードを動かす。それから転送を待ってから続きを描く。
        if (leon_anim != ANIM_OFF)
        {
            anim_frame += (anim_dir < 0 ? ANIM_STEP * 4 / 3 : ANIM_STEP) * anim_dir;   // （後ろ歩き：歩きの300に対して400）

            // 基本のアニメーション：抜き / しまい中は抜き、銃を出している間はハンドガンのセット（構え、歩き、
            // 走り）、それ以外は素手のセット。
            const AnimSet *set = &leon_anims;
            int clip = leon_anim, clip_frame = anim_frame;
            if (leon_hp < LEON_HURT_BELOW)
                clip += CLIP_HURT_IDLE;   // （待機、歩き、走り：負傷時）
            if (react != REACT_NONE)
            {
                clip = react_clip[react];
                clip_frame = react_frame;
                int len = inttof32(leon_anims.frames[clip]) - 1;
                if (react == REACT_CLIMB_DOWN)
                    clip_frame = len - react_frame;   // 登り、逆再生
                if (clip_frame > len)
                    clip_frame = len;                 // 最後のフレームで止める（先頭に戻らない）
                if (clip_frame < 0)
                    clip_frame = 0;
            }
            else if (drawing)
            {
                set = &leon_gun;
                clip = GUN_DRAW;
                clip_frame = draw_frame;
            }
            else if (gun_state == GUN_READY)
            {
                set = &leon_gun;
                clip = leon_anim == ANIM_WALK ? GUN_WALK : leon_anim == ANIM_RUN ? GUN_RUN : GUN_READY_POSE;
            }
            if (knifing && react == REACT_NONE)
            {
                set = &leon_knife;
                clip = slash_frame >= 0 ? (slash_high ? KNIFE_HIGH : KNIFE_SLASH) : KNIFE_READY;
                clip_frame = slash_frame >= 0 ? slash_frame : 0;   // （構えは1ポーズ）
#ifdef TEST_KNIFE_VIEW   // テスト用ビルド：セットの各クリップを順に（それぞれ2秒）、ループ
                clip = (int)((vblank_count / 120) % 7);
                clip_frame = (int)(vblank_count % 120) * ANIM_STEP / 2;
                if (leon_knife.frames[clip] > 0) clip_frame %= inttof32(leon_knife.frames[clip]); else clip_frame = 0;
#endif
            }
            anim_sample(set, clip, clip_frame, &leon, pose_base);

            // アニメーションが変わったら前のポーズからクロスフェード。
            int key = (set == &leon_gun ? 100 : set == &leon_knife ? 200 : 0) + clip;
            if (key != clip_key)
            {
                if (clip_key >= 0)
                {
                    memcpy(pose_from, pose_prev, sizeof(pose_from));
                    xfade_w = inttof32(1);
                }
                clip_key = key;
            }
            if (xfade_w > 0)
            {
                memcpy(pose_tilt, pose_from, sizeof(pose_tilt));
                anim_blend(pose_tilt, pose_base, leon.node_count, inttof32(1) - xfade_w);
                memcpy(pose_base, pose_tilt, sizeof(pose_base));
                xfade_w -= XFADE_STEP;
            }
            memcpy(pose_prev, pose_base, sizeof(pose_prev));


            if (aim_w)
            {
                // 構えのポーズ、今のアニメーションの上に aim_w でブレンド。
                // 両手の構え：水平のポーズから、傾きに応じて上か下のポーズへ。
                anim_sample(&leon_gun, AIM_FORWARD, 0, &leon, pose_aim);
                anim_sample(&leon_gun, aim_tilt >= 0 ? AIM_UP : AIM_DOWN, 0, &leon, pose_tilt);
                anim_blend(pose_aim, pose_tilt, leon.node_count, aim_tilt >= 0 ? aim_tilt : -aim_tilt);
                anim_blend(pose_base, pose_aim, leon.node_count, aim_w);
            }
            if (fire_frame >= 0 && aim_w)
            {
                // 射撃：射撃アニメーションの動き（ショットガンの反動とポンプ）を最初のフレームから、各関節の回転を
                // 構えの後に回転としてかける（オイラー角で足したり、上下のクリップを丸ごと混ぜたりすると、ポンプで
                // 銃が肩越しに振れた）。クリップは彼の狙う向き：上か下。
                int clip_fire = aim_tilt >= 0 ? FIRE_UP : FIRE_DOWN;
                anim_sample(&leon_gun, clip_fire, fire_frame, &leon, pose_fx);
                anim_sample(&leon_gun, clip_fire, 0, &leon, pose_fx0);
                anim_add_delta_rot(pose_base, pose_fx, pose_fx0, leon.node_count, mulf32(RECOIL, aim_w));
            }
            if (reload_frame >= 0)
            {
                // リロードのポーズをほかの全部の上に、両端でフェードイン / アウト。
                int edge = reload_frame < inttof32(RELOAD_FRAMES) - reload_frame
                         ? reload_frame : inttof32(RELOAD_FRAMES) - reload_frame;
                int w = divf32(edge, RELOAD_FADE);
                anim_sample(&leon_gun, GUN_RELOAD, reload_frame, &leon, pose_fx);
                // （背骨から上の上半身だけ：脚は歩き続ける）
                anim_blend(pose_base + UPPER_BODY, pose_fx + UPPER_BODY, leon.node_count - UPPER_BODY, w > inttof32(1) ? inttof32(1) : w);
            }
            anim_compose(&leon, pose_base, leon_pose);
            // ナイフでは上半身が視点の上下に従う：腰から上の全部（ノード UPPER_BODY とその下の全部）が、腰を
            // 通る彼自身の左右の軸のまわりに回り、見上げるとのけぞり、見下ろすと前へ倒れる。（胴がねじれている
            // ナイフの構えの中で関節ごとに回すと、横に曲がっていた。）
            if (knifing && react == REACT_NONE && aim_tilt)
            {
                // （後ろへは最大 KNIFE_LEAN_UP：それ以上だと胸が後ろのカメラに入った。前へは KNIFE_LEAN_DOWN：
                // 構えはもうしゃがんでいる）
                int a = -mulf32(aim_tilt, aim_tilt > 0 ? KNIFE_LEAN_UP : KNIFE_LEAN_DOWN), c, s;
                c = cosLerp(a);
                s = sinLerp(a);
                const int *w = leon_pose[UPPER_BODY];
                int py = w[10], pz = w[11];
                int rot[12] = { inttof32(1), 0, 0,  0, c, -s,  0, s, c,
                                0, py - mulf32(c, py) + mulf32(s, pz), pz - mulf32(s, py) - mulf32(c, pz) };
                for (int n = UPPER_BODY; n < leon.node_count; n++)
                    mtx43_mul(leon_pose[n], rot, leon_pose[n]);
            }

            // ワールドでの銃口と銃身の向き（銃は手のノードにぶら下がる。銃身は -X 向き）、そしてレーザー / 弾が
            // 当たる所。
            const int *hand = leon_pose[HAND_NODE];
            int m_model[3], d_model[3] = { -hand[0], -hand[3], -hand[6] }, d_world[3], zero[3] = { 0, 0, 0 };
            for (int k = 0; k < 3; k++)
                m_model[k] = mulf32(hand[k * 3], weapons[wep].muzzle[0]) + mulf32(hand[k * 3 + 1], weapons[wep].muzzle[1])
                           + mulf32(hand[k * 3 + 2], weapons[wep].muzzle[2]) + hand[9 + k];
            leon_to_world(leon_pos, leon_yaw, m_model, muzzle);
            leon_to_world(zero, leon_yaw, d_model, d_world);
            // スコープ越しでは弾は見ている所へ行く：彼の目から、視線に沿って。
            scoping = weapons[wep].special == WS_SCOPE && aim_w == inttof32(1) && reload_frame < 0 && react == REACT_NONE;
            if (scoping)
            {
                int pitch = mulf32(aim_tilt, SCOPE_PITCH), cp = cosLerp(pitch);
                d_world[0] = mulf32(sinLerp(leon_yaw), cp);
                d_world[1] = sinLerp(pitch);
                d_world[2] = mulf32(cosLerp(leon_yaw), cp);
                muzzle[0] = leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(2));
                muzzle[1] = leon_feet + SCOPE_EYE;
                muzzle[2] = leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(2));
                for (int k = 0; k < 3; k++)
                    scope_dir[k] = d_world[k];
            }
            laser_hit = 0;
            laser_enemy = laser_obj = -1;
            if (aim_w == inttof32(1) && reload_frame < 0)
            {
                laser_hit = ground_raycast(&stage_ground, muzzle, d_world, SHOT_RANGE, laser_end, RAY_SHOT);
                // 壁 / 床より近い木箱 / 樽 / メダル、またはガナードが代わりに弾を受ける（一番近いもの）。
                int t_best = SHOT_RANGE;
                if (laser_hit)
                {
                    s64 ex = laser_end[0] - muzzle[0], ey = laser_end[1] - muzzle[1], ez = laser_end[2] - muzzle[2];
                    t_best = (int)sqrt64((u64)(ex * ex + ey * ey + ez * ez));
                }
                laser_obj = objects_raycast(&objs, muzzle, d_world, &t_best);
                // （梁や軒の奥に隠れた宝も、その奥のこの距離までなら数える。）
                int t_drop = t_best + TREASURE_REACH;
                laser_drop = drops_raycast(&drops, muzzle, d_world, &t_drop);
                if (laser_drop >= 0)
                {
                    if (t_drop < t_best)
                        t_best = t_drop;
                    laser_obj = -1;
                }
                laser_enemy = enemies_raycast(&ganados, muzzle, d_world, &t_best, &laser_part);
                // 7面の城壁の上の投げる者は胸壁の奥に立ち、その当たり判定が下の地面からの弾を全部止める（床の
                // どの場所からも試した：1つも通らなかった）：壁に当たった弾は、宝が梁の上にあるときと同じく、
                // その奥 THROWER_REACH 以内の上にいる者に当たったことにする。
                if (laser_enemy < 0 && laser_hit)
                {
                    int t2 = t_best + THROWER_REACH, part2;
                    int k = enemies_raycast(&ganados, muzzle, d_world, &t2, &part2);
                    if (k >= 0 && ganados.e[k].thrower)
                    {
                        laser_enemy = k;
                        laser_part = part2;
                        t_best = t2;
                    }
                }
                laser_head = laser_enemy >= 0 && laser_part == PART_HEAD;
                laser_ash = ash.on && ashley_raycast(&ash, muzzle, d_world, &t_best);   // 彼女が邪魔
                laser_giant = 0;   // （1 + どれか）
                for (int i = 0; i < giant_count; i++)
                    if (giant_raycast(&giants[i], muzzle, d_world, &t_best))
                        laser_giant = i + 1;
                laser_gal = 0;   // （1 + どれか）
                for (int i = 0; i < gal_count; i++)
                    if (garrador_raycast(&gals[i], muzzle, d_world, &t_best, &laser_gal_back))
                        laser_gal = i + 1;
                if (laser_gal)
                    laser_enemy = laser_obj = -1, laser_ash = laser_giant = 0;
                laser_ver = ver.on && verdugo_raycast(&ver, muzzle, d_world, &t_best);
                if (laser_ver)
                    laser_enemy = laser_obj = -1, laser_ash = laser_giant = laser_gal = 0;
                if (laser_giant)
                    laser_enemy = laser_obj = -1, laser_ash = 0;
                if (laser_ash)
                    laser_enemy = laser_obj = -1;
                if (laser_enemy >= 0)
                    laser_obj = -1;
                if (laser_enemy >= 0 || laser_obj >= 0 || laser_ash || laser_giant || laser_gal || laser_ver || !laser_hit)
                    for (int k = 0; k < 3; k++)
                        laser_end[k] = muzzle[k] + mulf32(d_world[k], t_best);
                laser_hit |= laser_enemy >= 0 || laser_obj >= 0 || laser_ash || laser_giant || laser_gal || laser_ver;
            }
        }

#ifdef TEST_KILL_AT   // テスト用ビルド：このフレームでガナードが全員倒れ、それぞれアイテムを落とす
        static u32 test_frame;
        if (++test_frame == TEST_KILL_AT)
            for (int i = 0; i < ganados.count; i++)
            {
                enemies_damage(&ganados, i, ENEMY_FELL);
                maybe_drop(&ganados.e[i], leon_hp, 100);
            }
#endif
#ifdef TEST_KILL_EVERY   // テスト用ビルド：このフレーム数ごとに、ステージのガナードが倒れる
        static u32 kill_frame;
        if (++kill_frame % TEST_KILL_EVERY == 0)
            for (int i = 0; i < ganados.count; i++)
                if (ganados.e[i].active && ganados.e[i].state < ENEMY_DYING && !ganados.e[i].thrower)
                {
                    enemies_damage(&ganados, i, ENEMY_FELL);
                    maybe_drop(&ganados.e[i], leon_hp, 100);
                }
        if (kill_frame % TEST_KILL_EVERY == 0)   // （とボス）
        {
            for (int i = 0; i < giant_count; i++)
                giant_blast(&giants[i]);
            for (int i = 0; i < gal_count; i++)
                garrador_shot(&gals[i], ENEMY_FELL, 1);
            if (ver.on)
                verdugo_shot(&ver, ENEMY_FELL);
        }
#endif
        ganados.noise = shot_noise || (leon_anim == ANIM_RUN && react == REACT_NONE);   // （聞こえた：走りか射撃）
        shot_noise = 0;
#ifdef TEST_GOD   // テスト用ビルド：ガナードは決してレオンを傷つけない
        enemies_update(&ganados, leon_pos, 0, &ev);
#else
        PROF(1);
        enemies_update(&ganados, leon_pos, ctrl == CTRL_LEON && free_to_act && safe_timer == 0 && clear_t < 0, &ev);
        PROF(2);
#endif
        // ガナードの声と振り、その場所から。
        if (ev.spotted >= 0)
            sfx_play_at(SFX_GANADO_SPOT, 110, ganados.e[ev.spotted].pos, leon_pos, leon_yaw);
        if (ev.attacking >= 0)
            sfx_play_at(SFX_GANADO_ATTACK, 120, ganados.e[ev.attacking].pos, leon_pos, leon_yaw);
        if (ev.swing >= 0)
            sfx_play_at(SFX_SWING, 100, ganados.e[ev.swing].pos, leon_pos, leon_yaw);
        int damage = 0;
        if (ev.swiped_by >= 0)
        {
            damage = armored(HIT_DAMAGE(&ganados.e[ev.swiped_by]));   // （ベスト。チェーンソーには効かない）
            if (ganados.e[ev.swiped_by].boss)
                damage = LEON_HP;   // チェーンソー：誰も生き残れない
            react = REACT_FLINCH;
            react_frame = 0;
        }
        // ボウガンの矢：彼の胸に向けて放たれ、彼か壁に当たるまでまっすぐ飛ぶ。
        if (ev.shot >= 0)
        {
            const Enemy *e = &ganados.e[ev.shot];
            int from[3] = { e->pos[0] + mulf32(sinLerp(e->yaw), inttof32(2)), e->feet + BOLT_HEIGHT, e->pos[2] + mulf32(cosLerp(e->yaw), inttof32(2)) };
            // （狙われているのがアシュリーなら彼女に：彼女の胸、彼より低い）
            int at_ash = ash.on && ev.shot_at[0] == ash.pos[0] && ev.shot_at[2] == ash.pos[2];
            int to[3] = { ev.shot_at[0], at_ash ? ash.feet + inttof32(8) : leon_feet + BOLT_AIM, ev.shot_at[2] };
            bolt_fire(from, to, HIT_DAMAGE(e));
            sfx_play_at(SFX_SWING, 110, e->pos, leon_pos, leon_yaw);
        }
        {
            int ash_bd = 0;
#ifdef TEST_GOD
            const int bolt_vuln = 0;
#else
            const int bolt_vuln = ctrl == CTRL_LEON && free_to_act && safe_timer == 0 && clear_t < 0;
#endif
            int bd = bolts_update((const int[3]){ leon_pos[0], leon_feet, leon_pos[2] }, bolt_vuln,
                                  ash.on && !ash.wait ? (const int[3]){ ash.pos[0], ash.feet, ash.pos[2] } : NULL, &ash_bd);
            if (bd > 0 && !damage)
            {
                damage = armored(bd);
                react = REACT_FLINCH;
                react_frame = 0;
            }
            if (ash_bd > 0)
            {
                ashley_hit(&ash, ash_bd);
                sfx_play_at(SFX_LEON_HURT2, 120, ash.pos, leon_pos, leon_yaw);
            }
        }
        if (ev.grabbed_by >= 0)
        {
            // ガナードの方を向く。ガナードは彼の目の前でつかむ。
            holder = ev.grabbed_by;
            leon_yaw = ganados.e[holder].yaw + DEGREES_IN_CIRCLE / 2;
            enemies_hold_at(&ganados, holder, leon_pos, GRAB_DIST);
            react = REACT_HELD;
            react_frame = 0;
            mash = held_frames = 0;
        }
        // エルヒガンテ：レオンへ歩く。その打撃はとても痛い。
        for (int gi = 0; gi < giant_count; gi++)
        {
            Giant *gp = &giants[gi];
            for (int j = 0; j < giant_count; j++)   // 互いにぶつからない
                if (j != gi)
                    giant_push(&giants[j], &gp->pos[0], &gp->pos[2], GIANT_RADIUS);
            GiantEvents gev;
#ifdef TEST_ELG_VIEW
            memset(&gev, 0, sizeof(gev));
#else
#ifdef TEST_GOD
            giant_update(gp, &stage_ground, leon_pos, 0, &gev);
#else
            giant_update(gp, &stage_ground, leon_pos, ctrl == CTRL_LEON && free_to_act && safe_timer == 0 && clear_t < 0, &gev);
#endif
#ifdef TEST_AUTO_CUT   // テスト用ビルド：寄生体を斬れるようになったらすぐ斬る
            if (giant_cuttable(gp, leon_pos))
            {
                giant_cut(gp);
                sfx_play(SFX_KNIFE, 127, 64);
            }
#endif
#endif
            if (gev.stomp)
                cam_kick = floattof32(2.5);
            if (gev.hit_leon && react != REACT_DEAD)
            {
                leon_hp -= armored(gev.hit_leon);
                damage_taken += armored(gev.hit_leon);
                hurt_flash = 12;
                sfx_play(SFX_LEON_HURT2, 127, 64);
                if (leon_hp <= 0)
                {
                    leon_hp = 0;
                    if (holder >= 0)
                        enemies_release(&ganados, holder);
                    holder = -1;
                    react = REACT_DEAD;
                }
                else
                    react = REACT_FLINCH;
                react_frame = 0;
            }
        }
        // ガラドール：近くで（またはすぐ横で）走るレオンが聞こえると、突進して斬る。
        for (int gi = 0; gi < gal_count; gi++)
        {
            Garrador *gp = &gals[gi];
            GarradorEvents gev;
#ifdef TEST_GAL_VIEW
            memset(&gev, 0, sizeof(gev));
#else
            if (ctrl == CTRL_LEON && react != REACT_DEAD)
                garrador_hear(gp, leon_pos, leon_anim == ANIM_RUN ? GAL_HEAR_RUN : GAL_HEAR_NEAR);
#ifdef TEST_GOD
            garrador_update(gp, &stage_ground, leon_pos, ash.on ? ash.pos : NULL, 0, &gev);
#else
            garrador_update(gp, &stage_ground, leon_pos, ash.on ? ash.pos : NULL, ctrl == CTRL_LEON && free_to_act && safe_timer == 0 && clear_t < 0, &gev);
#endif
#endif
            if (gev.roar)
                sfx_play_at(SFX_GANADO_HIT, 127, gp->pos, leon_pos, leon_yaw);
            if (gev.hit_ally)
                ashley_hit(&ash, gev.hit_ally);
            if (gev.hit_leon && react != REACT_DEAD)
            {
                leon_hp -= armored(gev.hit_leon);
                damage_taken += armored(gev.hit_leon);
                hurt_flash = 12;
                sfx_play(SFX_LEON_HURT2, 127, 64);
                if (leon_hp <= 0)
                {
                    leon_hp = 0;
                    if (holder >= 0)
                        enemies_release(&ganados, holder);
                    holder = -1;
                    react = REACT_DEAD;
                }
                else
                    react = REACT_FLINCH;
                react_frame = 0;
            }
        }
        // ヴェルデューゴ：レオンに忍び寄り、尾で打ち、跳び上がって彼のそばに降りる。
        if (ver.on)
        {
#ifdef TEST_TANK_AT   // テスト用ビルド：このフレームで彼に一番近いタンクが破裂する
            static u32 tank_frame;
            if (++tank_frame == TEST_TANK_AT)
            {
                int best = -1;
                s64 bd = 0;
                for (int i = 0; i < objs.count; i++)
                {
                    const Obj *o = &objs.o[i];
                    s64 ox = o->pos[0] - ver.pos[0], oz = o->pos[2] - ver.pos[2];
                    if (o->kind == OBJ_BARREL && o->alive && (best < 0 || ox * ox + oz * oz < bd))
                        best = i, bd = ox * ox + oz * oz;
                }
                if (best >= 0)
                    objects_hit(&objs, best);
            }
#endif
            VerdugoEvents vev;
#ifdef TEST_VER_VIEW
            memset(&vev, 0, sizeof(vev));
#else
#ifdef TEST_GOD
            verdugo_update(&ver, &stage_ground, leon_pos, leon_feet, 0, &vev);
#else
            verdugo_update(&ver, &stage_ground, leon_pos, leon_feet, ctrl == CTRL_LEON && free_to_act && safe_timer == 0 && clear_t < 0, &vev);
#endif
#endif
            if (vev.landed)
            {
                cam_kick = floattof32(2);
                sfx_play_at(SFX_GANADO_HIT, 127, ver.pos, leon_pos, leon_yaw);
            }
            if (vev.hit_leon && react != REACT_DEAD)
            {
                leon_hp -= armored(vev.hit_leon);
                damage_taken += armored(vev.hit_leon);
                hurt_flash = 12;
                sfx_play(SFX_LEON_HURT2, 127, 64);
                if (leon_hp <= 0)
                {
                    leon_hp = 0;
                    if (holder >= 0)
                        enemies_release(&ganados, holder);
                    holder = -1;
                    react = REACT_DEAD;
                }
                else
                    react = REACT_FLINCH;
                react_frame = 0;
            }
        }
        // アシュリー：レオンについて行く。ガナードの振りが当たると傷つく。彼女が倒れたら終わり。
        if (ash.on)
        {
            // レオンが彼女の方を狙う（約35度、80単位以内）：彼女はかがむ。
            {
                int ax = ash.pos[0] - leon_pos[0], az = ash.pos[2] - leon_pos[2];
                int ad = (int)sqrt64((u64)((s64)ax * ax + (s64)az * az));
                s64 dot = ((s64)sinLerp(leon_yaw) * ax + (s64)cosLerp(leon_yaw) * az) >> 12;
                ash.duck = aim_w > 0 && ad < inttof32(80) && dot * 10 > (s64)ad * 8;
            }
#ifndef TEST_ASH_VIEW
            // はしごや、はしごの無い段差から（落下）：彼が離れたら彼女もそこにいる。
            if (!ash.wait && !ash.ladder && ash.state == ASH_FOLLOW && react == REACT_NONE
                && (ash.feet - leon_feet > inttof32(10) || leon_feet - ash.feet > inttof32(10)))
            {
                int hp_was = ash.hp;   // （体力はそのまま）
                ashley_place(&ash, &stage_ground, leon_pos[0] - mulf32(sinLerp(leon_yaw), inttof32(5)) + mulf32(cosLerp(leon_yaw), inttof32(7)),
                             leon_pos[2] - mulf32(cosLerp(leon_yaw), inttof32(5)) - mulf32(sinLerp(leon_yaw), inttof32(7)), leon_feet + inttof32(4), leon_yaw);
                ash.hp = hp_was;
            }
            // 彼の階で遠くに取り残された（彼女はまっすぐ彼へ歩くので、壁でずっと止まることがある）：
            // 40より離れて6秒たつと、彼のすぐ後ろ（邪魔にならない所）に来る。
            {
                static int ash_far_t;
                int fdx = ash.pos[0] - leon_pos[0], fdz = ash.pos[2] - leon_pos[2];
                int far = !ash.wait && !ash.ladder && ash.state == ASH_FOLLOW && react == REACT_NONE
                          && (s64)fdx * fdx + (s64)fdz * fdz > (s64)inttof32(40) * inttof32(40);
                ash_far_t = far ? ash_far_t + 1 : 0;
                if (ash_far_t > 6 * 60)
                {
                    int hp_was = ash.hp;   // （それで回復はしない）
                    ashley_place(&ash, &stage_ground, leon_pos[0] - mulf32(sinLerp(leon_yaw), inttof32(6)), leon_pos[2] - mulf32(cosLerp(leon_yaw), inttof32(6)),
                                 leon_feet + inttof32(4), leon_yaw);
                    ash.hp = hp_was;
                    ash_far_t = 0;
                }
            }
            ashley_update(&ash, &stage_ground, leon_pos, leon_yaw);
#endif
            if (ev.ally_hit >= 0)
            {
                ashley_hit(&ash, HIT_DAMAGE(&ganados.e[ev.ally_hit]));
                sfx_play_at(SFX_LEON_HURT2, 120, ash.pos, leon_pos, leon_yaw);
            }
            if (ash.state == ASH_DOWN && mode == MODE_PLAY && ++ash_lost >= DEAD_TO_MENU)
            {
                mode = MODE_GAMEOVER;
                dead_timer = DEAD_TO_MENU;
            }
        }
        // 投げる者が投げる：今レオンが立っている所へ。爆発する投げ物は近くの者を傷つける。
        if (ev.thrown >= 0)
        {
            const Enemy *e = &ganados.e[ev.thrown];
            int from[3] = { e->pos[0], e->feet + inttof32(14), e->pos[2] };
            int to[3] = { leon_pos[0], leon_feet, leon_pos[2] };
            missiles_throw(&missiles, from, to);
#ifdef TEST_SHOW_THROW
            throws_seen++;
#endif
            sfx_play_at(SFX_SWING, 100, e->pos, leon_pos, leon_yaw);
        }
        int went = missiles_update(&missiles, &stage_ground);
        if (went >= 0)
        {
            const int *bp = missiles.m[went].pos;
            sfx_play_at(SFX_EXPLODE, 127, bp, leon_pos, leon_yaw);
            cam_kick = floattof32(2);
            int dx = leon_pos[0] - bp[0], dz = leon_pos[2] - bp[2];
#ifndef TEST_GOD
            if ((s64)dx * dx + (s64)dz * dz < (s64)MISSILE_RANGE * MISSILE_RANGE && react != REACT_DEAD && react != REACT_HELD)
            {
                leon_hp -= armored(MISSILE_DAMAGE);
                damage_taken += armored(MISSILE_DAMAGE);
                hurt_flash = 12;
                sfx_play(SFX_LEON_HURT2, 120, 64);
                if (leon_hp <= 0)
                {
                    leon_hp = 0;
                    react = REACT_DEAD;
                }
                else
                    react = REACT_FLINCH;
                react_frame = 0;
            }
#endif
            if (ash.on)
            {
                int ax = ash.pos[0] - bp[0], az = ash.pos[2] - bp[2];
                if ((s64)ax * ax + (s64)az * az < (s64)MISSILE_RANGE * MISSILE_RANGE)
                    ashley_hit(&ash, MISSILE_DAMAGE / 3);   // （彼女はかがむ。岩1つ30だと7面は数回の投擲で終わる）
            }
        }
        if (ev.bite >= 0)
            damage = armored(BITE_DAMAGE);
        if (damage)
        {
            leon_hp -= damage;
            damage_taken += damage;
            hurt_flash = 12;
            sfx_play(damage == armored(SWIPE_DAMAGE) && leon_hp > 0 ? SFX_LEON_HURT : SFX_LEON_HURT2, 120, 64);
            if (leon_hp <= 0)
            {
                leon_hp = 0;
                if (holder >= 0)
                    enemies_release(&ganados, holder);
                holder = -1;
                react = REACT_DEAD;
                react_frame = 0;
            }
        }

        // ステージの仕掛け：揺れる門、樽の導火線。樽の爆発は BLAST_RANGE 以内のガナードを全員倒し、その
        // 半分以内のレオンを傷つける。
#ifdef TEST_HANG_AT   // テスト用ビルド：このフレームで射撃が吊るされた容器を落とす
        static u32 hang_frame;
        if (++hang_frame == TEST_HANG_AT)
            for (int i = 0; i < objs.count; i++)
                if (objs.o[i].kind == OBJ_HANG)
                {
#ifdef TEST_HANG_RAY   // （弾自身の判定で：銃の高さから容器への光線）
                    const Obj *o = &objs.o[i];
                    int p[3] = { leon_pos[0], leon_feet + inttof32(16), leon_pos[2] };
                    float dx = f32tofloat(o->pos[0] - p[0]), dy = f32tofloat(o->pos[1] - inttof32(3) - p[1]), dz = f32tofloat(o->pos[2] - p[2]);
                    float len = sqrtf(dx * dx + dy * dy + dz * dz);
                    int d[3] = { floattof32(dx / len), floattof32(dy / len), floattof32(dz / len) }, t = inttof32(400);
                    if (objects_raycast(&objs, p, d, &t) == i)
#endif
                    objects_hit(&objs, i);
                }
#endif
        objs.leon = leon_pos;
        objects_update(&objs, &oev);
        rocket_boom = 0;
        if (rocket_on)
        {
            int dv[3] = { rocket_to[0] - rocket_pos[0], rocket_to[1] - rocket_pos[1], rocket_to[2] - rocket_pos[2] };
            int len = (int)sqrt64((u64)((s64)dv[0] * dv[0] + (s64)dv[1] * dv[1] + (s64)dv[2] * dv[2]));
            if (len <= ROCKET_SPEED)
            {
                // 着いた：破裂する（ランチャーは1発だけ：無くなり、ハンドガンを持つ）。
                rocket_on = 0;
                rocket_boom = 1;
                oev.exploded = 1;
                oev.small = 0;
                for (int k = 0; k < 3; k++)
                    oev.explode_pos[k] = rocket_to[k];
                objects_boom(&objs, rocket_to);
                if (wep == WEP_ROCKET)
                {
                    wep_owned &= ~(1 << WEP_ROCKET);
                    weapon_switch(WEP_HANDGUN, &ammo, &spare);
                    fire_frame = reload_frame = -1;
                    clip_key = -1;
                }
            }
            else
                for (int k = 0; k < 3; k++)
                    rocket_pos[k] += (int)((s64)dv[k] * ROCKET_SPEED / len);
        }
        if (oev.door >= 0)
        {
            const Obj *d = &objs.o[oev.door];
            int at[3] = { d->hinge[0], d->pos[1], d->hinge[1] };
            sfx_play_at(SFX_DOOR_KICK, 70, at, leon_pos, leon_yaw);
        }
        // 閉じた扉に歩いて来たガナードは押し開ける（原作で扉を通って来るとおり）。
        for (int i = 0; i < ganados.count; i++)
        {
            const Enemy *e = &ganados.e[i];
            if (!e->active || e->state >= ENEMY_HIT || e->thrower)
                continue;
            int d = objects_door_at(&objs, e->pos[0], e->pos[2], inttof32(6));
            if (d >= 0)
                objects_open_door(&objs, d, e->pos);
        }
        if (oev.exploded)
        {
            sfx_play_at(SFX_EXPLODE, 127, oev.explode_pos, leon_pos, leon_yaw);
            // （種類05の容器の爆発は原作のサイズ5：20単位、レオンには10、同じ階で）
            int blast = oev.small ? HANG_BLAST : rocket_boom ? ROCKET_BLAST : BLAST_RANGE, leon_blast = oev.small ? HANG_BLAST / 2 : BLAST_RANGE / 2;
            if (rocket_boom)   // 巨人とガラドールは倒れ、ヴェルデューゴは大きく傷つく
            {
                for (int g = 0; g < giant_count; g++)
                {
                    int dx = giants[g].pos[0] - oev.explode_pos[0], dz = giants[g].pos[2] - oev.explode_pos[2];
                    if ((s64)dx * dx + (s64)dz * dz < (s64)(ROCKET_BLAST + inttof32(9)) * (ROCKET_BLAST + inttof32(9)) && giant_blast(&giants[g]))
                    {
                        hits++;
                        sfx_play_at(SFX_GANADO_DOWN, 127, giants[g].pos, leon_pos, leon_yaw);
                    }
                }
                for (int g = 0; g < gal_count; g++)
                {
                    int dx = gals[g].pos[0] - oev.explode_pos[0], dz = gals[g].pos[2] - oev.explode_pos[2];
                    if ((s64)dx * dx + (s64)dz * dz < (s64)ROCKET_BLAST * ROCKET_BLAST)
                        garrador_shot(&gals[g], ENEMY_FELL, 1);
                }
                if (ver.on)
                {
                    int dx = ver.pos[0] - oev.explode_pos[0], dz = ver.pos[2] - oev.explode_pos[2];
                    if ((s64)dx * dx + (s64)dz * dz < (s64)ROCKET_BLAST * ROCKET_BLAST)
                        verdugo_shot(&ver, 5000);   // （彼の体力の4分の1：移植版独自）
                }
            }
            int level = !oev.small || (leon_feet - oev.explode_pos[1] < HANG_BLAST && oev.explode_pos[1] - leon_feet < HANG_BLAST);
            for (int g = 0; g < gal_count; g++)
                if (oev.small)
                    garrador_hear(&gals[g], oev.explode_pos, inttof32(4000));
            for (int i = 0; i < ganados.count; i++)
            {
                Enemy *e = &ganados.e[i];
                int dx = e->pos[0] - oev.explode_pos[0], dz = e->pos[2] - oev.explode_pos[2];
                int near_y = !oev.small || (e->feet - oev.explode_pos[1] < HANG_BLAST && oev.explode_pos[1] - e->feet < HANG_BLAST);
                if (near_y && e->active && e->state < ENEMY_DYING && (s64)dx * dx + (s64)dz * dz < (s64)blast * blast)
                {
                    if (!rocket_boom)
                        felled_by_blast++;   // （樽か容器のもの：Strategic!）
                    enemies_damage(&ganados, i, ENEMY_FELL);
                    sfx_play_at(SFX_GANADO_DOWN, 110, e->pos, leon_pos, leon_yaw);
                    maybe_drop(e, leon_hp, DROP_PERCENT);
                }
            }
            // 液体窒素のタンク：近くのヴェルデューゴは凍る。レオンは傷つかない。
            if (objs.cold && ver.on)
                verdugo_freeze(&ver, oev.explode_pos, TANK_RANGE);
            if (felled_by_blast)
                bonus_note(T_BONUS_STRAT);
            felled_by_blast = 0;
            int dx = leon_pos[0] - oev.explode_pos[0], dz = leon_pos[2] - oev.explode_pos[2];
            if (!objs.cold && level && (s64)dx * dx + (s64)dz * dz < (s64)leon_blast * leon_blast && react != REACT_DEAD)
            {
                leon_hp -= armored(BLAST_DAMAGE);
                damage_taken += armored(BLAST_DAMAGE);
                hurt_flash = 12;
                sfx_play(SFX_LEON_HURT2, 120, 64);
                if (leon_hp <= 0)
                {
                    leon_hp = 0;
                    react = REACT_DEAD;
                    react_frame = 0;
                }
                else if (react == REACT_NONE)
                {
                    react = REACT_FLINCH;
                    react_frame = 0;
                }
            }
            cam_kick = floattof32(3);
        }
        if (medal_note > 0)
            medal_note--;

        // ほかが倒れるにつれてガナードが入って来る（一度に最大 GANADO_ACTIVE）。ほかのステージでは、
        // ミッションのイベントがグループを入れ、ユニットが次のレコードを取る。
        if (cur_stage != 0 && mode == MODE_PLAY)
            {
            int feet_at[3] = { leon_pos[0], leon_feet, leon_pos[2] };
            foes_events(&stage_foes[cur_stage], cur_stage, feet_at);
            foes_step(feet_at);
            // 10面：鍵で扉が開いたら（イベント2、その場面10：「Ashley opens the door Leon is standing behind
            // and throws herself at him」）、場面のとおり彼女は彼と一緒にいる。まだ下のホールで彼が見つけに
            // 来るのを待ってはいない。
            if (cur_stage == ASH_STAGE_E && ash.on && ash.wait && ev_bit(ev_played, 2))
            {
                ashley_place(&ash, &stage_ground, leon_pos[0] - mulf32(sinLerp(leon_yaw), inttof32(5)) + mulf32(cosLerp(leon_yaw), inttof32(7)),
                             leon_pos[2] - mulf32(cosLerp(leon_yaw), inttof32(5)) - mulf32(sinLerp(leon_yaw), inttof32(7)), leon_feet + inttof32(4), leon_yaw);
                ash.wait = 0;
                ash_waiting = 0;
            }
        }
        else if (next_ganado < ganados.count && enemies_active_standing(&ganados) < GANADO_ACTIVE && mode == MODE_PLAY)
        {
            if (next_ganado == GANADO_FROM_DOOR)
            {
                int d = objects_door(&objs);
                const Obj *o = &objs.o[d];
                int at[2] = { (o->hinge[0] + o->end[0]) / 2, (o->hinge[1] + o->end[1]) / 2 + inttof32(2) };
                objects_open(&objs, d);
                sfx_play_at(SFX_DOOR_KICK, 110, ganados.e[next_ganado].pos, leon_pos, leon_yaw);
                enemies_activate(&ganados, next_ganado, at);
            }
            else
                enemies_activate(&ganados, next_ganado, NULL);
            next_ganado++;
        }

#ifdef TEST_KNOCK_AT   // テスト用ビルド：このフレームで高い所の宝が落ちてくる
        static u32 knock_frame;
        if (++knock_frame == TEST_KNOCK_AT)
            for (int i = 0; i < DROPS_MAX; i++)
            {
#ifdef TEST_KNOCK_AHEAD   // …レオンのこれだけ前に落ちる（拾うのを確かめるため）
                drops.d[i].land[0] = leon_pos[0] + sinLerp(leon_yaw) * TEST_KNOCK_AHEAD;
                drops.d[i].land[1] = leon_feet;
                drops.d[i].land[2] = leon_pos[2] + cosLerp(leon_yaw) * TEST_KNOCK_AHEAD;
#endif
                drops_knock(&drops, i);
            }
#endif
        // アイテム：そばで A を押すと拾う（take_item、A を読む所で設定）。
        int leon_at[3] = { leon_pos[0], leon_feet, leon_pos[2] };   // （足元：段差の上の宝は届かないまま）
#ifdef TEST_ASH_ROOM   // テスト用ビルド：5面のチャペルのまわりの場所から、敵の塗りつぶしがアシュリーの部屋に入るか？
        if (play_frames == 2)
                {
                    consoleDemoInit();
                    static const s16 from[][3] = { { 122, 50, 62 }, { 120, 50, 80 }, { 110, 50, 60 }, { 64, 49, 21 }, { 129, 56, 61 }, { 90, 50, 168 } };
                    for (unsigned k = 0; k < sizeof(from) / sizeof(from[0]); k++)
                    {
                        int at[3] = { inttof32(from[k][0]), inttof32(from[k][1]), inttof32(from[k][2]) };
                        foe_flood(at);
                        int in = 0;
                        for (int c = 0; c < FOE_GRID * FOE_GRID; c++)
                            if (foe_dist[c] != 0xff && !foe_shut[c] && abs(foe_cx + (c % FOE_GRID - FOE_GRID / 2) * FOE_CELL - inttof32(177)) < inttof32(14)
                                && abs(foe_cz + (c / FOE_GRID - FOE_GRID / 2) * FOE_CELL - inttof32(70)) < inttof32(14) && foe_fy[c] > inttof32(40))
                                in++;
                        int nb[3] = { 0, 0, 0 }, ok = foe_near(k, nb);
                        printf("%3d %2d %3d: cells %4d room %3d ash %d\n near %d %d %d %d\n", from[k][0], from[k][1], from[k][2], foe_cells, in,
                               foe_reachable(177, 50, 70), ok, nb[0], nb[1], nb[2]);
                    }
                    while (1)
                        swiWaitForVBlank();
                }
#endif
#ifdef TEST_CULT_VIEW   // テスト用ビルド：種類 TEST_CULT_VIEW（foe_kind_cultist）の教徒をレオンの TEST_CULT_DIST 前に、彼の方を向けて
#ifndef TEST_CULT_DIST
#define TEST_CULT_DIST 35
#endif
        if (play_frames == 2)
        {
            int slot = ganados.count - 1;
            int xyz[3] = { f32toint(leon_pos[0] + sinLerp(leon_yaw) * TEST_CULT_DIST), f32toint(leon_feet) + 2,
                           f32toint(leon_pos[2] + cosLerp(leon_yaw) * TEST_CULT_DIST) };
            enemies_respawn(&ganados, slot, xyz, leon_yaw + DEGREES_IN_CIRCLE / 2);
            enemies_set_kind(&ganados, slot, &foe_kind_cultist[TEST_CULT_VIEW]);
            enemies_alert(&ganados, slot);
        }
#endif
#ifdef TEST_PLG_VIEW   // テスト用ビルド：3面のプラーガのガナード（種類18）をレオンのこれだけ前に、彼の方を向けて
        if (play_frames == 2)
        {
            int xyz[3] = { f32toint(leon_pos[0] + sinLerp(leon_yaw) * TEST_PLG_VIEW), f32toint(leon_feet) + 2,
                           f32toint(leon_pos[2] + cosLerp(leon_yaw) * TEST_PLG_VIEW) };
            enemies_respawn(&ganados, ENEMY_MAX - 1 < ganados.count ? ENEMY_MAX - 1 : ganados.count - 1, xyz, leon_yaw + DEGREES_IN_CIRCLE / 2);
            enemies_set_kind(&ganados, ENEMY_MAX - 1 < ganados.count ? ENEMY_MAX - 1 : ganados.count - 1, &foe_kind_ganado[18]);
        }
#endif
#ifdef TEST_DROP_CODE   // テスト用ビルド：6.7秒で、そのドロップコードが落とすものをレオンの20前に
        if (play_frames == 400)
        {
            Enemy fake = { 0 };
            fake.pos[0] = leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(20));
            fake.pos[2] = leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(20));
            ground_height(&stage_ground, fake.pos[0], fake.pos[2], leon_feet + STEP_UP, STEP_UP, &fake.feet);
            fake.drop = TEST_DROP_CODE;
            mission_drop(&fake);
        }
#endif
        int picked = drops_update(&drops, react == REACT_DEAD ? NULL : leon_at, take_item);
        take_item = 0;
        if (picked >= 0)
        {
            sfx_play(SFX_ITEM_GET, 120, 64);
            popup_value = drops.picked_value;
            if (picked == DROP_AMMO)
            {
                // 多機能弾丸：手に持っている銃に関係なく、1つの数に MULTI_DROP 発。
                popup_value = multi_ammo_gun(wep) * 1000 + MULTI_DROP;
                spare += MULTI_DROP;
            }
            else if (picked == DROP_MONEY)
                ptas += drops.picked_value;
            else if (picked == DROP_TREASURE)
            {
                ptas += drops.picked_value;
                treasures_found++;
                bonus_note(T_BONUS_JEWEL);
            }
            else if (herbs < HERBS_MAX)
                herbs++;
            popup_type = picked;
            popup = POPUP_FRAMES;
        }
        if (popup > 0)
            popup--;

        // ストーリーのイベント：エルヒガンテが初めてひざをつく（4面）、倒れたときの報酬、アシュリー発見
        // （5面：ミッションが置く所で待っていた）、ガラドールに出会う（8面）。
        if (mode == MODE_PLAY && react != REACT_DEAD)
        {
            int ev = -1;
            for (int i = 0; i < giant_count; i++)
            {
                if (giants[i].state == GIANT_KNEEL && !kneel_seen)
                {
                    kneel_seen = 1;
                    ev = CUT_GIANT_KNEEL;
                }
                if (giants[i].state == GIANT_DEAD && !(giant_paid >> i & 1))
                {
                    giant_paid |= 1 << i;
                    const CutScene *c = cut_find(cur_stage, CUT_GIANT_DEAD);
                    if (c && c->money)
                    {
                        ptas += c->money;
                        popup_type = DROP_MONEY;
                        popup_value = c->money;
                        popup = POPUP_FRAMES;
                        sfx_play(SFX_ITEM_GET, 120, 64);
                    }
                }
            }
            if (ash.on && ash_waiting && !ash.wait)
                ev = CUT_ASHLEY;
            ash_waiting = ash.on && ash.wait;
            if (gal_count && gals[0].on && !gal_seen && abs(gals[0].pos[0] - leon_pos[0]) < GAL_CUT_NEAR
                && abs(gals[0].pos[2] - leon_pos[2]) < GAL_CUT_NEAR && abs(gals[0].feet - leon_feet) < inttof32(16))
            {
                gal_seen = 1;
                ev = CUT_GARRADOR;
            }
            if (ev >= 0 && ((ev == CUT_GIANT_KNEEL && cur_stage == 3) || (ev == CUT_ASHLEY && cur_stage == 4) || (ev == CUT_GARRADOR && cur_stage == 7)))
            {
                if (ev_scene_own(cur_stage, ev))
                    foes_scene_played(&stage_foes[cur_stage], ev);
                if (cut_start(cur_stage, ev, MODE_PLAY, CUT_AFTER_NONE, 0))
                    mode = MODE_CUTIN;
            }

            // ミッションのイベントがくれたもの（foes_events()）、それからその場面、1つずつ。
            for (int g = 0; cur_stage != 0 && g < ev_gots; g++)
            {
                const EvItem *it = ev_item(ev_got[g]);
                if (!it)
                    continue;
                sfx_play(SFX_ITEM_GET, 120, 64);
                if (it->kind == EVI_TREASURE)
                {
                    ptas += it->price;
                    treasures_found++;
                bonus_note(T_BONUS_JEWEL);
                    popup_type = DROP_TREASURE;
                    popup_value = it->price;
                    popup = POPUP_FRAMES;
                }
                else if (it->kind == EVI_HERB)
                {
                    if (herbs < HERBS_MAX)
                        herbs++;
                    popup_type = DROP_HERB;
                    popup = POPUP_FRAMES;
                }
                else
                {
                    // （「'...' taken」、プレイ画面の上のメッセージの枠に、原作のとおり）
                    static char got_text[64];
                    static CutPage got_page;
                    static CutScene got_scene;
                    snprintf(got_text, sizeof(got_text), it->kind == EVI_FILE ? T_GOT_FILE : T_GOT_ITEM, it->name);
                    got_page = (CutPage){ -1, 0, got_text };
                    got_scene = (CutScene){ (u8)cur_stage, 255, 1, &got_page, 0 };
                    if (mode == MODE_PLAY)
                    {
                        msg = &got_scene;
                        msg_page = 0;
                        mode = MODE_MESSAGE;
                    }
                    else
                        vital_message(got_text, 180);
                }
            }
            ev_gots = 0;
            // 紋章の半分2つで六角形の紋章になる（ケースの Combine、彼の代わりに行う）。
            if (cur_stage != 0 && item_held(80) && item_held(81))
            {
                item_set(80, 0);
                item_set(81, 0);
                item_set(82, 1);
                vital_message(T_GOT_EMBLEM, 180);
            }
            if (cur_stage != 0 && mode == MODE_PLAY && ev_scenes > 0)
            {
                int s = ev_scene[0];
                ev_scenes--;
                memmove(ev_scene, ev_scene + 1, ev_scenes);
                const CutScene *c = cut_find(cur_stage, s);
                int pictures = 0;
                for (int k = 0; c && k < c->count; k++)
                    pictures |= c->page[k].pic >= 0;
                if (c && c->count && !pictures)   // （せりふだけ：プレイ画面の上に）
                {
                    msg = c;
                    msg_page = 0;
                    mode = MODE_MESSAGE;
                }
                else if (cut_start(cur_stage, s, MODE_PLAY, CUT_AFTER_NONE, 0))
                    mode = MODE_CUTIN;
            }
        }

        // ステージクリア：ガナードが全員倒れて動かず、レオンは生きている。
#ifdef TEST_NO_CLEAR   // テスト用ビルド：ステージは決してクリアにならない（倒れて残っているものを見るため）
        if (0)
#else
        if (mode == MODE_PLAY && clear_t < 0 && react != REACT_DEAD && (ganados.count > 0 || cur_stage != 0) && (FOES_ON(cur_stage) || giant_count > 0))
#endif
        {
            // 全員（待っている者も）倒れ、まだ倒れている途中の者もいない。
            int all_down = (cur_stage != 0 || enemies_left(&ganados) == 0) && giants_down() && garradors_down()
                           && (!ver.on || ver.state == VER_DEAD) && (cur_stage == 0 || foes_finished(&stage_foes[cur_stage]));
            if (cur_stage != 0 && mis_flag_clear)   // （ミッションの目的：そのフラグ0、たいていは出口に着いたこと）
                all_down = (mis_flags & 1) && ev_scenes == 0;
            for (int i = 0; cur_stage == 0 && i < ganados.count; i++)
                all_down &= ganados.e[i].state == ENEMY_DEAD || ganados.e[i].thrower;
#ifdef TEST_BOSS   // テスト用ビルド：彼がすぐ来る
            all_down |= boss_phase == 0;
#endif
            // 村が片付くとチェーンソー男が来る（原作のとおり、その場面のせりふの後）。彼が倒れるか、
            // BOSS_SURVIVE フレームたつと鐘が鳴る。
            if (all_down && boss_phase == 0 && cur_stage != 0)
            {
                boss_phase = 2;   // （チェーンソー男は1面のもの）
                clear_t = 0;
            }
            else if (all_down && boss_phase == 0)
            {
                cha_loaded = model_load(&cha_model, cha_bin, cha_bin_size) == 0;
                if (cha_loaded)
                {
                    int slot = 0;
                    while (slot < ganados.count - 1 && ganados.e[slot].state < ENEMY_DYING && ganados.e[slot].active)
                        slot++;
                    enemies_spawn_boss(&ganados, slot, boss_spawn, DEGREES_IN_CIRCLE / 2, &cha_model, &cha_anims);
                    boss_phase = 1;
                    boss_t = 0;
                    sfx_play(SFX_GANADO_SPOT, 127, 64);
                    if (cut_start(cur_stage, CUT_CHAINSAW, MODE_PLAY, CUT_AFTER_NONE, 0))
                        mode = MODE_CUTIN;
                }
                else
                    clear_t = 0;
            }
            else if (boss_phase == 1)
            {
                int boss_down = 0;
                for (int i = 0; i < ganados.count; i++)
                    boss_down |= ganados.e[i].boss && ganados.e[i].state == ENEMY_DEAD;
                if (boss_down || ++boss_t >= BOSS_SURVIVE)
                {
                    boss_phase = 2;
                    clear_t = 0;
                }
            }
        }
#ifdef TEST_CLEAR_AT   // テスト用ビルド：このフレームでミッションが終わる
        if (play_frames == TEST_CLEAR_AT && clear_t < 0 && mode == MODE_PLAY)
        {
            boss_phase = 2;
            clear_t = 0;
        }
#endif
        // 12面：エレベーターが来て、レオンはそれで逃げる。
        if (mode == MODE_PLAY && clear_t < 0 && react != REACT_DEAD && cur_stage == VER_STAGE
            && play_frames >= ELEVATOR_FRAMES)
        {
            clear_t = 0;
            elevator_note = ELEVATOR_NOTE;
        }
        if (elevator_note > 0 && mode == MODE_PLAY)
            elevator_note--;
        if (clear_t >= 0 && mode == MODE_PLAY)
        {
            clear_t++;
            if (clear_t >= BELL_FIRST && (clear_t - BELL_FIRST) % BELL_GAP == 0 && clear_t < BELL_FIRST + 3 * BELL_GAP)
                sfx_play(SFX_BELL, 127, 64);
            if (clear_t == CLEAR_END)
                pending = cut_start(cur_stage, CUT_CLEAR, MODE_RESULT, CUT_AFTER_NONE, 1) ? MODE_CUTIN : MODE_RESULT;
        }
        else if (mode == MODE_PLAY)
            play_frames++;
        if (obj_t > 0 && mode == MODE_PLAY)
            obj_t--;
        PROF(3);
        model_sync();
        if (stage.chunked)
        {
            while (GFX_STATUS & (1 << 27))   // （FIFO を通したジオメトリエンジン）
                ;
            stage_used = GFX_POLYGON_RAM_USAGE;   // ステージ自身の分（最初に送る）
        }
#ifdef TEST_GX   // ステージ自身の分
        if (GFX_POLYGON_RAM_USAGE > gx_stage_poly) gx_stage_poly = GFX_POLYGON_RAM_USAGE;
        if (GFX_VERTEX_RAM_USAGE > gx_stage_vert) gx_stage_vert = GFX_VERTEX_RAM_USAGE;
#endif
        PROF(4);
        // HUD を最初に：混み合った場面がハードウェアのポリゴンの上限を超えたとき、落とされるのは最後に
        // 送ったポリゴンで、HUD のものは決して落ちない。
        if (ctrl == CTRL_LEON && react != REACT_GIANT_CUT)   // （イベントの画面が覆う）
            draw_hud(leon_hp, ammo, hurt_flash, react == REACT_DEAD, popup_type, popup_value, popup);
        if (ctrl == CTRL_LEON && ash.on && !ash.wait && react != REACT_GIANT_CUT && hud_on)   // （彼が彼女を見つける前は無し）
            draw_ash_hud(ash.hp, ASHLEY_HP, ash.wait);
        if (scoping)
            draw_scope();
        // 完全に構えている間のレーザーサイト：HUD のすぐ後に。混み合ったフレーム（9面の2体の
        // ガラドール）ではほかのポリゴンが落ち、これは落ちない。
        if (ctrl == CTRL_LEON && aim_w == inttof32(1) && reload_frame < 0 && !scoping)
        {
            billboard_begin(look_eye, look_at);
            draw_line(muzzle, laser_end, RGB15(31, 2, 2), 10);
            if (laser_hit)
                draw_star(laser_end, floattof32(0.25), RGB15(31, 4, 4), 28, 11);
        }
        if (scope_was != scoping)
            set_fov(scoping ? SCOPE_FOV : CAM_FOV);
        scope_was = scoping;
        // はしごの段差が、登りで後ろへ運ばれる距離より近い（CLIP_CLIMB は16離れた段差に立てかけた
        // はしご用に作られている。5面のチャペルのはしごは壁に接している）：上がるにつれてその分外に
        // 描くので、体は壁の中でなく壁の所で止まる。
        int leon_draw[3] = { leon_pos[0], leon_pos[1], leon_pos[2] };
        if ((react == REACT_CLIMB_UP || react == REACT_CLIMB_DOWN) && act_obj >= 0 && objs.o[act_obj].kind == OBJ_LADDER)
        {
            const Obj *l = &objs.o[act_obj];
            int reach = l->end[0] - inttof32(3);
            int len = inttof32(leon_anims.frames[react_clip[react]]);
            int t = len > 0 ? divf32(react_frame, len) : inttof32(1);
            t = t > inttof32(1) ? inttof32(1) : t;
            if (react == REACT_CLIMB_DOWN)
                t = inttof32(1) - t;
            if (reach < CLIMB_BACK)
            {
                int shift = mulf32(CLIMB_BACK - (reach > 0 ? reach : 0), t);
                leon_draw[0] -= mulf32(l->dir[0], shift);
                leon_draw[2] -= mulf32(l->dir[1], shift);
            }
        }
        if (leon_anim != ANIM_OFF && !scoping)   // （スコープ越しでは彼は見えない）
        {
            model_draw(&leon, leon_draw, leon_yaw, cull_modes[cull] | POLY_ID(2) | POLY_FOG | (leon_fade < 31 ? POLY_ALPHA(leon_fade) : 0), (const int (*)[12])leon_pose);
            // ハンドガンは、出しているとき、抜き / しまい中は手がホルスターに届いてから、構えるときは腕が
            // 半分上がってから、彼の手にある。
            int in_hand = (gun_state == GUN_READY || (drawing && draw_frame >= DRAW_GRAB) || aim_w >= inttof32(1) / 2 || reload_frame >= 0) && !knifing;
            if (knifing)
                model_draw_held(&knife_model, leon_pos, leon_yaw, cull_modes[cull] | POLY_ID(3) | POLY_FOG, leon_pose[KNIFE_HAND]);
            if (in_hand)
                model_draw_held(&gun, leon_pos, leon_yaw, cull_modes[cull] | POLY_ID(3) | POLY_FOG, leon_pose[HAND_NODE]);
        }

        enemies_draw(&ganados, cull_modes[cull] | POLY_ID(4) | POLY_FOG, look_eye, enemy_show);
        if (ash.on)
            ashley_draw(&ash, cull_modes[cull] | POLY_ID(6) | POLY_FOG);
        for (int i = 0; i < giant_count; i++)
            giant_draw(&giants[i], cull_modes[cull] | POLY_ID(7) | POLY_FOG);
        for (int i = 0; i < gal_count; i++)
            garrador_draw(&gals[i], cull_modes[cull] | POLY_ID(7) | POLY_FOG);
        if (ver.on)
            verdugo_draw(&ver, cull_modes[cull] | POLY_ID(7) | POLY_FOG);
        objects_draw(&objs, look_eye, cull_modes[cull] | POLY_ID(5) | POLY_FOG);
        bolts_draw();
        billboard_begin(look_eye, look_at);
        objects_draw_fx(&objs);
        missiles_draw(&missiles);
        if (ver.on)
            verdugo_draw_fx(&ver, &spark);
        drops.ammo_gun = multi_ammo_gun(wep);
        drops_draw(&drops);

        // 完全に構えている間のレーザーサイト、マズルフラッシュ、最後の弾の着弾。
        if (rocket_on)
        {
            draw_star(rocket_pos, floattof32(0.8), RGB15(31, 24, 10), 26, 12);   // ロケットの炎
            draw_star(rocket_pos, floattof32(0.4), RGB15(31, 31, 26), 30, 13);
        }
        if (flash_timer > 0)
        {
            draw_star(muzzle, floattof32(0.9) - flash_timer * 0, RGB15(31, 27, 12), 26, 12);
            draw_star(muzzle, floattof32(0.45), RGB15(31, 31, 28), 30, 13);
            flash_timer--;
        }
        if (impact_timer > 0)
        {
            // 壁からの火花、ガナードからの血。
            draw_star(impact_pos, floattof32(0.15) * impact_timer, impact_blood ? RGB15(20, 0, 0) : RGB15(31, 22, 10),
                      2 + impact_timer * 2, 14);
            impact_timer--;
        }
        if (ctrl == CTRL_LEON)
        {
            // HUD の上の文字：持っている弾、拾ったばかりのアイテム、STAGE CLEAR。
            if (mode == MODE_PLAY)
            {
                // （持っている弾は下画面に。原作の上画面には出ない。）
                text_hide(SPARE_LINE);
                bonus_draw();
                if (popup > 0)
                {
                    char money[24];
                    if (popup_type == DROP_AMMO)
                        snprintf(money, sizeof(money), T_PICK_AMMO, popup_value % 1000);
                    else
                        snprintf(money, sizeof(money), popup_type == DROP_TREASURE ? T_PICK_TREASURE : T_PICK_MONEY, popup_value);
                    const char *s = popup_type == DROP_HERB ? T_PICK_HERB : money;
                    int rise = popup > POPUP_FRAMES - 8 ? (popup - (POPUP_FRAMES - 8)) * 2 : 0;
                    text_line(POPUP_LINE, 64, 135 + rise, TEXT_WHITE, s);
                }
                else
                    text_hide(POPUP_LINE);
                if (clear_t >= CLEAR_TEXT_AT)
                    text_big(CLEAR_LINE, 128 - text_width(T_COMPLETE), 70, TEXT_GOLD, T_COMPLETE);
                // 開始時にミッションの目的。来たときにチェーンソー男のせりふ（エンジンのうなり、画面に収まるよう
                // 2行で、それからレオンの一言）。
                if (boss_phase == 1 && boss_t < BOSS_LINES && !cut_find(cur_stage, CUT_CHAINSAW))
                {
                    text_line(OBJ_TITLE_LINE, 128 - text_width(T_BOSS_1) / 2, 70, TEXT_WHITE, T_BOSS_1);
                    text_line(OBJ_LINE, 128 - text_width(T_BOSS_1B) / 2, 84, TEXT_WHITE, T_BOSS_1B);
                    if (boss_t > BOSS_LINES / 2)
                        text_line(OBJ_HINT_LINE, 128 - text_width(T_BOSS_2) / 2, 102, TEXT_GOLD, T_BOSS_2);
                    else
                        text_hide(OBJ_HINT_LINE);
                }
                else if (obj_t > 0 && clear_t < CLEAR_TEXT_AT)
                {
                    char mt[24];
                    snprintf(mt, sizeof(mt), T_MISSION, stage_no(cur_stage));
                    static const char *const objectives[STAGE_COUNT] = { T_OBJ_1, T_OBJ_2, T_OBJ_3, T_OBJ_4, T_OBJ_5, T_OBJ_6, T_OBJ_7, T_OBJ_8, T_OBJ_9, T_OBJ_10, T_OBJ_11, T_OBJ_N };   // （各ミッションの目的：そのイベント）
                    const char *ob = objectives[cur_stage];
                    text_big(OBJ_TITLE_LINE, 128 - text_width(mt), 46, TEXT_GOLD, mt);
                    text_line(OBJ_LINE, 128 - text_width(ob) / 2, 82, TEXT_WHITE, ob);
                    if (cur_stage == VER_STAGE)
                        text_line(OBJ_HINT_LINE, 128 - text_width(T_ELEV_WAIT) / 2, 102, TEXT_GOLD, T_ELEV_WAIT);
                    else
                        text_hide(OBJ_HINT_LINE);
                }
                else if (elevator_note > 0)
                {
                    text_hide(OBJ_TITLE_LINE);
                    text_hide(OBJ_HINT_LINE);
                    text_line(OBJ_LINE, 128 - text_width(T_ELEV_HERE) / 2, 110, TEXT_WHITE, T_ELEV_HERE);
                }
                else
                {
                    text_hide(OBJ_TITLE_LINE);
                    text_hide(OBJ_LINE);
                    text_hide(OBJ_HINT_LINE);
                }
                // ここで A がすること（自由に動けて構えていないとき）。
                int which, act = react == REACT_NONE && aim_w == 0 && !knifing ? objects_action(&objs, leon_pos, leon_feet, leon_yaw, &which) : ACT_NONE;
                static const char *const prompts[] = { "", T_ACT_GATE, T_ACT_CRATE, T_ACT_UP, T_ACT_DOWN, T_ACT_DOOR };
                if (react == REACT_GIANT_CUT)
                    text_hide(PROMPT_LINE);
                else if (react == REACT_HELD)
                    text_line(PROMPT_LINE, 128 - text_width(T_ACT_STRUGGLE) / 2, 150, (tick / 8) % 2 ? TEXT_GOLD : TEXT_WHITE, T_ACT_STRUGGLE);
                else if (react == REACT_NONE && aim_w == 0 && jump_at_hand(cur_stage, leon_pos, leon_feet, leon_yaw))
                    text_line(PROMPT_LINE, 128 - text_width(T_ACT_JUMP) / 2, 150, TEXT_WHITE, T_ACT_JUMP);
                else if (react == REACT_NONE && aim_w == 0 && giant_at_hand(leon_pos))
                    text_line(PROMPT_LINE, 128 - text_width(T_ACT_CUT) / 2, 150, TEXT_WHITE, T_ACT_CUT);
                else if (react == REACT_NONE && aim_w == 0 && !knifing && enemies_kickable(&ganados, leon_pos, leon_yaw) >= 0)
                    text_line(PROMPT_LINE, 128 - text_width(T_ACT_KICK) / 2, 150, TEXT_WHITE, T_ACT_KICK);
                else if (react == REACT_NONE && aim_w == 0 && !knifing && cur_stage != 0 && ev_button >= 0 && clear_t < 0
                         && act != ACT_LADDER_UP && act != ACT_LADDER_DOWN)   // （はしごで：A で登る、先にその場面）
                {
                    const char *s = stage_foes[cur_stage].event[ev_button].ctg == EV_POINT ? T_ACT_EXAMINE : T_ACT_PICK;
                    text_line(PROMPT_LINE, 128 - text_width(s) / 2, 150, TEXT_WHITE, s);
                }
                else if (react == REACT_NONE && aim_w == 0 && !knifing && drops_in_reach(&drops, (const int[3]){ leon_pos[0], leon_feet, leon_pos[2] }) >= 0)
                    text_line(PROMPT_LINE, 128 - text_width(T_ACT_PICK) / 2, 150, TEXT_WHITE, T_ACT_PICK);
                else if (act != ACT_NONE)
                    text_line(PROMPT_LINE, 128 - text_width(prompts[act]) / 2, 150, TEXT_WHITE, prompts[act]);
                else
                    text_hide(PROMPT_LINE);
                if (medal_note > 0)
                {
                    char m[48];
                    if (objs.medals_hit == objs.medals)
                        snprintf(m, sizeof(m), T_MEDAL_ALL, MEDAL_REWARD);
                    else
                        snprintf(m, sizeof(m), T_MEDAL, objs.medals_hit, objs.medals);
                    text_line(MEDAL_LINE, 128 - text_width(m) / 2, 24, TEXT_GOLD, m);
                }
                else
                    text_hide(MEDAL_LINE);
            }
        }
        if (hurt_flash > 0)
            hurt_flash--;


        PROF(5);
        model_sync();   // （最後の文字の一覧を終えた）
#ifdef TEST_GX   // テスト用ビルド：ジオメトリの負荷、1秒の間のピーク（バイタル画面を参照）
        {
            while (GFX_STATUS & (1 << 27))   // FIFO を通したジオメトリエンジン
                ;
            int pc = GFX_POLYGON_RAM_USAGE, vc = GFX_VERTEX_RAM_USAGE;
            if (pc > gx_poly_peak) gx_poly_peak = pc;
            if (vc > gx_vert_peak) gx_vert_peak = vc;
            u32 st = GFX_CONTROL;
            if (st & (1 << 12)) gx_underflows++;   // レンダラーがいくつかの行で時間切れになった
            int rd = *(vu8 *)0x04000320;            // RDLINES_COUNT：バッファの空いている行の最小（0：行が欠けた）
            if (rd < gx_rdlines_min) gx_rdlines_min = rd;
            if (st & (1 << 13)) gx_overflows++;    // 上限を超えたポリゴンか頂点が落とされた
            GFX_CONTROL = st | (1 << 12) | (1 << 13);
        }
#endif
        if (stage.chunked && mode == MODE_PLAY)
        {
            // このフレームで実際に残ったもの：ステージの分はそれに従う、毎フレーム少しずつ。
            while (GFX_STATUS & (1 << 27))   // （FIFO を通したジオメトリエンジン）
                ;
            int used = GFX_POLYGON_RAM_USAGE;
            if (used >= POLY_HIGH)
            {
                // ステージは POLY_STAGE_MIN まで譲る。それを過ぎたら（あるいはステージがもともと小さく、6面の
                // 小屋のようにキャラクターでフレームが埋まるときは）、一番遠い敵を省く。
                if ((stage_budget <= POLY_STAGE_MIN || stage_used < POLY_STAGE_MIN) && enemy_show > ENEMY_SHOW_MIN)
                    enemy_show--;
                stage_budget -= (used - POLY_LOW) / 2 + 20;
            }
            else if (used < POLY_LOW)
            {
                if (enemy_show < ENEMY_MAX && used < POLY_LOW - 300)
                    enemy_show++;
                else
                    stage_budget += 30;
            }
            if (stage_budget < POLY_STAGE_MIN)
                stage_budget = POLY_STAGE_MIN;
        }
        // 1フレームにかかる時間：実機の DS では混み合った場面は数 vblank かかることがある（5面の
        // チャペルで18 fps、エミュレーターでは60のまま）。1 vblank より遅いフレームが0.5秒の大半を
        // 占めると、描くステージと敵の上限を段階的に下げる。全速で1秒続くとまた上げる。
        if (mode == MODE_PLAY)
        {
            static u32 last_vb;
            static int frames_n, slow_n;
            frames_n++;
            slow_n += vblank_count - last_vb > 1;
            last_vb = vblank_count;
            if (frames_n >= 30)
            {
                if (slow_n >= 15)
                {
                    time_cap = time_cap - 120 < TIME_CAP_MIN ? TIME_CAP_MIN : time_cap - 120;
                    if (enemy_cap > ENEMY_SHOW_MIN + 2)
                        enemy_cap--;
                }
                else if (slow_n == 0)
                {
                    time_cap = time_cap + 40 > 4000 ? 4000 : time_cap + 40;
                    if (enemy_cap < ENEMY_MAX)
                        enemy_cap++;
                }
                frames_n = slow_n = 0;
            }
            if (stage.chunked && stage_budget > time_cap)
                stage_budget = time_cap;
            if (enemy_show > enemy_cap)
                enemy_show = enemy_cap;
        }
        glFlush(0);
        swiWaitForVBlank();
        PROF(6);
        text_commit();
        vital_commit();   // （下画面のマップ：その回転、その上のスプライト）

        frames++;
        if (vblank_count - fps_mark >= 60)
        {
            fps = frames;
            frames = 0;
            fps_mark = vblank_count;
        }

        // 下画面：バイタル画面。枠に触れると選ぶ。ハーブはダブルタップで使い、体力を戻す。死んでいるときと
        // フリーカメラのときは無し（そのタッチのドラッグで見回す）。
#ifdef TEST_INF_AMMO   // テスト用ビルド：弾は決して足りなくならない
        if (spare < 20) spare = 20;
#endif
        Vital v = { leon_hp, LEON_HP, ammo, MAG_SIZE, spare, herbs, HERB_HEAL, (int)fps, weapons[wep].name, { 0, 0 }, { 0, 0 },
                    cur_stage, leon_pos[0], leon_pos[2], leon_yaw, ash.on, ash.pos[0], ash.pos[2], bottom_items };
        item_icon(weapons[wep].item, &v.gun_icon[0], &v.gun_icon[1]);   // （装備している銃のもの、ステータス画面と同じ）
        item_icon(AMMO_ITEM(wep), &v.ammo_icon[0], &v.ammo_icon[1]);
#ifdef TEST_SHOW_FOES   // テスト用ビルド：FPS の欄 = 出てきたエントリー x 100 + 入場したグループ（ビット）。ハーブ：立っている敵
        v.fps = foe_entered * 100 + foe_groups;
        v.herbs = foes_standing();
#ifdef TEST_SHOW_TIMER   // …ハーブ：代わりにミッションのタイマーの秒
        v.herbs = mis_timer / 60;
#endif
#ifdef TEST_SHOW_VB   // …ハーブ：電源が入ってからの vblank の秒
        v.herbs = vblank_count / 60;
#endif
        {
            int best = 9999;   // spare：一番近い者の距離（単位）。ammo：その高さ - レオンの高さ
            for (int i = 0; i < ganados.count; i++)
                if (ganados.e[i].active && ganados.e[i].state < ENEMY_DYING)
                {
                    int d = f32toint(sqrt64((u64)((s64)(ganados.e[i].pos[0] - leon_pos[0]) * (ganados.e[i].pos[0] - leon_pos[0]) + (s64)(ganados.e[i].pos[2] - leon_pos[2]) * (ganados.e[i].pos[2] - leon_pos[2]))));
                    if (d < best)
                    {
                        best = d;
                        v.ammo = f32toint(ganados.e[i].pos[0]);
                        v.spare = f32toint(ganados.e[i].pos[2]);
                    }
                }
            v.mag = best;
        }
#ifdef TEST_SHOW_TIMER   // …手持ちの弾：再生したイベント6..10（数字は1/0、6が先）、削除されたものは2を足す
        v.ammo = 0;
        for (int e = 6; e <= 10; e++)
            v.ammo = v.ammo * 10 + ev_bit(ev_played, e) + 2 * ev_bit(ev_deleted, e);
#endif
#endif
#ifdef TEST_SHOW_HEAP   // テスト用ビルド：FPS の欄 = malloc() がまだくれる一番大きいブロック、KB
        if (play_frames == 120)
        {
            int lo = 0, hi = 4096;
            while (lo < hi)
            {
                int mid = (lo + hi + 1) / 2;
                void *p = malloc((size_t)mid * 1024);
                if (p) { free(p); lo = mid; } else hi = mid - 1;
            }
            heap_kb = lo;
        }
        v.fps = heap_kb / 10;   // （数十 KB：欄は3桁を出す）
#endif
#ifdef TEST_SHOW_KNIFE
        v.fps = knife_probe;
        v.spare = v_knife2;
#endif
#ifdef TEST_SHOW_TILT   // テスト用ビルド：FPS の欄 = 狙いの傾き（パーセント、上100、下 -100）
        v.fps = aim_tilt * 100 / 4096;
#endif
#ifdef TEST_SHOW_REACT   // テスト用ビルド：FPS の欄 = レオンの反応 x 100 + これまでのつかみの回数
        {
            static int grabs, was_held;
            grabs += react == REACT_HELD && !was_held;
            was_held = react == REACT_HELD;
            v.fps = react * 100 + grabs;
        }
#endif
#ifdef TEST_PROF
        // 直近1秒のピークの時間（0.1 ms）：ポーズ / レーザー、敵、更新の残り、ステージの DMA 待ち、
        // キャラクターの描画。
        v.hp = prof_peak[1]; v.ammo = prof_peak[2]; v.spare = prof_peak[3]; v.herbs = prof_peak[4]; v.fps = prof_peak[5]; v.mag = prof_peak[6];
        if (tick % 60 == 0)
            memset(prof_peak, 0, sizeof(prof_peak));
#endif
#ifdef TEST_GX   // FPS の欄：ポリゴン（2048のうち）。spare：頂点（6144のうち）。herbs：ポリゴンを落とした
        // フレーム数（直近1秒）
        // ammo / magazine（Loaded a/b）：ステージ自身のポリゴン / 頂点
        v.fps = gx_poly_peak; v.spare = gx_vert_peak; v.herbs = gx_overflows * 100 + gx_underflows; v.ammo = gx_stage_poly; v.mag = enemy_show;
        if (tick % 60 == 0)
            gx_poly_peak = gx_vert_peak = gx_overflows = gx_underflows = gx_stage_poly = gx_stage_vert = 0, gx_rdlines_min = 99;
#endif
#ifdef TEST_ASH_VIEW
        v.fps = ash.clip;
#endif
#ifdef TEST_ELG_VIEW
        v.fps = giant.clip;
#endif
#ifdef TEST_GAL_VIEW
        v.fps = gal.clip;
#endif
#ifdef TEST_VER_VIEW
        v.fps = ver.clip;
#endif
#ifdef TEST_SHOW_VER   // ヴェルデューゴ：体力 * 10 + 状態。spare = 立っているタンク
        v.fps = ver.hp / 10 * 10 + ver.state;
        {
            int tanks = 0;
            for (int i = 0; i < objs.count; i++)
                tanks += objs.o[i].kind == OBJ_BARREL && objs.o[i].alive;
            v.spare = tanks;
#ifdef TEST_SHOW_TANKS
            v.fps = tanks * 100 + ver.state * 10 + (ver.hp * 9 / 20000);
#endif
        }
#endif
        if (wep_note > 0 && mode == MODE_PLAY)
        {
            if (--wep_note)
                text_line(WEP_LINE, 256 - 8 - text_width(weapons[wep].name), 8, TEXT_GOLD, weapons[wep].name);
            else
                text_hide(WEP_LINE);
        }
#ifdef AUTOPILOT
        if (giant_count)
        {
            int tx = giants[0].pos[0] - leon_pos[0], tz = giants[0].pos[2] - leon_pos[2];
            int want = (int)(atan2f((float)tx, (float)tz) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
            ap_aim_err = (s16)(want - leon_yaw);
        }
#endif
#ifdef TEST_SHOW_GIANT   // 1体目の巨人：状態、その状態のフレーム数、斬った回数、残りの弾
        {
            char gs[56];
            snprintf(gs, sizeof(gs), "s%d c%d h%d L%d g%d o%d r%d w%d d%d", giant.state, giant.cuts, giant.hp, giant.life, laser_giant, laser_obj, laser_drop, laser_hit,
                     (int)(sqrt64((u64)((s64)(giant.pos[0] - leon_pos[0]) * (giant.pos[0] - leon_pos[0]) + (s64)(giant.pos[2] - leon_pos[2]) * (giant.pos[2] - leon_pos[2]))) >> 12));
            text_line(0, 4, 4, TEXT_GOLD, gs);
        }
#endif
#ifdef TEST_SHOW_POS   // レオンが立っている所
        {
            char pos_s[32];
            snprintf(pos_s, sizeof(pos_s), "%d %d %d r%d a%d %d", leon_pos[0] >> 12, leon_feet >> 12, leon_pos[2] >> 12, react, ash.ladder, ash.feet >> 12); snprintf(pos_s + strlen(pos_s), 12, " %d,%d", ash.pos[0] >> 12, ash.pos[2] >> 12);
            text_line(0, 4, 4, TEXT_GOLD, pos_s);
#ifdef TEST_SHOW_KNIFE   // …そして一番近い木箱：番号、場所、残っているか
            {
                int bi = -1, bd = 1 << 30;
                for (int i = 0; i < objs.count; i++)
                    if (objs.o[i].kind == OBJ_CRATE)
                    {
                        int d = abs(objs.o[i].pos[0] - leon_pos[0]) + abs(objs.o[i].pos[2] - leon_pos[2]);
                        if (d < bd) { bd = d; bi = i; }
                    }
                if (bi >= 0)
                {
                    snprintf(pos_s, sizeof(pos_s), "k%d %d %d %d a%d p%d %d", bi, objs.o[bi].pos[0] >> 12, objs.o[bi].pos[1] >> 12, objs.o[bi].pos[2] >> 12, objs.o[bi].alive, knife_probe, v_knife2);
                    text_line(2, 4, 32, TEXT_GOLD, pos_s);
                }
            }
#endif
#ifdef TEST_SHOW_CAM   // …そしてカメラ：その場所、彼を透けさせる度合い
            snprintf(pos_s, sizeof(pos_s), "c %d %d %d y%d f%d", follow_eye[0] >> 12, follow_eye[1] >> 12, follow_eye[2] >> 12, leon_yaw * 360 / DEGREES_IN_CIRCLE, leon_fade);
            text_line(1, 4, 18, TEXT_GOLD, pos_s);
#endif
        }
#endif
#ifdef TEST_SHOW_DROP   // 最初の宝：レオンへの水平の距離（単位）x10 + 高い所か。spare = レオンの足元からの高さ
        for (int i = 0; i < DROPS_MAX; i++)
            if (drops.d[i].active && drops.d[i].type == DROP_TREASURE)
            {
                int dx = (drops.d[i].pos[0] - leon_pos[0]) >> 12, dz = (drops.d[i].pos[2] - leon_pos[2]) >> 12;
                v.fps = (int)sqrtf((float)(dx * dx + dz * dz)) * 10 + drops.d[i].aloft;
                v.spare = (drops.d[i].pos[1] - leon_feet) >> 12;
                break;
            }
#endif
#ifdef TEST_SHOW_GAL   // ガラドール：体力 * 10 + 状態
        v.fps = gal.hp * 10 + gal.state;
        v.spare = laser_gal * 10 + laser_gal_back;
#endif
#ifdef TEST_SHOW_THROW   // これまでに投げた数 x100 + 最初の投げる者の状態 x10 + 飛んでいる投げ物
        {
            int fl = 0, th = -1;
            for (int i = 0; i < MISSILES_MAX; i++) fl += missiles.m[i].t >= 0;
            for (int i = 0; i < ganados.count; i++) if (ganados.e[i].thrower) { th = i; break; }
            v.fps = throws_seen * 100 + (th >= 0 ? ganados.e[th].state * 10 : 90) + fl;
            v.spare = th >= 0 ? ganados.e[th].cooldown : -1;
            v.herbs = th >= 0 ? ganados.e[th].active : -1;
        }
#endif
#ifdef TEST_SHOW_SPOTTED   // FPS の欄：レオンに気づいた数 x 100 + ステージにいる数
        {
            int seen = 0, in = 0;
            for (int i = 0; i < ganados.count; i++)
                if (ganados.e[i].active && ganados.e[i].state < ENEMY_DYING)
                {
                    in++;
                    seen += ganados.e[i].spotted;
                }
            v.fps = seen * 100 + in;
        }
#endif
#ifdef TEST_SHOW_HP   // FPS の欄：最後に撃った者の体力。spare：射撃。herbs：よろめき。magazine：倒した数
        v.fps = test_shot_foe >= 0 ? ganados.e[test_shot_foe].hp : 0;
        v.spare = test_shots;
        v.herbs = test_staggers;
        v.mag = test_downs;
#endif
#ifdef TEST_SHOW_GIANT   // 巨人：残りの弾 * 100 + 状態 * 10 + 斬った回数
        v.fps = giant.hp * 100 + giant.state * 10 + giant.cuts;
        v.herbs = giant_count;   // 2体目：ハーブの枠 = 体数、spare = その残りの弾 x100 + 状態 x10 + 斬った回数
        v.spare = giants[1].hp * 100 + giants[1].state * 10 + giants[1].cuts;
#endif
#ifdef TEST_KNIFE_VIEW
        v.fps = (int)((vblank_count / 120) % 7);
#endif
        int act = vital_update(&v, down, tick, ctrl == CTRL_LEON && mode == MODE_PLAY && react != REACT_DEAD);
        if (act == VITAL_SELECT)
            sfx_play(SFX_MENU, 70, 64);
        else if (act == VITAL_USE)
        {
            if (herbs == 0)
            {
                sfx_play(SFX_EMPTY, 110, 64);
                vital_message(T_NONE_LEFT, 90);
            }
            else if (leon_hp >= LEON_HP)
            {
                sfx_play(SFX_EMPTY, 110, 64);
                vital_message(T_FULL, 90);
            }
            else
            {
                herbs--;
                leon_hp = leon_hp + HERB_HEAL > LEON_HP ? LEON_HP : leon_hp + HERB_HEAL;
                sfx_play(SFX_ITEM_GET, 110, 64);
                vital_message(T_USED, 90);
            }
        }
    }
    return 0;
}
