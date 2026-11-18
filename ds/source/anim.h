#ifndef ANIM_H__
#define ANIM_H__

#include "model.h"

#define ANIM_MAX 16

typedef struct {
    int count, nodes;
    int frames[ANIM_MAX];
    const u8 *node_data[ANIM_MAX][MODEL_MAX_NODES];
} AnimSet;

typedef struct {
    int t[3];
    int r[3];
    int has_r;
} NodeLocal;

int anim_load(AnimSet *a, const void *data, size_t size);

void anim_sample(const AnimSet *a, int anim, int frame, const Model *m, NodeLocal out[]);

void anim_blend(NodeLocal dst[], const NodeLocal src[], int count, int w);

void anim_add_delta(NodeLocal dst[], const NodeLocal a[], const NodeLocal b[], int count, int w);
void anim_add_delta_rot(NodeLocal dst[], const NodeLocal a[], const NodeLocal b[], int count, int w);

void anim_compose(const Model *m, const NodeLocal loc[], int out[][12]);

void anim_pose(const AnimSet *a, int anim, int frame, const Model *m, int out[][12]);

#endif
