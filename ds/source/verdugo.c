#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "verdugo.h"

// ver_anim.bin のクリップ（ch_ver.mcas #0,1,2,3,4,6,9,10,12,16,17,18,20,22,24）、ポーズで
// 見分けた：0 立つ、1 歩く、2 は11フレームのしゃがんだループ（走り）、6 は尾を頭の上に振り上げて
// 前を打つ、7 は70単位跳び上がる（天井へ）、8 は64単位から降りてくる、13 は短いひるみ、14 は
// ゆっくり床に沈む（死）。テスト用ビルド：-DTEST_VER_VIEW でそれぞれを順に出す（番号は FPS の欄）。
#ifndef VER_IDLE
#define VER_IDLE    0
#endif
#ifndef VER_WALK
#define VER_WALK    1
#endif
#ifndef VER_RUN
#define VER_RUN     2
#endif
#ifndef VER_TAIL_CLIP
#define VER_TAIL_CLIP 6
#endif
#ifndef VER_JUMP
#define VER_JUMP    7
#endif
#ifndef VER_DROP_CLIP
#define VER_DROP_CLIP 8
#endif
#ifndef VER_HIT_CLIP
#define VER_HIT_CLIP 13
#endif
#ifndef VER_DIE
#define VER_DIE     14
#endif

#define ANIM_STEP     (inttof32(20) / 60)
#define WALK_SPEED    (inttof32(200) / 768)  // 原作の 20Hz の1ティックあたり200、1/256 単位（ch_param_7。docs/NOTES.md）
#define RUN_SPEED     floattof32(0.36)      // レオンの走り（0.60）より遅い：タンクまで誘い出せる
#define RUN_FROM      inttof32(30)          // レオンがこれより遠いと走る
#define TURN          (800 / 6)             // 原作の RotSpd、20Hz の1ティックあたり800（1回転 65536。ch_param_7）
#define RADIUS        floattof32(3)
#define HEIGHT        inttof32(24)
#define TAIL_FROM     inttof32(13)          // レオンが前方のこの近さにいると打つ
#define REACH         inttof32(16)          // 尾はここまで届く
#define TAIL_DAMAGE   54                    // 原作のレオンの1200のうち650（ch_param_7 のスクリプト12-40。docs/NOTES.md）
// ch_param_7.bin（種類0、体のレコード0）から：体力20000、弾はその火力（ハンドガン100）の100%を減らし、
// 同じだけひるみの合計に足す。1200でひるむ（docs/NOTES.md）。
#define HP            20000
#define FLINCH_MAX    1200
#define FROZEN_TIMES  20                    // 凍っている間の当たりはこの回数分に数える（移植版独自）
#define FROZEN_FRAMES (60 * 5)
#ifndef LEAP_EVERY
#define LEAP_EVERY    (60 * 14)             // これだけ忍び寄ったら、跳び上がって見えなくなる
#endif
#define GONE_FRAMES   (60 * 2)
#define DROP_BEHIND   inttof32(14)          // レオンの横のこの距離に降りてくる
#define LEON_ROOM     floattof32(3.5)
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

int verdugo_load(Verdugo *v)
{
    if (v->loaded)
        return 0;
    size_t ms, as;
    v->model_buf = read_all("nitro:/ver.bin", &ms);
    v->anim_buf = read_all("nitro:/ver_anim.bin", &as);
    if (!v->model_buf || !v->anim_buf)
        return -1;
    memset(&v->model, 0, sizeof(v->model));
    if (model_load(&v->model, v->model_buf, ms))
        return -2;
    if (anim_load(&v->anims, v->anim_buf, as) || v->anims.nodes != v->model.node_count)
        return -3;
    v->loaded = 1;
    return 0;
}

void verdugo_unload(Verdugo *v)
{
    if (!v->loaded)
        return;
    model_free(&v->model);
    free(v->model_buf);
    free(v->anim_buf);
    v->model_buf = v->anim_buf = NULL;
    v->loaded = 0;
}

static void set_state(Verdugo *v, int state, int clip)
{
    v->state = state;
    v->clip = clip;
    v->frame = 0;
    v->timer = 0;
    v->struck = 0;
}

void verdugo_place(Verdugo *v, const Ground *gr, int x, int z, int ref_y, int yaw)
{
    v->pos[0] = x;
    v->pos[2] = z;
    v->feet = ref_y;
    ground_height(gr, x, z, ref_y, STEP_UP, &v->feet);
    v->pos[1] = v->feet;
    v->yaw = yaw;
    v->hp = HP;
    v->flinch = 0;
    v->next_leap = LEAP_EVERY;
    set_state(v, VER_STALK, VER_WALK);
}

static int clip_len(const Verdugo *v)
{
    int n = v->anims.frames[v->clip];
    return inttof32(n > 0 ? n : 1);
}

