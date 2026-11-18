#include <string.h>

#include "drops.h"

// 半透明ポリゴンの ID（0..63。同じ ID 同士は重ねて混ぜない）：ドロップと効果の層ごとに1つ。
// シーン（1..4）、弾（10..14）、HUD（20..24）とは別。
#define ID_BURST 30
#define ID_BEAM  38
#define ID_STAR  46   // 星の4分の1ずつの4枚は1つを共有：横に並ぶので
#define ID_DOT   54
// 拾える範囲（ゲームと同じ：GM_CtrlMain_Field::Check_GetDropItm、Set_DropItemEntry）：
// 床に沿って、アイテムの半径 2000 + レオンの幅（MoveCol_R 1000）以内。高さはアイテムが
// 足元の 2000 下から、身長（4800）+ 2000 上まで（ゲーム単位、1/256）。
#define PICK_RADIUS  (inttof32(3000) / 256)
#define PICK_BELOW   (inttof32(2000) / 256)
#define PICK_ABOVE   (inttof32(6800) / 256)
#define ALOFT_ABOVE  inttof32(6)           // 床からこれより高い宝は手が届かない
#define HIT_RADIUS   inttof32(7)            // 高い所の宝（撃って落とす）
#define BURST_FRAMES 24
#define POLY_FX      (POLY_FOG)

void drops_init(Drops *s, const Sheet *star, const Sheet *beam, const Sheet *spark, const Sheet *items)
{
    memset(s, 0, sizeof(*s));
    s->star = star;
    s->beam = beam;
    s->spark = spark;
    s->items = items;
}

void drops_clear(Drops *s)
{
    for (int i = 0; i < DROPS_MAX; i++)
        s->d[i].active = s->d[i].burst = 0;
}

void drops_spawn(Drops *s, const int ground[3], int type, int delay)
{
    drops_spawn_value(s, ground, type, delay, 0);
}

void drops_spawn_value(Drops *s, const int ground[3], int type, int delay, int value)
{
    for (int i = 0; i < DROPS_MAX; i++)
    {
        Drop *d = &s->d[i];
        if (d->active || d->burst)
            continue;
        memcpy(d->pos, ground, sizeof(d->pos));
        d->type = type;
        d->value = value;
        d->delay = delay;
        d->age = 0;
        memcpy(d->land, ground, sizeof(d->land));
        d->aloft = 0;
        d->active = 1;
        return;
    }
}

void drops_place_treasure(Drops *s, const int pos[3], const int land[3], int value)
{
    drops_spawn_value(s, pos, DROP_TREASURE, 0, value);
    for (int i = 0; i < DROPS_MAX; i++)
    {
        Drop *d = &s->d[i];
        if (d->active && d->type == DROP_TREASURE && d->pos[0] == pos[0] && d->pos[2] == pos[2])
        {
            memcpy(d->land, land, sizeof(d->land));
            d->aloft = pos[1] - land[1] > ALOFT_ABOVE;
            if (!d->aloft)
                memcpy(d->pos, land, sizeof(d->pos));
        }
    }
}

int drops_raycast(const Drops *s, const int p[3], const int d[3], int *t_best)
{
    int found = -1;
    for (int i = 0; i < DROPS_MAX; i++)
    {
        const Drop *t = &s->d[i];
        if (!t->active || !t->aloft)
            continue;
        // 光の周りの球（点の3上）。
        s64 fx = p[0] - t->pos[0], fy = p[1] - (t->pos[1] + inttof32(3)), fz = p[2] - t->pos[2];
        s64 b = (fx * d[0] + fy * d[1] + fz * d[2]) >> 12;
        s64 c = ((fx * fx + fy * fy + fz * fz) >> 12) - (((s64)HIT_RADIUS * HIT_RADIUS) >> 12);
        s64 disc = ((b * b) >> 12) - c;
        if (disc < 0)
            continue;
        s64 tt = -b - (s64)sqrt64((u64)(disc << 12));
        if (tt <= 0 || tt >= *t_best)
            continue;
        *t_best = (int)tt;
        found = i;
    }
    return found;
}

void drops_knock(Drops *s, int i)
{
    Drop *d = &s->d[i];
    if (!d->active || !d->aloft)
        return;
    d->burst = BURST_FRAMES / 2;   // 当たった所にきらめき…
    memcpy(d->pos, d->land, sizeof(d->pos));   // …宝は下の床に落ちる
    d->aloft = 0;
    d->age = 0;
}

int drops_in_reach(const Drops *s, const int leon[3])
{
    int best = -1;
    s64 best_d = 0;
    for (int i = 0; i < DROPS_MAX; i++)
    {
        const Drop *d = &s->d[i];
        if (!d->active || d->delay > 0 || d->aloft)
            continue;
        int dx = leon[0] - d->pos[0], dz = leon[2] - d->pos[2], up = d->pos[1] - leon[1];
        s64 dd = (s64)dx * dx + (s64)dz * dz;
        if (up >= -PICK_BELOW && up <= PICK_ABOVE && dd <= (s64)PICK_RADIUS * PICK_RADIUS && (best < 0 || dd < best_d))
        {
            best = i;
            best_d = dd;
        }
    }
    return best;
}

