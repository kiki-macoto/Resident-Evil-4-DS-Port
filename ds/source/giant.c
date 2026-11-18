#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "giant.h"

// elg_anim.bin のクリップ（ch_elg.mcas #0..15）。テスト用ビルド：-DTEST_ELG_VIEW で順に表示
// （番号は FPS 欄）。
#ifndef ELG_IDLE
#define ELG_IDLE    0
#endif
#ifndef ELG_WALK
#define ELG_WALK    1
#endif
#ifndef ELG_ATTACK
#define ELG_ATTACK  6
#endif
#ifndef ELG_HIT
#define ELG_HIT     9
#endif
#ifndef ELG_KNEEL
#define ELG_KNEEL   8
#endif
#ifndef ELG_RISE
#define ELG_RISE    13
#endif
#ifndef ELG_DIE
#define ELG_DIE     5
#endif

#define ANIM_STEP     (inttof32(20) / 60)
#define WALK_SPEED    (inttof32(240) / 768)  // 原作の 20Hz の1ティックあたり 240、1/256 単位（ch_param_6。docs/NOTES.md）
#define TURN          (800 / 6)          // 原作の RotSpd 1ティックあたり 800（1回転 65536。ch_param_6）
#define RADIUS        inttof32(9)
#define HEIGHT        inttof32(60)
#define REACH         inttof32(26)       // 攻撃は前方にこれだけ届く
#define ATTACK_FROM   inttof32(24)       // レオンがこれだけ近いと攻撃を始める…
#define KEEP_OFF      inttof32(20)       // …そしてこれ以上は近づかない（腕の長さ）
#define ATTACK_DAMAGE 67                 // レオンの1200のうち原作の800（ch_param_6 のスクリプト16-25。docs/NOTES.md）
// ch_param_6.bin（種類0、体のレコード0）から：体力 4500。胴への弾は火力（ハンドガン 100）の
// 100% を減らし、ひるみの合計に 50% を足す。1010 でひざをつく（docs/NOTES.md）。
// 斬りと、ひざをついている時間は移植版独自。
#define GIANT_LIFE    4500
#define FLINCH_PCT    50
#define FLINCH_MAX    1010
#define KNEEL_FRAMES  (60 * 12)          // ひざをついたら、これだけそのまま（駆け寄って斬る時間）
#define CUTS_TO_DIE   3
#define CUT_DAMAGE    (GIANT_LIFE / 3)   // 寄生体を1回斬るとこれだけ減る
#define CUT_RANGE     inttof32(36)       // （大きいので、ここから背中に手が届く）
#define STEP_UP       inttof32(4)

static void *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *p = malloc(n);
    if (p && fread(p, 1, n, f) != (size_t)n)
    {
        free(p);
        p = NULL;
    }
    fclose(f);
    *size = n;
    return p;
}

int giant_load(Giant *g)
{
    if (g->loaded)
        return 0;
    size_t ms, as;
    g->model_buf = read_all("nitro:/elg.bin", &ms);
    g->anim_buf = read_all("nitro:/elg_anim.bin", &as);
    if (!g->model_buf || !g->anim_buf)
        return -1;
    memset(&g->model, 0, sizeof(g->model));
    if (model_load(&g->model, g->model_buf, ms))
        return -2;
    if (anim_load(&g->anims, g->anim_buf, as) || g->anims.nodes != g->model.node_count)
        return -3;
    g->loaded = 1;
    return 0;
}

void giant_share(Giant *g, const Giant *from)
{
    giant_unload(g);
    g->model = from->model;
    g->model.async_list = NULL;   // 自分用の描画リスト（最初の描画で作る）
    g->model.async_words = 0;
    g->anims = from->anims;
    g->shared = 1;
}

void giant_unload(Giant *g)
{
    if (g->shared)
    {
        free(g->model.async_list);
        g->model.async_list = NULL;
        g->shared = 0;
        return;
    }
    if (!g->loaded)
        return;
    model_free(&g->model);
    free(g->model_buf);
    free(g->anim_buf);
    g->model_buf = g->anim_buf = NULL;
    g->loaded = 0;
}

