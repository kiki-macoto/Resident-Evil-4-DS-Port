#include <stdlib.h>
#include <string.h>

#include "model.h"

#define ASYNC_DMA 2   // DMA channel used by model_draw_async()
#define BEHIND_MARGIN inttof32(60)

static u16 rd16(const u8 *p) { return p[0] | p[1] << 8; }
static u32 rd32(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }

int model_load(Model *m, const void *data, size_t size)
{
    const u8 *p = data, *end = p + size;
    memset(m, 0, sizeof(*m));
    if (size < 64 || memcmp(p, "RDS1", 4) != 0)
        return -1;

    m->tex_count = rd16(p + 4);
    m->draw_count = rd16(p + 6);
    if (m->tex_count > MODEL_MAX_TEXTURES || m->draw_count > MODEL_MAX_DRAWS)
        return -2;
    m->draws = malloc(m->draw_count * sizeof(ModelDraw));
    if (!m->draws)
        return -2;
    for (int k = 0; k < 3; k++)
    {
        m->center[k] = (int)rd32(p + 8 + k * 4);
        m->bbox_min[k] = (int)rd32(p + 24 + k * 4);
        m->bbox_max[k] = (int)rd32(p + 36 + k * 4);
        m->ground[k] = (int)rd32(p + 48 + k * 4);
    }
    m->shift = rd16(p + 20);
    int flags = rd16(p + 22);
    m->triangles = rd32(p + 60);
    p += 64;

    for (int i = 0; i < m->tex_count; i++)
    {
        if (p + 12 > end)
            return -3;
        int w = rd16(p), h = rd16(p + 2);
        GL_TEXTURE_TYPE_ENUM type = p[4];
        int transparent = p[5] & 1;
        int pal_colors = rd16(p + 6);
        u32 bytes = rd32(p + 8);
        const u8 *texels = p + 12;
        const u16 *pal = (const u16 *)(texels + bytes);
        p = texels + bytes + pal_colors * 2;
        p += (4 - ((uintptr_t)p & 3)) & 3;
        if (p > end)
            return -3;

        int param = TEXGEN_TEXCOORD | GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T;
        if (transparent)
            param |= GL_TEXTURE_COLOR0_TRANSPARENT;
        glGenTextures(1, &m->tex_names[i]);
        glBindTexture(0, m->tex_names[i]);
        if (!glTexImageNtr2D(type, w, h, param, texels, NULL))
            return -4;
        if (!glColorTableNtr(pal_colors, pal))
            return -5;
    }

    for (int i = 0; i < m->draw_count; i++)
    {
        if (p + 12 > end)
            return -6;
        ModelDraw *d = &m->draws[i];
        d->texture = rd16(p);
        d->transparent = p[2];
        d->two_sided = p[3];
        d->triangles = rd32(p + 4);
        p += 8;
        if (flags & 2)
        {
            m->chunked = 1;
            for (int k = 0; k < 4; k++)
                d->chunk[k] = (int)rd32(p + k * 4);
            p += 16;
        }
        d->list = (const u32 *)p;
        p += (1 + d->list[0]) * 4;
        if (p > end)
            return -6;
    }

    if (flags & 1)
    {
        if (p + 4 > end)
            return -7;
        m->node_count = rd16(p);
        m->slot_base = rd16(p + 2);
        p += 4;
        if (m->node_count > MODEL_MAX_NODES || m->slot_base + m->node_count > 31 || p + m->node_count * 52 > end)
            return -7;
        for (int i = 0; i < m->node_count; i++, p += 52)
        {
            m->parent[i] = (s16)rd16(p);
            for (int k = 0; k < 12; k++)
                m->bind_local[i][k] = (int)rd32(p + 4 + k * 4);
        }
    }
    return 0;
}

void mtx43_mul(int out[12], const int a[12], const int b[12])
{
    int r[12];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
            r[i * 3 + j] = mulf32(a[i * 3], b[j]) + mulf32(a[i * 3 + 1], b[3 + j]) + mulf32(a[i * 3 + 2], b[6 + j]);
        r[9 + i] = mulf32(a[i * 3], b[9]) + mulf32(a[i * 3 + 1], b[10]) + mulf32(a[i * 3 + 2], b[11]) + a[9 + i];
    }
    memcpy(out, r, sizeof(r));
}

