#ifndef MODEL_H__
#define MODEL_H__

#include <nds.h>

#define MODEL_MAX_TEXTURES 32
#define MODEL_MAX_DRAWS    1024
#define MODEL_MAX_NODES    28

typedef struct {
    u16 texture;
    u8 transparent;
    u8 two_sided;
    u32 triangles;
    const u32 *list;
    int chunk[4];
    u32 list_at;
} ModelDraw;

typedef struct {
    int center[3];
    int shift;
    int bbox_min[3], bbox_max[3];
    int ground[3];
    u32 triangles;
    int tex_count, draw_count;
    int tex_names[MODEL_MAX_TEXTURES];
    ModelDraw *draws;
    int chunked;

    int node_count, slot_base;
    s16 parent[MODEL_MAX_NODES];
    int bind_local[MODEL_MAX_NODES][12];

    u32 *async_list;
    size_t async_words;
    u32 async_fmt;
} Model;

int model_load(Model *m, const void *data, size_t size);

#define VIEW_FAR inttof32(260)
void model_set_view(const int eye[3], const int at[3]);
int view_visible(const int pos[3], int radius);

void model_free(Model *m);

void model_draw(const Model *m, const int pos[3], int yaw, u32 poly_fmt, const int (*pose)[12]);

void model_draw_held(const Model *m, const int pos[3], int yaw, u32 poly_fmt, const int hand[12]);
void model_draw_on(const Model *m, const int pos[3], int yaw, u32 poly_fmt, const int base[12], const int (*local)[12]);

void model_draw_async(Model *m, const int pos[3], u32 poly_fmt);
int model_build_async(Model *m, u32 poly_fmt);

void model_draw_near(Model *m, const int eye[3], const int at[3], int budget, u32 poly_fmt);

enum { CULL_NONE, CULL_INSIDE, CULL_OUTSIDE };
void model_set_cull_box(int mode, const int box[4]);
void model_set_cull_below(int y);

void model_sync(void);

void mtx43_mul(int out[12], const int a[12], const int b[12]);
void mtx43_euler(int out[12], int rx, int ry, int rz);

#endif