static void set_state(Giant *g, int state, int clip)
{
    g->state = state;
    g->clip = clip;
    g->frame = 0;
    g->timer = 0;
    g->struck = 0;
}

void giant_place(Giant *g, const Ground *gr, int x, int z, int ref_y, int yaw)
{
    g->pos[0] = x;
    g->pos[2] = z;
    g->feet = ref_y;
    ground_height(gr, x, z, ref_y, STEP_UP, &g->feet);
    g->pos[1] = g->feet;
    g->yaw = yaw;
    g->hp = 0;
    g->life = GIANT_LIFE;
    g->cuts = 0;
    set_state(g, GIANT_WALK, ELG_WALK);
}

static int clip_len(const Giant *g)
{
    int n = g->anims.frames[g->clip];
    return inttof32(n > 0 ? n : 1);
}

void giant_update(Giant *g, const Ground *gr, const int leon_pos[3], int vulnerable, GiantEvents *ev)
{
    ev->hit_leon = ev->stomp = ev->down = 0;
    g->frame += ANIM_STEP;
    g->timer++;
    int len = clip_len(g);
    int dx = leon_pos[0] - g->pos[0], dz = leon_pos[2] - g->pos[2];
    int dist = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
    int fx = sinLerp(g->yaw), fz = cosLerp(g->yaw);
    s64 dot = ((s64)fx * dx + (s64)fz * dz) >> 12;
    s64 cross = ((s64)fx * dz - (s64)fz * dx) >> 12;

    switch (g->state)
    {
    case GIANT_WALK:
        if (cross < -(s64)dist / 20 || (dot < 0 && cross <= 0))
            g->yaw += TURN;
        else if (cross > (s64)dist / 20 || dot < 0)
            g->yaw -= TURN;
        if (dist < ATTACK_FROM && dot > 0 && vulnerable)
            set_state(g, GIANT_ATTACK, ELG_ATTACK);
        else if (dist > KEEP_OFF)
        {
            g->pos[0] += mulf32(fx, WALK_SPEED);
            g->pos[2] += mulf32(fz, WALK_SPEED);
            ground_collide_n(gr, &g->pos[0], &g->pos[2], g->feet + STEP_UP, g->feet + HEIGHT, RADIUS, 1);
            ground_height(gr, g->pos[0], g->pos[2], g->feet + STEP_UP, STEP_UP, &g->feet);
            g->pos[1] = g->feet;
        }
        if (g->frame >= len)
            g->frame -= len;
        break;
    case GIANT_ATTACK:
        // 打撃はクリップの中ほどで下りる：レオンがまだ前方の近くにいればダメージ。
        if (!g->struck && g->frame >= len / 2)
        {
            g->struck = 1;
            ev->stomp = 1;
            if (vulnerable && dist < REACH && dot * 2 > dist)
                ev->hit_leon = ATTACK_DAMAGE;
        }
        if (g->frame >= len)
            set_state(g, GIANT_WALK, ELG_WALK);
        break;
    case GIANT_HIT:
        if (g->frame >= len)
            set_state(g, GIANT_WALK, ELG_WALK);
        break;
    case GIANT_KNEEL:
        if (g->held)
            g->timer = 0;         // （レオンが背中にいる：ひざをついたまま）
        if (g->frame > len - 1)
            g->frame = len - 1;   // ひざをついたまま
        else
            g->timer = 0;         // （時間はひざをついてから数える。沈み込む間は数えない）
        if (g->timer >= KNEEL_FRAMES)
            set_state(g, GIANT_RISE, ELG_RISE);
        break;
    case GIANT_RISE:
        if (g->frame >= len)
        {
            g->hp = 0;
            set_state(g, GIANT_WALK, ELG_WALK);
        }
        break;
    case GIANT_DEAD:
        if (g->frame > len - 1)
            g->frame = len - 1;
        break;
    }
}