void mtx43_euler(int out[12], int rx, int ry, int rz)
{
    int sx = sinLerp(rx), cx = cosLerp(rx);
    int sy = sinLerp(ry), cy = cosLerp(ry);
    int sz = sinLerp(rz), cz = cosLerp(rz);
    int sysx = mulf32(sy, sx), sycx = mulf32(sy, cx);
    out[0] = mulf32(cz, cy);
    out[1] = mulf32(cz, sysx) - mulf32(sz, cx);
    out[2] = mulf32(cz, sycx) + mulf32(sz, sx);
    out[3] = mulf32(sz, cy);
    out[4] = mulf32(sz, sysx) + mulf32(cz, cx);
    out[5] = mulf32(sz, sycx) - mulf32(cz, sx);
    out[6] = -sy;
    out[7] = mulf32(cy, sx);
    out[8] = mulf32(cy, cx);
}

// Synchronous CPU copy into the geometry FIFO, so texture/attribute writes made
// between lists can't interleave with a still-running DMA transfer.
static void call_list(const u32 *list)
{
    u32 n = *list++;
    while (n--)
        GFX_FIFO = *list++;
}

// Loads a 4x3 matrix (rotation rows + translation) into the hardware, which takes
// row-vector order: the transpose of the rotation, then the translation.
static void mult_mtx43(const int a[12])
{
    m4x3 t = { {
        a[0], a[3], a[6],
        a[1], a[4], a[7],
        a[2], a[5], a[8],
        a[9], a[10], a[11],
    } };
    glMultMatrix4x3(&t);
}

// The camera, for view_visible() (set once per frame).
static int view_eye[3], view_fwd[2];

void model_set_view(const int eye[3], const int at[3])
{
    int dx = at[0] - eye[0], dz = at[2] - eye[2];
    int d = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
    memcpy(view_eye, eye, sizeof(view_eye));
    view_fwd[0] = d ? divf32(dx, d) : 0;
    view_fwd[1] = d ? divf32(dz, d) : inttof32(1);
}

int view_visible(const int pos[3], int radius)
{
    // Seen from above: in front of the camera, within the horizontal field of view (70 degrees
    // vertical at 4:3: tan(half) = 0.93), and not lost in the fog.
    int dx = pos[0] - view_eye[0], dz = pos[2] - view_eye[2];
    s64 along = ((s64)dx * view_fwd[0] + (s64)dz * view_fwd[1]) >> 12;
    s64 side = ((s64)dx * view_fwd[1] - (s64)dz * view_fwd[0]) >> 12;
    if (side < 0)
        side = -side;
    if (along < -radius || along > VIEW_FAR)
        return 0;
    return side <= ((along * 3810) >> 12) + radius * 3 / 2;
}

void model_free(Model *m)
{
    for (int i = 0; i < m->tex_count; i++)
        if (m->tex_names[i])
            glDeleteTextures(1, &m->tex_names[i]);
    m->tex_count = 0;
    m->draw_count = 0;   // nothing left to draw until loaded again
    free(m->draws);
    m->draws = NULL;
    free(m->async_list);
    m->async_list = NULL;
    m->async_words = 0;
}

void model_draw_held(const Model *m, const int pos[3], int yaw, u32 poly_fmt, const int hand[12])
{
    if (!m->node_count)
    {
        model_draw(m, pos, yaw, poly_fmt, (const int (*)[12])hand);
        return;
    }
    // A weapon with a skeleton of its own (the knife, the sickle): each of its nodes in its
    // bind pose, hung off the hand. Parents may come after their children in the list.
    static int g[MODEL_MAX_NODES][12];
    u32 done = 0, all = (1u << m->node_count) - 1;
    for (int pass = 0; pass < m->node_count && done != all; pass++)
        for (int i = 0; i < m->node_count; i++)
        {
            int p = m->parent[i];
            if (done & (1u << i))
                continue;
            if (p < 0 || p >= m->node_count)
                mtx43_mul(g[i], hand, m->bind_local[i]);
            else if (done & (1u << p))
                mtx43_mul(g[i], g[p], m->bind_local[i]);
            else
                continue;
            done |= 1u << i;
        }
    model_draw(m, pos, yaw, poly_fmt, (const int (*)[12])g);
}

