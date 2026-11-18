#ifndef CUTINS_H
#define CUTINS_H

#include <nds.h>

enum { WHO_NARRATOR, WHO_LEON, WHO_ASHLEY, WHO_LUIS, WHO_SADDLER };

typedef struct
{
    s8 pic;
    u8 who;
    const char *text;
} CutPage;

typedef struct
{
    u8 stage;
    u8 scene;
    u8 count;
    const CutPage *page;
    int money;
} CutScene;

extern const CutScene cut_scenes[];
extern const int cut_scene_count;

#define CUT_START      0
#define CUT_CLEAR      9
#define CUT_CHAINSAW   10
#define CUT_GIANT_UP   11
#define CUT_GIANT_KNEEL 13
#define CUT_GIANT_DEAD 14
#define CUT_ASHLEY     10
#define CUT_GARRADOR   16
#define CUT_VERDUGO    10
#define CUT_CHANDELIER 17

static inline const CutScene *cut_find(int stage, int scene)
{
    for (int i = 0; i < cut_scene_count; i++)
        if (cut_scenes[i].stage == stage && cut_scenes[i].scene == scene)
            return &cut_scenes[i];
    return NULL;
}

#endif
