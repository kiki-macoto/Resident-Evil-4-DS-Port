#ifndef GROUND_H__
#define GROUND_H__

#include <nds.h>

typedef struct {
    int cells_x, cells_z;
    int origin_x, origin_z, cell_size;
    u32 triangles;
    const u32 *cell_start;
    const u16 *index;
    const s32 *tri;
    u32 walls;
    const u32 *wall_cell_start;
    const u16 *wall_index;
    const s32 *wall;
    u32 ray_tris, ray_walls;
    const u32 *ray_cell_start, *ray_wall_cell_start;
    const u16 *ray_index, *ray_wall_index;
    const s32 *ray_tri, *ray_wall;
    const u8 *ray_tri_mask, *ray_wall_mask;
} Ground;

enum { RAY_CAMERA = 1, RAY_SHOT = 2 };

int ground_load(Ground *g, const void *data, size_t size);

void ground_clear_gaps(void);
void ground_add_gap(int cx, int cz, int ux, int uz, int half_len, int half_depth);

int ground_height(const Ground *g, int x, int z, int ref_y, int step_up, int *y);

int ground_collide(const Ground *g, int *x, int *z, int y_lo, int y_hi, int radius);
int ground_collide_n(const Ground *g, int *x, int *z, int y_lo, int y_hi, int radius, int passes);

int ground_raycast(const Ground *g, const int p[3], const int d[3], int max_dist, int hit[3], int mask);

#endif