void model_draw(const Model *m, const int pos[3], int yaw, u32 poly_fmt, const int (*pose)[12])
{
    // The previous model's list may still be streaming: the matrices below go through the
    // same FIFO.
    model_sync();
    glPushMatrix();   // stack slot 0 = caller's matrix, restored by the list's MTX_POP
    glTranslatef32(pos[0], pos[1], pos[2]);
    if (yaw)
        glRotateYi(yaw);
    int s = inttof32(1 << m->shift);

    if (m->node_count)
    {
        // Skinned: slot 1 = model base; slot slot_base + j = base * pose[j] * scale.
        // The display lists pick a node's matrix with MTX_RESTORE before its vertices.
        static int bind_global[MODEL_MAX_NODES][12];
        if (!pose)
        {
            for (int i = 0; i < m->node_count; i++)
            {
                if (m->parent[i] < 0)
                    memcpy(bind_global[i], m->bind_local[i], sizeof(bind_global[i]));
                else
                    mtx43_mul(bind_global[i], bind_global[m->parent[i]], m->bind_local[i]);
            }
            pose = (const int (*)[12])bind_global;
        }
        glStoreMatrix(1);
        for (int j = 0; j < m->node_count; j++)
        {
            glRestoreMatrix(1);
            mult_mtx43(pose[j]);
            if (m->shift)
                glScalef32(s, s, s);
            glStoreMatrix(m->slot_base + j);
        }
    }
    else
    {
        // Static: pose[0], if given, is a parent transform (e.g. a hand node, for a weapon).
        if (pose)
            mult_mtx43(pose[0]);
        glTranslatef32(m->center[0], m->center[1], m->center[2]);
        glScalef32(s, s, s);
    }

    // The draws (textures, polygon format, vertices) and the closing MTX_POP as one packed
    // list, streamed by DMA: the CPU goes on (posing the next character) while the geometry
    // engine works through it. Anything else writing GX commands calls model_sync() first.
    Model *mm = (Model *)m;   // (the list is a cache of the model's draws)
    if (!mm->async_list || mm->async_fmt != poly_fmt)
    {
        if (model_build_async(mm, poly_fmt))
        {
            // No memory for the list: draw the slow way.
            for (int i = 0; i < m->draw_count; i++)
            {
                const ModelDraw *d = &m->draws[i];
                glBindTexture(0, d->texture == 0xFFFF ? 0 : m->tex_names[d->texture]);
                glPolyFmt((poly_fmt & POLY_ALPHA(31) ? 0 : POLY_ALPHA(31)) | poly_fmt);   // (an alpha given: a fading model)
                call_list(d->list);
            }
            glPopMatrix(1);
            return;
        }
    }
    DMA_SRC(ASYNC_DMA) = (u32)mm->async_list;
    DMA_DEST(ASYNC_DMA) = (u32)&GFX_FIFO;
    DMA_CR(ASYNC_DMA) = DMA_ENABLE | DMA_START_FIFO | DMA_32_BIT | DMA_SRC_INC | DMA_DST_FIX | mm->async_words;
}

// GX command ids (GBATEK "Geometry Commands").
#define GX_MTX_POP       0x12
#define GX_POLYGON_ATTR  0x29
#define GX_TEXIMAGE_PARAM 0x2A
#define GX_PLTT_BASE     0x2B

