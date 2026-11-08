// The missions' story scenes (cutins.c, written by re4zeebo/gen_cutins.ps1): pages of a
// picture (nitro:/eveNN_k.bin, 256 x 128; -1 none) and up to 3 lines of text ('\n' between).
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
    u8 stage;   // 0-based
    u8 scene;   // the mission script's scene: 0 the start, 9 the clear, others events
    u8 count;
    const CutPage *page;
    int money;  // PTAS the scene gives (a boss's reward)
} CutScene;

extern const CutScene cut_scenes[];
extern const int cut_scene_count;

// Scene numbers of the events.
#define CUT_START      0
#define CUT_CLEAR      9
#define CUT_CHAINSAW   10   // stage 1
#define CUT_GIANT_UP   11   // stages 4 and 11: Leon on El Gigante's back
#define CUT_GIANT_KNEEL 13  // stage 4
#define CUT_GIANT_DEAD 14   // stage 4: the reward
#define CUT_ASHLEY     10   // stage 5: Ashley found
#define CUT_GARRADOR   16   // stage 8
#define CUT_VERDUGO    10   // stage 12
#define CUT_CHANDELIER 17   // stage 5: Leon swings across the nave on the chandelier

static inline const CutScene *cut_find(int stage, int scene)
{
    for (int i = 0; i < cut_scene_count; i++)
        if (cut_scenes[i].stage == stage && cut_scenes[i].scene == scene)
            return &cut_scenes[i];
    return NULL;
}

#endif
