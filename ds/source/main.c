// RE4 Zeebo -> DS demo: Stage 1 (village) + Leon.
//
// Screens: title (menu: start / controls) -> game; START in the game pauses (resume / start
// over / back to the title); when Leon dies, a game over menu (continue / back to the title).
//
// SELECT opens the status screen (the weapon list). In the game, two modes, switched with
// L + SELECT:
//
// Control Leon (default), RE4-style tank controls with a follow camera:
//   Up                walk forward           B + Up            run
//   Down              walk backward          Left / Right      turn
//   R (hold)          aim the handgun (laser sight); Up/Down tilt the aim, Left/Right turn slowly
//   A (while aiming)  fire                   Y                 reload (also automatic when empty)
//   X                 draw the handgun into the ready stance / put it back in the holster
//   touch camera pad  look around Leon (bottom screen; springs back when released)
//   A / B / X / Y     mash to break free when a Ganado has hold of him
//   START             pause menu             L                 item select / vital screen
//
// Free camera:
//   D-pad up/down     move forward/back      D-pad left/right  turn
//   L / R             strafe                 X / B             look up/down
//   Y / A             move down/up           touch + drag      look around
//   START             Leon: idle / walk / run (around a circle) / hidden
//
// Both: Y + SELECT    culling: back/front/off
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

// Full-screen pictures (title, stage select, results) are 64 KB textures, and next to the
// stage's textures VRAM has room for one at a time: they share this slot.
enum { PIC_TITLE, PIC_STAGESEL, PIC_RESULT };
static Sheet picture;
static int picture_id = -1;

// The stage on show (0..STAGE_COUNT-1): its model and ground, read from NitroFS into buffers
// that stay allocated while it is loaded (the model's draw lists and the ground point into them).
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

// Loads stage i (frees the one before). Returns 0 on success.
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

// Sky and fog colour (the haze), 8-bit RGB.
static void set_haze(const u8 c[3])
{
    glClearColor(c[0] >> 3, c[1] >> 3, c[2] >> 3, 31);
    glFogColor(c[0] >> 3, c[1] >> 3, c[2] >> 3, 31);
}

// The chainsaw man's texture (32 KB) shares texture VRAM with the menu pictures: he is
// loaded only while on the stage, and the picture is dropped during play.
static Model cha_model;
static AnimSet cha_anims;
static int cha_loaded;

static void cha_unload(void)
{
    if (cha_loaded)
        model_free(&cha_model);
    cha_loaded = 0;
}

// Ashley, in stages 5, 6, 7 and 8 (ASH_STAGE_A..D, 0-based; in the Zeebo video she is at Leon's
// side through the castle stage with the catapults and past the Garrador's cell); her model
// is loaded only then.
#define ASH_STAGE_A    4
#define ASH_STAGE_B    5
#define ASH_STAGE_C    6
#define ASH_STAGE_D    7

// The castle (stages 7-12, from CASTLE_FROM, 0-based): its enemies are the cultists (ch_jya,
// from the stage 6 archive: the Ganados' skeleton and clip order), read from NitroFS then.
#define CASTLE_FROM    6   // (mission 6 is the cabin siege in the village: Ganados)

// Stages 2-12: how far an enemy sees Leon in front of it (it hears him closer, any way).
#ifndef ENEMY_SIGHT
#define ENEMY_SIGHT    inttof32(150)
#endif
static Model jya_model;
static AnimSet jya_anims, jya_w0;
static void *jya_buf[3];
static int jya_loaded;

static void jya_unload(void)
{
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
    return 0;
}

// Thrown missiles (stage 2's dynamite, the castle's burning rocks).
#define MISSILE_RANGE  inttof32(14)      // a blast hurts within this distance
#define MISSILE_DAMAGE 20               // (several throwers at once: 30 a blast was death in two volleys)
static Missiles missiles;
#ifdef TEST_SHOW_THROW
static int throws_seen;
#endif
#define ASH_SHOT_DAMAGE 25
static Ashley ash;
// El Gigante: one in stage 4, two in stage 11 (0-based 3 and 10; mission 11, "A Fornalha":
// "dois El Gigantes").
#define GIANT_STAGE_A  3
#define GIANT_STAGE_B  10
#ifndef GIANT_AHEAD
#define GIANT_AHEAD    70                // the first starts this far ahead of Leon (units)
#endif
#define GIANT_AHEAD_U  inttof32(GIANT_AHEAD)
#define GIANT_MAX      2
#ifndef GIANT2_DEG
#define GIANT2_DEG     40                   // the second: this many degrees to Leon's left...
#endif
#define GIANT2_FARTHER inttof32(30)         // ...and this much farther off
#define GIANT_RADIUS   inttof32(9)          // (giant.c's body radius: they keep apart)
static Giant giants[GIANT_MAX];
static int giant_count;
#define giant giants[0]   // (test displays: the first)
static int giants_down(void)
{
    for (int i = 0; i < giant_count; i++)
        if (giants[i].state != GIANT_DEAD)
            return 0;
    return 1;
}
// The giant whose parasite Leon can cut from where he stands, or NULL.
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

// The Garradors, one in stage 8 and two in stage 9 (0-based 7, 8): blind, they go for noises.
// Shots carry everywhere; Leon running is heard within GAL_HEAR_RUN, walking only right beside
// one (GAL_HEAR_NEAR). Test builds: -DGAL_AHEAD=n puts them n units ahead of Leon instead.
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
#define gal gals[0]   // (test displays: the first)

// The Verdugo, in stage 12 (0-based 11), with liquid nitrogen tanks (the barrel model, read
// in for that stage only) around where Leon starts: one shot open near him freezes him.
#define VER_STAGE      11
#ifndef VER_AHEAD
#define VER_AHEAD      50                // he starts this far ahead of Leon (units)
#endif
#define TANK_RANGE     inttof32(34)      // a tank's cold reaches this far (22 needed him all but on it)
// Stage 12 is the game's only story mission against the clock: the elevator takes "about 4
// minutes" (mis_data_s12's note) and its arrival ends the mission, Verdugo down or not. As in
// the Zeebo video, no count is shown on screen.
#ifndef ELEVATOR_FRAMES                  // test builds: -DELEVATOR_FRAMES=n for a shorter wait
#define ELEVATOR_FRAMES (240 * 60)
#endif
#define ELEVATOR_NOTE  150               // frames "The elevator has arrived." stays up
static Verdugo ver;
static Model barrel_model;   // (with the other gimmick models below)
static int barrel_loaded;

static void barrel_unload(void)
{
    if (barrel_loaded)
        model_free(&barrel_model);
    barrel_loaded = 0;
}

#ifdef TEST_PROF   // test builds: time spent in parts of the frame (see the vital screen)
static u32 prof_mark, prof_peak[8];
#define PROF(i) do { u32 t_ = cpuGetTiming(); u32 d_ = (t_ - prof_mark) / 3351; if (d_ > prof_peak[i]) prof_peak[i] = d_; prof_mark = t_; } while (0)
#else
#define PROF(i) ((void)0)
#endif

static void gimmicks_free(void);
static void use_picture(int id)
{
    static const void *const data[] = { title_bin, stagesel_bin, result_bin };
    static const size_t size[] = { title_bin_size, stagesel_bin_size, result_bin_size };
    if (id == picture_id)
        return;
    cha_unload();
    ashley_unload(&ash);
    for (int i = GIANT_MAX - 1; i >= 0; i--)   // (the shared ones first)
        giant_unload(&giants[i]);
    for (int i = GAL_MAX - 1; i >= 0; i--)
        garrador_unload(&gals[i]);
    verdugo_unload(&ver);
    barrel_unload();
    gimmicks_free();
    jya_unload();
    sheet_free(&picture);
    picture_id = sheet_load(&picture, data[id], size[id]) == 0 ? id : -1;
}
static AnimSet leon_anims, leon_gun, ganado_anims, ganado_w0, ganado_w1;
static Model sickle_model, axe_model, fork_model;   // the Ganados' weapons (eqp_wep_all #14, #10, #11)
// Leon's knife (eqp_wep_all #0) and its animations (ch_leon_w00.mcas: #2 the ready stance,
// crouched; #3 a low slash, for a crate at his feet; #1 a slash at chest height). As with the
// gun, the view moves in over his shoulder and Up/Down tilt it: tilted up he slashes high.
static Model knife_model;
static AnimSet leon_knife;
#ifndef KNIFE_SLASH
#define KNIFE_SLASH   3
#endif
#define KNIFE_READY   2
#define KNIFE_HIGH    1
#define KNIFE_HIGH_TILT floattof32(0.3)  // tilted up past this: the high slash
#define KNIFE_RANGE   inttof32(8)
#define KNIFE_REACH   inttof32(14)      // the blade's reach along the view (from his chest)
#define KNIFE_PITCH   degreesToAngle(55) // the view (and his upper body) at full tilt up / down
#define KNIFE_CHEST   inttof32(13)
#define KNIFE_LEAN_UP   degreesToAngle(35)   // his upper body at full tilt (the arms' high slash does the rest)
#define KNIFE_LEAN_DOWN degreesToAngle(33)
#define KNIFE_CAM_DROP  inttof32(7)          // the view comes down this much at full tilt up
#define KNIFE_DAMAGE  1
#ifndef KNIFE_HAND
#define KNIFE_HAND    14                // the knife is in his right hand (the one that slashes)
#endif
static Enemies ganados;
static Model door_model, ladder_model, barrel_model, crate_model, medal_model;
static Model bell_model, tank_model;   // fix_all #9, #6: the mission files' bells, stage 12's tanks
static Model hang_model;               // fix_all #5: the canister on chains (kind 05, stage 8)
static int door_loaded, bell_loaded, tank_loaded, hang_loaded;   // (read in only for the stages that have them)

// Loads (want) or frees (!want) a gimmick model; returns whether it is loaded.
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

// The villagers of mission 1 (world x, y, z), in a group on the open ground ahead of Leon's
// start. (The game's own points, mis_data_s01.bin, are spread over the whole village, roofs
// and walkways included; the Ganados here only notice Leon within SIGHT, so most were never
// found.) At most GANADO_ACTIVE are in the stage at once (polygon budget); the rest come in
// at their points, in this order, as others fall.
static const int ganado_spawn[][3] = {
    { 292, 0, 340 }, { 318, 0, 352 }, { 276, 0, 372 }, { 330, 0, 384 }, { 300, 0, 404 },
    { 262, 0, 350 }, { 344, 0, 362 }, { 284, 0, 420 }, { 316, 0, 428 }, { 300, 0, 320 } };
#define GANADO_COUNT ((int)(sizeof(ganado_spawn) / sizeof(ganado_spawn[0])))

// Stage 1 is the game's first narrative-mode mission (mis_data_s01): down every villager,
// then the chainsaw man, then the bell; its results go back to the stage select.
#ifndef OBJ_FRAMES                       // (test builds: longer, to catch it in a capture)
#define OBJ_FRAMES     240               // the mission's objective on screen at its start
#endif
#ifndef BOSS_SURVIVE
#define BOSS_SURVIVE   (90 * 60)         // the bell rings this long after he comes...
#endif
#ifndef BOSS_LINES
#define BOSS_LINES     240               // ...his entry lines stay this long
#endif
static const int boss_spawn[3] = { 300, 0, 380 };   // where the villagers stood
#define MISSION_PTAS   5000              // paid out for the mission (mis_data_s01 says 5000)
#define TREASURE_REACH inttof32(20)
#define TREASURE_PTAS  3000              // a treasure taken (the video's results: Joia +3000)
#define GANADO_ACTIVE 5
#define GANADO_FROM_DOOR GANADO_COUNT     // (none comes out of the house door now)

// Stages 2-12: the mission file's enemies (stage_foes.c), in waves: the first wave stands at
// its points from the start, watching; each later one comes in once every enemy of the waves
// before it is down, at most GANADO_ACTIVE standing at once. There are more of them than enemy
// slots: the first foe_slots fill the slots, later ones take the slots of bodies that have gone.
#define FOES_MAX 32
#define WALL_TOP 80     // a cultist's record this high is on top of stage 7's walls: a thrower
// Every stage has the mission file's enemies. (-DSTAGE1_ENEMIES_ONLY: only stage 1 does, the
// others walked through empty and not cleared, while their floors, doors and ladders settle.)
#ifdef STAGE1_ENEMIES_ONLY
#define FOES_ON(stage) ((stage) == 0)
#else
#define FOES_ON(stage) 1
#endif
static const StageFoe *foe_list[FOES_MAX];
static int foe_n, foe_next, foe_slots, foe_wave;

static int foes_standing(void)
{
    int n = 0;
    for (int i = 0; i < foe_slots; i++)
        n += ganados.e[i].active && ganados.e[i].state < ENEMY_DYING;
    return n;
}

// Brings in the next foes while their wave is open and there is room. One whose point is where
// Leon is (stage 8's first cultist stands on his start; stages 2 and 5 send their last waves in
// by the way he came) waits until he has moved off it, or FOE_WAIT frames at most.
#define FOE_CLEAR inttof32(30)
#define FOE_WAIT  (60 * 20)
static int foe_held;

// Foes whose point is far off or on another level (stage 6's across the fields, stage 5's up on
// the chapel's gallery, stage 10's down in the hall Leon can't get to) would never come, and the
// stage would never clear. Such a one comes in instead where Leon can walk straight to from
// where he is, FOE_NEAR_R or so away, already after him (as stage 1's villagers stand together).
#define FOE_FAR     inttof32(110)
#define FOE_STEP    inttof32(3)    // (STEP_UP, defined further down)
#define FOE_LEVEL   inttof32(16)
static int foe_tries, foe_moves, foe_cells, foe_y0;   // (test readout)

// Buildings to cull by (see the stage's draw): the walls' box (world x, z), how high its roof
// is, and how much of the outside still shows from in it.
typedef struct { s8 stage; s16 x0, z0, x1, z1, top, yard; } Room;
static const Room rooms[] = {
    { 5, 174, 217, 294, 316, 60, 30 },   // stage 6: the cabin (its ladders stand outside its walls)
};
typedef struct { s8 stage; s16 from, below; } Gallery;   // Leon at least rom high: nothing below elow drawn
static const Gallery galleries[] = {
    { 7, 36, 30 },   // stage 8: the gallery over the audience hall
};
static const Room *room_of(int stage)
{
    for (unsigned i = 0; i < sizeof(rooms) / sizeof(rooms[0]); i++)
        if (rooms[i].stage == stage)
            return &rooms[i];
    return NULL;
}
// Where a far-off foe comes in instead (see FOE_FAR): a flood fill over the floor from Leon's
// feet, in FOE_CELL steps (up or down at most a step, not through walls), so it follows the
// corridors and stage 10's narrow balcony; of the places FOE_NEAR_MIN..FOE_NEAR_MAX of walking
// away it takes the one farthest from those already standing (they come from all round, not in
// a crowd), the foe's number choosing between equals.
#define FOE_CELL      inttof32(4)
#define FOE_BODY      floattof32(2.2)        // (Leon's is 2.5: where he fits)
#define FOE_GRID      101                    // cells across (Leon in the middle)
#define FOE_NEAR_MIN  12                     // cells of walking
#define FOE_NEAR_MAX  45
static u8 foe_dist[FOE_GRID * FOE_GRID];
static s32 foe_fy[FOE_GRID * FOE_GRID];     // floor height (f32: whole units would lose the slopes)
static int foe_cx, foe_cz;                  // the flood's centre (f32 world): where Leon was
// The flood itself: from Leon's feet over the floor, up or down a step at a time, not through
// walls (a body as wide as Leon's, about: the castles' doorways let it through, gaps in a fence don't).
static void foe_flood(const int leon_at[3])
{
    static u16 queue[FOE_GRID * FOE_GRID];
    memset(foe_dist, 0xff, sizeof(foe_dist));
    foe_cx = leon_at[0];
    foe_cz = leon_at[2];
    // (the floor under him: the mission's start height can be a little under the floor's)
    int y0 = leon_at[1];
    ground_height(&stage_ground, leon_at[0], leon_at[2], leon_at[1] + inttof32(16), 0, &y0);
    int c0 = (FOE_GRID / 2) * FOE_GRID + FOE_GRID / 2, head = 0, tail = 0;
    foe_dist[c0] = 0;
    foe_fy[c0] = y0;
    queue[tail++] = c0;
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
            // No wall at the new place, nor between it and the last (a thin wall between two
            // places 4 apart is within 2 of the point half way: it had let the flood step over
            // the field's walls, and foes come in outside them).
            int qx = px, qz = pz, mx = (x + px) / 2, mz = (z + pz) / 2;
            if (ground_collide_n(&stage_ground, &qx, &qz, y + FOE_STEP, y + inttof32(16), FOE_BODY, 1)
                || ground_collide_n(&stage_ground, &mx, &mz, y + FOE_STEP, y + inttof32(16), FOE_BODY, 1))
                continue;
            foe_dist[n] = foe_dist[c] + 1;
            foe_fy[n] = y;
            queue[tail++] = n;
        }
    }
    foe_cells = tail;
    foe_y0 = f32toint(y0);
}

// Whether the flood reached (x, z) at about height y (world units): a mission point outside the
// field (behind its walls, across a gap) is not.
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