int drops_update(Drops *s, const int leon[3], int take)
{
    int picked = -1;
    int near = leon && take ? drops_in_reach(s, leon) : -1;
    for (int i = 0; i < DROPS_MAX; i++)
    {
        Drop *d = &s->d[i];
        if (d->burst > 0)
            d->burst--;
        if (!d->active)
            continue;
        if (d->delay > 0)
        {
            d->delay--;
            continue;
        }
        d->age++;
        if (i == near)
        {
            picked = d->type;
            s->picked_value = d->value;
            d->active = 0;
            d->burst = BURST_FRAMES;
        }
    }
    return picked;
}

void drops_icon(int type, int value, int *sx, int *sy)
{
    // アイテムのシート（main.c の item_icon() の並びの 2d_itemic_00）：銃ごとの弾箱
    // （ハンドガン、マグナム、ショットガン、ライフル、TMP：アイテム 20、21、22、23、24）、緑ハーブ
    // （2行目の3番目）。3行目は宝（真珠、アメジスト、ルビー：ルビーを使う。お金は真珠）。
    static const u8 ammo[5][2] = { { 64, 96 }, { 32, 24 }, { 96, 96 }, { 0, 24 }, { 64, 24 } };
    if (type == DROP_AMMO)
    {
        int w = value / 1000;
        *sx = ammo[w < 5 ? w : 0][0];
        *sy = ammo[w < 5 ? w : 0][1];
        return;
    }
    *sx = type == DROP_HERB || type == DROP_TREASURE ? 64 : 0;
    *sy = type == DROP_TREASURE || type == DROP_MONEY ? 72 : type == DROP_HERB ? 48 : 0;
}

void drops_draw(Drops *s)
{
    for (int i = 0; i < DROPS_MAX; i++)
    {
        Drop *d = &s->d[i];
        int p[3] = { d->pos[0], d->pos[1], d->pos[2] };
        if (d->burst > 0)
        {
            // 拾ったとき：黄色いきらめきが膨らんで消える（eff_all のきらめきの絵）。
            int t = BURST_FRAMES - d->burst;
            p[1] += inttof32(3) + t * floattof32(0.15);
            int size = inttof32(3) + t * floattof32(0.25);
            billboard(s->spark, 0, 0, 64, 64, p, size, size, t * 300, 0, RGB15(31, 28, 8), 1 + d->burst * 30 / BURST_FRAMES, ID_BURST + i, POLY_FX);
        }
        if (!d->active || d->delay > 0)
            continue;
        int grow = d->age < 20 ? d->age * inttof32(1) / 20 : inttof32(1);   // フェードしながら大きくなって現れる
        int pulse = sinLerp(d->age * 300);                                 // -1..1、周期約1.4秒

        // 目印の色は中身で変える（原作と同じ）：弾は黄、ハーブは緑、お金と宝は白。
        u16 tint = d->type == DROP_AMMO ? RGB15(31, 27, 4) : d->type == DROP_HERB ? RGB15(8, 31, 8) : RGB15(31, 31, 31);
        u16 tint_soft = d->type == DROP_AMMO ? RGB15(31, 29, 14) : d->type == DROP_HERB ? RGB15(18, 31, 18) : RGB15(31, 31, 31);

        // 光の柱（fix_all #7）：ドロップの上に立つ、柔らかい縦長の光。
        int b[3] = { p[0], p[1] + inttof32(6), p[2] };
        billboard(s->beam, 0, 0, 32, 64, b, mulf32(inttof32(6), grow), inttof32(14), 0, 0, tint,
                  20 + (pulse > 0 ? pulse : 0) * 10 / 4096, ID_BEAM + i, POLY_FX);

        // アイテム本体：柱の中に浮かんで上下に揺れる。
        int ix, iy;
        drops_icon(d->type, d->type == DROP_AMMO ? s->ammo_gun * 1000 : d->value, &ix, &iy);
        int c[3] = { p[0], p[1] + inttof32(3) + pulse / 4, p[2] };
        billboard(s->items, ix, iy, 32, 24, c, mulf32(inttof32(4), grow), mulf32(inttof32(3), grow), 0, 0,
                  RGB15(31, 31, 31), 31, ID_STAR + i, POLY_FX);

        // きらめく星（fix_all #0）：絵の左上4分の1を中心のまわりに4方向へ反転させて並べる
        // （原作のモデルと同じ）。回転しながら明滅する。
        int st[3] = { p[0], c[1] + inttof32(2), p[2] };
        int size = mulf32(inttof32(2) + pulse / 2, grow), a = 22 + pulse * 8 / 4096;
        int ang = d->age * 120;
        for (int q = 0; q < 4; q++)
        {
            // 4分の1の q（左右、上下）：絵の内側の角 (32, 32) を中心に置き、外側へ反転させる。
            int one = inttof32(1);
            int x0 = q & 1 ? one : -one, y0 = q & 2 ? -one : one;
            billboard_rect(s->star, 0, 0, 32, 32, st, size, size, x0, y0, 0, 0, ang, 0,
                           tint_soft, a, ID_STAR + i, POLY_FX);
        }
        // 星の下のぼんやりした光の点（fix_all #0、右下4分の1）。
        billboard(s->star, 32, 32, 32, 32, st, mulf32(inttof32(3), grow), mulf32(inttof32(3), grow), 0, 0,
                  tint, 12 + (pulse > 0 ? pulse : 0) * 8 / 4096, ID_DOT + i, POLY_FX);
    }
}