int giant_raycast(const Giant *g, const int p[3], const int d[3], int *t_best)
{
    if (g->state == GIANT_DEAD)
        return 0;
    s64 aa = ((s64)d[0] * d[0] + (s64)d[2] * d[2]) >> 12;
    if (aa == 0)
        return 0;
    s64 fx = p[0] - g->pos[0], fz = p[2] - g->pos[2];
    s64 b = (fx * d[0] + fz * d[2]) >> 12;
    s64 c = ((fx * fx + fz * fz) >> 12) - (((s64)RADIUS * RADIUS) >> 12);
    s64 disc = b * b - aa * c;
    if (disc < 0)
        return 0;
    s64 root = (s64)sqrt64(disc);
    s64 t = ((-b - root) << 12) / aa;
    if (t <= 0 && ((-b + root) << 12) / aa > 0)
        t = 1;   // 銃口が体の中（至近距離）：その場で当たり
    if (t <= 0 || t >= *t_best)
        return 0;
    s64 y = p[1] + ((d[1] * t) >> 12) - g->feet;
    int top = g->state == GIANT_KNEEL ? HEIGHT / 2 : HEIGHT;
    if (y < 0 || y > top)
        return 0;
    *t_best = (int)t;
    return 1;
}

void giant_shot(Giant *g, int power)
{
    if (g->state == GIANT_DEAD)
        return;
    g->life -= power;
    if (g->life <= 0)
    {
        set_state(g, GIANT_DEAD, ELG_DIE);   // （Zeebo の動画：ライフルでとどめ）
        return;
    }
    if (g->state == GIANT_KNEEL || g->state == GIANT_RISE)
        return;
    g->hp += power * FLINCH_PCT / 100;
    if (g->hp >= FLINCH_MAX)
    {
        g->hp = 0;
        set_state(g, GIANT_KNEEL, ELG_KNEEL);   // ひざをつく：寄生体が出てくる
    }
}

int giant_cuttable(const Giant *g, const int pos[3])
{
    if (g->state != GIANT_KNEEL)
        return 0;
    int dx = pos[0] - g->pos[0], dz = pos[2] - g->pos[2];
    return (s64)dx * dx + (s64)dz * dz < (s64)CUT_RANGE * CUT_RANGE;
}

int giant_blast(Giant *g)
{
    if (g->state == GIANT_DEAD)
        return 0;
    set_state(g, GIANT_DEAD, ELG_DIE);   // ロケット：状態に関係なく倒れる
    return 1;
}

void giant_cut(Giant *g)
{
    if (g->state != GIANT_KNEEL)
        return;
    g->life -= CUT_DAMAGE;
    if (++g->cuts >= CUTS_TO_DIE || g->life <= 0)
        set_state(g, GIANT_DEAD, ELG_DIE);
    else
        set_state(g, GIANT_RISE, ELG_RISE);   // レオンを振り落として起き上がる
}

void giant_push(const Giant *g, int *x, int *z, int r)
{
    if (g->state == GIANT_DEAD)
        return;
    int ox = *x - g->pos[0], oz = *z - g->pos[2], min = r + RADIUS;
    int d = (int)sqrt64((u64)((s64)ox * ox + (s64)oz * oz));
    if (d > 0 && d < min)
    {
        *x = g->pos[0] + mulf32(divf32(ox, d), min);
        *z = g->pos[2] + mulf32(divf32(oz, d), min);
    }
}

void giant_draw(Giant *g, u32 poly_fmt)
{
    if (!view_visible(g->pos, RADIUS * 3))
        return;
    int clip = g->clip, frame = g->frame;
#ifdef TEST_ELG_VIEW   // テスト用ビルド：クリップを順に、3秒ずつ
    static u32 t;
    t++;
    clip = (t / 180) % g->anims.count;
    frame = (int)(t % 180) * ANIM_STEP % inttof32(g->anims.frames[clip] > 0 ? g->anims.frames[clip] : 1);
    g->clip = clip;
#endif
    static u32 tick;
    if (tick++ % 3 == 0 || g->state == GIANT_ATTACK)
        anim_pose(&g->anims, clip, frame, &g->model, g->pose);
    model_draw(&g->model, g->pos, g->yaw, poly_fmt, (const int (*)[12])g->pose);
}