// A place for a foe from the flood: FOE_NEAR_MIN..FOE_NEAR_MAX of walking from Leon, the one
// farthest from those already standing (they come from all round, not in a crowd), the foe's
// number choosing between equals.
static int foe_near(int seed, int out[3])
{
    foe_tries++;
    int best = -1, best_score = -1;
    for (int c = 0; c < FOE_GRID * FOE_GRID; c++)
    {
        if (foe_dist[c] == 0xff || foe_dist[c] < FOE_NEAR_MIN)
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
}static void foes_step(const int leon_at[3])
{
    int flooded = 0;   // (the flood is made once, when a foe first needs it)
    while (foe_next < foe_n && foes_standing() < GANADO_ACTIVE)
    {
        const StageFoe *f = foe_list[foe_next];
        int ldx = inttof32(f->pos[0]) - leon_at[0], ldz = inttof32(f->pos[2]) - leon_at[2];
        int ldy = inttof32(f->pos[1]) - leon_at[1];   // (on the floor below a balcony he stands on: no matter)
        if ((s64)ldx * ldx + (s64)ldz * ldz < (s64)FOE_CLEAR * FOE_CLEAR && ldy < inttof32(20) && ldy > -inttof32(20)
            && ++foe_held < FOE_WAIT)
            return;
        foe_held = 0;
        if (f->wave != foe_wave)
        {
            if (foes_standing() > 0)
                return;   // (the wave before is still on its feet)
            foe_wave = f->wave;
        }
        int slot = -1;
        if (foe_next < foe_slots)
            slot = foe_next;   // (placed, waiting)
        else
            for (int i = 0; i < foe_slots && slot < 0; i++)
                if (!ganados.e[i].active && ganados.e[i].state == ENEMY_DEAD)
                    slot = i;
        if (slot < 0)
            return;   // (no body has gone yet)
        // Several of a wave may share a spawn point: they come out side by side.
        int same = 0;
        for (int k = foe_next - 1; k >= 0 && foe_list[k]->wave == f->wave; k--)
            same += foe_list[k]->pos[0] == f->pos[0] && foe_list[k]->pos[2] == f->pos[2];
        int xyz[3] = { f->pos[0] + (same & 1 ? 6 : -6) * ((same + 1) / 2), f->pos[1], f->pos[2] };
        int yaw = f->dir * (DEGREES_IN_CIRCLE / 256);
        // Its point far off, on another level, or anywhere Leon can't walk to from where he is
        // (outside the field: behind its walls, across a gap): it comes in near him instead.
        if (!flooded)
        {
            foe_flood(leon_at);
            flooded = 1;
        }
        int pfy = inttof32(f->pos[1]);
        ground_height(&stage_ground, inttof32(xyz[0]), inttof32(xyz[2]), inttof32(f->pos[1] + 6), FOE_STEP * 2, &pfy);
        int fdx = inttof32(xyz[0]) - leon_at[0], fdz = inttof32(xyz[2]) - leon_at[2], fdy = inttof32(f->pos[1] - 4) - leon_at[1];
        int moved = ((s64)fdx * fdx + (s64)fdz * fdz > (s64)FOE_FAR * FOE_FAR || fdy > FOE_LEVEL || fdy < -FOE_LEVEL
                     || !foe_reachable(xyz[0], f32toint(pfy), xyz[2]))
                    && foe_near(foe_next, xyz);
        if (moved)   // (facing him)
            yaw = (int)(atan2f((float)(leon_at[0] - inttof32(xyz[0])), (float)(leon_at[2] - inttof32(xyz[2]))) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
        if (foe_next >= foe_slots || same || moved)
            enemies_respawn(&ganados, slot, xyz, yaw);
        ganados.e[slot].yaw = yaw;
        ganados.e[slot].drop = f->drop;
#ifdef TEST_DROP_CODE   // test builds: every one of them leaves this drop code's item
        ganados.e[slot].drop = TEST_DROP_CODE;
#endif
        enemies_activate(&ganados, slot, NULL);
        if (moved)
            enemies_alert(&ganados, slot);   // (he comes for Leon)
        foe_next++;
    }
}

// The n-th foe of a kind (the bosses: FOE_GARRADOR, FOE_GIANT, FOE_VERDUGO), or NULL.
static const StageFoe *stage_boss(int stage, int type, int n)
{
    const StageFoes *sf = &stage_foes[stage];
    for (int i = 0; (FOES_ON(stage) || type == FOE_GIANT) && i < sf->count; i++)   // (the giants are back already)
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
// Leon starts where the game starts him: on the hill road south-west of the village, facing
// the south gate (mis_data_s01.bin).
#define STAGE_START_X 19.5f
#define STAGE_START_Z 781.2f
#define STAGE_START_YAW 138               // degrees
#define BODY_DAMAGE  weapon_hit(1)
#define HEAD_DAMAGE  weapon_hit(2)
static int leon_pose[MODEL_MAX_NODES][12];
static NodeLocal pose_base[MODEL_MAX_NODES], pose_aim[MODEL_MAX_NODES], pose_tilt[MODEL_MAX_NODES];
static NodeLocal pose_prev[MODEL_MAX_NODES], pose_from[MODEL_MAX_NODES];
static NodeLocal pose_fx[MODEL_MAX_NODES], pose_fx0[MODEL_MAX_NODES];

// Order of animations in leon_anim.bin (re4zeebo.ps1 -DsAnim ...#0,1,2,11,20,21,13,3,10,8,16):
// idle, walk, run, then the reactions: flinch (#11), held by a Ganado (#20, loops), breaking
// free (#21), falling dead (#13); and the stage actions: climbing a ladder (#3: up 32 units
// and 16 back), a kick (#10), stepping off a ledge (#8), landing (#16). ANIM_OFF is a
// free-camera mode (Leon hidden), not a clip.
enum { ANIM_IDLE, ANIM_WALK, ANIM_RUN, ANIM_OFF };
enum { CLIP_FLINCH = 3, CLIP_HELD, CLIP_BREAK_FREE, CLIP_DEATH, CLIP_CLIMB, CLIP_KICK, CLIP_STEP_OFF, CLIP_LAND };
#define KICK_HIT      inttof32(18)      // frame of CLIP_KICK where the foot lands
#define CLIMB_BACK    inttof32(16)      // CLIP_CLIMB carries him this far back...
#define CLIMB_UP      inttof32(32)      // ...and up
#define DROP_MIN      inttof32(8)       // walking off a ledge higher than this: he drops
#define DROP_GRAVITY  floattof32(0.06)
#define LAND_FRAMES   inttof32(24)      // the part of CLIP_LAND played
#define BLAST_RANGE   inttof32(24)      // a barrel fells Ganados this close, hurts Leon half as close
#define BLAST_DAMAGE  30
#define HANG_BLAST    inttof32(20)      // a hanging canister's blast (0x4a688 size 5)
#define MEDAL_REWARD  20                // rounds for shooting every medallion

// Leon's health and his reactions to the Ganados' attacks.
#define LEON_HP       100
#define SWIPE_DAMAGE  15
#define WEAPON_DAMAGE 25                // a sickle, hatchet or pitchfork hits harder than hands
#define KICK_DAMAGE   2                 // Leon's kick on a dazed Ganado (and any next to it)
#define BITE_DAMAGE   8
#define GRAB_DIST     floattof32(6.5)   // Ganado to Leon while held (the Ganado's hands on him)
#define GRAB_MASH     8                 // A/B/X/Y presses to break free...
#define GRAB_MAX      (60 * 4)          // ...else the Ganado lets go after this many frames (biting meanwhile)
#define FREE_DAMAGE   2                 // breaking free knocks him back, dazed: then a kick ("Acao: Chutar")
#define SAFE_FRAMES   90               // after a reaction, attacks don't connect for this long
#define MUSIC_VOLUME  80                // background music, under the sound effects
#define KICK_FRAME    inttof32(16)     // frame of CLIP_BREAK_FREE where his kick lands
#define DEAD_TO_MENU  150              // frames from his fall to the game over menu
#define MENU_DELAY    60                // ...which takes no input for this long
enum { REACT_NONE, REACT_FLINCH, REACT_HELD, REACT_BREAK_FREE, REACT_DEAD,
       REACT_KICK, REACT_CLIMB_UP, REACT_CLIMB_DOWN, REACT_DROP, REACT_LAND, REACT_GIANT_UP, REACT_GIANT_CUT };

enum { MODE_TITLE, MODE_HOWTO, MODE_SELECT, MODE_PLAY, MODE_PAUSE, MODE_GAMEOVER, MODE_RESULT, MODE_STATUS, MODE_SHOP, MODE_CUTIN };
#define HERBS_MAX     9

// Ammo and items.
#ifndef SPARE_START
#define SPARE_START   20                // rounds carried besides the magazine at the start
#endif
#define AMMO_PICKUP   10                // rounds in a dropped box
#define HERB_HEAL     40                // health a green herb gives back
#define DROP_PERCENT  70                // chance a Ganado leaves an item when it goes down
#define DROP_DELAY    40                // ...which shows once it has fallen

// Stage clear: once every Ganado lies dead, the church bell tolls three times (and the music
// fades), STAGE CLEAR comes up, then the results.
#define BELL_FIRST    30                // frames after the last one fell
#define BELL_GAP      96
#define CLEAR_TEXT_AT 60
#define CLEAR_END     330
#define RESULT_STEP   24                // frames between result lines appearing
static const u8 react_clip[] = { 0, CLIP_FLINCH, CLIP_HELD, CLIP_BREAK_FREE, CLIP_DEATH,
                                 CLIP_KICK, CLIP_CLIMB, CLIP_CLIMB, CLIP_STEP_OFF, CLIP_LAND, CLIP_CLIMB, CLIP_CLIMB };
// The missions' story scenes (cutins.h: the mission files' scripts, their pictures and lines):
// on their own screen (MODE_CUTIN), the game standing still meanwhile. A page is its picture over
// black (256 x 128 at the top) and its text below, with who speaks; A turns the page (or it turns
// by itself after CUT_AUTO), START skips the rest. Played: each mission's opening and clear, and
// the events (the chainsaw man, El Gigante kneeling and Leon on his back, Ashley found, the
// Garrador, the Verdugo).
#ifndef CUT_AUTO   // (test builds: shorter)
#define CUT_AUTO      (60 * 8)
#endif
#define GAL_CUT_NEAR  inttof32(60)      // the Garrador's scene: Leon this near him
enum { CUT_AFTER_NONE, CUT_AFTER_GIANT, CUT_AFTER_JUMP };

// Jumps across a gap (the Zeebo version's "Acao: Pular"): stage 5's chandelier, from the north
// gallery's railing over the nave to the south gallery (mis_data_s05 scene 17: its picture and
// line, then Leon at the stage's point 13), and back. Standing at the spot (within JUMP_NEAR,
// on its floor) facing the way (within ~50 degrees), A plays the scene and he lands there.
typedef struct { s8 stage; s16 x, z, y, yaw, to_x, to_z, to_y, to_yaw; } JumpSpot;
static const JumpSpot jumps[] = {
    { 4, 64, 18, 46, 0, 64, 155, 46, 0 },
    { 4, 64, 150, 46, 180, 64, 17, 46, 180 },
};
#define JUMP_NEAR inttof32(7)
static const JumpSpot *jump_now;   // (the one being made)
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

// Sets up a stage's scene (none there, or no text: 0). The caller switches to MODE_CUTIN (with a
// fade: fade = 1, at an opening or a clear); at the end it goes to 	hen (MODE_PLAY: as it was).
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

// Onto a kneeling giant's back (the Zeebo version: Acao: Escalar). The game has no animation of
// it for Leon: it is an event, its scene (mis_data_s04 / s11 scene 11: the picture of Leon on top
// of El Gigante and two lines); then Leon is behind him, facing him.
#define GIANT_BACK    inttof32(8)       // the climb ends this far behind the giant's centre
#define GIANT_CUT_HIT inttof32(6)       // frame of the slash where the blade lands
// leon_gun.bin: handgun animations from ch_leon_w01.mcas #0 (ready stance), #1 (walk),
// #2 (run), #8 (draw: ends in the ready stance), the aim level, up and down (made by the
// exporter from #6 and #4, a turn apart: halfway, 90% and 10% of the way, each node turned
// the short way), #5 (fire, aiming up), #7 (fire, aiming down), #3 (reload)
enum { GUN_READY_POSE, GUN_WALK, GUN_RUN, GUN_DRAW, AIM_FORWARD, AIM_UP, AIM_DOWN,
       FIRE_UP, FIRE_DOWN, GUN_RELOAD };
#define DRAW_FRAMES leon_gun.frames[GUN_DRAW]
#define FIRE_FRAMES leon_gun.frames[FIRE_UP]
#define RELOAD_FRAMES leon_gun.frames[GUN_RELOAD]
#define MAG_SIZE    wep_mag(wep)
// The guns. Item, price and case size are item_base_info.bin's; firepower (x 10) and rounds the
// first level of item_tuneup_info.bin's and item_weapon_info.bin's; box and ammo price (per round)
// item_ammo_info.bin's and item_base_info.bin's. Refire is the animation frames before the next
// shot. Their models and animations (eqp_wep_all #N, ch_leon_w0N, same clip order as the
// handgun's) come from NitroFS when equipped.
typedef struct {
    const char *name, *model, *anim;
    u8 item, mag, refire, tam, box;
    u16 power, ammo_price;
    int price, muzzle[3];
    u8 special;            // WS_SCOPE: aimed, the view is the scope's; WS_ROCKET: one rocket, then it's gone
} Weapon;
enum { WS_NONE, WS_SCOPE, WS_ROCKET };
// (The muzzle, in the gun's own space: its barrel-end node in eqp_wep_all's skeleton for it.)
#define MUZZLE(x, y, z) { floattof32(x), floattof32(y), floattof32(z) }
static const Weapon weapons[] = {
    { T_ITEM_GUN, NULL, NULL, 4, 10, 3, 6, 50, 10, 50, 8000, MUZZLE(-2.92, -0.24, 0.84) },
    { T_ITEM_MAGNUM, "nitro:/gun5.bin", "nitro:/gun5_anim.bin", 5, 6, 13, 8, 10, 130, 500, 38000, MUZZLE(-4.47, -0.26, 1.20) },
    { T_ITEM_SHOTGUN, "nitro:/gun2.bin", "nitro:/gun2_anim.bin", 6, 6, 20, 16, 15, 40, 120, 20000, MUZZLE(-8.30, -0.18, 0.60) },
    { T_ITEM_RIFLE, "nitro:/gun3.bin", "nitro:/gun3_anim.bin", 7, 5, 20, 14, 10, 50, 150, 20000, MUZZLE(-7.79, -0.21, 1.00), WS_SCOPE },
    { T_ITEM_TMP, "nitro:/gun4.bin", "nitro:/gun4_anim.bin", 8, 30, 2, 10, 100, 4, 20, 14000, MUZZLE(-2.96, -0.24, 0.80) },
    { T_ITEM_ROCKET, "nitro:/gun6.bin", "nitro:/gun6_anim.bin", 9, 1, 40, 16, 0, 0, 0, 30000, MUZZLE(-6.31, 0.06, 0.50), WS_ROCKET },
};
#define WEP_LINE 18                      // (a text line nothing else uses in play)
enum { WEP_HANDGUN, WEP_MAGNUM, WEP_SHOTGUN, WEP_RIFLE, WEP_TMP, WEP_ROCKET, WEP_COUNT };
#define SCOPE_EYE     inttof32(15)       // (his eye: this high above his feet)
#define ROCKET_SPEED  inttof32(5)        // a rocket flies this far a frame...
#define ROCKET_BLAST  inttof32(30)       // ...and its blast fells all within this (El Gigante too)
#define SCOPE_FOV     20                 // the rifle's scope: degrees across (the view is 70)
#define SCOPE_R       26                 // its lens on screen, pixels (the video's: a seventh of the height)
#define SCOPE_PITCH   degreesToAngle(50) // aim tilt at full: this far up or down
#define SHOTGUN_RANGE inttof32(40)       // the shotgun's spread: these others are hit too
#define SHOTGUN_CONE  floattof32(0.95)   // (cosine of the half angle)
static int wep;                          // the weapon in hand
static int wep_ammo[WEP_COUNT], wep_spare[WEP_COUNT];
// What the shop changes: the guns owned (bits), their upgrades, the case.
#define WEP_STATS 4
static const u8 wep_stat_id[WEP_STATS] = { 0, 3, 5, 6 };      // (ShopTune.stat)
static const char *const wep_stat_name[WEP_STATS] = { T_STAT_POWER, T_STAT_CAPACITY, T_STAT_RATE, T_STAT_RELOAD };
enum { STAT_POWER, STAT_CAPACITY, STAT_RATE, STAT_RELOAD };
static int wep_owned = 1;
static u8 wep_level[WEP_COUNT][WEP_STATS];
static int case_level;
static const u8 case_tam[] = { 60, 77, 96, 120 };             // item_ac_info.bin
static const int case_price[] = { 0, 24000, 40000, 73000 };   // item_base_info.bin: the larger cases
static int shot_bank;                                         // firepower left over from the last hits

static const ShopTune *tune_at(int w, int stat, int level)
{
    for (int i = 0; i < shop_tune_count; i++)
        if (shop_tune[i].item == weapons[w].item && shop_tune[i].stat == wep_stat_id[stat] && shop_tune[i].level == level)
            return &shop_tune[i];
    return NULL;
}

// A stat of gun w at its level (x 10); base: before any upgrade.
static int wep_stat(int w, int stat, int base)
{
    const ShopTune *t = wep_level[w][stat] ? tune_at(w, stat, wep_level[w][stat]) : NULL;
    return t ? t->value : base;
}

static int wep_mag(int w) { return wep_stat(w, STAT_CAPACITY, weapons[w].mag * 10) / 10; }

// The damage of a hit, in whole points: weak guns land one every few hits.
static int weapon_hit(int times)
{
    shot_bank += wep_stat(wep, STAT_POWER, weapons[wep].power) * times;
    int d = shot_bank / 10;
    shot_bank -= d * 10;
    return d;
}

// Animation frames before the next shot (f32): the table's 0.4, 0.3 against an assumed first 0.5.
static int wep_refire(int w)
{
    return inttof32(weapons[w].refire) * wep_stat(w, STAT_RATE, 5) / 5;
}

// The reload's speed (f32): the table's seconds against 1.25 x its first level's.
static int wep_reload_speed(int w)
{
    const ShopTune *first = tune_at(w, STAT_RELOAD, 1);
    if (!first || !wep_level[w][STAT_RELOAD])
        return inttof32(1);
    return inttof32(first->value * 5 / 4) / wep_stat(w, STAT_RELOAD, first->value);
}
#define RECOIL      inttof32(1)          // share of the fire animations' motion added to the aim
#define RELOAD_FADE floattof32(1.5)      // reload pose fades in/out over this many anim frames
#define SHOT_RANGE  inttof32(400)
#define DRAW_GRAB   floattof32(4.5)      // draw frame where the hand reaches the holster
#define XFADE_STEP  (inttof32(1) / 6)    // animation switches cross-fade over 6 frames

enum { GUN_HOLSTERED, GUN_DRAWING, GUN_READY, GUN_HOLSTERING };

#define HAND_NODE   14                   // Leon's right wrist: the gun model is in its frame
#define UPPER_BODY  10                   // Leon's nodes from the spine up (0-9: root, hips, legs)
#define AIM_RAISE   (inttof32(1) / 8)    // aim blend per frame: raised in 8 frames
#define AIM_TILT    (inttof32(1) / 40)   // tilt per frame while Up/Down is held
#define AIM_TILT_RANGE floattof32(0.8)   // full tilt: 80% of the way to the up/down poses (~50 degrees)
#define AIM_TURN    degreesToAngle(1)
#define ANIM_FPS 20   // animation frames per second (the game updates at 20 Hz)
#define ANIM_STEP (inttof32(ANIM_FPS) / 60)

// Movement per 60 Hz frame (Leon is ~18 units tall).
// Measured against the Zeebo video: walking from mission 1's start to the house ahead (56 units)
// takes 3.4 s; running mission 12's first canal (about 130 units) 3.6 s.
#define WALK_SPEED  floattof32(0.27)   // ~16 units/s
#define RUN_SPEED   floattof32(0.60)   // ~36 units/s
#define RUN_ANIM    floattof32(1.5)    // the run clip played this much faster (its feet cover ~22 units/s)
#define BACK_SPEED  floattof32(0.16)
#define TURN_SPEED  degreesToAngle(4)  // 240 degrees/s
#define STEP_UP     inttof32(3)        // highest step climbed per frame; anything higher is ignored
#define LEON_RADIUS floattof32(2.5)    // collision cylinder against walls
#define LEON_HEIGHT inttof32(16)

// Follow camera, matched to a capture of the original game: close behind Leon's right
// shoulder at about head height, looking slightly down past him, so he fills the lower
// left of the screen from the head down to about the knees.
#define CAM_BACK      floattof32(14.5)   // behind Leon
#define CAM_NEAR      inttof32(9)        // the camera this close to Leon (a wall behind it)...
#define CAM_NEAR_LIFT floattof32(1.2)    // ...rises this much for each unit closer
#define CAM_FADE      inttof32(5)        // ...and closer than this he fades
#define CAM_CLIMB_OUT    inttof32(20)     // on a ladder: this far out from the wall
#define CAM_CLIMB_SIDE   inttof32(6)      // ...this far to his right
#define CAM_CLIMB_HEIGHT inttof32(12)     // ...this high above his feet
#define CAM_CLIMB_AT     inttof32(9)      // ...looking at his back
#define CAM_WALL_GAP  floattof32(1.5)    // the camera keeps this far in front of a wall
#define CAM_RIGHT     floattof32(1.5)    // to his right (he sits a little left of centre)
#define CAM_HEIGHT    floattof32(16.5)   // above his feet
#define CAM_AHEAD     floattof32(40)     // look-at point in front of him...
#define CAM_AT_HEIGHT floattof32(13)     // ...at this height (about 4 degrees down)
// While aiming, the camera moves in over the shoulder and follows the aim's tilt.
#define CAM_AIM_BACK    floattof32(9)
#define CAM_AIM_RIGHT   floattof32(3.5)
#define CAM_AIM_HEIGHT  floattof32(15.5)
#define CAM_AIM_TILT    floattof32(25)   // look-at height change at full up/down tilt

enum { CTRL_LEON, CTRL_FREECAM };

#ifndef START_CTRL   // -DSTART_CTRL=1 starts with the free camera
#define START_CTRL CTRL_LEON
#endif
#ifndef LEON_START_MODE   // free camera: -DLEON_START_MODE=1 starts walking round the circle
#define LEON_START_MODE ANIM_IDLE
#endif
// Free-camera walk circle (world units). Default: radius 20 around the stage centre, which
// is flat ground. -DLEON_CIRCLE_X=60 -DLEON_CIRCLE_Z=180 -DLEON_CIRCLE_R=50 circles a hill.
#ifndef LEON_CIRCLE_R
#define LEON_CIRCLE_R 20
#endif

static volatile u32 vblank_count;
static void on_vblank(void) { vblank_count++; }

static const u32 cull_modes[] = { POLY_CULL_BACK, POLY_CULL_FRONT, POLY_CULL_NONE };

#ifdef AUTOPILOT
// Test build (-DAUTOPILOT): scripted D-pad input in Leon mode, since an emulator window
// can't be driven from outside. Repeats every 10 s.
static int ap_aim_err;   // (test builds: the giant's bearing from Leon's facing, + = to his left)
static u32 autopilot_keys(u32 frame)
{
#if AUTOPILOT == 2
    // -DAUTOPILOT=2: run straight ahead, then veer left a little every 4 s.
    return (frame % 240) < 20 ? KEY_UP | KEY_B | KEY_LEFT : KEY_UP | KEY_B;
#elif AUTOPILOT == 4
    // -DAUTOPILOT=4: aiming test. Stand 9 s, aim 3 s, tilt up, tilt down, lower, repeat.
    if (frame < 540) return 0;
    u32 a = (frame - 540) % 600;
    if (a < 180) return KEY_R;
    if (a < 240) return KEY_R | KEY_UP;
    if (a < 360) return KEY_R | KEY_DOWN;
    if (a < 420) return KEY_R | KEY_UP;
    return 0;
#elif AUTOPILOT == 5
    // -DAUTOPILOT=5: draw/holster test. Stand 9 s, draw (X), stand ready 2 s, walk 2 s,
    // run 2 s, stand 1 s, holster (X), stand.
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
    // -DAUTOPILOT=7: shooting test. After 9 s, aim and fire every 0.4 s; the 11th shot
    // finds the magazine empty and reloads, then firing goes on. With -DLEON_START_X=225
    // -DLEON_START_Z=204 -DLEON_START_YAW=106 the shots hit a barn wall.
    if (frame < 540) return 0;
    u32 a = frame - 540, keys = KEY_R;
    if (a % 24 < 2 && a > 20) keys |= KEY_A;
    return keys;
#elif AUTOPILOT == 8
    // -DAUTOPILOT=8: Ganado test. Stand 9 s (they walk up), then aim and fire every 0.5 s,
    // turning a little left/right in between to pick up the next one.
    if (frame < 540) return 0;
    u32 a = frame - 540, keys = KEY_R;
    if (a % 30 < 2 && a > 20) keys |= KEY_A;
    if ((a / 90) % 4 == 1 && a % 30 > 10) keys |= KEY_LEFT;
    if ((a / 90) % 4 == 3 && a % 30 > 10) keys |= KEY_RIGHT;
    return keys;
#elif AUTOPILOT == 9
    // -DAUTOPILOT=9: Ganado attack test. Walk into the group for 4 s, then stand and let them
    // attack, tapping A five times a second (breaks free from a grab in ~1.6 s).
    if (frame < 540) return 0;
    u32 a = frame - 540;
    if (a < 240) return KEY_UP;
    return a % 12 < 2 ? KEY_A : 0;
#elif AUTOPILOT == 10
    // -DAUTOPILOT=10: menu test, from the title: controls screen (frames 560-840), back, start
    // the game, walk, pause (1140-1400, moving to "start over"), start over.
    // Then the stage select (900-1100): right to a locked stage, A (refused), left, A (zoom
    // into the village); walk, pause (1400-1600), start over.
    static const struct { u16 at; u16 key; } script[] = {
        { 540, KEY_DOWN }, { 560, KEY_A }, { 840, KEY_B }, { 870, KEY_DOWN }, { 900, KEY_A },
        { 960, KEY_RIGHT }, { 1000, KEY_A }, { 1040, KEY_LEFT }, { 1100, KEY_A },
        { 1400, KEY_START }, { 1460, KEY_DOWN }, { 1600, KEY_A } };
    for (unsigned i = 0; i < sizeof(script) / sizeof(script[0]); i++)
        if (frame >= script[i].at && frame < script[i].at + 2u)
            return script[i].key;
    return frame > 1200 && frame < 1400 ? KEY_UP : 0;
#elif AUTOPILOT == 12
    // -DAUTOPILOT=12 -DTEST_HURT -DTEST_GOD: the vital screen, by scripted taps (see main()).
    (void)frame;
    return 0;
#elif AUTOPILOT == 13
    // -DAUTOPILOT=13 -DSTAGE_START -DLEON_START_X=240 -DLEON_START_Z=565 -DLEON_START_YAW=180:
    // walk up to the south gate, kick it open (A), walk into the village.
    if (frame >= 540 && frame < 720) return KEY_UP;   // ends pressed against the gate
    if (frame >= 740 && frame < 742) return KEY_A;
    if (frame >= 840 && frame < 1100) return KEY_UP;
    return 0;
#elif AUTOPILOT == 14
    // -DAUTOPILOT=14 -DSTAGE_START -DLEON_START_X=310 -DLEON_START_Z=76 -DLEON_START_YAW=90:
    // up the ladder (A), turn round, walk to the edge and climb down (A); walk off the walkway
    // elsewhere and drop.
    if (frame >= 540 && frame < 560) return KEY_UP;
    if (frame >= 570 && frame < 572) return KEY_A;
    if (frame >= 780 && frame < 800) return KEY_A;     // straight back down (he faces the edge)
    if (frame >= 1000 && frame < 1002) return KEY_A;   // up again
    if (frame >= 1200 && frame < 1260) return KEY_LEFT;
    if (frame >= 1260 && frame < 1500) return KEY_UP;  // along the walkway, off its end
    return 0;
#elif AUTOPILOT == 15
    // -DAUTOPILOT=15 -DSTAGE_START -DLEON_START_X=340 -DLEON_START_Z=76 -DLEON_START_YAW=270:
    // on the walkway (the start lands on the highest floor), walk off its edge and drop.
    return frame >= 540 && frame < 700 ? KEY_UP : 0;
#elif AUTOPILOT == 16
    // -DAUTOPILOT=16 -DSTAGE_START -DLEON_START_X=359 -DLEON_START_Z=250 -DLEON_START_YAW=0:
    // aim at the barrel ahead and shoot it (it blows up); then at the crate to the right.
    // (Barrels and crates are lower than the gun: the aim tilts down a little first.)
    if (frame >= 540 && frame < 700) return KEY_R | (frame >= 560 && frame < 563 ? KEY_DOWN : 0) | (frame >= 570 && frame < 577 ? KEY_RIGHT : 0) | (frame >= 620 && frame < 622 ? KEY_A : 0);
    if (frame >= 760 && frame < 960) return KEY_R | (frame >= 770 && frame < 773 ? KEY_DOWN : 0) | (frame >= 800 && frame < 830 ? KEY_LEFT : 0) | (frame >= 900 && frame < 902 ? KEY_A : 0);
    return 0;
#elif AUTOPILOT == 18
    // -DAUTOPILOT=18 -DSTAGE_START -DLEON_START_X=391 -DLEON_START_Z=322 -DLEON_START_YAW=90:
    // next to a crate: hold L (knife ready), slash it (A).
    if (frame >= 540 && frame < 760) return KEY_L | (frame >= 600 && frame < 602 ? KEY_A : 0);
    return 0;
#elif AUTOPILOT == 45
    // -DAUTOPILOT=45 -DGIANT_AHEAD_TEST: shoot him to his knees and let him get up again; then
    // shoot him down and run up to cut, again and again.
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
    // -DAUTOPILOT=46 -DTEST_ALL_WEAPONS -DTEST_WEAPON=n: draw, n x L+X, then aim and fire once
    // after 4 s (the launcher at the giant ahead; the rifle through its scope, tilting).
    if (frame < 400) return 0;
    if (frame < 403) return KEY_X;
    for (int k = 0; k < TEST_WEAPON; k++)
        if (frame >= 480u + k * 40 && frame < 483u + k * 40) return KEY_L | KEY_X;
    if (frame < 800) return 0;
    if (frame >= 1040 && frame < 1042) return KEY_R | KEY_A;
    if (frame >= 1100 && frame < 1160) return KEY_R | KEY_UP;
    return frame < 1400 ? KEY_R : 0;
#elif AUTOPILOT == 47
    // -DAUTOPILOT=47 -DTEST_ALL_WEAPONS -DGIANT_AHEAD_TEST: the rifle (3 x L+X), through the scope
    // at the giant; when he kneels, run at him and A (the climb and the cut); again until he falls.
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
    // (Turned on him: the left/right of aiming.)
    u32 turn = ap_aim_err > degreesToAngle(2) ? KEY_LEFT : ap_aim_err < -degreesToAngle(2) ? KEY_RIGHT : 0;
    return KEY_R | turn | (!turn && (frame % 30) < 2 ? KEY_A : 0);
#elif AUTOPILOT == 19
    // -DAUTOPILOT=19: from 5 s, aim straight ahead and fire every 1/3 s (the giant test).
    if (frame < 300) return 0;
    return KEY_R | ((frame % 20) < 2 ? KEY_A : 0);
#elif AUTOPILOT == 17
    // -DAUTOPILOT=17 -DTEST_DAZE -DTEST_GOD: let one walk up, shoot it (every hit dazes in this
    // build), lower the gun and kick it (A); again every 2 s.
    if (frame < 900) return 0;
    u32 a = (frame - 900) % 120;
    if (a < 40) return KEY_R | (a < 24 ? KEY_LEFT : 0) | (a >= 30 && a < 32 ? KEY_A : 0);   // turning, to find one
    return a >= 50 && a < 52 ? KEY_A : 0;
#elif AUTOPILOT == 11
    // -DAUTOPILOT=11 -DTEST_GOD -DTEST_KILL_AT=600: they walk up and all go down at frame 600,
    // dropping items (on show 11-14 s); Leon walks forward through them (14-18 s), then the
    // stage clears (bells, STAGE CLEAR, results) and A goes on to the stage select.
    if (frame >= 840 && frame < 1080) return KEY_UP;
    if (frame >= 1700 && frame % 60 < 2) return KEY_A;
    return 0;
#elif AUTOPILOT == 21
    // -DAUTOPILOT=21: the pause menu: START, down to Quit, A (the question), up to YES, B (back),
    // down to Sound, A, up to Medium, A (set), down to Controls, A, B, START (back to the game).
    static const struct { u16 at; u16 key; } script[] = {
        { 400, KEY_START }, { 520, KEY_DOWN }, { 640, KEY_A }, { 760, KEY_UP }, { 880, KEY_B },
        { 1000, KEY_DOWN }, { 1060, KEY_A }, { 1180, KEY_UP }, { 1300, KEY_A }, { 1420, KEY_DOWN },
        { 1480, KEY_A }, { 1600, KEY_B }, { 1720, KEY_START } };
    for (unsigned i = 0; i < sizeof(script) / sizeof(script[0]); i++)
        if (frame >= script[i].at && frame < script[i].at + 2u)
            return script[i].key;
    return 0;
#elif AUTOPILOT == 23
    // -DAUTOPILOT=23: from 5 s, aim, tilting up a little and firing in turn (the hanging canister).
    if (frame < 300) return 0;
    return KEY_R | ((frame % 30) < 5 && frame > 330 ? KEY_UP : 0) | ((frame % 30) >= 15 && (frame % 30) < 17 ? KEY_A : 0);
#elif AUTOPILOT == 22
    // -DAUTOPILOT=22 -DTEST_HURT: the status screen: SELECT, A on the handgun (its commands),
    // B, down to the ammo, A, B, down to a herb, A, A (use it), down x3 (scrolling), SELECT.
    static const struct { u16 at; u16 key; } script[] = {
        { 400, KEY_SELECT }, { 520, KEY_A }, { 640, KEY_B }, { 700, KEY_DOWN }, { 760, KEY_A },
        { 880, KEY_B }, { 940, KEY_DOWN }, { 1000, KEY_A }, { 1120, KEY_A }, { 1240, KEY_DOWN },
        { 1300, KEY_DOWN }, { 1360, KEY_DOWN }, { 1480, KEY_SELECT } };
    for (unsigned i = 0; i < sizeof(script) / sizeof(script[0]); i++)
        if (frame >= script[i].at && frame < script[i].at + 2u)
            return script[i].key;
    return 0;
#elif AUTOPILOT == 31
    // -DAUTOPILOT=31: draw, then every 4 s the next weapon (L+X), aim (R) and fire.
    if (frame < 400) return 0;
    if (frame < 403) return KEY_X;
    if (frame < 480) return 0;
    u32 a = (frame - 480) % 240;
    if (a < 3) return KEY_L | KEY_X;
    if (a < 60) return 0;
    if (a == 130 || a == 131) return KEY_R | KEY_A;
    return a < 200 ? KEY_R : 0;
#elif AUTOPILOT == 32
    // -DAUTOPILOT=32 -DTEST_WEAPON=n: draw, n x L+X, then keep aiming and fire every 4 s.
    if (frame < 400) return 0;
    if (frame < 403) return KEY_X;
    for (int k = 0; k < TEST_WEAPON; k++)
        if (frame >= 480u + k * 40 && frame < 483u + k * 40) return KEY_L | KEY_X;
    if (frame < 800) return 0;
    return (frame - 1000) % 240 < 2 ? KEY_R | KEY_A : KEY_R;
#elif AUTOPILOT == 33
    // -DAUTOPILOT=33: the status screen: SELECT, down x2 (the shotgun), A, A (Equip), SELECT, draw, aim.
    static const struct { u16 at; u16 key; } script33[] = {
        { 400, KEY_SELECT }, { 480, KEY_DOWN }, { 540, KEY_DOWN }, { 620, KEY_A }, { 700, KEY_A }, { 860, KEY_SELECT }, { 960, KEY_X } };
    for (unsigned i = 0; i < sizeof(script33) / sizeof(script33[0]); i++)
        if (frame >= script33[i].at && frame < script33[i].at + 2u)
            return script33[i].key;
    return frame > 1060 ? KEY_R : 0;
#elif AUTOPILOT == 34
    // -DAUTOPILOT=34 -DTEST_SHOP: the shop: Buy, the Magnum, then its ammo; B; Upgrade, the handgun,
    // firepower; B, B; Sell, the Magnum; B; Start.
    static const struct { u16 at; u16 key; } script34[] = {
        { 300, KEY_A }, { 380, KEY_DOWN }, { 460, KEY_A }, { 560, KEY_DOWN }, { 620, KEY_DOWN }, { 680, KEY_DOWN },
        { 740, KEY_DOWN }, { 800, KEY_DOWN }, { 880, KEY_A }, { 980, KEY_B }, { 1060, KEY_RIGHT }, { 1120, KEY_RIGHT },
        { 1200, KEY_A }, { 1280, KEY_A }, { 1360, KEY_A }, { 1460, KEY_B }, { 1520, KEY_B }, { 1600, KEY_LEFT },
        { 1680, KEY_A }, { 1760, KEY_A }, { 1860, KEY_B }, { 1940, KEY_LEFT }, { 2020, KEY_LEFT }, { 2100, KEY_A } };
    for (unsigned i = 0; i < sizeof(script34) / sizeof(script34[0]); i++)
        if (frame >= script34[i].at && frame < script34[i].at + 2u)
            return script34[i].key;
    return 0;
#elif AUTOPILOT == 35
    // -DAUTOPILOT=35: from the title: New Game, a stage, the shop's Upgrade tab, Start.
    static const struct { u16 at; u16 key; } script35[] = {
        { 300, KEY_A }, { 420, KEY_A }, { 700, KEY_A }, { 800, KEY_DOWN }, { 900, KEY_B }, { 1000, KEY_LEFT }, { 1080, KEY_A } };
    for (unsigned i = 0; i < sizeof(script35) / sizeof(script35[0]); i++)
        if (frame >= script35[i].at && frame < script35[i].at + 2u)
            return script35[i].key;
    return 0;
#elif AUTOPILOT == 49
    // -DAUTOPILOT=49: aim and fire a few shots, R held all along; then Y (reload) and Up held for
    // 3 s (he walks while reloading).
    if (frame < 300) return 0;
    u32 k = KEY_R;
    if (frame > 340 && frame < 400 && frame % 18 == 0) k |= KEY_A;
    if (frame == 900) k |= KEY_Y;
    if (frame >= 904 && frame < 1080) k |= KEY_UP;
    return k;
#elif AUTOPILOT == 50
    // -DAUTOPILOT=50: the knife (L held) from 10 s: Up for 1 s (looking up), slash, slash; then
    // Down for 2 s (looking down), slash, slash.
    if (frame < 600) return 0;
    u32 k = KEY_L;
    if (frame < 660) k |= KEY_UP;
    if (frame == 720 || frame == 800) k |= KEY_A;
    if (frame >= 880 && frame < 1000) k |= KEY_DOWN;
    if (frame == 1060 || frame == 1140) k |= KEY_A;
    return k;
#elif AUTOPILOT == 51
    // -DAUTOPILOT=51: from the title: New Game, Right x5 on the stage select (stage 6, the
    // battlements), A, the shop's Start; then turn on the spot (as 42).
    static const struct { u16 at; u16 key; } script51[] = {
        { 600, KEY_A }, { 900, KEY_RIGHT }, { 960, KEY_RIGHT }, { 1020, KEY_RIGHT }, { 1080, KEY_RIGHT }, { 1140, KEY_RIGHT },
        { 1260, KEY_A }, { 1500, KEY_LEFT }, { 1600, KEY_A }, { 2000, KEY_START } };
    for (unsigned i = 0; i < sizeof(script51) / sizeof(script51[0]); i++)
        if (frame >= script51[i].at && frame < script51[i].at + 2u)
            return script51[i].key;
    return frame > 2300 && (frame / 90) % 3 == 0 ? KEY_LEFT : 0;
#elif AUTOPILOT == 48
    // -DAUTOPILOT=48: from the title: New Game, then about the stage select (past stage 6's place).
    static const struct { u16 at; u16 key; } script48[] = {
        { 600, KEY_A }, { 900, KEY_RIGHT }, { 1020, KEY_RIGHT }, { 1140, KEY_RIGHT }, { 1260, KEY_RIGHT }, { 1380, KEY_RIGHT }, { 1500, KEY_LEFT } };
    for (unsigned i = 0; i < sizeof(script48) / sizeof(script48[0]); i++)
        if (frame >= script48[i].at && frame < script48[i].at + 2u)
            return script48[i].key;
    return 0;
#elif AUTOPILOT == 36
    // -DAUTOPILOT=36 -DTEST_SHOP: Buy, down to the Green Herb, A again and again until the case is full.
    if (frame == 300 || frame == 301) return KEY_A;
    for (unsigned i = 0; i < 6; i++)
        if (frame >= 360 + i * 30 && frame < 362 + i * 30) return KEY_DOWN;
    return frame > 600 && frame % 16 < 2 ? KEY_A : 0;
#elif AUTOPILOT == 37
    // -DAUTOPILOT=37 -DTEST_SHOP: Buy, A (the owned handgun: nothing), down, A (the Magnum).
    static const struct { u16 at; u16 key; } script37[] = { { 300, KEY_A }, { 400, KEY_DOWN }, { 480, KEY_A } };
    for (unsigned i = 0; i < 3; i++)
        if (frame >= script37[i].at && frame < script37[i].at + 2u)
            return script37[i].key;
    return 0;
#elif AUTOPILOT == 38
    // -DAUTOPILOT=38 -DTEST_CLEAR_AT=500: after the mission's end, A now and then: results, stage select, shop.
    return frame > 900 && frame % 120 < 2 ? KEY_A : 0;
#elif AUTOPILOT == 39
    // -DAUTOPILOT=39: the pause menu's Quit: START, down, A, up to YES, A.
    static const struct { u16 at; u16 key; } script39[] = { { 400, KEY_START }, { 480, KEY_DOWN }, { 560, KEY_A }, { 640, KEY_UP }, { 720, KEY_A } };
    for (unsigned i = 0; i < 5; i++)
        if (frame >= script39[i].at && frame < script39[i].at + 2u)
            return script39[i].key;
    return 0;
#elif AUTOPILOT == 40
    // -DAUTOPILOT=40 -DTEST_SHOP: buy the Magnum, go back (saves); restart: the title offers Continue.
    static const struct { u16 at; u16 key; } script40[] = {
        { 300, KEY_A }, { 380, KEY_DOWN }, { 460, KEY_A }, { 560, KEY_B }, { 640, KEY_B } };
    for (unsigned i = 0; i < 5; i++)
        if (frame >= script40[i].at && frame < script40[i].at + 2u)
            return script40[i].key;
    return 0;
#elif AUTOPILOT == 41
    // -DAUTOPILOT=41: from the title: Continue, a stage, the shop's Buy list.
    static const struct { u16 at; u16 key; } script41[] = { { 300, KEY_A }, { 420, KEY_A }, { 700, KEY_A } };
    for (unsigned i = 0; i < 3; i++)
        if (frame >= script41[i].at && frame < script41[i].at + 2u)
            return script41[i].key;
    return 0;
#elif AUTOPILOT == 42
    // -DAUTOPILOT=42: turn on the spot, walking now and then (nothing of the stage may go missing).
    if (frame < 300) return 0;
    return (frame / 240) % 2 ? KEY_RIGHT : KEY_UP | KEY_B | KEY_LEFT;
#elif AUTOPILOT == 43
    // -DAUTOPILOT=43: A (up the ladder), then stand while she follows; A (down), stand.
    if (frame == 540 || frame == 541 || frame == 1500 || frame == 1501) return KEY_A;
    return 0;
#elif AUTOPILOT == 44
    // -DAUTOPILOT=44: run till a wall stops him, then turn slowly with his back and side to it
    // (the camera pressed against it), then run on along it; again.
    if (frame < 300) return 0;
    u32 a = (frame - 300) % 600;
    if (a < 180) return KEY_UP | KEY_B;
    if (a < 420) return KEY_LEFT;
    return KEY_UP | KEY_B | (a & 64 ? KEY_RIGHT : 0);
#elif AUTOPILOT == 30
    // -DAUTOPILOT=30: A (the door or ladder at hand), then walk on through; A, walk on again.
    if (frame < 540) return 0;
    u32 a = (frame - 540) % 420;
    if (a < 3) return KEY_A;
    return a >= 60 && a < 360 ? KEY_UP : 0;
#elif AUTOPILOT == 20
    // -DAUTOPILOT=20 (+ the barn-wall start of 3): run into the wall, then turn round slowly
    // with his back to it (the camera must stay in front of the wall).
    if (frame < 540) return 0;
    if (frame < 660) return KEY_UP | KEY_B;
    return frame < 900 ? KEY_LEFT : 0;
#elif AUTOPILOT == 3
    // -DAUTOPILOT=3: stand for 9 s (emulator start-up), then keep running straight. With
    // -DLEON_START_X=225 -DLEON_START_Z=204 -DLEON_START_YAW=106 he runs head-on into a barn wall.
    return frame < 540 ? 0 : KEY_UP | KEY_B;
#endif
    u32 t = frame % 600;
    if (t < 120) return KEY_UP;                 // walk
    if (t < 180) return KEY_UP | KEY_LEFT;      // walk while turning
    if (t < 330) return KEY_UP | KEY_B;         // run
    if (t < 390) return KEY_RIGHT;              // turn in place
    if (t < 480) return KEY_DOWN;               // walk backward
    return 0;                                   // idle
}
#endif

// Haze of the original (sampled from a capture: sky and far distance are 100,100,78); each
// stage sets its own (stage_info[].haze, from its mission file).
#define HAZE_R 12
#define HAZE_G 12
#define HAZE_B 9
#define NEAR_PLANE 1.0f
#ifndef FAR_PLANE
#define FAR_PLANE  700.0f
#endif
#define POLY_HIGH      1940   // a frame this full: the stage sends less (the hardware keeps 2048)
#define POLY_LOW       1800   // ...this empty: it sends more again
#define POLY_STAGE_MIN 1200   // (triangles sent, nearest first: never fewer)
static int stage_budget = -1; // triangles of the stage sent a frame (-1: all of it)
// When the stage's floor is reached and the frame is still too full, the farthest enemies are
// left out instead (in the haze, but walls in Leon's face must stay).
#define ENEMY_SHOW_MIN 2
static int enemy_show = ENEMY_MAX;
static int stage_used;   // the polygons the stage took this frame
#define TIME_CAP_MIN   1200   // (the stage's triangles under a slow frame rate: never fewer; 800 lost near walls)
static int time_cap = 4000, enemy_cap = ENEMY_MAX;   // caps from the time frames take (see the main loop)
#define FOG_START  15.0f   // world units: no fog closer than this
#define FOG_SCALE  130.0f  // density = 1 - exp(-(d - FOG_START) / FOG_SCALE): ~40% at 80, ~75% at 200

// 15-bit depth buffer value of a point at view distance d (Z-buffering, perspective
// with NEAR_PLANE/FAR_PLANE), and back.
static float depth_of(float d)
{
    return 32767.0f * FAR_PLANE / (FAR_PLANE - NEAR_PLANE) * (1.0f - NEAR_PLANE / d);
}

static float distance_of(float depth)
{
    float q = depth / 32767.0f * (FAR_PLANE - NEAR_PLANE) / FAR_PLANE;
    return q >= 1.0f ? 1e9f : NEAR_PLANE / (1.0f - q);
}

// Hardware fog: the 32 table entries sit at depths FOG_OFFSET + (n + 1) * (0x400 >> shift).
// Depth is far from linear in distance, so each entry's density is computed from the
// distance its depth corresponds to.

static void setup_fog(void)
{
    const int shift = 3;   // 128 depth units per entry: covers ~10 to ~700 units
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

// A small translucent "star" (three crossed squares) for muzzle flashes and impacts.
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

// A 1-pixel line (the hardware draws a triangle with two equal vertices as a line).
static void draw_line(const int from[3], const int to[3], u16 color, int id)
{
    model_sync();
    glPushMatrix();
    glTranslatef32(from[0], from[1], from[2]);
    glScalef32(inttof32(64), inttof32(64), inttof32(64));   // keeps vertices inside v16 range
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

// HUD parts in hud.bin, the right half (x 128..255) of 2d_gms_00.mct (coordinates below are
// in the sheet): Leon's plate with the ammo counter box and the health track, the gauge fills
// and the counter digits.
#define PLATE_SX 29          // plate: 99 x 25
#define PLATE_SY 59
#define PLATE_W  99
#define PLATE_H  25
#define PLATE_X  (256 - PLATE_W - 3)   // on screen: bottom right
#define PLATE_Y  (192 - PLATE_H - 4)
#define TRACK_X  1           // health track inside the plate: 96 x 4
#define TRACK_Y  19
#define TRACK_W  96
#define TRACK_H  4
#define LIFE_BAR_X 120       // Leon's life bar starts here (the video: under the middle of the screen)
#define FILL_SX  90          // middle columns of the green and red bars, stretched along the track
#define GREEN_SY 110
#define RED_SY   118
#define DIGIT_SY 0           // counter digits: 8 x 15 ('1' is 4 wide)
#define DIGIT_H  15
#define AMMO_X   25          // the plate's counter slots (2nd and 3rd of three)
#define AMMO_Y   1

static void draw_digit(int d, int x, int y)
{
    int sx = d == 0 ? 24 : d == 1 ? 36 : 40 + (d - 2) * 8, w = d == 1 ? 4 : 8;
    overlay_sheet(&hud, sx, DIGIT_SY, w, DIGIT_H, x + (8 - w) / 2, y, w, DIGIT_H, RGB15(31, 31, 31), 31, 24);
}

// Leon's plate (bottom right) with the health gauge, green FINE / yellow CAUTION / red DANGER,
// and the rounds in the magazine; a red flash when he gets hurt, a dark red veil once he's dead;
// the icon of an item just picked up (popup frames left, 0 = none).
#define POPUP_FRAMES 120
#define SPARE_LINE   11   // text lines used in the game (the menus clear them)
#define POPUP_LINE   10
#define CLEAR_LINE   9
#define PROMPT_LINE  8
#define MEDAL_LINE   7
#define OBJ_LINE     5
#define OBJ_TITLE_LINE 4
#define OBJ_HINT_LINE 3
static void draw_hud(int hp, int ammo, int flash, int dead, int popup_type, int popup)
{
    if (ammo > 99)
        ammo = 99;   // (two digits in the counter)
    overlay_begin();
    if (popup > 0)
    {
        // Icon left of the popup text (text line POPUP_LINE), rising in over the first frames.
        int ix, iy;
        drops_icon(popup_type, &ix, &iy);
        int rise = popup > POPUP_FRAMES - 8 ? (popup - (POPUP_FRAMES - 8)) * 2 : 0;
        int alpha = popup < 16 ? popup * 2 : 31;
        overlay_rect(20, 128 + rise, 236, 156 + rise, RGB15(0, 0, 0), alpha / 2 + 1, 25);
        overlay_sheet(&items, ix, iy, 32, 24, 26, 130 + rise, 32, 24, RGB15(31, 31, 31), alpha, 26);
    }
    if (dead)
        overlay_rect(0, 0, 256, 192, RGB15(10, 0, 0), 14, 20);
    else if (flash > 0)
        overlay_rect(0, 0, 256, 192, RGB15(31, 0, 0), flash, 20);

    overlay_sheet(&hud, PLATE_SX, PLATE_SY, PLATE_W, PLATE_H, PLATE_X, PLATE_Y, PLATE_W, PLATE_H, RGB15(31, 31, 31), 31, 21);
    // The life bar as the Zeebo version draws it: long, from under the middle of the screen to
    // the plate's right end, black where life is lost; it empties from the left.
    int right = PLATE_X + TRACK_X + TRACK_W, len = right - LIFE_BAR_X, y = PLATE_Y + TRACK_Y;
    overlay_rect(LIFE_BAR_X - 1, y - 1, right + 1, y + TRACK_H + 1, RGB15(0, 0, 0), 31, 21);
    int fill = len * hp / LEON_HP, x = right - fill;
    if (fill > 0)
    {
        // Caution: the red bar half over the green one turns it yellow-brown.
        int danger = hp <= LEON_HP / 3, caution = !danger && hp <= LEON_HP * 2 / 3;
        overlay_sheet(&hud, FILL_SX, danger ? RED_SY : GREEN_SY, 4, TRACK_H, x, y, fill, TRACK_H, RGB15(31, 31, 31), 31, 22);
        if (caution)
            overlay_sheet(&hud, FILL_SX, RED_SY, 4, TRACK_H, x, y, fill, TRACK_H, RGB15(31, 31, 31), 16, 23);
    }
    if (ammo >= 10)
        draw_digit(ammo / 10 % 10, PLATE_X + AMMO_X, PLATE_Y + AMMO_Y);
    draw_digit(ammo % 10, PLATE_X + AMMO_X + 8, PLATE_Y + AMMO_Y);
    overlay_end();
}

// Moves pos (x, z) dist along (dx, dz) a unit at a time as a walk would, kept out of the walls
// at the height of a floor at y (off a giant's back: never through a fence beside him).
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

// The view's width (degrees): the scope narrows it.
static void set_fov(int deg)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspectivef32(degreesToAngle(deg), floattof32(256.0 / 192.0), floattof32(NEAR_PLANE), floattof32(FAR_PLANE));
    glMatrixMode(GL_MODELVIEW);
}

// The rifle's scope as the video shows it: the view darkened all round a clear round lens in
// the middle, its black rim, and the red cross hairs.
static void draw_scope(void)
{
    overlay_begin();
    const int cx = 128 * 64, cy = 96 * 64, r = SCOPE_R * 64, rim = (SCOPE_R + 2) * 64, far = 300 * 64;
    glBindTexture(0, 0);
    for (int pass = 0; pass < 2; pass++)
    {
        // The veil (translucent) out to past the screen's edge, then the rim (solid).
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
    overlay_rect(128 - SCOPE_R + 4, 96, 128 + SCOPE_R - 4, 97, RGB15(31, 2, 2), 31, 42);   // cross hairs
    overlay_rect(128, 96 - SCOPE_R + 4, 129, 96 + SCOPE_R - 4, RGB15(31, 2, 2), 31, 43);
    overlay_end();
}

// Ashley's plate (2d_gms_00, the game's: her name, a life track and the round command icon),
// over Leon's, while she is with him: her life in the track as his, the icon of her following
// him (or the second, waiting) in the circle.
#define ASH_PLATE_SX 24       // (in the HUD sheet, the right half of 2d_gms_00): 104 x 27
#define ASH_PLATE_SY 32
#define ASH_PLATE_W  104
#define ASH_PLATE_H  27
#define ASH_PLATE_X  (256 - ASH_PLATE_W - 3)
#define ASH_PLATE_Y  (PLATE_Y - ASH_PLATE_H + 3)   // (right above Leon's, as in the video)
#define ASH_TRACK_X  2
#define ASH_TRACK_Y  14
#define ASH_TRACK_W  75
#define ASH_ICON_SX  20       // the command icons, 16 x 16 side by side
#define ASH_ICON_SY  16
#define ASH_ICON_X   86       // the circle's inside, in the plate
#define ASH_ICON_Y   9

static void draw_ash_hud(int hp, int max, int waiting)
{
    overlay_begin();
    overlay_sheet(&hud, ASH_PLATE_SX, ASH_PLATE_SY, ASH_PLATE_W, ASH_PLATE_H, ASH_PLATE_X, ASH_PLATE_Y, ASH_PLATE_W, ASH_PLATE_H, RGB15(31, 31, 31), 31, 21);
    int fill = hp > 0 ? (ASH_TRACK_W * hp + max - 1) / max : 0, y = ASH_PLATE_Y + ASH_TRACK_Y;
    int x = ASH_PLATE_X + ASH_TRACK_X + ASH_TRACK_W - fill;   // (empties from the left, as Leon's)
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

// What a Ganado whose mission point names a drop leaves (byte 2 of the point), as the game's
// drop table reads the code (bio4_brew.mod at 0x49f48; rolls are rand() % n). Leon only has the
// handgun here, so every kind of ammo is a handgun box, and the healing items (first aid spray,
// eggs) are green herbs; grenades, which he can't use, leave nothing.
static void code_drop(const int ground[3], int code)
{
    int type = -1, value = 0, r;
    switch (code)
    {
    case 1:                                    // green herb
    case 2:                                    // first aid spray
        type = DROP_HERB;
        break;
    case 5:                                    // 600..1000 PTAS
    case 6:                                    // 1100..1500 PTAS
        type = DROP_MONEY;
        value = (code == 5 ? 600 : 1100) + (int)(rand() % 5) * 100;
        break;
    case 7:                                    // handgun, shotgun, rifle or TMP ammo
    case 8:                                    // rifle, TMP, magnum ammo or mine darts
    case 9:                                    // ammo for the weapon in hand
        type = DROP_AMMO;
        break;
    case 10:                                   // a herb, a spray or nothing
        type = rand() % 3 ? DROP_HERB : -1;
        break;
    case 11:                                   // an egg (white, brown, gold) or nothing
        type = rand() % 4 ? DROP_HERB : -1;
        break;
    case 12:                                   // nothing, ammo (three kinds), a grenade or a herb
    case 13:
        r = rand() % 6;
        type = r == 5 ? DROP_HERB : r >= 1 && r <= 3 ? DROP_AMMO : -1;
        break;
    }                                          // (3, 4: hand / flash grenade)
    if (type >= 0)
        drops_spawn_value(&drops, ground, type, DROP_DELAY, value);
}

static void mission_drop(const Enemy *e)
{
    int ground[3] = { e->pos[0], e->feet, e->pos[2] };
    code_drop(ground, e->drop);
}

// A Ganado going down may leave an item where it falls; more likely a herb when Leon is hurt.
// One on a mission point with a drop code leaves that instead (mission_drop()).
static void maybe_drop(const Enemy *e, int leon_hp, int percent)
{
    if (e->drop >= 0)
    {
        mission_drop(e);
        return;
    }
    if ((int)(rand() % 100) >= percent)
        return;
    int herb_pct = leon_hp <= LEON_HP / 2 ? 70 : 35;
    int ground[3] = { e->pos[0], e->feet, e->pos[2] };
    drops_spawn(&drops, ground, (int)(rand() % 100) < herb_pct ? DROP_HERB : DROP_AMMO, DROP_DELAY);
}

// A broken crate may hold an item (as often as a fallen Ganado).
static void crate_drop(const Obj *o, int leon_hp)
{
    if (o->drop >= 0)   // a mission file's crate: what it names
    {
        code_drop(o->pos, o->drop);
        return;
    }
    if ((int)(rand() % 100) >= DROP_PERCENT)
        return;
    int herb_pct = leon_hp <= LEON_HP / 2 ? 70 : 35;
    drops_spawn(&drops, o->pos, (int)(rand() % 100) < herb_pct ? DROP_HERB : DROP_AMMO, 10);
}

// The status screen (SELECT): the game's Status screen opened on its weapon list (Armas), as
// its layout files place it on the 320 x 240 screen (Bio4_ImageLayout.bin records 285-293
// and 356-402, Bio4_StringLayout.bin 444-485), scaled here by 0.8 about the centre: the
// frame bars and list panels of 2d_gms_01 (the menu sheet, loaded only while it is open),
// four rows of item icon (the equipped weapon marked E), name, count with the ammo mark and
// its size in the case (TAM), the chosen row in the red frame (2d_gms_00, the HUD sheet),
// the money and the case's use at the bottom, and Leon on the right holding his gun. A on a
// row opens its commands in a small window.
#define ST_X(x)   (128 + (x) * 4 / 5)
#define ST_Y(y)   (96 + (y) * 4 / 5)
#define ST_S(n)   ((n) * 4 / 5)
#define ST_ROWS   4
// Sizes in the case (item_base_info.bin, the byte after the price): handgun 6, ammo / herb 2.
enum { ST_GUN, ST_AMMO, ST_HERB };
static const u8 st_tam[] = { 6, 2, 2 };

// Where an item's icon is in the items sheet (2d_itemic_00, laid out by item_icon_idx.bin:
// top row guns, then rounds; the sheet's last row is the first row's right half).
static void item_icon(int item, int *sx, int *sy)
{
    static const struct { u8 item, x, y; } at[] = {
        { 4, 0, 0 }, { 6, 32, 0 }, { 7, 64, 0 }, { 5, 96, 0 }, { 8, 0, 96 },
        { 20, 64, 96 }, { 22, 96, 96 }, { 23, 0, 24 }, { 21, 32, 24 }, { 24, 64, 24 }, { 9, 0, 48 } };
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
    overlay_sheet(&menu_sheet, sx, sy, sw, sh, ST_X(x), ST_Y(y), ST_S(w), ST_S(h), RGB15(31, 31, 31), 31, 25);
}

// A small window (the commands): corners and edges of 2d_gms_01 around a black inside, its
// top-left corner at (x, y), the inside w x h (layout units).
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

// The 2D part: frames, rows (icons, marks, the chosen row's frame), arrows, the command window.
static void draw_status(const StItem *it, int n, int top, int sel, int cmds)
{
    overlay_begin();
    if (menu_loaded)
    {
        menu_piece(0, 0, 240, 32, -160, -112, 320, 32);       // top bar
        menu_piece(80, 96, 156, 26, -160, -85, 208, 26);      // list head
        for (int i = 0; i < 5; i++)
            menu_piece(80, 32, 156, 32, -160, -59 + 32 * i, 208, 32);   // list panels
        menu_piece(0, 0, 240, 32, -160, 80, 320, 32);         // bottom bar
        menu_piece(203, 64, 37, 29, 54, 80, 37, 29);          // the attaché case
    }
    else   // (no room for the menu sheet: plain panels)
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
            drops_icon(DROP_HERB, &ix, &iy);
        else
            item_icon(t->type == ST_GUN ? weapons[t->weapon].item : AMMO_ITEM(t->weapon), &ix, &iy);
        overlay_rect(ST_X(-139), ST_Y(-59 + dy), ST_X(-139) + ST_S(32), ST_Y(-59 + dy) + ST_S(24), RGB15(1, 1, 1), 31, 26);   // (its dark box)
        overlay_sheet(&items, ix, iy, 32, 24, ST_X(-139), ST_Y(-59 + dy), ST_S(32), ST_S(24), RGB15(31, 31, 31), 31, 26);
        if (menu_loaded)
        {
            if (t->type == ST_GUN && t->weapon == wep)
                menu_piece(240, 112, 6, 7, -137, -58 + dy, 6, 7);    // E: equipped
        }
        if (top + r == sel)
        {
            // The red frame (the game's is 2d_gms_00 240,84; thin lines that would vanish at 0.8).
            int x0 = ST_X(-140), y0 = ST_Y(-61 + dy), x1 = ST_X(38), y1 = y0 + ST_S(31) - 1;
            overlay_line(x0, y0, x1, y0, RGB15(27, 7, 7), 27);
            overlay_line(x0, y1, x1, y1, RGB15(27, 7, 7), 27);
            overlay_line(x0, y0, x0, y1, RGB15(27, 7, 7), 27);
            overlay_line(x1, y0, x1, y1, RGB15(27, 7, 7), 27);
        }
    }
    // More rows above / below: small triangles where the game puts its arrows (-51, -77 / 66).
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

// Takes up weapon w: its model and animations in place of the last one's, each gun keeping its
// own rounds. Anything missing: back to the handgun, which is always there.
static void *wep_buf[2];

static void weapon_switch(int w, int *ammo, int *spare)
{
    wep_ammo[wep] = *ammo;
    wep_spare[wep] = *spare;
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
    *spare = wep_spare[w];
}

// The case: a gun's size, 2 for a box of rounds (of any gun, part-filled too) or a herb.
static int case_used(int spare, int herbs)
{
    int used = herbs * 2;
    for (int w = 0; w < WEP_COUNT; w++)
    {
        if (!(wep_owned >> w & 1))
            continue;
        int rounds = w == wep ? spare : wep_spare[w];
        used += weapons[w].tam + (weapons[w].box ? (rounds + weapons[w].box - 1) / weapons[w].box * 2 : 0);
    }
    return used;
}

// A stat for the upgrade list (it is kept x 10): 13, 1.2.
static void stat_text(char *s, int v)
{
    if (v % 10)
        sprintf(s, "%d.%d", v / 10, v % 10);
    else
        sprintf(s, "%d", v / 10);
}

// The save file, on the card the ROM runs from (nothing on a cart without a file system: the
// game then simply doesn't remember). Written to a second file first, so that a power cut
// can't lose the last good one.
#define SAVE_NAME   "re4ds.sav"
#define SAVE_MAGIC  0x53344552    // "RE4S"
#define SAVE_VER    1
typedef struct {
    u32 magic, version;
    int ptas, owned, case_level, herbs, weapon;
    int spare[WEP_COUNT];
    u8 level[WEP_COUNT][WEP_STATS];
    u32 check;
} SaveData;
static SaveData save_data;
static int save_ok, have_save;

static u32 save_sum(const SaveData *d)
{
    u32 sum = 0x1234;
    const u8 *b = (const u8 *)d;
    for (size_t i = 0; i < offsetof(SaveData, check); i++)
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
    int got = fread(&d, 1, sizeof(d), f) == sizeof(d);
    fclose(f);
    if (got && d.magic == SAVE_MAGIC && d.version == SAVE_VER && d.check == save_sum(&d)
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
    for (int w = 0; w < WEP_COUNT; w++)
        d->spare[w] = w == wep ? spare : wep_spare[w];
    memcpy(d->level, wep_level, sizeof(d->level));
    d->check = save_sum(d);
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

// Continue: all of it back, the weapon in hand too.
static void save_apply(int *ammo, int *spare, int *herbs, int *ptas)
{
    const SaveData *d = &save_data;
    memcpy(wep_level, d->level, sizeof(wep_level));
    wep_owned = d->owned;
    case_level = d->case_level;
    memcpy(wep_spare, d->spare, sizeof(wep_spare));
    for (int w = 0; w < WEP_COUNT; w++)
        wep_ammo[w] = wep_mag(w);
    *spare = wep_spare[wep];
    if (d->weapon != wep)
        weapon_switch(d->weapon, ammo, spare);
    *ammo = MAG_SIZE;
    *spare = wep_spare[wep];
    *herbs = d->herbs;
    *ptas = d->ptas;
}

// A new game: only the handgun, nothing bought, money enough for a gun and some rounds.
#define NEW_GAME_PTAS 30000
static void new_game(int *ammo, int *spare, int *herbs, int *ptas)
{
    if (wep != WEP_HANDGUN)
        weapon_switch(WEP_HANDGUN, ammo, spare);
    memset(wep_level, 0, sizeof(wep_level));
    wep_owned = 1;
    case_level = 0;
    for (int w = 0; w < WEP_COUNT; w++)
    {
        wep_ammo[w] = wep_mag(w);
        wep_spare[w] = 0;
    }
    *ammo = MAG_SIZE;
    *spare = SPARE_START;
    *herbs = 0;
    *ptas = NEW_GAME_PTAS;
}

// The shop's list: a row per thing on offer. Its frames are the status screen's.
enum { SHOP_ST_TABS, SHOP_ST_LIST };
enum { ROW_WEAPON, ROW_AMMO, ROW_HERB, ROW_CASE, ROW_SELL_WEAPON, ROW_SELL_HERB, ROW_UP_WEAPON, ROW_UP_STAT };
typedef struct { int kind, arg, price, ix, iy; char name[26], note[28]; } ShopRow;
#define SHOP_ROWS 12

static void draw_shop(const ShopRow *it, int n, int top, int sel)
{
    overlay_begin();
    if (menu_loaded)
    {
        menu_piece(0, 0, 240, 32, -160, -112, 320, 32);       // top bar
        menu_piece(80, 96, 156, 26, -160, -85, 320, 26);      // list head, the screen's width
        for (int i = 0; i < 5; i++)
            menu_piece(80, 32, 156, 32, -160, -59 + 32 * i, 320, 32);   // list panels
        menu_piece(0, 0, 240, 32, -160, 80, 320, 32);         // bottom bar
        menu_piece(203, 64, 37, 29, 54, 80, 37, 29);          // the attaché case
    }
    else
        overlay_rect(0, 0, 256, 192, RGB15(3, 3, 4), 31, 25);
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

// Title art, full screen.
static void draw_title(void)
{
    use_picture(PIC_TITLE);
    overlay_begin();
    overlay_sheet(&picture, 0, 0, picture.w, picture.h, 0, 0, 256, 192, RGB15(31, 31, 31), 31, 1);
    overlay_end();
}

// Stage select, one stage at a time: its number and place and the mission's name at the top,
// its picture (the original's, 2d_stage_00: 3 x 4 of 85 x 56, at twice the size) between red
// arrows, and the mission's lines below (the game's mission list, stgnfo_mission_ms.bin, in
// English). Left / Right scroll to the next one (the pictures slide across); once A confirms
// (zoom 0..ZOOM_FRAMES) the picture grows to fill the screen.
#define STAGES       12
#define ZOOM_FRAMES  30
#define SCROLL_FRAMES 10
#define SEL_X  43       // the picture: 170 x 112
#define SEL_Y  34
#define SEL_W  170
#define SEL_H  112

// Stage 6 (mission 6, the cabin siege) is left out of the game: not on the stage select, and
// the stages after it are numbered on from 6.
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

// from: the stage scrolled away from (-1: none); scroll: frames left of the slide; dir: +1 the
// new one comes in from the right.
static void draw_select(int sel, int from, int scroll, int dir, int tick, int zoom)
{
    use_picture(PIC_STAGESEL);
    overlay_begin();
    overlay_rect(0, 0, 256, 192, RGB15(2, 2, 3), 31, 1);
    int x = SEL_X, y = SEL_Y, w = SEL_W, h = SEL_H;
    if (zoom >= 0)
    {
        // Ease out towards the full screen.
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
        // The arrows, pulsing outwards a little.
        int p = 2 + sinLerp(tick * 700) * 2 / 4096, cy = y + h / 2;
        overlay_tri(8 - p, cy, 30 - p, cy - 18, 30 - p, cy + 18, RGB15(31, 0, 0), 31, 5);
        overlay_tri(248 + p, cy, 226 + p, cy - 18, 226 + p, cy + 18, RGB15(31, 0, 0), 31, 6);
    }
    overlay_end();
}
// Results: "TIME:" and the counter digits are the original's (2d_gm_00, from y 48 in the
// sheet: the clock graphic at y 1, the gold bar at y 169); the rest is game-font text.
#define RES_TIME_W  46
#define RES_DIGIT_Y 1
#define RES_DIGIT_H 15
#define RES_BAR_Y   169
static int res_digit(int d, int x, int y)
{
    int sx = d == 0 ? 48 : d == 1 ? 60 : d <= 6 ? 64 + (d - 2) * 8 : 108 + (d - 7) * 8, w = d == 1 ? 4 : 8;
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
        // TIME: mm:ss
        int x = 64, y = 66;
        overlay_sheet(&picture, 0, RES_DIGIT_Y, RES_TIME_W, RES_DIGIT_H, x, y, RES_TIME_W, RES_DIGIT_H, RGB15(31, 31, 31), 31, 3);
        x += RES_TIME_W + 6;
        int m = seconds / 60 > 99 ? 99 : seconds / 60, s = seconds % 60;
        x = res_digit(m / 10, x, y);
        x = res_digit(m % 10, x, y);
        overlay_sheet(&picture, 42, RES_DIGIT_Y, 4, RES_DIGIT_H, x, y, 4, RES_DIGIT_H, RGB15(31, 31, 31), 31, 5);   // ':'
        x += 6;
        x = res_digit(s / 10, x, y);
        res_digit(s % 10, x, y);
    }
    overlay_end();
}

// Vertical menu centred at (x centre, y top); the selected item in gold.
static void show_menu(int first_line, int cx, int y, const char *const *items, int count, int selected)
{
    for (int i = 0; i < count; i++)
        text_line(first_line + i, cx - text_width(items[i]) / 2, y + i * 18, i == selected ? TEXT_GOLD : TEXT_WHITE, items[i]);
}

// Up/Down move the selection (wrapping); returns the new index.
static int menu_move(u32 down, int selected, int count)
{
    if (down & KEY_UP)
        selected = (selected + count - 1) % count;
    if (down & KEY_DOWN)
        selected = (selected + 1) % count;
    return selected;
}

// Darkens the 3D layer (BG0) behind menus; sprites (the text) stay bright. 0 = off, up to 16.
static void dim_3d(int level)
{
    REG_BLDCNT = level ? BLEND_FADE_BLACK | BLEND_SRC_BG0 : 0;
    REG_BLDY = level;
}

// Model space -> world for Leon (position + rotation about Y, as model_draw() applies them).
static void leon_to_world(const int pos[3], int yaw, const int v[3], int out[3])
{
    int s = sinLerp(yaw), c = cosLerp(yaw);
    out[0] = pos[0] + mulf32(v[0], c) + mulf32(v[2], s);
    out[1] = pos[1] + v[1];
    out[2] = pos[2] - mulf32(v[0], s) + mulf32(v[2], c);
}

// Moves `cur` 1/8 of the way to `want` (simple exponential smoothing).
static void approach(int cur[3], const int want[3])
{
    for (int k = 0; k < 3; k++)
        cur[k] += (want[k] - cur[k]) >> 3;
}

int main(void)
{
    videoSetMode(MODE_0_3D);
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankB(VRAM_B_TEXTURE);
    vramSetBankD(VRAM_D_TEXTURE);
    vramSetBankE(VRAM_E_TEX_PALETTE);
    irqSet(IRQ_VBLANK, on_vblank);

    glInit();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ANTIALIAS);
    glEnable(GL_BLEND);   // translucent polygons (effects, HUD) blend instead of drawing solid
    glClearColor(HAZE_R, HAZE_G, HAZE_B, 31);   // the sky is the haze colour
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
    if (!err_2d)   // bottom screen: the vital screen (VRAM C)
        err_2d = vital_init(vitalbmp_bin, vitalbmp_bin_size, iconbmp_bin, iconbmp_bin_size) * 100000;
    if (!err_2d)   // stage gimmicks (fix_all models, 32x32 textures)
    {
        // Stage 1 has only ladders and crates (its gates and doors are in the stage model, and
        // it has no barrels or medallions): the other gimmick models stay unloaded, which
        // leaves texture VRAM for the Ganados' weapons.
        err_2d = model_load(&ladder_model, ladder_bin, ladder_bin_size) || model_load(&crate_model, crate_bin, crate_bin_size);
        err_2d *= 1000000;
    }
    if (err_stage || err_leon || err_anim || err_ground || err_gun || err_aim || err_gna || err_2d)
    {
        consoleDemoInit();   // text on the bottom screen instead
        printf("load failed: stage %d leon %d anim %d\nground %d gun %d aim %d ganado %d\n2d %d\n",
               err_stage, err_leon, err_anim, err_ground, err_gun, err_aim, err_gna, err_2d);
        while (1)
            swiWaitForVBlank();
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspectivef32(degreesToAngle(70), floattof32(256.0 / 192.0), floattof32(NEAR_PLANE), floattof32(FAR_PLANE));

#ifdef LEON_CIRCLE_X
    const int circle_x = inttof32(LEON_CIRCLE_X), circle_z = inttof32(LEON_CIRCLE_Z);
#else
    const int circle_x = stage.ground[0], circle_z = stage.ground[2];
#endif
    const int circle_radius = inttof32(LEON_CIRCLE_R);

    // Leon starts at the game's start point outside the village; test builds start on the walk
    // circle in the village square (facing +Z), or at LEON_START_X/Z. His feet are at his bbox
    // minimum.
    int leon_feet = stage.ground[1];
#if defined(LEON_START_X)   // world units
    int leon_pos[3] = { inttof32(LEON_START_X), 0, inttof32(LEON_START_Z) };
#elif defined(AUTOPILOT) && !defined(STAGE_START)
    int leon_pos[3] = { circle_x, 0, circle_z + circle_radius };
#else
    int leon_pos[3] = { floattof32(STAGE_START_X), 0, floattof32(STAGE_START_Z) };
#endif
    // Start on the highest floor there (fine on open ground; a start under a roof would land on it).
    ground_height(&stage_ground, leon_pos[0], leon_pos[2], inttof32(10000), 0, &leon_feet);
    leon_pos[1] = leon_feet - leon.bbox_min[1];
#if defined(LEON_START_YAW)   // degrees, 0 = facing +Z
    int leon_yaw = degreesToAngle(LEON_START_YAW);
#elif defined(AUTOPILOT) && !defined(STAGE_START)
    int leon_yaw = 0;
#else
    int leon_yaw = degreesToAngle(STAGE_START_YAW);
#endif
    objects_init(&objs, &stage_ground, &door_model, &ladder_model, &barrel_model, &crate_model, &medal_model, &boom_sheet, &spark);
    objs.bell = &bell_model;
    enemies_init(&ganados, &ganado, &ganado_anims, &stage_ground, &objs, ganado_spawn, GANADO_COUNT, DEGREES_IN_CIRCLE / 2);
    {
        const Model *const weapons[WEAPON_KINDS] = { NULL, &sickle_model, &axe_model, &fork_model };
        enemies_arm(&ganados, weapons, &ganado_w0, &ganado_w1);
    }
    const int start_pos[3] = { leon_pos[0], leon_pos[1], leon_pos[2] };
    const int start_feet = leon_feet, start_yaw = leon_yaw;
    int leon_angle = 0, anim_frame = 0;
    int leon_anim = LEON_START_MODE;   // free camera: what START cycles through
    int ctrl = START_CTRL;
    int aim_w = 0;       // f32 0..1: how far the aim pose is blended in
    int aim_tilt = 0;    // f32 -1 (down) .. +1 (up)
    int gun_state = GUN_HOLSTERED;
    int draw_frame = 0;  // f32 position in GUN_DRAW while drawing/holstering
    int clip_key = -1;   // identifies the animation playing, to cross-fade on changes
    int xfade_w = 0;     // f32 1..0: weight of pose_from still mixed in
    int ammo = MAG_SIZE;
    int wep_note = 0;        // frames left showing the name of the weapon just taken up
    int gclimb = 0, gclimb_f[2] = { 0, 0 }, gclimb_y = 0, gcut_page = 0;   // the giant Leon climbs: which, his facing, the floor
    int rocket_on = 0, rocket_pos[3] = { 0 }, rocket_to[3] = { 0 }, rocket_boom = 0;
    int scoping = 0, scope_was = 0, scope_dir[3] = { 0, 0, inttof32(1) };   // the rifle's scope: looking through it this frame, last frame
    int leon_fade = 31;      // his alpha: the camera pressed against him by a wall fades him
    int shop_screen = SHOP_ST_TABS, shop_tab = 1, shop_sel = 0, shop_top = 0, shop_wep = -1, shop_msg = 0;
    const char *shop_text = "";
    int fire_frame = -1;     // f32 position in the fire animation, -1 when not firing
    int reload_frame = -1;   // f32 position in GUN_RELOAD, -1 when not reloading
    int reload_ch = -1;      // sound channel of the reload, to cut it if the reload is abandoned
    int flash_timer = 0, impact_timer = 0, cam_kick = 0;
    int impact_pos[3] = { 0 }, muzzle[3] = { 0 }, laser_end[3] = { 0 }, laser_hit = 0;
    int laser_enemy = -1, laser_head = 0, impact_blood = 0, laser_obj = -1, laser_ash = 0, laser_giant = 0, laser_gal = 0, laser_gal_back = 0, laser_ver = 0, laser_drop = -1;
    int ash_lost = 0;               // frames since Ashley went down (game over after a while)
    u32 hits = 0, headshots = 0;
    u32 shots = 0;
    int leon_hp = LEON_HP;
    int react = REACT_NONE;   // reaction to an attack overriding his controls and animation
    int react_frame = 0;      // f32 position in the reaction's clip
    int holder = -1;          // the Ganado holding him (REACT_HELD)
    int freed_from = -1;      // the one he broke free of (REACT_BREAK_FREE: kicked at KICK_FRAME)
    int mash = 0, held_frames = 0, safe_timer = 0, hurt_flash = 0;
    int slash_frame = -1;           // f32 position in the knife slash, -1 = not slashing
    int slash_high = 0;             // the slash under way is the high one
    int knife_w = 0;                // f32 0..1: how far the view has moved in for the knife
    int act_obj = -1;               // the gate / crate / ladder of the action under way
    int drop_to = 0, drop_vy = 0;   // dropping off a ledge: floor below, falling speed (f32)
    int next_ganado = 0;            // the next Ganado to come into the stage
#ifndef START_STAGE                // test builds: -DSTART_STAGE=n (1..12) with -DSKIP_TITLE
#define START_STAGE 1
#endif
    int cur_stage = START_STAGE - 1;  // the stage being played (0..STAGE_COUNT-1)
    int obj_t = 0;                 // frames the objective stays on screen
#ifdef TEST_SHOP_PTAS
    int ptas = TEST_SHOP_PTAS;
#elif defined(TEST_SHOP)
    int ptas = 60000;
#else
    int ptas = 0;                   // money earned (PTAS)
#endif
    int treasures_found = 0;        // this mission's treasures taken
    int boss_phase = 0, boss_t = 0; // mission 1: 0 villagers, 1 the chainsaw man, 2 over
    int medal_note = 0;             // frames left of the medallion count note
    int elevator_note = 0;          // stage 12: frames left of the elevator's arrival note
    ObjEvents oev;
    EnemyEvents ev;

    // Free camera: position + yaw/pitch. Follow camera: smoothed eye and target.
    int eye[3] = { leon_pos[0], leon_feet + inttof32(14), leon_pos[2] + inttof32(45) };
    int yaw = 0, pitch = -degreesToAngle(8);
    // Start at the follow position (approach() then keeps it there).
    int follow_eye[3] = { leon_pos[0] - mulf32(sinLerp(leon_yaw), CAM_BACK) - mulf32(cosLerp(leon_yaw), CAM_RIGHT),
                          leon_feet + CAM_HEIGHT,
                          leon_pos[2] - mulf32(cosLerp(leon_yaw), CAM_BACK) + mulf32(sinLerp(leon_yaw), CAM_RIGHT) };
    int follow_target[3] = { leon_pos[0] + mulf32(sinLerp(leon_yaw), CAM_AHEAD) - mulf32(cosLerp(leon_yaw), CAM_RIGHT),
                             leon_feet + CAM_AT_HEIGHT,
                             leon_pos[2] + mulf32(cosLerp(leon_yaw), CAM_AHEAD) + mulf32(sinLerp(leon_yaw), CAM_RIGHT) };
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

#if (defined(AUTOPILOT) && AUTOPILOT != 10 && AUTOPILOT != 35 && AUTOPILOT != 41 && AUTOPILOT != 48 && AUTOPILOT != 51) || defined(SKIP_TITLE)   // test builds go straight into the game
    int mode = MODE_PLAY;
#else
    int mode = MODE_TITLE;
#endif
#ifdef TEST_SHOP   // test builds: into the shop, with money
    mode = MODE_SHOP;
#endif
    int menu_sel = 0, dead_timer = 0;
    int reset = mode == MODE_PLAY;   // set by the menus: start the game over (Leon, Ganados, camera)
    int pending = -1, fade = 0;   // screen change waiting for the fade to black, its level 0..16
    int stage_sel = 0, zoom = -1, shake = 0, tick = 0;   // stage select
    int scroll = 0, scroll_from = -1, scroll_dir = 1;   // ...its slide from one stage to the next
    int spare = SPARE_START;      // rounds carried besides the magazine
    int herbs = 0;                // green herbs carried (used from the item screen)
    int popup = 0, popup_type = 0, popup_value = 0;
#ifdef TEST_CUTIN   // test builds: the stage's opening scene plays
    int cut_opening = 1;
#else
    int cut_opening = 0;   // the mission's opening scene is due (entered from the stage select)
#endif
    int kneel_seen = 0, gal_seen = 0, ash_waiting = 0, giant_paid = 0;   // events seen this mission
    int st_sel = 0, st_top = 0, st_cmd = -1, st_msg = 0;   // status screen: chosen row, first row shown, command (-1: none open), note frames
    int clear_t = -1;             // frames since the stage was cleared, -1 = not yet
    u32 play_frames = 0;          // time in the stage (not counting pauses)
    int damage_taken = 0, result_t = 0;
    drops_init(&drops, &star, &beam, &spark, &items);
    static const char *const title_items[] = { T_CONTINUE, T_NEW_GAME, T_HOWTO };
    static const char *const over_items[] = { T_CONTINUE, T_TO_TITLE };
    static const char *const howto[] = { T_HOWTO_KEYS };
    int menu_shown = -1;   // which screen's text is up
    while (1)
    {
        scanKeys();
        u32 down = keysDown(), held = keysHeld();
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
        // Taps on the vital screen: the ammo slot, the herb slot (selects), the herb slot slowly
        // again (nothing), then double taps on the herbs (use, use: now full, refused) and on
        // the ammo slot (nothing).
        static const struct { u16 at; u8 x, y; } taps[] = {
            { 300, 132, 118 }, { 400, 208, 118 }, { 500, 208, 118 },
            { 600, 208, 118 }, { 610, 208, 118 }, { 700, 208, 118 }, { 710, 208, 118 },
            { 800, 208, 118 }, { 810, 208, 118 }, { 900, 132, 118 }, { 910, 132, 118 } };
        for (unsigned i = 0; i < sizeof(taps) / sizeof(taps[0]); i++)
            if (frame_no == taps[i].at)
                vital_fake_touch(taps[i].x, taps[i].y);
#endif
#endif

        // Screen changes that fade: to black (no input meanwhile), switch, back in.
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

        // Title and controls screens: the title art and the menu; the game doesn't run.
        if (mode == MODE_TITLE || mode == MODE_HOWTO)
        {
            if (menu_shown != mode)
            {
                text_clear();
                menu_sel = menu_shown == MODE_HOWTO ? 1 + have_save : 0;   // back from the controls: still on that item
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
                    int item = menu_sel + !have_save;   // 0 Continue, 1 New Game, 2 Controls
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
                model_sync();   // (the last character list done)
                glFlush(0);
            }
            else
            {
                text_clear();   // to the stage select
                dim_3d(0);
                menu_shown = -1;
            }
            swiWaitForVBlank();
            text_commit();
            vital_idle(T_CAPTION, T_EDITION);
            continue;
        }

        // Stage select (see draw_select()): D-pad picks, A confirms (the picture zooms to fill
        // the screen, then the game starts), B goes back to the title.
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
                    while (stage_sel == STAGE_DROPPED);   // (stage 6 is left out)
                    scroll = SCROLL_FRAMES;
                    scroll_dir = step;
                    sfx_play(SFX_MENU, 60, 64);
                }
                if ((down & (KEY_A | KEY_START)) && scroll == 0)
                {
                    sfx_play(SFX_MENU, 100, 64);   // every stage is open
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
            // Its number and place, the mission's name, and its lines below the picture.
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
                model_sync();   // (the last character list done)
                glFlush(0);
            }
            else
                menu_shown = -1;
            swiWaitForVBlank();
            text_commit();
            vital_idle(T_CAPTION, T_EDITION);
            continue;
        }

        // The shop, between the stage select and the mission (the game's Iniciar, Comprar, Vender,
        // Upgrade tabs and the merchant's lines). Prices and upgrades are the game's; a box of
        // rounds is one press, instead of the game's question how many; things sell for half.
        if (mode == MODE_SHOP)
        {
            static const char *const tabs[] = { T_SHOP_START, T_SHOP_BUY, T_SHOP_SELL, T_SHOP_UPGRADE };
            static const s8 tab_x[] = { -116, -55, 10, 69 };          // (Bio4_StringLayout.bin 41-44)
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
                sheet_free(&picture);   // (the stage select's: its room goes to the menu sheet)
                picture_id = -1;
                if (!menu_loaded)
                    menu_loaded = sheet_load(&menu_sheet, menu_bin, menu_bin_size) == 0;
                glClearColor(0, 0, 0, 31);
            }
            music_play(MUSIC_TITLE, MUSIC_VOLUME);
            int n = 0;
            ShopRow *r = rows;
            #define ROW(k, a, p, it) (r = &rows[n++], *r = (ShopRow){ .kind = k, .arg = a, .price = p }, \
                                      (it) < 0 ? drops_icon(DROP_HERB, &r->ix, &r->iy) : item_icon(it, &r->ix, &r->iy))
            if (shop_tab == 1)
            {
                for (int w = 0; w < WEP_COUNT; w++)
                {
                    ROW(ROW_WEAPON, w, weapons[w].price, weapons[w].item);
                    snprintf(r->name, sizeof(r->name), "%s", weapons[w].name);
                    if (wep_owned >> w & 1)
                        snprintf(r->note, sizeof(r->note), T_SHOP_OWNED);
                    else
                        snprintf(r->note, sizeof(r->note), T_SHOP_PRICE, r->price);
                }
                for (int w = 0; w < WEP_COUNT; w++)
                    if ((wep_owned >> w & 1) && weapons[w].box)
                    {
                        ROW(ROW_AMMO, w, weapons[w].box * weapons[w].ammo_price, AMMO_ITEM(w));
                        snprintf(r->name, sizeof(r->name), T_SHOP_AMMO, weapons[w].name);
                        snprintf(r->note, sizeof(r->note), T_SHOP_BOX, weapons[w].box, r->price);
                    }
                ROW(ROW_HERB, 0, 1000, -1);
                snprintf(r->name, sizeof(r->name), T_ITEM_HERB);
                snprintf(r->note, sizeof(r->note), T_SHOP_PRICE, r->price);
                if (case_level < 3)
                {
                    ROW(ROW_CASE, 0, case_price[case_level + 1], 0);
                    snprintf(r->name, sizeof(r->name), T_SHOP_CASE_NAME);
                    snprintf(r->note, sizeof(r->note), T_SHOP_CASE, case_tam[case_level + 1], r->price);
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
                        continue;   // (this gun doesn't have it)
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
                                wep_spare[w] = 0;
                            }
                        }
                        else if (c->kind == ROW_AMMO)
                        {
                            int rounds = w == wep ? spare : wep_spare[w];
                            int need = rounds % weapons[w].box ? 0 : 2;   // (a part-filled box takes no more room)
                            if (ptas < c->price)
                                why = T_SHOP_MONEY;
                            else if (room < need)
                                why = T_SHOP_SPACE;
                            else if (w == wep)
                                spare += weapons[w].box;
                            else
                                wep_spare[w] += weapons[w].box;
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

            char m[28], lead[16];
            snprintf(lead, sizeof(lead), "%d ", ptas);
            snprintf(m, sizeof(m), T_SHOP_PRICE, ptas);
            text_line(13, ST_X(-84) - text_width(lead), ST_Y(89), TEXT_WHITE, m);
            snprintf(m, sizeof(m), "%03d/%03d", case_used(spare, herbs), case_tam[case_level]);
            text_line(14, ST_X(91), ST_Y(89), TEXT_WHITE, m);
            if (shop_screen == SHOP_ST_TABS)
            {
                text_line(0, ST_X(-140), ST_Y(-103), TEXT_WHITE, T_MISSION_SHOP);
                text_line(1, ST_X(-100), ST_Y(-50), TEXT_WHITE, T_SHOP_LINE_1);
                text_line(2, ST_X(-100), ST_Y(-34), TEXT_WHITE, T_SHOP_LINE_2);
                text_line(3, ST_X(-100), ST_Y(-18), TEXT_WHITE, T_SHOP_LINE_3);
                for (int i = 0; i < 4; i++)
                    text_line(15 + i, ST_X(tab_x[i]), ST_Y(49), i == shop_tab ? TEXT_GOLD : TEXT_GREY, tabs[i]);
                draw_shop(rows, 0, 0, 0);
            }
            else
            {
                if (shop_msg > 0)
                {
                    shop_msg--;
                    text_line(0, ST_X(-140), ST_Y(-103), TEXT_PINK, shop_text);
                }
                else
                    text_line(0, ST_X(-140), ST_Y(-103), TEXT_WHITE, tabs[shop_tab]);
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
            if (pending >= 0 && fade + 2 >= 16)   // (the screen is black: gone next frame)
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

        // Results (English, see draw_result()): lines appear one after another, then A goes
        // on to the stage select.
        if (mode == MODE_RESULT)
        {
            static char stats[5][24], rank[2];
            static const char *const labels[] = { T_ACCURACY, T_LIFE, T_KILLS, T_HEADSHOTS, T_TREASURE };
            int seconds = (int)(play_frames / 60);
            if (menu_shown != MODE_RESULT)
            {
                text_clear();
                dim_3d(0);
                music_stop();
                result_t = 0;
                menu_shown = MODE_RESULT;
                // As the game's MISSÃO COMPLETA: accuracy, life left, attack (here, kills),
                // a total rank, and the mission's PTAS.
                int kills = ganados.kills;
                int acc = shots ? (int)(hits * 100 / shots) : 0;
                snprintf(stats[0], sizeof(stats[0]), "%d%%", acc);
                snprintf(stats[1], sizeof(stats[1]), "%d", leon_hp);
                snprintf(stats[2], sizeof(stats[2]), "%d", kills);
                snprintf(stats[3], sizeof(stats[3]), "%lu", headshots);
                snprintf(stats[4], sizeof(stats[4]), "%d/%d", treasures_found, stage_info[cur_stage].treasures);
                // Rank: up to 3 points each for accuracy, life left and speed.
                int attack = seconds <= 120 ? 3 : seconds <= 240 ? 2 : 1;
                int pts = (acc >= 80 ? 3 : acc >= 60 ? 2 : 1) + (leon_hp >= 90 ? 3 : leon_hp >= 50 ? 2 : 1) + attack;
                ptas += MISSION_PTAS;
                rank[0] = pts >= 9 ? 'S' : pts >= 7 ? 'A' : pts >= 5 ? 'B' : 'C';
                rank[1] = 0;
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
                text_line(8, 128 - text_width(p) / 2, 160, TEXT_GOLD, p);   // between TOTAL and the rank
                text_big(6, 180, 152, rank[0] == 'S' || rank[0] == 'A' ? TEXT_GOLD : TEXT_WHITE, rank);
            }
            if (result_t == RESULT_STEP * 7)
                sfx_play(SFX_ITEM_GET, 110, 64);
            else if (result_t % RESULT_STEP == 0 && step >= 1 && step <= 6)
                sfx_play(SFX_EMPTY, 90, 64);   // a click per line
            if (step >= 8 && (tick / 30) % 2 == 0)
                text_line(7, 128 - text_width(T_PRESS_A) / 2, 180, TEXT_GREY, T_PRESS_A);
            else
                text_hide(7);
            if (step >= 8 && (down & (KEY_A | KEY_START)))
            {
                sfx_play(SFX_MENU, 100, 64);
                text_clear();
                pending = MODE_SELECT;   // back to the stage select
                save_write(spare, herbs, ptas);
            }
            draw_result(seconds, step);
            model_sync();   // (the last character list done)
            glFlush(0);
            swiWaitForVBlank();
            text_commit();
            vital_idle(T_CAPTION, T_EDITION);
            continue;
        }

        // A story scene (see cut_start()): drawn on the sub screen's bitmap (vital_cutin(): it
        // needs no texture memory, which a stage may have filled), with the screens swapped so
        // it shows on top; the 3D screen, below meanwhile, is black.
        if (mode == MODE_CUTIN)
        {
            if (menu_shown != MODE_CUTIN)
            {
                text_clear();
                dim_3d(0);
                menu_shown = MODE_CUTIN;
                cut_pic_id = -1;   // (the page shown)
                lcdMainOnBottom();
            }
            if (pending < 0 && cut_page < cut->count)
            {
                if ((down & KEY_A) || ++cut_t >= CUT_AUTO)
                {
                    cut_page++;
                    cut_t = 0;
                    if (cut_after == CUT_AFTER_GIANT && cut_page == 1)
                        sfx_play(SFX_KNIFE, 127, 64);   // (the tentacle cut)
                }
                if (down & KEY_START)
                    cut_page = cut->count;
                if (cut_page >= cut->count)
                {
                    if (cut_after == CUT_AFTER_GIANT)
                        gcut_page = 2;   // (Leon comes down off him)
                    if (cut_fade)
                        pending = cut_then;
                    else
                    {
                        mode = cut_then;
                        menu_shown = -1;
                    }
                    down = 0;   // (the press that ended it is not also the game's)
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
            setBrightness(2, -fade);   // (the fades: the sub screen too, now on top)
            if (mode != MODE_CUTIN || (pending >= 0 && fade + 2 >= 16))   // (black: the next frame is another screen)
            {
                vital_cutin(NULL, 0, NULL, NULL);   // (black, before it goes back below)
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
        // Pause: no new 3D frame is drawn, so the last one stays on screen, dimmed under the menu.
        if (mode == MODE_PLAY && ctrl == CTRL_LEON && (down & KEY_START))
        {
            mode = MODE_PAUSE;
            menu_sel = 0;
            down = 0;
        }
        // The pause menu is the game's Opções screen, as its layout file (Bio4_StringLayout.bin,
        // records 288-310) places it on the 320 x 240 screen, scaled here by 0.8 about the
        // centre: the title, the items (white, the chosen one gold), the chosen item's help
        // line in pink below, and for Retry / Quit a question with YES / NO (gold / grey).
        // Sound sets the volume (OFF, Low, Medium, High); Controls shows the controls (the
        // game's control types and Vibration have nothing to set on the DS).
        if (mode == MODE_PAUSE)
        {
            static const char *const items[] = { T_OPT_RETRY, T_OPT_QUIT, T_OPT_SOUND, T_OPT_CONTROLS };
            const char *const helps[] = { T_HELP_RETRY, FOES_ON(cur_stage) ? T_HELP_QUIT : T_HELP_LEAVE, T_HELP_SOUND, T_HELP_CONTROLS };
            static const char *const vols[] = { T_VOL_OFF, T_VOL_LOW, T_VOL_MID, T_VOL_HIGH };
            static const char *const yes_no[] = { T_YES, T_NO };
            static const u8 vol_pct[] = { 0, 40, 70, 100 };
            static int sub, sub_sel, sound = 3;   // sub: 0 the list, 1 retry?, 2 quit?, 3 sound, 4 controls
            if (menu_shown != MODE_PAUSE)
            {
                text_clear();
                menu_shown = MODE_PAUSE;
                sub = 0;
            }
            dim_3d(sub == 4 ? 12 : 10);
            music_volume(MUSIC_VOLUME / 3);   // quieter while paused
            int back = (down & KEY_B) != 0, pick = (down & KEY_A) != 0, shown = sub;
            if (sub == 0)
            {
                menu_sel = menu_move(down, menu_sel, 4);
                if (down & KEY_START || back)
                {
                    mode = MODE_PLAY;   // (back to the game)
                }
                else if (pick)
                {
                    sfx_play(SFX_MENU, 100, 64);
                    sub = menu_sel + 1;
                    sub_sel = sub == 3 ? sound : 1;   // (the questions start on NO)
                }
            }
            else if (sub == 1 || sub == 2)
            {
                sub_sel = menu_move(down, sub_sel, 2);
                if (back || (pick && sub_sel == 1))
                    sub = 0;
                else if (pick)
                {
                    sfx_play(SFX_MENU, 100, 64);
                    mode = sub != 2 ? MODE_PLAY : FOES_ON(cur_stage) ? MODE_TITLE : MODE_SELECT;   // (an empty stage doesn't clear: he leaves with what he found)
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
                // The controls, as on the title's controls screen.
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
                text_line(10, 128 - text_width(T_OPTIONS) / 2, 13, TEXT_WHITE, T_OPTIONS);           // y -104
                if (sub == 1 || sub == 2)
                {
                    const char *q = sub == 1 ? T_ASK_RETRY : FOES_ON(cur_stage) ? T_ASK_QUIT : T_ASK_LEAVE;
                    text_line(4, 128 - text_width(q) / 2, 96, TEXT_WHITE, q);                     // y 4
                    for (int i = 0; i < 2; i++)                                                     // y 28, 52
                        text_line(5 + i, 128 - text_width(yes_no[i]) / 2, 115 + i * 19, i == sub_sel ? TEXT_GOLD : TEXT_GREY, yes_no[i]);
                }
                else if (sub == 3)
                    for (int i = 0; i < 4; i++)                                                     // y -44 ...
                        text_line(i, 128 - text_width(vols[i]) / 2, 58 + i * 19, i == sub_sel ? TEXT_GOLD : TEXT_WHITE, vols[i]);
                else
                    for (int i = 0; i < 4; i++)                                                     // y -68, -44, -20, 4
                        text_line(i, 128 - text_width(items[i]) / 2, 39 + i * 19, i == menu_sel ? TEXT_GOLD : TEXT_WHITE, items[i]);
                const char *h = helps[sub == 0 ? menu_sel : sub - 1];
                text_line(9, 128 - text_width(h) / 2, 150, TEXT_PINK, h);                          // y 73
            }
            swiWaitForVBlank();
            text_commit();
            continue;
        }

        // The status screen (SELECT; see draw_status()): the game stands still meanwhile.
        if (mode == MODE_STATUS)
        {
            // Its rows: the handgun (rounds in it), the handgun ammo in boxes of up to
            // AMMO_STACK, each green herb.
            StItem list[24];
            int n = 0, used = 0;
            for (int w = 0; w < WEP_COUNT; w++)
                if (wep_owned >> w & 1)
                    list[n++] = (StItem){ ST_GUN, w == wep ? ammo : wep_ammo[w], w };
            for (int s = weapons[wep].box ? spare : 0; s > 0 && n < 18; s -= weapons[wep].box)
                list[n++] = (StItem){ ST_AMMO, s < weapons[wep].box ? s : weapons[wep].box, wep };
            for (int h = 0; h < herbs && n < 24; h++)
                list[n++] = (StItem){ ST_HERB, 1, 0 };
            used = case_used(spare, herbs);
            if (menu_shown != MODE_STATUS)
            {
                text_clear();
                menu_shown = MODE_STATUS;
                if (!menu_loaded)
                    menu_loaded = sheet_load(&menu_sheet, menu_bin, menu_bin_size) == 0;
                glClearColor(0, 0, 0, 31);
            }
            if (st_sel >= n)
                st_sel = n - 1;
            // The commands of the chosen row (the game's: Equipar / Cancelar for the weapon,
            // Descartar / Cancelar for ammo, Usar / Descartar / Cancelar for a herb; Combinar
            // and discarding the only weapon are left out).
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
                    // Back to the game.
                    mode = MODE_PLAY;
                    text_clear();
                    menu_shown = -1;
                    if (menu_loaded)
                        sheet_free(&menu_sheet);
                    menu_loaded = 0;
                    set_haze(stage_info[cur_stage].haze);
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
                        st_msg = 90;   // (nothing to heal)
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
                // Leon on the right, holding his gun ready, turned a little towards the list.
                static NodeLocal st_local[MODEL_MAX_NODES];
                static int st_pose[MODEL_MAX_NODES][12];
                anim_sample(&leon_gun, GUN_READY_POSE, 0, &leon, st_local);
                anim_compose(&leon, st_local, st_pose);
                int h = leon.bbox_max[1] - leon.bbox_min[1];
                int ex = h * 3 / 4, ey = h / 2, ez = h * 5 / 4;
                // Behind him, the list's panels carried on to the screen's right edge (the case's
                // dark inside; with nothing there the 3D layer's black showed past the list).
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
                draw_status(list, n, st_top, st_sel, st_cmd >= 0 ? ncmd : 0);   // (no life bar here)

                // The text (Bio4_StringLayout.bin 444-485).
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
                text_line(13, ST_X(-84) - text_width(lead), ST_Y(89), TEXT_WHITE, m);   // "PTAS" where the game puts it
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

        // The village theme plays while Leon lives (from the start on a new game or continue);
        // it stops when he dies.
        if (reset)
            music_play(MUSIC_VILLAGE, MUSIC_VOLUME);
        else if (clear_t >= BELL_FIRST)
        {
            // Stage clear: the music fades out under the bell.
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

        // Game over: the scene goes on behind the menu; Leon takes no input.
        if (mode == MODE_GAMEOVER)
        {
            if (menu_shown != MODE_GAMEOVER)
            {
                text_clear();
                menu_sel = 0;
                menu_shown = MODE_GAMEOVER;
            }
            // Ignore buttons for a moment, so mashing (breaking free) doesn't pick an item.
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
            if (held & KEY_Y)   // Y + SELECT
                cull = (cull + 1) % 3;
            else if (ctrl == CTRL_LEON && !(held & KEY_L))
            {
                // The status screen (the game's button 3), when Leon is free to act.
                if (mode == MODE_PLAY && react == REACT_NONE)
                {
                    mode = MODE_STATUS;
                    st_sel = st_top = 0;
                    st_cmd = -1;
                }
            }
            else if (ctrl == CTRL_LEON)   // L + SELECT
            {
                // Hand over to the free camera where the follow camera is, looking the same way.
                ctrl = CTRL_FREECAM;
                for (int k = 0; k < 3; k++)
                    eye[k] = follow_eye[k];
                yaw = DEGREES_IN_CIRCLE / 2 - leon_yaw;
                pitch = -degreesToAngle(8);
                leon_anim = ANIM_IDLE;
                gun_state = GUN_HOLSTERED;   // the free-camera demo walks without the gun
            }
            else
            {
                ctrl = CTRL_LEON;
                anim_frame = 0;
            }
        }

        if (reset)
        {
            // New game: back to the start point, gun holstered, full health, Ganados back in
            // place, camera right behind Leon.
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
            // Every gun loaded; the rounds, herbs and money carry on from mission to mission.
            for (int w = 0; w < WEP_COUNT; w++)
                wep_ammo[w] = wep_mag(w);
            ammo = MAG_SIZE;
            shot_bank = 0;
#ifdef TEST_ALL_WEAPONS   // test builds: every gun, with some rounds
            wep_owned = (1 << WEP_COUNT) - 1;
            for (int w = 1; w < WEP_COUNT; w++)
                wep_spare[w] = weapons[w].mag * 6;
#endif
            leon_hp = LEON_HP;
#ifdef TEST_HURT   // test builds: start hurt, with herbs
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
            frames = 0;   // the FPS counter only counts game frames: start it afresh
            fps = 60;
            fps_mark = vblank_count;
#ifdef TEST_DROPS   // test builds: an ammo box and a herb in front of Leon, to look at
            for (int k = 0; k < 2; k++)
            {
                int p[3] = { leon_pos[0] + inttof32(k ? 9 : -6), leon_feet, leon_pos[2] + inttof32(14) };
                ground_height(&stage_ground, p[0], p[2], leon_feet, STEP_UP, &p[1]);
                drops_spawn(&drops, p, k ? DROP_HERB : DROP_AMMO, 0);
            }
#endif
            objects_reset(&objs);
            // Play needs no menu picture (its VRAM goes to the chainsaw man when he comes).
            sheet_free(&picture);
            picture_id = -1;
            cha_unload();
            // The chosen stage: its model, ground and haze.
            stage_budget = -1;   // (a new stage: all of it to start with)
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
            if (cur_stage == 0)
            {
                objects_init(&objs, &stage_ground, &door_model, &ladder_model, &barrel_model, &crate_model, &medal_model, &boom_sheet, &spark);
                objs.bell = &bell_model;
            }
            else
                objects_clear(&objs);   // the gimmicks (ladders, crates) are placed for stage 1 only
            // The mission's treasures, glinting where the game puts them: on the floor they are
            // walked over; up high, a shot knocks them down onto the floor below.
            treasures_found = 0;
            {
                const StageInfo *si = &stage_info[cur_stage];
                for (int i = 0; i < si->treasures; i++)
                {
                    int p[3] = { inttof32(si->treasure[i][0]), inttof32(si->treasure[i][1]), inttof32(si->treasure[i][2]) };
                    // Where it lands: the floor well below it (not the roof or beam it sits on),
                    // pushed clear of the walls (one on the eaves would come down against the house).
                    // None that far below: it lies where it is.
                    int land[3] = { p[0], p[1], p[2] };
                    ground_height(&stage_ground, p[0], p[2], p[1] - inttof32(8), 0, &land[1]);
                    ground_collide(&stage_ground, &land[0], &land[2], land[1] + STEP_UP, land[1] + LEON_HEIGHT, inttof32(6));
                    ground_height(&stage_ground, land[0], land[2], land[1] + STEP_UP, STEP_UP, &land[1]);
                    if (cur_stage == 3 && i == 1)   // (stage 4's: above the mine's outer wall; it falls on Leon's side)
                        land[0] = inttof32(314);
                    drops_place_treasure(&drops, p, land);
                }
            }
            boss_phase = boss_t = 0;
            obj_t = OBJ_FRAMES;
            elevator_note = 0;
            if (cur_stage == 0)
                enemies_init(&ganados, &ganado, &ganado_anims, &stage_ground, &objs, ganado_spawn, GANADO_COUNT, DEGREES_IN_CIRCLE / 2);
            else
            {
                // The mission's own enemies (stage_foes.c), in waves (foes_step()): Ganados or
                // cultists, each on its spawn point facing its way, standing until it notices
                // Leon; each leaves what its record names when it falls (mission_drop()).
                const StageFoes *sf = &stage_foes[cur_stage];
                foe_n = foe_next = 0;
                for (int i = 0; FOES_ON(cur_stage) && i < sf->count && foe_n < FOES_MAX; i++)
                    if ((sf->foe[i].type == FOE_GANADO || sf->foe[i].type == FOE_CULTIST) && sf->foe[i].pos[1] < WALL_TOP)
                    {
                        // (in wave order, keeping the file's order within a wave)
                        int j = foe_n++;
                        while (j > 0 && foe_list[j - 1]->wave > sf->foe[i].wave)
                        {
                            foe_list[j] = foe_list[j - 1];
                            j--;
                        }
                        foe_list[j] = &sf->foe[i];
                    }
                foe_wave = foe_n ? foe_list[0]->wave : 0;
                // The cultists on top of stage 7's walls (its notes: at the word of the men on top, the
                // others hurl burning rocks; there is no floor up there): they stay at the height
                // their records give and throw. Nothing depends on felling them.
                int throwers = 0, tpts[4][3];
                for (int i = 0; FOES_ON(cur_stage) && i < sf->count && throwers < 4; i++)
                    if (sf->foe[i].type == FOE_CULTIST && sf->foe[i].pos[1] >= WALL_TOP)
                    {
                        for (int k = 0; k < 3; k++)
                            tpts[throwers][k] = sf->foe[i].pos[k];
                        throwers++;
                    }
                foe_slots = foe_n < ENEMY_MAX - throwers ? foe_n : ENEMY_MAX - throwers;
                int pts[ENEMY_MAX][3], n = 0;
                for (; n < foe_slots; n++)
                    for (int k = 0; k < 3; k++)
                        pts[n][k] = foe_list[n]->pos[k];
                int first_thrower = n;
                for (int i = 0; i < throwers; i++, n++)
                    for (int k = 0; k < 3; k++)
                        pts[n][k] = tpts[i][k];
                enemies_init(&ganados, &ganado, &ganado_anims, &stage_ground, &objs, (const int (*)[3])pts, n, DEGREES_IN_CIRCLE / 2);
                ganados.sight = ENEMY_SIGHT;
                ganados.watch = 1;
                for (int i = first_thrower; i < n; i++)
                {
                    enemies_make_thrower(&ganados, i);
                    enemies_perch(&ganados, i, pts[i][1]);
                    enemies_activate(&ganados, i, NULL);   // (on the walls from the start: not in the queue)
                }
                // In the castle they are cultists.
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
                }
                else
                    jya_unload();
            }
            if (cur_stage == 0)
                for (next_ganado = 0; next_ganado < GANADO_ACTIVE && next_ganado < GANADO_FROM_DOOR; next_ganado++)
                    enemies_activate(&ganados, next_ganado, NULL);
            else
                {
                // (where Leon will start: the mission's start point)
                const StageFoes *st = &stage_foes[cur_stage];
                int at[3] = { inttof32(st->start[0]), inttof32(st->start[1]), inttof32(st->start[2]) };
                foes_step(at);   // (the first wave)
#ifdef TEST_FOE_DUMP   // test builds: where the first ones stand, then stop
                consoleDemoInit();
                printf("start %d %d %d slots %d n %d\n", f32toint(at[0]), f32toint(at[1]), f32toint(at[2]), foe_slots, foe_n);
                printf("tries %d moves %d cells %d y0 %d\n", foe_tries, foe_moves, foe_cells, foe_y0);
                for (int i = 0; i < ganados.count && i < 14; i++)
                    printf("%2d a%d %4d %4d %4d s%d\n", i, ganados.e[i].active, f32toint(ganados.e[i].pos[0]), f32toint(ganados.e[i].feet), f32toint(ganados.e[i].pos[2]), ganados.e[i].spotted);
                while (1)
                    swiWaitForVBlank();
#endif
            }
#if !defined(AUTOPILOT) || defined(STAGE_START)
            // The start: in the village just inside the south gate, up the fenced path; on the
            // other stages, where the mission file starts him (its record of type 0), facing
            // its way.
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
            // On the floor at the point's height (not a roof above it).
            int ref = inttof32(10000);
            if (cur_stage != 0)
                ref = inttof32(stage_foes[cur_stage].start[1] + 4);
#if defined(LEON_START_Y)
            ref = inttof32(LEON_START_Y);
#endif
            if (!ground_height(&stage_ground, leon_pos[0], leon_pos[2], ref, 0, &leon_feet))
                ground_height(&stage_ground, leon_pos[0], leon_pos[2], inttof32(10000), 0, &leon_feet);   // none that low
            leon_pos[1] = leon_feet - leon.bbox_min[1];
#endif
            // El Gigante: in his stages (one in stage 4, two in stage 11), on the mission file's
            // points for him. (Test builds with GIANT_AHEAD_TEST: ahead of Leon, the second one
            // further off to one side.)
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
            // The Garradors: one in stage 8, two in stage 9 ("A Dupla de Guardiões"), on the
            // mission file's points for them, listening. (Test builds with GAL_AHEAD: one,
            // ahead of Leon.)
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
#ifdef TEST_GAL_BACK   // test builds: his back to Leon (the parasite in the line of fire)
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
            // The Verdugo: in his stage, ahead of Leon; the tanks on rings around Leon, where
            // there is floor at his height and no wall.
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
                const StageFoe *f = stage_boss(cur_stage, FOE_VERDUGO, 0);   // (his point in the mission file)
                verdugo_place(&ver, &stage_ground, inttof32(f->pos[0]), inttof32(f->pos[2]), inttof32(f->pos[1] + 6),
                              f->dir * (DEGREES_IN_CIRCLE / 256));
#endif
                ver.on = 1;
                objects_clear(&objs);
                objs.cold = 1;
                if (gimmick_model(&tank_model, &tank_loaded, 1, tank_bin, tank_bin_size))
                    objs.barrel = &tank_model;   // (the game's liquid nitrogen cylinder, fix_all #6)
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
            // The mission's objects: crates (kind 04) holding the item they name, explosive
            // barrels (kind 03 on the floor; stage 7's up on the walls are the catapults),
            // doors (01) and bells (09).
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
                    if (t->kind == 4)
                        objects_add_crate(&objs, x, z, ref, t->drop);
                    else if (t->kind == 2)   // a ladder, up to the floor behind it
                        objects_add_ladder(&objs, x, z, ref, t->dir * (DEGREES_IN_CIRCLE / 256));
                    else if (t->kind == 1 && doors)   // a door, on its hinge
                        objects_add_door(&objs, x, z, ref, t->dir * (DEGREES_IN_CIRCLE / 256));
                    else if (t->kind == 5 && hangs)   // a canister on chains
                        objects_add_hang(&objs, x, inttof32(t->pos[1]), z, t->dir * (DEGREES_IN_CIRCLE / 256));
                    else if (t->kind == 9 && bells)   // a bell, hanging from its point
                        objects_add_bell(&objs, x, inttof32(t->pos[1]), z);
                    else if (t->kind == 3 && barrel_loaded && t->pos[1] > 20)   // (up on stage 7's walls)
                        objects_add_barrel_at(&objs, x, inttof32(t->pos[1]), z);
                    else if (t->kind == 3 && barrel_loaded)
                        objects_add_barrel(&objs, x, z, ref);
                }
                // Stage 5's ladder is part of the chapel's model, not a mission object; the stage
                // data marks its foot and top (stg_data section 4, points b3, b4).
                if (cur_stage == 4 && objects_add_ladder(&objs, inttof32(14), inttof32(44), inttof32(6), 0))
                    objs.o[objs.count - 1].drawn = 1;
                if (!barrels && cur_stage != VER_STAGE)
                    barrel_unload();
            }
#ifdef TEST_HIGH_CRATE   // test builds: a crate up in the air, 9 in front of Leon and 13 up
            if (objects_add_barrel_at(&objs, leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(9)), leon_feet + inttof32(13),
                                      leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(9))))
            {
                objs.o[objs.count - 1].kind = OBJ_CRATE;
                objs.o[objs.count - 1].drop = -1;
            }
#endif
            // Ashley: in her stages, just behind Leon; the Ganados go for her too.
            ash_lost = 0;
            ash.on = 0;
            missiles_init(&missiles, &spark, cur_stage >= CASTLE_FROM);
            if (cur_stage == ASH_STAGE_A || cur_stage == ASH_STAGE_B || cur_stage == ASH_STAGE_C || cur_stage == ASH_STAGE_D)
            {
                if (ashley_load(&ash))
                {
                    consoleDemoInit();
                    printf("Ashley: load failed\n");
                    while (1)
                        swiWaitForVBlank();
                }
#ifdef TEST_ASH_VIEW   // test builds: in front of him, facing him
                ashley_place(&ash, &stage_ground, leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(9)),
                             leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(9)), leon_feet + inttof32(4), leon_yaw + DEGREES_IN_CIRCLE / 2);
#else
                ashley_place(&ash, &stage_ground, leon_pos[0] - mulf32(sinLerp(leon_yaw), inttof32(5)) + mulf32(cosLerp(leon_yaw), inttof32(7)),
                             leon_pos[2] - mulf32(cosLerp(leon_yaw), inttof32(5)) - mulf32(sinLerp(leon_yaw), inttof32(7)), leon_feet + inttof32(4), leon_yaw);
#endif
#ifndef TEST_ASH_VIEW
                // Where the mission has her, if he isn't there yet: she stays until he reaches her.
                const StageFoes *as = &stage_foes[cur_stage];
#ifdef TEST_ASH_WITH   // test builds: she is with him from the start
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
#ifdef TEST_ASH_HP   // test builds: she starts hurt
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
            follow_target[0] = leon_pos[0] + mulf32(s, CAM_AHEAD) - mulf32(c, CAM_RIGHT);
            follow_target[1] = leon_feet + CAM_AT_HEIGHT;
            follow_target[2] = leon_pos[2] + mulf32(c, CAM_AHEAD) + mulf32(s, CAM_RIGHT);
            kneel_seen = gal_seen = giant_paid = 0;
            ash_waiting = ash.on && ash.wait;
            // The mission's opening scene (stage 12's is empty: its Verdugo scene comes at once).
            if (cut_opening && cut_start(cur_stage, cur_stage == VER_STAGE ? CUT_VERDUGO : CUT_START, MODE_PLAY, CUT_AFTER_NONE, 1))
                mode = MODE_CUTIN;
            cut_opening = 0;
        }

        // Reactions to the Ganados (started below, after their update): they play out with
        // the controls locked. Held: mash A/B/X/Y to break free.
        if (react != REACT_NONE)
        {
            react_frame += ANIM_STEP;
            int len = inttof32(leon_anims.frames[react_clip[react]]);
            if (react == REACT_BREAK_FREE && react_frame >= KICK_FRAME && react_frame - ANIM_STEP < KICK_FRAME)
            {
                sfx_play(SFX_BREAK_FREE, 120, 64);   // the kick lands
                if (freed_from >= 0)
                {
                    enemies_damage(&ganados, freed_from, FREE_DAMAGE);
                    enemies_daze(&ganados, freed_from);   // (kickable now, as after a head shot)
                    if (ganados.e[freed_from].state >= ENEMY_DYING)
                        maybe_drop(&ganados.e[freed_from], leon_hp, DROP_PERCENT);
                    freed_from = -1;
                }
            }
            if (react == REACT_HELD)
            {
                held_frames++;
                if (down & (KEY_A | KEY_B | KEY_X | KEY_Y))
                    mash++;
#ifdef TEST_MASH   // test builds: as if A were mashed (every 6 frames)
                if (held_frames % 6 == 0)
                    mash++;
#endif
                if (held_frames >= GRAB_MAX && mash < GRAB_MASH && ganados.e[holder].state == ENEMY_GRAB)
                {
                    // Too slow: he lets go of him in his own time; Leon staggers free (no kick).
                    enemies_release(&ganados, holder);
                    holder = -1;
                    react = REACT_FLINCH;
                    react_frame = 0;
                }
                else if (mash >= GRAB_MASH || ganados.e[holder].state != ENEMY_GRAB)
                {
                    // Free: Leon shoves him off and kicks (CLIP_BREAK_FREE); he reels back, dazed.
                    freed_from = ganados.e[holder].state == ENEMY_GRAB ? holder : -1;
                    enemies_release(&ganados, holder);
                    holder = -1;
                    react = REACT_BREAK_FREE;
                    react_frame = 0;
                }
            }
            else if (react == REACT_DEAD)
            {
                if (react_frame > len - 1)
                    react_frame = len - 1;   // stays down (frames past the end would wrap)
                if (mode == MODE_PLAY && ++dead_timer >= DEAD_TO_MENU)
                    mode = MODE_GAMEOVER;
            }
            else if (react == REACT_KICK)
            {
                // The foot lands: a dazed Ganado (and any beside it) goes flying...
                if (react_frame >= KICK_HIT && react_frame - ANIM_STEP < KICK_HIT && act_obj < 0)
                {
                    if (enemies_kick(&ganados, leon_pos, leon_yaw, KICK_DAMAGE))
                        sfx_play(SFX_BREAK_FREE, 127, 64);
                    act_obj = -2;   // (landed)
                }
                // ...or the gate swings open / the crate breaks.
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
                        crate_drop(&objs.o[act_obj], leon_hp);
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
                    // On his back: turned to face his head, the knife comes down.
                    leon_pos[0] += mulf32(gclimb_f[0], CLIMB_BACK);
                    leon_pos[2] += mulf32(gclimb_f[1], CLIMB_BACK);
                    leon_feet = gclimb_y + CLIMB_UP;
                    leon_pos[1] = leon_feet - leon.bbox_min[1];
                    leon_yaw = giants[gclimb].yaw;
                    react = REACT_GIANT_CUT;
                    react_frame = 0;
                    clip_key = -1;
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
                    // He throws Leon off: down behind him, facing him.
                    gc->held = 0;
                    walk_out(&stage_ground, leon_pos, -gclimb_f[0], -gclimb_f[1], inttof32(4), gclimb_y);
                    {
                        int tx = gc->pos[0] - leon_pos[0], tz = gc->pos[2] - leon_pos[2];   // (landing, he faces him)
                        leon_yaw = (int)(atan2f((float)tx, (float)tz) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
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
                    // Off the ladder: on the walkway (where the climb carried him) or at its foot.
                    const Obj *l = &objs.o[act_obj];
                    leon_pos[0] = l->pos[0] + (react == REACT_CLIMB_UP ? mulf32(l->dir[0], CLIMB_BACK) : 0);
                    leon_pos[2] = l->pos[2] + (react == REACT_CLIMB_UP ? mulf32(l->dir[1], CLIMB_BACK) : 0);
                    leon_feet = react == REACT_CLIMB_UP ? l->top : l->pos[1];
                    ground_height(&stage_ground, leon_pos[0], leon_pos[2], leon_feet, STEP_UP, &leon_feet);
                    leon_pos[1] = leon_feet - leon.bbox_min[1];
                    react = REACT_NONE;
                    clip_key = -1;   // the climb's end pose is where he now stands: no cross-fade
                    if (ash.on)   // she climbs it after him
                    {
                        int foot[3] = { l->pos[0], l->pos[1], l->pos[2] };
                        ashley_use_ladder(&ash, foot, l->dir, l->top, l->yaw, leon_feet > l->pos[1] + inttof32(4));
                    }
                    act_obj = -1;
                }
            }
            else if (react == REACT_DROP)
            {
                // Off the ledge: a little forward, then gravity down to the floor below.
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
                        sfx_play(SFX_LEON_HURT, 70, 64);   // a hard landing
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

        int fwd_l[3] = { sinLerp(leon_yaw), 0, cosLerp(leon_yaw) };   // Leon's facing (model +Z)
        int anim_dir = 1;   // -1 plays the animation backwards (walking backward)

        // Reloading, Leon lowers the gun and can walk (as in the Zeebo version): R still held
        // aims again once the rounds are in.
        int aim_held = ctrl == CTRL_LEON && (held & KEY_R) && free_to_act;
        int aiming = aim_held && reload_frame < 0;
        // The knife (hold L, as the Zeebo version's knife button): Leon stands ready with it
        // and A slashes (low enough for a crate at his feet). A slash under way finishes.
        int knifing = (ctrl == CTRL_LEON && (held & KEY_L) && !aiming && free_to_act
                       && gun_state != GUN_DRAWING && gun_state != GUN_HOLSTERING) || slash_frame >= 0;
        knife_w += knifing ? AIM_RAISE : -AIM_RAISE;
        if (knife_w > inttof32(1)) knife_w = inttof32(1);
        if (knife_w < 0) knife_w = 0;
        if (knifing && free_to_act && (down & KEY_A) && slash_frame < 0)
        {
            slash_frame = 0;
            slash_high = aim_tilt > KNIFE_HIGH_TILT;
            sfx_play(SFX_KNIFE, 120, 64);
        }
        if (slash_frame >= 0)
        {
            int len = inttof32(leon_knife.frames[slash_high ? KNIFE_HIGH : KNIFE_SLASH]), hit = len / 2;
            slash_frame += ANIM_STEP;
            if (slash_frame >= hit && slash_frame - ANIM_STEP < hit)
            {
                // The blade comes through along the view, up or down as he looks (as the gun's
                // aim): a crate, barrel, bell or medallion there takes it (a treasure up high is
                // knocked down); else a crate at his feet breaks, or the nearest Ganado in reach
                // takes a cut.
                int which, pitch = mulf32(aim_tilt, KNIFE_PITCH), cp = cosLerp(pitch);
                int kdir[3] = { mulf32(sinLerp(leon_yaw), cp), sinLerp(pitch), mulf32(cosLerp(leon_yaw), cp) };
                int kfrom[3] = { leon_pos[0], leon_feet + KNIFE_CHEST, leon_pos[2] };
                int kt = KNIFE_REACH, kend[3];
                if (ground_raycast(&stage_ground, kfrom, kdir, KNIFE_REACH, kend, RAY_SHOT))
                {
                    s64 ex = kend[0] - kfrom[0], ey = kend[1] - kfrom[1], ez = kend[2] - kfrom[2];
                    kt = (int)sqrt64((u64)(ex * ex + ey * ey + ez * ez)) + inttof32(1);   // (not through a wall)
                }
                int kobj = objects_raycast(&objs, kfrom, kdir, &kt);
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
                        crate_drop(o, leon_hp);
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
                    crate_drop(&objs.o[which], leon_hp);
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

        // Draw / holster with X (GUN_DRAW forwards, or backwards to put the gun away).
        // L+X: the next weapon (the status screen's Equip does the same).
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

        // Fire (A, fully aimed) and reload (Y with the gun out, or firing on empty).
        int gun_out = gun_state == GUN_READY || aim_held;
        if (ctrl == CTRL_LEON && reload_frame < 0 && !drawing && free_to_act)
        {
            if ((down & KEY_Y) && gun_out && ammo < MAG_SIZE && spare > 0)
            {
                reload_frame = 0;
                reload_ch = sfx_play(SFX_RELOAD, 110, 64);
            }
            else if (((down & KEY_A) || (wep == WEP_TMP && (held & KEY_A))) && aiming && aim_w == inttof32(1)
                     && (fire_frame < 0 || fire_frame >= wep_refire(wep)))
            {
                if (ammo == 0)
                {
                    sfx_play(SFX_EMPTY, 110, 64);   // and reload, if there is anything to load
                    if (spare > 0)
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
                        garrador_hear(&gals[i], leon_pos, inttof32(4000));   // a shot: they hear it anywhere
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
                        // A rocket, flying to where the sight points; it bursts there (see below).
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
                    if (wep == WEP_SHOTGUN)   // (it hits everyone in its spread)
                        for (int i = 0; i < ganados.count; i++)
                        {
                            const Enemy *e = &ganados.e[i];
                            int dx = e->pos[0] - leon_pos[0], dz = e->pos[2] - leon_pos[2];
                            int d = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
                            if (i == laser_enemy || !e->active || e->state >= ENEMY_DYING || d == 0 || d > SHOTGUN_RANGE
                                || ((s64)sinLerp(leon_yaw) * dx + (s64)cosLerp(leon_yaw) * dz) >> 12 < mulf32(d, SHOTGUN_CONE))
                                continue;
                            enemies_damage(&ganados, i, BODY_DAMAGE);
                            hits++;
                            if (e->state == ENEMY_DYING)
                                maybe_drop(e, leon_hp, DROP_PERCENT);
                        }
                    if (laser_enemy >= 0)
                    {
                        enemies_damage(&ganados, laser_enemy, laser_head ? HEAD_DAMAGE : BODY_DAMAGE);
#ifdef TEST_DAZE   // test builds: every hit dazes
                        laser_head = 1;
#endif
                        if (laser_head)
                            enemies_daze(&ganados, laser_enemy);   // staggers: open to a kick
                        hits++;
                        headshots += laser_head;
                        const Enemy *e = &ganados.e[laser_enemy];
                        sfx_play_at(e->state == ENEMY_DYING ? SFX_GANADO_DOWN : SFX_GANADO_HIT, 120, e->pos, leon_pos, leon_yaw);
                        if (e->state == ENEMY_DYING)
                            maybe_drop(e, leon_hp, DROP_PERCENT);
                    }
                    else if (laser_ver)
                    {
                        int was_frozen = ver.state == VER_FROZEN;
                        verdugo_shot(&ver, BODY_DAMAGE);
                        hits++;
                        headshots += was_frozen;
                        impact_blood = !was_frozen;
                        sfx_play_at(ver.state == VER_DEAD ? SFX_GANADO_DOWN : SFX_GANADO_HIT, 110, ver.pos, leon_pos, leon_yaw);
                    }
                    else if (laser_gal)
                    {
                        Garrador *gh = &gals[laser_gal - 1];
                        garrador_shot(gh, BODY_DAMAGE, laser_gal_back);
                        hits++;
                        headshots += laser_gal_back;
                        impact_blood = 1;
                        sfx_play_at(gh->state == GAL_DEAD ? SFX_GANADO_DOWN : SFX_GANADO_HIT, 110, gh->pos, leon_pos, leon_yaw);
                    }
                    else if (laser_giant)
                    {
                        Giant *gh = &giants[laser_giant - 1];
                        giant_shot(gh, BODY_DAMAGE);
                        hits++;
                        impact_blood = 1;
                        sfx_play_at(SFX_GANADO_HIT, 100, gh->pos, leon_pos, leon_yaw);
                    }
                    else if (laser_ash)
                    {
                        ashley_hit(&ash, ASH_SHOT_DAMAGE);   // shot her
                        sfx_play_at(SFX_LEON_HURT2, 110, ash.pos, leon_pos, leon_yaw);
                    }
                    else if (laser_drop >= 0 && laser_obj < 0)
                    {
                        drops_knock(&drops, laser_drop);   // a treasure up high: down it comes
                        impact_blood = 0;
                        sfx_play(SFX_ITEM_GET, 90, 64);
                    }
                    else if (laser_obj >= 0)
                    {
                        // Crates break (maybe an item), barrels are lit, medallions shatter.
                        Obj *o = &objs.o[laser_obj];
                        impact_blood = 0;
                        if (objects_hit(&objs, laser_obj))
                        {
                            hits++;
                            if (o->kind == OBJ_CRATE)
                            {
                                sfx_play_at(SFX_BREAK, 120, o->pos, leon_pos, leon_yaw);
                                crate_drop(o, leon_hp);
                            }
                            else if (o->kind == OBJ_BELL)
                            {
                                // A bell: a loud ring the Garradors hear wherever they are
                                // (stage 8's notes: "if you strike it, it makes a loud noise").
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
            reload_frame = -1;   // stopped aiming/put the gun away/hurt: the reload is abandoned
            sfx_stop(reload_ch);
        }
        if (reload_frame < 0)
            reload_ch = -1;

        if (ctrl == CTRL_LEON && ((drawing && !aiming) || !free_to_act))
        {
            // Drawing/holstering or reacting to an attack: stand still.
        }
        else if (ctrl == CTRL_LEON && (aiming || knifing))
        {
            // Aiming: stand still, turn slowly, tilt the aim with up/down.
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
                aim_tilt = 0;   // next aim starts level

            // Tank controls: turn with left/right, move along the facing direction.
            if (held & KEY_LEFT)  leon_yaw += TURN_SPEED;
            if (held & KEY_RIGHT) leon_yaw -= TURN_SPEED;
            fwd_l[0] = sinLerp(leon_yaw);
            fwd_l[2] = cosLerp(leon_yaw);

            int speed = 0, want = ANIM_IDLE;
            if (held & KEY_UP)
            {
                want = (held & KEY_B) ? ANIM_RUN : ANIM_WALK;
                speed = want == ANIM_RUN ? RUN_SPEED : WALK_SPEED;
            }
            else if (held & KEY_DOWN)
            {
                want = ANIM_WALK;
                speed = -BACK_SPEED;
                anim_dir = -1;
            }
            if (want != leon_anim)
            {
                leon_anim = want;
                anim_frame = 0;
            }

            // A (not aiming): the action at hand: kick a dazed Ganado, kick a crate, climb a
            // ladder.
            // At the kneeling giant's back: A cuts the parasite (with the knife's slash).
            if (jump_now && mode == MODE_PLAY)
            {
                // Across: on the far side, facing on; Ashley with him, just behind.
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
                // Its scene first, if there is one (the landing waits until it is over).
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
                // To his back, facing away from him: the climb carries Leon back onto it.
                int fx = sinLerp(gc->yaw), fz = cosLerp(gc->yaw);
                gclimb = gc - giants;
                gclimb_f[0] = fx;
                gclimb_f[1] = fz;
                leon_pos[0] = gc->pos[0] - mulf32(fx, GIANT_BACK);
                leon_pos[2] = gc->pos[2] - mulf32(fz, GIANT_BACK);
                walk_out(&stage_ground, leon_pos, -fx, -fz, CLIMB_BACK, gc->feet);   // (not through a wall)
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
            int which = -1, act = (down & KEY_A) && dazed < 0 ? objects_action(&objs, leon_pos, leon_feet, leon_yaw, &which) : ACT_NONE;
            if (dazed >= 0)
            {
                // Turn to face it, then the kick (it lands at KICK_HIT).
                int dx = ganados.e[dazed].pos[0] - leon_pos[0], dz = ganados.e[dazed].pos[2] - leon_pos[2];
                leon_yaw = (int)(atan2f((float)dx, (float)dz) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
                react = REACT_KICK;
                react_frame = 0;
                act_obj = -1;
                speed = 0;
            }
            else if (act == ACT_KICK_CRATE)   // (the gates stay locked)
            {
                react = REACT_KICK;
                react_frame = 0;
                act_obj = which;
                speed = 0;
            }
            else if (act == ACT_OPEN_DOOR)   // (a mission's door: he pushes it open)
                objects_open_door(&objs, which, leon_pos);
            else if (act == ACT_LADDER_UP || act == ACT_LADDER_DOWN)
            {
                // On the ladder's line, facing away from the walkway: the climb (#3) carries
                // him back onto it; climbing down plays it backwards from the walkway.
                const Obj *l = &objs.o[which];
                react = act == ACT_LADDER_UP ? REACT_CLIMB_UP : REACT_CLIMB_DOWN;
                react_frame = 0;
                act_obj = which;
                leon_pos[0] = l->pos[0];
                leon_pos[2] = l->pos[2];
                leon_feet = l->pos[1];
                leon_pos[1] = leon_feet - leon.bbox_min[1];
                leon_yaw = l->yaw;
                gun_state = GUN_HOLSTERED;   // both hands on the rungs
                clip_key = -1;               // straight into the climb (no cross-fade from elsewhere)
                speed = 0;
            }

            if (speed)
            {
                leon_pos[0] += mulf32(fwd_l[0], speed);
                leon_pos[2] += mulf32(fwd_l[2], speed);
                // Ganados, shut gates, crates and barrels are solid; then walls get the last word.
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
                // Walls that rise above a step and reach his body push him back out.
                ground_collide(&stage_ground, &leon_pos[0], &leon_pos[2],
                                           leon_feet + STEP_UP, leon_feet + LEON_HEIGHT, LEON_RADIUS);
                // Follow the floor; keep the last height where there is none (holes, map edge).
                // A floor far below: he has walked off a ledge, and drops.
                int floor = leon_feet;
                ground_height(&stage_ground, leon_pos[0], leon_pos[2], leon_feet, STEP_UP, &floor);
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
                // Round the circle, facing the direction of travel (counter-clockwise from above).
                int speed = leon_anim == ANIM_WALK ? WALK_SPEED : RUN_SPEED;
                leon_angle += (int)(((s64)speed * DEGREES_IN_CIRCLE) / (2 * 3.14159265 * circle_radius));
                leon_pos[0] = circle_x + mulf32(sinLerp(leon_angle), circle_radius);
                leon_pos[2] = circle_z + mulf32(cosLerp(leon_angle), circle_radius);
                leon_yaw = leon_angle + DEGREES_IN_CIRCLE / 4;
                ground_height(&stage_ground, leon_pos[0], leon_pos[2], leon_feet, STEP_UP, &leon_feet);
                leon_pos[1] = leon_feet - leon.bbox_min[1];
            }
        }

        // Camera
        int look_eye[3], look_at[3];
        if (ctrl == CTRL_LEON)
        {
            // Over Leon's right shoulder, looking past him (see CAM_* above); closer in,
            // and following the tilt, as the aim is raised.
            int fwd_c[3] = { sinLerp(leon_yaw), 0, cosLerp(leon_yaw) };
            int right[3] = { -fwd_c[2], 0, fwd_c[0] };   // the camera's right
            // (The knife brings the view in the same way as the gun.)
            int cam_w = aim_w > knife_w ? aim_w : knife_w;
            int back = CAM_BACK + mulf32(CAM_AIM_BACK - CAM_BACK, cam_w);
            int side = CAM_RIGHT + mulf32(CAM_AIM_RIGHT - CAM_RIGHT, cam_w);
            int want_eye[3], want_at[3];
            for (int k = 0; k < 3; k += 2)
            {
                want_eye[k] = leon_pos[k] - mulf32(fwd_c[k], back) + mulf32(right[k], side);
                want_at[k] = leon_pos[k] + mulf32(fwd_c[k], CAM_AHEAD) + mulf32(right[k], side);
            }
            want_eye[1] = leon_feet + CAM_HEIGHT + mulf32(CAM_AIM_HEIGHT - CAM_HEIGHT, cam_w);
            want_at[1] = leon_feet + CAM_AT_HEIGHT + mulf32(mulf32(CAM_AIM_TILT, aim_tilt), cam_w);
            if (knife_w > aim_w && aim_tilt > 0)
            {
                // With the knife, looking up: the view comes down behind him and tilts less, so
                // he stays in it as he leans back (it looked over his head, him below the frame).
                want_eye[1] -= mulf32(mulf32(KNIFE_CAM_DROP, aim_tilt), knife_w);
                want_at[1] -= mulf32(mulf32(CAM_AIM_TILT / 2, aim_tilt), knife_w);
            }
            // A shot kicks the view up a little; it settles back with the follow smoothing.
            want_at[1] += cam_kick;
            cam_kick = cam_kick * 3 / 5;
#ifdef CAM_TEST_SIDE
            // Test builds: look at Leon from his right side instead.
            for (int k = 0; k < 3; k += 2)
            {
                want_eye[k] = leon_pos[k] + right[k] * 16 + fwd_l[k] * 4;
                want_at[k] = leon_pos[k] + fwd_l[k] * 4;
            }
            want_eye[1] = leon_feet + inttof32(12);
            want_at[1] = leon_feet + inttof32(11);
#endif
#ifdef CAM_TEST_FRONT
            // Test builds: close up, from in front of him (a little to his left).
            for (int k = 0; k < 3; k += 2)
            {
                want_eye[k] = leon_pos[k] + fwd_l[k] * 11 - right[k] * 4;
                want_at[k] = leon_pos[k];
            }
            want_eye[1] = leon_feet + inttof32(13);
            want_at[1] = leon_feet + inttof32(10);
#endif
            // On a ladder: he faces away from the wall, his body rising (or coming down) with the
            // climb while his position stays at the foot. The view follows the body from the open
            // side, a little to his right, looking at him and the wall.
            int on_giant = react == REACT_GIANT_UP;
            int climbing = ((react == REACT_CLIMB_UP || react == REACT_CLIMB_DOWN) && act_obj >= 0) || on_giant;
            if (climbing)
            {
                // The ladder's foot, the way in, its top; or the giant's: where Leon starts up him.
                int foot[3], in[2], top;
                if (on_giant)
                {
                    int back = react == REACT_GIANT_CUT ? CLIMB_BACK : 0;   // (at the top his place moved in)
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
                int out[2] = { -in[0], -in[1] };              // away from the wall (the giant)
                int sx = -out[1], sz = out[0];                 // his right, facing out
                for (int k = 0; k < 2; k++)
                {
                    int base = (k ? foot[2] : foot[0]) + mulf32(k ? in[1] : in[0], mulf32(CLIMB_BACK, t) / 2);
                    want_eye[k * 2] = base + mulf32(out[k], CAM_CLIMB_OUT) + mulf32(k ? sz : sx, CAM_CLIMB_SIDE);
                    want_at[k * 2] = base;
                }
                want_eye[1] = body_y + CAM_CLIMB_HEIGHT;
                want_at[1] = body_y + CAM_CLIMB_AT;
                if (react_frame <= ANIM_STEP)   // (a cut to it: gliding there would pass through him)
                    for (int k = 0; k < 3; k++)
                    {
                        follow_eye[k] = want_eye[k];
                        follow_target[k] = want_at[k];
                    }
            }
            approach(follow_eye, want_eye);
            approach(follow_target, want_at);
            // Walls: the camera stays on Leon's side of them. Seen from behind a wall (back
            // to it, turning), the wall faces away and isn't drawn, as if it had gone. A ray
            // from his head out to the camera; a wall in between brings the camera in, just
            // short of it.
            {
                int head[3] = { leon_pos[0], follow_eye[1], leon_pos[2] };
                int dv[3] = { follow_eye[0] - head[0], follow_eye[1] - head[1], follow_eye[2] - head[2] };
                int len = (int)sqrt64((u64)((s64)dv[0] * dv[0] + (s64)dv[1] * dv[1] + (s64)dv[2] * dv[2]));
                if (len > CAM_WALL_GAP && !climbing)   // (climbing, it is on the open side)
                {
                    int dir[3] = { divf32(dv[0], len), divf32(dv[1], len), divf32(dv[2], len) }, hit[3];
                    if (ground_raycast(&stage_ground, head, dir, len + CAM_WALL_GAP, hit, RAY_CAMERA))
                    {
                        int hx = hit[0] - head[0], hz = hit[2] - head[2];
                        int hd = (int)sqrt64((u64)((s64)hx * hx + (s64)hz * hz)) - CAM_WALL_GAP;
                        if (hd < inttof32(2))
                            hd = inttof32(2);
                        if (hd < len)
                            for (int k = 0; k < 3; k += 2)
                                follow_eye[k] = head[k] + mulf32(dir[k], hd);
                    }
                }
                // Pressed in close by a wall, the camera would be inside him: it rises to look
                // down over his shoulder instead, and closer still he fades out of the way.
                int hx = follow_eye[0] - leon_pos[0], hz = follow_eye[2] - leon_pos[2];
                int near = (int)sqrt64((u64)((s64)hx * hx + (s64)hz * hz));
                leon_fade = 31;
                if (near < CAM_NEAR && !climbing)
                {
                    int lift = leon_feet + CAM_HEIGHT + mulf32(CAM_NEAR - near, CAM_NEAR_LIFT);
                    if (follow_eye[1] < lift)
                        follow_eye[1] = lift;
                    if (near < CAM_FADE)
                        leon_fade = 8 + 23 * near / CAM_FADE;
                }
            }
            // Keep the camera above the floor under it (hills behind Leon; not an upper floor over
            // his head, when he has just climbed down from it).
            int cam_floor;
            if (ground_height(&stage_ground, follow_eye[0], follow_eye[2], (follow_eye[1] < want_eye[1] + inttof32(6) ? follow_eye[1] : want_eye[1] + inttof32(6)), 0, &cam_floor)
                && follow_eye[1] < cam_floor + inttof32(3))
                follow_eye[1] = cam_floor + inttof32(3);
            for (int k = 0; k < 3; k++)
            {
                look_eye[k] = follow_eye[k];
                look_at[k] = follow_target[k];
            }
            if (scoping)   // (the view from his eye, the scope's narrow one: see set_fov())
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
#ifdef TEST_ASH_FIRST   // test builds: Ashley before everything (polygon budget check)
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
            // The DS keeps 2048 polygons a frame (those facing the camera and on screen) and drops
            // what comes after: the stage, sent first, gets what the last frame left over, nearest
            // first (see the end of the frame), so the characters and the laser sight still fit.
            if (stage_budget < 0 || stage_budget > (int)stage.triangles)
                stage_budget = stage.triangles;
            // Buildings Leon goes in and out of (stage 6's cabin): from outside, its inside is not
            // drawn; from inside, nothing beyond its yard (the hardware's 2048 polygons went on the
            // whole village, and the walls and the villagers in the cabin were the ones dropped).
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
                // Galleries: from up on stage 8's, the hall below (seen through its windows) is
                // not drawn, nor the ones down there; its windows show the dark like a curtain.
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

        // While the stage streams to the geometry engine by DMA, the CPU poses Leon, casts
        // the laser and moves the Ganados; then it waits for the transfer before drawing more.
        if (leon_anim != ANIM_OFF)
        {
            anim_frame += (leon_anim == ANIM_RUN ? mulf32(ANIM_STEP, RUN_ANIM) : ANIM_STEP) * anim_dir;

            // Base animation: the draw while drawing/holstering, the handgun set (ready
            // stance, walk, run) while the gun is out, otherwise the unarmed set.
            const AnimSet *set = &leon_anims;
            int clip = leon_anim, clip_frame = anim_frame;
            if (react != REACT_NONE)
            {
                clip = react_clip[react];
                clip_frame = react_frame;
                int len = inttof32(leon_anims.frames[clip]) - 1;
                if (react == REACT_CLIMB_DOWN)
                    clip_frame = len - react_frame;   // the climb, backwards
                if (clip_frame > len)
                    clip_frame = len;                 // hold the last frame (no wrapping)
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
                clip = slash_frame >= 0 ? (slash_high ? KNIFE_HIGH : KNIFE_SLASH) : KNIFE_READY;
                clip_frame = slash_frame >= 0 ? slash_frame : 0;   // (the stance is a single pose)
#ifdef TEST_KNIFE_VIEW   // test builds: each clip of the set in turn (2 s each), looping
                clip = (int)((vblank_count / 120) % 7);
                clip_frame = (int)(vblank_count % 120) * ANIM_STEP / 2;
                if (leon_knife.frames[clip] > 0) clip_frame %= inttof32(leon_knife.frames[clip]); else clip_frame = 0;
#endif
            }
            anim_sample(set, clip, clip_frame, &leon, pose_base);

            // Cross-fade from the last pose when the animation changes.
            int key = (set == &leon_gun ? 100 : set == &leon_knife ? 200 : 0) + clip;
            if (key != clip_key)
            {
                if (clip_key >= 0)
                {
                    memcpy(pose_from, pose_prev, sizeof(pose_from));
                    xfade_w = inttof32(1);
                }
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
                // Aim pose, blended over the current animation by aim_w.
                // Two-handed aim: the level pose, towards the up or down one by the tilt.
                anim_sample(&leon_gun, AIM_FORWARD, 0, &leon, pose_aim);
                anim_sample(&leon_gun, aim_tilt >= 0 ? AIM_UP : AIM_DOWN, 0, &leon, pose_tilt);
                anim_blend(pose_aim, pose_tilt, leon.node_count, aim_tilt >= 0 ? aim_tilt : -aim_tilt);
                anim_blend(pose_base, pose_aim, leon.node_count, aim_w);
            }
            if (fire_frame >= 0 && aim_w)
            {
                // The shot: the fire animation's motion (the shotgun's kick and pump) from its first
                // frame, each joint's turn applied after the aim's as a rotation (added as Euler
                // angles, or the up and down clips mixed whole, the pump swung the gun over his
                // shoulder). The clip aimed the way he aims: up or down.
                int clip_fire = aim_tilt >= 0 ? FIRE_UP : FIRE_DOWN;
                anim_sample(&leon_gun, clip_fire, fire_frame, &leon, pose_fx);
                anim_sample(&leon_gun, clip_fire, 0, &leon, pose_fx0);
                anim_add_delta_rot(pose_base, pose_fx, pose_fx0, leon.node_count, mulf32(RECOIL, aim_w));
            }
            if (reload_frame >= 0)
            {
                // Reload pose over everything else, faded in and out at its ends.
                int edge = reload_frame < inttof32(RELOAD_FRAMES) - reload_frame
                         ? reload_frame : inttof32(RELOAD_FRAMES) - reload_frame;
                int w = divf32(edge, RELOAD_FADE);
                anim_sample(&leon_gun, GUN_RELOAD, reload_frame, &leon, pose_fx);
                // (the upper body only, from the spine up: his legs keep walking)
                anim_blend(pose_base + UPPER_BODY, pose_fx + UPPER_BODY, leon.node_count - UPPER_BODY, w > inttof32(1) ? inttof32(1) : w);
            }
            anim_compose(&leon, pose_base, leon_pose);
            // With the knife his upper body follows the view up or down: everything from the waist
            // up (node UPPER_BODY and all under it) turns about his own left-right axis through
            // the waist, leaning back to look up, forward to look down. (Turned joint by joint
            // inside the knife stance, whose torso is twisted, it had bent sideways.)
            if (knifing && react == REACT_NONE && aim_tilt)
            {
                // (back at most KNIFE_LEAN_UP: more and his chest came into the camera behind him;
                // forward KNIFE_LEAN_DOWN: the stance is crouched already)
                int a = -mulf32(aim_tilt, aim_tilt > 0 ? KNIFE_LEAN_UP : KNIFE_LEAN_DOWN), c, s;
                c = cosLerp(a);
                s = sinLerp(a);
                const int *w = leon_pose[UPPER_BODY];
                int py = w[10], pz = w[11];
                int rot[12] = { inttof32(1), 0, 0,  0, c, -s,  0, s, c,
                                0, py - mulf32(c, py) + mulf32(s, pz), pz - mulf32(s, py) - mulf32(c, pz) };
                for (int n = UPPER_BODY; n < leon.node_count; n++)
                    mtx43_mul(leon_pose[n], rot, leon_pose[n]);
            }

            // Muzzle and barrel direction in the world (the gun hangs off the hand node;
            // its barrel points along -X), and where the laser/shot would hit.
            const int *hand = leon_pose[HAND_NODE];
            int m_model[3], d_model[3] = { -hand[0], -hand[3], -hand[6] }, d_world[3], zero[3] = { 0, 0, 0 };
            for (int k = 0; k < 3; k++)
                m_model[k] = mulf32(hand[k * 3], weapons[wep].muzzle[0]) + mulf32(hand[k * 3 + 1], weapons[wep].muzzle[1])
                           + mulf32(hand[k * 3 + 2], weapons[wep].muzzle[2]) + hand[9 + k];
            leon_to_world(leon_pos, leon_yaw, m_model, muzzle);
            leon_to_world(zero, leon_yaw, d_model, d_world);
            // Through the scope the shot goes where it looks: from his eye, along the view.
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
                // A crate / barrel / medallion, or a Ganado, nearer than the wall/floor takes
                // the shot instead (whichever is nearest).
                int t_best = SHOT_RANGE;
                if (laser_hit)
                {
                    s64 ex = laser_end[0] - muzzle[0], ey = laser_end[1] - muzzle[1], ez = laser_end[2] - muzzle[2];
                    t_best = (int)sqrt64((u64)(ex * ex + ey * ey + ez * ez));
                }
                laser_obj = objects_raycast(&objs, muzzle, d_world, &t_best);
                // (A treasure tucked behind a beam or eave counts, up to this far behind it.)
                int t_drop = t_best + TREASURE_REACH;
                laser_drop = drops_raycast(&drops, muzzle, d_world, &t_drop);
                if (laser_drop >= 0)
                {
                    if (t_drop < t_best)
                        t_best = t_drop;
                    laser_obj = -1;
                }
                laser_enemy = enemies_raycast(&ganados, muzzle, d_world, &t_best, &laser_head);
                laser_ash = ash.on && ashley_raycast(&ash, muzzle, d_world, &t_best);   // she's in the way
                laser_giant = 0;   // (1 + which one)
                for (int i = 0; i < giant_count; i++)
                    if (giant_raycast(&giants[i], muzzle, d_world, &t_best))
                        laser_giant = i + 1;
                laser_gal = 0;   // (1 + which one)
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

#ifdef TEST_KILL_AT   // test builds: every Ganado goes down at this frame, each leaving an item
        static u32 test_frame;
        if (++test_frame == TEST_KILL_AT)
            for (int i = 0; i < ganados.count; i++)
            {
                enemies_damage(&ganados, i, 99);
                maybe_drop(&ganados.e[i], leon_hp, 100);
            }
#endif
#ifdef TEST_KILL_EVERY   // test builds: every this many frames, the Ganados in the stage go down
        static u32 kill_frame;
        if (++kill_frame % TEST_KILL_EVERY == 0)
            for (int i = 0; i < ganados.count; i++)
                if (ganados.e[i].active && ganados.e[i].state < ENEMY_DYING && !ganados.e[i].thrower)
                {
                    enemies_damage(&ganados, i, 99);
                    maybe_drop(&ganados.e[i], leon_hp, 100);
                }
        if (kill_frame % TEST_KILL_EVERY == 0)   // (and the bosses)
        {
            for (int i = 0; i < giant_count; i++)
                giant_blast(&giants[i]);
            for (int i = 0; i < gal_count; i++)
                garrador_shot(&gals[i], 99, 1);
            if (ver.on)
                verdugo_shot(&ver, 99);
        }
#endif
#ifdef TEST_GOD   // test builds: the Ganados never hurt Leon
        enemies_update(&ganados, leon_pos, 0, &ev);
#else
        PROF(1);
        enemies_update(&ganados, leon_pos, ctrl == CTRL_LEON && free_to_act && safe_timer == 0 && clear_t < 0, &ev);
        PROF(2);
#endif
        // Ganado voices and swings, from where they are.
        if (ev.spotted >= 0)
            sfx_play_at(SFX_GANADO_SPOT, 110, ganados.e[ev.spotted].pos, leon_pos, leon_yaw);
        if (ev.attacking >= 0)
            sfx_play_at(SFX_GANADO_ATTACK, 120, ganados.e[ev.attacking].pos, leon_pos, leon_yaw);
        if (ev.swing >= 0)
            sfx_play_at(SFX_SWING, 100, ganados.e[ev.swing].pos, leon_pos, leon_yaw);
        int damage = 0;
        if (ev.swiped_by >= 0)
        {
            damage = ganados.e[ev.swiped_by].weapon ? WEAPON_DAMAGE : SWIPE_DAMAGE;
            if (ganados.e[ev.swiped_by].boss)
                damage = LEON_HP;   // the chainsaw: no one survives it
            react = REACT_FLINCH;
            react_frame = 0;
        }
        if (ev.grabbed_by >= 0)
        {
            // Face the Ganado, which takes hold right in front of him.
            holder = ev.grabbed_by;
            leon_yaw = ganados.e[holder].yaw + DEGREES_IN_CIRCLE / 2;
            enemies_hold_at(&ganados, holder, leon_pos, GRAB_DIST);
            react = REACT_HELD;
            react_frame = 0;
            mash = held_frames = 0;
        }
        // El Gigante: walks at Leon; his blows hurt a lot.
        for (int gi = 0; gi < giant_count; gi++)
        {
            Giant *gp = &giants[gi];
            for (int j = 0; j < giant_count; j++)   // they don't walk into each other
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
#ifdef TEST_AUTO_CUT   // test builds: cut the parasite as soon as it can be
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
                leon_hp -= gev.hit_leon;
                damage_taken += gev.hit_leon;
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
        // The Garradors: hear Leon running near (or right beside) one, charge, slash.
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
                leon_hp -= gev.hit_leon;
                damage_taken += gev.hit_leon;
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
        // The Verdugo: stalks Leon, strikes with his tail, leaps up and drops down by him.
        if (ver.on)
        {
#ifdef TEST_TANK_AT   // test builds: the tank nearest him bursts at this frame
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
                leon_hp -= vev.hit_leon;
                damage_taken += vev.hit_leon;
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
        // Ashley: follows Leon; a Ganado's swing at her hurts her; if she goes down, it's over.
        if (ash.on)
        {
            // Leon aiming her way (within ~35 degrees, 80 units): she ducks.
            {
                int ax = ash.pos[0] - leon_pos[0], az = ash.pos[2] - leon_pos[2];
                int ad = (int)sqrt64((u64)((s64)ax * ax + (s64)az * az));
                s64 dot = ((s64)sinLerp(leon_yaw) * ax + (s64)cosLerp(leon_yaw) * az) >> 12;
                ash.duck = aim_w > 0 && ad < inttof32(80) && dot * 10 > (s64)ad * 8;
            }
#ifndef TEST_ASH_VIEW
            // Off a ladder or a ledge with no ladder (a drop): she is there too, once he is away.
            if (!ash.wait && !ash.ladder && ash.state == ASH_FOLLOW && react == REACT_NONE
                && (ash.feet - leon_feet > inttof32(10) || leon_feet - ash.feet > inttof32(10)))
                ashley_place(&ash, &stage_ground, leon_pos[0] - mulf32(sinLerp(leon_yaw), inttof32(5)) + mulf32(cosLerp(leon_yaw), inttof32(7)),
                             leon_pos[2] - mulf32(cosLerp(leon_yaw), inttof32(5)) - mulf32(sinLerp(leon_yaw), inttof32(7)), leon_feet + inttof32(4), leon_yaw);
            ashley_update(&ash, &stage_ground, leon_pos, leon_yaw);
#endif
            if (ev.ally_hit >= 0)
            {
                ashley_hit(&ash, ganados.e[ev.ally_hit].weapon ? WEAPON_DAMAGE : SWIPE_DAMAGE);
                sfx_play_at(SFX_LEON_HURT2, 120, ash.pos, leon_pos, leon_yaw);
            }
            if (ash.state == ASH_DOWN && mode == MODE_PLAY && ++ash_lost >= DEAD_TO_MENU)
            {
                mode = MODE_GAMEOVER;
                dead_timer = DEAD_TO_MENU;
            }
        }
        // Throwers let go: at where Leon stands now. A missile going off hurts whoever is near.
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
                leon_hp -= MISSILE_DAMAGE;
                damage_taken += MISSILE_DAMAGE;
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
                    ashley_hit(&ash, MISSILE_DAMAGE / 3);   // (she ducks; 30 a rock would end stage 7 in a few volleys)
            }
        }
        if (ev.bite >= 0)
            damage = BITE_DAMAGE;
        if (damage)
        {
            leon_hp -= damage;
            damage_taken += damage;
            hurt_flash = 12;
            sfx_play(damage == SWIPE_DAMAGE && leon_hp > 0 ? SFX_LEON_HURT : SFX_LEON_HURT2, 120, 64);
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

        // Stage gimmicks: swinging gates, barrel fuses. A barrel going off fells every Ganado
        // within BLAST_RANGE and hurts Leon within half of it.
#ifdef TEST_HANG_AT   // test builds: at this frame a shot drops the hanging canister
        static u32 hang_frame;
        if (++hang_frame == TEST_HANG_AT)
            for (int i = 0; i < objs.count; i++)
                if (objs.o[i].kind == OBJ_HANG)
                {
#ifdef TEST_HANG_RAY   // (through the shots' own test: a ray from gun height to the canister)
                    const Obj *o = &objs.o[i];
                    int p[3] = { leon_pos[0], leon_feet + inttof32(16), leon_pos[2] };
                    float dx = f32tofloat(o->pos[0] - p[0]), dy = f32tofloat(o->pos[1] - inttof32(3) - p[1]), dz = f32tofloat(o->pos[2] - p[2]);
                    float len = sqrtf(dx * dx + dy * dy + dz * dz);
                    int d[3] = { floattof32(dx / len), floattof32(dy / len), floattof32(dz / len) }, t = inttof32(400);
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
                // There: it bursts (the launcher was a single shot: it's gone, the handgun in hand).
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
        // A Ganado walking into a shut door pushes it open (as they come through the game's).
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
            // (a kind-05 canister's blast is the game's size 5: 20 units, 10 for Leon, on its level)
            int blast = oev.small ? HANG_BLAST : rocket_boom ? ROCKET_BLAST : BLAST_RANGE, leon_blast = oev.small ? HANG_BLAST / 2 : BLAST_RANGE / 2;
            if (rocket_boom)   // the giants and the Garradors are felled, the Verdugo badly hurt
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
                        garrador_shot(&gals[g], 99, 1);
                }
                if (ver.on)
                {
                    int dx = ver.pos[0] - oev.explode_pos[0], dz = ver.pos[2] - oev.explode_pos[2];
                    if ((s64)dx * dx + (s64)dz * dz < (s64)ROCKET_BLAST * ROCKET_BLAST)
                        verdugo_shot(&ver, 30);
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
                    enemies_damage(&ganados, i, 99);
                    sfx_play_at(SFX_GANADO_DOWN, 110, e->pos, leon_pos, leon_yaw);
                    maybe_drop(e, leon_hp, DROP_PERCENT);
                }
            }
            // A liquid nitrogen tank: the Verdugo freezes if near; Leon is not hurt by it.
            if (objs.cold && ver.on)
                verdugo_freeze(&ver, oev.explode_pos, TANK_RANGE);
            int dx = leon_pos[0] - oev.explode_pos[0], dz = leon_pos[2] - oev.explode_pos[2];
            if (!objs.cold && level && (s64)dx * dx + (s64)dz * dz < (s64)leon_blast * leon_blast && react != REACT_DEAD)
            {
                leon_hp -= BLAST_DAMAGE;
                damage_taken += BLAST_DAMAGE;
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

        // More Ganados come in as others fall (at most GANADO_ACTIVE at once); on the other
        // stages, the mission's next ones, wave by wave.
        if (cur_stage != 0 && mode == MODE_PLAY)
            {
            int feet_at[3] = { leon_pos[0], leon_feet, leon_pos[2] };
            foes_step(feet_at);
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

#ifdef TEST_KNOCK_AT   // test builds: the treasures up high come down at this frame
        static u32 knock_frame;
        if (++knock_frame == TEST_KNOCK_AT)
            for (int i = 0; i < DROPS_MAX; i++)
            {
#ifdef TEST_KNOCK_AHEAD   // ...landing this far ahead of Leon (to check the pickup)
                drops.d[i].land[0] = leon_pos[0] + sinLerp(leon_yaw) * TEST_KNOCK_AHEAD;
                drops.d[i].land[1] = leon_feet;
                drops.d[i].land[2] = leon_pos[2] + cosLerp(leon_yaw) * TEST_KNOCK_AHEAD;
#endif
                drops_knock(&drops, i);
            }
#endif
        // Items: walking over one takes it.
        int leon_at[3] = { leon_pos[0], leon_feet, leon_pos[2] };   // (feet: treasures up on ledges stay out of reach)
#ifdef TEST_DROP_CODE   // test builds: at 6.7 s, what that drop code leaves, 20 ahead of Leon
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
        int picked = drops_update(&drops, react == REACT_DEAD ? NULL : leon_at);
        if (picked >= 0)
        {
            sfx_play(SFX_ITEM_GET, 120, 64);
            if (picked == DROP_AMMO)
                spare += AMMO_PICKUP;
            else if (picked == DROP_MONEY)
            {
                ptas += drops.picked_value;
                popup_value = drops.picked_value;
            }
            else if (picked == DROP_TREASURE)
            {
                ptas += TREASURE_PTAS;
                treasures_found++;
            }
            else if (herbs < HERBS_MAX)
                herbs++;
            popup_type = picked;
            popup = POPUP_FRAMES;
        }
        if (popup > 0)
            popup--;

        // Story events: El Gigante kneeling the first time (stage 4), his reward when he falls,
        // Ashley found (stage 5: she waited where the mission has her), the Garrador met (stage 8).
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
            if (ev >= 0 && ((ev == CUT_GIANT_KNEEL && cur_stage == 3) || (ev == CUT_ASHLEY && cur_stage == 4) || (ev == CUT_GARRADOR && cur_stage == 7))
                && cut_start(cur_stage, ev, MODE_PLAY, CUT_AFTER_NONE, 0))
                mode = MODE_CUTIN;
        }

        // Stage clear: every Ganado down and lying still, Leon alive.
#ifdef TEST_NO_CLEAR   // test builds: the stage never clears (to look at what's left lying)
        if (0)
#else
        if (mode == MODE_PLAY && clear_t < 0 && react != REACT_DEAD && (ganados.count > 0 || cur_stage != 0) && (FOES_ON(cur_stage) || giant_count > 0))
#endif
        {
            // Every one of them (waiting ones too) down, and none still falling.
            int all_down = (cur_stage != 0 || enemies_left(&ganados) == 0) && giants_down() && garradors_down()
                           && (!ver.on || ver.state == VER_DEAD) && (cur_stage == 0 || foe_next >= foe_n);
            for (int i = 0; i < ganados.count; i++)
                all_down &= ganados.e[i].state == ENEMY_DEAD || ganados.e[i].thrower;
#ifdef TEST_BOSS   // test builds: he comes at once
            all_down |= boss_phase == 0;
#endif
            // The village down, the chainsaw man comes (as in the game, after its cut-scene
            // lines); the bell rings once he falls, or after BOSS_SURVIVE frames.
            if (all_down && boss_phase == 0 && cur_stage != 0)
            {
                boss_phase = 2;   // (the chainsaw man is stage 1's)
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
#ifdef TEST_CLEAR_AT   // test builds: the mission ends at this frame
        if (frame_no == TEST_CLEAR_AT && clear_t < 0 && mode == MODE_PLAY)
        {
            boss_phase = 2;
            clear_t = 0;
        }
#endif
        // Stage 12: the elevator comes, and Leon gets away in it.
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
            while (GFX_STATUS & (1 << 27))   // (the geometry engine through the FIFO)
                ;
            stage_used = GFX_POLYGON_RAM_USAGE;   // the stage's own share (it is sent first)
        }
#ifdef TEST_GX   // the stage's own share
        if (GFX_POLYGON_RAM_USAGE > gx_stage_poly) gx_stage_poly = GFX_POLYGON_RAM_USAGE;
        if (GFX_VERTEX_RAM_USAGE > gx_stage_vert) gx_stage_vert = GFX_VERTEX_RAM_USAGE;
#endif
        PROF(4);
        // The HUD first: when a crowded scene runs past the hardware's polygon limit, the
        // polygons that are dropped are the last ones sent, never the HUD's.
        if (ctrl == CTRL_LEON && react != REACT_GIANT_CUT)   // (the event screen covers it)
            draw_hud(leon_hp, ammo, hurt_flash, react == REACT_DEAD, popup_type, popup);
        if (ctrl == CTRL_LEON && ash.on && !ash.wait && react != REACT_GIANT_CUT)   // (not before he has found her)
            draw_ash_hud(ash.hp, ASHLEY_HP, ash.wait);
        if (scoping)
            draw_scope();
        // The laser sight while fully aimed: right after the HUD, so a crowded frame (stage 9's
        // two Garradors) drops other polygons, never it.
        if (ctrl == CTRL_LEON && aim_w == inttof32(1) && reload_frame < 0 && !scoping)
        {
            billboard_begin(look_eye, look_at);
            draw_line(muzzle, laser_end, RGB15(31, 2, 2), 10);
            if (laser_hit)
                draw_star(laser_end, floattof32(0.25), RGB15(31, 4, 4), 28, 11);
        }
        if (scope_was != scoping)
            set_fov(scoping ? SCOPE_FOV : 70);
        scope_was = scoping;
        // On a ladder whose ledge is nearer than the climb carries him back (CLIP_CLIMB was made
        // for a ladder leaning onto a ledge 16 out; stage 5's chapel ladder stands against its
        // wall): drawn that much further out as he rises, so his body stops at the wall, not in it.
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
        if (leon_anim != ANIM_OFF && !scoping)   // (through the scope he isn't seen)
        {
            model_draw(&leon, leon_draw, leon_yaw, cull_modes[cull] | POLY_ID(2) | POLY_FOG | (leon_fade < 31 ? POLY_ALPHA(leon_fade) : 0), (const int (*)[12])leon_pose);
            // The handgun is in his hand when it's out, once the hand has reached the holster
            // while drawing/holstering, and once the arm is halfway up when aiming.
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
        billboard_begin(look_eye, look_at);
        objects_draw_fx(&objs);
        missiles_draw(&missiles);
        if (ver.on)
            verdugo_draw_fx(&ver, &spark);
        drops_draw(&drops);

        // Laser sight while fully aimed, muzzle flash and the impact of the last shot.
        if (rocket_on)
        {
            draw_star(rocket_pos, floattof32(0.8), RGB15(31, 24, 10), 26, 12);   // the rocket's flame
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
            // Sparks off walls, blood off Ganados.
            draw_star(impact_pos, floattof32(0.15) * impact_timer, impact_blood ? RGB15(20, 0, 0) : RGB15(31, 22, 10),
                      2 + impact_timer * 2, 14);
            impact_timer--;
        }
        if (ctrl == CTRL_LEON)
        {
            // Text over the HUD: rounds carried, the item just taken, STAGE CLEAR.
            if (mode == MODE_PLAY)
            {
                // (The rounds carried are on the bottom screen, as the original's top shows none.)
                text_hide(SPARE_LINE);
                if (popup > 0)
                {
                    char money[24];
                    snprintf(money, sizeof(money), T_PICK_MONEY, popup_value);
                    const char *s = popup_type == DROP_AMMO ? T_PICK_AMMO : popup_type == DROP_TREASURE ? T_PICK_TREASURE
                                  : popup_type == DROP_MONEY ? money : T_PICK_HERB;
                    int rise = popup > POPUP_FRAMES - 8 ? (popup - (POPUP_FRAMES - 8)) * 2 : 0;
                    text_line(POPUP_LINE, 64, 135 + rise, TEXT_WHITE, s);
                }
                else
                    text_hide(POPUP_LINE);
                if (clear_t >= CLEAR_TEXT_AT)
                    text_big(CLEAR_LINE, 128 - text_width(T_COMPLETE), 70, TEXT_GOLD, T_COMPLETE);
                // The mission's objective at the start; the chainsaw man's lines as he comes
                // (the roar of an engine, in two lines to fit the screen, then Leon's remark).
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
                    const char *ob = cur_stage == 0 ? T_OBJ_1 : T_OBJ_N;
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
                // What A does here (when free to act and not aiming).
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
        model_sync();   // (the last character list done)
#ifdef TEST_GX   // test builds: the geometry load, peak over a second (see the vital screen)
        {
            while (GFX_STATUS & (1 << 27))   // the geometry engine through the FIFO
                ;
            int pc = GFX_POLYGON_RAM_USAGE, vc = GFX_VERTEX_RAM_USAGE;
            if (pc > gx_poly_peak) gx_poly_peak = pc;
            if (vc > gx_vert_peak) gx_vert_peak = vc;
            u32 st = GFX_CONTROL;
            if (st & (1 << 12)) gx_underflows++;   // the renderer ran out of time on some lines
            int rd = *(vu8 *)0x04000320;            // RDLINES_COUNT: fewest lines of the buffer left free (0: lines lost)
            if (rd < gx_rdlines_min) gx_rdlines_min = rd;
            if (st & (1 << 13)) gx_overflows++;    // polygons or vertices past the limit were dropped
            GFX_CONTROL = st | (1 << 12) | (1 << 13);
        }
#endif
        if (stage.chunked && mode == MODE_PLAY)
        {
            // What this frame really kept: the stage's share follows it, a little each frame.
            while (GFX_STATUS & (1 << 27))   // (the geometry engine through the FIFO)
                ;
            int used = GFX_POLYGON_RAM_USAGE;
            if (used >= POLY_HIGH)
            {
                // The stage gives way down to POLY_STAGE_MIN; past that (or where the stage is
                // small anyway, as in stage 6's cabin, and the characters fill the frame), the
                // farthest enemy goes.
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
        // The time a frame takes: on the real DS a crowded scene can take several vblanks (18 fps
        // in stage 5's chapel, where an emulator stays at 60). Frames slower than one vblank, most
        // of a half second: the stage and the enemies drawn are capped lower, step by step; a
        // second at full speed lets them back up.
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

        frames++;
        if (vblank_count - fps_mark >= 60)
        {
            fps = frames;
            frames = 0;
            fps_mark = vblank_count;
        }

        // Bottom screen: the vital screen. Touching a slot selects it; the herbs, used with a
        // double tap, give health back. Not while dead or with the free camera
        // (its touch-drag looks around).
#ifdef TEST_INF_AMMO   // test builds: never short of rounds
        if (spare < 20) spare = 20;
#endif
        Vital v = { leon_hp, LEON_HP, ammo, MAG_SIZE, spare, herbs, HERB_HEAL, (int)fps, weapons[wep].name, { 0, 0 }, { 0, 0 } };
        item_icon(weapons[wep].item, &v.gun_icon[0], &v.gun_icon[1]);   // (the equipped gun's, as in the status screen)
        item_icon(AMMO_ITEM(wep), &v.ammo_icon[0], &v.ammo_icon[1]);
#ifdef TEST_SHOW_FOES   // test builds: FPS field = foes tried x 100 + moved near Leon; herbs: foes standing
        v.fps = foe_tries * 100 + foe_moves;
        v.herbs = foes_standing();
        {
            int best = 9999;   // spare: the nearest one's distance (units); ammo: its height - Leon's
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
#endif
#ifdef TEST_SHOW_TILT   // test builds: FPS field = the aim tilt (percent, 100 up, -100 down)
        v.fps = aim_tilt * 100 / 4096;
#endif
#ifdef TEST_SHOW_REACT   // test builds: FPS field = Leon's reaction x 100 + grabs so far
        {
            static int grabs, was_held;
            grabs += react == REACT_HELD && !was_held;
            was_held = react == REACT_HELD;
            v.fps = react * 100 + grabs;
        }
#endif
#ifdef TEST_PROF
        // Peak times (0.1 ms) over the last second: poses/laser, enemies, the rest of the
        // update, waiting for the stage DMA, drawing the characters.
        v.hp = prof_peak[1]; v.ammo = prof_peak[2]; v.spare = prof_peak[3]; v.herbs = prof_peak[4]; v.fps = prof_peak[5]; v.mag = prof_peak[6];
        if (tick % 60 == 0)
            memset(prof_peak, 0, sizeof(prof_peak));
#endif
#ifdef TEST_GX   // FPS field: polygons (of 2048); spare: vertices (of 6144); herbs: frames that
        // dropped polygons (over the last second)
        // ammo / magazine (Loaded a/b): the stage's own polygons / vertices
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
#ifdef TEST_SHOW_VER   // Verdugo: health * 10 + state; spare = tanks standing
        v.fps = ver.hp * 10 + ver.state;
        {
            int tanks = 0;
            for (int i = 0; i < objs.count; i++)
                tanks += objs.o[i].kind == OBJ_BARREL && objs.o[i].alive;
            v.spare = tanks;
#ifdef TEST_SHOW_TANKS
            v.fps = tanks * 100 + ver.state * 10 + (ver.hp > 99 ? 9 : ver.hp / 12);
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
            int want = (int)(atan2f((float)tx, (float)tz) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
            ap_aim_err = (s16)(want - leon_yaw);
        }
#endif
#ifdef TEST_SHOW_GIANT   // the first giant: state, frames in it, cuts, shots left
        {
            char gs[56];
            snprintf(gs, sizeof(gs), "s%d c%d h%d L%d g%d o%d r%d w%d d%d", giant.state, giant.cuts, giant.hp, giant.life, laser_giant, laser_obj, laser_drop, laser_hit,
                     (int)(sqrt64((u64)((s64)(giant.pos[0] - leon_pos[0]) * (giant.pos[0] - leon_pos[0]) + (s64)(giant.pos[2] - leon_pos[2]) * (giant.pos[2] - leon_pos[2]))) >> 12));
            text_line(0, 4, 4, TEXT_GOLD, gs);
        }
#endif
#ifdef TEST_SHOW_POS   // where Leon stands
        {
            char pos_s[32];
            snprintf(pos_s, sizeof(pos_s), "%d %d %d r%d a%d %d", leon_pos[0] >> 12, leon_feet >> 12, leon_pos[2] >> 12, react, ash.ladder, ash.feet >> 12); snprintf(pos_s + strlen(pos_s), 12, " %d,%d", ash.pos[0] >> 12, ash.pos[2] >> 12);
            text_line(0, 4, 4, TEXT_GOLD, pos_s);
        }
#endif
#ifdef TEST_SHOW_DROP   // the first treasure: horizontal distance to Leon (units) x10 + aloft; spare = height above Leon's feet
        for (int i = 0; i < DROPS_MAX; i++)
            if (drops.d[i].active && drops.d[i].type == DROP_TREASURE)
            {
                int dx = (drops.d[i].pos[0] - leon_pos[0]) >> 12, dz = (drops.d[i].pos[2] - leon_pos[2]) >> 12;
                v.fps = (int)sqrtf((float)(dx * dx + dz * dz)) * 10 + drops.d[i].aloft;
                v.spare = (drops.d[i].pos[1] - leon_feet) >> 12;
                break;
            }
#endif
#ifdef TEST_SHOW_GAL   // Garrador: health * 10 + state
        v.fps = gal.hp * 10 + gal.state;
        v.spare = laser_gal * 10 + laser_gal_back;
#endif
#ifdef TEST_SHOW_THROW   // throws so far x100 + throwers' state of the first one x10 + missiles flying
        {
            int fl = 0, th = -1;
            for (int i = 0; i < MISSILES_MAX; i++) fl += missiles.m[i].t >= 0;
            for (int i = 0; i < ganados.count; i++) if (ganados.e[i].thrower) { th = i; break; }
            v.fps = throws_seen * 100 + (th >= 0 ? ganados.e[th].state * 10 : 90) + fl;
            v.spare = th >= 0 ? ganados.e[th].cooldown : -1;
            v.herbs = th >= 0 ? ganados.e[th].active : -1;
        }
#endif
#ifdef TEST_SHOW_FOES   // the mission's enemies: brought in x100 + current wave x10 + standing
        v.fps = foe_next * 100 + foe_wave * 10 + foes_standing();
        v.spare = foe_n;   // (spare slot: how many in all)
#endif
#ifdef TEST_SHOW_GIANT   // giant: shots left * 100 + state * 10 + cuts
        v.fps = giant.hp * 100 + giant.state * 10 + giant.cuts;
        v.herbs = giant_count;   // the second: herb slot = how many, spare = its shots left x100 + state x10 + cuts
        v.spare = giants[1].hp * 100 + giants[1].state * 10 + giants[1].cuts;
#endif
#ifdef TEST_KNIFE_VIEW
        v.fps = (int)((vblank_count / 120) % 7);
#endif
        int act = vital_update(&v, down, tick, ctrl == CTRL_LEON && mode == MODE_PLAY && react != REACT_DEAD);
        if (act == VITAL_SELECT)
            sfx_play(SFX_MENU, 70, 64);
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
