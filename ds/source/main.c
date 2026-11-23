#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <nds.h>

#include "anim.h"
#include "drops.h"
#include "enemy.h"
#include "objects.h"
#include "ground.h"
#include "model.h"
#include "music.h"
#include "sfx.h"
#include "sheet.h"
#include "text.h"
#include "vital.h"
#include <string.h>

#include "barrel_bin.h"
#include "beam_bin.h"
#include "bell_bin.h"
#include "hang_bin.h"
#include "tank_bin.h"
#include "boom_bin.h"
#include "crate_bin.h"
#include "door_bin.h"
#include "font_bin.h"
#include "ganado_anim_bin.h"
#include "ganado_bin.h"
#include "cha_anim_bin.h"
#include "cha_bin.h"
#include "axe_bin.h"
#include "fork_bin.h"
#include "gna_w0_bin.h"
#include "gna_w1_bin.h"
#include "sickle_bin.h"
#include "gun_bin.h"
#include "hud_bin.h"
#include "iconbmp_bin.h"
#include "items_bin.h"
#include "ladder_bin.h"
#include "vitalbmp_bin.h"
#include "leon_anim_bin.h"
#include "leon_bin.h"
#include "leon_gun_bin.h"
#include "knife_bin.h"
#include "leon_knife_bin.h"
#include "medal_bin.h"
#include "menu_bin.h"
#include "music_bin.h"
#include "result_bin.h"
#include "sfx_bin.h"
#include "spark_bin.h"
#include <fat.h>
#include <filesystem.h>
#include <stddef.h>
#include "stages.h"
#include "ashley.h"
#include "giant.h"
#include "garrador.h"
#include "cutins.h"
#include "verdugo.h"
#include "missile.h"
#include "stagesel_bin.h"
#include "star_bin.h"
#include "title_bin.h"

static Model leon, gun, ganado;
static Sheet hud, items, star, beam, spark;
static Drops drops;

enum { PIC_TITLE, PIC_STAGESEL, PIC_RESULT };
static Sheet picture;
static int picture_id = -1;

static Model stage;
static Ground stage_ground;
static void *stage_buf, *ground_buf;
static int stage_loaded = -1;

static void *read_file(const char *path)
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
    return p;
}

static long file_size(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

static int load_stage(int i)
{
    if (i == stage_loaded)
        return 0;
    if (stage_loaded >= 0)
    {
        model_free(&stage);
        free(stage_buf);
        free(ground_buf);
        stage_buf = ground_buf = NULL;
    }
    stage_loaded = -1;
    char sp[32], gp[32];
    snprintf(sp, sizeof(sp), "nitro:/stage%02d.bin", i + 1);
    snprintf(gp, sizeof(gp), "nitro:/ground%02d.bin", i + 1);
    stage_buf = read_file(sp);
    ground_buf = read_file(gp);
    if (!stage_buf || !ground_buf)
        return -1;
    memset(&stage, 0, sizeof(stage));
    int err = model_load(&stage, stage_buf, file_size(sp));
    if (!err)
        err = ground_load(&stage_ground, ground_buf, file_size(gp)) * 10;
    if (!err)
        stage_loaded = i;
    return err;
}

static void set_haze(const u8 c[3])
{
    glClearColor(c[0] >> 3, c[1] >> 3, c[2] >> 3, 31);
    glFogColor(c[0] >> 3, c[1] >> 3, c[2] >> 3, 31);
}

static Model cha_model;
static AnimSet cha_anims;
static int cha_loaded;

static void cha_unload(void)
{
    if (cha_loaded)
        model_free(&cha_model);
    cha_loaded = 0;
}

#define ASH_STAGE_A    4
#define ASH_STAGE_B    5
#define ASH_STAGE_C    6
#define ASH_STAGE_D    7
#define ASH_STAGE_E    9

#define CASTLE_FROM    6

#ifndef ENEMY_SIGHT
#define ENEMY_SIGHT    inttof32(150)
#endif
static Model jya_model;
static AnimSet jya_anims, jya_w0;
static void *jya_buf[3];
static int jya_loaded;
static Model cw_board, cw_xbow, cw_staff, bolt_model;
static AnimSet jya_w1, jya_w2;
static void *cw_buf[6];
static int cw_loaded;
static Enemies ganados;

static void jya_unload(void)
{
    enemies_arm_cultists(&ganados, NULL, NULL, NULL, NULL, NULL);
    if (cw_loaded)
    {
        model_free(&cw_board);
        model_free(&cw_xbow);
        model_free(&cw_staff);
        model_free(&bolt_model);
        cw_loaded = 0;
    }
    for (int i = 0; i < 6; i++)
    {
        free(cw_buf[i]);
        cw_buf[i] = NULL;
    }
    if (!jya_loaded)
        return;
    model_free(&jya_model);
    for (int i = 0; i < 3; i++)
    {
        free(jya_buf[i]);
        jya_buf[i] = NULL;
    }
    jya_loaded = 0;
}

static void jya_arm(void)
{
    if (cw_loaded)
        enemies_arm_cultists(&ganados, &cw_board, &cw_xbow, &cw_staff, &jya_w1, &jya_w2);
    else
        enemies_arm_cultists(&ganados, NULL, NULL, NULL, NULL, NULL);
}

static int jya_load(void)
{
    if (jya_loaded)
        return 0;
    static const char *const path[3] = { "nitro:/jya.bin", "nitro:/jya_anim.bin", "nitro:/jya_w0.bin" };
    long n[3];
    for (int i = 0; i < 3; i++)
    {
        n[i] = file_size(path[i]);
        if (!(jya_buf[i] = read_file(path[i])))
            return -1;
    }
    memset(&jya_model, 0, sizeof(jya_model));
    if (model_load(&jya_model, jya_buf[0], n[0]) || anim_load(&jya_anims, jya_buf[1], n[1]) || anim_load(&jya_w0, jya_buf[2], n[2]))
        return -2;
    jya_loaded = 1;
#ifndef TEST_NO_CW
    {
        static const char *const cpath[6] = { "nitro:/cboard.bin", "nitro:/cxbow.bin", "nitro:/cstaff.bin", "nitro:/bolt.bin",
                                              "nitro:/jya_w1.bin", "nitro:/jya_w2.bin" };
        Model *const cm[4] = { &cw_board, &cw_xbow, &cw_staff, &bolt_model };
        long cn[6];
        int ok = 1;
        for (int i = 0; i < 4; i++)
            memset(cm[i], 0, sizeof(*cm[i]));
        for (int i = 0; i < 6 && ok; i++)
        {
            cn[i] = file_size(cpath[i]);
            ok = (cw_buf[i] = read_file(cpath[i])) != NULL;
        }
        for (int i = 0; i < 4 && ok; i++)
            ok = model_load(cm[i], cw_buf[i], cn[i]) == 0;
        ok = ok && anim_load(&jya_w1, cw_buf[4], cn[4]) == 0 && anim_load(&jya_w2, cw_buf[5], cn[5]) == 0
             && jya_w1.nodes == jya_model.node_count && jya_w2.nodes == jya_model.node_count;
        if (!ok)
            for (int i = 0; i < 4; i++)
                model_free(cm[i]);
        cw_loaded = ok;
    }
#endif
    return 0;
}

#define BOLTS_MAX     6
#define BOLT_SPEED    floattof32(1.5)
#define BOLT_LIFE     120
#define BOLT_R        floattof32(2.2)
#define BOLT_HEIGHT   inttof32(13)
#define BOLT_AIM      inttof32(11)
typedef struct { int pos[3], vel[3], life, damage; } Bolt;
static Bolt bolts[BOLTS_MAX];

static void bolt_fire(const int from[3], const int at[3], int damage)
{
    for (int i = 0; i < BOLTS_MAX; i++)
        if (bolts[i].life <= 0)
        {
            Bolt *b = &bolts[i];
            int d[3] = { at[0] - from[0], at[1] - from[1], at[2] - from[2] };
            int len = (int)sqrt64((u64)((s64)d[0] * d[0] + (s64)d[1] * d[1] + (s64)d[2] * d[2]));
            if (len <= 0)
                return;
            for (int k = 0; k < 3; k++)
            {
                b->pos[k] = from[k];
                b->vel[k] = (int)(((s64)d[k] * BOLT_SPEED) / len);
            }
            b->life = BOLT_LIFE;
            b->damage = damage;
            return;
        }
}

static void bolts_clear(void)
{
    for (int i = 0; i < BOLTS_MAX; i++)
        bolts[i].life = 0;
}

static int bolts_update(const int at[3], int vulnerable, const int *ally, int *ally_hurt)
{
    int hurt = 0;
    *ally_hurt = 0;
    for (int i = 0; i < BOLTS_MAX; i++)
    {
        Bolt *b = &bolts[i];
        if (b->life <= 0)
            continue;
        b->life--;
        int dir[3] = { divf32(b->vel[0], BOLT_SPEED), divf32(b->vel[1], BOLT_SPEED), divf32(b->vel[2], BOLT_SPEED) }, hit[3];
        int wall = ground_raycast(&stage_ground, b->pos, dir, BOLT_SPEED, hit, RAY_SHOT);
        for (int k = 0; k < 3; k++)
            b->pos[k] += b->vel[k];
        int dx = b->pos[0] - at[0], dz = b->pos[2] - at[2];
        if (!hurt && vulnerable && b->pos[1] > at[1] && b->pos[1] < at[1] + inttof32(17)
            && (s64)dx * dx + (s64)dz * dz < (s64)BOLT_R * BOLT_R)
        {
            hurt = b->damage;
            b->life = 0;
        }
        else if (ally && !*ally_hurt && b->pos[1] > ally[1] && b->pos[1] < ally[1] + inttof32(13)
                 && (s64)(b->pos[0] - ally[0]) * (b->pos[0] - ally[0]) + (s64)(b->pos[2] - ally[2]) * (b->pos[2] - ally[2]) < (s64)BOLT_R * BOLT_R)
        {
            *ally_hurt = b->damage;
            b->life = 0;
        }
        else if (wall)
            b->life = 0;
    }
    return hurt;
}

static void bolts_draw(void)
{
    if (!cw_loaded)
        return;
    for (int i = 0; i < BOLTS_MAX; i++)
        if (bolts[i].life > 0)
        {
            const Bolt *b = &bolts[i];
            int yaw = angle_to(b->vel[0], b->vel[2]);
            model_draw(&bolt_model, b->pos, yaw, POLY_ALPHA(31) | POLY_CULL_NONE, NULL);
        }
}

#define MISSILE_RANGE  inttof32(14)
#define MISSILE_DAMAGE 20
static Missiles missiles;
#ifdef TEST_SHOW_THROW
static int throws_seen;
#endif
#define ASH_SHOT_DAMAGE 25
static Ashley ash;
#define GIANT_STAGE_A  3
#define GIANT_STAGE_B  10
#ifndef GIANT_AHEAD
#define GIANT_AHEAD    70
#endif
#define GIANT_AHEAD_U  inttof32(GIANT_AHEAD)
#define GIANT_MAX      2
#ifndef GIANT2_DEG
#define GIANT2_DEG     40
#endif
#define GIANT2_FARTHER inttof32(30)
#define GIANT_RADIUS   inttof32(9)
static Giant giants[GIANT_MAX];
static int giant_count;
#define giant giants[0]
static int giants_down(void)
{
    for (int i = 0; i < giant_count; i++)
        if (giants[i].state != GIANT_DEAD)
            return 0;
    return 1;
}
static Giant *giant_at_hand(const int pos[3])
{
    for (int i = 0; i < giant_count; i++)
        if (giant_cuttable(&giants[i], pos))
            return &giants[i];
    return NULL;
}
#ifdef TEST_GX
static int gx_poly_peak, gx_vert_peak, gx_overflows, gx_underflows, gx_stage_poly, gx_stage_vert, gx_rdlines_min = 99;
#endif

#define GAL_STAGE      7
#define GAL_STAGE_2    8
#define GAL_MAX        2
#define GAL_HEAR_RUN   inttof32(60)
#define GAL_HEAR_NEAR  inttof32(10)
static Garrador gals[GAL_MAX];
static int gal_count; int garradors_down(void)
{
    for (int i = 0; i < gal_count; i++)
        if (gals[i].state != GAL_DEAD)
            return 0;
    return 1;
}
#define gal gals[0]

#define VER_STAGE      11
#ifndef VER_AHEAD
#define VER_AHEAD      50
#endif
#define TANK_RANGE     inttof32(34)
#ifndef ELEVATOR_FRAMES
#define ELEVATOR_FRAMES (240 * 60)
#endif
#define ELEVATOR_NOTE  150
static Verdugo ver;
static Model barrel_model;
static int barrel_loaded;

static void barrel_unload(void)
{
    if (barrel_loaded)
        model_free(&barrel_model);
    barrel_loaded = 0;
}

#ifdef TEST_PROF
static u32 prof_mark, prof_peak[8];
#define PROF(i) do { u32 t_ = cpuGetTiming(); u32 d_ = (t_ - prof_mark) / 3351; if (d_ > prof_peak[i]) prof_peak[i] = d_; prof_mark = t_; } while (0)
#else
#define PROF(i) ((void)0)
#endif

static void gimmicks_free(void);
static void plaga_unload(void);
static void use_picture(int id)
{
    static const void *const data[] = { title_bin, stagesel_bin, result_bin };
    static const size_t size[] = { title_bin_size, stagesel_bin_size, result_bin_size };
    if (id == picture_id)
        return;
    cha_unload();
    ashley_unload(&ash);
    for (int i = GIANT_MAX - 1; i >= 0; i--)
        giant_unload(&giants[i]);
    for (int i = GAL_MAX - 1; i >= 0; i--)
        garrador_unload(&gals[i]);
    verdugo_unload(&ver);
    barrel_unload();
    gimmicks_free();
    jya_unload();
    plaga_unload();
    sheet_free(&picture);
    picture_id = sheet_load(&picture, data[id], size[id]) == 0 ? id : -1;
}
static AnimSet leon_anims, leon_gun, ganado_anims, ganado_w0, ganado_w1;
static Model sickle_model, axe_model, fork_model;
static Model knife_model;
static AnimSet leon_knife;
#define KNIFE_SLASH_UP   1
#define KNIFE_SLASH_DOWN 3
#define KNIFE_READY_UP   5
#define KNIFE_READY_DOWN 6
#define KNIFE_HIT_FRAME  inttof32(6)
#define KNIFE_RANGE   inttof32(8)
#define KNIFE_REACH   inttof32(14)
#define KNIFE_PITCH   degreesToAngle(55)
#define KNIFE_CHEST   inttof32(13)
#ifdef TEST_SHOW_KNIFE
static int knife_probe = 9999, v_knife2 = 8888;
#endif
#ifdef TEST_SHOW_HEAP
static int heap_kb;
#endif
static int knife_object(const Objects *s, const int from[3], const int dir[3], int *t)
{
    static const s8 up[] = { 0, 4, -4, 8 };
    for (unsigned k = 0; k < sizeof(up); k++)
    {
        int f[3] = { from[0], from[1] + inttof32(up[k]), from[2] }, tt = *t;
        int i = objects_raycast(s, f, dir, &tt);
        if (i >= 0)
        {
            *t = tt;
            return i;
        }
    }
    return -1;
}
#define KNIFE_DAMAGE  130
#ifndef KNIFE_HAND
#define KNIFE_HAND    14
#endif
static Enemies ganados;
static Model plg_model;
static AnimSet plg_anims;
static void *plg_buf[2];
static int plg_loaded;

static void plaga_unload(void)
{
    enemies_set_plaga(&ganados, NULL, NULL);
    if (!plg_loaded)
        return;
    model_free(&plg_model);
    for (int i = 0; i < 2; i++)
    {
        free(plg_buf[i]);
        plg_buf[i] = NULL;
    }
    plg_loaded = 0;
}

static int plaga_load(void)
{
    if (!plg_loaded)
    {
        static const char *const path[2] = { "nitro:/plg.bin", "nitro:/plg_anim.bin" };
        long n[2];
        for (int i = 0; i < 2; i++)
        {
            n[i] = file_size(path[i]);
            if (!(plg_buf[i] = read_file(path[i])))
                return -1;
        }
        memset(&plg_model, 0, sizeof(plg_model));
        if (model_load(&plg_model, plg_buf[0], n[0]) || anim_load(&plg_anims, plg_buf[1], n[1]))
            return -2;
        plg_loaded = 1;
    }
    enemies_set_plaga(&ganados, &plg_model, &plg_anims);
    return 0;
}

static Model door_model, ladder_model, barrel_model, crate_model, medal_model;
static Model bell_model, tank_model;
static Model hang_model;
static int door_loaded, bell_loaded, tank_loaded, hang_loaded;

static int gimmick_model(Model *m, int *loaded, int want, const void *bin, size_t size)
{
    if (want && !*loaded)
        *loaded = model_load(m, bin, size) == 0;
    else if (!want && *loaded)
    {
        model_free(m);
        *loaded = 0;
    }
    return *loaded;
}

static void gimmicks_free(void)
{
    gimmick_model(&door_model, &door_loaded, 0, NULL, 0);
    gimmick_model(&bell_model, &bell_loaded, 0, NULL, 0);
    gimmick_model(&tank_model, &tank_loaded, 0, NULL, 0);
    gimmick_model(&hang_model, &hang_loaded, 0, NULL, 0);
}
static Sheet boom_sheet;
static Objects objs;

static const int ganado_spawn[][3] = {
    { 292, 0, 340 }, { 318, 0, 352 }, { 276, 0, 372 }, { 330, 0, 384 }, { 300, 0, 404 },
    { 262, 0, 350 }, { 344, 0, 362 }, { 284, 0, 420 }, { 316, 0, 428 }, { 300, 0, 320 } };
#define GANADO_COUNT ((int)(sizeof(ganado_spawn) / sizeof(ganado_spawn[0])))

#ifndef OBJ_FRAMES
#define OBJ_FRAMES     240
#endif
#ifndef BOSS_SURVIVE
#define BOSS_SURVIVE   (90 * 60)
#endif
#ifndef BOSS_LINES
#define BOSS_LINES     240
#endif
static const int boss_spawn[3] = { 300, 0, 380 };
#define MISSION_PTAS   5000
#define TREASURE_REACH inttof32(20)
#define TREASURE_PTAS  3000
#define GANADO_ACTIVE 5
#define GANADO_FROM_DOOR GANADO_COUNT

#define FOES_MAX 32
#define FOE_UNITS 5
#define WALL_TOP 80
#ifdef STAGE1_ENEMIES_ONLY
#define FOES_ON(stage) ((stage) == 0)
#else
#define FOES_ON(stage) 1
#endif
static const StageFoe *foe_list[FOES_MAX];
static int foe_n, foe_slots;
static u8 foe_left[FOES_MAX];
static s8 foe_slot[FOES_MAX];
static s8 foe_fixed[FOES_MAX];
static s8 slot_foe[ENEMY_MAX];
static u16 foe_groups;
static int foe_entered;

static __attribute__((unused)) int foes_standing(void)
{
    int n = 0;
    for (int f = 0; f < foe_n; f++)
        n += foe_slot[f] >= 0 && ganados.e[foe_slot[f]].state < ENEMY_DYING;
    return n;
}

static int foe_record_down(int recn)
{
    for (int f = 0; f < foe_n; f++)
        if (foe_list[f]->rec == recn)
            return foe_left[f] == 0 && (foe_slot[f] < 0 || ganados.e[foe_slot[f]].state >= ENEMY_DYING);
    return 0;
}

static int foe_groups_down(void)
{
    for (int f = 0; f < foe_n; f++)
        if ((foe_groups >> foe_list[f]->wave & 1) && !foe_record_down(foe_list[f]->rec))
            return 0;
    return 1;
}

#define FOE_CLEAR inttof32(30)
#define FOE_WAIT  (60 * 20)
static int foe_held;

typedef struct { s8 stage; s16 x0, z0, x1, z1, top, yard; } Room;
static const Room rooms[] = {
    { 5, 174, 217, 294, 316, 60, 30 },
};
typedef struct { s8 stage; s16 from, below; } Gallery;
static const Gallery galleries[] = {
    { 7, 36, 30 },
};
static const Room *room_of(int stage)
{
    for (unsigned i = 0; i < sizeof(rooms) / sizeof(rooms[0]); i++)
        if (rooms[i].stage == stage)
            return &rooms[i];
    return NULL;
}
static const FoeKind *foe_kind_of(const StageFoe *f)
{
    if (f->type == FOE_CULTIST)
        return &foe_kind_cultist[f->variant < foe_kind_cultist_count ? f->variant : 0];
    return &foe_kind_ganado[f->variant < foe_kind_ganado_count ? f->variant : 0];
}

static u8 ev_played[8], ev_deleted[8], ev_inside[8];
static u8 items_held[32];
static u16 mis_flags;
static int mis_frames;
static int mis_timer, mis_timer_end;
static u32 mis_timer_vb;
static volatile u32 vblank_count;
static int mis_flag_clear;
static s8 thing_obj[24];
static s16 foe_at[FOES_MAX][3];
static int ev_button = -1;
static int ev_press;
static s8 ev_scene[4];
static int ev_scenes;
static s16 ev_got[4];
static int ev_gots;
static int ev_bit(const u8 *b, int i) { return i >= 0 && i < 64 && (b[i >> 3] >> (i & 7) & 1); }
static void ev_set(u8 *b, int i) { if (i >= 0 && i < 64) b[i >> 3] |= 1 << (i & 7); }
static void ev_clr(u8 *b, int i) { if (i >= 0 && i < 64) b[i >> 3] &= ~(1 << (i & 7)); }
static int item_held(int id) { return id >= 0 && id < 256 && (items_held[id >> 3] >> (id & 7) & 1); }
static void item_set(int id, int on)
{
    if (id >= 0 && id < 256)
        items_held[id >> 3] = on ? items_held[id >> 3] | 1 << (id & 7) : items_held[id >> 3] & ~(1 << (id & 7));
}

#define EV_WALL_REACH inttof32(3)
static int ev_wall_touched(const StageFoes *sf, int w, const int leon_at[3])
{
    if (w < 0 || w >= sf->walls)
        return 0;
    const s16 *l = sf->wall[w];
    if (leon_at[1] < inttof32(l[4] - 4) || leon_at[1] > inttof32(l[5]))
        return 0;
    s64 ax = inttof32(l[0]), az = inttof32(l[1]), dx = inttof32(l[2]) - ax, dz = inttof32(l[3]) - az;
    s64 px = leon_at[0] - ax, pz = leon_at[2] - az, len2 = dx * dx + dz * dz;
    s64 t = len2 ? (px * dx + pz * dz) * 4096 / len2 : 0;
    if (t < 0) t = 0;
    if (t > 4096) t = 4096;
    s64 ex = px - dx * t / 4096, ez = pz - dz * t / 4096;
    return ex * ex + ez * ez < (s64)EV_WALL_REACH * EV_WALL_REACH;
}

#define EV_LEON_W floattof32(2.5)
static int ev_near(int x, int y, int z, int reach, const int leon_at[3])
{
    if (reach <= 0)
        return 0;
    int dy = leon_at[1] - inttof32(y);
    if (dy < -inttof32(24) || dy > inttof32(8))
        return 0;
    s64 dx = leon_at[0] - inttof32(x), dz = leon_at[2] - inttof32(z), r = (s64)reach * 16 + EV_LEON_W;
    return dx * dx + dz * dz <= r * r;
}

static void ev_run(const StageFoes *sf, int i)
{
    const StageEvent *ev = &sf->event[i];
    ev_set(ev_played, i);
    for (int k = 0; k < ev->count; k++)
    {
        const StageOp *o = &sf->op[ev->first + k];
        switch (o->op)
        {
            case OP_ITEM:
                item_set(o->a, 1);
                if (ev_gots < 4)
                    ev_got[ev_gots++] = o->a;
                break;
            case OP_GIVE_ITEM: item_set(o->a, 0); break;
            case OP_FLAG: if (o->a >= 0 && o->a < 16) mis_flags = o->b ? mis_flags | 1 << o->a : mis_flags & ~(1 << o->a); break;
            case OP_DELETE: ev_set(ev_deleted, o->a); break;
            case OP_GROUP: if (o->a >= 0 && o->a < 16) foe_groups |= 1 << o->a; break;
            case OP_TIMER: mis_timer = 0; mis_timer_end = o->a * 60; break;
        }
    }
}

static int ev_cond(const StageFoes *sf, const StageEvent *ev, const int leon_at[3])
{
    switch (ev->cond)
    {
        case COND_NONE: case COND_DESTROYED: return 1;
        case COND_TIME: return mis_timer >= ev->value * 60 * 60;
        case COND_KILLS: return ev->value ? ganados.kills >= ev->value : foe_groups_down();
        case COND_RECORD_DOWN: return foe_record_down(ev->value);
        case COND_EVENT: return ev_bit(ev_played, ev->value);
        case COND_FLAG: return ev->value < 16 && (mis_flags >> ev->value & 1);
        case COND_ITEM: return item_held(ev->value);
        case COND_WALL: return ev_wall_touched(sf, ev->value, leon_at);
    }
    return 0;
}

static int ev_scene_own(int stage, int scene)
{
    return (stage == 4 && scene == CUT_ASHLEY) || (stage == 7 && scene == CUT_GARRADOR);
}
static void foes_scene_played(const StageFoes *sf, int scene)
{
    for (int i = 0; i < sf->events && i < 64; i++)
        if (!ev_bit(ev_played, i) && (sf->event[i].script[0] == scene || sf->event[i].script[1] == scene))
            ev_run(sf, i);
}

static void foes_events_start(const StageFoes *sf, int stage)
{
    memset(ev_played, 0, sizeof(ev_played));
    memset(ev_deleted, 0, sizeof(ev_deleted));
    memset(ev_inside, 0, sizeof(ev_inside));
    memset(items_held, 0, sizeof(items_held));
    mis_flags = 0;
    mis_frames = 0;
    mis_timer = mis_timer_end = 0;
    if (stage == 11)
        mis_timer_end = 240 * 60;
    ev_button = -1;
    ev_press = ev_scenes = ev_gots = 0;
    foe_groups = 1;
    mis_flag_clear = 0;
    for (int i = 0; stage != 11 && i < sf->events; i++)
        for (int k = 0; k < sf->event[i].count; k++)
            mis_flag_clear |= sf->op[sf->event[i].first + k].op == OP_FLAG && sf->op[sf->event[i].first + k].a == 0 && sf->op[sf->event[i].first + k].b;
    for (int i = 0; i < sf->events; i++)
        if (sf->event[i].ctg == EV_START)
            ev_run(sf, i);
    ev_gots = 0;
#ifdef TEST_HAVE_ITEM
    item_set(TEST_HAVE_ITEM, 1);
#endif
#ifdef TEST_ITEM
    item_set(TEST_ITEM, 1);
#endif
#ifdef TEST_EV_PLAYED
    ev_set(ev_played, TEST_EV_PLAYED);
#endif
}

enum { EVI_KEY, EVI_TREASURE, EVI_HERB, EVI_FILE };
typedef struct { s16 id; u8 kind; int price; const char *name; } EvItem;
static const EvItem ev_items[] = {
    { 40, EVI_HERB, 0, "Green Herb" }, { 41, EVI_HERB, 0, "Yellow Herb" }, { 42, EVI_HERB, 0, "Red Herb" },
    { 80, EVI_KEY, 0, "Emblem (Right Half)" }, { 81, EVI_KEY, 0, "Emblem (Left Half)" },
    { 82, EVI_KEY, 0, "Hexagonal Emblem" }, { 83, EVI_KEY, 0, "Insignia Key" }, { 84, EVI_KEY, 0, "Prison Key" },
    { 85, EVI_KEY, 0, "Castle Key" }, { 86, EVI_KEY, 0, "Old Key" },
    { 100, EVI_TREASURE, 2000, "Spinel" }, { 101, EVI_TREASURE, 3000, "Velvet Blue" },
    { 102, EVI_TREASURE, 10000, "Ruby" }, { 103, EVI_TREASURE, 10000, "Dirty Pocket Watch" },
    { 104, EVI_TREASURE, 10000, "Antique Pipe" }, { 105, EVI_TREASURE, 20000, "Pearl Bracelet" },
    { 106, EVI_TREASURE, 20000, "Amber Ring" }, { 107, EVI_TREASURE, 20000, "Gold Bangle" },
    { 108, EVI_TREASURE, 3000, "Elegant Mask" }, { 109, EVI_TREASURE, 3000, "Green Gem" },
    { 110, EVI_TREASURE, 3000, "Purple Gem" }, { 111, EVI_TREASURE, 3000, "Orange Gem" },
    { 119, EVI_TREASURE, 9000, "Crown" }, { 120, EVI_TREASURE, 11000, "Crown Jewel" },
    { 121, EVI_TREASURE, 13000, "Royal Insignia" },
    { 200, EVI_FILE, 0, "Game Manual 1" }, { 201, EVI_FILE, 0, "About Ashley" }, { 202, EVI_FILE, 0, "Game Manual 2" },
    { 203, EVI_FILE, 0, "Warning" }, { 204, EVI_FILE, 0, "Anonymous Letter" }, { 205, EVI_FILE, 0, "Closure of the Church" },
    { 206, EVI_FILE, 0, "Game Manual 3" }, { 207, EVI_FILE, 0, "Capture Luis Sera" }, { 208, EVI_FILE, 0, "Luis' Memo" },
    { 209, EVI_FILE, 0, "Luis' Memo 2" }, { 210, EVI_FILE, 0, "Notes on Grenades" }, { 211, EVI_FILE, 0, "Mine Thrower Manual" },
    { 212, EVI_FILE, 0, "Attache Case Manual" },
};
static const EvItem *ev_item(int id)
{
    for (unsigned i = 0; i < sizeof(ev_items) / sizeof(ev_items[0]); i++)
        if (ev_items[i].id == id)
            return &ev_items[i];
    return NULL;
}

#define EV_ASHLEY_NEAR inttof32(24)
static int ev_ashley_by(const int leon_at[3])
{
    return ash.on && !ash.wait && abs(ash.pos[0] - leon_at[0]) < EV_ASHLEY_NEAR && abs(ash.pos[2] - leon_at[2]) < EV_ASHLEY_NEAR
           && abs(ash.feet - leon_at[1]) < inttof32(12);
}

static void door_keys_update(const StageFoes *sf, const int leon_at[3])
{
    for (int i = 0; i < objs.count; i++)
    {
        Obj *o = &objs.o[i];
        if (o->kind != OBJ_SWING || o->locked)
            continue;
        int mx = o->hinge[0] - mulf32(cosLerp(o->yaw), inttof32(13) / 2), mz = o->hinge[1] + mulf32(sinLerp(o->yaw), inttof32(13) / 2);
        int keyed = 0;
        for (int e = 0; e < sf->events && e < 64 && !keyed; e++)
        {
            const StageEvent *ev = &sf->event[e];
            if (ev->ctg != EV_POINT || ev->cond != COND_ITEM || ev->trg < 0 || ev->trg >= sf->points
                || ev_bit(ev_played, e) || ev_bit(ev_deleted, e))
                continue;
            int px = inttof32(sf->point[ev->trg][0]), pz = inttof32(sf->point[ev->trg][2]);
            keyed = abs(px - mx) < inttof32(14) && abs(pz - mz) < inttof32(14);
        }
        if (o->keyed && !keyed)
            objects_open_door(&objs, i, leon_at);
        o->keyed = keyed;
    }
}

static void foes_events(const StageFoes *sf, int stage, const int leon_at[3])
{
    door_keys_update(sf, leon_at);
    mis_frames++;
    {
        u32 dv = vblank_count - mis_timer_vb;
        mis_timer_vb = vblank_count;
        mis_timer += dv > 30 ? 1 : (int)dv;
        if (mis_timer > mis_timer_end)
            mis_timer = mis_timer_end;
    }
    for (int f = 0; f < foe_n; f++)
        if (foe_slot[f] >= 0)
            for (int k = 0; k < 3; k++)
                foe_at[f][k] = f32toint(k == 1 ? ganados.e[foe_slot[f]].feet : ganados.e[foe_slot[f]].pos[k]);
    int button = -1, pressed = ev_press, press_ctg = -1, press_trg = 0;
    ev_press = 0;
    for (int i = 0; i < sf->events && i < 64; i++)
    {
        const StageEvent *ev = &sf->event[i];
        if (ev_bit(ev_deleted, i) || ev->ctg == EV_START || ev->ctg == EV_END
            || (ev_bit(ev_played, i) && !(ev->again && ev->ctg == EV_POINT)))
            continue;
        int on = 1;
        if (ev->ctg == EV_RECORD)
        {
            on = foe_record_down(ev->trg);
            for (int f = 0; on && f < foe_n; f++)
                if (foe_list[f]->rec == ev->trg && ev->get >= GET_TOUCH)
                    on = ev_near(foe_at[f][0], foe_at[f][1] + 8, foe_at[f][2], ev->reach, leon_at);
        }
        else if (ev->ctg == EV_OBJECT)
        {
            const Obj *o = ev->trg >= 0 && ev->trg < (int)sizeof(thing_obj) && thing_obj[ev->trg] >= 0 ? &objs.o[thing_obj[ev->trg]] : NULL;
            on = o && !o->alive && (ev->get < GET_TOUCH || ev_near(f32toint(o->pos[0]), f32toint(o->pos[1]) + 8, f32toint(o->pos[2]), ev->reach, leon_at));
        }
        else if (ev->ctg == EV_POINT)
        {
            on = ev->trg >= 0 && ev->trg < sf->points && ev_near(sf->point[ev->trg][0], sf->point[ev->trg][1], sf->point[ev->trg][2], ev->reach, leon_at);
            if (!on && stage == ASH_STAGE_E && ev->trg == 4)
                on = ev_near(sf->point[4][0], sf->point[4][1] + 68, sf->point[4][2], ev->reach, leon_at);
        }
        int was_inside = ev_bit(ev_inside, i);
        on = on && ev_cond(sf, ev, leon_at);
        if (on)
            ev_set(ev_inside, i);
        else
            ev_clr(ev_inside, i);
        if (!on)
            continue;
        int fire = 0;
        if (ev->get == GET_AUTO)
            fire = 1;
        else if (ev->get == GET_TOUCH)
            fire = !was_inside;
        else if (ev->get == GET_BUTTON || ev_ashley_by(leon_at))
        {
            if (button < 0)
            {
                button = i;
                fire = pressed;
                if (fire)
                {
                    press_ctg = ev->ctg;
                    press_trg = ev->trg;
                }
            }
            else
                fire = ev->ctg == press_ctg && ev->trg == press_trg
                       && !(ev->script[0] >= 0 && cut_find(stage, ev->script[0])) && !(ev->script[1] >= 0 && cut_find(stage, ev->script[1]));
        }
        if (!fire)
            continue;
        ev_run(sf, i);
        for (int k = 0; k < 2; k++)
            if (ev->script[k] >= 0 && ev_scenes < 4 && !ev_scene_own(stage, ev->script[k]) && cut_find(stage, ev->script[k]))
                ev_scene[ev_scenes++] = ev->script[k];
        if (button == i)
            pressed = 0;
    }
    ev_button = button;
}
static int foes_more_coming(const StageFoes *sf)
{
    int could = ganados.kills;
    for (int f = 0; f < foe_n; f++)
        if (foe_groups >> foe_list[f]->wave & 1)
            could += foe_left[f] + (foe_slot[f] >= 0 && ganados.e[foe_slot[f]].state < ENEMY_DYING);
    for (int i = 0; i < sf->events && i < 64; i++)
    {
        const StageEvent *ev = &sf->event[i];
        if (ev->ctg != EV_AUTO || ev_bit(ev_played, i) || ev_bit(ev_deleted, i))
            continue;
        int lets_in = 0;
        for (int k = 0; k < ev->count; k++)
            lets_in |= sf->op[ev->first + k].op == OP_GROUP && !(foe_groups >> sf->op[ev->first + k].a & 1);
        if (!lets_in)
            continue;
        if (ev->cond == COND_TIME || (ev->cond == COND_KILLS && ev->value <= could))
            return 1;
        if (ev->cond == COND_RECORD_DOWN)
            for (int f = 0; f < foe_n; f++)
                if (foe_list[f]->rec == ev->value && (foe_groups >> foe_list[f]->wave & 1))
                    return 1;
    }
    return 0;
}

static int foes_finished(const StageFoes *sf)
{
    if (!foe_entered)
        return foe_n == 0;
    for (int f = 0; f < foe_n; f++)
        if ((foe_groups >> foe_list[f]->wave & 1) && (foe_left[f] > 0 || (foe_slot[f] >= 0 && ganados.e[foe_slot[f]].state != ENEMY_DEAD)))
            return 0;
    return !foes_more_coming(sf);
}

#define FOE_FAR       inttof32(110)
#define FOE_STEP      inttof32(3)
#define FOE_LEVEL     inttof32(16)
#define FOE_CELL      inttof32(4)
#define FOE_BODY      floattof32(2.2)
#define FOE_GRID      101
#define FOE_NEAR_MIN  12
#define FOE_NEAR_MAX  45
static u8 foe_dist[FOE_GRID * FOE_GRID];
static s32 foe_fy[FOE_GRID * FOE_GRID];
static u8 foe_shut[FOE_GRID * FOE_GRID];
static int foe_cx, foe_cz;
static int foe_tries, foe_moves, foe_cells, foe_y0;
static void foe_flood(const int leon_at[3])
{
    static u16 queue[FOE_GRID * FOE_GRID];
    static u16 held[256];
    int nheld = 0;
    memset(foe_dist, 0xff, sizeof(foe_dist));
    memset(foe_shut, 0, sizeof(foe_shut));
    foe_cx = leon_at[0];
    foe_cz = leon_at[2];
    int y0 = leon_at[1];
    ground_height(&stage_ground, leon_at[0], leon_at[2], leon_at[1] + inttof32(16), 0, &y0);
    int c0 = (FOE_GRID / 2) * FOE_GRID + FOE_GRID / 2, head = 0, tail = 0;
    foe_dist[c0] = 0;
    foe_fy[c0] = y0;
    queue[tail++] = c0;
    for (int pass = 0; pass < 2; pass++)
    {
        if (pass)
            for (int h = 0; h < nheld; h++)
            {
                int c = held[h] >> 2, k = held[h] & 3, cx = c % FOE_GRID, cz = c / FOE_GRID;
                int n = (cz + (k == 2) - (k == 3)) * FOE_GRID + cx + (k == 0) - (k == 1), y;
                if (foe_dist[n] != 0xff)
                    continue;
                int px = foe_cx + (n % FOE_GRID - FOE_GRID / 2) * FOE_CELL, pz = foe_cz + (n / FOE_GRID - FOE_GRID / 2) * FOE_CELL;
                ground_height(&stage_ground, px, pz, foe_fy[c] + FOE_STEP, FOE_STEP * 2, &y);
                foe_dist[n] = foe_dist[c] + 1;
                foe_fy[n] = y;
                foe_shut[n] = 1;
                queue[tail++] = n;
            }
    while (head < tail)
    {
        int c = queue[head++], cx = c % FOE_GRID, cz = c / FOE_GRID;
        if (foe_dist[c] >= FOE_NEAR_MAX)
            continue;
        int x = foe_cx + (cx - FOE_GRID / 2) * FOE_CELL, z = foe_cz + (cz - FOE_GRID / 2) * FOE_CELL;
        for (int k = 0; k < 4; k++)
        {
            int nx = cx + (k == 0) - (k == 1), nz = cz + (k == 2) - (k == 3);
            if (nx < 0 || nz < 0 || nx >= FOE_GRID || nz >= FOE_GRID)
                continue;
            int n = nz * FOE_GRID + nx;
            if (foe_dist[n] != 0xff)
                continue;
            int px = x + (nx - cx) * FOE_CELL, pz = z + (nz - cz) * FOE_CELL, y;
            int from = foe_fy[c];
            if (!ground_height(&stage_ground, px, pz, from + FOE_STEP, FOE_STEP * 2, &y) || y < from - FOE_STEP || y > from + FOE_STEP)
                continue;
            int qx = px, qz = pz, mx = (x + px) / 2, mz = (z + pz) / 2;
            if (ground_collide_n(&stage_ground, &qx, &qz, y + FOE_STEP, y + inttof32(16), FOE_BODY, 1)
                || ground_collide_n(&stage_ground, &mx, &mz, y + FOE_STEP, y + inttof32(16), FOE_BODY, 1))
                continue;
            if (!foe_shut[c] && !pass)
            {
                int ox = px, oz = pz, ox2 = mx, oz2 = mz;
                objects_collide(&objs, &ox, &oz, y, FOE_BODY);
                objects_collide(&objs, &ox2, &oz2, y, FOE_BODY);
                if (ox != px || oz != pz || ox2 != mx || oz2 != mz)
                {
                    if (nheld < (int)(sizeof(held) / sizeof(held[0])))
                        held[nheld++] = (u16)(c << 2 | k);
                    continue;
                }
            }
            foe_dist[n] = foe_dist[c] + 1;
            foe_fy[n] = y;
            foe_shut[n] = foe_shut[c];
            queue[tail++] = n;
        }
    }
    }
    foe_cells = tail;
    foe_y0 = f32toint(y0);
}

static int foe_reachable(int x, int y, int z)
{
    int cx = (inttof32(x) - foe_cx + FOE_CELL / 2) / FOE_CELL + FOE_GRID / 2 - (inttof32(x) < foe_cx ? 1 : 0);
    int cz = (inttof32(z) - foe_cz + FOE_CELL / 2) / FOE_CELL + FOE_GRID / 2 - (inttof32(z) < foe_cz ? 1 : 0);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++)
        {
            int nx = cx + dx, nz = cz + dz;
            if (nx < 0 || nz < 0 || nx >= FOE_GRID || nz >= FOE_GRID)
                continue;
            int n = nz * FOE_GRID + nx, d = f32toint(foe_fy[n]) - y;
            if (foe_dist[n] != 0xff && d < 10 && d > -10)
                return 1;
        }
    return 0;
}

static int foe_near(int seed, int out[3])
{
    foe_tries++;
    int best = -1, best_score = -1;
    for (int c = 0; c < FOE_GRID * FOE_GRID; c++)
    {
        if (foe_dist[c] == 0xff || foe_dist[c] < FOE_NEAR_MIN || foe_shut[c])
            continue;
        int x = foe_cx + (c % FOE_GRID - FOE_GRID / 2) * FOE_CELL, z = foe_cz + (c / FOE_GRID - FOE_GRID / 2) * FOE_CELL;
        int near = 200;
        for (int i = 0; i < ganados.count; i++)
        {
            const Enemy *e = &ganados.e[i];
            if (!e->active || e->state >= ENEMY_DYING)
                continue;
            int dx = f32toint(e->pos[0] - x), dz = f32toint(e->pos[2] - z);
            int d = (dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz);
            if (d < near)
                near = d;
        }
        int score = near * 64 + ((c * 31 + seed * 17) & 63);
        if (score > best_score)
        {
            best_score = score;
            best = c;
        }
    }
    if (best < 0)
        return 0;
    foe_moves++;
    out[0] = f32toint(foe_cx) + (best % FOE_GRID - FOE_GRID / 2) * f32toint(FOE_CELL);
    out[1] = f32toint(foe_fy[best]) + 2;
    out[2] = f32toint(foe_cz) + (best / FOE_GRID - FOE_GRID / 2) * f32toint(FOE_CELL);
    return 1;
}
static void foes_step(const int leon_at[3])
{
    for (int s = 0; s < ganados.count; s++)
    {
        int f = slot_foe[s];
        if (f >= 0 && ganados.e[s].state == ENEMY_DEAD && (!ganados.e[s].active || ganados.e[s].thrower))
        {
            slot_foe[s] = -1;
            foe_slot[f] = -1;
        }
    }
    int units = 0, flooded = 0;
    for (int s = 0; s < ganados.count; s++)
        units += slot_foe[s] >= 0;
    while (units < FOE_UNITS)
    {
        int f = 0;
        while (f < foe_n && !((foe_groups >> foe_list[f]->wave & 1) && foe_left[f] > 0 && foe_slot[f] < 0))
            f++;
        if (f >= foe_n)
            return;
        const StageFoe *r = foe_list[f];
        int slot = foe_fixed[f];
        if (slot < 0)
            for (int s = 0; s < foe_slots && slot < 0; s++)
                if (slot_foe[s] < 0 && !ganados.e[s].active)
                    slot = s;
        if (slot < 0)
            return;
        int brought = 0;
        if (foe_fixed[f] < 0)
        {
            int ldx = inttof32(r->pos[0]) - leon_at[0], ldz = inttof32(r->pos[2]) - leon_at[2];
            int ldy = inttof32(r->pos[1]) - leon_at[1];
            if ((s64)ldx * ldx + (s64)ldz * ldz < (s64)FOE_CLEAR * FOE_CLEAR && ldy < inttof32(20) && ldy > -inttof32(20)
                && ++foe_held < FOE_WAIT)
                return;
            foe_held = 0;
            int xyz[3] = { r->pos[0], r->pos[1], r->pos[2] };
            if (!flooded)
            {
                foe_flood(leon_at);
                flooded = 1;
            }
            int pfy = inttof32(r->pos[1]);
            ground_height(&stage_ground, inttof32(xyz[0]), inttof32(xyz[2]), inttof32(r->pos[1] + 6), FOE_STEP * 2, &pfy);
            int fdy = inttof32(r->pos[1] - 4) - leon_at[1];
            brought = ((s64)ldx * ldx + (s64)ldz * ldz > (s64)FOE_FAR * FOE_FAR || fdy > FOE_LEVEL || fdy < -FOE_LEVEL
                       || !foe_reachable(xyz[0], f32toint(pfy), xyz[2]))
                      && foe_near(f, xyz);
            for (int k = 0; k < 4 && !brought; k++)
            {
                int busy = 0;
                for (int s = 0; s < foe_slots; s++)
                    busy |= slot_foe[s] >= 0 && abs(ganados.e[s].pos[0] - inttof32(xyz[0])) < inttof32(4)
                            && abs(ganados.e[s].pos[2] - inttof32(xyz[2])) < inttof32(4);
                if (!busy)
                    break;
                xyz[0] = r->pos[0] + (k & 1 ? -6 : 6) * (k / 2 + 1);
            }
            int yaw = r->dir * (DEGREES_IN_CIRCLE / 256);
            if (brought)
                yaw = angle_to((leon_at[0] - inttof32(xyz[0])), (leon_at[2] - inttof32(xyz[2])));
            enemies_respawn(&ganados, slot, xyz, yaw);
            ganados.e[slot].drop = r->drop;
            enemies_set_kind(&ganados, slot, foe_kind_of(r));
#ifdef TEST_DROP_CODE
            ganados.e[slot].drop = TEST_DROP_CODE;
#endif
        }
        enemies_activate(&ganados, slot, NULL);
        if (r->ai == 6 || r->ai == 7 || brought)
            ganados.e[slot].spotted = 1;
        foe_left[f]--;
        foe_slot[f] = slot;
        slot_foe[slot] = f;
        foe_entered++;
        units++;
    }
}

static void stage1_kinds(void)
{
    const StageFoes *sf = &stage_foes[0];
    int n = 0;
    for (int i = 0; i < sf->count && n < ganados.count; i++)
        if (sf->foe[i].type == FOE_GANADO)
        {
            ganados.e[n].drop = sf->foe[i].drop;
            enemies_set_kind(&ganados, n++, foe_kind_of(&sf->foe[i]));
        }
}

static const StageFoe *stage_boss(int stage, int type, int n)
{
    const StageFoes *sf = &stage_foes[stage];
    for (int i = 0; (FOES_ON(stage) || type == FOE_GIANT) && i < sf->count; i++)
        if (sf->foe[i].type == type && n-- == 0)
            return &sf->foe[i];
    return NULL;
}
static int stage_bosses(int stage, int type)
{
    int n = 0;
    while (stage_boss(stage, type, n))
        n++;
    return n;
}
#define STAGE_START_X 19.5f
#define STAGE_START_Z 781.2f
#define STAGE_START_YAW 138
static int leon_pose[MODEL_MAX_NODES][12];
static NodeLocal pose_base[MODEL_MAX_NODES], pose_aim[MODEL_MAX_NODES], pose_tilt[MODEL_MAX_NODES];
static NodeLocal pose_prev[MODEL_MAX_NODES], pose_from[MODEL_MAX_NODES];
static NodeLocal pose_fx[MODEL_MAX_NODES], pose_fx0[MODEL_MAX_NODES];

enum { ANIM_IDLE, ANIM_WALK, ANIM_RUN, ANIM_OFF };
enum { CLIP_FLINCH = 3, CLIP_HELD, CLIP_BREAK_FREE, CLIP_DEATH, CLIP_CLIMB, CLIP_KICK, CLIP_STEP_OFF, CLIP_LAND, CLIP_HURT_IDLE };
#define LEON_HURT_BELOW 67
#define KICK_HIT      inttof32(18)
#define CLIMB_BACK    inttof32(16)
#define CLIMB_UP      inttof32(32)
#define DROP_MIN      inttof32(8)
#define DROP_GRAVITY  floattof32(0.06)
#define LAND_FRAMES   inttof32(24)
#define BLAST_RANGE   inttof32(60)
#define BLAST_DAMAGE  30
#define HANG_BLAST    inttof32(20)
#define MEDAL_REWARD  20

#define LEON_HP       100
#define SWIPE_DAMAGE  32
#define CULTIST_DAMAGE 67
#define IS_CULTIST(e) ((e)->kind >= foe_kind_cultist && (e)->kind < foe_kind_cultist + foe_kind_cultist_count)
#define HIT_DAMAGE(e) ((enemies_blow(e) * 100 + 600) / 1200)
#define KICK_DAMAGE   200
#define ENEMY_FELL    30000
#define BITE_DAMAGE   4
#define GRAB_DIST     floattof32(6.5)
#define GRAB_MASH     8
#define SAFE_FRAMES   0
#define MUSIC_VOLUME  80
#define KICK_FRAME    inttof32(16)
#define DEAD_TO_MENU  150
#define MENU_DELAY    60
enum { REACT_NONE, REACT_FLINCH, REACT_HELD, REACT_BREAK_FREE, REACT_DEAD,
       REACT_KICK, REACT_CLIMB_UP, REACT_CLIMB_DOWN, REACT_DROP, REACT_LAND, REACT_GIANT_UP, REACT_GIANT_CUT };

enum { MODE_TITLE, MODE_HOWTO, MODE_SELECT, MODE_PLAY, MODE_PAUSE, MODE_GAMEOVER, MODE_RESULT, MODE_STATUS, MODE_SHOP, MODE_CUTIN, MODE_MESSAGE };
#define HERBS_MAX     9

#ifndef SPARE_START
#define SPARE_START   20
#endif
#define HERB_HEAL     50
#define DROP_PERCENT  100
#define DROP_DELAY    40

#define BELL_FIRST    30
#define BELL_GAP      96
#define CLEAR_TEXT_AT 60
#define CLEAR_END     330
#define RESULT_STEP   24
static const u8 react_clip[] = { 0, CLIP_FLINCH, CLIP_HELD, CLIP_BREAK_FREE, CLIP_DEATH,
                                 CLIP_KICK, CLIP_CLIMB, CLIP_CLIMB, CLIP_STEP_OFF, CLIP_LAND, CLIP_CLIMB, CLIP_CLIMB };
#ifndef CUT_AUTO
#define CUT_AUTO      (60 * 8)
#endif
#define GAL_CUT_NEAR  inttof32(60)
enum { CUT_AFTER_NONE, CUT_AFTER_GIANT, CUT_AFTER_JUMP };

typedef struct { s8 stage; s16 x, z, y, yaw, to_x, to_z, to_y, to_yaw; } JumpSpot;
static const JumpSpot jumps[] = {
    { 4, 64, 18, 46, 0, 64, 155, 46, 0 },
    { 4, 64, 150, 46, 180, 64, 17, 46, 180 },
};
#define JUMP_NEAR inttof32(7)
static const JumpSpot *jump_now;
static const JumpSpot *jump_at_hand(int stage, const int pos[3], int feet, int yaw)
{
    for (unsigned i = 0; i < sizeof(jumps) / sizeof(jumps[0]); i++)
    {
        const JumpSpot *j = &jumps[i];
        int dx = pos[0] - inttof32(j->x), dz = pos[2] - inttof32(j->z), dy = feet - inttof32(j->y);
        int face = (int)(((s64)sinLerp(yaw) * sinLerp(degreesToAngle(j->yaw)) + (s64)cosLerp(yaw) * cosLerp(degreesToAngle(j->yaw))) >> 12);
        if (j->stage == stage && (s64)dx * dx + (s64)dz * dz < (s64)JUMP_NEAR * JUMP_NEAR && dy < inttof32(6) && dy > -inttof32(6)
            && face > floattof32(0.64))
            return j;
    }
    return NULL;
}
static const CutScene *cut;
static int cut_page, cut_t, cut_then, cut_after, cut_fade, cut_pic_id = -1;
static const char *const cut_who[] = { "", "Leon", "Ashley", "Luis", "Saddler" };

static int cut_start(int stage, int scene, int then, int after, int fade)
{
    const CutScene *c = cut_find(stage, scene);
    if (!c || !c->count)
        return 0;
    cut = c;
    cut_page = cut_t = 0;
    cut_then = then;
    cut_after = after;
    cut_fade = fade;
    return 1;
}

#define GIANT_BACK    inttof32(8)
#define GIANT_CUT_HIT inttof32(6)
enum { GUN_READY_POSE, GUN_WALK, GUN_RUN, GUN_DRAW, AIM_FORWARD, AIM_UP, AIM_DOWN,
       FIRE_UP, FIRE_DOWN, GUN_RELOAD };
#define DRAW_FRAMES leon_gun.frames[GUN_DRAW]
#define FIRE_FRAMES leon_gun.frames[FIRE_UP]
#define RELOAD_FRAMES leon_gun.frames[GUN_RELOAD]
#define MAG_SIZE    wep_mag(wep)
typedef struct {
    const char *name, *model, *anim;
    u8 item, mag, refire, tam, box;
    u16 power, ammo_price;
    int price, muzzle[3];
    u8 special;
} Weapon;
enum { WS_NONE, WS_SCOPE, WS_ROCKET };
#define MUZZLE(x, y, z) { floattof32(x), floattof32(y), floattof32(z) }
static const Weapon weapons[] = {
    { T_ITEM_GUN, NULL, NULL, 4, 10, 10, 6, 50, 10, 50, 8000, MUZZLE(-2.92, -0.24, 0.84) },
    { T_ITEM_MAGNUM, "nitro:/gun5.bin", "nitro:/gun5_anim.bin", 5, 6, 14, 8, 10, 130, 500, 38000, MUZZLE(-4.47, -0.26, 1.20) },
    { T_ITEM_SHOTGUN, "nitro:/gun2.bin", "nitro:/gun2_anim.bin", 6, 6, 30, 16, 15, 40, 120, 20000, MUZZLE(-8.30, -0.18, 0.60) },
    { T_ITEM_RIFLE, "nitro:/gun3.bin", "nitro:/gun3_anim.bin", 7, 5, 40, 14, 10, 50, 150, 20000, MUZZLE(-7.79, -0.21, 1.00), WS_SCOPE },
    { T_ITEM_TMP, "nitro:/gun4.bin", "nitro:/gun4_anim.bin", 8, 30, 2, 10, 100, 4, 20, 14000, MUZZLE(-2.96, -0.24, 0.80) },
    { T_ITEM_ROCKET, "nitro:/gun6.bin", "nitro:/gun6_anim.bin", 9, 1, 47, 16, 0, 0, 0, 30000, MUZZLE(-6.31, 0.06, 0.50), WS_ROCKET },
};
#define WEP_LINE 18
#define MULTI_BOX     50
#define MULTI_DROP    10
#define USES_ROUNDS(w) (weapons[w].box > 0)
enum { WEP_HANDGUN, WEP_MAGNUM, WEP_SHOTGUN, WEP_RIFLE, WEP_TMP, WEP_ROCKET, WEP_COUNT };
#define SCOPE_EYE     inttof32(15)
#define ROCKET_SPEED  inttof32(5)
#define ROCKET_BLAST  inttof32(30)
#define SCOPE_FOV     20
#define SCOPE_R       26
#define SCOPE_PITCH   degreesToAngle(50)
#define SHOTGUN_RANGE inttof32(40)
#define SHOTGUN_CONE  floattof32(0.95)
static int wep;
static int wep_ammo[WEP_COUNT];
static int mis_rounds, mis_herbs, mis_ptas, mis_kept;
#define WEP_STATS 4
static const u8 wep_stat_id[WEP_STATS] = { 0, 3, 5, 6 };
static const char *const wep_stat_name[WEP_STATS] = { T_STAT_POWER, T_STAT_CAPACITY, T_STAT_RATE, T_STAT_RELOAD };
enum { STAT_POWER, STAT_CAPACITY, STAT_RATE, STAT_RELOAD };
static int wep_owned = 1;
static u8 wep_level[WEP_COUNT][WEP_STATS];
static int case_level;
static int last_rank;
static const u8 rank_discount[5] = { 100, 100, 100, 90, 80 };
#define BUY_PRICE(p) ((p) * rank_discount[last_rank] / 100)
static const u8 case_tam[] = { 60, 77, 96, 120 };
static const int case_price[] = { 0, 24000, 40000, 73000 };
static int armor_level;
static const u8 armor_cut[] = { 0, 10, 20, 30 };
static const int armor_price[] = { 0, 30000, 60000, 90000 };
static int armored(int damage) { return damage * (100 - armor_cut[armor_level]) / 100; }

static const ShopTune *tune_at(int w, int stat, int level)
{
    for (int i = 0; i < shop_tune_count; i++)
        if (shop_tune[i].item == weapons[w].item && shop_tune[i].stat == wep_stat_id[stat] && shop_tune[i].level == level)
            return &shop_tune[i];
    return NULL;
}

static int wep_stat(int w, int stat, int base)
{
    const ShopTune *t = wep_level[w][stat] ? tune_at(w, stat, wep_level[w][stat]) : NULL;
    return t ? t->value : base;
}

static int wep_mag(int w) { return wep_stat(w, STAT_CAPACITY, weapons[w].mag * 10) / 10; }

static int wep_power(void) { return wep_stat(wep, STAT_POWER, weapons[wep].power) * 10; }
static int wep_crit_rate(void) { static const u8 r[WEP_COUNT] = { 8, 16, 0, 56, 0, 0 }; return r[wep]; }
static int wep_crit_kind(void) { return wep == WEP_HANDGUN ? CRIT_HANDGUN : wep == WEP_MAGNUM ? CRIT_MAGNUM : wep == WEP_RIFLE ? CRIT_RIFLE : CRIT_NONE; }
#ifdef TEST_SHOW_HP
static int test_shot_foe = -1, test_shots, test_staggers, test_downs;
#endif

static int wep_refire(int w)
{
    static const u8 handgun[3] = { 10, 8, 6 };
    int lv = wep_level[w][STAT_RATE];
    return inttof32(w == WEP_HANDGUN && lv < 3 ? handgun[lv] : weapons[w].refire);
}

static const u8 wep_reload_end[WEP_COUNT][5] = {
    { 18, 15, 11 },
    { 62, 49, 37, 31 },
    { 44, 35, 18, 10 },
    { 35, 28, 18 },
    { 32, 28, 23, 18, 14 },
    { 0 },
};
static int wep_reload_speed(int w)
{
    int lv = wep_level[w][STAT_RELOAD], end = lv < 5 ? wep_reload_end[w][lv] : 0;
    while (lv > 0 && !end)
        end = wep_reload_end[w][--lv];
    return end ? inttof32(RELOAD_FRAMES) / end : inttof32(1);
}
#define RECOIL      inttof32(1)
#define RELOAD_FADE floattof32(1.5)
#define SHOT_RANGE  inttof32(400)
#define THROWER_REACH inttof32(20)
#define DRAW_GRAB   floattof32(4.5)
#define XFADE_STEP  (inttof32(1) / 6)

enum { GUN_HOLSTERED, GUN_DRAWING, GUN_READY, GUN_HOLSTERING };

#define HAND_NODE   14
#define UPPER_BODY  10
#define AIM_RAISE   (inttof32(1) / 8)
#define AIM_TILT    (inttof32(1) / 40)
#define AIM_TILT_RANGE floattof32(0.8)
#define AIM_TURN    degreesToAngle(1)
#define ANIM_FPS 20
#define ANIM_STEP (inttof32(ANIM_FPS) / 60)

#define WALK_SPEED  floattof32(0.27)
#define LEON_MOVE(s) (WALK_SPEED * (s) / 300)
#define RUN_SPEED   LEON_MOVE(700)
#define BACK_SPEED  LEON_MOVE(400)
#define QTURN_FRAMES 26
#define QTURN_MOVE_FROM 9
#define QTURN_PAIR 6
#define TURN_SPEED  degreesToAngle(4)
#define STEP_UP     inttof32(3)
#define LEON_RADIUS floattof32(2.5)
#define LEON_HEIGHT inttof32(16)

#ifdef TEST_AUDIT
#define AU_CELL  inttof32(2)
#define AU_LEVELS 16
#define AU_LEVEL(y) ((((y) >> 12) + 48) / 12)
#define AU_Q 16384
#ifdef AU_STEP_U
#define AU_STEP inttof32(AU_STEP_U)
#else
#define AU_STEP STEP_UP
#endif
typedef struct { int x, z, feet; } AuState;
static void audit_stage(int stage, const int start[3])
{
    const Ground *g = &stage_ground;
    const StageFoes *sf = &stage_foes[stage];
    int nx = (g->cells_x * g->cell_size) / AU_CELL + 1, nz = (g->cells_z * g->cell_size) / AU_CELL + 1;
    u32 bits = (u32)nx * nz * AU_LEVELS;
    u8 *seen = calloc(bits / 8 + 1, 1);
    AuState *q = malloc(sizeof(AuState) * AU_Q);
    consoleDemoInit();
    if (!seen || !q)
    {
        printf("audit: out of memory (%dx%d)\n", nx, nz);
        return;
    }
    enum { T_MAX = 48 };
    int tk[T_MAX], ti[T_MAX], tx[T_MAX], ty[T_MAX], tz[T_MAX], tr[T_MAX], hit[T_MAX], tn = 0;
    for (int p = 0; p < sf->points && tn < T_MAX; p++)
    {
        int reach = 0;
        for (int e = 0; e < sf->events; e++)
            if (sf->event[e].ctg == EV_POINT && sf->event[e].trg == p && sf->event[e].reach > reach)
                reach = sf->event[e].reach;
        if (!reach)
            continue;
        tk[tn] = 'P'; ti[tn] = p; tx[tn] = sf->point[p][0]; ty[tn] = sf->point[p][1]; tz[tn] = sf->point[p][2]; tr[tn] = reach; tn++;
    }
    for (int i = 0; i < sf->things && tn < T_MAX; i++)
        if (sf->thing[i].kind == 4 || sf->thing[i].kind == 7)
        {
            tk[tn] = 'C'; ti[tn] = i; tx[tn] = sf->thing[i].pos[0]; ty[tn] = sf->thing[i].pos[1]; tz[tn] = sf->thing[i].pos[2]; tr[tn] = 0; tn++;
        }
    for (int i = 0; i < objs.count && tn + 1 < T_MAX; i++)
        if (objs.o[i].kind == OBJ_LADDER)
        {
            tk[tn] = 'U'; ti[tn] = i; tn++;
            tk[tn] = 'D'; ti[tn] = i; tn++;
        }
    if (sf->ally && tn < T_MAX)
    {
        tk[tn] = 'A'; ti[tn] = 0; tx[tn] = sf->ally_pos[0]; ty[tn] = sf->ally_pos[1]; tz[tn] = sf->ally_pos[2]; tr[tn] = 0; tn++;
    }
    for (int t = 0; t < tn; t++)
        hit[t] = 0;

    int head = 0, tail = 0, count = 0, lost = 0;
    #define AU_PUSH(X, Z, F) do { \
        int cx_ = ((X) - g->origin_x) / AU_CELL, cz_ = ((Z) - g->origin_z) / AU_CELL, lv_ = AU_LEVEL(F); \
        if (cx_ >= 0 && cz_ >= 0 && cx_ < nx && cz_ < nz && lv_ >= 0 && lv_ < AU_LEVELS) { \
            u32 b_ = ((u32)cz_ * nx + cx_) * AU_LEVELS + lv_; \
            if (!(seen[b_ >> 3] & (1 << (b_ & 7)))) { \
                seen[b_ >> 3] |= 1 << (b_ & 7); count++; \
                if ((tail + 1) % AU_Q == head) lost++; \
                else { q[tail].x = (X); q[tail].z = (Z); q[tail].feet = (F); tail = (tail + 1) % AU_Q; } } } } while (0)
    AU_PUSH(start[0], start[2], start[1]);
    static const s8 dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
    while (head != tail)
    {
        AuState s = q[head];
        head = (head + 1) % AU_Q;
        for (int t = 0; t < tn; t++)
        {
            if (hit[t])
                continue;
            if (tk[t] == 'P')
            {
                int at[3] = { s.x, s.feet, s.z };
                hit[t] = ev_near(tx[t], ty[t], tz[t], tr[t], at);
            }
            else if (tk[t] == 'C' || tk[t] == 'A')
            {
                int dx = s.x - inttof32(tx[t]), dz = s.z - inttof32(tz[t]), dy = s.feet - inttof32(ty[t]);
                int r = tk[t] == 'C' ? inttof32(9) : inttof32(30);
                hit[t] = (s64)dx * dx + (s64)dz * dz < (s64)r * r && dy > -inttof32(12) && dy < inttof32(8);
            }
            else
            {
                const Obj *o = &objs.o[ti[t]];
                int dx = s.x - o->pos[0], dz = s.z - o->pos[2];
                int along = mulf32(dx, o->dir[0]) + mulf32(dz, o->dir[1]);
                int across = abs(mulf32(dx, o->dir[1]) - mulf32(dz, o->dir[0]));
                if (tk[t] == 'U')
                    hit[t] = across < inttof32(6) && along > -inttof32(8) && along < o->end[0] + inttof32(2) && s.feet < o->pos[1] + inttof32(4);
                else
                    hit[t] = across < inttof32(6) && along > o->end[0] - inttof32(3) && along < o->end[0] + inttof32(14) && s.feet > o->top - inttof32(4);
            }
        }
        for (int i = 0; i < objs.count; i++)
        {
            const Obj *o = &objs.o[i];
            if (o->kind != OBJ_LADDER)
                continue;
            int dx = s.x - o->pos[0], dz = s.z - o->pos[2];
            int along = mulf32(dx, o->dir[0]) + mulf32(dz, o->dir[1]);
            int across = abs(mulf32(dx, o->dir[1]) - mulf32(dz, o->dir[0]));
            if (across < inttof32(6) && along > -inttof32(8) && along < o->end[0] + inttof32(2) && s.feet < o->pos[1] + inttof32(4))
            {
                int x = o->pos[0] + mulf32(o->dir[0], CLIMB_BACK), z = o->pos[2] + mulf32(o->dir[1], CLIMB_BACK), f = o->top;
                ground_height(g, x, z, f, STEP_UP, &f);
                AU_PUSH(x, z, f);
            }
            if (across < inttof32(6) && along > o->end[0] - inttof32(3) && along < o->end[0] + inttof32(14) && s.feet > o->top - inttof32(4))
                AU_PUSH(o->pos[0], o->pos[2], o->pos[1]);
        }
        for (unsigned j = 0; j < sizeof(jumps) / sizeof(jumps[0]); j++)
        {
            const JumpSpot *js = &jumps[j];
            int dx = s.x - inttof32(js->x), dz = s.z - inttof32(js->z), dy = s.feet - inttof32(js->y);
            if (js->stage == stage && (s64)dx * dx + (s64)dz * dz < (s64)JUMP_NEAR * JUMP_NEAR && dy < inttof32(6) && dy > -inttof32(6))
            {
                int f = inttof32(js->to_y);
                ground_height(g, inttof32(js->to_x), inttof32(js->to_z), f + STEP_UP, STEP_UP * 2, &f);
                AU_PUSH(inttof32(js->to_x), inttof32(js->to_z), f);
            }
        }
        for (int d = 0; d < 8; d++)
        {
            int x = s.x + dirs[d][0] * AU_CELL, z = s.z + dirs[d][1] * AU_CELL;
            ground_collide(g, &x, &z, s.feet + AU_STEP, s.feet + LEON_HEIGHT, LEON_RADIUS);
            int floor = s.feet;
            if (!ground_height(g, x, z, s.feet, AU_STEP, &floor))
                continue;
#ifdef TEST_AUDIT_SHUT
            {
                int ox = x, oz = z;
                objects_collide(&objs, &ox, &oz, s.feet, LEON_RADIUS);
                if (ox != x || oz != z)
                    continue;
            }
#endif
            if (floor < s.feet - DROP_MIN && stage == ASH_STAGE_E)
                continue;
            AU_PUSH(x, z, floor);
        }
    }
    #undef AU_PUSH
    printf("S%d cells %d%s\n", stage + 1, count, lost ? " QUEUE FULL" : "");
    int col = 0;
    for (int t = 0; t < tn; t++)
    {
        char buf[16];
        if (tk[t] == 'U' || tk[t] == 'D')
            snprintf(buf, sizeof(buf), "L%d%c %s", ti[t], tk[t], hit[t] ? "ok" : "NO");
        else
            snprintf(buf, sizeof(buf), "%c%d %s", tk[t], ti[t], hit[t] ? "ok" : "NO");
        printf("%-10s", buf);
        if (++col == 3)
        {
            printf("\n");
            col = 0;
        }
    }
    printf("\n");
    free(seen);
    free(q);
}
#endif

static int cam_back = floattof32(14.5), cam_right = floattof32(1.5), cam_height = floattof32(16.5),
           cam_at_height = floattof32(13), cam_at_right = floattof32(1.5);
#define CAM_FOV       45
#define ST_FOV        70
#define CAM_BACK      cam_back
#define CAM_FADE      inttof32(5)
#define CAM_PIV_DIS   (inttof32(5000) / 256)
#define CAM_AIM_DIS   (inttof32(3000) / 256)
#define CAM_CLIMB_OUT    inttof32(20)
#define CAM_CLIMB_SIDE   inttof32(6)
#define CAM_CLIMB_HEIGHT inttof32(12)
#define CAM_CLIMB_AT     inttof32(9)
#define CAM_WALL_GAP  floattof32(1.5)
#define CAM_RIGHT     cam_right
#define CAM_AT_RIGHT  cam_at_right
#define CAM_HEIGHT    cam_height
#define CAM_AHEAD     floattof32(40)
#define CAM_AT_HEIGHT cam_at_height
#define CAM_AIM_BACK    floattof32(9)
#define CAM_AIM_RIGHT   floattof32(3.5)
#define CAM_AIM_HEIGHT  floattof32(15.5)
#define CAM_AIM_TILT    floattof32(25)
#define KNIFE_PIV_X    (-inttof32(100) / 256)
#define KNIFE_PIV_Y    (inttof32(480) / 256)
#define KNIFE_PIV_Z    (-inttof32(870) / 256)
#define KNIFE_CAM_BACK floattof32(11.546)
#define KNIFE_CAM_SIDE floattof32(1.672)
#define KNIFE_CAM_UP   floattof32(1.122)
#define KNIFE_CAM_TILT floattof32(6)
static int knife_hand[3] = { -inttof32(2), inttof32(9), inttof32(3) };
#ifdef TEST_CAM_JUMPS
static int cam_jumps, cam_near_min = 99, cam_xfades;
#endif

enum { CTRL_LEON, CTRL_FREECAM };

#ifndef START_CTRL
#define START_CTRL CTRL_LEON
#endif
#ifndef LEON_START_MODE
#define LEON_START_MODE ANIM_IDLE
#endif
#ifndef LEON_CIRCLE_R
#define LEON_CIRCLE_R 20
#endif

static void on_vblank(void) { vblank_count++; }

static const u32 cull_modes[] = { POLY_CULL_BACK, POLY_CULL_FRONT, POLY_CULL_NONE };

#ifdef AUTOPILOT
static int ap_aim_err;
static u32 autopilot_keys(u32 frame)
{
#if AUTOPILOT == 2
    return (frame % 240) < 20 ? KEY_UP | KEY_B | KEY_LEFT : KEY_UP | KEY_B;
#elif AUTOPILOT == 4
    if (frame < 540) return 0;
    u32 a = (frame - 540) % 600;
    if (a < 180) return KEY_R;
    if (a < 240) return KEY_R | KEY_UP;
    if (a < 360) return KEY_R | KEY_DOWN;
    if (a < 420) return KEY_R | KEY_UP;
    return 0;
#elif AUTOPILOT == 5
    if (frame < 540) return 0;
    u32 a = frame - 540;
    if (a < 4) return KEY_X;
    if (a < 160) return 0;
    if (a < 280) return KEY_UP;
    if (a < 400) return KEY_UP | KEY_B;
    if (a < 460) return 0;
    if (a < 464) return KEY_X;
    return 0;
#elif AUTOPILOT == 7
    if (frame < 540) return 0;
    u32 a = frame - 540, keys = KEY_R;
    if (a % 24 < 2 && a > 20) keys |= KEY_A;
    return keys;
#elif AUTOPILOT == 8
    if (frame < 540) return 0;
    u32 a = frame - 540, keys = KEY_R;
    if (a % 30 < 2 && a > 20) keys |= KEY_A;
    if ((a / 90) % 4 == 1 && a % 30 > 10) keys |= KEY_LEFT;
    if ((a / 90) % 4 == 3 && a % 30 > 10) keys |= KEY_RIGHT;
    return keys;
#elif AUTOPILOT == 9
    if (frame < 540) return 0;
    u32 a = frame - 540;
    if (a < 240) return KEY_UP;
    return a % 12 < 2 ? KEY_A : 0;
#elif AUTOPILOT == 10
    static const struct { u16 at; u16 key; } script[] = {
        { 540, KEY_DOWN }, { 560, KEY_A }, { 840, KEY_B }, { 870, KEY_DOWN }, { 900, KEY_A },
        { 960, KEY_RIGHT }, { 1000, KEY_A }, { 1040, KEY_LEFT }, { 1100, KEY_A },
        { 1400, KEY_START }, { 1460, KEY_DOWN }, { 1600, KEY_A } };
    for (unsigned i = 0; i < sizeof(script) / sizeof(script[0]); i++)
        if (frame >= script[i].at && frame < script[i].at + 2u)
            return script[i].key;
    return frame > 1200 && frame < 1400 ? KEY_UP : 0;
#elif AUTOPILOT == 12
    (void)frame;
    return 0;
#elif AUTOPILOT == 13
    if (frame >= 540 && frame < 720) return KEY_UP;
    if (frame >= 740 && frame < 742) return KEY_A;
    if (frame >= 840 && frame < 1100) return KEY_UP;
    return 0;
#elif AUTOPILOT == 14
    if (frame >= 540 && frame < 560) return KEY_UP;
    if (frame >= 570 && frame < 572) return KEY_A;
    if (frame >= 780 && frame < 800) return KEY_A;
    if (frame >= 1000 && frame < 1002) return KEY_A;
    if (frame >= 1200 && frame < 1260) return KEY_LEFT;
    if (frame >= 1260 && frame < 1500) return KEY_UP;
    return 0;
#elif AUTOPILOT == 60
    if (frame >= 540 && frame < 650) return KEY_UP;
    if (frame >= 660 && frame < 662) return KEY_A;
    if (frame >= 900 && frame < 918) return KEY_UP;
    if (frame >= 930 && frame < 932) return KEY_A;
    return 0;
#elif AUTOPILOT == 15
    return frame >= 540 && frame < 700 ? KEY_UP : 0;
#elif AUTOPILOT == 16
    if (frame >= 540 && frame < 700) return KEY_R | (frame >= 560 && frame < 563 ? KEY_DOWN : 0) | (frame >= 570 && frame < 577 ? KEY_RIGHT : 0) | (frame >= 620 && frame < 622 ? KEY_A : 0);
    if (frame >= 760 && frame < 960) return KEY_R | (frame >= 770 && frame < 773 ? KEY_DOWN : 0) | (frame >= 800 && frame < 830 ? KEY_LEFT : 0) | (frame >= 900 && frame < 902 ? KEY_A : 0);
    return 0;
#elif AUTOPILOT == 18
    if (frame >= 540 && frame < 760) return KEY_L | (frame >= 600 && frame < 602 ? KEY_A : 0);
    return 0;
#elif AUTOPILOT == 45
    static int kneels, last = -1;
    if (frame < 300) return 0;
    if (giants[0].state == GIANT_KNEEL && last != GIANT_KNEEL)
        kneels++;
    last = giants[0].state;
    if (giants[0].state == GIANT_KNEEL)
        return kneels == 1 ? 0 : KEY_UP | KEY_B | (frame % 12 < 2 ? KEY_A : 0);
    if (giants[0].state == GIANT_RISE || giants[0].state == GIANT_DEAD)
        return 0;
    return KEY_R | ((frame % 20) < 2 ? KEY_A : 0);
#elif AUTOPILOT == 46
    if (frame < 400) return 0;
    if (frame < 403) return KEY_X;
    for (int k = 0; k < TEST_WEAPON; k++)
        if (frame >= 480u + k * 40 && frame < 483u + k * 40) return KEY_L | KEY_X;
    if (frame < 800) return 0;
    if (frame >= 1040 && frame < 1042) return KEY_R | KEY_A;
    if (frame >= 1100 && frame < 1160) return KEY_R | KEY_UP;
    return frame < 1400 ? KEY_R : 0;
#elif AUTOPILOT == 47
    if (frame < 400) return 0;
    if (frame < 403) return KEY_X;
    for (int k = 0; k < 3; k++)
        if (frame >= 480u + k * 40 && frame < 483u + k * 40) return KEY_L | KEY_X;
    if (frame < 700) return 0;
    if (giants[0].held)
        return frame % 120 < 2 ? KEY_A : 0;
    if (giants[0].state == GIANT_KNEEL)
        return KEY_UP | KEY_B | (frame % 12 < 2 ? KEY_A : 0);
    if (giants[0].state == GIANT_RISE || giants[0].state == GIANT_DEAD)
        return 0;
    u32 turn = ap_aim_err > degreesToAngle(2) ? KEY_LEFT : ap_aim_err < -degreesToAngle(2) ? KEY_RIGHT : 0;
    return KEY_R | turn | (!turn && (frame % 30) < 2 ? KEY_A : 0);
#elif AUTOPILOT == 19
    if (frame < 300) return 0;
    return KEY_R | ((frame % 20) < 2 ? KEY_A : 0);
#elif AUTOPILOT == 17
    if (frame < 900) return 0;
    u32 a = (frame - 900) % 120;
    if (a < 40) return KEY_R | (a < 24 ? KEY_LEFT : 0) | (a >= 30 && a < 32 ? KEY_A : 0);
    return a >= 50 && a < 52 ? KEY_A : 0;
#elif AUTOPILOT == 11
    if (frame >= 840 && frame < 1080) return KEY_UP;
    if (frame >= 1700 && frame % 60 < 2) return KEY_A;
    return 0;
#elif AUTOPILOT == 21
    static const struct { u16 at; u16 key; } script[] = {
        { 400, KEY_START }, { 520, KEY_DOWN }, { 640, KEY_A }, { 760, KEY_UP }, { 880, KEY_B },
        { 1000, KEY_DOWN }, { 1060, KEY_A }, { 1180, KEY_UP }, { 1300, KEY_A }, { 1420, KEY_DOWN },
        { 1480, KEY_A }, { 1600, KEY_B }, { 1720, KEY_START } };
    for (unsigned i = 0; i < sizeof(script) / sizeof(script[0]); i++)
        if (frame >= script[i].at && frame < script[i].at + 2u)
            return script[i].key;
    return 0;
#elif AUTOPILOT == 23
    if (frame < 300) return 0;
    return KEY_R | ((frame % 30) < 5 && frame > 330 ? KEY_UP : 0) | ((frame % 30) >= 15 && (frame % 30) < 17 ? KEY_A : 0);
#elif AUTOPILOT == 22
    static const struct { u16 at; u16 key; } script[] = {
        { 400, KEY_SELECT }, { 520, KEY_A }, { 640, KEY_B }, { 700, KEY_DOWN }, { 760, KEY_A },
        { 880, KEY_B }, { 940, KEY_DOWN }, { 1000, KEY_A }, { 1120, KEY_A }, { 1240, KEY_DOWN },
        { 1300, KEY_DOWN }, { 1360, KEY_DOWN }, { 1480, KEY_SELECT } };
    for (unsigned i = 0; i < sizeof(script) / sizeof(script[0]); i++)
        if (frame >= script[i].at && frame < script[i].at + 2u)
            return script[i].key;
    return 0;
#elif AUTOPILOT == 31
    if (frame < 400) return 0;
    if (frame < 403) return KEY_X;
    if (frame < 480) return 0;
    u32 a = (frame - 480) % 240;
    if (a < 3) return KEY_L | KEY_X;
    if (a < 60) return 0;
    if (a == 130 || a == 131) return KEY_R | KEY_A;
    return a < 200 ? KEY_R : 0;
#elif AUTOPILOT == 32
    if (frame < 400) return 0;
    if (frame < 403) return KEY_X;
    for (int k = 0; k < TEST_WEAPON; k++)
        if (frame >= 480u + k * 40 && frame < 483u + k * 40) return KEY_L | KEY_X;
    if (frame < 800) return 0;
    return (frame - 1000) % 240 < 2 ? KEY_R | KEY_A : KEY_R;
#elif AUTOPILOT == 33
    static const struct { u16 at; u16 key; } script33[] = {
        { 400, KEY_SELECT }, { 480, KEY_DOWN }, { 540, KEY_DOWN }, { 620, KEY_A }, { 700, KEY_A }, { 860, KEY_SELECT }, { 960, KEY_X } };
    for (unsigned i = 0; i < sizeof(script33) / sizeof(script33[0]); i++)
        if (frame >= script33[i].at && frame < script33[i].at + 2u)
            return script33[i].key;
    return frame > 1060 ? KEY_R : 0;
#elif AUTOPILOT == 34
    static const struct { u16 at; u16 key; } script34[] = {
        { 300, KEY_A }, { 380, KEY_DOWN }, { 460, KEY_A }, { 560, KEY_DOWN }, { 620, KEY_DOWN }, { 680, KEY_DOWN },
        { 740, KEY_DOWN }, { 800, KEY_DOWN }, { 880, KEY_A }, { 980, KEY_B }, { 1060, KEY_RIGHT }, { 1120, KEY_RIGHT },
        { 1200, KEY_A }, { 1280, KEY_A }, { 1360, KEY_A }, { 1460, KEY_B }, { 1520, KEY_B }, { 1600, KEY_LEFT },
        { 1680, KEY_A }, { 1760, KEY_A }, { 1860, KEY_B }, { 1940, KEY_LEFT }, { 2020, KEY_LEFT }, { 2100, KEY_A } };
    for (unsigned i = 0; i < sizeof(script34) / sizeof(script34[0]); i++)
        if (frame >= script34[i].at && frame < script34[i].at + 2u)
            return script34[i].key;
    return 0;
#elif AUTOPILOT == 63
    if (frame >= 470 && frame < 530) return KEY_UP;
    if (frame >= 620 && frame < 622) return KEY_A;
    return 0;
#elif AUTOPILOT == 69
    if (frame >= 180 && frame < 300) return KEY_DOWN | KEY_LEFT;
    if (frame >= 300 && frame < 420) return KEY_DOWN | KEY_RIGHT;
    if (frame >= 480 && frame < 482) return KEY_SELECT;
    return 0;
#elif AUTOPILOT == 68
    if ((frame >= 60 && frame < 64) || (frame >= 90 && frame < 94)) return KEY_L | KEY_X;
    if (frame >= 120 && frame < 300) return KEY_R | (frame >= 200 && frame < 202 ? KEY_A : 0);
    return 0;
#elif AUTOPILOT == 67
    if (frame >= 180 && frame < 480) return KEY_R | (frame >= 300 && frame < 420 && frame % 6 == 0 ? KEY_A : 0);
    return 0;
#elif AUTOPILOT == 72
    if (frame >= 180 && frame < 900) return KEY_UP;
    return 0;
#elif AUTOPILOT == 71
    if (frame >= 180 && frame < 420) return KEY_DOWN | (frame >= 240 && frame < 300 ? KEY_B : 0);
    if (frame >= 540 && frame < 560) return KEY_DOWN | KEY_B;
    return 0;
#elif AUTOPILOT == 66
    if (frame >= 300 && frame < 304) return KEY_DOWN | KEY_B;
    if (frame >= 480 && frame < 600) return KEY_DOWN;
    return 0;
#elif AUTOPILOT == 65
    if (frame >= 720 && frame < 722) return KEY_A;
#ifdef TEST_A_EVERY_S
    if (frame >= 720 && frame % 60 == 0) return KEY_A;
#endif
#ifdef TEST_CRATE_PICK
    if (frame >= 960 && frame < 962) return KEY_A;
#endif
    return 0;
#elif AUTOPILOT == 64
    if (frame >= 180 && frame < 480) return KEY_UP;
    return 0;
#elif AUTOPILOT == 61
    if (frame >= 300 && frame < 540) return KEY_UP;
    if (frame >= 540 && frame < 780) return KEY_UP | KEY_B;
    return 0;
#elif AUTOPILOT == 62
    if (frame == 300 || frame == 1000) return KEY_A;
    if (frame >= 380 && frame < 380 + 9 * 40 && frame % 40 == 20) return KEY_DOWN;
    return 0;
#elif AUTOPILOT == 35
    static const struct { u16 at; u16 key; } script35[] = {
        { 300, KEY_A }, { 420, KEY_A }, { 700, KEY_A }, { 800, KEY_DOWN }, { 900, KEY_B }, { 1000, KEY_LEFT }, { 1080, KEY_A } };
    for (unsigned i = 0; i < sizeof(script35) / sizeof(script35[0]); i++)
        if (frame >= script35[i].at && frame < script35[i].at + 2u)
            return script35[i].key;
    return 0;
#elif AUTOPILOT == 49
    if (frame < 300) return 0;
    u32 k = KEY_R;
    if (frame > 340 && frame < 400 && frame % 18 == 0) k |= KEY_A;
    if (frame == 900) k |= KEY_Y;
    if (frame >= 904 && frame < 1080) k |= KEY_UP;
    return k;
#elif AUTOPILOT == 70
    if (frame < 240) return 0;
    if (frame < 243) return KEY_X;
    if (frame < 300) return 0;
    if (frame < 420) return KEY_R | (frame > 320 && frame % 18 == 0 ? KEY_A : 0);
    if (frame == 430) return KEY_Y;
    if (frame == 600) return KEY_START;
    if (frame == 640) return KEY_A;
    if (frame == 680) return KEY_UP;
    if (frame == 720) return KEY_A;
    return 0;
#elif AUTOPILOT == 50
    if (frame < 600) return 0;
    u32 k = KEY_L;
    if (frame < 660) k |= KEY_UP;
    if (frame == 720 || frame == 800) k |= KEY_A;
    if (frame >= 880 && frame < 1000) k |= KEY_DOWN;
    if (frame == 1060 || frame == 1140) k |= KEY_A;
    return k;
#elif AUTOPILOT == 51
    static const struct { u16 at; u16 key; } script51[] = {
        { 600, KEY_A }, { 900, KEY_RIGHT }, { 960, KEY_RIGHT }, { 1020, KEY_RIGHT }, { 1080, KEY_RIGHT }, { 1140, KEY_RIGHT },
        { 1260, KEY_A }, { 1500, KEY_LEFT }, { 1600, KEY_A }, { 2000, KEY_START } };
    for (unsigned i = 0; i < sizeof(script51) / sizeof(script51[0]); i++)
        if (frame >= script51[i].at && frame < script51[i].at + 2u)
            return script51[i].key;
    return frame > 2300 && (frame / 90) % 3 == 0 ? KEY_LEFT : 0;
#elif AUTOPILOT == 48
    static const struct { u16 at; u16 key; } script48[] = {
        { 600, KEY_A }, { 900, KEY_RIGHT }, { 1020, KEY_RIGHT }, { 1140, KEY_RIGHT }, { 1260, KEY_RIGHT }, { 1380, KEY_RIGHT }, { 1500, KEY_LEFT } };
    for (unsigned i = 0; i < sizeof(script48) / sizeof(script48[0]); i++)
        if (frame >= script48[i].at && frame < script48[i].at + 2u)
            return script48[i].key;
    return 0;
#elif AUTOPILOT == 36
    if (frame == 300 || frame == 301) return KEY_A;
    for (unsigned i = 0; i < 6; i++)
        if (frame >= 360 + i * 30 && frame < 362 + i * 30) return KEY_DOWN;
    return frame > 600 && frame % 16 < 2 ? KEY_A : 0;
#elif AUTOPILOT == 37
    static const struct { u16 at; u16 key; } script37[] = { { 300, KEY_A }, { 400, KEY_DOWN }, { 480, KEY_A } };
    for (unsigned i = 0; i < 3; i++)
        if (frame >= script37[i].at && frame < script37[i].at + 2u)
            return script37[i].key;
    return 0;
#elif AUTOPILOT == 77
    static const struct { u16 at; u16 key; } s77[] = {
        { 720, KEY_START }, { 750, KEY_DOWN }, { 770, KEY_DOWN }, { 790, KEY_DOWN }, { 820, KEY_A }, { 850, KEY_DOWN }, { 880, KEY_A },
        { 910, KEY_DOWN }, { 940, KEY_A }, { 970, KEY_DOWN }, { 1000, KEY_A }, { 1040, KEY_START } };
    for (unsigned i = 0; i < sizeof(s77) / sizeof(s77[0]); i++)
        if (frame >= s77[i].at && frame < s77[i].at + 2u)
            return s77[i].key;
    return 0;
#elif AUTOPILOT == 79
    if (frame < 600) return 0;
#ifdef AUTOPILOT_STILL
    return KEY_L;
#endif
    if (frame >= 1500) return KEY_L;
    return KEY_L | ((frame / 30) % 4 == 0 ? KEY_UP : (frame / 30) % 4 == 2 ? KEY_DOWN : 0);
#elif AUTOPILOT == 78
    if (frame < 120 || frame >= 1400) return 0;
#ifdef AUTOPILOT_STRAIGHT
    return frame >= 300 ? KEY_UP : 0;
#endif
    return KEY_UP | ((frame / 90) % 3 == 1 ? KEY_LEFT : 0);
#elif AUTOPILOT == 76
    if (frame < 600) return 0;
#ifdef AUTOPILOT_KNIFE_UP
    if (frame >= 620 && frame < 620 + AUTOPILOT_KNIFE_UP) return KEY_L | KEY_UP;
#endif
    return KEY_L | (frame == 720 || frame == 900 ? KEY_A : 0);
#elif AUTOPILOT == 75
#ifndef AUTOPILOT_TILT
#define AUTOPILOT_TILT 0
#endif
    if (frame < 840)
        return frame % 60 < 2 ? KEY_A : 0;
    {
        u32 k = KEY_R;
        if (frame >= 960 && frame < 960 + AUTOPILOT_TILT)
            k |= KEY_UP;
        if (frame >= 900 && frame % 30 < 2)
            k |= KEY_A;
        return k;
    }
#elif AUTOPILOT == 74
    if (frame >= 240 && frame < 243) return KEY_X;
    if (frame >= 300 && frame < 480) return KEY_R | (frame % 20 == 0 ? KEY_A : 0);
    if (frame >= 520 && frame < 523) return KEY_X;
    if (frame >= 700 && frame % 60 < 2) return KEY_A;
    return 0;
#elif AUTOPILOT == 73
    if (frame > 900 && frame < 2300) return frame % 120 < 2 ? KEY_A : 0;
    if (frame >= 2400 && frame < 2402) return KEY_B;
    if (frame >= 2460 && frame < 2462) return KEY_LEFT;
    if (frame >= 2520 && frame < 2522) return KEY_A;
    if (frame > 2700 && frame < 3300) return frame % 90 < 2 ? KEY_START : 0;
    if (frame >= 3900 && frame < 4500) return KEY_UP;
    return 0;
#elif AUTOPILOT == 38
    return frame > 900 && frame % 120 < 2 ? KEY_A : 0;
#elif AUTOPILOT == 39
    static const struct { u16 at; u16 key; } script39[] = { { 400, KEY_START }, { 480, KEY_DOWN }, { 560, KEY_A }, { 640, KEY_UP }, { 720, KEY_A } };
    for (unsigned i = 0; i < 5; i++)
        if (frame >= script39[i].at && frame < script39[i].at + 2u)
            return script39[i].key;
    return 0;
#elif AUTOPILOT == 40
    static const struct { u16 at; u16 key; } script40[] = {
        { 300, KEY_A }, { 380, KEY_DOWN }, { 460, KEY_A }, { 560, KEY_B }, { 640, KEY_B } };
    for (unsigned i = 0; i < 5; i++)
        if (frame >= script40[i].at && frame < script40[i].at + 2u)
            return script40[i].key;
    return 0;
#elif AUTOPILOT == 41
    static const struct { u16 at; u16 key; } script41[] = { { 300, KEY_A }, { 420, KEY_A }, { 700, KEY_A } };
    for (unsigned i = 0; i < 3; i++)
        if (frame >= script41[i].at && frame < script41[i].at + 2u)
            return script41[i].key;
    return 0;
#elif AUTOPILOT == 42
    if (frame < 300) return 0;
    return (frame / 240) % 2 ? KEY_RIGHT : KEY_UP | KEY_B | KEY_LEFT;
#elif AUTOPILOT == 43
    if (frame == 540 || frame == 541 || frame == 1500 || frame == 1501) return KEY_A;
    return 0;
#elif AUTOPILOT == 44
    if (frame < 300) return 0;
    u32 a = (frame - 300) % 600;
    if (a < 180) return KEY_UP | KEY_B;
    if (a < 420) return KEY_LEFT;
    return KEY_UP | KEY_B | (a & 64 ? KEY_RIGHT : 0);
#elif AUTOPILOT == 30
    if (frame < 540) return 0;
    u32 a = (frame - 540) % 420;
    if (a < 3) return KEY_A;
    return a >= 60 && a < 360 ? KEY_UP : 0;
#elif AUTOPILOT == 20
    if (frame < 540) return 0;
    if (frame < 660) return KEY_UP | KEY_B;
    return frame < 900 ? KEY_LEFT : 0;
#elif AUTOPILOT == 3
    return frame < 540 ? 0 : KEY_UP | KEY_B;
#endif
    u32 t = frame % 600;
    if (t < 120) return KEY_UP;
    if (t < 180) return KEY_UP | KEY_LEFT;
    if (t < 330) return KEY_UP | KEY_B;
    if (t < 390) return KEY_RIGHT;
    if (t < 480) return KEY_DOWN;
    return 0;
}
#endif

#define HAZE_R 12
#define HAZE_G 12
#define HAZE_B 9
#define NEAR_PLANE 1.0f
#ifndef FAR_PLANE
#define FAR_PLANE  700.0f
#endif
#define POLY_HIGH      1940
#define POLY_LOW       1800
#define POLY_STAGE_MIN 1200
static int stage_budget = -1;
#define ENEMY_SHOW_MIN 2
static int enemy_show = ENEMY_MAX;
static int stage_used;
#define TIME_CAP_MIN   1200
static int time_cap = 4000, enemy_cap = ENEMY_MAX;
#define FOG_START  15.0f
#define FOG_SCALE  130.0f

static float depth_of(float d)
{
    return 32767.0f * FAR_PLANE / (FAR_PLANE - NEAR_PLANE) * (1.0f - NEAR_PLANE / d);
}

static float distance_of(float depth)
{
    float q = depth / 32767.0f * (FAR_PLANE - NEAR_PLANE) / FAR_PLANE;
    return q >= 1.0f ? 1e9f : NEAR_PLANE / (1.0f - q);
}

static void setup_fog(void)
{
    const int shift = 3;
    int offset = (int)depth_of(FOG_START);
    glEnable(GL_FOG);
    glFogShift(shift);
    glFogOffset(offset);
    glFogColor(HAZE_R, HAZE_G, HAZE_B, 31);
    for (int n = 0; n < 32; n++)
    {
        float d = distance_of(offset + (n + 1) * (0x400 >> shift));
        float density = d <= FOG_START ? 0.0f : 1.0f - expf(-(d - FOG_START) / FOG_SCALE);
        int v = (int)(density * 127.0f + 0.5f);
        glFogDensity(n, v > 127 ? 127 : v);
    }
}

static void stage_fog(const s16 fog[2])
{
#ifdef TEST_OLD_FOG
    (void)fog;
    setup_fog();
    return;
#endif
    float from = fog[0] < 1 ? 1.0f : (float)fog[0], to = fog[1] > FAR_PLANE ? FAR_PLANE : (float)fog[1];
    if (to <= from + 1.0f)
        to = from + 1.0f;
    int offset = (int)depth_of(from);
    int span = (int)depth_of(to) - offset, shift = 10;
    while (shift > 0 && (0x400 >> shift) * 32 < span)
        shift--;
    if (offset + 32 * (0x400 >> shift) > 0x7fff)
        offset = 0x7fff - 32 * (0x400 >> shift);
    if (offset < 0)
        offset = 0;
    glFogShift(shift);
    glFogOffset(offset);
    for (int n = 0; n < 32; n++)
    {
        float d = distance_of(offset + (n + 1) * (0x400 >> shift));
        float density = d <= from ? 0.0f : d >= to ? 1.0f : (d - from) / (to - from);
        int v = (int)(density * 127.0f + 0.5f);
        glFogDensity(n, v > 127 ? 127 : v);
    }
}

static void draw_star(const int pos[3], int size, u16 color, int alpha, int id)
{
    model_sync();
    glPushMatrix();
    glTranslatef32(pos[0], pos[1], pos[2]);
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(id));
    glBegin(GL_TRIANGLES);
    glColor(color);
    for (int plane = 0; plane < 3; plane++)
    {
        static const int corner[6][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, -1 }, { 1, 1 }, { -1, 1 } };
        for (int c = 0; c < 6; c++)
        {
            int a = corner[c][0] * size, b = corner[c][1] * size;
            if (plane == 0) glVertex3v16(a, b, 0);
            else if (plane == 1) glVertex3v16(0, a, b);
            else glVertex3v16(a, 0, b);
        }
    }
    glEnd();
    glPopMatrix(1);
}

static void draw_line(const int from[3], const int to[3], u16 color, int id)
{
    model_sync();
    glPushMatrix();
    glTranslatef32(from[0], from[1], from[2]);
    glScalef32(inttof32(64), inttof32(64), inttof32(64));
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(id));
    glBegin(GL_TRIANGLES);
    glColor(color);
    int dx = (to[0] - from[0]) / 64, dy = (to[1] - from[1]) / 64, dz = (to[2] - from[2]) / 64;
    glVertex3v16(0, 0, 0);
    glVertex3v16(dx, dy, dz);
    glVertex3v16(dx, dy, dz);
    glEnd();
    glPopMatrix(1);
}

#define PLATE_SX 29
#define PLATE_SY 59
#define PLATE_W  99
#define PLATE_H  25
#define PLATE_X  (256 - PLATE_W - 3)
#define PLATE_Y  (192 - PLATE_H - 4)
#define TRACK_X  1
#define TRACK_Y  19
#define TRACK_W  96
#define TRACK_H  4
#define LIFE_BAR_X 120
#define FILL_SX  90
#define GREEN_SY 110
#define RED_SY   118
#define DIGIT_SY 0
#define DIGIT_H  15
#define AMMO_X   25
#define AMMO_Y   1

static void draw_digit(int d, int x, int y)
{
    int sx = d == 0 ? 24 : d == 1 ? 36 : 40 + (d - 2) * 8, w = d == 1 ? 4 : 8;
    overlay_sheet(&hud, sx, DIGIT_SY, w, DIGIT_H, x + (8 - w) / 2, y, w, DIGIT_H, RGB15(31, 31, 31), 31, 24);
}

#define POPUP_FRAMES 120
#define SPARE_LINE   11
#define POPUP_LINE   10
#define CLEAR_LINE   9
#define PROMPT_LINE  8
#define MEDAL_LINE   7
#define OBJ_LINE     5
#define OBJ_TITLE_LINE 4
#define OBJ_HINT_LINE 3
static int hud_on = 1, bottom_items = 0;
static void draw_plate(int hp, int ammo, int dy);

#define BONUS_FRAMES 90
#define BONUS_LINE   12
static const char *bonus_text[2];
static int bonus_t[2], felled_by_blast;
static void bonus_note(const char *s)
{
    bonus_text[1] = bonus_text[0];
    bonus_t[1] = bonus_t[0];
    bonus_text[0] = s;
    bonus_t[0] = BONUS_FRAMES;
}
static void bonus_draw(void)
{
    for (int k = 0; k < 2; k++)
    {
        if (bonus_t[k] > 0)
        {
            bonus_t[k]--;
            text_line(BONUS_LINE + k, 4, 4 + k * 15, TEXT_GOLD, bonus_text[k]);
        }
        else
            text_hide(BONUS_LINE + k);
    }
}
static void draw_hud(int hp, int ammo, int flash, int dead, int popup_type, int popup_value, int popup)
{
    overlay_begin();
    if (popup > 0)
    {
        int ix, iy;
        drops_icon(popup_type, popup_value, &ix, &iy);
        int rise = popup > POPUP_FRAMES - 8 ? (popup - (POPUP_FRAMES - 8)) * 2 : 0;
        int alpha = popup < 16 ? popup * 2 : 31;
        overlay_rect(20, 128 + rise, 236, 156 + rise, RGB15(0, 0, 0), alpha / 2 + 1, 25);
        overlay_sheet(&items, ix, iy, 32, 24, 26, 130 + rise, 32, 24, RGB15(31, 31, 31), alpha, 26);
    }
    if (dead)
        overlay_rect(0, 0, 256, 192, RGB15(10, 0, 0), 14, 20);
    else if (flash > 0)
        overlay_rect(0, 0, 256, 192, RGB15(31, 0, 0), flash, 20);

    if (hud_on)
        draw_plate(hp, ammo, 0);
    overlay_end();
}

static void draw_plate(int hp, int ammo, int dy)
{
    if (ammo > 99)
        ammo = 99;
    overlay_sheet(&hud, PLATE_SX, PLATE_SY, PLATE_W, PLATE_H, PLATE_X, PLATE_Y - dy, PLATE_W, PLATE_H, RGB15(31, 31, 31), 31, 21);
    int right = PLATE_X + TRACK_X + TRACK_W, len = right - LIFE_BAR_X, y = PLATE_Y - dy + TRACK_Y;
    overlay_rect(LIFE_BAR_X - 1, y - 1, right + 1, y + TRACK_H + 1, RGB15(0, 0, 0), 31, 21);
    int fill = len * hp / LEON_HP, x = right - fill;
    if (fill > 0)
    {
        int danger = hp <= LEON_HP / 3, caution = !danger && hp <= LEON_HP * 2 / 3;
        overlay_sheet(&hud, FILL_SX, danger ? RED_SY : GREEN_SY, 4, TRACK_H, x, y, fill, TRACK_H, RGB15(31, 31, 31), 31, 22);
        if (caution)
            overlay_sheet(&hud, FILL_SX, RED_SY, 4, TRACK_H, x, y, fill, TRACK_H, RGB15(31, 31, 31), 16, 23);
    }
    if (ammo >= 10)
        draw_digit(ammo / 10 % 10, PLATE_X + AMMO_X, PLATE_Y - dy + AMMO_Y);
    draw_digit(ammo % 10, PLATE_X + AMMO_X + 8, PLATE_Y - dy + AMMO_Y);
}

static void walk_out(const Ground *g, int pos[3], int dx, int dz, int dist, int y)
{
    for (int d = 0; d < dist; d += inttof32(1))
    {
        int step = dist - d < inttof32(1) ? dist - d : inttof32(1);
        pos[0] += mulf32(dx, step);
        pos[2] += mulf32(dz, step);
        ground_collide(g, &pos[0], &pos[2], y + STEP_UP, y + LEON_HEIGHT, LEON_RADIUS);
    }
}

static void cam_setup(int stage)
{
    int giants = stage == 3 || stage == 10;
    float pitch = (giants ? -200 : stage == 9 ? 2400 : 600) * 6.2831853f / 65536.0f, yaw = 1200 * 6.2831853f / 65536.0f;
    float pivot_up = (giants ? 4280 : 4080) / 256.0f, pivot_back = 670 / 256.0f, pivot_right = 670 / 256.0f, dis = 5000 / 256.0f;
    float back = pivot_back + dis * cosf(pitch) * cosf(yaw), right = pivot_right + dis * cosf(pitch) * sinf(yaw);
    float up = pivot_up + dis * sinf(pitch), ahead = back + f32tofloat(CAM_AHEAD);
    cam_back = floattof32(back);
    cam_right = floattof32(right);
    cam_height = floattof32(up);
    cam_at_right = floattof32(right - ahead * tanf(yaw));
    cam_at_height = floattof32(up - ahead * tanf(pitch) / cosf(yaw));
}

static void set_fov(int deg)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspectivef32(degreesToAngle(deg), floattof32(256.0 / 192.0), floattof32(NEAR_PLANE), floattof32(FAR_PLANE));
    glMatrixMode(GL_MODELVIEW);
}

static void draw_scope(void)
{
    overlay_begin();
    const int cx = 128 * 64, cy = 96 * 64, r = SCOPE_R * 64, rim = (SCOPE_R + 2) * 64, far = 300 * 64;
    glBindTexture(0, 0);
    for (int pass = 0; pass < 2; pass++)
    {
        int r0 = pass ? r : rim, r1 = pass ? rim : far;
        glPolyFmt(POLY_ALPHA(pass ? 31 : 20) | POLY_CULL_NONE | POLY_ID(40 + pass));
        glBegin(GL_QUADS);
        glColor(pass ? RGB15(1, 1, 1) : RGB15(10, 10, 10));
        for (int i = 0; i < 32; i++)
        {
            int a0 = i * (DEGREES_IN_CIRCLE / 32), a1 = (i + 1) * (DEGREES_IN_CIRCLE / 32);
            int x0 = mulf32(cosLerp(a0), inttof32(1)), y0 = mulf32(sinLerp(a0), inttof32(1));
            int x1 = mulf32(cosLerp(a1), inttof32(1)), y1 = mulf32(sinLerp(a1), inttof32(1));
            glVertex3v16(cx + mulf32(x0, r0), cy + mulf32(y0, r0), 2048);
            glVertex3v16(cx + mulf32(x1, r0), cy + mulf32(y1, r0), 2048);
            glVertex3v16(cx + mulf32(x1, r1), cy + mulf32(y1, r1), 2048);
            glVertex3v16(cx + mulf32(x0, r1), cy + mulf32(y0, r1), 2048);
        }
        glEnd();
    }
    overlay_rect(128 - SCOPE_R + 4, 96, 128 + SCOPE_R - 4, 97, RGB15(31, 2, 2), 31, 42);
    overlay_rect(128, 96 - SCOPE_R + 4, 129, 96 + SCOPE_R - 4, RGB15(31, 2, 2), 31, 43);
    overlay_end();
}

#define ASH_PLATE_SX 24
#define ASH_PLATE_SY 32
#define ASH_PLATE_W  104
#define ASH_PLATE_H  27
#define ASH_PLATE_X  (256 - ASH_PLATE_W - 3)
#define ASH_PLATE_Y  (PLATE_Y - ASH_PLATE_H + 3)
#define ASH_TRACK_X  2
#define ASH_TRACK_Y  14
#define ASH_TRACK_W  75
#define ASH_ICON_SX  20
#define ASH_ICON_SY  16
#define ASH_ICON_X   86
#define ASH_ICON_Y   9

static void draw_ash_hud(int hp, int max, int waiting)
{
    overlay_begin();
    overlay_sheet(&hud, ASH_PLATE_SX, ASH_PLATE_SY, ASH_PLATE_W, ASH_PLATE_H, ASH_PLATE_X, ASH_PLATE_Y, ASH_PLATE_W, ASH_PLATE_H, RGB15(31, 31, 31), 31, 21);
    int fill = hp > 0 ? (ASH_TRACK_W * hp + max - 1) / max : 0, y = ASH_PLATE_Y + ASH_TRACK_Y;
    int x = ASH_PLATE_X + ASH_TRACK_X + ASH_TRACK_W - fill;
    if (fill > 0)
    {
        int danger = hp <= max / 3, caution = !danger && hp <= max * 2 / 3;
        overlay_sheet(&hud, FILL_SX, danger ? RED_SY : GREEN_SY, 4, TRACK_H, x, y, fill, TRACK_H, RGB15(31, 31, 31), 31, 22);
        if (caution)
            overlay_sheet(&hud, FILL_SX, RED_SY, 4, TRACK_H, x, y, fill, TRACK_H, RGB15(31, 31, 31), 16, 23);
    }
    overlay_sheet(&hud, ASH_ICON_SX + (waiting ? 16 : 0), ASH_ICON_SY, 16, 16, ASH_PLATE_X + ASH_ICON_X, ASH_PLATE_Y + ASH_ICON_Y, 16, 16, RGB15(31, 31, 31), 31, 24);
    overlay_end();
}

enum { DROP_ROUNDS_HANDGUN = 10, DROP_ROUNDS_MAGNUM = 5, DROP_ROUNDS_SHOTGUN = 5, DROP_ROUNDS_RIFLE = 3, DROP_ROUNDS_TMP = 50 };
static const u8 drop_rounds[] = { DROP_ROUNDS_HANDGUN, DROP_ROUNDS_MAGNUM, DROP_ROUNDS_SHOTGUN, DROP_ROUNDS_RIFLE, DROP_ROUNDS_TMP };
static int multi_ammo_gun(int w) { return w < (int)sizeof(drop_rounds) ? w : WEP_HANDGUN; }
static void code_drop(const int ground[3], int code)
{
    static const s8 c7[4] = { WEP_HANDGUN, WEP_SHOTGUN, WEP_RIFLE, WEP_TMP }, c8[4] = { WEP_RIFLE, WEP_TMP, WEP_MAGNUM, -1 };
    static const s8 c12[6] = { -1, WEP_HANDGUN, WEP_SHOTGUN, WEP_RIFLE, -1, -2 }, c13[6] = { -1, WEP_TMP, WEP_MAGNUM, -1, -1, -2 };
    int type = -1, value = 0, w = -1;
    switch (code)
    {
    case 1: case 2: type = DROP_HERB; break;
    case 5: type = DROP_MONEY; value = 600 + (int)(rand() % 5) * 100; break;
    case 6: type = DROP_MONEY; value = 1100 + (int)(rand() % 5) * 100; break;
    case 7: w = c7[rand() & 3]; break;
    case 8: w = c8[rand() & 3]; break;
    case 9: w = WEP_HANDGUN; break;
    case 10: type = rand() % 3 ? DROP_HERB : -1; break;
    case 11: type = rand() & 3 ? DROP_HERB : -1; break;
    case 12: w = c12[rand() % 6]; break;
    case 13: w = c13[rand() % 6]; break;
    case 14: type = DROP_TREASURE; value = 2000; break;
    case 15: type = DROP_TREASURE; value = 3000; break;
    }
    if (w == -2)
        type = DROP_HERB;
    else if (w >= 0)
        type = DROP_AMMO;
    if (type >= 0)
        drops_spawn_value(&drops, ground, type, DROP_DELAY, value);
}

static void mission_drop(const Enemy *e)
{
    int ground[3] = { e->pos[0], e->feet, e->pos[2] };
    code_drop(ground, e->drop);
}

static void maybe_drop(const Enemy *e, int leon_hp, int percent)
{
    (void)leon_hp;
    (void)percent;
    if (e->drop >= 0)
        mission_drop(e);
}

static void crate_drop(const Obj *o, const int leon_at[3])
{
    if (o->drop < 0)
        return;
    int floor = o->pos[1];
    ground_height(&stage_ground, o->pos[0], o->pos[2], o->pos[1], 0, &floor);
    if (o->pos[1] - floor <= inttof32(3))
    {
        code_drop(o->pos, o->drop);
        return;
    }
    int dx = leon_at[0] - o->pos[0], dz = leon_at[2] - o->pos[2];
    int len = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
    int at[3] = { leon_at[0], leon_at[1], leon_at[2] };
    if (len > 0)
    {
        int ux = divf32(dx, len), uz = divf32(dz, len);
        for (int d = inttof32(2); d <= inttof32(20); d += inttof32(1))
        {
            int x = o->pos[0] + mulf32(ux, d), z = o->pos[2] + mulf32(uz, d), y = leon_at[1];
            if (!ground_height(&stage_ground, x, z, leon_at[1] + STEP_UP, STEP_UP * 2, &y))
                continue;
            int cx = x, cz = z;
            ground_collide(&stage_ground, &cx, &cz, y + STEP_UP, y + inttof32(10), inttof32(3));
            if (cx != x || cz != z)
                continue;
            at[0] = x; at[1] = y; at[2] = z;
            break;
        }
    }
    code_drop(at, o->drop);
}
#define ST_X(x)   (128 + (x) * 4 / 5)
#define ST_Y(y)   (96 + (y) * 4 / 5)
#define ST_S(n)   ((n) * 4 / 5)
#define ST_ROWS   4
enum { ST_GUN, ST_AMMO, ST_HERB };
static const u8 st_tam[] = { 6, 2, 2 };

static void item_icon(int item, int *sx, int *sy)
{
    static const struct { u8 item, x, y; } at[] = {
        { 4, 0, 0 }, { 6, 32, 0 }, { 7, 64, 0 }, { 5, 96, 0 }, { 8, 0, 96 },
        { 20, 64, 96 }, { 22, 96, 96 }, { 23, 0, 24 }, { 21, 32, 24 }, { 24, 64, 24 }, { 9, 0, 48 },
        { 71, 96, 72 } };
    for (unsigned i = 0; i < sizeof(at) / sizeof(at[0]); i++)
        if (at[i].item == item)
        {
            *sx = at[i].x;
            *sy = at[i].y;
            return;
        }
    *sx = *sy = 0;
}
#define AMMO_ITEM(w) ((w) == WEP_HANDGUN ? 20 : (w) == WEP_MAGNUM ? 21 : (w) == WEP_SHOTGUN ? 22 : (w) == WEP_RIFLE ? 23 : 24)
typedef struct { int type, count, weapon; } StItem;
static Sheet menu_sheet;
static int menu_loaded;

static void menu_piece(int sx, int sy, int sw, int sh, int x, int y, int w, int h)
{
    overlay_sheet(&menu_sheet, sx, sy, sw, sh, ST_X(x), ST_Y(y), ST_X(x + w) - ST_X(x), ST_Y(y + h) - ST_Y(y), RGB15(31, 31, 31), 31, 25);
}

static void menu_window(int x, int y, int w, int h)
{
    if (!menu_loaded)
    {
        overlay_rect(ST_X(x), ST_Y(y), ST_X(x + w + 32), ST_Y(y + h + 32), RGB15(2, 2, 2), 31, 25);
        return;
    }
    overlay_rect(ST_X(x + 16), ST_Y(y + 16), ST_X(x + 16 + w), ST_Y(y + 16 + h), RGB15(0, 0, 0), 31, 25);
    menu_piece(0, 80, 16, 16, x, y, 16, 16);
    menu_piece(0, 64, 176, 16, x + 16, y, w, 16);
    menu_piece(16, 80, 16, 16, x + 16 + w, y, 16, 16);
    menu_piece(240, 0, 16, 112, x, y + 16, 16, h);
    menu_piece(240, 0, 16, 112, x + 16 + w, y + 16, 16, h);
    menu_piece(32, 80, 16, 16, x, y + 16 + h, 16, 16);
    menu_piece(0, 64, 176, 16, x + 16, y + 16 + h, w, 16);
    menu_piece(48, 80, 16, 16, x + 16 + w, y + 16 + h, 16, 16);
}

#define ST_PLATE_DY 29
static void draw_status(const StItem *it, int n, int top, int sel, int cmds)
{
    overlay_begin();
    if (menu_loaded)
    {
        menu_piece(0, 0, 240, 32, -160, -112, 320, 32);
        menu_piece(80, 96, 156, 26, -160, -85, 208, 26);
        for (int i = 0; i < 5; i++)
            menu_piece(80, 32, 156, 32, -160, -59 + 32 * i, 208, 32);
        menu_piece(0, 0, 240, 32, -160, 80, 320, 32);
        menu_piece(203, 64, 37, 29, 54, 80, 37, 29);
    }
    else
    {
        overlay_rect(0, 0, 256, ST_Y(-80), RGB15(4, 3, 2), 31, 25);
        overlay_rect(0, ST_Y(-85), ST_X(48), ST_Y(80), RGB15(3, 3, 4), 31, 25);
        overlay_rect(0, ST_Y(80), 256, 192, RGB15(4, 3, 2), 31, 25);
    }
    for (int r = 0; r < ST_ROWS && top + r < n; r++)
    {
        const StItem *t = &it[top + r];
        int ix, iy, dy = 31 * r;
        if (t->type == ST_HERB)
            drops_icon(DROP_HERB, 0, &ix, &iy);
        else
            item_icon(t->type == ST_GUN ? weapons[t->weapon].item : AMMO_ITEM(t->weapon), &ix, &iy);
        overlay_rect(ST_X(-139), ST_Y(-59 + dy), ST_X(-139) + ST_S(32), ST_Y(-59 + dy) + ST_S(24), RGB15(1, 1, 1), 31, 26);
        overlay_sheet(&items, ix, iy, 32, 24, ST_X(-139), ST_Y(-59 + dy), ST_S(32), ST_S(24), RGB15(31, 31, 31), 31, 26);
        if (menu_loaded)
        {
            if (t->type == ST_GUN && t->weapon == wep)
                menu_piece(240, 112, 6, 7, -137, -58 + dy, 6, 7);
        }
        if (top + r == sel)
        {
            int x0 = ST_X(-140), y0 = ST_Y(-61 + dy), x1 = ST_X(38), y1 = y0 + ST_S(31) - 1;
            overlay_line(x0, y0, x1, y0, RGB15(27, 7, 7), 27);
            overlay_line(x0, y1, x1, y1, RGB15(27, 7, 7), 27);
            overlay_line(x0, y0, x0, y1, RGB15(27, 7, 7), 27);
            overlay_line(x1, y0, x1, y1, RGB15(27, 7, 7), 27);
        }
    }
    for (int k = 0; k < 2; k++)
        if (k == 0 ? top > 0 : top + ST_ROWS < n)
            for (int j = 0; j < 4; j++)
            {
                int y = k == 0 ? ST_Y(-77) + 2 + j : ST_Y(66) + 5 - j;
                overlay_line(ST_X(-51) + 4 - j, y, ST_X(-51) + 4 + j, y, RGB15(31, 27, 8), 28);
            }
    if (cmds)
        menu_window(52, -42, 64, 16 * cmds);
    overlay_end();
}

static void *wep_buf[2];

static void weapon_switch(int w, int *ammo, int *spare)
{
    (void)spare;
    wep_ammo[wep] = *ammo;
    model_free(&gun);
    free(wep_buf[0]);
    free(wep_buf[1]);
    wep_buf[0] = wep_buf[1] = NULL;
    int ok = 0;
    if (weapons[w].model && (wep_buf[0] = read_file(weapons[w].model)) && (wep_buf[1] = read_file(weapons[w].anim)))
        ok = model_load(&gun, wep_buf[0], file_size(weapons[w].model)) == 0
             && anim_load(&leon_gun, wep_buf[1], file_size(weapons[w].anim)) == 0 && leon_gun.nodes == leon.node_count;
    if (!ok)
    {
        w = WEP_HANDGUN;
        model_load(&gun, gun_bin, gun_bin_size);
        anim_load(&leon_gun, leon_gun_bin, leon_gun_bin_size);
    }
    wep = w;
    *ammo = wep_ammo[w];
}

static int case_used(int spare, int herbs)
{
    int used = herbs * 2 + (spare + MULTI_BOX - 1) / MULTI_BOX * 2;
    for (int w = 0; w < WEP_COUNT; w++)
        if (wep_owned >> w & 1)
            used += weapons[w].tam;
    return used;
}

static void stat_text(char *s, int v)
{
    if (v % 10)
        sprintf(s, "%d.%d", v / 10, v % 10);
    else
        sprintf(s, "%d", v / 10);
}

#define SAVE_NAME   "re4ds.sav"
#define SAVE_MAGIC  0x53344552
#define SAVE_VER    3
typedef struct {
    u32 magic, version;
    int ptas, owned, case_level, herbs, weapon;
    int spare[WEP_COUNT];
    u8 level[WEP_COUNT][WEP_STATS];
    int armor;
    int last_rank;
    u32 check;
} SaveData;
#define SAVE_V1_SIZE offsetof(SaveData, armor)
#define SAVE_V2_SIZE offsetof(SaveData, last_rank)
static SaveData save_data;
static int save_ok, have_save;

static u32 save_sum(const SaveData *d, size_t n)
{
    u32 sum = 0x1234;
    const u8 *b = (const u8 *)d;
    for (size_t i = 0; i < n; i++)
        sum = sum * 31 + b[i];
    return sum;
}

static void save_path(char *s, size_t n, const char *name)
{
    snprintf(s, n, "%s%s", fatGetDefaultDrive(), name);
}

static void save_read(void)
{
    char path[32];
    save_ok = fatInitDefault();
    if (!save_ok)
        return;
    save_path(path, sizeof(path), SAVE_NAME);
    FILE *f = fopen(path, "rb");
    if (!f)
        return;
    SaveData d;
    memset(&d, 0, sizeof(d));
    size_t got = fread(&d, 1, sizeof(d), f);
    fclose(f);
    int good = 0;
    if (got == sizeof(d) && d.version == SAVE_VER)
        good = d.check == save_sum(&d, offsetof(SaveData, check));
    else if (got == SAVE_V1_SIZE + sizeof(u32) && d.version == 1)
    {
        u32 check;
        memcpy(&check, (const u8 *)&d + SAVE_V1_SIZE, sizeof(check));
        good = check == save_sum(&d, SAVE_V1_SIZE);
        d.armor = 0;
    }
    else if (got == SAVE_V2_SIZE + sizeof(u32) && d.version == 2)
    {
        u32 check;
        memcpy(&check, (const u8 *)&d + SAVE_V2_SIZE, sizeof(check));
        good = check == save_sum(&d, SAVE_V2_SIZE);
        d.last_rank = 0;
    }
    if (good && d.magic == SAVE_MAGIC && d.armor >= 0 && d.armor <= 3 && d.last_rank >= 0 && d.last_rank <= 4
        && d.case_level >= 0 && d.case_level <= 3 && d.weapon >= 0 && d.weapon < WEP_COUNT
        && (d.owned >> d.weapon & 1) && (d.owned & ~((1 << WEP_COUNT) - 1)) == 0)
    {
        save_data = d;
        have_save = 1;
    }
}

static void save_write(int spare, int herbs, int ptas)
{
    SaveData *d = &save_data;
    memset(d, 0, sizeof(*d));
    d->magic = SAVE_MAGIC;
    d->version = SAVE_VER;
    d->ptas = ptas;
    d->owned = wep_owned;
    d->case_level = case_level;
    d->herbs = herbs;
    d->weapon = wep;
    d->spare[0] = spare;
    memcpy(d->level, wep_level, sizeof(d->level));
    d->armor = armor_level;
    d->last_rank = last_rank;
    d->check = save_sum(d, offsetof(SaveData, check));
    have_save = 1;
    if (!save_ok)
        return;
    char path[32], tmp[32];
    save_path(path, sizeof(path), SAVE_NAME);
    save_path(tmp, sizeof(tmp), "re4ds.tmp");
    FILE *f = fopen(tmp, "wb");
    if (!f)
        return;
    int put = fwrite(d, 1, sizeof(*d), f) == sizeof(*d);
    fclose(f);
    if (put)
    {
        remove(path);
        rename(tmp, path);
    }
}

static void save_apply(int *ammo, int *spare, int *herbs, int *ptas)
{
    const SaveData *d = &save_data;
    memcpy(wep_level, d->level, sizeof(wep_level));
    wep_owned = d->owned;
    case_level = d->case_level;
    armor_level = d->armor;
    last_rank = d->last_rank;
    for (int w = 0; w < WEP_COUNT; w++)
        wep_ammo[w] = wep_mag(w);
    if (d->weapon != wep)
        weapon_switch(d->weapon, ammo, spare);
    *ammo = MAG_SIZE;
    *spare = 0;
    for (int w = 0; w < WEP_COUNT; w++)
        *spare += d->spare[w] > 0 ? d->spare[w] : 0;
    *herbs = d->herbs;
    *ptas = d->ptas;
}

#define NEW_GAME_PTAS 30000
static void new_game(int *ammo, int *spare, int *herbs, int *ptas)
{
    if (wep != WEP_HANDGUN)
        weapon_switch(WEP_HANDGUN, ammo, spare);
    memset(wep_level, 0, sizeof(wep_level));
    wep_owned = 1;
    case_level = 0;
    armor_level = 0;
    last_rank = 0;
    for (int w = 0; w < WEP_COUNT; w++)
        wep_ammo[w] = wep_mag(w);
    *ammo = MAG_SIZE;
    *spare = SPARE_START;
    *herbs = 0;
    *ptas = NEW_GAME_PTAS;
}

enum { SHOP_ST_TABS, SHOP_ST_LIST };
enum { ROW_WEAPON, ROW_AMMO, ROW_HERB, ROW_CASE, ROW_VEST, ROW_SELL_WEAPON, ROW_SELL_HERB, ROW_UP_WEAPON, ROW_UP_STAT };
typedef struct { int kind, arg, price, ix, iy; char name[26], note[28]; } ShopRow;
#define SHOP_ROWS 12

static void draw_shop(const ShopRow *it, int n, int top, int sel)
{
    overlay_begin();
    if (menu_loaded)
    {
        menu_piece(0, 0, 240, 32, -160, -112, 320, 32);
        menu_piece(80, 96, 156, 26, -160, -85, 320, 26);
        for (int i = 0; i < 5; i++)
            menu_piece(80, 32, 156, 32, -160, -59 + 32 * i, 320, 32);
        menu_piece(0, 0, 240, 32, -160, 80, 320, 32);
        menu_piece(203, 64, 37, 29, -150, 80, 37, 29);
    }
    else
        overlay_rect(0, 0, 256, 192, RGB15(3, 3, 4), 31, 25);
    overlay_rect(ST_X(-156), ST_Y(-110), ST_X(-156) + ST_S(36), ST_Y(-110) + ST_S(28), RGB15(1, 1, 1), 31, 26);
    {
        int x0 = ST_X(-156), y0 = ST_Y(-110), x1 = x0 + ST_S(36) - 1, y1 = y0 + ST_S(28) - 1;
        u16 c = RGB15(20, 17, 10);
        overlay_line(x0, y0, x1, y0, c, 27);
        overlay_line(x0, y1, x1, y1, c, 27);
        overlay_line(x0, y0, x0, y1, c, 27);
        overlay_line(x1, y0, x1, y1, c, 27);
    }
    if (n > 0 && sel >= 0 && sel < n)
    {
        if (it[sel].kind == ROW_CASE && menu_loaded)
            menu_piece(203, 64, 37, 29, -154, -108, 32, 24);
        else
            overlay_sheet(&items, it[sel].ix, it[sel].iy, 32, 24, ST_X(-154), ST_Y(-108), ST_S(32), ST_S(24), RGB15(31, 31, 31), 31, 27);
    }
    for (int r = 0; r < ST_ROWS && top + r < n; r++)
    {
        int dy = 31 * r;
        overlay_rect(ST_X(-139), ST_Y(-59 + dy), ST_X(-139) + ST_S(32), ST_Y(-59 + dy) + ST_S(24), RGB15(1, 1, 1), 31, 26);
        if (it[top + r].kind == ROW_CASE && menu_loaded)
            menu_piece(203, 64, 37, 29, -139, -59 + dy, 32, 24);
        else
            overlay_sheet(&items, it[top + r].ix, it[top + r].iy, 32, 24, ST_X(-139), ST_Y(-59 + dy), ST_S(32), ST_S(24), RGB15(31, 31, 31), 31, 26);
        if (top + r == sel)
        {
            int x0 = ST_X(-140), y0 = ST_Y(-61 + dy), x1 = ST_X(152), y1 = y0 + ST_S(31) - 1;
            overlay_line(x0, y0, x1, y0, RGB15(27, 7, 7), 27);
            overlay_line(x0, y1, x1, y1, RGB15(27, 7, 7), 27);
            overlay_line(x0, y0, x0, y1, RGB15(27, 7, 7), 27);
            overlay_line(x1, y0, x1, y1, RGB15(27, 7, 7), 27);
        }
    }
    for (int k = 0; k < 2; k++)
        if (k == 0 ? top > 0 : top + ST_ROWS < n)
            for (int j = 0; j < 4; j++)
            {
                int y = k == 0 ? ST_Y(-77) + 2 + j : ST_Y(66) + 5 - j;
                overlay_line(ST_X(-51) + 4 - j, y, ST_X(-51) + 4 + j, y, RGB15(31, 27, 8), 28);
            }
    overlay_end();
}

static void draw_title(void)
{
    use_picture(PIC_TITLE);
    overlay_begin();
    overlay_sheet(&picture, 0, 0, picture.w, picture.h, 0, 0, 256, 192, RGB15(31, 31, 31), 31, 1);
    overlay_end();
}

#define STAGES       12
#define ZOOM_FRAMES  30
#define SCROLL_FRAMES 10
#define SEL_X  43
#define SEL_Y  34
#define SEL_W  170
#define SEL_H  112

#define STAGE_DROPPED 5
static int stage_no(int i) { return i < STAGE_DROPPED ? i + 1 : i; }
static const char *const stage_names[] = { T_SN_1, T_SN_2, T_SN_3, T_SN_4, T_SN_5, T_SN_6, T_SN_7, T_SN_8, T_SN_9, T_SN_10, T_SN_11, T_SN_12 };
static const char *const mission_names[] = {
    "Bloodthirst", "Luis Sera", "Las Plagas", "The Giant's Cage", "Ashley's Rescue", "The Siege",
    "To the Great Castle", "The Blind Weapon", "The Guardian Pair", "Reunion", "The Furnace", "The Strongest Assassin" };
static const char *const mission_lines[][3] = {
    { "The nightmare begins! Leon", "enters the village. Defeat", "the Ganados!" },
    { "Captured and taken to a", "safe place, Leon meets", "Luis for the first time." },
    { "Leon's body writhes in pain", "as the Plaga parasite", "spreads." },
    { "Leon falls into a trap in", "the mine as the terrible", "El Gigante approaches!" },
    { "Leon gets into the church.", "Where could Ashley be", "held...?" },
    { "The villagers start to", "surround the house Leon", "is in." },
    { "Leon flees to the old", "castle, but what awaits", "him may be worse." },
    { "As Leon looks for a way", "out, the iron guardian,", "Garrador, appears!" },
    { "Standing between Ashley", "and Leon is not one, but", "two Garradors." },
    { "Leon rescues Ashley. But", "she is captured again", "by the Ganados!" },
    { "A trap puts Leon up", "against a real nightmare:", "two El Gigantes!" },
    { "Waiting for Leon at the", "old canal is the deadly", "assassin, Verdugo!" } };

static void select_picture(int i, int x, int y, int w, int h, int alpha, int id)
{
    overlay_sheet(&picture, (i % 3) * 85, (i / 3) * 56, 85, 56, x, y, w, h, RGB15(31, 31, 31), alpha, id);
}

static void draw_select(int sel, int from, int scroll, int dir, int tick, int zoom)
{
    use_picture(PIC_STAGESEL);
    overlay_begin();
    overlay_rect(0, 0, 256, 192, RGB15(2, 2, 3), 31, 1);
    int x = SEL_X, y = SEL_Y, w = SEL_W, h = SEL_H;
    if (zoom >= 0)
    {
        int t = inttof32(zoom) / ZOOM_FRAMES;
        t = inttof32(1) - mulf32(inttof32(1) - t, inttof32(1) - t);
        x += mulf32(0 - x, t);
        y += mulf32(0 - y, t);
        w += mulf32(256 - w, t);
        h += mulf32(192 - h, t);
        select_picture(sel, x, y, w, h, 31, 4);
    }
    else
    {
        int off = scroll > 0 ? scroll * 256 / SCROLL_FRAMES : 0;
        if (scroll > 0 && from >= 0)
            select_picture(from, x - dir * (256 - off), y, w, h, 31, 3);
        overlay_rect(x - 2, y - 2, x + w + 2, y + h + 2, RGB15(12, 10, 6), 31, 2);
        select_picture(sel, x + dir * off, y, w, h, 31, 4);
        int p = 2 + sinLerp(tick * 700) * 2 / 4096, cy = y + h / 2;
        overlay_tri(8 - p, cy, 30 - p, cy - 18, 30 - p, cy + 18, RGB15(31, 0, 0), 31, 5);
        overlay_tri(248 + p, cy, 226 + p, cy - 18, 226 + p, cy + 18, RGB15(31, 0, 0), 31, 6);
    }
    overlay_end();
}
#define RES_TIME_W  46
#define RES_DIGIT_Y 1
#define RES_DIGIT_H 15
#define RES_BAR_Y   169
static int res_digit(int d, int x, int y)
{
    int sx = d == 0 ? 48 : d == 1 ? 60 : 64 + (d - 2) * 8, w = d == 1 ? 4 : 8;
    overlay_sheet(&picture, sx, RES_DIGIT_Y, w, RES_DIGIT_H, x + (8 - w) / 2, y, w, RES_DIGIT_H, RGB15(31, 31, 31), 31, 5);
    return x + 9;
}

static void draw_result(int seconds, int step)
{
    use_picture(PIC_RESULT);
    overlay_begin();
    overlay_rect(0, 0, 256, 192, RGB15(3, 1, 1), 31, 1);
    if (step >= 1)
    {
        overlay_sheet(&picture, 1, RES_BAR_Y, 148, 9, 24, 50, 208, 9, RGB15(31, 31, 31), 31, 2);
        int x = 64, y = 66;
        overlay_sheet(&picture, 0, RES_DIGIT_Y, RES_TIME_W, RES_DIGIT_H, x, y, RES_TIME_W, RES_DIGIT_H, RGB15(31, 31, 31), 31, 3);
        x += RES_TIME_W + 6;
        int m = seconds / 60 > 99 ? 99 : seconds / 60, s = seconds % 60;
        x = res_digit(m / 10, x, y);
        x = res_digit(m % 10, x, y);
        overlay_sheet(&picture, 42, RES_DIGIT_Y, 4, RES_DIGIT_H, x, y, 4, RES_DIGIT_H, RGB15(31, 31, 31), 31, 5);
        x += 6;
        x = res_digit(s / 10, x, y);
        res_digit(s % 10, x, y);
    }
    overlay_end();
}

static void show_menu(int first_line, int cx, int y, const char *const *items, int count, int selected)
{
    for (int i = 0; i < count; i++)
        text_line(first_line + i, cx - text_width(items[i]) / 2, y + i * 18, i == selected ? TEXT_GOLD : TEXT_WHITE, items[i]);
}

static int menu_move(u32 down, int selected, int count)
{
    if (down & KEY_UP)
        selected = (selected + count - 1) % count;
    if (down & KEY_DOWN)
        selected = (selected + 1) % count;
    return selected;
}

static void dim_3d(int level)
{
    REG_BLDCNT = level ? BLEND_FADE_BLACK | BLEND_SRC_BG0 : 0;
    REG_BLDY = level;
}

static void leon_to_world(const int pos[3], int yaw, const int v[3], int out[3])
{
    int s = sinLerp(yaw), c = cosLerp(yaw);
    out[0] = pos[0] + mulf32(v[0], c) + mulf32(v[2], s);
    out[1] = pos[1] + v[1];
    out[2] = pos[2] - mulf32(v[0], s) + mulf32(v[2], c);
}

static void approach(int cur[3], const int want[3])
{
    for (int k = 0; k < 3; k++)
        cur[k] += (want[k] - cur[k]) >> 3;
}

#ifdef TEST_BOT
#include "bot.h"
#endif
static int st_jya_out;

static void lid_rest(void)
{
    model_sync();
    int vol = music_asked();
    music_volume(0);
    fifoSendValue32(FIFO_PM, PM_REQ_OFF | PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
    fifoSendValue32(FIFO_PM, PM_REQ_LED | PM_LED_SLEEP);
    do
    {
        swiWaitForVBlank();
        scanKeys();
    } while (keysHeld() & KEY_LID);
    fifoSendValue32(FIFO_PM, PM_REQ_ON | PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM);
    fifoSendValue32(FIFO_PM, PM_REQ_LED | PM_LED_ON);
    music_volume(vol);
}
static void gfx_restore(int stage)
{
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankB(VRAM_B_TEXTURE);
    vramSetBankD(VRAM_D_TEXTURE);
    vramSetBankE(VRAM_E_TEX_PALETTE);
    vramSetBankF(VRAM_F_MAIN_SPRITE_0x06400000);
    vramSetBankG(VRAM_G_MAIN_SPRITE_0x06404000);
    vramSetBankC(VRAM_C_SUB_BG);
    REG_DISPCNT |= DISPLAY_BG0_ACTIVE | ENABLE_3D;
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ANTIALIAS);
    glEnable(GL_BLEND);
    glClearPolyID(63);
    glClearDepth(0x7FFF);
    glViewport(0, 0, 255, 191);
    if (stage >= 0)
    {
        set_haze(stage_info[stage].haze);
        stage_fog(stage_info[stage].fog);
    }
    else
    {
        glClearColor(HAZE_R, HAZE_G, HAZE_B, 31);
        setup_fog();
    }
}

int main(void)
{
    disableSleep();
    videoSetMode(MODE_0_3D);
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankB(VRAM_B_TEXTURE);
    vramSetBankD(VRAM_D_TEXTURE);
    vramSetBankE(VRAM_E_TEX_PALETTE);
    irqSet(IRQ_VBLANK, on_vblank);

    glInit();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ANTIALIAS);
    glEnable(GL_BLEND);
    glClearColor(HAZE_R, HAZE_G, HAZE_B, 31);
    setup_fog();
    glClearPolyID(63);
    glClearDepth(0x7FFF);
    glViewport(0, 0, 255, 191);

    int err_stage = nitroFSInit(NULL) ? load_stage(0) : -99;
    save_read();
    int err_leon = model_load(&leon, leon_bin, leon_bin_size);
    int err_anim = anim_load(&leon_anims, leon_anim_bin, leon_anim_bin_size);
    if (!err_anim && leon_anims.nodes != leon.node_count)
        err_anim = -10;
    int err_ground = 0;
    int err_gun = model_load(&gun, gun_bin, gun_bin_size);
    int err_aim = anim_load(&leon_gun, leon_gun_bin, leon_gun_bin_size);
    if (!err_aim && leon_gun.nodes != leon.node_count)
        err_aim = -10;
    if (!err_aim)
        err_aim = anim_load(&leon_knife, leon_knife_bin, leon_knife_bin_size) || leon_knife.nodes != leon.node_count
                  || model_load(&knife_model, knife_bin, knife_bin_size) ? -11 : 0;
    int err_gna = model_load(&ganado, ganado_bin, ganado_bin_size);
    if (!err_gna)
        err_gna = anim_load(&ganado_anims, ganado_anim_bin, ganado_anim_bin_size);
    if (!err_gna && ganado_anims.nodes != ganado.node_count)
        err_gna = -10;
    if (!err_gna)
        err_gna = anim_load(&ganado_w0, gna_w0_bin, gna_w0_bin_size) || anim_load(&ganado_w1, gna_w1_bin, gna_w1_bin_size)
                  || model_load(&sickle_model, sickle_bin, sickle_bin_size) || model_load(&axe_model, axe_bin, axe_bin_size)
                  || model_load(&fork_model, fork_bin, fork_bin_size) ? -20 : 0;
    if (!err_gna)
        err_gna = anim_load(&cha_anims, cha_anim_bin, cha_anim_bin_size) ? -30 : 0;
    int err_2d = sheet_load(&hud, hud_bin, hud_bin_size);
    if (!err_2d)
        err_2d = sheet_load(&items, items_bin, items_bin_size) * 2;
    if (!err_2d)
        err_2d = sheet_load(&star, star_bin, star_bin_size) * 3;
    if (!err_2d)
        err_2d = sheet_load(&beam, beam_bin, beam_bin_size) * 4;
    if (!err_2d)
        err_2d = sheet_load(&spark, spark_bin, spark_bin_size) * 5;
    if (!err_2d)
    {
        use_picture(PIC_TITLE);
        err_2d = picture_id == PIC_TITLE ? 0 : -6;
    }
    if (!err_2d)
        err_2d = text_init(font_bin, font_bin_size) * 100;
    if (!err_2d)
        err_2d = sfx_init(sfx_bin, sfx_bin_size) * 1000;
    if (!err_2d)
        err_2d = music_init(music_bin, music_bin_size) * 10000;
    if (!err_2d)
        err_2d = vital_init(vitalbmp_bin, vitalbmp_bin_size, iconbmp_bin, iconbmp_bin_size) * 100000;
    if (!err_2d)
    {
        err_2d = model_load(&ladder_model, ladder_bin, ladder_bin_size) || model_load(&crate_model, crate_bin, crate_bin_size);
        err_2d *= 1000000;
    }
    if (err_stage || err_leon || err_anim || err_ground || err_gun || err_aim || err_gna || err_2d)
    {
        consoleDemoInit();
        printf("load failed: stage %d leon %d anim %d\nground %d gun %d aim %d ganado %d\n2d %d\n",
               err_stage, err_leon, err_anim, err_ground, err_gun, err_aim, err_gna, err_2d);
        while (1)
            swiWaitForVBlank();
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspectivef32(degreesToAngle(CAM_FOV), floattof32(256.0 / 192.0), floattof32(NEAR_PLANE), floattof32(FAR_PLANE));

#ifdef LEON_CIRCLE_X
    const int circle_x = inttof32(LEON_CIRCLE_X), circle_z = inttof32(LEON_CIRCLE_Z);
#else
    const int circle_x = stage.ground[0], circle_z = stage.ground[2];
#endif
    const int circle_radius = inttof32(LEON_CIRCLE_R);

    int leon_feet = stage.ground[1];
#if defined(LEON_START_X)
    int leon_pos[3] = { inttof32(LEON_START_X), 0, inttof32(LEON_START_Z) };
#elif defined(AUTOPILOT) && !defined(STAGE_START)
    int leon_pos[3] = { circle_x, 0, circle_z + circle_radius };
#else
    int leon_pos[3] = { floattof32(STAGE_START_X), 0, floattof32(STAGE_START_Z) };
#endif
    ground_height(&stage_ground, leon_pos[0], leon_pos[2], inttof32(10000), 0, &leon_feet);
    leon_pos[1] = leon_feet - leon.bbox_min[1];
#if defined(LEON_START_YAW)
    int leon_yaw = degreesToAngle(LEON_START_YAW);
#elif defined(AUTOPILOT) && !defined(STAGE_START)
    int leon_yaw = 0;
#else
    int leon_yaw = degreesToAngle(STAGE_START_YAW);
#endif
    objects_init(&objs, &stage_ground, &door_model, &ladder_model, &barrel_model, &crate_model, &medal_model, &boom_sheet, &spark);
    objs.bell = &bell_model;
    enemies_init(&ganados, &ganado, &ganado_anims, &stage_ground, &objs, ganado_spawn, GANADO_COUNT, DEGREES_IN_CIRCLE / 2);
    stage1_kinds();
#ifdef TEST_SIGHT
    ganados.sight = inttof32(TEST_SIGHT);
#endif
    {
        const Model *const weapons[WEAPON_KINDS] = { NULL, &sickle_model, &axe_model, &fork_model };
        enemies_arm(&ganados, weapons, &ganado_w0, &ganado_w1);
    }
    const int start_pos[3] = { leon_pos[0], leon_pos[1], leon_pos[2] };
    const int start_feet = leon_feet, start_yaw = leon_yaw;
    int leon_angle = 0, anim_frame = 0;
    int leon_anim = LEON_START_MODE;
    int ctrl = START_CTRL;
    int aim_w = 0;
    int aim_tilt = 0;
    int gun_state = GUN_HOLSTERED;
    int draw_frame = 0;
    int clip_key = -1;
    int xfade_w = 0;
    int ammo = MAG_SIZE;
    int wep_note = 0;
    int gclimb = 0, gclimb_f[2] = { 0, 0 }, gclimb_y = 0, gcut_page = 0;
    int rocket_on = 0, rocket_pos[3] = { 0 }, rocket_to[3] = { 0 }, rocket_boom = 0;
    int scoping = 0, scope_was = 0, scope_dir[3] = { 0, 0, inttof32(1) };
    int leon_fade = 31;
    int shop_screen = SHOP_ST_TABS, shop_tab = 1, shop_sel = 0, shop_top = 0, shop_wep = -1, shop_msg = 0;
    const char *shop_text = "";
    int fire_frame = -1;
    int reload_frame = -1;
    int reload_ch = -1;
    int flash_timer = 0, impact_timer = 0, cam_kick = 0;
    int impact_pos[3] = { 0 }, muzzle[3] = { 0 }, laser_end[3] = { 0 }, laser_hit = 0;
    int laser_enemy = -1, laser_head = 0, laser_part = PART_BODY, impact_blood = 0, laser_obj = -1, laser_ash = 0, laser_giant = 0, laser_gal = 0, laser_gal_back = 0, laser_ver = 0, laser_drop = -1;
    int ash_lost = 0;
    u32 hits = 0, headshots = 0;
    u32 shots = 0;
    int leon_hp = LEON_HP;
    int react = REACT_NONE;
    int react_frame = 0;
    int holder = -1;
    int mash = 0, held_frames = 0, safe_timer = 0, hurt_flash = 0;
    int slash_frame = -1;
    int knife_w = 0;
    int act_obj = -1;
    int drop_to = 0, drop_vy = 0;
    int next_ganado = 0;
#ifndef START_STAGE
#define START_STAGE 1
#endif
    int cur_stage = START_STAGE - 1;
    int obj_t = 0;
#ifdef TEST_SHOP_PTAS
    int ptas = TEST_SHOP_PTAS;
#elif defined(TEST_SHOP)
    int ptas = 60000;
#else
    int ptas = 0;
#endif
    int treasures_found = 0;
    int boss_phase = 0, boss_t = 0;
    int medal_note = 0;
    int elevator_note = 0;
    ObjEvents oev;
    EnemyEvents ev;

    int eye[3] = { leon_pos[0], leon_feet + inttof32(14), leon_pos[2] + inttof32(45) };
    int yaw = 0, pitch = -degreesToAngle(8);
    int follow_eye[3] = { leon_pos[0] - mulf32(sinLerp(leon_yaw), CAM_BACK) - mulf32(cosLerp(leon_yaw), CAM_RIGHT),
                          leon_feet + CAM_HEIGHT,
                          leon_pos[2] - mulf32(cosLerp(leon_yaw), CAM_BACK) + mulf32(sinLerp(leon_yaw), CAM_RIGHT) };
    int follow_target[3] = { leon_pos[0] + mulf32(sinLerp(leon_yaw), CAM_AHEAD) - mulf32(cosLerp(leon_yaw), CAM_AT_RIGHT),
                             leon_feet + CAM_AT_HEIGHT,
                             leon_pos[2] + mulf32(cosLerp(leon_yaw), CAM_AHEAD) + mulf32(sinLerp(leon_yaw), CAM_AT_RIGHT) };
#ifdef TEST_CULL
    int cull = TEST_CULL;
#else
    int cull = 0;
#endif
    touchPosition last_touch = { 0 };

    u32 frames = 0, fps = 0, fps_mark = 0;
#ifdef AUTOPILOT
    u32 frame_no = 0;
#endif

#if (defined(AUTOPILOT) && AUTOPILOT != 10 && AUTOPILOT != 35 && AUTOPILOT != 41 && AUTOPILOT != 48 && AUTOPILOT != 51) || defined(SKIP_TITLE)
    int mode = MODE_PLAY;
#else
    int mode = MODE_TITLE;
#endif
#ifdef TEST_SHOP
    mode = MODE_SHOP;
#endif
    int menu_sel = 0, dead_timer = 0;
    int reset = mode == MODE_PLAY;
    int pending = -1, fade = 0;
    int stage_sel = 0, zoom = -1, shake = 0, tick = 0;
    int scroll = 0, scroll_from = -1, scroll_dir = 1;
    int spare = SPARE_START;
    int herbs = 0;
    int popup = 0, popup_type = 0, popup_value = 0;
    int qturn = 0;
    int cam_yaw = leon_yaw;
    const CutScene *msg = NULL;
    int msg_page = 0, msg_t = 0;
    int take_item = 0;
    int shot_noise = 0;
#ifdef TEST_CUTIN
    int cut_opening = 1;
#else
    int cut_opening = 0;
#endif
    int kneel_seen = 0, gal_seen = 0, ash_waiting = 0, giant_paid = 0;
    int st_sel = 0, st_top = 0, st_cmd = -1, st_msg = 0;
    int clear_t = -1;
    u32 play_frames = 0;
    int damage_taken = 0, result_t = 0;
    drops_init(&drops, &star, &beam, &spark, &items);
    static const char *const title_items[] = { T_CONTINUE, T_NEW_GAME, T_HOWTO };
    static const char *const over_items[] = { T_CONTINUE, T_TO_TITLE };
    static const char *const howto[] = { T_HOWTO_KEYS };
    int menu_shown = -1;
    while (1)
    {
        scanKeys();
        u32 down = keysDown(), held = keysHeld();
        if (held & KEY_LID)
        {
            lid_rest();
            gfx_restore(stage_loaded >= 0 ? stage_loaded : -1);
            scanKeys();
            down = 0;
            held = keysHeld();
        }
#ifdef TEST_BOT
        {
            static u32 bot_prev;
#include "bot_frame.inc"
            down = held & ~bot_prev;
            bot_prev = held;
        }
#endif
#ifdef AUTOPILOT
        if (ctrl == CTRL_LEON)
        {
            static u32 prev_held;
            held = autopilot_keys(frame_no);
            down = held & ~prev_held;
            prev_held = held;
        }
        frame_no++;
#if AUTOPILOT == 12
        static const struct { u16 at; u8 x, y; } taps[] = {
            { 300, 20, 68 }, { 400, 20, 110 }, { 500, 20, 110 },
            { 600, 20, 110 }, { 610, 20, 110 }, { 700, 20, 110 }, { 710, 20, 110 },
            { 800, 20, 110 }, { 810, 20, 110 }, { 900, 20, 68 }, { 910, 20, 68 } };
#elif AUTOPILOT == 78 && defined(AUTOPILOT_STRAIGHT)
        static const struct { u16 at; u8 x, y; } taps[] = { { 200, 109, 180 } };
#elif AUTOPILOT == 78
        static const struct { u16 at; u8 x, y; } taps[] = {
            { 300, 25, 180 }, { 420, 67, 180 }, { 540, 67, 180 }, { 660, 25, 180 }, { 780, 109, 180 },
            { 1000, 222, 180 }, { 1200, 222, 180 }, { 1300, 109, 180 }, { 1450, 162, 180 } };
#endif
#if AUTOPILOT == 12 || AUTOPILOT == 78
#ifndef TAPS_FROM
#define TAPS_FROM 0
#endif
        for (unsigned i = 0; i < sizeof(taps) / sizeof(taps[0]); i++)
            if (frame_no == taps[i].at + TAPS_FROM)
                vital_fake_touch(taps[i].x, taps[i].y);
#endif
#endif

        if (pending >= 0)
        {
            down = held = 0;
            fade += 2;
            if (fade >= 16)
            {
                fade = 16;
                mode = pending;
                pending = -1;
                menu_shown = -1;
            }
        }
        else if (fade > 0)
            fade -= 2;
        setBrightness(1, -fade);
        tick++;

        if (mode == MODE_TITLE || mode == MODE_HOWTO)
        {
            if (menu_shown != mode)
            {
                text_clear();
                menu_sel = menu_shown == MODE_HOWTO ? 1 + have_save : 0;
                menu_shown = mode;
            }
            music_play(MUSIC_TITLE, MUSIC_VOLUME);
            if (mode == MODE_TITLE)
            {
                dim_3d(0);
                menu_sel = menu_move(down, menu_sel, mode == MODE_TITLE ? 2 + have_save : 2);
                show_menu(0, 128, have_save ? 130 : 138, title_items + !have_save, 2 + have_save, menu_sel);
                if (down & (KEY_A | KEY_START))
                {
                    int item = menu_sel + !have_save;
                    sfx_play(SFX_MENU, 100, 64);
                    if (item == 0)
                        save_apply(&ammo, &spare, &herbs, &ptas);
                    else if (item == 1)
                        new_game(&ammo, &spare, &herbs, &ptas);
                    mode = item < 2 ? MODE_SELECT : MODE_HOWTO;
                }
            }
            else
            {
                dim_3d(12);
                text_line(0, 128 - text_width(T_HOWTO) / 2, 10, TEXT_GOLD, T_HOWTO);
                for (int i = 0; i < 8; i++)
                {
                    text_line(TEXT_WIDE_LINES + i, 10, 34 + i * 18, TEXT_GOLD, howto[i * 2]);
                    text_line(2 + i, 72, 34 + i * 18, TEXT_WHITE, howto[i * 2 + 1]);
                }
                text_line(1, 246 - text_width(T_BACK), 176, TEXT_GREY, T_BACK);
                if (down & (KEY_B | KEY_A | KEY_START))
                    mode = MODE_TITLE;
            }
            if (mode == MODE_TITLE || mode == MODE_HOWTO)
            {
                draw_title();
                model_sync();
                glFlush(0);
            }
            else
            {
                text_clear();
                dim_3d(0);
                menu_shown = -1;
            }
            swiWaitForVBlank();
            text_commit();
            vital_idle(T_CAPTION, T_EDITION);
            continue;
        }

        if (mode == MODE_SELECT)
        {
            if (menu_shown != MODE_SELECT)
            {
                text_clear();
                dim_3d(0);
                zoom = -1;
                menu_shown = MODE_SELECT;
            }
            music_play(MUSIC_TITLE, MUSIC_VOLUME);
            if (scroll > 0)
                scroll--;
            if (zoom < 0)
            {
                int step = (down & KEY_RIGHT) ? 1 : (down & KEY_LEFT) ? -1 : 0;
                if (step && scroll == 0)
                {
                    scroll_from = stage_sel;
                    do
                        stage_sel = (stage_sel + step + STAGES) % STAGES;
                    while (stage_sel == STAGE_DROPPED);
                    scroll = SCROLL_FRAMES;
                    scroll_dir = step;
                    sfx_play(SFX_MENU, 60, 64);
                }
                if ((down & (KEY_A | KEY_START)) && scroll == 0)
                {
                    sfx_play(SFX_MENU, 100, 64);
                    zoom = 0;
                }
                if (down & KEY_B)
                    mode = MODE_TITLE;
            }
            else if (zoom < ZOOM_FRAMES && ++zoom == ZOOM_FRAMES)
            {
                pending = MODE_SHOP;
                cur_stage = stage_sel;
            }
            if (shake > 0)
                shake--;
            char name[32];
            snprintf(name, sizeof(name), T_STAGE_N, stage_no(stage_sel), stage_names[stage_sel]);
            text_hide(0);
            text_line(1, 128 - text_width(name) / 2, 3, TEXT_GOLD, name);
            text_line(2, 128 - text_width(mission_names[stage_sel]) / 2, 17, TEXT_WHITE, mission_names[stage_sel]);
            for (int i = 0; i < 3; i++)
                text_line(3 + i, 128 - text_width(mission_lines[stage_sel][i]) / 2, 150 + i * 13, TEXT_WHITE, mission_lines[stage_sel][i]);
            if (zoom >= 0)
                for (int i = 0; i < 6; i++)
                    text_hide(i);
            if (mode == MODE_SELECT)
            {
                draw_select(stage_sel, scroll_from, scroll, scroll_dir, tick, zoom);
                model_sync();
                glFlush(0);
            }
            else
                menu_shown = -1;
            swiWaitForVBlank();
            text_commit();
            vital_idle(T_CAPTION, T_EDITION);
            continue;
        }

        if (mode == MODE_SHOP)
        {
            static const char *const tabs[] = { T_SHOP_START, T_SHOP_BUY, T_SHOP_SELL, T_SHOP_UPGRADE };
            static const s8 tab_x[] = { -116, -55, 10, 69 };
            static ShopRow rows[SHOP_ROWS];
            if (menu_shown != MODE_SHOP)
            {
                text_clear();
                dim_3d(0);
                menu_shown = MODE_SHOP;
                shop_screen = SHOP_ST_TABS;
                shop_tab = 1;
                shop_wep = -1;
                shop_msg = 0;
                sheet_free(&picture);
                picture_id = -1;
                if (!menu_loaded)
                    menu_loaded = sheet_load(&menu_sheet, menu_bin, menu_bin_size) == 0;
                glClearColor(0, 0, 0, 31);
            }
            music_play(MUSIC_TITLE, MUSIC_VOLUME);
            int n = 0;
            ShopRow *r = rows;
            #define ROW(k, a, p, it) (r = &rows[n++], *r = (ShopRow){ .kind = k, .arg = a, .price = p }, \
                                      (it) < 0 ? drops_icon(DROP_HERB, 0, &r->ix, &r->iy) : item_icon(it, &r->ix, &r->iy))
            if (shop_tab == 1)
            {
                for (int w = 0; w < WEP_COUNT; w++)
                {
                    ROW(ROW_WEAPON, w, BUY_PRICE(weapons[w].price), weapons[w].item);
                    snprintf(r->name, sizeof(r->name), "%s", weapons[w].name);
                    if (wep_owned >> w & 1)
                        snprintf(r->note, sizeof(r->note), T_SHOP_OWNED);
                    else
                        snprintf(r->note, sizeof(r->note), T_SHOP_PRICE, r->price);
                }
                ROW(ROW_AMMO, WEP_HANDGUN, BUY_PRICE(MULTI_BOX * weapons[WEP_HANDGUN].ammo_price), AMMO_ITEM(WEP_HANDGUN));
                snprintf(r->name, sizeof(r->name), T_ITEM_AMMO);
                snprintf(r->note, sizeof(r->note), T_SHOP_BOX, MULTI_BOX, r->price);
                ROW(ROW_HERB, 0, BUY_PRICE(1000), -1);
                snprintf(r->name, sizeof(r->name), T_ITEM_HERB);
                snprintf(r->note, sizeof(r->note), T_SHOP_PRICE, r->price);
                if (case_level < 3)
                {
                    ROW(ROW_CASE, 0, BUY_PRICE(case_price[case_level + 1]), 0);
                    snprintf(r->name, sizeof(r->name), T_SHOP_CASE_NAME);
                    snprintf(r->note, sizeof(r->note), T_SHOP_CASE, case_tam[case_level + 1], r->price);
                }
                if (armor_level < 3)
                {
                    static const char *const vest[] = { T_VEST_1, T_VEST_2, T_VEST_3 };
                    ROW(ROW_VEST, 0, BUY_PRICE(armor_price[armor_level + 1]), 71);
                    snprintf(r->name, sizeof(r->name), "%s", vest[armor_level]);
                    snprintf(r->note, sizeof(r->note), T_SHOP_VEST, armor_cut[armor_level + 1], r->price);
                }
            }
            else if (shop_tab == 2)
            {
                for (int w = 0; w < WEP_COUNT; w++)
                    if ((wep_owned >> w & 1) && w != wep)
                    {
                        ROW(ROW_SELL_WEAPON, w, weapons[w].price / 2, weapons[w].item);
                        snprintf(r->name, sizeof(r->name), "%s", weapons[w].name);
                        snprintf(r->note, sizeof(r->note), T_SHOP_PRICE, r->price);
                    }
                if (herbs > 0)
                {
                    ROW(ROW_SELL_HERB, 0, 500, -1);
                    snprintf(r->name, sizeof(r->name), T_ITEM_HERB);
                    snprintf(r->note, sizeof(r->note), T_SHOP_PRICE, r->price);
                }
            }
            else if (shop_tab == 3 && shop_wep < 0)
            {
                for (int w = 0; w < WEP_COUNT; w++)
                    if (wep_owned >> w & 1)
                    {
                        int done = 0;
                        for (int i = 0; i < WEP_STATS; i++)
                            done += wep_level[w][i];
                        ROW(ROW_UP_WEAPON, w, 0, weapons[w].item);
                        snprintf(r->name, sizeof(r->name), "%s", weapons[w].name);
                        snprintf(r->note, sizeof(r->note), T_SHOP_LEVELS, done);
                    }
            }
            else if (shop_tab == 3)
            {
                for (int i = 0; i < WEP_STATS; i++)
                {
                    if (!tune_at(shop_wep, i, 1))
                        continue;
                    int base = i == STAT_POWER ? weapons[shop_wep].power : i == STAT_CAPACITY ? weapons[shop_wep].mag * 10
                             : i == STAT_RATE ? 5 : tune_at(shop_wep, i, 1)->value * 5 / 4;
                    int now = wep_stat(shop_wep, i, base);
                    const ShopTune *next = tune_at(shop_wep, i, wep_level[shop_wep][i] + 1);
                    ROW(ROW_UP_STAT, i, next ? next->price : 0, weapons[shop_wep].item);
                    snprintf(r->name, sizeof(r->name), "%s", wep_stat_name[i]);
                    char a[8], b[8];
                    stat_text(a, now);
                    if (next)
                    {
                        stat_text(b, next->value);
                        snprintf(r->note, sizeof(r->note), T_SHOP_NEXT, a, b, r->price);
                    }
                    else
                        snprintf(r->note, sizeof(r->note), T_SHOP_TOP, a);
                }
            }
            #undef ROW
            if (shop_sel >= n)
                shop_sel = n > 0 ? n - 1 : 0;
            if (pending < 0 && shop_screen == SHOP_ST_TABS)
            {
                int was = shop_tab;
                if (down & KEY_LEFT)  shop_tab = (shop_tab + 3) % 4;
                if (down & KEY_RIGHT) shop_tab = (shop_tab + 1) % 4;
                if (shop_tab != was)
                    sfx_play(SFX_MENU, 60, 64);
                if (down & KEY_B)
                {
                    pending = MODE_SELECT;
                    save_write(spare, herbs, ptas);
                    sfx_play(SFX_MENU, 100, 64);
                }
                else if (down & KEY_A)
                {
                    sfx_play(SFX_MENU, 100, 64);
                    if (shop_tab == 0)
                    {
                        pending = MODE_PLAY;
                        reset = 1;
                        cut_opening = 1;
                        save_write(spare, herbs, ptas);
                    }
                    else
                    {
                        shop_screen = SHOP_ST_LIST;
                        shop_sel = shop_top = 0;
                        shop_wep = -1;
                        text_clear();
                    }
                }
            }
            else if (pending < 0)
            {
                if (down & KEY_B)
                {
                    sfx_play(SFX_MENU, 100, 64);
                    if (shop_tab == 3 && shop_wep >= 0)
                    {
                        shop_wep = -1;
                        shop_sel = 0;
                    }
                    else
                        shop_screen = SHOP_ST_TABS;
                    shop_msg = 0;
                    text_clear();
                }
                else if (n > 0)
                {
                    int was = shop_sel;
                    shop_sel = menu_move(down, shop_sel, n);
                    if (shop_sel != was)
                        sfx_play(SFX_MENU, 60, 64);
                    if (down & KEY_A)
                    {
                        const ShopRow *c = &rows[shop_sel];
                        int w = c->arg, got = 1;
                        const char *why = NULL;
                        int room = case_tam[case_level] - case_used(spare, herbs);
                        if (c->kind == ROW_WEAPON)
                        {
                            if (wep_owned >> w & 1)
                                got = 0;
                            else if (ptas < c->price)
                                why = T_SHOP_MONEY;
                            else if (room < weapons[w].tam)
                                why = T_SHOP_SPACE;
                            else
                            {
                                wep_owned |= 1 << w;
                                wep_ammo[w] = wep_mag(w);
                            }
                        }
                        else if (c->kind == ROW_AMMO)
                        {
                            int need = spare % MULTI_BOX ? 0 : 2;
                            if (ptas < c->price)
                                why = T_SHOP_MONEY;
                            else if (room < need)
                                why = T_SHOP_SPACE;
                            else
                                spare += MULTI_BOX;
                        }
                        else if (c->kind == ROW_HERB)
                        {
                            if (ptas < c->price)
                                why = T_SHOP_MONEY;
                            else if (room < 2)
                                why = T_SHOP_SPACE;
                            else
                                herbs++;
                        }
                        else if (c->kind == ROW_CASE)
                        {
                            if (ptas < c->price)
                                why = T_SHOP_MONEY;
                            else
                                case_level++;
                        }
                        else if (c->kind == ROW_VEST)
                        {
                            if (ptas < c->price)
                                why = T_SHOP_MONEY;
                            else
                                armor_level++;
                        }
                        else if (c->kind == ROW_SELL_WEAPON)
                        {
                            ptas += c->price;
                            wep_owned &= ~(1 << w);
                            memset(wep_level[w], 0, sizeof(wep_level[w]));
                            got = -1;
                        }
                        else if (c->kind == ROW_SELL_HERB)
                        {
                            ptas += c->price;
                            herbs--;
                            got = -1;
                        }
                        else if (c->kind == ROW_UP_WEAPON)
                        {
                            shop_wep = w;
                            shop_sel = shop_top = 0;
                            text_clear();
                            got = 0;
                            sfx_play(SFX_MENU, 100, 64);
                        }
                        else if (c->kind == ROW_UP_STAT)
                        {
                            const ShopTune *next = tune_at(shop_wep, w, wep_level[shop_wep][w] + 1);
                            if (!next)
                                why = T_SHOP_MAX;
                            else if (ptas < next->price)
                                why = T_SHOP_MONEY;
                            else
                            {
                                wep_level[shop_wep][w]++;
                                if (w == STAT_CAPACITY && shop_wep == wep)
                                    ammo = MAG_SIZE;
                                if (w == STAT_CAPACITY)
                                    wep_ammo[shop_wep] = wep_mag(shop_wep);
                            }
                        }
                        if (why)
                        {
                            shop_text = why;
                            shop_msg = 120;
                            sfx_play(SFX_EMPTY, 110, 64);
                        }
                        else if (got)
                        {
                            if (got > 0)
                                ptas -= c->price;
                            sfx_play(SFX_ITEM_GET, 110, 64);
                        }
                    }
                }
            }
            if (shop_sel < shop_top)
                shop_top = shop_sel;
            if (shop_sel >= shop_top + ST_ROWS)
                shop_top = shop_sel - ST_ROWS + 1;

            char m[28];
            snprintf(m, sizeof(m), "%d / %d", case_used(spare, herbs), case_tam[case_level]);
            text_line(14, ST_X(-108), ST_Y(89), TEXT_WHITE, m);
            snprintf(m, sizeof(m), T_SHOP_PRICE, ptas);
            text_line(13, ST_X(150) - text_width(m), ST_Y(89), TEXT_WHITE, m);
            if (shop_screen == SHOP_ST_TABS)
            {
                text_line(0, ST_X(150) - text_width(T_MISSION_SHOP), ST_Y(-103), TEXT_WHITE, T_MISSION_SHOP);
                text_line(1, ST_X(-100), ST_Y(-50), TEXT_WHITE, last_rank >= 3 ? T_SHOP_DISCOUNT_1 : T_SHOP_LINE_1);
                text_line(2, ST_X(-100), ST_Y(-34), TEXT_WHITE, last_rank >= 3 ? T_SHOP_DISCOUNT_2 : T_SHOP_LINE_2);
                text_line(3, ST_X(-100), ST_Y(-18), TEXT_WHITE, T_SHOP_LINE_3);
                for (int i = 0; i < 4; i++)
                    text_line(15 + i, ST_X(tab_x[i]), ST_Y(49), i == shop_tab ? TEXT_GOLD : TEXT_GREY, tabs[i]);
                draw_shop(rows, 0, 0, 0);
            }
            else
            {
                const char *ask = shop_tab == 1 ? T_SHOP_ASK_BUY : shop_tab == 2 ? T_SHOP_ASK_SELL : T_SHOP_ASK_UP;
                if (shop_msg > 0)
                {
                    shop_msg--;
                    ask = shop_text;
                }
                text_line(0, ST_X(150) - text_width(ask), ST_Y(-103), shop_msg > 0 ? TEXT_PINK : TEXT_WHITE, ask);
                for (int i = 0; i < ST_ROWS; i++)
                {
                    int k = shop_top + i;
                    if (k >= n)
                    {
                        text_hide(1 + i * 2);
                        text_hide(2 + i * 2);
                        continue;
                    }
                    text_line(1 + i * 2, ST_X(-106), ST_Y(-54 + 31 * i), TEXT_WHITE, rows[k].name);
                    text_line(2 + i * 2, ST_X(142) - text_width(rows[k].note), ST_Y(-54 + 31 * i), rows[k].kind == ROW_WEAPON && (wep_owned >> rows[k].arg & 1) ? TEXT_GREY : TEXT_GOLD, rows[k].note);
                }
                if (n == 0)
                    text_line(1, ST_X(-106), ST_Y(-54), TEXT_GREY, T_SHOP_EMPTY);
                draw_shop(rows, n, shop_top, shop_sel);
            }
            model_sync();
            glFlush(0);
            if (pending >= 0 && fade + 2 >= 16)
            {
                text_clear();
                if (menu_loaded)
                    sheet_free(&menu_sheet);
                menu_loaded = 0;
            }
            swiWaitForVBlank();
            text_commit();
            vital_idle(T_CAPTION, T_EDITION);
            continue;
        }

        if (mode == MODE_RESULT)
        {
            static char stats[5][24], rank[3];
            static const char *const labels[] = { T_ACCURACY, T_LIFE, T_KILLS, T_HEADSHOTS, T_TREASURE };
            int seconds = (int)(play_frames / 60);
#ifdef TEST_RESULT_SECONDS
            seconds = TEST_RESULT_SECONDS;
#endif
            if (menu_shown != MODE_RESULT)
            {
                text_clear();
                dim_3d(0);
                music_stop();
                result_t = 0;
                menu_shown = MODE_RESULT;
                int kills = ganados.kills;
                int acc = shots ? (int)(hits * 100 / shots) : 0;
                snprintf(stats[0], sizeof(stats[0]), "%d%%", acc);
                snprintf(stats[1], sizeof(stats[1]), "%d", leon_hp);
                snprintf(stats[2], sizeof(stats[2]), "%d", kills);
                snprintf(stats[3], sizeof(stats[3]), "%lu", headshots);
                snprintf(stats[4], sizeof(stats[4]), "%d/%d", treasures_found, stage_info[cur_stage].treasures);
                (void)seconds;
                int life = 100 - (damage_taken * 12 + (ash.on ? (ASHLEY_HP - ash.hp) * 6 : 0)) / 100;
                if (life < 0)
                    life = 0;
                int all = 0, felled = kills;
                if (cur_stage == 0)
                {
                    all = GANADO_COUNT + 10;
                    for (int i = 0; i < ganados.count; i++)
                        if (ganados.e[i].boss && ganados.e[i].state >= ENEMY_DYING)
                            felled += 10 - 1;
                }
                else
                {
                    const StageFoes *sf = &stage_foes[cur_stage];
                    for (int i = 0; i < sf->count; i++)
                    {
                        int w = sf->foe[i].type == FOE_CHAINSAW ? 10 : sf->foe[i].type == FOE_GARRADOR ? 50
                              : sf->foe[i].type == FOE_GIANT || sf->foe[i].type == FOE_VERDUGO ? 100 : 1;
                        all += w * sf->foe[i].entries;
                        if (w == 50 && garradors_down())
                            felled += 50;
                        else if (w == 100 && (sf->foe[i].type == FOE_GIANT ? giants_down() : !ver.on || ver.state == VER_DEAD))
                            felled += 100;
                    }
                }
                int attack = all ? felled * 100 / all : 0;
                if (attack > 100)
                    attack = 100;
                int score = acc + life + attack;
                ptas += MISSION_PTAS;
                last_rank = score > 269 ? 4 : score > 209 ? 3 : score > 179 ? 2 : score > 149 ? 1 : 0;
                static const char *const letters[5] = { "D", "C", "B", "A", "S" };
                snprintf(rank, sizeof(rank), "%s", letters[last_rank]);
            }
            result_t++;
            int step = result_t / RESULT_STEP;
            text_big(0, 128 - text_width(T_COMPLETE), 14, TEXT_GOLD, T_COMPLETE);
            for (int i = 0; i < 5; i++)
                if (step >= 2 + i)
                {
                    int y = 84 + i * 14;
                    text_line(i < 4 ? 1 + i : 9, 48, y, TEXT_WHITE, labels[i]);
                    text_line(TEXT_WIDE_LINES + i, 208 - text_width(stats[i]), y, TEXT_GOLD, stats[i]);
                }
            if (step >= 7)
            {
                text_line(5, 48, 160, TEXT_WHITE, T_TOTAL);
                char p[24];
                snprintf(p, sizeof(p), T_PTAS, ptas);
                text_line(8, 128 - text_width(p) / 2, 160, TEXT_GOLD, p);
                text_big(6, 180, 152, last_rank >= 3 ? TEXT_GOLD : TEXT_WHITE, rank);
            }
            if (result_t == RESULT_STEP * 7)
                sfx_play(SFX_ITEM_GET, 110, 64);
            else if (result_t % RESULT_STEP == 0 && step >= 1 && step <= 6)
                sfx_play(SFX_EMPTY, 90, 64);
            if (step >= 8 && (tick / 30) % 2 == 0)
                text_line(7, 128 - text_width(T_PRESS_A) / 2, 180, TEXT_GREY, T_PRESS_A);
            else
                text_hide(7);
            if (step >= 8 && (down & (KEY_A | KEY_START)))
            {
                sfx_play(SFX_MENU, 100, 64);
                text_clear();
                pending = MODE_SELECT;
                save_write(spare, herbs, ptas);
            }
            draw_result(seconds, step);
            model_sync();
            glFlush(0);
            swiWaitForVBlank();
            text_commit();
            vital_idle(T_CAPTION, T_EDITION);
            continue;
        }

        if (mode == MODE_CUTIN)
        {
            if (menu_shown != MODE_CUTIN)
            {
                text_clear();
                dim_3d(0);
                menu_shown = MODE_CUTIN;
                cut_pic_id = -1;
                lcdMainOnBottom();
            }
            if (pending < 0 && cut_page < cut->count)
            {
                if ((down & KEY_A) || ++cut_t >= CUT_AUTO)
                {
                    cut_page++;
                    cut_t = 0;
                    if (cut_after == CUT_AFTER_GIANT && cut_page == 1)
                        sfx_play(SFX_KNIFE, 127, 64);
                }
                if (down & KEY_START)
                    cut_page = cut->count;
                if (cut_page >= cut->count)
                {
                    if (cut_after == CUT_AFTER_GIANT)
                        gcut_page = 2;
                    if (cut_fade)
                        pending = cut_then;
                    else
                    {
                        mode = cut_then;
                        menu_shown = -1;
                    }
                    down = 0;
                }
            }
            if (cut_page < cut->count && cut_page != cut_pic_id)
            {
                const CutPage *pg = &cut->page[cut_page];
                void *buf = NULL;
                size_t size = 0;
                if (pg->pic >= 0)
                {
                    char name[32];
                    snprintf(name, sizeof(name), "nitro:/eve%02d_%d.bin", cut->stage + 1, pg->pic);
                    buf = read_file(name);
                    size = buf ? file_size(name) : 0;
                }
                vital_cutin(buf, size, pg->who && pg->who < sizeof(cut_who) / sizeof(cut_who[0]) ? cut_who[pg->who] : NULL, pg->text);
                free(buf);
                cut_pic_id = cut_page;
            }
            setBrightness(2, -fade);
            if (mode != MODE_CUTIN || (pending >= 0 && fade + 2 >= 16))
            {
                vital_cutin(NULL, 0, NULL, NULL);
                lcdMainOnTop();
                setBrightness(2, 0);
            }
            overlay_begin();
            overlay_rect(0, 0, 256, 192, RGB15(0, 0, 0), 31, 1);
            overlay_end();
            model_sync();
            glFlush(0);
            swiWaitForVBlank();
            text_commit();
            if (mode == MODE_CUTIN)
                continue;
        }
        if (mode == MODE_PLAY && ctrl == CTRL_LEON && (down & KEY_START))
        {
            mode = MODE_PAUSE;
            menu_sel = 0;
            down = 0;
        }
        if (mode == MODE_PAUSE)
        {
            static const char *const items[] = { T_OPT_RETRY, T_OPT_QUIT, T_OPT_SOUND, T_OPT_HUD, T_OPT_SCREEN, T_OPT_CONTROLS };
            static const u8 item_sub[] = { 1, 2, 3, 5, 6, 4 };
            #define PAUSE_ITEMS 6
            const char *const helps[] = { T_HELP_RETRY, FOES_ON(cur_stage) ? T_HELP_QUIT : T_HELP_LEAVE, T_HELP_SOUND, T_HELP_CONTROLS,
                                          T_HELP_HUD, T_HELP_SCREEN };
            static const char *const vols[] = { T_VOL_OFF, T_VOL_LOW, T_VOL_MID, T_VOL_HIGH };
            static const char *const yes_no[] = { T_YES, T_NO };
            static const char *const on_off[] = { T_HUD_ON, T_HUD_OFF };
            static const char *const views[] = { T_SCR_MAP, T_SCR_ITEMS };
            static const u8 vol_pct[] = { 0, 40, 70, 100 };
            static int sub, sub_sel, sound = 3;
            if (menu_shown != MODE_PAUSE)
            {
                text_clear();
                menu_shown = MODE_PAUSE;
                sub = 0;
            }
            dim_3d(sub == 4 ? 12 : 10);
            music_volume(MUSIC_VOLUME / 3);
            int back = (down & KEY_B) != 0, pick = (down & KEY_A) != 0, shown = sub;
            if (sub == 0)
            {
                menu_sel = menu_move(down, menu_sel, PAUSE_ITEMS);
                if (down & KEY_START || back)
                {
                    mode = MODE_PLAY;
                }
                else if (pick)
                {
                    sfx_play(SFX_MENU, 100, 64);
                    sub = item_sub[menu_sel];
                    sub_sel = sub == 3 ? sound : sub == 5 ? !hud_on : sub == 6 ? bottom_items : 1;
                }
            }
            else if (sub == 5 || sub == 6)
            {
                sub_sel = menu_move(down, sub_sel, 2);
                if (pick)
                {
                    if (sub == 5)
                        hud_on = sub_sel == 0;
                    else
                        bottom_items = sub_sel;
                    sfx_play(SFX_MENU, 100, 64);
                }
                if (pick || back)
                    sub = 0;
            }
            else if (sub == 1 || sub == 2)
            {
                sub_sel = menu_move(down, sub_sel, 2);
                if (back || (pick && sub_sel == 1))
                    sub = 0;
                else if (pick)
                {
                    sfx_play(SFX_MENU, 100, 64);
                    mode = sub != 2 ? MODE_PLAY : FOES_ON(cur_stage) ? MODE_TITLE : MODE_SELECT;
                    if (mode == MODE_SELECT)
                        save_write(spare, herbs, ptas);
                    reset = sub == 1;
                }
            }
            else if (sub == 3)
            {
                sub_sel = menu_move(down, sub_sel, 4);
                if (pick)
                {
                    sound = sub_sel;
                    sfx_level(vol_pct[sound]);
                    music_level(vol_pct[sound]);
                    sfx_play(SFX_MENU, 100, 64);
                }
                if (pick || back)
                    sub = 0;
            }
            else if (back || pick || (down & KEY_START))
                sub = 0;
            if (sub != shown)
                text_clear();
            if (mode != MODE_PAUSE)
            {
                text_clear();
                dim_3d(0);
                menu_shown = -1;
            }
            else if (sub == 4)
            {
                text_line(0, 128 - text_width(T_OPT_CONTROLS) / 2, 10, TEXT_GOLD, T_OPT_CONTROLS);
                for (int i = 0; i < 8; i++)
                {
                    text_line(TEXT_WIDE_LINES + i, 10, 34 + i * 18, TEXT_GOLD, howto[i * 2]);
                    text_line(2 + i, 72, 34 + i * 18, TEXT_WHITE, howto[i * 2 + 1]);
                }
                text_line(1, 246 - text_width(T_BACK), 176, TEXT_GREY, T_BACK);
            }
            else
            {
                text_line(10, 128 - text_width(T_OPTIONS) / 2, 13, TEXT_WHITE, T_OPTIONS);
                if (sub == 1 || sub == 2)
                {
                    const char *q = sub == 1 ? T_ASK_RETRY : FOES_ON(cur_stage) ? T_ASK_QUIT : T_ASK_LEAVE;
                    text_line(4, 128 - text_width(q) / 2, 96, TEXT_WHITE, q);
                    for (int i = 0; i < 2; i++)
                        text_line(5 + i, 128 - text_width(yes_no[i]) / 2, 115 + i * 19, i == sub_sel ? TEXT_GOLD : TEXT_GREY, yes_no[i]);
                }
                else if (sub == 3)
                    for (int i = 0; i < 4; i++)
                        text_line(i, 128 - text_width(vols[i]) / 2, 58 + i * 19, i == sub_sel ? TEXT_GOLD : TEXT_WHITE, vols[i]);
                else if (sub == 5 || sub == 6)
                    for (int i = 0; i < 2; i++)
                    {
                        const char *s = sub == 5 ? on_off[i] : views[i];
                        text_line(i, 128 - text_width(s) / 2, 77 + i * 19, i == sub_sel ? TEXT_GOLD : TEXT_WHITE, s);
                    }
                else
                    for (int i = 0; i < PAUSE_ITEMS; i++)
                        text_line(i, 128 - text_width(items[i]) / 2, 34 + i * 18, i == menu_sel ? TEXT_GOLD : TEXT_WHITE, items[i]);
                const char *h = helps[(sub == 0 ? item_sub[menu_sel] : sub) - 1];
                text_line(9, 128 - text_width(h) / 2, 150, TEXT_PINK, h);
            }
            swiWaitForVBlank();
            text_commit();
            continue;
        }

        if (mode == MODE_MESSAGE)
        {
            static const char *const who_names[] = { "", "Leon", "Ashley", "Luis", "Saddler" };
            if (menu_shown != MODE_MESSAGE)
            {
                text_clear();
                menu_shown = MODE_MESSAGE;
                msg_t = 0;
            }
            dim_3d(7);
            msg_t++;
            if (msg_t > 10 && (down & (KEY_A | KEY_B)))
            {
                msg_page++;
                msg_t = 0;
                text_clear();
                sfx_play(SFX_MENU, 80, 64);
            }
            if (!msg || msg_page >= msg->count)
            {
                text_clear();
                dim_3d(0);
                menu_shown = -1;
                mode = MODE_PLAY;
                down = 0;
            }
            else
            {
                const CutPage *pg = &msg->page[msg_page];
                if (pg->who > 0 && pg->who < 5)
                    text_line(0, 128 - text_width(who_names[pg->who]) / 2, 62, TEXT_GOLD, who_names[pg->who]);
                char line[48];
                const char *s = pg->text;
                for (int k = 0; k < 3 && *s; k++)
                {
                    int n = 0;
                    while (s[n] && s[n] != '\n' && n < (int)sizeof(line) - 1)
                        n++;
                    memcpy(line, s, n);
                    line[n] = 0;
                    text_line(1 + k, 128 - text_width(line) / 2, 82 + k * 16, TEXT_WHITE, line);
                    s += n + (s[n] == '\n');
                }
            }
            swiWaitForVBlank();
            text_commit();
            continue;
        }

        if (mode == MODE_STATUS)
        {
            StItem list[24];
            int n = 0, used = 0;
            for (int w = 0; w < WEP_COUNT; w++)
                if (wep_owned >> w & 1)
                    list[n++] = (StItem){ ST_GUN, w == wep ? ammo : wep_ammo[w], w };
            for (int s = spare; s > 0 && n < 18; s -= MULTI_BOX)
                list[n++] = (StItem){ ST_AMMO, s < MULTI_BOX ? s : MULTI_BOX, multi_ammo_gun(wep) };
            for (int h = 0; h < herbs && n < 24; h++)
                list[n++] = (StItem){ ST_HERB, 1, 0 };
            used = case_used(spare, herbs);
            if (menu_shown != MODE_STATUS)
            {
                text_clear();
                menu_shown = MODE_STATUS;
                if (!menu_loaded)
                    menu_loaded = sheet_load(&menu_sheet, menu_bin, menu_bin_size) == 0;
                if (!menu_loaded && jya_loaded)
                {
                    jya_unload();
                    st_jya_out = 1;
                    menu_loaded = sheet_load(&menu_sheet, menu_bin, menu_bin_size) == 0;
                }
                glClearColor(0, 0, 0, 31);
                set_fov(ST_FOV);
            }
            if (st_sel >= n)
                st_sel = n - 1;
            enum { CMD_EQUIP, CMD_USE, CMD_DISCARD, CMD_CANCEL };
            static const u8 gun_cmds[] = { CMD_EQUIP, CMD_CANCEL }, ammo_cmds[] = { CMD_DISCARD, CMD_CANCEL },
                            herb_cmds[] = { CMD_USE, CMD_DISCARD, CMD_CANCEL };
            static const char *const cmd_text[] = { T_ST_EQUIP, T_ST_USE, T_ST_DISCARD, T_ST_CANCEL };
            const StItem *cur = &list[st_sel];
            const u8 *cmds = cur->type == ST_GUN ? gun_cmds : cur->type == ST_AMMO ? ammo_cmds : herb_cmds;
            int ncmd = cur->type == ST_HERB ? 3 : 2;
            if (st_cmd < 0)
            {
                if (down & (KEY_B | KEY_SELECT | KEY_START))
                {
                    mode = MODE_PLAY;
                    text_clear();
                    menu_shown = -1;
                    if (menu_loaded)
                        sheet_free(&menu_sheet);
                    menu_loaded = 0;
                    if (st_jya_out)
                    {
                        st_jya_out = 0;
                        if (jya_load() == 0)
                        {
                            enemies_set_look(&ganados, &jya_model, &jya_anims, &jya_w0);
                            jya_arm();
                        }
                        else
                            enemies_set_look(&ganados, &ganado, &ganado_anims, &ganado_w0);
                    }
                    set_haze(stage_info[cur_stage].haze);
                    stage_fog(stage_info[cur_stage].fog);
                    set_fov(CAM_FOV);
                    down = 0;
                }
                else
                {
                    int was = st_sel;
                    st_sel = menu_move(down, st_sel, n);
                    if (st_sel != was)
                        sfx_play(SFX_MENU, 60, 64);
                    if (down & KEY_A)
                    {
                        st_cmd = 0;
                        sfx_play(SFX_MENU, 100, 64);
                    }
                }
            }
            else
            {
                st_cmd = menu_move(down, st_cmd, ncmd);
                if (down & KEY_B)
                    st_cmd = -1;
                else if (down & KEY_A)
                {
                    int c = cmds[st_cmd];
                    if (c == CMD_EQUIP && cur->weapon != wep)
                    {
                        weapon_switch(cur->weapon, &ammo, &spare);
                        fire_frame = reload_frame = -1;
                        clip_key = -1;
                    }
                    if (c == CMD_USE && leon_hp >= LEON_HP)
                        st_msg = 90;
                    else if (c == CMD_USE)
                    {
                        herbs--;
                        leon_hp = leon_hp + HERB_HEAL > LEON_HP ? LEON_HP : leon_hp + HERB_HEAL;
                        sfx_play(SFX_ITEM_GET, 110, 64);
                    }
                    else if (c == CMD_DISCARD)
                    {
                        if (cur->type == ST_AMMO)
                            spare -= cur->count;
                        else
                            herbs--;
                    }
                    if (c != CMD_USE)
                        sfx_play(SFX_MENU, 100, 64);
                    st_cmd = -1;
                }
            }
            if (st_sel < st_top)
                st_top = st_sel;
            if (st_sel >= st_top + ST_ROWS)
                st_top = st_sel - ST_ROWS + 1;
            if (mode == MODE_STATUS)
            {
                static NodeLocal st_local[MODEL_MAX_NODES];
                static int st_pose[MODEL_MAX_NODES][12];
                anim_sample(&leon_gun, GUN_READY_POSE, 0, &leon, st_local);
                anim_compose(&leon, st_local, st_pose);
                int h = leon.bbox_max[1] - leon.bbox_min[1];
                int ex = h * 3 / 4, ey = h / 2, ez = h * 5 / 4;
                overlay_begin();
                overlay_far();
                if (menu_loaded)
                    for (int i = 0; i < 6; i++)
                        menu_piece(80, 32, 156, 32, 48, -85 + 32 * i, 112, 32);
                else
                    overlay_rect(ST_X(48), ST_Y(-85), 256, ST_Y(80), RGB15(3, 3, 4), 31, 25);
                overlay_end();
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
                gluLookAtf32(-ex, ey, ez, -ex, ey, 0, 0, inttof32(1), 0);
                glColor(RGB15(31, 31, 31));
                int p[3] = { 0, -leon.bbox_min[1], 0 }, py = -degreesToAngle(20);
                model_draw(&leon, p, py, POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(2), (const int (*)[12])st_pose);
                model_draw_held(&gun, p, py, POLY_ALPHA(31) | POLY_CULL_BACK | POLY_ID(3), st_pose[HAND_NODE]);
                draw_status(list, n, st_top, st_sel, st_cmd >= 0 ? ncmd : 0);
                overlay_begin();
                draw_plate(leon_hp, ammo, ST_PLATE_DY);
                overlay_end();

                if (st_msg > 0)
                {
                    st_msg--;
                    text_line(0, ST_X(-140), ST_Y(-103), TEXT_PINK, T_FULL);
                }
                else
                    text_line(0, ST_X(-140), ST_Y(-103), TEXT_WHITE, T_WEAPONS);
                static const char *const names[] = { T_ITEM_GUN, T_ITEM_AMMO, T_ITEM_HERB };
                for (int r = 0; r < ST_ROWS; r++)
                {
                    int k = st_top + r, line = 1 + r * 3;
                    if (k >= n)
                    {
                        for (int j = 0; j < 3; j++)
                            text_hide(line + j);
                        continue;
                    }
                    char num[8];
                    text_line(line, ST_X(-106), ST_Y(-54 + 31 * r), TEXT_WHITE, list[k].type == ST_GUN ? weapons[list[k].weapon].name
                              : names[list[k].type]);
                    snprintf(num, sizeof(num), "%d", list[k].count);
                    if (list[k].type == ST_HERB)
                        text_hide(line + 1);
                    else
                        text_line(line + 1, ST_X(4) - text_width(num), ST_Y(-54 + 31 * r), TEXT_WHITE, num);
                    snprintf(num, sizeof(num), "%d", list[k].type == ST_GUN ? weapons[list[k].weapon].tam : st_tam[list[k].type]);
                    text_line(line + 2, ST_X(34) - text_width(num), ST_Y(-54 + 31 * r), TEXT_GREY, num);
                }
                char m[24], lead[16];
                snprintf(lead, sizeof(lead), "%d ", ptas);
                snprintf(m, sizeof(m), "%d PTAS", ptas);
                text_line(13, ST_X(-84) - text_width(lead), ST_Y(89), TEXT_WHITE, m);
                snprintf(m, sizeof(m), "%03d/%03d", used, case_tam[case_level]);
                text_line(14, ST_X(91), ST_Y(89), TEXT_WHITE, m);
                for (int i = 0; i < 3; i++)
                    if (st_cmd >= 0 && i < ncmd)
                        text_line(15 + i, ST_X(65), ST_Y(-33 + 16 * i), i == st_cmd ? TEXT_GOLD : TEXT_GREY, cmd_text[cmds[i]]);
                    else
                        text_hide(15 + i);
                model_sync();
                glFlush(0);
            }
            swiWaitForVBlank();
            text_commit();
            continue;
        }

        if (reset)
            music_play(MUSIC_VILLAGE, MUSIC_VOLUME);
        else if (clear_t >= BELL_FIRST)
        {
            int v = MUSIC_VOLUME - (clear_t - BELL_FIRST) * MUSIC_VOLUME / 90;
            if (v > 0)
                music_volume(v);
            else
                music_stop();
        }
        else if (react != REACT_DEAD)
            music_play(MUSIC_VILLAGE, MUSIC_VOLUME);
        else
            music_stop();

        if (mode == MODE_GAMEOVER)
        {
            if (menu_shown != MODE_GAMEOVER)
            {
                text_clear();
                menu_sel = 0;
                menu_shown = MODE_GAMEOVER;
            }
            if (dead_timer < DEAD_TO_MENU + MENU_DELAY)
            {
                dead_timer++;
                down = 0;
            }
            menu_sel = menu_move(down, menu_sel, mode == MODE_TITLE ? 2 + have_save : 2);
            const char *dead = ash.on && ash.state == ASH_DOWN ? T_ASH_DEAD : T_DEAD;
            text_line(4, 128 - text_width(dead) / 2, 64, TEXT_RED, dead);
            show_menu(0, 128, 100, over_items, 2, menu_sel);
            if (down & (KEY_A | KEY_START))
            {
                sfx_play(SFX_MENU, 100, 64);
                text_clear();
                menu_shown = -1;
                if (menu_sel == 0)
                {
                    mode = MODE_PLAY;
                    reset = 1;
                }
                else
                {
                    mode = MODE_TITLE;
                    continue;
                }
            }
            if (mode == MODE_GAMEOVER)
                held = down = 0;
        }

        if (down & KEY_SELECT)
        {
            if (held & KEY_Y)
                cull = (cull + 1) % 3;
            else if (ctrl == CTRL_LEON && !(held & KEY_L))
            {
                if (mode == MODE_PLAY && react == REACT_NONE)
                {
                    mode = MODE_STATUS;
                    st_sel = st_top = 0;
                    st_cmd = -1;
                }
            }
            else if (ctrl == CTRL_LEON)
            {
                ctrl = CTRL_FREECAM;
                for (int k = 0; k < 3; k++)
                    eye[k] = follow_eye[k];
                yaw = DEGREES_IN_CIRCLE / 2 - leon_yaw;
                pitch = -degreesToAngle(8);
                leon_anim = ANIM_IDLE;
                gun_state = GUN_HOLSTERED;
            }
            else
            {
                ctrl = CTRL_LEON;
                anim_frame = 0;
            }
        }

        if (reset)
        {
            reset = 0;
            ctrl = CTRL_LEON;
            for (int k = 0; k < 3; k++)
                leon_pos[k] = start_pos[k];
            leon_feet = start_feet;
            leon_yaw = start_yaw;
            leon_anim = ANIM_IDLE;
            anim_frame = 0;
            gun_state = GUN_HOLSTERED;
            aim_w = aim_tilt = 0;
            fire_frame = reload_frame = -1;
            flash_timer = impact_timer = cam_kick = 0;
            if (cut_opening || !mis_kept)
            {
                mis_rounds = spare;
                mis_herbs = herbs;
                mis_ptas = ptas;
                mis_kept = 1;
            }
            else
            {
                spare = mis_rounds;
                herbs = mis_herbs;
                ptas = mis_ptas;
            }
            for (int w = 0; w < WEP_COUNT; w++)
                wep_ammo[w] = wep_mag(w);
            ammo = MAG_SIZE;
#ifdef TEST_ALL_WEAPONS
            wep_owned = (1 << WEP_COUNT) - 1;
            spare += 100;
#endif
            leon_hp = LEON_HP;
#ifdef TEST_HURT
            herbs = 3;
            leon_hp = 30;
#endif
            react = REACT_NONE;
            holder = -1;
            safe_timer = hurt_flash = dead_timer = popup = damage_taken = 0;
            hits = shots = headshots = 0;
            clear_t = -1;
            play_frames = 0;
            drops_clear(&drops);
            frames = 0;
            fps = 60;
            fps_mark = vblank_count;
#ifdef TEST_DROPS
            for (int k = 0; k < 2; k++)
            {
                int p[3] = { leon_pos[0] + inttof32(k ? 9 : -6), leon_feet, leon_pos[2] + inttof32(14) };
                ground_height(&stage_ground, p[0], p[2], leon_feet, STEP_UP, &p[1]);
                drops_spawn(&drops, p, k ? DROP_HERB : DROP_AMMO, 0);
            }
#endif
            objects_reset(&objs);
            sheet_free(&picture);
            picture_id = -1;
            cha_unload();
            stage_budget = -1;
            enemy_show = ENEMY_MAX;
            time_cap = 4000;
            enemy_cap = ENEMY_MAX;
            if (load_stage(cur_stage))
            {
                consoleDemoInit();
                printf("stage %d: load failed\n", cur_stage + 1);
                while (1)
                    swiWaitForVBlank();
            }
            set_haze(stage_info[cur_stage].haze);
            stage_fog(stage_info[cur_stage].fog);
            cam_setup(cur_stage);
            if (cur_stage == 0)
            {
                objects_init(&objs, &stage_ground, &door_model, &ladder_model, &barrel_model, &crate_model, &medal_model, &boom_sheet, &spark);
                objs.bell = &bell_model;
            }
            else
                objects_clear(&objs);
            treasures_found = 0;
            {
                const StageInfo *si = &stage_info[cur_stage];
                for (int i = 0; i < si->treasures; i++)
                {
                    int p[3] = { inttof32(si->treasure[i][0]), inttof32(si->treasure[i][1]), inttof32(si->treasure[i][2]) };
                    int land[3] = { p[0], p[1], p[2] };
                    ground_height(&stage_ground, p[0], p[2], p[1] - inttof32(8), 0, &land[1]);
                    ground_collide(&stage_ground, &land[0], &land[2], land[1] + STEP_UP, land[1] + LEON_HEIGHT, inttof32(6));
                    ground_height(&stage_ground, land[0], land[2], land[1] + STEP_UP, STEP_UP, &land[1]);
                    if (cur_stage == 3 && i == 1)
                        land[0] = inttof32(314);
                    const EvItem *ti = ev_item(stage_treasure_item[cur_stage][i < 2 ? i : 1]);
                    drops_place_treasure(&drops, p, land, ti && ti->price ? ti->price : TREASURE_PTAS);
                }
            }
            boss_phase = boss_t = 0;
            obj_t = OBJ_FRAMES;
            elevator_note = 0;
            if (cur_stage == 0)
            {
                enemies_init(&ganados, &ganado, &ganado_anims, &stage_ground, &objs, ganado_spawn, GANADO_COUNT, DEGREES_IN_CIRCLE / 2);
                enemies_set_look(&ganados, &ganado, &ganado_anims, &ganado_w0);
                stage1_kinds();
#ifdef TEST_SIGHT
                ganados.sight = inttof32(TEST_SIGHT);
#endif
            }
            else
            {
                const StageFoes *sf = &stage_foes[cur_stage];
                foe_n = 0;
                int throwers = 0, tpts[4][3];
                for (int i = 0; FOES_ON(cur_stage) && i < sf->count && foe_n < FOES_MAX; i++)
                    if (sf->foe[i].type == FOE_GANADO || sf->foe[i].type == FOE_CULTIST)
                    {
                        foe_fixed[foe_n] = -1;
                        if (sf->foe[i].type == FOE_CULTIST && sf->foe[i].pos[1] >= WALL_TOP)
                        {
                            if (throwers == 4)
                                continue;
                            for (int k = 0; k < 3; k++)
                                tpts[throwers][k] = sf->foe[i].pos[k];
                            foe_fixed[foe_n] = throwers++;
                        }
                        foe_left[foe_n] = sf->foe[i].entries;
                        foe_slot[foe_n] = -1;
                        foe_list[foe_n++] = &sf->foe[i];
                    }
                foe_slots = FOE_UNITS;
                foe_entered = 0;
                foe_held = 0;
                int pts[ENEMY_MAX][3], n = 0;
                for (; n < foe_slots; n++)
                    for (int k = 0; k < 3; k++)
                        pts[n][k] = sf->start[k];
                int first_thrower = n;
                for (int i = 0; i < throwers; i++, n++)
                    for (int k = 0; k < 3; k++)
                        pts[n][k] = tpts[i][k];
                for (int f = 0; f < foe_n; f++)
                    if (foe_fixed[f] >= 0)
                        foe_fixed[f] += first_thrower;
                for (int s = 0; s < ENEMY_MAX; s++)
                    slot_foe[s] = -1;
                enemies_init(&ganados, &ganado, &ganado_anims, &stage_ground, &objs, (const int (*)[3])pts, n, DEGREES_IN_CIRCLE / 2);
                enemies_set_look(&ganados, &ganado, &ganado_anims, &ganado_w0);
                ganados.sight = ENEMY_SIGHT;
                ganados.watch = 1;
                for (int f = 0; f < foe_n; f++)
                    if (foe_fixed[f] >= 0)
                    {
                        int i = foe_fixed[f];
                        enemies_make_thrower(&ganados, i);
                        enemies_set_kind(&ganados, i, foe_kind_of(foe_list[f]));
                        enemies_perch(&ganados, i, pts[i][1]);
                        ganados.e[i].yaw = foe_list[f]->dir * (DEGREES_IN_CIRCLE / 256);
                        ganados.e[i].drop = foe_list[f]->drop;
                    }
                foes_events_start(sf, cur_stage);
                {
                    int with_plaga = 0;
                    for (int f = 0; f < foe_n; f++)
                        with_plaga |= foe_kind_of(foe_list[f])->plaga;
                    if (with_plaga)
                        plaga_load();
                    else
                        plaga_unload();
                }

#ifdef TEST_THROWER_AUDIT
                {
                    consoleDemoInit();
                    for (int i = 0; i < ganados.count; i++)
                    {
                        Enemy *e = &ganados.e[i];
                        if (!e->thrower)
                            continue;
                        e->active = 1;
                        int ok = 0, steep = 0, walled = 0, missed = 0, best_d = 0;
                        for (int gx = -50; gx <= 50; gx++)
                            for (int gz = -50; gz <= 50; gz++)
                            {
                                int x = e->pos[0] + gx * inttof32(4), z = e->pos[2] + gz * inttof32(4), y = 0;
                                if (!ground_height(&stage_ground, x, z, inttof32(60), 0, &y) || y > inttof32(60))
                                    continue;
                                int eye[3] = { x, y + inttof32(14), z }, to[3] = { e->pos[0], e->feet + inttof32(10), e->pos[2] };
                                int d[3] = { to[0] - eye[0], to[1] - eye[1], to[2] - eye[2] };
                                int len = (int)sqrt64((u64)((s64)d[0] * d[0] + (s64)d[1] * d[1] + (s64)d[2] * d[2]));
                                if (len <= 0) continue;
                                int hl = (int)sqrt64((u64)((s64)d[0] * d[0] + (s64)d[2] * d[2]));
                                if (d[1] > mulf32(hl, floattof32(1.19)))
                                { steep++; continue; }
                                for (int k = 0; k < 3; k++) d[k] = divf32(d[k], len);
                                int hit[3], t_best = len + inttof32(5), part;
                                if (ground_raycast(&stage_ground, eye, d, len, hit, RAY_SHOT))
                                {
                                    s64 hx = hit[0] - eye[0], hy = hit[1] - eye[1], hz = hit[2] - eye[2];
                                    t_best = (int)sqrt64((u64)(hx * hx + hy * hy + hz * hz)) + THROWER_REACH;
                                    if (t_best < len) { walled++; continue; }
                                }
                                if (enemies_raycast(&ganados, eye, d, &t_best, &part) != i)
                                { missed++; continue; }
                                ok++;
                                if (!best_d || hl < best_d) best_d = hl;
                            }
                        printf("t%d %d,%d,%d ok%d steep%d wall%d miss%d near%d\n", i, f32toint(e->pos[0]), f32toint(e->feet), f32toint(e->pos[2]),
                               ok, steep, walled, missed, f32toint(best_d));
                    }
                    while (1) swiWaitForVBlank();
                }
#endif
                if (cur_stage >= CASTLE_FROM && foe_n + throwers > 0)
                {
                    if (jya_load())
                    {
                        consoleDemoInit();
                        printf("cultists: load failed\n");
                        while (1)
                            swiWaitForVBlank();
                    }
                    enemies_set_look(&ganados, &jya_model, &jya_anims, &jya_w0);
                    jya_arm();
                }
                else
                    jya_unload();
            }
            if (cur_stage == 0)
                for (next_ganado = 0; next_ganado < GANADO_ACTIVE && next_ganado < GANADO_FROM_DOOR; next_ganado++)
                    enemies_activate(&ganados, next_ganado, NULL);
            else
                {
                const StageFoes *st = &stage_foes[cur_stage];
                int at[3] = { inttof32(st->start[0]), inttof32(st->start[1]), inttof32(st->start[2]) };
                foes_step(at);
#ifdef TEST_FOE_DUMP
                consoleDemoInit();
                printf("start %d %d %d slots %d n %d\n", f32toint(at[0]), f32toint(at[1]), f32toint(at[2]), foe_slots, foe_n);
                printf("groups %x entered %d events %d\n", foe_groups, foe_entered, stage_foes[cur_stage].events);
                {
                    int nb[3] = { 0, 0, 0 };
                    foe_flood(at);
                    int okp = foe_near(0, nb);
                    printf("near %d: %d %d %d cells %d y0 %d moved %d\n", okp, nb[0], nb[1], nb[2], foe_cells, foe_y0, foe_moves);
                }
                for (int f = 0; f < foe_n; f++)
                    printf("r%02d g%d a%d %d %2d%s", foe_list[f]->rec, foe_list[f]->wave, foe_list[f]->ai, foe_left[f], foe_slot[f], f % 3 == 2 ? "\n" : " ");
                printf("\n");
                for (int i = 0; i < ganados.count && i < 9; i++)
                    printf("%2d a%d %4d %4d %4d s%d\n", i, ganados.e[i].active, f32toint(ganados.e[i].pos[0]), f32toint(ganados.e[i].feet), f32toint(ganados.e[i].pos[2]), ganados.e[i].spotted);
                while (1)
                    swiWaitForVBlank();
#endif
            }
#if !defined(AUTOPILOT) || defined(STAGE_START)
            if (cur_stage == 0)
            {
                leon_pos[0] = inttof32(240);
                leon_pos[2] = inttof32(500);
                leon_yaw = degreesToAngle(180);
            }
            else
            {
                const StageFoes *sf = &stage_foes[cur_stage];
                leon_pos[0] = inttof32(sf->start[0]);
                leon_pos[2] = inttof32(sf->start[2]);
                leon_yaw = sf->start_dir * (DEGREES_IN_CIRCLE / 256);
            }
#if defined(LEON_START_X)
            leon_pos[0] = inttof32(LEON_START_X);
            leon_pos[2] = inttof32(LEON_START_Z);
#endif
#if defined(LEON_START_YAW)
            leon_yaw = degreesToAngle(LEON_START_YAW);
#endif
            int ref = inttof32(10000);
            if (cur_stage != 0)
                ref = inttof32(stage_foes[cur_stage].start[1] + 4);
#if defined(LEON_START_Y)
            ref = inttof32(LEON_START_Y);
#endif
            if (!ground_height(&stage_ground, leon_pos[0], leon_pos[2], ref, 0, &leon_feet))
                ground_height(&stage_ground, leon_pos[0], leon_pos[2], inttof32(10000), 0, &leon_feet);
            leon_pos[1] = leon_feet - leon.bbox_min[1];
#endif
            for (int i = 0; i < GIANT_MAX; i++)
                giants[i].on = 0;
            giant_count = stage_bosses(cur_stage, FOE_GIANT);
            if (giant_count > GIANT_MAX)
                giant_count = GIANT_MAX;
            if (giant_count && giant_load(&giants[0]))
            {
                consoleDemoInit();
                printf("El Gigante: load failed\n");
                while (1)
                    swiWaitForVBlank();
            }
            for (int i = 0; i < giant_count; i++)
            {
                if (i)
                    giant_share(&giants[i], &giants[0]);
#ifdef GIANT_AHEAD_TEST
                int a = leon_yaw + i * degreesToAngle(GIANT2_DEG), r = GIANT_AHEAD_U + i * GIANT2_FARTHER;
                giant_place(&giants[i], &stage_ground, leon_pos[0] + mulf32(sinLerp(a), r),
                            leon_pos[2] + mulf32(cosLerp(a), r), leon_feet + inttof32(6), a + DEGREES_IN_CIRCLE / 2);
#else
                const StageFoe *f = stage_boss(cur_stage, FOE_GIANT, i);
                giant_place(&giants[i], &stage_ground, inttof32(f->pos[0]), inttof32(f->pos[2]),
                            inttof32(f->pos[1] + 6), f->dir * (DEGREES_IN_CIRCLE / 256));
#endif
                giants[i].on = 1;
            }
            if (!giant_count)
                for (int i = GIANT_MAX - 1; i >= 0; i--)
                    giant_unload(&giants[i]);
            for (int i = 0; i < GAL_MAX; i++)
                gals[i].on = 0;
            gal_count = stage_bosses(cur_stage, FOE_GARRADOR);
            if (gal_count > GAL_MAX)
                gal_count = GAL_MAX;
            if (gal_count && garrador_load(&gals[0]))
            {
                consoleDemoInit();
                printf("Garrador: load failed\n");
                while (1)
                    swiWaitForVBlank();
            }
            for (int i = 0; i < gal_count; i++)
            {
                if (i)
                    garrador_share(&gals[i], &gals[0]);
#ifdef GAL_AHEAD
                garrador_place(&gals[i], &stage_ground, leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(GAL_AHEAD + i * 20)),
                               leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(GAL_AHEAD + i * 20)), leon_feet + inttof32(6),
#ifdef TEST_GAL_BACK
                               leon_yaw);
#else
                               leon_yaw + DEGREES_IN_CIRCLE / 2);
#endif
#else
                const StageFoe *f = stage_boss(cur_stage, FOE_GARRADOR, i);
                garrador_place(&gals[i], &stage_ground, inttof32(f->pos[0]), inttof32(f->pos[2]),
                               inttof32(f->pos[1] + 4), f->dir * (DEGREES_IN_CIRCLE / 256));
#endif
                gals[i].on = 1;
            }
            if (!gal_count)
                for (int i = GAL_MAX - 1; i >= 0; i--)
                    garrador_unload(&gals[i]);
            ver.on = 0;
            if (cur_stage == VER_STAGE && FOES_ON(cur_stage))
            {
                if (verdugo_load(&ver) || (!barrel_loaded && model_load(&barrel_model, barrel_bin, barrel_bin_size)))
                {
                    consoleDemoInit();
                    printf("Verdugo: load failed\n");
                    while (1)
                        swiWaitForVBlank();
                }
                barrel_loaded = 1;
#ifdef VER_AHEAD_TEST
                verdugo_place(&ver, &stage_ground, leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(VER_AHEAD)),
                              leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(VER_AHEAD)), leon_feet + inttof32(6),
                              leon_yaw + DEGREES_IN_CIRCLE / 2);
#else
                const StageFoe *f = stage_boss(cur_stage, FOE_VERDUGO, 0);
                verdugo_place(&ver, &stage_ground, inttof32(f->pos[0]), inttof32(f->pos[2]), inttof32(f->pos[1] + 6),
                              f->dir * (DEGREES_IN_CIRCLE / 256));
#endif
                ver.on = 1;
                objects_clear(&objs);
                objs.cold = 1;
                if (gimmick_model(&tank_model, &tank_loaded, 1, tank_bin, tank_bin_size))
                    objs.barrel = &tank_model;
                const StageInfo *si = &stage_info[cur_stage];
                for (int i = 0; i < si->tanks; i++)
                    objects_add_barrel(&objs, inttof32(si->tank[i][0]), inttof32(si->tank[i][2]), inttof32(si->tank[i][1] + 12));
            }
            else
            {
                verdugo_unload(&ver);
                gimmick_model(&tank_model, &tank_loaded, 0, NULL, 0);
                objs.barrel = &barrel_model;
            }
            if (cur_stage == 0)
                gimmicks_free();
            else
            {
                const StageFoes *sf = &stage_foes[cur_stage];
                int barrels = 0, doors = 0, bells = 0, hangs = 0;
                for (int i = 0; i < sf->things; i++)
                {
                    barrels += sf->thing[i].kind == 3;
                    doors += sf->thing[i].kind == 1;
                    bells += sf->thing[i].kind == 9;
                    hangs += sf->thing[i].kind == 5;
                }
                doors = gimmick_model(&door_model, &door_loaded, doors, door_bin, door_bin_size);
                bells = gimmick_model(&bell_model, &bell_loaded, bells, bell_bin, bell_bin_size);
                hangs = gimmick_model(&hang_model, &hang_loaded, hangs, hang_bin, hang_bin_size);
                objs.hang = &hang_model;
                if (barrels && !barrel_loaded)
                    barrel_loaded = model_load(&barrel_model, barrel_bin, barrel_bin_size) == 0;
                for (int i = 0; i < sf->things; i++)
                {
                    const StageThing *t = &sf->thing[i];
                    int x = inttof32(t->pos[0]), z = inttof32(t->pos[2]), ref = inttof32(t->pos[1] + 6);
                    if (i < (int)sizeof(thing_obj))
                        thing_obj[i] = -1;
                    if (t->kind == 4 && objects_add_crate(&objs, x, z, ref, t->drop) && i < (int)sizeof(thing_obj))
                        thing_obj[i] = objs.count - 1;
                    else if (t->kind == 2)
                        objects_add_ladder(&objs, x, z, ref, t->dir * (DEGREES_IN_CIRCLE / 256));
                    else if (t->kind == 1 && doors)
                        objects_add_door(&objs, x, z, ref, t->dir * (DEGREES_IN_CIRCLE / 256));
                    else if (t->kind == 5 && hangs)
                        objects_add_hang(&objs, x, inttof32(t->pos[1]), z, t->dir * (DEGREES_IN_CIRCLE / 256));
                    else if (t->kind == 9 && bells)
                        objects_add_bell(&objs, x, inttof32(t->pos[1]), z);
                    else if (t->kind == 3 && barrel_loaded && t->pos[1] > 20)
                        objects_add_barrel_at(&objs, x, inttof32(t->pos[1]), z);
                    else if (t->kind == 3 && barrel_loaded)
                        objects_add_barrel(&objs, x, z, ref);
                }
                if (cur_stage == 4 && objects_add_ladder(&objs, inttof32(14), inttof32(44), inttof32(6), 0))
                    objs.o[objs.count - 1].drawn = 1;
                if (!barrels && cur_stage != VER_STAGE)
                    barrel_unload();
            }
#ifdef TEST_HIGH_CRATE
            if (objects_add_barrel_at(&objs, leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(9)), leon_feet + inttof32(13),
                                      leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(9))))
            {
                objs.o[objs.count - 1].kind = OBJ_CRATE;
                objs.o[objs.count - 1].drop = -1;
            }
#endif
#ifdef TEST_AUDIT
            {
                int at[3] = { leon_pos[0], leon_feet, leon_pos[2] };
                audit_stage(cur_stage, at);
                while (1)
                    swiWaitForVBlank();
            }
#endif
            ash_lost = 0;
            ash.on = 0;
            missiles_init(&missiles, &spark, cur_stage >= CASTLE_FROM);
            bolts_clear();
            bonus_t[0] = bonus_t[1] = 0;
            if (cur_stage == ASH_STAGE_A || cur_stage == ASH_STAGE_B || cur_stage == ASH_STAGE_C || cur_stage == ASH_STAGE_D
                || cur_stage == ASH_STAGE_E)
            {
                if (ashley_load(&ash))
                {
                    consoleDemoInit();
                    printf("Ashley: load failed\n");
                    while (1)
                        swiWaitForVBlank();
                }
#ifdef TEST_ASH_VIEW
                ashley_place(&ash, &stage_ground, leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(9)),
                             leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(9)), leon_feet + inttof32(4), leon_yaw + DEGREES_IN_CIRCLE / 2);
#else
                ashley_place(&ash, &stage_ground, leon_pos[0] - mulf32(sinLerp(leon_yaw), inttof32(5)) + mulf32(cosLerp(leon_yaw), inttof32(7)),
                             leon_pos[2] - mulf32(cosLerp(leon_yaw), inttof32(5)) - mulf32(sinLerp(leon_yaw), inttof32(7)), leon_feet + inttof32(4), leon_yaw);
#endif
#ifndef TEST_ASH_VIEW
                const StageFoes *as = &stage_foes[cur_stage];
#ifdef TEST_ASH_WITH
                if (0)
#else
                if (as->ally)
#endif
                {
                    ashley_place(&ash, &stage_ground, inttof32(as->ally_pos[0]), inttof32(as->ally_pos[2]), inttof32(as->ally_pos[1] + 2), leon_yaw);
                    ash.wait = 1;
                }
#endif
                ash.on = 1;
#ifdef TEST_ASH_HP
                ash.hp = TEST_ASH_HP;
#endif
                ganados.ally = ash.pos;
            }
            else
                ashley_unload(&ash);
            int s = sinLerp(leon_yaw), c = cosLerp(leon_yaw);
            follow_eye[0] = leon_pos[0] - mulf32(s, CAM_BACK) - mulf32(c, CAM_RIGHT);
            follow_eye[1] = leon_feet + CAM_HEIGHT;
            follow_eye[2] = leon_pos[2] - mulf32(c, CAM_BACK) + mulf32(s, CAM_RIGHT);
            follow_target[0] = leon_pos[0] + mulf32(s, CAM_AHEAD) - mulf32(c, CAM_AT_RIGHT);
            follow_target[1] = leon_feet + CAM_AT_HEIGHT;
            follow_target[2] = leon_pos[2] + mulf32(c, CAM_AHEAD) + mulf32(s, CAM_AT_RIGHT);
            cam_yaw = leon_yaw;
            kneel_seen = gal_seen = giant_paid = 0;
            ash_waiting = ash.on && ash.wait;
#ifdef TEST_OPENING
            cut_opening = 1;
#endif
            if (cut_opening && cut_start(cur_stage, cur_stage == VER_STAGE ? CUT_VERDUGO : CUT_START, MODE_PLAY, CUT_AFTER_NONE, 1))
                mode = MODE_CUTIN;
            cut_opening = 0;
        }

        if (react != REACT_NONE)
        {
            react_frame += ANIM_STEP;
            int len = inttof32(leon_anims.frames[react_clip[react]]);
            if (react == REACT_BREAK_FREE && react_frame >= KICK_FRAME && react_frame - ANIM_STEP < KICK_FRAME)
            {
                sfx_play(SFX_BREAK_FREE, 120, 64);
            }
            if (react == REACT_HELD)
            {
                held_frames++;
                if (down & (KEY_A | KEY_B | KEY_X | KEY_Y))
                    mash++;
#ifdef TEST_MASH
                if (held_frames % 6 == 0)
                    mash++;
#endif
                if (mash >= GRAB_MASH || ganados.e[holder].state != ENEMY_GRAB)
                {
                    enemies_release(&ganados, holder);
                    holder = -1;
                    react = REACT_BREAK_FREE;
                    react_frame = 0;
                }
            }
            else if (react == REACT_DEAD)
            {
                if (react_frame > len - 1)
                    react_frame = len - 1;
                if (mode == MODE_PLAY && ++dead_timer >= DEAD_TO_MENU)
                    mode = MODE_GAMEOVER;
            }
            else if (react == REACT_KICK)
            {
                if (react_frame >= KICK_HIT && react_frame - ANIM_STEP < KICK_HIT && act_obj < 0)
                {
                    if (enemies_kick(&ganados, leon_pos, leon_yaw, KICK_DAMAGE))
                        sfx_play(SFX_BREAK_FREE, 127, 64);
                    act_obj = -2;
                }
                if (react_frame >= KICK_HIT && react_frame - ANIM_STEP < KICK_HIT && act_obj >= 0)
                {
                    if (objs.o[act_obj].kind == OBJ_GATE_LEAF)
                    {
                        objects_open(&objs, act_obj);
                        sfx_play(SFX_DOOR_KICK, 127, 64);
                    }
                    else if (objects_hit(&objs, act_obj))
                    {
                        sfx_play(SFX_BREAK, 120, 64);
                        crate_drop(&objs.o[act_obj], (const int[3]){ leon_pos[0], leon_feet, leon_pos[2] });
                    }
                    act_obj = -1;
                }
                if (react_frame >= len)
                    react = REACT_NONE;
            }
            else if (react == REACT_GIANT_UP)
            {
                if (react_frame >= len)
                {
                    leon_pos[0] += mulf32(gclimb_f[0], CLIMB_BACK);
                    leon_pos[2] += mulf32(gclimb_f[1], CLIMB_BACK);
                    leon_feet = gclimb_y + CLIMB_UP;
                    leon_pos[1] = leon_feet - leon.bbox_min[1];
                    leon_yaw = giants[gclimb].yaw;
                    react = REACT_GIANT_CUT;
                    react_frame = 0;
                    clip_key = -1;
                    bonus_note(T_BONUS_TECH);
                    sfx_play(SFX_KNIFE, 127, 64);
                }
            }
            else if (react == REACT_GIANT_CUT)
            {
                Giant *gc = &giants[gclimb];
                down &= ~KEY_A;
                if (gcut_page >= 2)
                {
                    sfx_play_at(SFX_GANADO_DOWN, 127, gc->pos, leon_pos, leon_yaw);
                    giant_cut(gc);
                    gc->held = 0;
                    walk_out(&stage_ground, leon_pos, -gclimb_f[0], -gclimb_f[1], inttof32(4), gclimb_y);
                    {
                        int tx = gc->pos[0] - leon_pos[0], tz = gc->pos[2] - leon_pos[2];
                        leon_yaw = angle_to(tx, tz);
                    }
                    ground_height(&stage_ground, leon_pos[0], leon_pos[2], leon_feet + STEP_UP, STEP_UP, &leon_feet);
                    leon_pos[1] = leon_feet - leon.bbox_min[1];
                    react = REACT_NONE;
                    react_frame = 0;
                    clip_key = -1;
                }
            }
            else if (react == REACT_CLIMB_UP || react == REACT_CLIMB_DOWN)
            {
                if (react_frame >= len)
                {
                    const Obj *l = &objs.o[act_obj];
                    leon_pos[0] = l->pos[0] + (react == REACT_CLIMB_UP ? mulf32(l->dir[0], CLIMB_BACK) : 0);
                    leon_pos[2] = l->pos[2] + (react == REACT_CLIMB_UP ? mulf32(l->dir[1], CLIMB_BACK) : 0);
                    leon_feet = react == REACT_CLIMB_UP ? l->top : l->pos[1];
                    ground_height(&stage_ground, leon_pos[0], leon_pos[2], leon_feet, STEP_UP, &leon_feet);
                    leon_pos[1] = leon_feet - leon.bbox_min[1];
                    react = REACT_NONE;
                    clip_key = -1;
                    if (ash.on)
                    {
                        int foot[3] = { l->pos[0], l->pos[1], l->pos[2] };
                        ashley_use_ladder(&ash, foot, l->dir, l->top, l->yaw, leon_feet > l->pos[1] + inttof32(4));
                    }
                    act_obj = -1;
                }
            }
            else if (react == REACT_DROP)
            {
                if (react_frame > len - 1)
                    react_frame = len - 1;
                drop_vy += DROP_GRAVITY;
                leon_feet -= drop_vy;
                if (react_frame < inttof32(6))
                {
                    leon_pos[0] += mulf32(sinLerp(leon_yaw), floattof32(0.25));
                    leon_pos[2] += mulf32(cosLerp(leon_yaw), floattof32(0.25));
                }
                if (leon_feet <= drop_to)
                {
                    leon_feet = drop_to;
                    react = REACT_LAND;
                    react_frame = 0;
                    if (drop_vy > floattof32(1.2))
                        sfx_play(SFX_LEON_HURT, 70, 64);
                }
                leon_pos[1] = leon_feet - leon.bbox_min[1];
            }
            else if (react == REACT_LAND)
            {
                if (react_frame >= LAND_FRAMES)
                    react = REACT_NONE;
            }
            else if (react_frame >= len)
            {
                react = REACT_NONE;
                safe_timer = SAFE_FRAMES;
            }
        }
        else if (safe_timer > 0)
            safe_timer--;
        int free_to_act = react == REACT_NONE;

        int fwd_l[3] = { sinLerp(leon_yaw), 0, cosLerp(leon_yaw) };
        int anim_dir = 1;

        int aim_held = ctrl == CTRL_LEON && (held & KEY_R) && free_to_act;
        int aiming = aim_held && reload_frame < 0;
        int knifing = (ctrl == CTRL_LEON && (held & KEY_L) && !aiming && free_to_act
                       && gun_state != GUN_DRAWING && gun_state != GUN_HOLSTERING) || slash_frame >= 0;
        if (knifing && knife_w == 0 && slash_frame < 0)
            aim_tilt = 0;
        knife_w += knifing ? AIM_RAISE : -AIM_RAISE;
        if (knife_w > inttof32(1)) knife_w = inttof32(1);
        if (knife_w < 0) knife_w = 0;
        if (knifing && free_to_act && (down & KEY_A) && slash_frame < 0)
        {
            slash_frame = 0;
            sfx_play(SFX_KNIFE, 120, 64);
        }
        if (slash_frame >= 0)
        {
            int len = inttof32(leon_knife.frames[KNIFE_SLASH_UP]), hit = KNIFE_HIT_FRAME;
            slash_frame += ANIM_STEP;
            if (slash_frame >= hit && slash_frame - ANIM_STEP < hit)
            {
                int which, pitch = mulf32(aim_tilt, KNIFE_PITCH), cp = cosLerp(pitch);
                int kdir[3] = { mulf32(sinLerp(leon_yaw), cp), sinLerp(pitch), mulf32(cosLerp(leon_yaw), cp) };
                int kfrom[3] = { leon_pos[0], leon_feet + KNIFE_CHEST, leon_pos[2] };
                int kt = KNIFE_REACH, kend[3];
                if (ground_raycast(&stage_ground, kfrom, kdir, KNIFE_REACH, kend, RAY_SHOT))
                {
                    s64 ex = kend[0] - kfrom[0], ey = kend[1] - kfrom[1], ez = kend[2] - kfrom[2];
                    kt = (int)sqrt64((u64)(ex * ex + ey * ey + ez * ez)) + inttof32(1);
                }
                int kobj = knife_object(&objs, kfrom, kdir, &kt);
                if (kobj < 0 && aim_tilt > 0)
                {
                    int ldir[3] = { sinLerp(leon_yaw), 0, cosLerp(leon_yaw) }, lt = KNIFE_REACH;
                    if (ground_raycast(&stage_ground, kfrom, ldir, KNIFE_REACH, kend, RAY_SHOT))
                    {
                        s64 ex = kend[0] - kfrom[0], ez = kend[2] - kfrom[2];
                        lt = (int)sqrt64((u64)(ex * ex + ez * ez)) + inttof32(1);
                    }
                    int lobj = knife_object(&objs, kfrom, ldir, &lt);
                    if (lobj >= 0)
                    {
                        kobj = lobj;
                        kt = lt;
                    }
                }
                int kdrop = drops_raycast(&drops, kfrom, kdir, &kt);
                if (kdrop >= 0)
                {
                    drops_knock(&drops, kdrop);
                    sfx_play(SFX_ITEM_GET, 90, 64);
                    hits++;
                }
                else if (kobj >= 0 && objects_hit(&objs, kobj))
                {
                    Obj *o = &objs.o[kobj];
                    hits++;
                    if (o->kind == OBJ_CRATE)
                    {
                        sfx_play_at(SFX_BREAK, 120, o->pos, leon_pos, leon_yaw);
                        crate_drop(o, (const int[3]){ leon_pos[0], leon_feet, leon_pos[2] });
                    }
                    else if (o->kind == OBJ_BELL)
                    {
                        sfx_play_at(SFX_BELL, 127, o->pos, leon_pos, leon_yaw);
                        for (int g = 0; g < gal_count; g++)
                            garrador_hear(&gals[g], o->pos, inttof32(4000));
                    }
                    else if (o->kind == OBJ_MEDAL)
                    {
                        sfx_play_at(SFX_BREAK, 110, o->pos, leon_pos, leon_yaw);
                        medal_note = 120;
                        if (objs.medals_hit == objs.medals)
                        {
                            spare += MEDAL_REWARD;
                            sfx_play(SFX_ITEM_GET, 120, 64);
                        }
                    }
                }
                else if (aim_tilt <= 0 && objects_action(&objs, leon_pos, leon_feet, leon_yaw, &which) == ACT_KICK_CRATE
                    && objects_hit(&objs, which))
                {
                    sfx_play(SFX_BREAK, 120, 64);
                    crate_drop(&objs.o[which], (const int[3]){ leon_pos[0], leon_feet, leon_pos[2] });
                    hits++;
                }
                else
                {
                    int e = enemies_slash(&ganados, leon_pos, leon_yaw, KNIFE_RANGE, KNIFE_DAMAGE);
                    if (e >= 0)
                    {
                        hits++;
                        sfx_play_at(ganados.e[e].state == ENEMY_DYING ? SFX_GANADO_DOWN : SFX_GANADO_HIT, 120, ganados.e[e].pos, leon_pos, leon_yaw);
                        if (ganados.e[e].state == ENEMY_DYING)
                            maybe_drop(&ganados.e[e], leon_hp, DROP_PERCENT);
                    }
                }
            }
            if (slash_frame >= len || !free_to_act)
                slash_frame = -1;
        }
        aim_w += aiming ? AIM_RAISE : -AIM_RAISE;
        if (aim_w > inttof32(1)) aim_w = inttof32(1);
        if (aim_w < 0) aim_w = 0;

        if (ctrl == CTRL_LEON && (down & KEY_X) && (held & KEY_L) && !aiming && free_to_act && reload_frame < 0)
        {
            int next = (wep + 1) % WEP_COUNT;
            while (!(wep_owned >> next & 1))
                next = (next + 1) % WEP_COUNT;
            weapon_switch(next, &ammo, &spare);
            fire_frame = -1;
            clip_key = -1;
            wep_note = 120;
            sfx_play(SFX_MENU, 100, 64);
            down &= ~KEY_X;
        }
        if (ctrl == CTRL_LEON && (down & KEY_X) && !aiming && free_to_act)
        {
            if (gun_state == GUN_HOLSTERED)
            {
                gun_state = GUN_DRAWING;
                draw_frame = 0;
            }
            else if (gun_state == GUN_READY)
            {
                gun_state = GUN_HOLSTERING;
                draw_frame = inttof32(DRAW_FRAMES);
            }
        }
        if (gun_state == GUN_DRAWING && (draw_frame += ANIM_STEP) >= inttof32(DRAW_FRAMES))
            gun_state = GUN_READY;
        if (gun_state == GUN_HOLSTERING && (draw_frame -= ANIM_STEP) <= 0)
            gun_state = GUN_HOLSTERED;
        int drawing = gun_state == GUN_DRAWING || gun_state == GUN_HOLSTERING;

        int gun_out = gun_state == GUN_READY || aim_held;
        if (ctrl == CTRL_LEON && reload_frame < 0 && !drawing && free_to_act)
        {
            if ((down & KEY_Y) && gun_out && ammo < MAG_SIZE && spare > 0 && USES_ROUNDS(wep))
            {
                reload_frame = 0;
                reload_ch = sfx_play(SFX_RELOAD, 110, 64);
            }
            else if (((down & KEY_A) || (wep == WEP_TMP && (held & KEY_A))) && aiming && aim_w == inttof32(1)
                     && (fire_frame < 0 || fire_frame >= wep_refire(wep)))
            {
                if (ammo == 0)
                {
                    sfx_play(SFX_EMPTY, 110, 64);
                    if (spare > 0 && USES_ROUNDS(wep))
                    {
                        reload_frame = 0;
                        reload_ch = sfx_play(SFX_RELOAD, 110, 64);
                    }
                }
                else
                {
                    sfx_play(SFX_SHOT, 127, 64);
                    ammo--;
                    shots++;
                    for (int i = 0; i < gal_count; i++)
                        garrador_hear(&gals[i], leon_pos, inttof32(4000));
                    shot_noise = 1;
                    fire_frame = 0;
                    flash_timer = 3;
                    cam_kick = floattof32(1.2);
                    if (laser_hit)
                    {
                        for (int k = 0; k < 3; k++)
                            impact_pos[k] = laser_end[k];
                        impact_timer = 12;
                        impact_blood = laser_enemy >= 0;
                    }
                    if (weapons[wep].special == WS_ROCKET)
                    {
                        for (int k = 0; k < 3; k++)
                        {
                            rocket_pos[k] = muzzle[k];
                            rocket_to[k] = laser_end[k];
                        }
                        rocket_on = 1;
                        sfx_play(SFX_EXPLODE, 70, 64);
                        laser_enemy = laser_obj = laser_drop = -1;
                        laser_ash = laser_giant = laser_gal = laser_ver = 0;
                        impact_timer = 0;
                    }
                    if (wep == WEP_SHOTGUN)
                    {
                        static const s8 spread[5][2] = { { 0, 0 }, { 5, 0 }, { -5, 0 }, { 0, 4 }, { 0, -4 } };
                        int ax = laser_end[0] - leon_pos[0], ay = laser_end[1] - (leon_feet + inttof32(12)), az = laser_end[2] - leon_pos[2];
                        (void)ax; (void)ay; (void)az;
                        int aim_yaw = leon_yaw;
                        if (laser_enemy >= 0)
                            aim_yaw = angle_to((ganados.e[laser_enemy].pos[0] - leon_pos[0]), (ganados.e[laser_enemy].pos[2] - leon_pos[2]));
                        int aim_pitch = mulf32(aim_tilt, degreesToAngle(40));
                        int pellets_hit = 0;
                        for (int k = 0; k < 5; k++)
                        {
                            int yw = aim_yaw + degreesToAngle(spread[k][0]), pt = aim_pitch + degreesToAngle(spread[k][1]);
                            int dir[3] = { mulf32(sinLerp(yw), cosLerp(pt)), sinLerp(pt), mulf32(cosLerp(yw), cosLerp(pt)) };
                            int best = -1, best_t = SHOTGUN_RANGE;
                            for (int i = 0; i < ganados.count; i++)
                            {
                                const Enemy *e = &ganados.e[i];
                                if (!e->active || e->state >= ENEMY_DYING)
                                    continue;
                                int v[3] = { e->pos[0] - leon_pos[0], e->feet + inttof32(10) - (leon_feet + inttof32(12)), e->pos[2] - leon_pos[2] };
                                int tt = mulf32(v[0], dir[0]) + mulf32(v[1], dir[1]) + mulf32(v[2], dir[2]);
                                if (tt <= 0 || tt >= best_t)
                                    continue;
                                s64 off2 = 0;
                                for (int c = 0; c < 3; c++)
                                {
                                    s64 o = v[c] - mulf32(dir[c], tt);
                                    off2 += o * o;
                                }
                                s64 r = floattof32(3.0), hh = inttof32(9);
                                int up = v[1] - mulf32(dir[1], tt);
                                if (off2 - (s64)up * up > r * r || up > hh || up < -hh)
                                    continue;
                                best_t = tt;
                                best = i;
                            }
                            if (best < 0)
                                continue;
                            enemies_shot(&ganados, best, best_t < inttof32(60) ? 120 : 80, PART_BODY, 0, CRIT_NONE);
                            pellets_hit++;
#ifdef TEST_SHOW_HP
                            test_shot_foe = best;
                            test_shots++;
#endif
                            if (ganados.e[best].state == ENEMY_DYING)
                                maybe_drop(&ganados.e[best], leon_hp, DROP_PERCENT);
                        }
                        hits += pellets_hit > 0;
                    }
                    if (laser_enemy >= 0 && wep != WEP_SHOTGUN)
                    {
                        int staggered = enemies_shot(&ganados, laser_enemy, wep_power(), laser_part, wep_crit_rate(), wep_crit_kind());
#ifdef TEST_SHOW_HP
                        test_shot_foe = laser_enemy;
                        test_shots++;
                        test_staggers += staggered && ganados.e[laser_enemy].state == ENEMY_HIT;
                        test_downs += ganados.e[laser_enemy].state == ENEMY_DYING;
#else
                        (void)staggered;
#endif
#ifdef TEST_DAZE
                        laser_head = 1;
#endif
                        if (laser_head)
                            enemies_daze(&ganados, laser_enemy);
                        if (ganados.crit_hit)
                            bonus_note(T_BONUS_CRIT);
                        else if (laser_head && ganados.e[laser_enemy].plaga)
                            bonus_note(T_BONUS_TECH);
                        ganados.crit_hit = 0;
                        hits++;
                        headshots += laser_head;
                        const Enemy *e = &ganados.e[laser_enemy];
                        sfx_play_at(e->state == ENEMY_DYING ? SFX_GANADO_DOWN : SFX_GANADO_HIT, 120, e->pos, leon_pos, leon_yaw);
                        if (e->state == ENEMY_DYING)
                            maybe_drop(e, leon_hp, DROP_PERCENT);
                        int through = wep == WEP_MAGNUM ? 2 : 0;
                        int dir[3], len2 = 0;
                        for (int k = 0; k < 3; k++)
                            dir[k] = laser_end[k] - muzzle[k];
                        len2 = f32toint(dir[0]) * f32toint(dir[0]) + f32toint(dir[2]) * f32toint(dir[2]);
                        int last = laser_enemy;
                        while (through-- > 0 && len2 > 0)
                        {
                            int best = -1, best_t = 0x7fffffff;
                            for (int i = 0; i < ganados.count; i++)
                            {
                                const Enemy *o = &ganados.e[i];
                                if (i == laser_enemy || i == last || !o->active || o->state >= ENEMY_DYING)
                                    continue;
                                int ox = f32toint(o->pos[0] - muzzle[0]), oz = f32toint(o->pos[2] - muzzle[2]);
                                int lt = ox * f32toint(dir[0]) + oz * f32toint(dir[2]);
                                int lx = f32toint(ganados.e[last].pos[0] - muzzle[0]), lz = f32toint(ganados.e[last].pos[2] - muzzle[2]);
                                if (lt <= lx * f32toint(dir[0]) + lz * f32toint(dir[2]) || lt > len2 * 6)
                                    continue;
                                s64 cross = (s64)ox * f32toint(dir[2]) - (s64)oz * f32toint(dir[0]);
                                if (cross * cross > (s64)3 * 3 * len2)
                                    continue;
                                if (lt < best_t)
                                {
                                    best_t = lt;
                                    best = i;
                                }
                            }
                            if (best < 0)
                                break;
                            enemies_shot(&ganados, best, wep_power(), PART_BODY, 0, CRIT_NONE);
                            hits++;
                            if (ganados.e[best].state == ENEMY_DYING)
                                maybe_drop(&ganados.e[best], leon_hp, DROP_PERCENT);
                            last = best;
                        }
                    }
                    else if (laser_ver)
                    {
                        int was_frozen = ver.state == VER_FROZEN;
                        verdugo_shot(&ver, wep_power());
                        hits++;
                        headshots += was_frozen;
                        impact_blood = !was_frozen;
                        sfx_play_at(ver.state == VER_DEAD ? SFX_GANADO_DOWN : SFX_GANADO_HIT, 110, ver.pos, leon_pos, leon_yaw);
                    }
                    else if (laser_gal)
                    {
                        Garrador *gh = &gals[laser_gal - 1];
                        garrador_shot(gh, wep_power(), laser_gal_back);
                        if (laser_gal_back)
                            bonus_note(T_BONUS_TECH);
                        hits++;
                        headshots += laser_gal_back;
                        impact_blood = 1;
                        sfx_play_at(gh->state == GAL_DEAD ? SFX_GANADO_DOWN : SFX_GANADO_HIT, 110, gh->pos, leon_pos, leon_yaw);
                    }
                    else if (laser_giant)
                    {
                        Giant *gh = &giants[laser_giant - 1];
                        giant_shot(gh, wep_power());
                        hits++;
                        impact_blood = 1;
                        sfx_play_at(SFX_GANADO_HIT, 100, gh->pos, leon_pos, leon_yaw);
                    }
                    else if (laser_ash)
                    {
                        ashley_hit(&ash, ASH_SHOT_DAMAGE);
                        sfx_play_at(SFX_LEON_HURT2, 110, ash.pos, leon_pos, leon_yaw);
                    }
                    else if (laser_drop >= 0 && laser_obj < 0)
                    {
                        drops_knock(&drops, laser_drop);
                        impact_blood = 0;
                        sfx_play(SFX_ITEM_GET, 90, 64);
                    }
                    else if (laser_obj >= 0)
                    {
                        Obj *o = &objs.o[laser_obj];
                        impact_blood = 0;
                        if (objects_hit(&objs, laser_obj))
                        {
                            hits++;
                            if (o->kind == OBJ_CRATE)
                            {
                                sfx_play_at(SFX_BREAK, 120, o->pos, leon_pos, leon_yaw);
                                crate_drop(o, (const int[3]){ leon_pos[0], leon_feet, leon_pos[2] });
                            }
                            else if (o->kind == OBJ_BELL)
                            {
                                sfx_play_at(SFX_BELL, 127, o->pos, leon_pos, leon_yaw);
                                for (int g = 0; g < gal_count; g++)
                                    garrador_hear(&gals[g], o->pos, inttof32(4000));
                            }
                            else if (o->kind == OBJ_MEDAL)
                            {
                                sfx_play_at(SFX_BREAK, 110, o->pos, leon_pos, leon_yaw);
                                medal_note = 120;
                                if (objs.medals_hit == objs.medals)
                                {
                                    spare += MEDAL_REWARD;
                                    sfx_play(SFX_ITEM_GET, 120, 64);
                                }
                            }
                            else
                                sfx_play_at(SFX_WALL, 70, laser_end, leon_pos, leon_yaw);
                        }
                    }
                    else if (laser_hit)
                        sfx_play_at(SFX_WALL, 70, laser_end, leon_pos, leon_yaw);
                }
            }
        }
        if (fire_frame >= 0 && (fire_frame += ANIM_STEP) >= inttof32(FIRE_FRAMES))
            fire_frame = -1;
        if (reload_frame >= 0 && (reload_frame += mulf32(ANIM_STEP, wep_reload_speed(wep))) >= inttof32(RELOAD_FRAMES))
        {
            reload_frame = -1;
            int load = MAG_SIZE - ammo < spare ? MAG_SIZE - ammo : spare;
            ammo += load;
            spare -= load;
        }
        if (reload_frame >= 0 && (!gun_out || !free_to_act))
        {
            reload_frame = -1;
            sfx_stop(reload_ch);
        }
        if (reload_frame < 0)
            reload_ch = -1;

        if (ctrl == CTRL_LEON && ((drawing && !aiming) || !free_to_act))
        {
        }
        else if (ctrl == CTRL_LEON && (aiming || knifing))
        {
            qturn = 0;
            if (held & KEY_LEFT)  leon_yaw += AIM_TURN;
            if (held & KEY_RIGHT) leon_yaw -= AIM_TURN;
            if (held & KEY_UP)    aim_tilt += AIM_TILT;
            if (held & KEY_DOWN)  aim_tilt -= AIM_TILT;
            if (aim_tilt > inttof32(1))  aim_tilt = inttof32(1);
            if (aim_tilt < -inttof32(1)) aim_tilt = -inttof32(1);
            fwd_l[0] = sinLerp(leon_yaw);
            fwd_l[2] = cosLerp(leon_yaw);
            if (leon_anim != ANIM_IDLE)
            {
                leon_anim = ANIM_IDLE;
                anim_frame = 0;
            }
        }
        else if (ctrl == CTRL_LEON)
        {
            if (!aim_w)
                aim_tilt = 0;

            static int down_t, b_t;
            down_t = (held & KEY_DOWN) ? down_t + 1 : 0;
            b_t = (held & KEY_B) ? b_t + 1 : 0;
            if (qturn == 0 && (held & KEY_DOWN) && (held & KEY_B) && ((down & KEY_B) || (down & KEY_DOWN))
                && down_t <= QTURN_PAIR && b_t <= QTURN_PAIR)
                qturn = QTURN_FRAMES;
            if (qturn == 0)
            {
                if (held & KEY_LEFT)  leon_yaw += TURN_SPEED;
                if (held & KEY_RIGHT) leon_yaw -= TURN_SPEED;
            }
            else
                leon_yaw += (DEGREES_IN_CIRCLE / 2) / QTURN_FRAMES;
            fwd_l[0] = sinLerp(leon_yaw);
            fwd_l[2] = cosLerp(leon_yaw);

            int hurt = leon_hp < LEON_HURT_BELOW;
            int speed = 0, want = ANIM_IDLE;
            if (qturn > 0)
            {
                want = ANIM_WALK;
                speed = QTURN_FRAMES - qturn >= QTURN_MOVE_FROM ? LEON_MOVE(200) : 0;
                qturn--;
            }
            else if (held & KEY_UP)
            {
                want = (held & KEY_B) ? ANIM_RUN : ANIM_WALK;
                speed = want == ANIM_RUN ? (hurt ? LEON_MOVE(600) : RUN_SPEED) : (hurt ? LEON_MOVE(280) : WALK_SPEED);
            }
            else if (held & KEY_DOWN)
            {
                want = ANIM_WALK;
                speed = -(hurt ? LEON_MOVE(380) : BACK_SPEED);
                anim_dir = -1;
            }
            if (want != leon_anim)
            {
                leon_anim = want;
                anim_frame = 0;
            }

            if (jump_now && mode == MODE_PLAY)
            {
                leon_pos[0] = inttof32(jump_now->to_x);
                leon_pos[2] = inttof32(jump_now->to_z);
                leon_feet = inttof32(jump_now->to_y);
                ground_height(&stage_ground, leon_pos[0], leon_pos[2], leon_feet + STEP_UP, STEP_UP * 2, &leon_feet);
                leon_pos[1] = leon_feet - leon.bbox_min[1];
                leon_yaw = degreesToAngle(jump_now->to_yaw);
                if (ash.on && !ash.wait)
                    ashley_place(&ash, &stage_ground, leon_pos[0] - mulf32(sinLerp(leon_yaw), inttof32(6)), leon_pos[2] - mulf32(cosLerp(leon_yaw), inttof32(6)),
                                 leon_feet + inttof32(4), leon_yaw);
                clip_key = -1;
                jump_now = NULL;
            }
            const JumpSpot *js = (down & KEY_A) && react == REACT_NONE ? jump_at_hand(cur_stage, leon_pos, leon_feet, leon_yaw) : NULL;
            if (js)
            {
                jump_now = js;
                if (cut_start(cur_stage, CUT_CHANDELIER, MODE_PLAY, CUT_AFTER_JUMP, 0))
                    mode = MODE_CUTIN;
                sfx_play(SFX_MENU, 100, 64);
                speed = 0;
                down &= ~KEY_A;
            }
            Giant *gc = (down & KEY_A) ? giant_at_hand(leon_pos) : NULL;
            if (gc)
            {
                int fx = sinLerp(gc->yaw), fz = cosLerp(gc->yaw);
                gclimb = gc - giants;
                gclimb_f[0] = fx;
                gclimb_f[1] = fz;
                leon_pos[0] = gc->pos[0] - mulf32(fx, GIANT_BACK);
                leon_pos[2] = gc->pos[2] - mulf32(fz, GIANT_BACK);
                walk_out(&stage_ground, leon_pos, -fx, -fz, CLIMB_BACK, gc->feet);
                leon_feet = gc->feet;
                ground_height(&stage_ground, leon_pos[0], leon_pos[2], gc->feet + STEP_UP, STEP_UP, &leon_feet);
                leon_pos[1] = leon_feet - leon.bbox_min[1];
                gclimb_y = leon_feet;
                leon_yaw = gc->yaw;
                react = REACT_GIANT_CUT;
                react_frame = 0;
                gcut_page = 0;
                if (cut_start(cur_stage, CUT_GIANT_UP, MODE_PLAY, CUT_AFTER_GIANT, 0))
                    mode = MODE_CUTIN;
                else
                    gcut_page = 2;
                sfx_play(SFX_MENU, 100, 64);
                gun_state = GUN_HOLSTERED;
                clip_key = -1;
                slash_frame = -1;
                gc->held = 1;
                speed = 0;
                down &= ~KEY_A;
            }
            int dazed = (down & KEY_A) ? enemies_kickable(&ganados, leon_pos, leon_yaw) : -1;
            if ((down & KEY_A) && dazed < 0)
            {
                int at[3] = { leon_pos[0], leon_feet, leon_pos[2] };
                if (cur_stage != 0 && ev_button >= 0)
                {
                    ev_press = 1;
                    int lw, la = objects_action(&objs, leon_pos, leon_feet, leon_yaw, &lw);
                    if (la != ACT_LADDER_UP && la != ACT_LADDER_DOWN)
                        down &= ~KEY_A;
                }
                else if (drops_in_reach(&drops, at) >= 0)
                {
                    take_item = 1;
                    down &= ~KEY_A;
                }
            }
            int which = -1, act = (down & KEY_A) && dazed < 0 ? objects_action(&objs, leon_pos, leon_feet, leon_yaw, &which) : ACT_NONE;
            if (dazed >= 0)
            {
                int dx = ganados.e[dazed].pos[0] - leon_pos[0], dz = ganados.e[dazed].pos[2] - leon_pos[2];
                leon_yaw = angle_to(dx, dz);
                react = REACT_KICK;
                react_frame = 0;
                act_obj = -1;
                speed = 0;
            }
            else if (act == ACT_KICK_CRATE)
            {
                react = REACT_KICK;
                react_frame = 0;
                act_obj = which;
                speed = 0;
            }
            else if (act == ACT_OPEN_DOOR)
                objects_open_door(&objs, which, leon_pos);
            else if (act == ACT_LADDER_UP || act == ACT_LADDER_DOWN)
            {
                const Obj *l = &objs.o[which];
                react = act == ACT_LADDER_UP ? REACT_CLIMB_UP : REACT_CLIMB_DOWN;
                react_frame = 0;
                act_obj = which;
                leon_pos[0] = l->pos[0];
                leon_pos[2] = l->pos[2];
                leon_feet = l->pos[1];
                leon_pos[1] = leon_feet - leon.bbox_min[1];
                leon_yaw = l->yaw;
                gun_state = GUN_HOLSTERED;
                clip_key = -1;
                speed = 0;
            }

            if (speed)
            {
                int from_x = leon_pos[0], from_z = leon_pos[2];
                leon_pos[0] += mulf32(fwd_l[0], speed);
                leon_pos[2] += mulf32(fwd_l[2], speed);
                enemies_push(&ganados, &leon_pos[0], &leon_pos[2], LEON_RADIUS);
                if (ash.on)
                    ashley_push(&ash, &leon_pos[0], &leon_pos[2], LEON_RADIUS);
                for (int i = 0; i < giant_count; i++)
                    giant_push(&giants[i], &leon_pos[0], &leon_pos[2], LEON_RADIUS);
                for (int i = 0; i < gal_count; i++)
                    garrador_push(&gals[i], &leon_pos[0], &leon_pos[2], LEON_RADIUS);
                if (ver.on)
                    verdugo_push(&ver, &leon_pos[0], &leon_pos[2], LEON_RADIUS);
                objects_collide(&objs, &leon_pos[0], &leon_pos[2], leon_feet, LEON_RADIUS);
                ground_collide(&stage_ground, &leon_pos[0], &leon_pos[2],
                                           leon_feet + STEP_UP, leon_feet + LEON_HEIGHT, LEON_RADIUS);
                int floor = leon_feet;
                ground_height(&stage_ground, leon_pos[0], leon_pos[2], leon_feet, STEP_UP, &floor);
                if (floor < leon_feet - DROP_MIN)
                {
                    int wx = from_x + mulf32(fwd_l[0], speed), wz = from_z + mulf32(fwd_l[2], speed), wfloor = leon_feet;
                    ground_collide(&stage_ground, &wx, &wz, leon_feet + STEP_UP, leon_feet + LEON_HEIGHT, LEON_RADIUS);
                    ground_height(&stage_ground, wx, wz, leon_feet, STEP_UP, &wfloor);
                    if (wfloor >= leon_feet - DROP_MIN)
                    {
                        leon_pos[0] = wx;
                        leon_pos[2] = wz;
                        floor = wfloor;
                    }
                }
                if (floor < leon_feet - DROP_MIN && cur_stage == ASH_STAGE_E)
                {
                    leon_pos[0] = from_x;
                    leon_pos[2] = from_z;
                    floor = leon_feet;
                }
                if (floor < leon_feet - DROP_MIN)
                {
                    react = REACT_DROP;
                    react_frame = 0;
                    drop_to = floor;
                    drop_vy = 0;
                }
                else
                    leon_feet = floor;
                leon_pos[1] = leon_feet - leon.bbox_min[1];
            }
        }
        else
        {
            if (down & KEY_START)
            {
                leon_anim = (leon_anim + 1) % 4;
                anim_frame = 0;
            }
            if (leon_anim == ANIM_WALK || leon_anim == ANIM_RUN)
            {
                int speed = leon_anim == ANIM_WALK ? WALK_SPEED : RUN_SPEED;
                leon_angle += (int)(((s64)speed * DEGREES_IN_CIRCLE) / (2 * 3.14159265 * circle_radius));
                leon_pos[0] = circle_x + mulf32(sinLerp(leon_angle), circle_radius);
                leon_pos[2] = circle_z + mulf32(cosLerp(leon_angle), circle_radius);
                leon_yaw = leon_angle + DEGREES_IN_CIRCLE / 4;
                ground_height(&stage_ground, leon_pos[0], leon_pos[2], leon_feet, STEP_UP, &leon_feet);
                leon_pos[1] = leon_feet - leon.bbox_min[1];
            }
        }

        int look_eye[3], look_at[3];
        if (ctrl == CTRL_LEON)
        {
            {
                s16 dy = (s16)(leon_yaw - cam_yaw);
                cam_yaw += (aim_w > 0 || knife_w > 0) ? dy / 2 : dy / 8;
            }
            int fwd_c[3] = { sinLerp(cam_yaw), 0, cosLerp(cam_yaw) };
            int right[3] = { -fwd_c[2], 0, fwd_c[0] };
            int cam_w = aim_w;
            int to_back = CAM_AIM_BACK, to_side = CAM_AIM_RIGHT;
            int to_height = CAM_AIM_HEIGHT;
            int back = CAM_BACK + mulf32(to_back - CAM_BACK, cam_w);
            int side = CAM_RIGHT + mulf32(to_side - CAM_RIGHT, cam_w);
            int side_at = CAM_AT_RIGHT + mulf32(to_side - CAM_AT_RIGHT, cam_w);
            int want_eye[3], want_at[3];
            for (int k = 0; k < 3; k += 2)
            {
                want_eye[k] = leon_pos[k] - mulf32(fwd_c[k], back) + mulf32(right[k], side);
                want_at[k] = leon_pos[k] + mulf32(fwd_c[k], CAM_AHEAD) + mulf32(right[k], side_at);
            }
            want_eye[1] = leon_feet + CAM_HEIGHT + mulf32(to_height - CAM_HEIGHT, cam_w);
            want_at[1] = leon_feet + CAM_AT_HEIGHT + mulf32(mulf32(CAM_AIM_TILT, aim_tilt), cam_w);
            if (knife_w > aim_w)
            {
                int piv_m[3] = { knife_hand[0] + KNIFE_PIV_X, knife_hand[1] + KNIFE_PIV_Y, knife_hand[2] + KNIFE_PIV_Z };
                int eye_m[3] = { piv_m[0] - KNIFE_CAM_SIDE, piv_m[1] + KNIFE_CAM_UP, piv_m[2] - KNIFE_CAM_BACK };
                int k_eye[3], k_at[3];
                for (int k = 0; k < 3; k += 2)
                {
                    k_eye[k] = leon_pos[k] + mulf32(fwd_c[k], eye_m[2]) - mulf32(right[k], eye_m[0]);
                    k_at[k] = leon_pos[k] + mulf32(fwd_c[k], piv_m[2]) - mulf32(right[k], piv_m[0]);
                }
                k_eye[1] = leon_feet + eye_m[1];
                k_at[1] = leon_feet + piv_m[1] + mulf32(KNIFE_CAM_TILT, aim_tilt);
                for (int k = 0; k < 3; k++)
                {
                    want_eye[k] += mulf32(k_eye[k] - want_eye[k], knife_w);
                    want_at[k] += mulf32(k_at[k] - want_at[k], knife_w);
                }
            }
            want_at[1] += cam_kick;
            cam_kick = cam_kick * 3 / 5;
#ifdef CAM_TEST_SIDE
            for (int k = 0; k < 3; k += 2)
            {
                want_eye[k] = leon_pos[k] + right[k] * 16 + fwd_l[k] * 4;
                want_at[k] = leon_pos[k] + fwd_l[k] * 4;
            }
            want_eye[1] = leon_feet + inttof32(12);
            want_at[1] = leon_feet + inttof32(11);
#endif
#ifdef CAM_TEST_FRONT
            for (int k = 0; k < 3; k += 2)
            {
                want_eye[k] = leon_pos[k] + fwd_l[k] * 11 - right[k] * 4;
                want_at[k] = leon_pos[k];
            }
            want_eye[1] = leon_feet + inttof32(13);
            want_at[1] = leon_feet + inttof32(10);
#endif
            int on_giant = react == REACT_GIANT_UP;
            int climbing = ((react == REACT_CLIMB_UP || react == REACT_CLIMB_DOWN) && act_obj >= 0) || on_giant;
            if (climbing)
            {
                int foot[3], in[2], top;
                if (on_giant)
                {
                    int back = react == REACT_GIANT_CUT ? CLIMB_BACK : 0;
                    foot[0] = leon_pos[0] - mulf32(gclimb_f[0], back);
                    foot[1] = gclimb_y;
                    foot[2] = leon_pos[2] - mulf32(gclimb_f[1], back);
                    in[0] = gclimb_f[0];
                    in[1] = gclimb_f[1];
                    top = gclimb_y + CLIMB_UP;
                }
                else
                {
                    const Obj *l = &objs.o[act_obj];
                    foot[0] = l->pos[0];
                    foot[1] = l->pos[1];
                    foot[2] = l->pos[2];
                    in[0] = l->dir[0];
                    in[1] = l->dir[1];
                    top = l->top;
                }
                int len = inttof32(leon_anims.frames[react_clip[react]]);
                int t = react == REACT_GIANT_CUT ? inttof32(1) : len > 0 ? divf32(react_frame, len) : inttof32(1);
                t = t > inttof32(1) ? inttof32(1) : t;
                if (react == REACT_CLIMB_DOWN)
                    t = inttof32(1) - t;
                int body_y = foot[1] + mulf32(top - foot[1], t);
                int out[2] = { -in[0], -in[1] };
                int sx = -out[1], sz = out[0];
                for (int k = 0; k < 2; k++)
                {
                    int base = (k ? foot[2] : foot[0]) + mulf32(k ? in[1] : in[0], mulf32(CLIMB_BACK, t) / 2);
                    want_eye[k * 2] = base + mulf32(out[k], CAM_CLIMB_OUT) + mulf32(k ? sz : sx, CAM_CLIMB_SIDE);
                    want_at[k * 2] = base;
                }
                want_eye[1] = body_y + CAM_CLIMB_HEIGHT;
                want_at[1] = body_y + CAM_CLIMB_AT;
                if (react_frame <= ANIM_STEP)
                    for (int k = 0; k < 3; k++)
                    {
                        follow_eye[k] = want_eye[k];
                        follow_target[k] = want_at[k];
                    }
            }
            if (climbing)
                approach(follow_eye, want_eye);
            else
            {
                follow_eye[0] = want_eye[0];
                follow_eye[2] = want_eye[2];
                follow_eye[1] += (want_eye[1] - follow_eye[1]) >> 3;
            }
            if (climbing)
                approach(follow_target, want_at);
            else
            {
                follow_target[0] = want_at[0];
                follow_target[2] = want_at[2];
                follow_target[1] += (want_at[1] - follow_target[1]) >> 3;
            }
            if (!climbing)
            {
                int cw = knife_w > aim_w ? knife_w : aim_w;
                int dist = CAM_PIV_DIS + mulf32(CAM_AIM_DIS - CAM_PIV_DIS, cw);
                int tilt_y = mulf32(mulf32(CAM_AIM_TILT, aim_tilt), aim_w) + mulf32(mulf32(KNIFE_CAM_TILT, aim_tilt), knife_w);
                int dv[3] = { follow_target[0] - follow_eye[0], follow_target[1] - tilt_y - follow_eye[1], follow_target[2] - follow_eye[2] };
                int len = (int)sqrt64((u64)((s64)dv[0] * dv[0] + (s64)dv[1] * dv[1] + (s64)dv[2] * dv[2]));
                if (len > 0)
                {
                    int back[3], piv[3], hit[3];
                    for (int k = 0; k < 3; k++)
                    {
                        back[k] = -divf32(dv[k], len);
                        piv[k] = follow_eye[k] - mulf32(back[k], dist);
                    }
                    if (ground_raycast(&stage_ground, piv, back, dist + CAM_WALL_GAP, hit, RAY_CAMERA))
                    {
                        int hv[3] = { hit[0] - piv[0], hit[1] - piv[1], hit[2] - piv[2] };
                        int hd = (int)sqrt64((u64)((s64)hv[0] * hv[0] + (s64)hv[1] * hv[1] + (s64)hv[2] * hv[2])) - CAM_WALL_GAP;
                        if (hd < 0)
                            hd = 0;
                        if (hd < dist)
                            for (int k = 0; k < 3; k++)
                                follow_eye[k] = piv[k] + mulf32(back[k], hd);
                    }
                }
            }
#ifdef TEST_CAM_JUMPS
            {
                static int prev_eye[3];
                s64 jx = follow_eye[0] - prev_eye[0], jy = follow_eye[1] - prev_eye[1], jz = follow_eye[2] - prev_eye[2];
                if (frame_no > 600 && knife_w == inttof32(1) && jx * jx + jy * jy + jz * jz > (s64)inttof32(2) * inttof32(2))
                    cam_jumps++;
                for (int k = 0; k < 3; k++)
                    prev_eye[k] = follow_eye[k];
                int ex = f32toint(follow_eye[0] - leon_pos[0]), ez = f32toint(follow_eye[2] - leon_pos[2]);
                int d = (int)sqrtf((float)(ex * ex + ez * ez));
                if (frame_no > 600 && knife_w == inttof32(1) && d < cam_near_min)
                    cam_near_min = d;
            }
#endif
            {
                int hx = follow_eye[0] - leon_pos[0], hz = follow_eye[2] - leon_pos[2];
                int near = (int)sqrt64((u64)((s64)hx * hx + (s64)hz * hz));
                leon_fade = 31;
                if (near < CAM_FADE && !climbing)
                    leon_fade = 8 + 23 * near / CAM_FADE;
            }
            int cam_floor;
            if (ground_height(&stage_ground, follow_eye[0], follow_eye[2], (follow_eye[1] < want_eye[1] + inttof32(6) ? follow_eye[1] : want_eye[1] + inttof32(6)), 0, &cam_floor)
                && follow_eye[1] < cam_floor + inttof32(3))
                follow_eye[1] = cam_floor + inttof32(3);
            for (int k = 0; k < 3; k++)
            {
                look_eye[k] = follow_eye[k];
                look_at[k] = follow_target[k];
            }
            if (scoping)
                for (int k = 0; k < 3; k++)
                {
                    look_eye[k] = muzzle[k];
                    look_at[k] = muzzle[k] + scope_dir[k] * 64;
                }
        }
        else
        {
            if (held & KEY_LEFT)  yaw -= degreesToAngle(2);
            if (held & KEY_RIGHT) yaw += degreesToAngle(2);
            if (held & KEY_X)     pitch += degreesToAngle(1);
            if (held & KEY_B)     pitch -= degreesToAngle(1);
            if (held & KEY_TOUCH)
            {
                touchPosition t;
                touchRead(&t);
                if (!(down & KEY_TOUCH))
                {
                    yaw += (t.px - last_touch.px) * degreesToAngle(1) / 2;
                    pitch -= (t.py - last_touch.py) * degreesToAngle(1) / 2;
                }
                last_touch = t;
            }
            if (pitch > degreesToAngle(80))  pitch = degreesToAngle(80);
            if (pitch < -degreesToAngle(80)) pitch = -degreesToAngle(80);

            int sy = sinLerp(yaw), cy = cosLerp(yaw), sp = sinLerp(pitch), cp = cosLerp(pitch);
            int fwd[3] = { mulf32(sy, cp), sp, -mulf32(cy, cp) };
            int right[3] = { cy, 0, sy };

            const int speed = floattof32(1.5);
            int dir = ((held & KEY_UP) ? 1 : 0) - ((held & KEY_DOWN) ? 1 : 0);
            int side = ((held & KEY_R) ? 1 : 0) - ((held & KEY_L) ? 1 : 0);
            int lift = ((held & KEY_A) ? 1 : 0) - ((held & KEY_Y) ? 1 : 0);
            eye[0] += mulf32(sy, speed) * dir + mulf32(right[0], speed) * side;
            eye[2] += -mulf32(cy, speed) * dir + mulf32(right[2], speed) * side;
            eye[1] += speed * lift;
            for (int k = 0; k < 3; k++)
            {
                look_eye[k] = eye[k];
                look_at[k] = eye[k] + fwd[k];
            }
        }

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAtf32(look_eye[0], look_eye[1], look_eye[2], look_at[0], look_at[1], look_at[2], 0, inttof32(1), 0);

        glColor(RGB15(31, 31, 31));
        int origin[3] = { 0, 0, 0 };
#ifdef TEST_ASH_FIRST
        if (ash.on)
            ashley_draw(&ash, cull_modes[cull] | POLY_ID(6) | POLY_FOG);
        if (giant.on)
            giant_draw(&giant, cull_modes[cull] | POLY_ID(7) | POLY_FOG);
#endif
#ifdef TEST_PROF
        cpuStartTiming(0);
        prof_mark = 0;
#endif
        model_set_view(look_eye, look_at);
        if (stage.chunked)
        {
            if (stage_budget < 0 || stage_budget > (int)stage.triangles)
                stage_budget = stage.triangles;
            {
                const Room *rm = room_of(cur_stage);
                if (rm)
                {
                    int in = leon_pos[0] > inttof32(rm->x0) && leon_pos[0] < inttof32(rm->x1) && leon_pos[2] > inttof32(rm->z0) && leon_pos[2] < inttof32(rm->z1)
                             && leon_feet < inttof32(rm->top);
                    int box[4] = { inttof32(rm->x0 + (in ? -rm->yard : 4)), inttof32(rm->z0 + (in ? -rm->yard : 4)),
                                   inttof32(rm->x1 - (in ? -rm->yard : 4)), inttof32(rm->z1 - (in ? -rm->yard : 4)) };
                    model_set_cull_box(in ? CULL_OUTSIDE : CULL_INSIDE, box);
                }
                else
                    model_set_cull_box(CULL_NONE, NULL);
                int below = 0;
                for (unsigned i = 0; i < sizeof(galleries) / sizeof(galleries[0]); i++)
                    if (galleries[i].stage == cur_stage && leon_feet >= inttof32(galleries[i].from))
                        below = inttof32(galleries[i].below);
                model_set_cull_below(below);
                ganados.hide_below = below;
            }
            model_draw_near(&stage, look_eye, look_at, stage_budget, cull_modes[cull] | POLY_ID(1) | POLY_FOG);
        }
        else
            model_draw_async(&stage, origin, cull_modes[cull] | POLY_ID(1) | POLY_FOG);

        if (leon_anim != ANIM_OFF)
        {
            anim_frame += (anim_dir < 0 ? ANIM_STEP * 4 / 3 : ANIM_STEP) * anim_dir;

            const AnimSet *set = &leon_anims;
            int clip = leon_anim, clip_frame = anim_frame, knife_mix = 0;
            if (leon_hp < LEON_HURT_BELOW)
                clip += CLIP_HURT_IDLE;
            if (react != REACT_NONE)
            {
                clip = react_clip[react];
                clip_frame = react_frame;
                int len = inttof32(leon_anims.frames[clip]) - 1;
                if (react == REACT_CLIMB_DOWN)
                    clip_frame = len - react_frame;
                if (clip_frame > len)
                    clip_frame = len;
                if (clip_frame < 0)
                    clip_frame = 0;
            }
            else if (drawing)
            {
                set = &leon_gun;
                clip = GUN_DRAW;
                clip_frame = draw_frame;
            }
            else if (gun_state == GUN_READY)
            {
                set = &leon_gun;
                clip = leon_anim == ANIM_WALK ? GUN_WALK : leon_anim == ANIM_RUN ? GUN_RUN : GUN_READY_POSE;
            }
            if (knifing && react == REACT_NONE)
            {
                set = &leon_knife;
                clip = slash_frame >= 0 ? KNIFE_SLASH_UP : KNIFE_READY_UP;
                if (slash_frame >= 0)
                    clip_frame = slash_frame;
                else
                {
                    int last = leon_knife.frames[KNIFE_READY_DOWN] - 1;
                    clip_frame = inttof32(last > 0 ? last : 0);
                }
                knife_mix = 1;
#ifdef TEST_KNIFE_VIEW
                {
                    static int kv;
#ifdef TEST_KNIFE_SLOW
                    static const int order[] = { 1, 5, 6, 3 };
                    clip = order[(kv / 480) % 4];
                    clip_frame = (kv % 480) * ANIM_STEP / 8;
#else
                    clip = (kv / 360) % 7;
                    clip_frame = (kv % 360) * ANIM_STEP / 4;
#endif
                    kv++;
                }
                if (leon_knife.frames[clip] > 0) clip_frame %= inttof32(leon_knife.frames[clip]); else clip_frame = 0;
                knife_mix = 0;
#endif
            }
            anim_sample(set, clip, clip_frame, &leon, pose_base);
            if (knife_mix)
            {
                int down_clip = clip == KNIFE_SLASH_UP ? KNIFE_SLASH_DOWN : KNIFE_READY_DOWN;
                int f = clip_frame;
                if (leon_knife.frames[down_clip] > 0 && f >= inttof32(leon_knife.frames[down_clip]))
                    f = inttof32(leon_knife.frames[down_clip]) - 1;
                anim_sample(&leon_knife, down_clip, f, &leon, pose_tilt);
#ifdef TEST_EULER_BLEND
                anim_blend(pose_base, pose_tilt, leon.node_count, (inttof32(1) - aim_tilt) / 2);
#elif defined(TEST_KNIFE_ONLY)
                anim_blend_q(pose_base, pose_tilt, leon.node_count, TEST_KNIFE_ONLY == 6 ? inttof32(1) : 0);
#else
                anim_blend_q(pose_base, pose_tilt, leon.node_count, (inttof32(1) - aim_tilt) / 2);
#endif
                knife_mix = 0;
#ifdef TEST_CAM_JUMPS
                {
                    static int prev_r[MODEL_MAX_NODES][3], have;
                    for (int n = 0; n < leon.node_count; n++)
                        for (int j = 0; j < 3; j++)
                        {
                            int d = (pose_base[n].r[j] - prev_r[n][j]) % DEGREES_IN_CIRCLE;
                            if (d > DEGREES_IN_CIRCLE / 2) d -= DEGREES_IN_CIRCLE;
                            if (d < -DEGREES_IN_CIRCLE / 2) d += DEGREES_IN_CIRCLE;
                            if (have && slash_frame < 0 && knife_w == inttof32(1) && (d > degreesToAngle(30) || d < -degreesToAngle(30)))
                                cam_xfades++;
                            prev_r[n][j] = pose_base[n].r[j];
                        }
                    have = 1;
                }
#endif
            }

            int key =(set == &leon_gun ? 100 : set == &leon_knife ? 200 : 0) + clip;
            if (key != clip_key)
            {
                if (clip_key >= 0)
                {
                    memcpy(pose_from, pose_prev, sizeof(pose_from));
                    xfade_w = inttof32(1);
                }
#ifdef TEST_CAM_JUMPS
                if (frame_no > 660 && knife_w == inttof32(1))
                    cam_xfades += 10000;
#endif
                clip_key = key;
            }
            if (xfade_w > 0)
            {
                memcpy(pose_tilt, pose_from, sizeof(pose_tilt));
                anim_blend(pose_tilt, pose_base, leon.node_count, inttof32(1) - xfade_w);
                memcpy(pose_base, pose_tilt, sizeof(pose_base));
                xfade_w -= XFADE_STEP;
            }
            memcpy(pose_prev, pose_base, sizeof(pose_prev));

            if (aim_w)
            {
                anim_sample(&leon_gun, AIM_FORWARD, 0, &leon, pose_aim);
                anim_sample(&leon_gun, aim_tilt >= 0 ? AIM_UP : AIM_DOWN, 0, &leon, pose_tilt);
                anim_blend(pose_aim, pose_tilt, leon.node_count, aim_tilt >= 0 ? aim_tilt : -aim_tilt);
                anim_blend(pose_base, pose_aim, leon.node_count, aim_w);
            }
            if (fire_frame >= 0 && aim_w)
            {
                int clip_fire = aim_tilt >= 0 ? FIRE_UP : FIRE_DOWN;
                anim_sample(&leon_gun, clip_fire, fire_frame, &leon, pose_fx);
                anim_sample(&leon_gun, clip_fire, 0, &leon, pose_fx0);
                anim_add_delta_rot(pose_base, pose_fx, pose_fx0, leon.node_count, mulf32(RECOIL, aim_w));
            }
            if (reload_frame >= 0)
            {
                int edge = reload_frame < inttof32(RELOAD_FRAMES) - reload_frame
                         ? reload_frame : inttof32(RELOAD_FRAMES) - reload_frame;
                int w = divf32(edge, RELOAD_FADE);
                anim_sample(&leon_gun, GUN_RELOAD, reload_frame, &leon, pose_fx);
                anim_blend(pose_base + UPPER_BODY, pose_fx + UPPER_BODY, leon.node_count - UPPER_BODY, w > inttof32(1) ? inttof32(1) : w);
            }
            anim_compose(&leon, pose_base, leon_pose);
#ifdef TEST_CAM_JUMPS
            {
                static int prev_p[MODEL_MAX_NODES][3], have_p;
                int worst = 0;
                for (int n = 0; n < leon.node_count; n++)
                {
                    s64 dx = leon_pose[n][9] - prev_p[n][0], dy = leon_pose[n][10] - prev_p[n][1], dz = leon_pose[n][11] - prev_p[n][2];
                    int d = (int)sqrt64((u64)(dx * dx + dy * dy + dz * dz));
                    if (d > worst)
                        worst = d;
                    for (int k = 0; k < 3; k++)
                        prev_p[n][k] = leon_pose[n][9 + k];
                }
                if (have_p && knifing && slash_frame < 0 && knife_w == inttof32(1) && worst > floattof32(0.8))
                    cam_jumps++;
                have_p = 1;
            }
#endif
            if (knifing && slash_frame < 0 && react == REACT_NONE && knife_w < inttof32(1))
                for (int k = 0; k < 3; k++)
                    knife_hand[k] = leon_pose[KNIFE_HAND][9 + k];
            const int *hand = leon_pose[HAND_NODE];
            int m_model[3], d_model[3] = { -hand[0], -hand[3], -hand[6] }, d_world[3], zero[3] = { 0, 0, 0 };
            for (int k = 0; k < 3; k++)
                m_model[k] = mulf32(hand[k * 3], weapons[wep].muzzle[0]) + mulf32(hand[k * 3 + 1], weapons[wep].muzzle[1])
                           + mulf32(hand[k * 3 + 2], weapons[wep].muzzle[2]) + hand[9 + k];
            leon_to_world(leon_pos, leon_yaw, m_model, muzzle);
            leon_to_world(zero, leon_yaw, d_model, d_world);
            scoping = weapons[wep].special == WS_SCOPE && aim_w == inttof32(1) && reload_frame < 0 && react == REACT_NONE;
            if (scoping)
            {
                int pitch = mulf32(aim_tilt, SCOPE_PITCH), cp = cosLerp(pitch);
                d_world[0] = mulf32(sinLerp(leon_yaw), cp);
                d_world[1] = sinLerp(pitch);
                d_world[2] = mulf32(cosLerp(leon_yaw), cp);
                muzzle[0] = leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(2));
                muzzle[1] = leon_feet + SCOPE_EYE;
                muzzle[2] = leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(2));
                for (int k = 0; k < 3; k++)
                    scope_dir[k] = d_world[k];
            }
            laser_hit = 0;
            laser_enemy = laser_obj = -1;
            if (aim_w == inttof32(1) && reload_frame < 0)
            {
                laser_hit = ground_raycast(&stage_ground, muzzle, d_world, SHOT_RANGE, laser_end, RAY_SHOT);
                int t_best = SHOT_RANGE;
                if (laser_hit)
                {
                    s64 ex = laser_end[0] - muzzle[0], ey = laser_end[1] - muzzle[1], ez = laser_end[2] - muzzle[2];
                    t_best = (int)sqrt64((u64)(ex * ex + ey * ey + ez * ez));
                }
                laser_obj = objects_raycast(&objs, muzzle, d_world, &t_best);
                int t_drop = t_best + TREASURE_REACH;
                laser_drop = drops_raycast(&drops, muzzle, d_world, &t_drop);
                if (laser_drop >= 0)
                {
                    if (t_drop < t_best)
                        t_best = t_drop;
                    laser_obj = -1;
                }
                laser_enemy = enemies_raycast(&ganados, muzzle, d_world, &t_best, &laser_part);
                if (laser_enemy < 0 && laser_hit)
                {
                    int t2 = t_best + THROWER_REACH, part2;
                    int k = enemies_raycast(&ganados, muzzle, d_world, &t2, &part2);
                    if (k >= 0 && ganados.e[k].thrower)
                    {
                        laser_enemy = k;
                        laser_part = part2;
                        t_best = t2;
                    }
                }
                laser_head = laser_enemy >= 0 && laser_part == PART_HEAD;
                laser_ash = ash.on && ashley_raycast(&ash, muzzle, d_world, &t_best);
                laser_giant = 0;
                for (int i = 0; i < giant_count; i++)
                    if (giant_raycast(&giants[i], muzzle, d_world, &t_best))
                        laser_giant = i + 1;
                laser_gal = 0;
                for (int i = 0; i < gal_count; i++)
                    if (garrador_raycast(&gals[i], muzzle, d_world, &t_best, &laser_gal_back))
                        laser_gal = i + 1;
                if (laser_gal)
                    laser_enemy = laser_obj = -1, laser_ash = laser_giant = 0;
                laser_ver = ver.on && verdugo_raycast(&ver, muzzle, d_world, &t_best);
                if (laser_ver)
                    laser_enemy = laser_obj = -1, laser_ash = laser_giant = laser_gal = 0;
                if (laser_giant)
                    laser_enemy = laser_obj = -1, laser_ash = 0;
                if (laser_ash)
                    laser_enemy = laser_obj = -1;
                if (laser_enemy >= 0)
                    laser_obj = -1;
                if (laser_enemy >= 0 || laser_obj >= 0 || laser_ash || laser_giant || laser_gal || laser_ver || !laser_hit)
                    for (int k = 0; k < 3; k++)
                        laser_end[k] = muzzle[k] + mulf32(d_world[k], t_best);
                laser_hit |= laser_enemy >= 0 || laser_obj >= 0 || laser_ash || laser_giant || laser_gal || laser_ver;
            }
        }

#ifdef TEST_KILL_AT
        static u32 test_frame;
        if (++test_frame == TEST_KILL_AT)
            for (int i = 0; i < ganados.count; i++)
            {
                enemies_damage(&ganados, i, ENEMY_FELL);
                maybe_drop(&ganados.e[i], leon_hp, 100);
            }
#endif
#ifdef TEST_KILL_EVERY
        static u32 kill_frame;
        if (++kill_frame % TEST_KILL_EVERY == 0)
            for (int i = 0; i < ganados.count; i++)
                if (ganados.e[i].active && ganados.e[i].state < ENEMY_DYING && !ganados.e[i].thrower)
                {
                    enemies_damage(&ganados, i, ENEMY_FELL);
                    maybe_drop(&ganados.e[i], leon_hp, 100);
                }
        if (kill_frame % TEST_KILL_EVERY == 0)
        {
            for (int i = 0; i < giant_count; i++)
                giant_blast(&giants[i]);
            for (int i = 0; i < gal_count; i++)
                garrador_shot(&gals[i], ENEMY_FELL, 1);
            if (ver.on)
                verdugo_shot(&ver, ENEMY_FELL);
        }
#endif
        ganados.noise = shot_noise || (leon_anim == ANIM_RUN && react == REACT_NONE);
        shot_noise = 0;
#ifdef TEST_GOD
        enemies_update(&ganados, leon_pos, 0, &ev);
#else
        PROF(1);
        enemies_update(&ganados, leon_pos, ctrl == CTRL_LEON && free_to_act && safe_timer == 0 && clear_t < 0, &ev);
        PROF(2);
#endif
        if (ev.spotted >= 0)
            sfx_play_at(SFX_GANADO_SPOT, 110, ganados.e[ev.spotted].pos, leon_pos, leon_yaw);
        if (ev.attacking >= 0)
            sfx_play_at(SFX_GANADO_ATTACK, 120, ganados.e[ev.attacking].pos, leon_pos, leon_yaw);
        if (ev.swing >= 0)
            sfx_play_at(SFX_SWING, 100, ganados.e[ev.swing].pos, leon_pos, leon_yaw);
        int damage = 0;
        if (ev.swiped_by >= 0)
        {
            damage = armored(HIT_DAMAGE(&ganados.e[ev.swiped_by]));
            if (ganados.e[ev.swiped_by].boss)
                damage = LEON_HP;
            react = REACT_FLINCH;
            react_frame = 0;
        }
        if (ev.shot >= 0)
        {
            const Enemy *e = &ganados.e[ev.shot];
            int from[3] = { e->pos[0] + mulf32(sinLerp(e->yaw), inttof32(2)), e->feet + BOLT_HEIGHT, e->pos[2] + mulf32(cosLerp(e->yaw), inttof32(2)) };
            int at_ash = ash.on && ev.shot_at[0] == ash.pos[0] && ev.shot_at[2] == ash.pos[2];
            int to[3] = { ev.shot_at[0], at_ash ? ash.feet + inttof32(8) : leon_feet + BOLT_AIM, ev.shot_at[2] };
            bolt_fire(from, to, HIT_DAMAGE(e));
            sfx_play_at(SFX_SWING, 110, e->pos, leon_pos, leon_yaw);
        }
        {
            int ash_bd = 0;
#ifdef TEST_GOD
            const int bolt_vuln = 0;
#else
            const int bolt_vuln = ctrl == CTRL_LEON && free_to_act && safe_timer == 0 && clear_t < 0;
#endif
            int bd = bolts_update((const int[3]){ leon_pos[0], leon_feet, leon_pos[2] }, bolt_vuln,
                                  ash.on && !ash.wait ? (const int[3]){ ash.pos[0], ash.feet, ash.pos[2] } : NULL, &ash_bd);
            if (bd > 0 && !damage)
            {
                damage = armored(bd);
                react = REACT_FLINCH;
                react_frame = 0;
            }
            if (ash_bd > 0)
            {
                ashley_hit(&ash, ash_bd);
                sfx_play_at(SFX_LEON_HURT2, 120, ash.pos, leon_pos, leon_yaw);
            }
        }
        if (ev.grabbed_by >= 0)
        {
            holder = ev.grabbed_by;
            leon_yaw = ganados.e[holder].yaw + DEGREES_IN_CIRCLE / 2;
            enemies_hold_at(&ganados, holder, leon_pos, GRAB_DIST);
            react = REACT_HELD;
            react_frame = 0;
            mash = held_frames = 0;
        }
        for (int gi = 0; gi < giant_count; gi++)
        {
            Giant *gp = &giants[gi];
            for (int j = 0; j < giant_count; j++)
                if (j != gi)
                    giant_push(&giants[j], &gp->pos[0], &gp->pos[2], GIANT_RADIUS);
            GiantEvents gev;
#ifdef TEST_ELG_VIEW
            memset(&gev, 0, sizeof(gev));
#else
#ifdef TEST_GOD
            giant_update(gp, &stage_ground, leon_pos, 0, &gev);
#else
            giant_update(gp, &stage_ground, leon_pos, ctrl == CTRL_LEON && free_to_act && safe_timer == 0 && clear_t < 0, &gev);
#endif
#ifdef TEST_AUTO_CUT
            if (giant_cuttable(gp, leon_pos))
            {
                giant_cut(gp);
                sfx_play(SFX_KNIFE, 127, 64);
            }
#endif
#endif
            if (gev.stomp)
                cam_kick = floattof32(2.5);
            if (gev.hit_leon && react != REACT_DEAD)
            {
                leon_hp -= armored(gev.hit_leon);
                damage_taken += armored(gev.hit_leon);
                hurt_flash = 12;
                sfx_play(SFX_LEON_HURT2, 127, 64);
                if (leon_hp <= 0)
                {
                    leon_hp = 0;
                    if (holder >= 0)
                        enemies_release(&ganados, holder);
                    holder = -1;
                    react = REACT_DEAD;
                }
                else
                    react = REACT_FLINCH;
                react_frame = 0;
            }
        }
        for (int gi = 0; gi < gal_count; gi++)
        {
            Garrador *gp = &gals[gi];
            GarradorEvents gev;
#ifdef TEST_GAL_VIEW
            memset(&gev, 0, sizeof(gev));
#else
            if (ctrl == CTRL_LEON && react != REACT_DEAD)
                garrador_hear(gp, leon_pos, leon_anim == ANIM_RUN ? GAL_HEAR_RUN : GAL_HEAR_NEAR);
#ifdef TEST_GOD
            garrador_update(gp, &stage_ground, leon_pos, ash.on ? ash.pos : NULL, 0, &gev);
#else
            garrador_update(gp, &stage_ground, leon_pos, ash.on ? ash.pos : NULL, ctrl == CTRL_LEON && free_to_act && safe_timer == 0 && clear_t < 0, &gev);
#endif
#endif
            if (gev.roar)
                sfx_play_at(SFX_GANADO_HIT, 127, gp->pos, leon_pos, leon_yaw);
            if (gev.hit_ally)
                ashley_hit(&ash, gev.hit_ally);
            if (gev.hit_leon && react != REACT_DEAD)
            {
                leon_hp -= armored(gev.hit_leon);
                damage_taken += armored(gev.hit_leon);
                hurt_flash = 12;
                sfx_play(SFX_LEON_HURT2, 127, 64);
                if (leon_hp <= 0)
                {
                    leon_hp = 0;
                    if (holder >= 0)
                        enemies_release(&ganados, holder);
                    holder = -1;
                    react = REACT_DEAD;
                }
                else
                    react = REACT_FLINCH;
                react_frame = 0;
            }
        }
        if (ver.on)
        {
#ifdef TEST_TANK_AT
            static u32 tank_frame;
            if (++tank_frame == TEST_TANK_AT)
            {
                int best = -1;
                s64 bd = 0;
                for (int i = 0; i < objs.count; i++)
                {
                    const Obj *o = &objs.o[i];
                    s64 ox = o->pos[0] - ver.pos[0], oz = o->pos[2] - ver.pos[2];
                    if (o->kind == OBJ_BARREL && o->alive && (best < 0 || ox * ox + oz * oz < bd))
                        best = i, bd = ox * ox + oz * oz;
                }
                if (best >= 0)
                    objects_hit(&objs, best);
            }
#endif
            VerdugoEvents vev;
#ifdef TEST_VER_VIEW
            memset(&vev, 0, sizeof(vev));
#else
#ifdef TEST_GOD
            verdugo_update(&ver, &stage_ground, leon_pos, leon_feet, 0, &vev);
#else
            verdugo_update(&ver, &stage_ground, leon_pos, leon_feet, ctrl == CTRL_LEON && free_to_act && safe_timer == 0 && clear_t < 0, &vev);
#endif
#endif
            if (vev.landed)
            {
                cam_kick = floattof32(2);
                sfx_play_at(SFX_GANADO_HIT, 127, ver.pos, leon_pos, leon_yaw);
            }
            if (vev.hit_leon && react != REACT_DEAD)
            {
                leon_hp -= armored(vev.hit_leon);
                damage_taken += armored(vev.hit_leon);
                hurt_flash = 12;
                sfx_play(SFX_LEON_HURT2, 127, 64);
                if (leon_hp <= 0)
                {
                    leon_hp = 0;
                    if (holder >= 0)
                        enemies_release(&ganados, holder);
                    holder = -1;
                    react = REACT_DEAD;
                }
                else
                    react = REACT_FLINCH;
                react_frame = 0;
            }
        }
        if (ash.on)
        {
            {
                int ax = ash.pos[0] - leon_pos[0], az = ash.pos[2] - leon_pos[2];
                int ad = (int)sqrt64((u64)((s64)ax * ax + (s64)az * az));
                s64 dot = ((s64)sinLerp(leon_yaw) * ax + (s64)cosLerp(leon_yaw) * az) >> 12;
                ash.duck = aim_w > 0 && ad < inttof32(80) && dot * 10 > (s64)ad * 8;
            }
#ifndef TEST_ASH_VIEW
            if (!ash.wait && !ash.ladder && ash.state == ASH_FOLLOW && react == REACT_NONE
                && (ash.feet - leon_feet > inttof32(10) || leon_feet - ash.feet > inttof32(10)))
            {
                int hp_was = ash.hp;
                ashley_place(&ash, &stage_ground, leon_pos[0] - mulf32(sinLerp(leon_yaw), inttof32(5)) + mulf32(cosLerp(leon_yaw), inttof32(7)),
                             leon_pos[2] - mulf32(cosLerp(leon_yaw), inttof32(5)) - mulf32(sinLerp(leon_yaw), inttof32(7)), leon_feet + inttof32(4), leon_yaw);
                ash.hp = hp_was;
            }
            {
                static int ash_far_t;
                int fdx = ash.pos[0] - leon_pos[0], fdz = ash.pos[2] - leon_pos[2];
                int far = !ash.wait && !ash.ladder && ash.state == ASH_FOLLOW && react == REACT_NONE
                          && (s64)fdx * fdx + (s64)fdz * fdz > (s64)inttof32(40) * inttof32(40);
                ash_far_t = far ? ash_far_t + 1 : 0;
                if (ash_far_t > 6 * 60)
                {
                    int hp_was = ash.hp;
                    ashley_place(&ash, &stage_ground, leon_pos[0] - mulf32(sinLerp(leon_yaw), inttof32(6)), leon_pos[2] - mulf32(cosLerp(leon_yaw), inttof32(6)),
                                 leon_feet + inttof32(4), leon_yaw);
                    ash.hp = hp_was;
                    ash_far_t = 0;
                }
            }
            ashley_update(&ash, &stage_ground, leon_pos, leon_yaw);
#endif
            if (ev.ally_hit >= 0)
            {
                ashley_hit(&ash, HIT_DAMAGE(&ganados.e[ev.ally_hit]));
                sfx_play_at(SFX_LEON_HURT2, 120, ash.pos, leon_pos, leon_yaw);
            }
            if (ash.state == ASH_DOWN && mode == MODE_PLAY && ++ash_lost >= DEAD_TO_MENU)
            {
                mode = MODE_GAMEOVER;
                dead_timer = DEAD_TO_MENU;
            }
        }
        if (ev.thrown >= 0)
        {
            const Enemy *e = &ganados.e[ev.thrown];
            int from[3] = { e->pos[0], e->feet + inttof32(14), e->pos[2] };
            int to[3] = { leon_pos[0], leon_feet, leon_pos[2] };
            missiles_throw(&missiles, from, to);
#ifdef TEST_SHOW_THROW
            throws_seen++;
#endif
            sfx_play_at(SFX_SWING, 100, e->pos, leon_pos, leon_yaw);
        }
        int went = missiles_update(&missiles, &stage_ground);
        if (went >= 0)
        {
            const int *bp = missiles.m[went].pos;
            sfx_play_at(SFX_EXPLODE, 127, bp, leon_pos, leon_yaw);
            cam_kick = floattof32(2);
            int dx = leon_pos[0] - bp[0], dz = leon_pos[2] - bp[2];
#ifndef TEST_GOD
            if ((s64)dx * dx + (s64)dz * dz < (s64)MISSILE_RANGE * MISSILE_RANGE && react != REACT_DEAD && react != REACT_HELD)
            {
                leon_hp -= armored(MISSILE_DAMAGE);
                damage_taken += armored(MISSILE_DAMAGE);
                hurt_flash = 12;
                sfx_play(SFX_LEON_HURT2, 120, 64);
                if (leon_hp <= 0)
                {
                    leon_hp = 0;
                    react = REACT_DEAD;
                }
                else
                    react = REACT_FLINCH;
                react_frame = 0;
            }
#endif
            if (ash.on)
            {
                int ax = ash.pos[0] - bp[0], az = ash.pos[2] - bp[2];
                if ((s64)ax * ax + (s64)az * az < (s64)MISSILE_RANGE * MISSILE_RANGE)
                    ashley_hit(&ash, MISSILE_DAMAGE / 3);
            }
        }
        if (ev.bite >= 0)
            damage = armored(BITE_DAMAGE);
        if (damage)
        {
            leon_hp -= damage;
            damage_taken += damage;
            hurt_flash = 12;
            sfx_play(damage == armored(SWIPE_DAMAGE) && leon_hp > 0 ? SFX_LEON_HURT : SFX_LEON_HURT2, 120, 64);
            if (leon_hp <= 0)
            {
                leon_hp = 0;
                if (holder >= 0)
                    enemies_release(&ganados, holder);
                holder = -1;
                react = REACT_DEAD;
                react_frame = 0;
            }
        }

#ifdef TEST_HANG_AT
        static u32 hang_frame;
        if (++hang_frame == TEST_HANG_AT)
            for (int i = 0; i < objs.count; i++)
                if (objs.o[i].kind == OBJ_HANG)
                {
#ifdef TEST_HANG_RAY
                    const Obj *o = &objs.o[i];
                    int p[3] = { leon_pos[0], leon_feet + inttof32(16), leon_pos[2] };
                    int32_t d[3] = { o->pos[0] - p[0], o->pos[1] - inttof32(3) - p[1], o->pos[2] - p[2] };
                    normalizef32(d);
                    int t = inttof32(400);
                    if (objects_raycast(&objs, p, d, &t) == i)
#endif
                    objects_hit(&objs, i);
                }
#endif
        objs.leon = leon_pos;
        objects_update(&objs, &oev);
        rocket_boom = 0;
        if (rocket_on)
        {
            int dv[3] = { rocket_to[0] - rocket_pos[0], rocket_to[1] - rocket_pos[1], rocket_to[2] - rocket_pos[2] };
            int len = (int)sqrt64((u64)((s64)dv[0] * dv[0] + (s64)dv[1] * dv[1] + (s64)dv[2] * dv[2]));
            if (len <= ROCKET_SPEED)
            {
                rocket_on = 0;
                rocket_boom = 1;
                oev.exploded = 1;
                oev.small = 0;
                for (int k = 0; k < 3; k++)
                    oev.explode_pos[k] = rocket_to[k];
                objects_boom(&objs, rocket_to);
                if (wep == WEP_ROCKET)
                {
                    wep_owned &= ~(1 << WEP_ROCKET);
                    weapon_switch(WEP_HANDGUN, &ammo, &spare);
                    fire_frame = reload_frame = -1;
                    clip_key = -1;
                }
            }
            else
                for (int k = 0; k < 3; k++)
                    rocket_pos[k] += (int)((s64)dv[k] * ROCKET_SPEED / len);
        }
        if (oev.door >= 0)
        {
            const Obj *d = &objs.o[oev.door];
            int at[3] = { d->hinge[0], d->pos[1], d->hinge[1] };
            sfx_play_at(SFX_DOOR_KICK, 70, at, leon_pos, leon_yaw);
        }
        for (int i = 0; i < ganados.count; i++)
        {
            const Enemy *e = &ganados.e[i];
            if (!e->active || e->state >= ENEMY_HIT || e->thrower)
                continue;
            int d = objects_door_at(&objs, e->pos[0], e->pos[2], inttof32(6));
            if (d >= 0)
                objects_open_door(&objs, d, e->pos);
        }
        if (oev.exploded)
        {
            sfx_play_at(SFX_EXPLODE, 127, oev.explode_pos, leon_pos, leon_yaw);
            int blast = oev.small ? HANG_BLAST : rocket_boom ? ROCKET_BLAST : BLAST_RANGE, leon_blast = oev.small ? HANG_BLAST / 2 : BLAST_RANGE / 2;
            if (rocket_boom)
            {
                for (int g = 0; g < giant_count; g++)
                {
                    int dx = giants[g].pos[0] - oev.explode_pos[0], dz = giants[g].pos[2] - oev.explode_pos[2];
                    if ((s64)dx * dx + (s64)dz * dz < (s64)(ROCKET_BLAST + inttof32(9)) * (ROCKET_BLAST + inttof32(9)) && giant_blast(&giants[g]))
                    {
                        hits++;
                        sfx_play_at(SFX_GANADO_DOWN, 127, giants[g].pos, leon_pos, leon_yaw);
                    }
                }
                for (int g = 0; g < gal_count; g++)
                {
                    int dx = gals[g].pos[0] - oev.explode_pos[0], dz = gals[g].pos[2] - oev.explode_pos[2];
                    if ((s64)dx * dx + (s64)dz * dz < (s64)ROCKET_BLAST * ROCKET_BLAST)
                        garrador_shot(&gals[g], ENEMY_FELL, 1);
                }
                if (ver.on)
                {
                    int dx = ver.pos[0] - oev.explode_pos[0], dz = ver.pos[2] - oev.explode_pos[2];
                    if ((s64)dx * dx + (s64)dz * dz < (s64)ROCKET_BLAST * ROCKET_BLAST)
                        verdugo_shot(&ver, 5000);
                }
            }
            int level = !oev.small || (leon_feet - oev.explode_pos[1] < HANG_BLAST && oev.explode_pos[1] - leon_feet < HANG_BLAST);
            for (int g = 0; g < gal_count; g++)
                if (oev.small)
                    garrador_hear(&gals[g], oev.explode_pos, inttof32(4000));
            for (int i = 0; i < ganados.count; i++)
            {
                Enemy *e = &ganados.e[i];
                int dx = e->pos[0] - oev.explode_pos[0], dz = e->pos[2] - oev.explode_pos[2];
                int near_y = !oev.small || (e->feet - oev.explode_pos[1] < HANG_BLAST && oev.explode_pos[1] - e->feet < HANG_BLAST);
                if (near_y && e->active && e->state < ENEMY_DYING && (s64)dx * dx + (s64)dz * dz < (s64)blast * blast)
                {
                    if (!rocket_boom)
                        felled_by_blast++;
                    enemies_damage(&ganados, i, ENEMY_FELL);
                    sfx_play_at(SFX_GANADO_DOWN, 110, e->pos, leon_pos, leon_yaw);
                    maybe_drop(e, leon_hp, DROP_PERCENT);
                }
            }
            if (objs.cold && ver.on)
                verdugo_freeze(&ver, oev.explode_pos, TANK_RANGE);
            if (felled_by_blast)
                bonus_note(T_BONUS_STRAT);
            felled_by_blast = 0;
            int dx = leon_pos[0] - oev.explode_pos[0], dz = leon_pos[2] - oev.explode_pos[2];
            if (!objs.cold && level && (s64)dx * dx + (s64)dz * dz < (s64)leon_blast * leon_blast && react != REACT_DEAD)
            {
                leon_hp -= armored(BLAST_DAMAGE);
                damage_taken += armored(BLAST_DAMAGE);
                hurt_flash = 12;
                sfx_play(SFX_LEON_HURT2, 120, 64);
                if (leon_hp <= 0)
                {
                    leon_hp = 0;
                    react = REACT_DEAD;
                    react_frame = 0;
                }
                else if (react == REACT_NONE)
                {
                    react = REACT_FLINCH;
                    react_frame = 0;
                }
            }
            cam_kick = floattof32(3);
        }
        if (medal_note > 0)
            medal_note--;

        if (cur_stage != 0 && mode == MODE_PLAY)
            {
            int feet_at[3] = { leon_pos[0], leon_feet, leon_pos[2] };
            foes_events(&stage_foes[cur_stage], cur_stage, feet_at);
            foes_step(feet_at);
            if (cur_stage == ASH_STAGE_E && ash.on && ash.wait && ev_bit(ev_played, 2))
            {
                ashley_place(&ash, &stage_ground, leon_pos[0] - mulf32(sinLerp(leon_yaw), inttof32(5)) + mulf32(cosLerp(leon_yaw), inttof32(7)),
                             leon_pos[2] - mulf32(cosLerp(leon_yaw), inttof32(5)) - mulf32(sinLerp(leon_yaw), inttof32(7)), leon_feet + inttof32(4), leon_yaw);
                ash.wait = 0;
                ash_waiting = 0;
            }
        }
        else if (next_ganado < ganados.count && enemies_active_standing(&ganados) < GANADO_ACTIVE && mode == MODE_PLAY)
        {
            if (next_ganado == GANADO_FROM_DOOR)
            {
                int d = objects_door(&objs);
                const Obj *o = &objs.o[d];
                int at[2] = { (o->hinge[0] + o->end[0]) / 2, (o->hinge[1] + o->end[1]) / 2 + inttof32(2) };
                objects_open(&objs, d);
                sfx_play_at(SFX_DOOR_KICK, 110, ganados.e[next_ganado].pos, leon_pos, leon_yaw);
                enemies_activate(&ganados, next_ganado, at);
            }
            else
                enemies_activate(&ganados, next_ganado, NULL);
            next_ganado++;
        }

#ifdef TEST_KNOCK_AT
        static u32 knock_frame;
        if (++knock_frame == TEST_KNOCK_AT)
            for (int i = 0; i < DROPS_MAX; i++)
            {
#ifdef TEST_KNOCK_AHEAD
                drops.d[i].land[0] = leon_pos[0] + sinLerp(leon_yaw) * TEST_KNOCK_AHEAD;
                drops.d[i].land[1] = leon_feet;
                drops.d[i].land[2] = leon_pos[2] + cosLerp(leon_yaw) * TEST_KNOCK_AHEAD;
#endif
                drops_knock(&drops, i);
            }
#endif
        int leon_at[3] = { leon_pos[0], leon_feet, leon_pos[2] };
#ifdef TEST_ASH_ROOM
        if (play_frames == 2)
                {
                    consoleDemoInit();
                    static const s16 from[][3] = { { 122, 50, 62 }, { 120, 50, 80 }, { 110, 50, 60 }, { 64, 49, 21 }, { 129, 56, 61 }, { 90, 50, 168 } };
                    for (unsigned k = 0; k < sizeof(from) / sizeof(from[0]); k++)
                    {
                        int at[3] = { inttof32(from[k][0]), inttof32(from[k][1]), inttof32(from[k][2]) };
                        foe_flood(at);
                        int in = 0;
                        for (int c = 0; c < FOE_GRID * FOE_GRID; c++)
                            if (foe_dist[c] != 0xff && !foe_shut[c] && abs(foe_cx + (c % FOE_GRID - FOE_GRID / 2) * FOE_CELL - inttof32(177)) < inttof32(14)
                                && abs(foe_cz + (c / FOE_GRID - FOE_GRID / 2) * FOE_CELL - inttof32(70)) < inttof32(14) && foe_fy[c] > inttof32(40))
                                in++;
                        int nb[3] = { 0, 0, 0 }, ok = foe_near(k, nb);
                        printf("%3d %2d %3d: cells %4d room %3d ash %d\n near %d %d %d %d\n", from[k][0], from[k][1], from[k][2], foe_cells, in,
                               foe_reachable(177, 50, 70), ok, nb[0], nb[1], nb[2]);
                    }
                    while (1)
                        swiWaitForVBlank();
                }
#endif
#ifdef TEST_CULT_VIEW
#ifndef TEST_CULT_DIST
#define TEST_CULT_DIST 35
#endif
        if (play_frames == 2)
        {
            int slot = ganados.count - 1;
            int xyz[3] = { f32toint(leon_pos[0] + sinLerp(leon_yaw) * TEST_CULT_DIST), f32toint(leon_feet) + 2,
                           f32toint(leon_pos[2] + cosLerp(leon_yaw) * TEST_CULT_DIST) };
            enemies_respawn(&ganados, slot, xyz, leon_yaw + DEGREES_IN_CIRCLE / 2);
            enemies_set_kind(&ganados, slot, &foe_kind_cultist[TEST_CULT_VIEW]);
            enemies_alert(&ganados, slot);
        }
#endif
#ifdef TEST_PLG_VIEW
        if (play_frames == 2)
        {
            int xyz[3] = { f32toint(leon_pos[0] + sinLerp(leon_yaw) * TEST_PLG_VIEW), f32toint(leon_feet) + 2,
                           f32toint(leon_pos[2] + cosLerp(leon_yaw) * TEST_PLG_VIEW) };
            enemies_respawn(&ganados, ENEMY_MAX - 1 < ganados.count ? ENEMY_MAX - 1 : ganados.count - 1, xyz, leon_yaw + DEGREES_IN_CIRCLE / 2);
            enemies_set_kind(&ganados, ENEMY_MAX - 1 < ganados.count ? ENEMY_MAX - 1 : ganados.count - 1, &foe_kind_ganado[18]);
        }
#endif
#ifdef TEST_DROP_CODE
        if (play_frames == 400)
        {
            Enemy fake = { 0 };
            fake.pos[0] = leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(20));
            fake.pos[2] = leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(20));
            ground_height(&stage_ground, fake.pos[0], fake.pos[2], leon_feet + STEP_UP, STEP_UP, &fake.feet);
            fake.drop = TEST_DROP_CODE;
            mission_drop(&fake);
        }
#endif
        int picked = drops_update(&drops, react == REACT_DEAD ? NULL : leon_at, take_item);
        take_item = 0;
        if (picked >= 0)
        {
            sfx_play(SFX_ITEM_GET, 120, 64);
            popup_value = drops.picked_value;
            if (picked == DROP_AMMO)
            {
                popup_value = multi_ammo_gun(wep) * 1000 + MULTI_DROP;
                spare += MULTI_DROP;
            }
            else if (picked == DROP_MONEY)
                ptas += drops.picked_value;
            else if (picked == DROP_TREASURE)
            {
                ptas += drops.picked_value;
                treasures_found++;
                bonus_note(T_BONUS_JEWEL);
            }
            else if (herbs < HERBS_MAX)
                herbs++;
            popup_type = picked;
            popup = POPUP_FRAMES;
        }
        if (popup > 0)
            popup--;

        if (mode == MODE_PLAY && react != REACT_DEAD)
        {
            int ev = -1;
            for (int i = 0; i < giant_count; i++)
            {
                if (giants[i].state == GIANT_KNEEL && !kneel_seen)
                {
                    kneel_seen = 1;
                    ev = CUT_GIANT_KNEEL;
                }
                if (giants[i].state == GIANT_DEAD && !(giant_paid >> i & 1))
                {
                    giant_paid |= 1 << i;
                    const CutScene *c = cut_find(cur_stage, CUT_GIANT_DEAD);
                    if (c && c->money)
                    {
                        ptas += c->money;
                        popup_type = DROP_MONEY;
                        popup_value = c->money;
                        popup = POPUP_FRAMES;
                        sfx_play(SFX_ITEM_GET, 120, 64);
                    }
                }
            }
            if (ash.on && ash_waiting && !ash.wait)
                ev = CUT_ASHLEY;
            ash_waiting = ash.on && ash.wait;
            if (gal_count && gals[0].on && !gal_seen && abs(gals[0].pos[0] - leon_pos[0]) < GAL_CUT_NEAR
                && abs(gals[0].pos[2] - leon_pos[2]) < GAL_CUT_NEAR && abs(gals[0].feet - leon_feet) < inttof32(16))
            {
                gal_seen = 1;
                ev = CUT_GARRADOR;
            }
            if (ev >= 0 && ((ev == CUT_GIANT_KNEEL && cur_stage == 3) || (ev == CUT_ASHLEY && cur_stage == 4) || (ev == CUT_GARRADOR && cur_stage == 7)))
            {
                if (ev_scene_own(cur_stage, ev))
                    foes_scene_played(&stage_foes[cur_stage], ev);
                if (cut_start(cur_stage, ev, MODE_PLAY, CUT_AFTER_NONE, 0))
                    mode = MODE_CUTIN;
            }

            for (int g = 0; cur_stage != 0 && g < ev_gots; g++)
            {
                const EvItem *it = ev_item(ev_got[g]);
                if (!it)
                    continue;
                sfx_play(SFX_ITEM_GET, 120, 64);
                if (it->kind == EVI_TREASURE)
                {
                    ptas += it->price;
                    treasures_found++;
                bonus_note(T_BONUS_JEWEL);
                    popup_type = DROP_TREASURE;
                    popup_value = it->price;
                    popup = POPUP_FRAMES;
                }
                else if (it->kind == EVI_HERB)
                {
                    if (herbs < HERBS_MAX)
                        herbs++;
                    popup_type = DROP_HERB;
                    popup = POPUP_FRAMES;
                }
                else
                {
                    static char got_text[64];
                    static CutPage got_page;
                    static CutScene got_scene;
                    snprintf(got_text, sizeof(got_text), it->kind == EVI_FILE ? T_GOT_FILE : T_GOT_ITEM, it->name);
                    got_page = (CutPage){ -1, 0, got_text };
                    got_scene = (CutScene){ (u8)cur_stage, 255, 1, &got_page, 0 };
                    if (mode == MODE_PLAY)
                    {
                        msg = &got_scene;
                        msg_page = 0;
                        mode = MODE_MESSAGE;
                    }
                    else
                        vital_message(got_text, 180);
                }
            }
            ev_gots = 0;
            if (cur_stage != 0 && item_held(80) && item_held(81))
            {
                item_set(80, 0);
                item_set(81, 0);
                item_set(82, 1);
                vital_message(T_GOT_EMBLEM, 180);
            }
            if (cur_stage != 0 && mode == MODE_PLAY && ev_scenes > 0)
            {
                int s = ev_scene[0];
                ev_scenes--;
                memmove(ev_scene, ev_scene + 1, ev_scenes);
                const CutScene *c = cut_find(cur_stage, s);
                int pictures = 0;
                for (int k = 0; c && k < c->count; k++)
                    pictures |= c->page[k].pic >= 0;
                if (c && c->count && !pictures)
                {
                    msg = c;
                    msg_page = 0;
                    mode = MODE_MESSAGE;
                }
                else if (cut_start(cur_stage, s, MODE_PLAY, CUT_AFTER_NONE, 0))
                    mode = MODE_CUTIN;
            }
        }

#ifdef TEST_NO_CLEAR
        if (0)
#else
        if (mode == MODE_PLAY && clear_t < 0 && react != REACT_DEAD && (ganados.count > 0 || cur_stage != 0) && (FOES_ON(cur_stage) || giant_count > 0))
#endif
        {
            int all_down = (cur_stage != 0 || enemies_left(&ganados) == 0) && giants_down() && garradors_down()
                           && (!ver.on || ver.state == VER_DEAD) && (cur_stage == 0 || foes_finished(&stage_foes[cur_stage]));
            if (cur_stage != 0 && mis_flag_clear)
                all_down = (mis_flags & 1) && ev_scenes == 0;
            for (int i = 0; cur_stage == 0 && i < ganados.count; i++)
                all_down &= ganados.e[i].state == ENEMY_DEAD || ganados.e[i].thrower;
#ifdef TEST_BOSS
            all_down |= boss_phase == 0;
#endif
            if (all_down && boss_phase == 0 && cur_stage != 0)
            {
                boss_phase = 2;
                clear_t = 0;
            }
            else if (all_down && boss_phase == 0)
            {
                cha_loaded = model_load(&cha_model, cha_bin, cha_bin_size) == 0;
                if (cha_loaded)
                {
                    int slot = 0;
                    while (slot < ganados.count - 1 && ganados.e[slot].state < ENEMY_DYING && ganados.e[slot].active)
                        slot++;
                    enemies_spawn_boss(&ganados, slot, boss_spawn, DEGREES_IN_CIRCLE / 2, &cha_model, &cha_anims);
                    boss_phase = 1;
                    boss_t = 0;
                    sfx_play(SFX_GANADO_SPOT, 127, 64);
                    if (cut_start(cur_stage, CUT_CHAINSAW, MODE_PLAY, CUT_AFTER_NONE, 0))
                        mode = MODE_CUTIN;
                }
                else
                    clear_t = 0;
            }
            else if (boss_phase == 1)
            {
                int boss_down = 0;
                for (int i = 0; i < ganados.count; i++)
                    boss_down |= ganados.e[i].boss && ganados.e[i].state == ENEMY_DEAD;
                if (boss_down || ++boss_t >= BOSS_SURVIVE)
                {
                    boss_phase = 2;
                    clear_t = 0;
                }
            }
        }
#ifdef TEST_CLEAR_AT
        if (play_frames == TEST_CLEAR_AT && clear_t < 0 && mode == MODE_PLAY)
        {
            boss_phase = 2;
            clear_t = 0;
        }
#endif
        if (mode == MODE_PLAY && clear_t < 0 && react != REACT_DEAD && cur_stage == VER_STAGE
            && play_frames >= ELEVATOR_FRAMES)
        {
            clear_t = 0;
            elevator_note = ELEVATOR_NOTE;
        }
        if (elevator_note > 0 && mode == MODE_PLAY)
            elevator_note--;
        if (clear_t >= 0 && mode == MODE_PLAY)
        {
            clear_t++;
            if (clear_t >= BELL_FIRST && (clear_t - BELL_FIRST) % BELL_GAP == 0 && clear_t < BELL_FIRST + 3 * BELL_GAP)
                sfx_play(SFX_BELL, 127, 64);
            if (clear_t == CLEAR_END)
                pending = cut_start(cur_stage, CUT_CLEAR, MODE_RESULT, CUT_AFTER_NONE, 1) ? MODE_CUTIN : MODE_RESULT;
        }
        else if (mode == MODE_PLAY)
            play_frames++;
        if (obj_t > 0 && mode == MODE_PLAY)
            obj_t--;
        PROF(3);
        model_sync();
        if (stage.chunked)
        {
            while (GFX_STATUS & (1 << 27))
                ;
            stage_used = GFX_POLYGON_RAM_USAGE;
        }
#ifdef TEST_GX
        if (GFX_POLYGON_RAM_USAGE > gx_stage_poly) gx_stage_poly = GFX_POLYGON_RAM_USAGE;
        if (GFX_VERTEX_RAM_USAGE > gx_stage_vert) gx_stage_vert = GFX_VERTEX_RAM_USAGE;
#endif
        PROF(4);
        if (ctrl == CTRL_LEON && react != REACT_GIANT_CUT)
            draw_hud(leon_hp, ammo, hurt_flash, react == REACT_DEAD, popup_type, popup_value, popup);
        if (ctrl == CTRL_LEON && ash.on && !ash.wait && react != REACT_GIANT_CUT && hud_on)
            draw_ash_hud(ash.hp, ASHLEY_HP, ash.wait);
        if (scoping)
            draw_scope();
        if (ctrl == CTRL_LEON && aim_w == inttof32(1) && reload_frame < 0 && !scoping)
        {
            billboard_begin(look_eye, look_at);
            draw_line(muzzle, laser_end, RGB15(31, 2, 2), 10);
            if (laser_hit)
                draw_star(laser_end, floattof32(0.25), RGB15(31, 4, 4), 28, 11);
        }
        if (scope_was != scoping)
            set_fov(scoping ? SCOPE_FOV : CAM_FOV);
        scope_was = scoping;
        int leon_draw[3] = { leon_pos[0], leon_pos[1], leon_pos[2] };
        if ((react == REACT_CLIMB_UP || react == REACT_CLIMB_DOWN) && act_obj >= 0 && objs.o[act_obj].kind == OBJ_LADDER)
        {
            const Obj *l = &objs.o[act_obj];
            int reach = l->end[0] - inttof32(3);
            int len = inttof32(leon_anims.frames[react_clip[react]]);
            int t = len > 0 ? divf32(react_frame, len) : inttof32(1);
            t = t > inttof32(1) ? inttof32(1) : t;
            if (react == REACT_CLIMB_DOWN)
                t = inttof32(1) - t;
            if (reach < CLIMB_BACK)
            {
                int shift = mulf32(CLIMB_BACK - (reach > 0 ? reach : 0), t);
                leon_draw[0] -= mulf32(l->dir[0], shift);
                leon_draw[2] -= mulf32(l->dir[1], shift);
            }
        }
        if (leon_anim != ANIM_OFF && !scoping)
        {
            model_draw(&leon, leon_draw, leon_yaw, cull_modes[cull] | POLY_ID(2) | POLY_FOG | (leon_fade < 31 ? POLY_ALPHA(leon_fade) : 0), (const int (*)[12])leon_pose);
            int in_hand = (gun_state == GUN_READY || (drawing && draw_frame >= DRAW_GRAB) || aim_w >= inttof32(1) / 2 || reload_frame >= 0) && !knifing;
            if (knifing)
                model_draw_held(&knife_model, leon_pos, leon_yaw, cull_modes[cull] | POLY_ID(3) | POLY_FOG, leon_pose[KNIFE_HAND]);
            if (in_hand)
                model_draw_held(&gun, leon_pos, leon_yaw, cull_modes[cull] | POLY_ID(3) | POLY_FOG, leon_pose[HAND_NODE]);
        }

        enemies_draw(&ganados, cull_modes[cull] | POLY_ID(4) | POLY_FOG, look_eye, enemy_show);
        if (ash.on)
            ashley_draw(&ash, cull_modes[cull] | POLY_ID(6) | POLY_FOG);
        for (int i = 0; i < giant_count; i++)
            giant_draw(&giants[i], cull_modes[cull] | POLY_ID(7) | POLY_FOG);
        for (int i = 0; i < gal_count; i++)
            garrador_draw(&gals[i], cull_modes[cull] | POLY_ID(7) | POLY_FOG);
        if (ver.on)
            verdugo_draw(&ver, cull_modes[cull] | POLY_ID(7) | POLY_FOG);
        objects_draw(&objs, look_eye, cull_modes[cull] | POLY_ID(5) | POLY_FOG);
        bolts_draw();
        billboard_begin(look_eye, look_at);
        objects_draw_fx(&objs);
        missiles_draw(&missiles);
        if (ver.on)
            verdugo_draw_fx(&ver, &spark);
        drops.ammo_gun = multi_ammo_gun(wep);
        drops_draw(&drops);

        if (rocket_on)
        {
            draw_star(rocket_pos, floattof32(0.8), RGB15(31, 24, 10), 26, 12);
            draw_star(rocket_pos, floattof32(0.4), RGB15(31, 31, 26), 30, 13);
        }
        if (flash_timer > 0)
        {
            draw_star(muzzle, floattof32(0.9) - flash_timer * 0, RGB15(31, 27, 12), 26, 12);
            draw_star(muzzle, floattof32(0.45), RGB15(31, 31, 28), 30, 13);
            flash_timer--;
        }
        if (impact_timer > 0)
        {
            draw_star(impact_pos, floattof32(0.15) * impact_timer, impact_blood ? RGB15(20, 0, 0) : RGB15(31, 22, 10),
                      2 + impact_timer * 2, 14);
            impact_timer--;
        }
        if (ctrl == CTRL_LEON)
        {
            if (mode == MODE_PLAY)
            {
                text_hide(SPARE_LINE);
                bonus_draw();
                if (popup > 0)
                {
                    char money[24];
                    if (popup_type == DROP_AMMO)
                        snprintf(money, sizeof(money), T_PICK_AMMO, popup_value % 1000);
                    else
                        snprintf(money, sizeof(money), popup_type == DROP_TREASURE ? T_PICK_TREASURE : T_PICK_MONEY, popup_value);
                    const char *s = popup_type == DROP_HERB ? T_PICK_HERB : money;
                    int rise = popup > POPUP_FRAMES - 8 ? (popup - (POPUP_FRAMES - 8)) * 2 : 0;
                    text_line(POPUP_LINE, 64, 135 + rise, TEXT_WHITE, s);
                }
                else
                    text_hide(POPUP_LINE);
                if (clear_t >= CLEAR_TEXT_AT)
                    text_big(CLEAR_LINE, 128 - text_width(T_COMPLETE), 70, TEXT_GOLD, T_COMPLETE);
                if (boss_phase == 1 && boss_t < BOSS_LINES && !cut_find(cur_stage, CUT_CHAINSAW))
                {
                    text_line(OBJ_TITLE_LINE, 128 - text_width(T_BOSS_1) / 2, 70, TEXT_WHITE, T_BOSS_1);
                    text_line(OBJ_LINE, 128 - text_width(T_BOSS_1B) / 2, 84, TEXT_WHITE, T_BOSS_1B);
                    if (boss_t > BOSS_LINES / 2)
                        text_line(OBJ_HINT_LINE, 128 - text_width(T_BOSS_2) / 2, 102, TEXT_GOLD, T_BOSS_2);
                    else
                        text_hide(OBJ_HINT_LINE);
                }
                else if (obj_t > 0 && clear_t < CLEAR_TEXT_AT)
                {
                    char mt[24];
                    snprintf(mt, sizeof(mt), T_MISSION, stage_no(cur_stage));
                    static const char *const objectives[STAGE_COUNT] = { T_OBJ_1, T_OBJ_2, T_OBJ_3, T_OBJ_4, T_OBJ_5, T_OBJ_6, T_OBJ_7, T_OBJ_8, T_OBJ_9, T_OBJ_10, T_OBJ_11, T_OBJ_N };
                    const char *ob = objectives[cur_stage];
                    text_big(OBJ_TITLE_LINE, 128 - text_width(mt), 46, TEXT_GOLD, mt);
                    text_line(OBJ_LINE, 128 - text_width(ob) / 2, 82, TEXT_WHITE, ob);
                    if (cur_stage == VER_STAGE)
                        text_line(OBJ_HINT_LINE, 128 - text_width(T_ELEV_WAIT) / 2, 102, TEXT_GOLD, T_ELEV_WAIT);
                    else
                        text_hide(OBJ_HINT_LINE);
                }
                else if (elevator_note > 0)
                {
                    text_hide(OBJ_TITLE_LINE);
                    text_hide(OBJ_HINT_LINE);
                    text_line(OBJ_LINE, 128 - text_width(T_ELEV_HERE) / 2, 110, TEXT_WHITE, T_ELEV_HERE);
                }
                else
                {
                    text_hide(OBJ_TITLE_LINE);
                    text_hide(OBJ_LINE);
                    text_hide(OBJ_HINT_LINE);
                }
                int which, act = react == REACT_NONE && aim_w == 0 && !knifing ? objects_action(&objs, leon_pos, leon_feet, leon_yaw, &which) : ACT_NONE;
                static const char *const prompts[] = { "", T_ACT_GATE, T_ACT_CRATE, T_ACT_UP, T_ACT_DOWN, T_ACT_DOOR };
                if (react == REACT_GIANT_CUT)
                    text_hide(PROMPT_LINE);
                else if (react == REACT_HELD)
                    text_line(PROMPT_LINE, 128 - text_width(T_ACT_STRUGGLE) / 2, 150, (tick / 8) % 2 ? TEXT_GOLD : TEXT_WHITE, T_ACT_STRUGGLE);
                else if (react == REACT_NONE && aim_w == 0 && jump_at_hand(cur_stage, leon_pos, leon_feet, leon_yaw))
                    text_line(PROMPT_LINE, 128 - text_width(T_ACT_JUMP) / 2, 150, TEXT_WHITE, T_ACT_JUMP);
                else if (react == REACT_NONE && aim_w == 0 && giant_at_hand(leon_pos))
                    text_line(PROMPT_LINE, 128 - text_width(T_ACT_CUT) / 2, 150, TEXT_WHITE, T_ACT_CUT);
                else if (react == REACT_NONE && aim_w == 0 && !knifing && enemies_kickable(&ganados, leon_pos, leon_yaw) >= 0)
                    text_line(PROMPT_LINE, 128 - text_width(T_ACT_KICK) / 2, 150, TEXT_WHITE, T_ACT_KICK);
                else if (react == REACT_NONE && aim_w == 0 && !knifing && cur_stage != 0 && ev_button >= 0 && clear_t < 0
                         && act != ACT_LADDER_UP && act != ACT_LADDER_DOWN)
                {
                    const char *s = stage_foes[cur_stage].event[ev_button].ctg == EV_POINT ? T_ACT_EXAMINE : T_ACT_PICK;
                    text_line(PROMPT_LINE, 128 - text_width(s) / 2, 150, TEXT_WHITE, s);
                }
                else if (react == REACT_NONE && aim_w == 0 && !knifing && drops_in_reach(&drops, (const int[3]){ leon_pos[0], leon_feet, leon_pos[2] }) >= 0)
                    text_line(PROMPT_LINE, 128 - text_width(T_ACT_PICK) / 2, 150, TEXT_WHITE, T_ACT_PICK);
                else if (act != ACT_NONE)
                    text_line(PROMPT_LINE, 128 - text_width(prompts[act]) / 2, 150, TEXT_WHITE, prompts[act]);
                else
                    text_hide(PROMPT_LINE);
                if (medal_note > 0)
                {
                    char m[48];
                    if (objs.medals_hit == objs.medals)
                        snprintf(m, sizeof(m), T_MEDAL_ALL, MEDAL_REWARD);
                    else
                        snprintf(m, sizeof(m), T_MEDAL, objs.medals_hit, objs.medals);
                    text_line(MEDAL_LINE, 128 - text_width(m) / 2, 24, TEXT_GOLD, m);
                }
                else
                    text_hide(MEDAL_LINE);
            }
        }
        if (hurt_flash > 0)
            hurt_flash--;

        PROF(5);
        model_sync();
#ifdef TEST_GX
        {
            while (GFX_STATUS & (1 << 27))
                ;
            int pc = GFX_POLYGON_RAM_USAGE, vc = GFX_VERTEX_RAM_USAGE;
            if (pc > gx_poly_peak) gx_poly_peak = pc;
            if (vc > gx_vert_peak) gx_vert_peak = vc;
            u32 st = GFX_CONTROL;
            if (st & (1 << 12)) gx_underflows++;
            int rd = *(vu8 *)0x04000320;
            if (rd < gx_rdlines_min) gx_rdlines_min = rd;
            if (st & (1 << 13)) gx_overflows++;
            GFX_CONTROL = st | (1 << 12) | (1 << 13);
        }
#endif
        if (stage.chunked && mode == MODE_PLAY)
        {
            while (GFX_STATUS & (1 << 27))
                ;
            int used = GFX_POLYGON_RAM_USAGE;
            if (used >= POLY_HIGH)
            {
                if ((stage_budget <= POLY_STAGE_MIN || stage_used < POLY_STAGE_MIN) && enemy_show > ENEMY_SHOW_MIN)
                    enemy_show--;
                stage_budget -= (used - POLY_LOW) / 2 + 20;
            }
            else if (used < POLY_LOW)
            {
                if (enemy_show < ENEMY_MAX && used < POLY_LOW - 300)
                    enemy_show++;
                else
                    stage_budget += 30;
            }
            if (stage_budget < POLY_STAGE_MIN)
                stage_budget = POLY_STAGE_MIN;
        }
        if (mode == MODE_PLAY)
        {
            static u32 last_vb;
            static int frames_n, slow_n;
            frames_n++;
            slow_n += vblank_count - last_vb > 1;
            last_vb = vblank_count;
            if (frames_n >= 30)
            {
                if (slow_n >= 15)
                {
                    time_cap = time_cap - 120 < TIME_CAP_MIN ? TIME_CAP_MIN : time_cap - 120;
                    if (enemy_cap > ENEMY_SHOW_MIN + 2)
                        enemy_cap--;
                }
                else if (slow_n == 0)
                {
                    time_cap = time_cap + 40 > 4000 ? 4000 : time_cap + 40;
                    if (enemy_cap < ENEMY_MAX)
                        enemy_cap++;
                }
                frames_n = slow_n = 0;
            }
            if (stage.chunked && stage_budget > time_cap)
                stage_budget = time_cap;
            if (enemy_show > enemy_cap)
                enemy_show = enemy_cap;
        }
        glFlush(0);
        swiWaitForVBlank();
        PROF(6);
        text_commit();
        vital_commit();

        frames++;
        if (vblank_count - fps_mark >= 60)
        {
            fps = frames;
            frames = 0;
            fps_mark = vblank_count;
        }

#ifdef TEST_INF_AMMO
        if (spare < 20) spare = 20;
#endif
        Vital v = { leon_hp, LEON_HP, ammo, MAG_SIZE, spare, herbs, HERB_HEAL, (int)fps, weapons[wep].name, { 0, 0 }, { 0, 0 },
                    cur_stage, leon_pos[0], leon_pos[2], leon_yaw, ash.on, ash.pos[0], ash.pos[2], bottom_items,
                    ptas, objs.medals, objs.medals_hit };
        item_icon(weapons[wep].item, &v.gun_icon[0], &v.gun_icon[1]);
        item_icon(AMMO_ITEM(wep), &v.ammo_icon[0], &v.ammo_icon[1]);
#ifdef TEST_SHOW_FOES
        v.fps = foe_entered * 100 + foe_groups;
        v.herbs = foes_standing();
#ifdef TEST_SHOW_TIMER
        v.herbs = mis_timer / 60;
#endif
#ifdef TEST_SHOW_VB
        v.herbs = vblank_count / 60;
#endif
        {
            int best = 9999;
            for (int i = 0; i < ganados.count; i++)
                if (ganados.e[i].active && ganados.e[i].state < ENEMY_DYING)
                {
                    int d = f32toint(sqrt64((u64)((s64)(ganados.e[i].pos[0] - leon_pos[0]) * (ganados.e[i].pos[0] - leon_pos[0]) + (s64)(ganados.e[i].pos[2] - leon_pos[2]) * (ganados.e[i].pos[2] - leon_pos[2]))));
                    if (d < best)
                    {
                        best = d;
                        v.ammo = f32toint(ganados.e[i].pos[0]);
                        v.spare = f32toint(ganados.e[i].pos[2]);
                    }
                }
            v.mag = best;
        }
#ifdef TEST_SHOW_TIMER
        v.ammo = 0;
        for (int e = 6; e <= 10; e++)
            v.ammo = v.ammo * 10 + ev_bit(ev_played, e) + 2 * ev_bit(ev_deleted, e);
#endif
#endif
#ifdef TEST_SHOW_HEAP
        if (play_frames == 120)
        {
            int lo = 0, hi = 4096;
            while (lo < hi)
            {
                int mid = (lo + hi + 1) / 2;
                void *p = malloc((size_t)mid * 1024);
                if (p) { free(p); lo = mid; } else hi = mid - 1;
            }
            heap_kb = lo;
        }
        v.fps = heap_kb / 10;
#endif
#ifdef TEST_SHOW_KNIFE
        v.fps = knife_probe;
        v.spare = v_knife2;
#endif
#ifdef TEST_SHOW_TILT
        v.fps = aim_tilt * 100 / 4096;
#endif
#ifdef TEST_SHOW_REACT
        {
            static int grabs, was_held;
            grabs += react == REACT_HELD && !was_held;
            was_held = react == REACT_HELD;
            v.fps = react * 100 + grabs;
        }
#endif
#ifdef TEST_PROF
        v.hp = prof_peak[1]; v.ammo = prof_peak[2]; v.spare = prof_peak[3]; v.herbs = prof_peak[4]; v.fps = prof_peak[5]; v.mag = prof_peak[6];
        if (tick % 60 == 0)
            memset(prof_peak, 0, sizeof(prof_peak));
#endif
#ifdef TEST_GX
        v.fps = gx_poly_peak; v.spare = gx_vert_peak; v.herbs = gx_overflows * 100 + gx_underflows; v.ammo = gx_stage_poly; v.mag = enemy_show;
        if (tick % 60 == 0)
            gx_poly_peak = gx_vert_peak = gx_overflows = gx_underflows = gx_stage_poly = gx_stage_vert = 0, gx_rdlines_min = 99;
#endif
#ifdef TEST_ASH_VIEW
        v.fps = ash.clip;
#endif
#ifdef TEST_ELG_VIEW
        v.fps = giant.clip;
#endif
#ifdef TEST_GAL_VIEW
        v.fps = gal.clip;
#endif
#ifdef TEST_VER_VIEW
        v.fps = ver.clip;
#endif
#ifdef TEST_SHOW_VER
        v.fps = ver.hp / 10 * 10 + ver.state;
        {
            int tanks = 0;
            for (int i = 0; i < objs.count; i++)
                tanks += objs.o[i].kind == OBJ_BARREL && objs.o[i].alive;
            v.spare = tanks;
#ifdef TEST_SHOW_TANKS
            v.fps = tanks * 100 + ver.state * 10 + (ver.hp * 9 / 20000);
#endif
        }
#endif
        if (wep_note > 0 && mode == MODE_PLAY)
        {
            if (--wep_note)
                text_line(WEP_LINE, 256 - 8 - text_width(weapons[wep].name), 8, TEXT_GOLD, weapons[wep].name);
            else
                text_hide(WEP_LINE);
        }
#ifdef AUTOPILOT
        if (giant_count)
        {
            int tx = giants[0].pos[0] - leon_pos[0], tz = giants[0].pos[2] - leon_pos[2];
            int want = angle_to(tx, tz);
            ap_aim_err = (s16)(want - leon_yaw);
        }
#endif
#ifdef TEST_SHOW_GIANT
        {
            char gs[56];
            snprintf(gs, sizeof(gs), "s%d c%d h%d L%d g%d o%d r%d w%d d%d", giant.state, giant.cuts, giant.hp, giant.life, laser_giant, laser_obj, laser_drop, laser_hit,
                     (int)(sqrt64((u64)((s64)(giant.pos[0] - leon_pos[0]) * (giant.pos[0] - leon_pos[0]) + (s64)(giant.pos[2] - leon_pos[2]) * (giant.pos[2] - leon_pos[2]))) >> 12));
            text_line(0, 4, 4, TEXT_GOLD, gs);
        }
#endif
#ifdef TEST_SHOW_POS
        {
            char pos_s[32];
            snprintf(pos_s, sizeof(pos_s), "%d %d %d r%d a%d %d", leon_pos[0] >> 12, leon_feet >> 12, leon_pos[2] >> 12, react, ash.ladder, ash.feet >> 12); snprintf(pos_s + strlen(pos_s), 12, " %d,%d", ash.pos[0] >> 12, ash.pos[2] >> 12);
            text_line(0, 4, 4, TEXT_GOLD, pos_s);
#ifdef TEST_SHOW_KNIFE
            {
                int bi = -1, bd = 1 << 30;
                for (int i = 0; i < objs.count; i++)
                    if (objs.o[i].kind == OBJ_CRATE)
                    {
                        int d = abs(objs.o[i].pos[0] - leon_pos[0]) + abs(objs.o[i].pos[2] - leon_pos[2]);
                        if (d < bd) { bd = d; bi = i; }
                    }
                if (bi >= 0)
                {
                    snprintf(pos_s, sizeof(pos_s), "k%d %d %d %d a%d p%d %d", bi, objs.o[bi].pos[0] >> 12, objs.o[bi].pos[1] >> 12, objs.o[bi].pos[2] >> 12, objs.o[bi].alive, knife_probe, v_knife2);
                    text_line(2, 4, 32, TEXT_GOLD, pos_s);
                }
            }
#endif
#ifdef TEST_SHOW_CAM
            snprintf(pos_s, sizeof(pos_s), "c %d %d %d y%d f%d", follow_eye[0] >> 12, follow_eye[1] >> 12, follow_eye[2] >> 12, leon_yaw * 360 / DEGREES_IN_CIRCLE, leon_fade);
            text_line(1, 4, 18, TEXT_GOLD, pos_s);
            snprintf(pos_s, sizeof(pos_s), "h %d %d %d", knife_hand[0] * 10 >> 12, knife_hand[1] * 10 >> 12, knife_hand[2] * 10 >> 12);
            text_line(2, 4, 32, TEXT_GOLD, pos_s);
#endif
        }
#endif
#ifdef TEST_SHOW_DROP
        for (int i = 0; i < DROPS_MAX; i++)
            if (drops.d[i].active && drops.d[i].type == DROP_TREASURE)
            {
                int dx = (drops.d[i].pos[0] - leon_pos[0]) >> 12, dz = (drops.d[i].pos[2] - leon_pos[2]) >> 12;
                v.fps = (int)sqrtf((float)(dx * dx + dz * dz)) * 10 + drops.d[i].aloft;
                v.spare = (drops.d[i].pos[1] - leon_feet) >> 12;
                break;
            }
#endif
#ifdef TEST_SHOW_GAL
        v.fps = gal.hp * 10 + gal.state;
        v.spare = laser_gal * 10 + laser_gal_back;
#endif
#ifdef TEST_SHOW_THROW
        {
            int fl = 0, th = -1;
            for (int i = 0; i < MISSILES_MAX; i++) fl += missiles.m[i].t >= 0;
            for (int i = 0; i < ganados.count; i++) if (ganados.e[i].thrower) { th = i; break; }
            v.fps = throws_seen * 100 + (th >= 0 ? ganados.e[th].state * 10 : 90) + fl;
            v.spare = th >= 0 ? ganados.e[th].cooldown : -1;
            v.herbs = th >= 0 ? ganados.e[th].active : -1;
        }
#endif
#ifdef TEST_SHOW_SPOTTED
        {
            int seen = 0, in = 0;
            for (int i = 0; i < ganados.count; i++)
                if (ganados.e[i].active && ganados.e[i].state < ENEMY_DYING)
                {
                    in++;
                    seen += ganados.e[i].spotted;
                }
            v.fps = seen * 100 + in;
        }
#endif
#ifdef TEST_SHOW_HP
        v.fps = test_shot_foe >= 0 ? ganados.e[test_shot_foe].hp : 0;
        v.spare = test_shots;
        v.herbs = test_staggers;
        v.mag = test_downs;
#endif
#ifdef TEST_SHOW_GIANT
        v.fps = giant.hp * 100 + giant.state * 10 + giant.cuts;
        v.herbs = giant_count;
        v.spare = giants[1].hp * 100 + giants[1].state * 10 + giants[1].cuts;
#endif
#ifdef TEST_KNIFE_VIEW
        v.fps = (int)((vblank_count / 120) % 7);
#endif
#ifdef TEST_CAM_JUMPS
        v.fps = cam_jumps * 1000 + knife_w * 100 / 4096;
        v.spare = cam_near_min;
        v.ammo = cam_xfades;
#endif
        int act = vital_update(&v, down, tick, ctrl == CTRL_LEON && mode == MODE_PLAY && react != REACT_DEAD);
        if (act == VITAL_SELECT)
            sfx_play(SFX_MENU, 70, 64);
        else if (act == VITAL_CASE)
        {
            if (mode == MODE_PLAY && react == REACT_NONE && ctrl == CTRL_LEON)
            {
                sfx_play(SFX_MENU, 70, 64);
                mode = MODE_STATUS;
                st_sel = st_top = 0;
                st_cmd = -1;
            }
        }
        else if (act == VITAL_USE)
        {
            if (herbs == 0)
            {
                sfx_play(SFX_EMPTY, 110, 64);
                vital_message(T_NONE_LEFT, 90);
            }
            else if (leon_hp >= LEON_HP)
            {
                sfx_play(SFX_EMPTY, 110, 64);
                vital_message(T_FULL, 90);
            }
            else
            {
                herbs--;
                leon_hp = leon_hp + HERB_HEAL > LEON_HP ? LEON_HP : leon_hp + HERB_HEAL;
                sfx_play(SFX_ITEM_GET, 110, 64);
                vital_message(T_USED, 90);
            }
        }
    }
    return 0;
}
