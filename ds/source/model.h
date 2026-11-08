// Loader/renderer for the RDS1 model format written by re4zeebo/DsExport.cs.
#ifndef MODEL_H__
#define MODEL_H__

#include <nds.h>

#define MODEL_MAX_TEXTURES 32
#define MODEL_MAX_DRAWS    1024
#define MODEL_MAX_NODES    28   // bone matrices live in stack slots slot_base..30

typedef struct {
    u16 texture;        // index into Model.tex_names, 0xFFFF = untextured
    u8 transparent;
    u8 two_sided;       // drawn from both sides (the game's material says so: walls seen from inside)
    u32 triangles;
    const u32 *list;    // packed GX list, list[0] = word count
    int chunk[4];       // chunked models: centre x, y, z and radius (f32, world space)
    u32 list_at;        // ...and where the next draw starts in Model.async_list
} ModelDraw;

typedef struct {
    int center[3];      // f32, model space
    int shift;          // vertices are stored divided by 2^shift
    int bbox_min[3], bbox_max[3];
    int ground[3];      // f32, model space
    u32 triangles;
    int tex_count, draw_count;
    int tex_names[MODEL_MAX_TEXTURES];
    ModelDraw *draws;       // draw_count of them
    int chunked;            // a stage, cut up by place: model_draw_near()

    // Skinned models only (node_count == 0 otherwise)
    int node_count, slot_base;
    s16 parent[MODEL_MAX_NODES];
    int bind_local[MODEL_MAX_NODES][12];   // f32: 3x3 rotation rows, then translation

    // Static models drawn with model_draw_async(): every draw in one GX list.
    u32 *async_list;
    size_t async_words;
    u32 async_fmt;
} Model;

// Parses the file and uploads its textures. Returns 0 on success.
int model_load(Model *m, const void *data, size_t size);

// View culling for characters: model_set_view() takes the frame's camera; view_visible()
// says whether a body of radius r (f32) around pos may be on screen (seen from above).
#define VIEW_FAR inttof32(260)
void model_set_view(const int eye[3], const int at[3]);
int view_visible(const int pos[3], int radius);

// Frees the model's textures (VRAM): for a model only needed some of the time.
void model_free(Model *m);

// Draws the model at world position pos (f32) rotated by yaw (libnds angle units).
// poly_fmt is OR-ed into every draw's polygon attributes (culling mode etc.).
// pose: for skinned models, one global 4x3 matrix per node (same layout as bind_local);
// NULL draws the bind pose. For static models, pose[0] (if not NULL) is applied as a
// parent transform, e.g. the hand node of a character holding this model.
void model_draw(const Model *m, const int pos[3], int yaw, u32 poly_fmt, const int (*pose)[12]);

// Draws a held object (a weapon) in a hand: hand is the hand node's global matrix (as in a
// pose). Works whether or not the object has a skeleton of its own.
void model_draw_held(const Model *m, const int pos[3], int yaw, u32 poly_fmt, const int hand[12]);

// Static models only, without rotation: starts drawing the model at pos by DMA and returns
// at once, so the CPU can do other work while the geometry engine is busy. Texture binds
// and the final matrix pop are part of the DMA'd list (built on first use and whenever
// poly_fmt changes). Call model_sync() before any other 3D command.
void model_draw_async(Model *m, const int pos[3], u32 poly_fmt);
int model_build_async(Model *m, u32 poly_fmt);

// The same for a chunked model (a stage), the eye looking towards at: the chunks not behind it,
// nearest first, up to budget triangles.
void model_draw_near(Model *m, const int eye[3], const int at[3], int budget, u32 poly_fmt);

// Rooms: model_draw_near() leaves out the chunks wholly inside the box (x0, z0, x1, z1, f32;
// mode CULL_INSIDE: Leon is out of the building) or wholly outside it (CULL_OUTSIDE: he is in
// it); CULL_NONE draws them all.
enum { CULL_NONE, CULL_INSIDE, CULL_OUTSIDE };
void model_set_cull_box(int mode, const int box[4]);
// Floors: chunks wholly below y (f32) are left out (0: none), e.g. the hall under a gallery.
void model_set_cull_below(int y);

// Waits until a model_draw_async() transfer has finished.
void model_sync(void);

// 4x3 helpers (f32, rotation rows + translation).
void mtx43_mul(int out[12], const int a[12], const int b[12]);   // out = a * b
void mtx43_euler(int out[12], int rx, int ry, int rz);          // Rz * Ry * Rx, angles in libnds units

#endif
