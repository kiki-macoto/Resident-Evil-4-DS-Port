// ステージごとのデータ（stages.c を参照）。ステージのモデルと地面は NitroFS に stageNN.bin /
// groundNN.bin としてある（re4zeebo -DsModel / -DsGround、Makefile を参照）。
#ifndef STAGES_H__
#define STAGES_H__

#include <nds.h>

#define STAGE_COUNT  12

typedef struct {
    u8 haze[3];                    // 8ビットの RGB
    int treasures;                 // 宝（種類07）：拾うか、高い所から撃ち落とす
    s16 treasure[2][3];
    int tanks;                     // 液体窒素のタンク（種類06、12面）
    s16 tank[4][3];
    s16 fog[2];                    // 霧の始まり / 終わり（ワールド単位：ミッションファイルの FogDis /256）
} StageInfo;

extern const StageInfo stage_info[STAGE_COUNT];

// 各ミッションの敵（stage_foes.c）：ミッションファイルの敵のレコード、グループ分け（グループは
// ミッションのイベントで入場し、グループ0は最初から）、それぞれステージのデータの出現地点に。
enum { FOE_GANADO = 2, FOE_CULTIST, FOE_CHAINSAW, FOE_GARRADOR, FOE_GIANT, FOE_VERDUGO };
typedef struct {
    u8 wave, type;                 // そのグループ（GrpNo）。FOE_*
    s16 pos[3];                    // ワールドの x, y（床を選ぶ）, z
    u8 dir;                        // 向き、1回転 256
    s8 drop;                       // 倒れたときに落とすもの（main.c の mission_drop()）
    u8 variant;                    // その種類：foe_kind_ganado / _cultist / _chainsaw の番号
    u8 rec;                        // ミッションファイルでのそのレコードの番号（イベントがそれを指す）
    u8 entries;                    // 全部でそこから何人来るか、1人ずつ（EntryMax）
    u8 ai;                         // 最初の AI モード（FstAiId：1 レオンを待つ、3 / 4 歩き
                                   // 回る、5 降りてくる、6 / 7 すぐ彼に向かう）
} StageFoe;
// ミッションのイベント（GM_CtrlMain_Event）：何がそれを起こし、そのスクリプトが何をするか
// （ops[first] から、count 個）。ctg：EV_START .. EV_POINT。trg：それが依存するレコード、物
// （thing の番号）、イベント地点。cond、value：COND_*。
enum { EV_START, EV_END, EV_AUTO, EV_RECORD, EV_OBJECT, EV_POINT };
enum { COND_NONE, COND_TIME, COND_KILLS, COND_RECORD_DOWN, COND_EVENT, COND_FLAG, COND_DESTROYED,
       COND_ITEM, COND_WALL };
// get：レオンがそれを起こすやり方（GM_CtrlMain_Event::_Check_EveGetCond）：GET_*。again：毎回
// （そうでなければ1回）。script：それが再生するミッションのスクリプト（cutins.c の場面）、-1 なし。
enum { GET_AUTO, GET_TOUCH, GET_BUTTON, GET_WITH_ASHLEY };
typedef struct { u8 ctg; s8 trg; u8 cond, value; s16 reach; u8 first, count; u8 get, again; s8 script[2]; } StageEvent;
// OP_TIMER：GM_MisConduct::Set_LimitTime（a 秒、b その種類）：イベントの時間条件が数える時間を
// 0からやり直し、a まで進める。
enum { OP_ITEM = 203, OP_GIVE_ITEM = 204, OP_TIMER = 205, OP_FLAG = 206, OP_DELETE = 208, OP_GROUP = 210 };
typedef struct { u8 op; s16 a, b; } StageOp;
// 敵の種類、原作の ch_param_N.bin から（re4zeebo/gen_stage_foes.ps1 を参照）：体力、よろめく
// ひるみの合計、部位ごと（[0] 頭、[1] 胴、[2] 脚）の、弾の火力のうち体力から引き、ひるみの合計に
// 足すパーセント。火力：ハンドガン 100。
// ctg：装備のカテゴリ（GM_Data_ChP_Info EqpCtg。ガナードは0 素手、13 手斧、14 フォーク、19 重い者）、
// モーション表でその攻撃も選ぶ。
// crit：頭のクリティカルの確率（x 武器のクリティカル率 / 100）、crit_mul：そのときのダメージの
// 倍率、武器ごと：ナイフ、ハンドガン、マグナム、ライフル（1：なし）。
// conc、duty：その敵意（GM_CtrlSub_ChAi::_Set_UpDateParam）は1ティックに conc 増え（1000まで）、
// どれかの敵が攻撃している間は先に duty 減る。1000で攻撃できる（モーションの条件 EnmityMax）。
// look_*、search_*：何に気づくか（体のレコード +0x4c..：LookDis、LookRotR、SearchDis、SearchRotR、
// 原作の単位。角度は1回転 65536、片側）。width：その MoveCol_R（GM_CtrlSub_ChAi）。
typedef struct { u16 hp, flinch_max; u8 dmg[3], flinch[3]; u8 ctg; u8 crit, crit_mul[4]; u8 conc; s16 duty;
                 s32 look_dis, look_rot, search_dis, search_rot; u16 width;
                 u8 leader;   // リーダー：レオンを見た者が全員に彼を知らせる（enemies_alert() を参照）
                 u8 plaga;    // 首からプラーガが出て、頭が無い（WithPlgFlg：3面のもの。enemy.c）
               } FoeKind;
enum { CRIT_KNIFE, CRIT_HANDGUN, CRIT_MAGNUM, CRIT_RIFLE, CRIT_NONE };
enum { PART_HEAD, PART_BODY, PART_LEGS };
extern const FoeKind foe_kind_ganado[], foe_kind_cultist[], foe_kind_chainsaw[];
extern const int foe_kind_ganado_count, foe_kind_cultist_count, foe_kind_chainsaw_count;
// ミッションの物：扉（種類01、ちょうつがいの位置）、爆発する樽（03）、壊せる物（04：名前のアイテムを
// 落とす）、鐘（09、吊るされている地点）。
typedef struct {
    u8 kind;
    s16 pos[3];
    s8 drop;
    u8 dir;                        // 向き、1回転 256
} StageThing;
typedef struct {
    int count;
    const StageFoe *foe;
    s16 start[3];                  // レオンのスタート（タイプ0のレコード）と向き
    u8 start_dir;
    int ally;                      // アシュリーはこのミッションにいる（タイプ1のレコード）、そこに
    s16 ally_pos[3];
    int things;
    const StageThing *thing;
    int events;
    const StageEvent *event;
    const StageOp *op;
    int points;                    // イベント地点（ワールドの x, y, z）
    const s16 (*point)[3];
    int walls;                     // イベントの壁：x0, z0, x1, z1, 下端, 上端
    const s16 (*wall)[6];
} StageFoes;

extern const StageFoes stage_foes[STAGE_COUNT];
// 各ステージの宝（stage_info.treasure、順に）がくれるもの：アイテムの ID（宝の物、種類07に付いた
// イベントがくれる）、-1 なし。
extern const s16 stage_treasure_item[STAGE_COUNT][2];

// 銃の改造の1段階（shop_tune.c）：アイテム（4 ハンドガン .. 8 TMP）、性能（0 火力、3 装弾数、
// 5 連射速度、6 リロード速度）、レベル 1..、その値 x 10、その値段。
typedef struct { u8 item, stat, level; u16 value; int price; } ShopTune;
extern const ShopTune shop_tune[];
extern const int shop_tune_count;

#endif