void verdugo_update(Verdugo *v, const Ground *gr, const int leon_pos[3], int leon_feet, int vulnerable, VerdugoEvents *ev)
{
    ev->hit_leon = ev->landed = ev->thawed = 0;
    if (v->state != VER_FROZEN)
        v->frame += v->state == VER_STALK && v->clip == VER_RUN ? ANIM_STEP * 2 : ANIM_STEP;
    v->timer++;
    int len = clip_len(v);
    int fx = sinLerp(v->yaw), fz = cosLerp(v->yaw);
    int dx = leon_pos[0] - v->pos[0], dz = leon_pos[2] - v->pos[2];
    int dist = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
    s64 dot = ((s64)fx * dx + (s64)fz * dz) >> 12;
    s64 cross = ((s64)fx * dz - (s64)fz * dx) >> 12;

    switch (v->state)
    {
    case VER_STALK:
    {
        if (cross < -(s64)dist / 20 || (dot < 0 && cross <= 0))
            v->yaw += TURN;
        else if (cross > (s64)dist / 20 || dot < 0)
            v->yaw -= TURN;
        if (vulnerable && dist < TAIL_FROM && dot * 2 > dist)
        {
            set_state(v, VER_TAIL, VER_TAIL_CLIP);
            break;
        }
        if (--v->next_leap <= 0 && dist > TAIL_FROM)
        {
            set_state(v, VER_UP, VER_JUMP);
            break;
        }
        int want = dist > RUN_FROM ? VER_RUN : VER_WALK;
        if (want != v->clip)
        {
            v->clip = want;
            v->frame = 0;
            len = clip_len(v);
        }
        if (dist > TAIL_FROM - inttof32(3))
        {
            int sp = want == VER_RUN ? RUN_SPEED : WALK_SPEED;
            v->pos[0] += mulf32(fx, sp);
            v->pos[2] += mulf32(fz, sp);
            ground_collide_n(gr, &v->pos[0], &v->pos[2], v->feet + STEP_UP, v->feet + HEIGHT, RADIUS, 1);
            ground_height(gr, v->pos[0], v->pos[2], v->feet + STEP_UP, STEP_UP, &v->feet);
            v->pos[1] = v->feet;
        }
        if (v->frame >= len)
            v->frame -= len;
        break;
    }
    case VER_TAIL:
        // 尾はクリップの半分を少し過ぎたところで振り下ろされる。
        if (!v->struck && v->frame >= len * 3 / 5)
        {
            v->struck = 1;
            if (vulnerable && dist < REACH && dot * 2 > dist)
                ev->hit_leon = TAIL_DAMAGE;
        }
        if (v->frame >= len)
            set_state(v, VER_STALK, VER_WALK);
        break;
    case VER_HIT:
        if (v->frame >= len)
            set_state(v, VER_STALK, VER_WALK);
        break;
    case VER_FROZEN:
        if (v->timer >= FROZEN_FRAMES)
        {
            ev->thawed = 1;
            set_state(v, VER_STALK, VER_WALK);
        }
        break;
    case VER_UP:
        if (v->frame >= len)
            set_state(v, VER_GONE, VER_JUMP);
        break;
    case VER_GONE:
        if (v->timer >= GONE_FRAMES)
        {
            // レオンの横、彼がいた所の片側（でなければ反対側）に、レオンの後ろのカメラから見える所に降りる。
            int ld = dist > 0 ? dist : 1;
            for (int side = 1; side >= -1; side -= 2)
            {
                int x = leon_pos[0] + side * mulf32(divf32(dz, ld), DROP_BEHIND);
                int z = leon_pos[2] - side * mulf32(divf32(dx, ld), DROP_BEHIND);
                int y = leon_feet;
                if (ground_height(gr, x, z, leon_feet + STEP_UP, STEP_UP, &y))
                {
                    v->pos[0] = x;
                    v->pos[2] = z;
                    v->feet = v->pos[1] = y;
                    break;
                }
            }
            {
                int ex = leon_pos[0] - v->pos[0], ez = leon_pos[2] - v->pos[2];
                // レオンの方を向く：(sin, cos) が (ex, ez) の方を指す角度。
                int best = 0;
                s64 best_dot = -((s64)1 << 62);
                for (int a = 0; a < DEGREES_IN_CIRCLE; a += DEGREES_IN_CIRCLE / 64)
                {
                    s64 d = (s64)sinLerp(a) * ex + (s64)cosLerp(a) * ez;
                    if (d > best_dot)
                    {
                        best_dot = d;
                        best = a;
                    }
                }
                v->yaw = best;
            }
            set_state(v, VER_DROP, VER_DROP_CLIP);
        }
        break;
    case VER_DROP:
        if (!v->struck && v->frame >= len / 3)
        {
            v->struck = 1;
            ev->landed = 1;
        }
        if (v->frame >= len)
        {
            v->next_leap = LEAP_EVERY;
            set_state(v, VER_STALK, VER_WALK);
        }
        break;
    case VER_DEAD:
        if (v->frame > len - 1)
            v->frame = len - 1;
        break;
    }
    // レオンにめり込まない。
    if (v->state != VER_DEAD && v->state != VER_GONE && dist > 0 && dist < RADIUS + LEON_ROOM)
    {
        v->pos[0] = leon_pos[0] - mulf32(divf32(dx, dist), RADIUS + LEON_ROOM);
        v->pos[2] = leon_pos[2] - mulf32(divf32(dz, dist), RADIUS + LEON_ROOM);
        ground_collide_n(gr, &v->pos[0], &v->pos[2], v->feet + STEP_UP, v->feet + HEIGHT, RADIUS, 1);
    }
}

