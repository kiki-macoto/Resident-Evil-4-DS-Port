// ミッションの物語のシーン（cutins.c、re4zeebo/gen_cutins.ps1 が書き出す）：
// 絵（nitro:/eveNN_k.bin、256 x 128、-1 なし）と、3行までの文（間は '\n'）のページ。
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
    u8 stage;   // 0始まり
    u8 scene;   // ミッションのスクリプトのシーン：0 始まり、9 クリア、ほかはイベント
    u8 count;
    const CutPage *page;
    int money;  // シーンでもらえる PTAS（ボスの報酬）
} CutScene;

extern const CutScene cut_scenes[];
extern const int cut_scene_count;

// イベントのシーン番号。
#define CUT_START      0
#define CUT_CLEAR      9
#define CUT_CHAINSAW   10   // 1面
#define CUT_GIANT_UP   11   // 4面と11面：エルヒガンテの背中のレオン
#define CUT_GIANT_KNEEL 13  // 4面
#define CUT_GIANT_DEAD 14   // 4面：報酬
#define CUT_ASHLEY     10   // 5面：アシュリー発見
#define CUT_GARRADOR   16   // 8面
#define CUT_VERDUGO    10   // 12面
#define CUT_CHANDELIER 17   // 5面：シャンデリアにつかまって身廊を渡る

static inline const CutScene *cut_find(int stage, int scene)
{
    for (int i = 0; i < cut_scene_count; i++)
        if (cut_scenes[i].stage == stage && cut_scenes[i].scene == scene)
            return &cut_scenes[i];
    return NULL;
}

#endif
