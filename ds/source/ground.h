// Floor height lookups and wall collision on the RDG2 ground file written by re4zeebo/DsExport.cs.
#ifndef GROUND_H__
#define GROUND_H__

#include <nds.h>

typedef struct {
    int cells_x, cells_z;
    int origin_x, origin_z, cell_size;   // f32
    u32 triangles;
    const u32 *cell_start;
    const u16 *index;
    const s32 *tri;                      // 9 words per triangle: x0 z0 x1 z1 x2 z2 a b c
    u32 walls;
    const u32 *wall_cell_start;
    const u16 *wall_index;
    const s32 *wall;                     // 6 words per wall: x0 z0 x1 z1 y_min y_max
    // What the rays hit (the game's camera and shot groups), on the same grid. None: none in the file.
    u32 ray_tris, ray_walls;
    const u32 *ray_cell_start, *ray_wall_cell_start;
    const u16 *ray_index, *ray_wall_index;
    const s32 *ray_tri, *ray_wall;
    const u8 *ray_tri_mask, *ray_wall_mask;   // RAY_CAMERA, RAY_SHOT
} Ground;

enum { RAY_CAMERA = 1, RAY_SHOT = 2 };

// Returns 0 on success.
int ground_load(Ground *g, const void *data, size_t size);

// Doorways: walls are not met inside these boxes (centre x, z; unit vector along the door's leaf;
// half its length; half the depth across it, f32). A room's collision outline can run right
// across its own doorway (stage 5's room where Ashley waits): an open door would still be shut.
void ground_clear_gaps(void);
void ground_add_gap(int cx, int cz, int ux, int uz, int half_len, int half_depth);

// Floor height (f32) under (x, z): the highest floor at or below ref_y + step_up, so roofs
// above are ignored while stairs and slopes are followed. Returns 0 (and leaves *y alone)
// if no such floor exists there.
int ground_height(const Ground *g, int x, int z, int ref_y, int step_up, int *y);

// Pushes a vertical cylinder (centre *x, *z, radius, spanning y_lo..y_hi) out of the walls
// it overlaps. Returns the number of pushes applied.
int ground_collide(const Ground *g, int *x, int *z, int y_lo, int y_hi, int radius);
// The same with a given number of passes (1 is cheaper: for the enemies).
int ground_collide_n(const Ground *g, int *x, int *z, int y_lo, int y_hi, int radius, int passes);

// Casts a ray from p (f32) along the unit direction d (f32) against the walls and floors of
// the set mask (RAY_CAMERA, RAY_SHOT), up to max_dist (f32). Returns 1 and the nearest hit
// point in hit[3] (f32), or 0.
int ground_raycast(const Ground *g, const int p[3], const int d[3], int max_dist, int hit[3], int mask);

#endif
