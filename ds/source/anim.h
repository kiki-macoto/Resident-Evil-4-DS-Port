// Keyframe animations in the RDA1 format written by re4zeebo/DsExport.cs.
#ifndef ANIM_H__
#define ANIM_H__

#include "model.h"

#define ANIM_MAX 16

typedef struct {
    int count, nodes;
    int frames[ANIM_MAX];
    // Per animation and node: translation keys, rotation keys and where they start.
    const u8 *node_data[ANIM_MAX][MODEL_MAX_NODES];
} AnimSet;

// One node's local transform before it becomes a matrix, so poses can be blended.
typedef struct {
    int t[3];     // translation, f32 (the bind translation when not animated)
    int r[3];     // Euler angles X, Y, Z in libnds units, applied X first
    int has_r;    // 0: keep the bind rotation (node not rotated by any blended pose)
} NodeLocal;

// Returns 0 on success. The node count must match the model's skeleton.
int anim_load(AnimSet *a, const void *data, size_t size);

// Local transforms of every node of `m` for animation `anim` at `frame` (f32, wraps at the
// animation length).
void anim_sample(const AnimSet *a, int anim, int frame, const Model *m, NodeLocal out[]);

// dst = dst + (src - dst) * w for every node (w is f32, 0..1); angles take the short way.
void anim_blend(NodeLocal dst[], const NodeLocal src[], int count, int w);

// dst += (a - b) * w for every node: adds the change from pose b to pose a (e.g. a recoil
// relative to its first frame) on top of another pose. Angles take the short way.
void anim_add_delta(NodeLocal dst[], const NodeLocal a[], const NodeLocal b[], int count, int w);
// The same with each joint's rotation from b to a turned as a rotation (b^-1 a), applied after
// dst's own: right for big motions (the shotgun's pump), where adding the Euler angles is not.
void anim_add_delta_rot(NodeLocal dst[], const NodeLocal a[], const NodeLocal b[], int count, int w);

// Global node matrices (f32 4x3) from local transforms.
void anim_compose(const Model *m, const NodeLocal loc[], int out[][12]);

// anim_sample() + anim_compose().
void anim_pose(const AnimSet *a, int anim, int frame, const Model *m, int out[][12]);

#endif
