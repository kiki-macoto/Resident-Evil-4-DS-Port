#ifndef STAGES_H__
#define STAGES_H__

#include <nds.h>

#define STAGE_COUNT  12

typedef struct {
    u8 haze[3];
    int treasures;
    s16 treasure[2][3];
    int tanks;
    s16 tank[4][3];
    s16 fog[2];
} StageInfo;

extern const StageInfo stage_info[STAGE_COUNT];

enum { FOE_GANADO = 2, FOE_CULTIST, FOE_CHAINSAW, FOE_GARRADOR, FOE_GIANT, FOE_VERDUGO };
typedef struct {
    u8 wave, type;
    s16 pos[3];
    u8 dir;
    s8 drop;
    u8 variant;
    u8 rec;
    u8 entries;
    u8 ai;
} StageFoe;
enum { EV_START, EV_END, EV_AUTO, EV_RECORD, EV_OBJECT, EV_POINT };
enum { COND_NONE, COND_TIME, COND_KILLS, COND_RECORD_DOWN, COND_EVENT, COND_FLAG, COND_DESTROYED,
       COND_ITEM, COND_WALL };
enum { GET_AUTO, GET_TOUCH, GET_BUTTON, GET_WITH_ASHLEY };
typedef struct { u8 ctg; s8 trg; u8 cond, value; s16 reach; u8 first, count; u8 get, again; s8 script[2]; } StageEvent;
enum { OP_ITEM = 203, OP_GIVE_ITEM = 204, OP_TIMER = 205, OP_FLAG = 206, OP_DELETE = 208, OP_GROUP = 210 };
typedef struct { u8 op; s16 a, b; } StageOp;
typedef struct { u16 hp, flinch_max; u8 dmg[3], flinch[3]; u8 ctg; u8 crit, crit_mul[4]; u8 conc; s16 duty;
                 s32 look_dis, look_rot, search_dis, search_rot; u16 width;
                 u8 leader;
                 u8 plaga;
               } FoeKind;
enum { CRIT_KNIFE, CRIT_HANDGUN, CRIT_MAGNUM, CRIT_RIFLE, CRIT_NONE };
enum { PART_HEAD, PART_BODY, PART_LEGS };
extern const FoeKind foe_kind_ganado[], foe_kind_cultist[], foe_kind_chainsaw[];
extern const int foe_kind_ganado_count, foe_kind_cultist_count, foe_kind_chainsaw_count;
typedef struct {
    u8 kind;
    s16 pos[3];
    s8 drop;
    u8 dir;
} StageThing;
typedef struct {
    int count;
    const StageFoe *foe;
    s16 start[3];
    u8 start_dir;
    int ally;
    s16 ally_pos[3];
    int things;
    const StageThing *thing;
    int events;
    const StageEvent *event;
    const StageOp *op;
    int points;
    const s16 (*point)[3];
    int walls;
    const s16 (*wall)[6];
} StageFoes;

extern const StageFoes stage_foes[STAGE_COUNT];
extern const s16 stage_treasure_item[STAGE_COUNT][2];

typedef struct { u8 item, stat, level; u16 value; int price; } ShopTune;
extern const ShopTune shop_tune[];
extern const int shop_tune_count;

#endif
