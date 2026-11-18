#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "garrador.h"

// gal_anim.bin のクリップ（ch_gal.mcas #0,2,4,6,8,10,11,14,16,17,18,19,20,21,22,23）。
// ポーズで見分けた：1 頭を回しながら立つ（聞き耳）、3 腕を振ってかがんだ13フレームのループ
// （走り）、12 最後に頭の高さで爪を前に突き出す、14 その腕を120フレーム突き出したまま
// （壁に刺さった）、4 かがんで起き上がる（ひるみ）、7 床に倒れてそのまま（死亡）。
// テスト用ビルド：-DTEST_GAL_VIEW で順に表示（番号は FPS 欄）。
#ifndef GAL_IDLE
#define GAL_IDLE    1
#endif
#ifndef GAL_RUN
#define GAL_RUN     3
#endif
#ifndef GAL_SLASH_CLIP
#define GAL_SLASH_CLIP 12
#endif
#ifndef GAL_STUCK_CLIP
#define GAL_STUCK_CLIP 14
#endif
#ifndef GAL_HIT_CLIP
#define GAL_HIT_CLIP 4
#endif
#ifndef GAL_DIE
#define GAL_DIE     7
#endif

#define ANIM_STEP     (inttof32(20) / 60)
#define RUN_STEP      (inttof32(20) / 60)   // （×2：走りとひるみは速く再生する）
#define CHARGE_SPEED  floattof32(0.42)      // レオンの走り（0.60）より少し遅い：よけられる
#define TURN          (900 / 6)             // 原作の RotSpd 1ティックあたり 900（1回転 65536。ch_param_5）
#define CHARGE_TURN   degreesToAngle(2)     // （突進中はその場で急に曲がれない）
#define RADIUS        floattof32(3)
#define HEIGHT        inttof32(19)
#define REACH         inttof32(8)           // 爪は前方にこれだけ届く
#define SLASH_DAMAGE  53                    // レオンの1200のうち原作の640（ch_param_5 のスクリプト33の最初の打撃。docs/NOTES.md）
// ch_param_5.bin（種類0、体のレコード0）から：体力 3500。弾は胴では火力の7%、背中の寄生体では
// 100% 減らす。ひるみの合計に 100% / 200% を足し、400 でよろめく（docs/NOTES.md）。
#define HP            3500
#define BODY_PCT      7
#define BACK_PCT      100
#define FLINCH_BODY   100
#define FLINCH_BACK   200
#define FLINCH_MAX    400
#define CHARGE_MAX    150                   // 突進が続く最大フレーム数
#define STUCK_FRAMES  150                   // 爪が壁に刺さっている
#define CALM_FRAMES   100                   // 斬りか突進の後、また聞き耳を立てるまで
#define STEP_UP       inttof32(4)
#define LEON_ROOM     floattof32(3.5)       // レオンの体と、少し

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