int model_build_async(Model *m, u32 poly_fmt)
{
    // One packed list for the whole (static) model: per draw a header word with
    // TEXIMAGE_PARAM, PLTT_BASE, POLYGON_ATTR (+ NOP) and their parameters, then the draw's
    // own packed words; at the end MTX_POP so the caller's matrix comes back.
    size_t words = 2;
    for (int i = 0; i < m->draw_count; i++)
        words += 4 + m->draws[i].list[0];
    if (!m->async_list || m->async_words < words)
    {
        free(m->async_list);
        m->async_list = malloc(words * 4);
        if (!m->async_list)
            return -1;
    }
    u32 *out = m->async_list;
    for (int i = 0; i < m->draw_count; i++)
    {
        const ModelDraw *d = &m->draws[i];
        // The texture/palette register values libnds would write for this texture.
        int pal = 0;
        u32 tex = 0;
        if (d->texture != 0xFFFF)
        {
            glBindTexture(0, m->tex_names[d->texture]);
            tex = glGetTexParameter();
            glGetColorTableParameterEXT(0, GL_COLOR_TABLE_FORMAT_EXT, &pal);
        }
        *out++ = GX_TEXIMAGE_PARAM | GX_PLTT_BASE << 8 | GX_POLYGON_ATTR << 16;
        *out++ = tex;
        *out++ = (u32)pal;
        u32 fmt = (poly_fmt & POLY_ALPHA(31) ? 0 : POLY_ALPHA(31)) | poly_fmt;
        if (d->two_sided)
            fmt = (fmt & ~POLY_CULL_NONE) | POLY_CULL_NONE;   // (no culling: both faces)
        *out++ = fmt;
        memcpy(out, d->list + 1, d->list[0] * 4);
        out += d->list[0];
        if (m->chunked)
            m->draws[i].list_at = out - m->async_list;   // (where the draw after it starts)
    }
    *out++ = GX_MTX_POP;
    *out++ = 1;
    m->async_words = out - m->async_list;
    m->async_fmt = poly_fmt;
    DC_FlushRange(m->async_list, m->async_words * 4);
    return 0;
}

void model_draw_async(Model *m, const int pos[3], u32 poly_fmt)
{
    model_sync();   // building binds textures (register writes): never during a transfer
    if (!m->async_list || m->async_fmt != poly_fmt)
        model_build_async(m, poly_fmt);
    glPushMatrix();
    glTranslatef32(pos[0], pos[1], pos[2]);
    glTranslatef32(m->center[0], m->center[1], m->center[2]);
    int s = inttof32(1 << m->shift);
    glScalef32(s, s, s);
    // GX FIFO DMA: refills the FIFO whenever it is half empty, in the background.
    DMA_SRC(ASYNC_DMA) = (u32)m->async_list;
    DMA_DEST(ASYNC_DMA) = (u32)&GFX_FIFO;
    DMA_CR(ASYNC_DMA) = DMA_ENABLE | DMA_START_FIFO | DMA_32_BIT | DMA_SRC_INC | DMA_DST_FIX | m->async_words;
}

// A chunked model: the chunks wanted, nearest first, each one DMA transfer, the next started
// from the last one's interrupt.
#define CHAIN_MAX MODEL_MAX_DRAWS
#define NEAR_BUCKETS 32
#define BUCKET_SHIFT 16      // f32 distance to bucket: 16 units each
static struct { const u32 *src; u32 words; } chain[CHAIN_MAX + 1];
static volatile int chain_at, chain_n;

static void chain_next(void)
{
    if (chain_at >= chain_n)
        return;
    int i = chain_at++;
    DMA_SRC(ASYNC_DMA) = (u32)chain[i].src;
    DMA_DEST(ASYNC_DMA) = (u32)&GFX_FIFO;
    DMA_CR(ASYNC_DMA) = DMA_ENABLE | DMA_START_FIFO | DMA_32_BIT | DMA_SRC_INC | DMA_DST_FIX | DMA_IRQ_REQ | chain[i].words;
}

static int cull_mode, cull_box[4], cull_below;
void model_set_cull_below(int y) { cull_below = y; }
void model_set_cull_box(int mode, const int box[4])
{
    cull_mode = mode;
    if (box)
        for (int k = 0; k < 4; k++)
            cull_box[k] = box[k];
}

// The view's sides: the screen is 70 degrees high and 4:3 wide, so about 43 degrees each way
// across; a chunk wholly beyond 52 degrees off the line of sight is off screen.
#define SIDE_TAN     floattof32(1.28)
#define SIDE_MARGIN  inttof32(12)

