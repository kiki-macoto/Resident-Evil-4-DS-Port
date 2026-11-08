// Per-stage data (see stages.c). The stage models and ground live in NitroFS as
// stageNN.bin / groundNN.bin (re4zeebo -DsModel / -DsGround, see the Makefile).
#ifndef STAGES_H__
#define STAGES_H__

#include <nds.h>

#define STAGE_COUNT  12

typedef struct {
    u8 haze[3];                    // 8-bit RGB
    int treasures;                 // treasures (kind 07): picked up, or shot down from up high
    s16 treasure[2][3];
    int tanks;                     // liquid nitrogen tanks (kind 06, stage 12)
    s16 tank[4][3];
} StageInfo;

extern const StageInfo stage_info[STAGE_COUNT];

// The enemies of each mission (stage_foes.c): the mission file's enemy records, in waves (a
// wave comes in when the ones before it are down), each on a spawn point of the stage data.
enum { FOE_GANADO = 2, FOE_CULTIST, FOE_CHAINSAW, FOE_GARRADOR, FOE_GIANT, FOE_VERDUGO };
typedef struct {
    u8 wave, type;                 // FOE_*
    s16 pos[3];                    // world x, y (picks the floor), z
    u8 dir;                        // facing, 256 to a turn
    s8 drop;                       // what it leaves when it falls (mission_drop() in main.c)
} StageFoe;
// The mission's objects: doors (kind 01, on their hinge), explosive barrels (03), breakable
// objects (04: drops the item it names), bells (09, the point they hang from).
typedef struct {
    u8 kind;
    s16 pos[3];
    s8 drop;
    u8 dir;                        // facing, 256 to a turn
} StageThing;
typedef struct {
    int count;
    const StageFoe *foe;
    s16 start[3];                  // Leon's start (the record of type 0) and facing
    u8 start_dir;
    int ally;                      // Ashley is in this mission (record of type 1), there
    s16 ally_pos[3];
    int things;
    const StageThing *thing;
} StageFoes;

extern const StageFoes stage_foes[STAGE_COUNT];

// One level of a gun's upgrade (shop_tune.c): item (4 handgun .. 8 TMP), stat (0 firepower,
// 3 capacity, 5 firing speed, 6 reload speed), level 1.., its value x 10, its price.
typedef struct { u8 item, stat, level; u16 value; int price; } ShopTune;
extern const ShopTune shop_tune[];
extern const int shop_tune_count;

#endif