int garrador_load(Garrador *g)
{
    if (g->loaded)
        return 0;
    size_t ms, as;
    g->model_buf = read_all("nitro:/gal.bin", &ms);
    g->anim_buf = read_all("nitro:/gal_anim.bin", &as);
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

void garrador_share(Garrador *g, const Garrador *from)
{
    garrador_unload(g);
    g->model = from->model;
    g->model.async_list = NULL;   // 自分用の描画リスト（最初の描画で作る）
    g->model.async_words = 0;
    g->anims = from->anims;
    g->shared = 1;
}

void garrador_unload(Garrador *g)
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

static void set_state(Garrador *g, int state, int clip)
{
    g->state = state;
    g->clip = clip;
    g->frame = 0;
    g->timer = 0;
    g->struck = 0;
}

void garrador_place(Garrador *g, const Ground *gr, int x, int z, int ref_y, int yaw)
{
    g->pos[0] = x;
    g->pos[2] = z;
    g->feet = ref_y;
    ground_height(gr, x, z, ref_y, STEP_UP, &g->feet);
    g->pos[1] = g->feet;
    g->yaw = yaw;
    g->hp = HP;
    g->flinch = 0;
    set_state(g, GAL_LISTEN, GAL_IDLE);
    g->timer = CALM_FRAMES;
}

static int clip_len(const Garrador *g)
{
    int n = g->anims.frames[g->clip];
    return inttof32(n > 0 ? n : 1);
}

void garrador_hear(Garrador *g, const int pos[3], int range)
{
    if (g->state != GAL_LISTEN && g->state != GAL_CHARGE)
        return;
    if (g->state == GAL_LISTEN && g->timer < CALM_FRAMES)
        return;
    int dx = pos[0] - g->pos[0], dz = pos[2] - g->pos[2];
    if ((s64)dx * dx + (s64)dz * dz >= (s64)range * range)
        return;
    memcpy(g->target, pos, sizeof(g->target));
    if (g->state != GAL_CHARGE)
        set_state(g, GAL_CHARGE, GAL_RUN);
    else
        g->timer = 0;   // 新しい物音：突進を続け、そちらへ向かう
}

static int in_front(const Garrador *g, const int p[3], int reach)
{
    int dx = p[0] - g->pos[0], dz = p[2] - g->pos[2];
    int d = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
    s64 dot = ((s64)sinLerp(g->yaw) * dx + (s64)cosLerp(g->yaw) * dz) >> 12;
    return d < reach && dot * 2 > d;
}

void garrador_update(Garrador *g, const Ground *gr, const int leon_pos[3], const int *ally_pos, int vulnerable, GarradorEvents *ev)
{
    ev->hit_leon = ev->hit_ally = ev->roar = ev->down = 0;
    int was = g->state;
    g->frame += g->state == GAL_CHARGE || g->state == GAL_HIT ? RUN_STEP * 2 : ANIM_STEP;
    g->timer++;
    int len = clip_len(g);
    int fx = sinLerp(g->yaw), fz = cosLerp(g->yaw);
    int lx = leon_pos[0] - g->pos[0], lz = leon_pos[2] - g->pos[2];
    int ldist = (int)sqrt64((u64)((s64)lx * lx + (s64)lz * lz));
    s64 ldot = ((s64)fx * lx + (s64)fz * lz) >> 12;

    switch (g->state)
    {
    case GAL_LISTEN:
        // 頭をかしげて立つ。時々ゆっくり向きを変える。
        if ((g->timer / 120) % 3 == 1)
            g->yaw += TURN;
        if (g->frame >= len)
            g->frame -= len;
        break;
    case GAL_CHARGE:
    {
        // レオン（かアシュリー）が前方の届く範囲に：爪。
        if ((vulnerable && ldist < REACH && ldot * 2 > ldist) || (ally_pos && in_front(g, ally_pos, REACH)))
        {
            set_state(g, GAL_SLASH, GAL_SLASH_CLIP);
            break;
        }
        int dx = g->target[0] - g->pos[0], dz = g->target[2] - g->pos[2];
        int dist = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
        s64 cross = ((s64)fx * dz - (s64)fz * dx) >> 12;
        s64 dot = ((s64)fx * dx + (s64)fz * dz) >> 12;
        if (cross < -(s64)dist / 20 || (dot < 0 && cross <= 0))
            g->yaw += CHARGE_TURN;
        else if (cross > (s64)dist / 20 || dot < 0)
            g->yaw -= CHARGE_TURN;
        int ox = g->pos[0], oz = g->pos[2];
        g->pos[0] += mulf32(fx, CHARGE_SPEED);
        g->pos[2] += mulf32(fz, CHARGE_SPEED);
        ground_collide_n(gr, &g->pos[0], &g->pos[2], g->feet + STEP_UP, g->feet + HEIGHT, RADIUS, 1);
        ground_height(gr, g->pos[0], g->pos[2], g->feet + STEP_UP, STEP_UP, &g->feet);
        g->pos[1] = g->feet;
        int mx = g->pos[0] - ox, mz = g->pos[2] - oz;
        int moved = (int)sqrt64((u64)((s64)mx * mx + (s64)mz * mz));
        if (g->timer > 10 && moved < CHARGE_SPEED / 3 && dot > 0)
            set_state(g, GAL_STUCK, GAL_STUCK_CLIP);   // 壁にぶつかった：爪が刺さる
        else if (dist < inttof32(4) || g->timer > CHARGE_MAX)
            set_state(g, GAL_LISTEN, GAL_IDLE);        // そこには何も無い
        else if (g->frame >= len)
            g->frame -= len;
        break;
    }
    case GAL_SLASH:
        if (!g->struck && g->frame >= len / 2)
        {
            g->struck = 1;
            if (vulnerable && ldist < REACH + RADIUS && ldot * 2 > ldist)
                ev->hit_leon = SLASH_DAMAGE;
            if (ally_pos && in_front(g, ally_pos, REACH + RADIUS))
                ev->hit_ally = SLASH_DAMAGE;
        }
        if (g->frame >= len)
            set_state(g, GAL_LISTEN, GAL_IDLE);
        break;
    case GAL_STUCK:
        if (g->frame > len - 1)
            g->frame = len - 1;
        if (g->timer >= STUCK_FRAMES)
        {
            set_state(g, GAL_LISTEN, GAL_IDLE);
            g->yaw += DEGREES_IN_CIRCLE / 2;   // 振り返りながら爪を抜く
        }
        break;
    case GAL_HIT:
        if (g->frame >= len)
        {
            // 吠えて、撃たれた方へ向かう。
            set_state(g, GAL_CHARGE, GAL_RUN);
            memcpy(g->target, leon_pos, sizeof(g->target));
        }
        break;
    case GAL_DEAD:
        if (g->frame > len - 1)
            g->frame = len - 1;
        break;
    }
    // レオンにめり込まない：腕の長さだけ離す（レオンと彼の体の半径）。
    if (g->state != GAL_DEAD && ldist > 0 && ldist < RADIUS + LEON_ROOM)
    {
        g->pos[0] = leon_pos[0] - mulf32(divf32(lx, ldist), RADIUS + LEON_ROOM);
        g->pos[2] = leon_pos[2] - mulf32(divf32(lz, ldist), RADIUS + LEON_ROOM);
        ground_collide_n(gr, &g->pos[0], &g->pos[2], g->feet + STEP_UP, g->feet + HEIGHT, RADIUS, 1);
    }
    if (g->state == GAL_CHARGE && was != GAL_CHARGE)
        ev->roar = 1;
    if (g->state == GAL_LISTEN && was != GAL_LISTEN)
        g->timer = 0;   // CALM_FRAMES の間おとなしい
}

int garrador_raycast(const Garrador *g, const int p[3], const int d[3], int *t_best, int *back)
{
    if (g->state == GAL_DEAD)
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
        t = 1;   // 至近距離
    if (t <= 0 || t >= *t_best)
        return 0;
    s64 y = p[1] + ((d[1] * t) >> 12) - g->feet;
    if (y < 0 || y > HEIGHT)
        return 0;
    *t_best = (int)t;
    // 背後から：弾は彼の向いている向きに飛ぶ。寄生体は背中の上の方。
    s64 along = ((s64)sinLerp(g->yaw) * d[0] + (s64)cosLerp(g->yaw) * d[2]) >> 12;
    *back = along > 0 && y > HEIGHT / 2;
    return 1;
}

void garrador_shot(Garrador *g, int power, int back)
{
    if (g->state == GAL_DEAD)
        return;
    g->hp -= power * (back ? BACK_PCT : BODY_PCT) / 100;
    if (g->hp <= 0)
    {
        set_state(g, GAL_DEAD, GAL_DIE);
        return;
    }
    g->flinch += power * (back ? FLINCH_BACK : FLINCH_BODY) / 100;
    if (g->flinch >= FLINCH_MAX && g->state != GAL_SLASH)
    {
        g->flinch = 0;
        set_state(g, GAL_HIT, GAL_HIT_CLIP);   // よろめく
    }
}

void garrador_push(const Garrador *g, int *x, int *z, int r)
{
    if (g->state == GAL_DEAD)
        return;
    int ox = *x - g->pos[0], oz = *z - g->pos[2], min = r + RADIUS;
    int d = (int)sqrt64((u64)((s64)ox * ox + (s64)oz * oz));
    if (d > 0 && d < min)
    {
        *x = g->pos[0] + mulf32(divf32(ox, d), min);
        *z = g->pos[2] + mulf32(divf32(oz, d), min);
    }
}

void garrador_draw(Garrador *g, u32 poly_fmt)
{
    if (!view_visible(g->pos, RADIUS * 4))
        return;
    int clip = g->clip, frame = g->frame;
#ifdef TEST_GAL_VIEW   // テスト用ビルド：クリップを順に、3秒ずつ
    static u32 t;
    t++;
    clip = (t / 180) % g->anims.count;
    frame = (int)(t % 180) * ANIM_STEP % inttof32(g->anims.frames[clip] > 0 ? g->anims.frames[clip] : 1);
    g->clip = clip;
#endif
    static u32 tick;
    if (tick++ % 3 == 1 || g->state == GAL_SLASH)   // （3フレームに1回、アシュリーとはずらして）
        anim_pose(&g->anims, clip, frame, &g->model, g->pose);
    model_draw(&g->model, g->pos, g->yaw, poly_fmt, (const int (*)[12])g->pose);
}