void model_draw_near(Model *m, const int eye[3], const int at[3], int budget, u32 poly_fmt)
{
    static int irq_set;
    static s16 head[NEAR_BUCKETS], next[MODEL_MAX_DRAWS];
    model_sync();
    if (!irq_set)
    {
        irqSet(IRQ_DMA2, chain_next);
        irqEnable(IRQ_DMA2);
        irq_set = 1;
    }
    if (!m->async_list || m->async_fmt != poly_fmt)
        model_build_async(m, poly_fmt);
    glPushMatrix();
    glTranslatef32(m->center[0], m->center[1], m->center[2]);
    int s = inttof32(1 << m->shift);
    glScalef32(s, s, s);
    // Forward on the ground, as a unit vector.
    int fx = at[0] - eye[0], fz = at[2] - eye[2];
    int len = (int)sqrt64((u64)((s64)fx * fx + (s64)fz * fz));
    if (len == 0)
        len = 1;
    fx = divf32(fx, len);
    fz = divf32(fz, len);
    // Sorted by distance, roughly: those that don't fit the budget are the farthest, in the haze.
    for (int i = 0; i < NEAR_BUCKETS; i++)
        head[i] = -1;
    for (int i = m->draw_count - 1; i >= 0; i--)
    {
        const ModelDraw *d = &m->draws[i];
        int dx = d->chunk[0] - eye[0], dz = d->chunk[2] - eye[2];
        // (Wholly behind the camera, with room to spare.)
        int fwd = ((s64)dx * fx + (s64)dz * fz) >> 12;
        if (fwd < -(d->chunk[3] + BEHIND_MARGIN))
            continue;
        // (Wholly off to a side.)
        int lat = ((s64)dz * fx - (s64)dx * fz) >> 12;
        if (lat < 0)
            lat = -lat;
        if (lat > mulf32(fwd > 0 ? fwd : 0, SIDE_TAN) + d->chunk[3] + SIDE_MARGIN)
            continue;
        // (The floor below, from a gallery above it.)
        if (cull_below && d->chunk[1] + d->chunk[3] < cull_below)
            continue;
        // (A room's inside from out of it, or the world outside from in it.)
        if (cull_mode)
        {
            int r = d->chunk[3];
            int in = d->chunk[0] - r > cull_box[0] && d->chunk[0] + r < cull_box[2] && d->chunk[2] - r > cull_box[1] && d->chunk[2] + r < cull_box[3];
            int out = d->chunk[0] + r < cull_box[0] || d->chunk[0] - r > cull_box[2] || d->chunk[2] + r < cull_box[1] || d->chunk[2] - r > cull_box[3];
            if ((cull_mode == CULL_INSIDE && in) || (cull_mode == CULL_OUTSIDE && out))
                continue;
        }
        int ax = dx < 0 ? -dx : dx, az = dz < 0 ? -dz : dz;
        int b = (ax > az ? ax + az / 2 : az + ax / 2) - d->chunk[3];
        b = b < 0 ? 0 : b >> BUCKET_SHIFT;
        if (b >= NEAR_BUCKETS)
            b = NEAR_BUCKETS - 1;
        next[i] = head[b];
        head[b] = i;
    }
    // Nearest first; once one doesn't fit, nothing farther is drawn (its own distance band is
    // finished). Skipping just that one let smaller chunks behind it in: a big wall near the
    // camera vanished, and the distance showed through it.
    int n = 0, left = budget, full = 0;
    for (int b = 0; b < NEAR_BUCKETS && !full; b++)
        for (int i = head[b]; i >= 0; i = next[i])
        {
            const ModelDraw *d = &m->draws[i];
            if ((int)d->triangles > left)
            {
                full = 1;
                continue;
            }
            left -= d->triangles;
            u32 from = i ? m->draws[i - 1].list_at : 0;
            chain[n].src = m->async_list + from;
            chain[n++].words = d->list_at - from;
        }
    // Last, the list's closing MTX_POP.
    chain[n].src = m->async_list + m->async_words - 2;
    chain[n++].words = 2;
    chain_at = 0;
    chain_n = n;
    int irq = enterCriticalSection();
    chain_next();
    leaveCriticalSection(irq);
}

void model_sync(void)
{
    while (chain_at < chain_n || dmaBusy(ASYNC_DMA))
        ;
}
