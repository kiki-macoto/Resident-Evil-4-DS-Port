// re4zeebo/DsExport.cs が書き出す RDS1 のモデル形式の読み込みと描画。
#ifndef MODEL_H__
#define MODEL_H__

#include <nds.h>

#define MODEL_MAX_TEXTURES 32
#define MODEL_MAX_DRAWS    1024
#define MODEL_MAX_NODES    28   // ボーンの行列はスタックのスロット slot_base..30 にある

typedef struct {
    u16 texture;        // Model.tex_names の番号、0xFFFF = テクスチャなし
    u8 transparent;
    u8 two_sided;       // 両面から描く（原作のマテリアルの指定：中から見る壁）
    u32 triangles;
    const u32 *list;    // パックした GX のリスト、list[0] = ワード数
    int chunk[4];       // 分割したモデル：中心 x, y, z と半径（f32、ワールド空間）
    u32 list_at;        // …そして Model.async_list で次の描画が始まる所
} ModelDraw;

typedef struct {
    int center[3];      // f32、モデル空間
    int shift;          // 頂点は 2^shift で割って保存している
    int bbox_min[3], bbox_max[3];
    int ground[3];      // f32、モデル空間
    u32 triangles;
    int tex_count, draw_count;
    int tex_names[MODEL_MAX_TEXTURES];
    ModelDraw *draws;       // draw_count 個
    int chunked;            // ステージ、場所ごとに分割：model_draw_near()

    // スキン付きのモデルだけ（それ以外は node_count == 0）
    int node_count, slot_base;
    s16 parent[MODEL_MAX_NODES];
    int bind_local[MODEL_MAX_NODES][12];   // f32：3x3 の回転の行、それから平行移動

    // model_draw_async() で描く静的なモデル：全部の描画を1つの GX のリストに。
    u32 *async_list;
    size_t async_words;
    u32 async_fmt;
} Model;

// ファイルを解析してテクスチャを転送する。成功で0を返す。
int model_load(Model *m, const void *data, size_t size);

// キャラクターの視野カリング：model_set_view() がフレームのカメラを受け取り、view_visible() は
// pos のまわりの半径 r（f32）の体が画面に入りうるか（上から見て）を返す。
#define VIEW_FAR inttof32(260)
void model_set_view(const int eye[3], const int at[3]);
int view_visible(const int pos[3], int radius);

// モデルのテクスチャ（VRAM）を解放する：一部の時間だけ要るモデル用。
void model_free(Model *m);

// ワールドの位置 pos（f32）に、yaw（libnds の角度の単位）で回してモデルを描く。
// poly_fmt は全部の描画のポリゴン属性に OR する（カリングのモードなど）。
// pose：スキン付きのモデルでは、ノードごとに1つのグローバルな 4x3 の行列（bind_local と同じ配置）。
// NULL ならバインドポーズを描く。静的なモデルでは、pose[0]（NULL でなければ）を親の変換として
// かける。たとえばこのモデルを持つキャラクターの手のノード。
void model_draw(const Model *m, const int pos[3], int yaw, u32 poly_fmt, const int (*pose)[12]);

// 手に持つ物（武器）を描く：hand は手のノードのグローバルな行列（ポーズと同じ）。物が自分の骨格を
// 持っていてもいなくても動く。
void model_draw_held(const Model *m, const int pos[3], int yaw, u32 poly_fmt, const int hand[12]);
// アニメーションするもの用の同じもの：local はそれ自身のポーズ（anim_pose()、そのモデル空間で）、
// base（それを持つキャラクターのノードのグローバルな行列：首の上のプラーガ）にぶら下げる。
void model_draw_on(const Model *m, const int pos[3], int yaw, u32 poly_fmt, const int base[12], const int (*local)[12]);

// 回転なしの静的なモデルだけ：pos にモデルを DMA で描き始めてすぐ戻るので、ジオメトリエンジンが
// 忙しい間 CPU はほかの仕事ができる。テクスチャの結び付けと最後の行列のポップは DMA で送るリストの
// 一部（最初に使うときと poly_fmt が変わったときに作る）。ほかの 3D のコマンドの前に model_sync() を
// 呼ぶ。
void model_draw_async(Model *m, const int pos[3], u32 poly_fmt);
int model_build_async(Model *m, u32 poly_fmt);

// 分割したモデル（ステージ）用の同じもの、視点は at の方を見る：後ろにないチャンクを近い順に、
// 三角形 budget 個まで。
void model_draw_near(Model *m, const int eye[3], const int at[3], int budget, u32 poly_fmt);

// 部屋：model_draw_near() は箱（x0, z0, x1, z1、f32）の中に丸ごとあるチャンク（モード CULL_INSIDE：
// レオンは建物の外）か、丸ごと外にあるもの（CULL_OUTSIDE：彼は中）を省く。CULL_NONE は全部描く。
enum { CULL_NONE, CULL_INSIDE, CULL_OUTSIDE };
void model_set_cull_box(int mode, const int box[4]);
// 床：丸ごと y（f32）より下のチャンクを省く（0：なし）。たとえば回廊の下のホール。
void model_set_cull_below(int y);

// model_draw_async() の転送が終わるまで待つ。
void model_sync(void);

// 4x3 の補助関数（f32、回転の行 + 平行移動）。
void mtx43_mul(int out[12], const int a[12], const int b[12]);   // out = a * b
void mtx43_euler(int out[12], int rx, int ry, int rz);          // Rz * Ry * Rx、角度は libnds の単位

#endif
