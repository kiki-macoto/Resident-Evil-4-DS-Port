#ifndef STAGE_MAPS_H__
#define STAGE_MAPS_H__

#include <nds.h>

typedef struct { u8 group; s16 p[6]; } MapTri;
typedef struct { const MapTri *tri; int count; } StageMap;

extern const StageMap stage_maps[12];

#endif
