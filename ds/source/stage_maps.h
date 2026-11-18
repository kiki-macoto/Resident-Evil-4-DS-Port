// ステージのマップの平面図（stage_maps.c、map_stg_NN.mcms から）：ワールド単位（x, z）の平らな
// 三角形、グループ分け（0 床、1 上の階の床、2-3 その上に立つもの）。下画面のマップ用（vital.c）。
#ifndef STAGE_MAPS_H__
#define STAGE_MAPS_H__

#include <nds.h>

typedef struct { u8 group; s16 p[6]; } MapTri;   // x0, z0, x1, z1, x2, z2
typedef struct { const MapTri *tri; int count; } StageMap;

extern const StageMap stage_maps[12];

#endif