int verdugo_freeze(Verdugo *v, const int pos[3], int range)
{
    if (v->state == VER_DEAD || v->state == VER_GONE || v->state == VER_UP)
        return 0;
    int dx = v->pos[0] - pos[0], dz = v->pos[2] - pos[2];
    if ((s64)dx * dx + (s64)dz * dz >= (s64)range * range)
        return 0;
    if (v->state != VER_FROZEN)
    {
        int clip = v->clip, frame = v->frame;
        set_state(v, VER_FROZEN, clip);
        v->frame = frame;   // 立っていた所で固まる
    }
    else
        v->timer = 0;
    return 1;
}

int verdugo_raycast(const Verdugo *v, const int p[3], const int d[3], int *t_best)
{
    if (v->state == VER_DEAD || v->state == VER_GONE || v->state == VER_UP)
        return 0;
    s64 aa = ((s64)d[0] * d[0] + (s64)d[2] * d[2]) >> 12;
    if (aa == 0)
        return 0;
    s64 fx = p[0] - v->pos[0], fz = p[2] - v->pos[2];
    s64 b = (fx * d[0] + fz * d[2]) >> 12;
    s64 c = ((fx * fx + fz * fz) >> 12) - (((s64)RADIUS * RADIUS) >> 12);
    s64 disc = b * b - aa * c;
    if (disc < 0)
        return 0;
    s64 root = (s64)sqrt64(disc);
    s64 t = ((-b - root) << 12) / aa;
    if (t <= 0 && ((-b + root) << 12) / aa > 0)
        t = 1;   // 至近距離
    if (t <= 0 || t >= *t_best)
        return 0;
    s64 y = p[1] + ((d[1] * t) >> 12) - v->feet;
    if (y < 0 || y > HEIGHT)
        return 0;
    *t_best = (int)t;
    return 1;
}

void verdugo_shot(Verdugo *v, int power)
{
    if (v->state == VER_DEAD || v->state == VER_GONE || v->state == VER_UP)
        return;
    int frozen = v->state == VER_FROZEN;
    v->hp -= frozen ? power * FROZEN_TIMES : power;
    if (v->hp <= 0)
    {
        set_state(v, VER_DEAD, VER_DIE);
        return;
    }
    if (frozen)
        return;
    v->flinch += power;
    if (v->flinch >= FLINCH_MAX && v->state == VER_STALK)
    {
        v->flinch = 0;
        set_state(v, VER_HIT, VER_HIT_CLIP);
    }
}

void verdugo_push(const Verdugo *v, int *x, int *z, int r)
{
    if (v->state == VER_DEAD || v->state == VER_GONE)
        return;
    int ox = *x - v->pos[0], oz = *z - v->pos[2], min = r + RADIUS;
    int d = (int)sqrt64((u64)((s64)ox * ox + (s64)oz * oz));
    if (d > 0 && d < min)
    {
        *x = v->pos[0] + mulf32(divf32(ox, d), min);
        *z = v->pos[2] + mulf32(divf32(oz, d), min);
    }
}

void verdugo_draw(Verdugo *v, u32 poly_fmt)
{
    if (v->state == VER_GONE || !view_visible(v->pos, RADIUS * 6))
        return;
    int clip = v->clip, frame = v->frame;
#ifdef TEST_VER_VIEW   // テスト用ビルド：クリップを順に、3秒ずつ
    static u32 t;
    t++;
    clip = (t / 180) % v->anims.count;
    frame = (int)(t % 180) * ANIM_STEP % inttof32(v->anims.frames[clip] > 0 ? v->anims.frames[clip] : 1);
    v->clip = clip;
#endif
    static u32 tick;
    if (v->state != VER_FROZEN && (tick++ % 2 == 0 || v->state == VER_TAIL || v->state == VER_DROP))
        anim_pose(&v->anims, clip, frame, &v->model, v->pose);
    model_draw(&v->model, v->pos, v->yaw, poly_fmt, (const int (*)[12])v->pose);
}

void verdugo_draw_fx(Verdugo *v, const Sheet *fx)
{
    if (v->state != VER_FROZEN)
        return;
    // 霜：彼の上に薄い青のもや、氷がとけるにつれて薄くなる。
    int alpha = 20 - v->timer * 14 / FROZEN_FRAMES;
    for (int i = 0; i < 2; i++)
    {
        int p[3] = { v->pos[0], v->feet + inttof32(7) + i * inttof32(10), v->pos[2] };
        billboard(fx, 0, 0, 64, 64, p, inttof32(13), inttof32(13), v->timer * 60 + i * 9000, 0, RGB15(22, 28, 31), alpha, 50 + i, 0);
    }
}
