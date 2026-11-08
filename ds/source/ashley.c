#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ashley.h"

// Clips of ash_anim.bin (ch_ash.mcas #0..15, in order; seen with -DTEST_ASH_VIEW, which shows
// each in turn, number in the FPS field): 0-1 walk, 2 run, 4 idle (long), 7 crouching down,
// 8 crouched with her hands over her head, 11 climbing a ladder (as Leon's: 32 up, 16 in),
// 12 hit, 13 falling down.
#ifndef ASH_CLIP_IDLE
#define ASH_CLIP_IDLE  4
#endif
#ifndef ASH_CLIP_WALK
#define ASH_CLIP_WALK  1
#endif
#ifndef ASH_CLIP_RUN
#define ASH_CLIP_RUN   2
#endif
#ifndef ASH_CLIP_HURT
#define ASH_CLIP_HURT  12
#endif
#ifndef ASH_CLIP_DOWN
#define ASH_CLIP_DOWN  13
#define ASH_CLIP_DUCK  8
#endif
#define ASH_CLIP_CLIMB 11
#define LADDER_IN     inttof32(16)       // the climb ends this far in from the foot
#define LADDER_NEAR    inttof32(4)       // at its start this close (then on it)
#define LADDER_GIVE_UP (60 * 8)          // not at the ladder after this long: she is just there

#define ANIM_STEP     (inttof32(20) / 60)
#define FOLLOW_NEAR   inttof32(3)        // stops this close to her place by Leon...
#define BEHIND        inttof32(5)        // (her place: this far behind him...
#define BESIDE        inttof32(7)        // ...and this far to his left)
#define FOLLOW_RUN    inttof32(28)       // ...runs when farther than this
#define WAIT_NEAR     inttof32(30)       // waiting, she joins him this close
#define WALK_SPEED    floattof32(0.22)
#define RUN_SPEED     floattof32(0.45)
#define TURN          degreesToAngle(6)
#define RADIUS        floattof32(2)
#define HEIGHT        inttof32(15)
#define STEP_UP       inttof32(3)
#define DUCK_HEIGHT   inttof32(7)

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

int ashley_load(Ashley *a)
{
    if (a->loaded)
        return 0;
    size_t ms, as;
    a->model_buf = read_all("nitro:/ash.bin", &ms);
    a->anim_buf = read_all("nitro:/ash_anim.bin", &as);
    if (!a->model_buf || !a->anim_buf)
        return -1;
    memset(&a->model, 0, sizeof(a->model));
    if (model_load(&a->model, a->model_buf, ms))
        return -2;
    if (anim_load(&a->anims, a->anim_buf, as) || a->anims.nodes != a->model.node_count)
        return -3;
    a->loaded = 1;
    return 0;
}

void ashley_unload(Ashley *a)
{
    if (!a->loaded)
        return;
    model_free(&a->model);
    free(a->model_buf);
    free(a->anim_buf);
    a->model_buf = a->anim_buf = NULL;
    a->loaded = 0;
}

void ashley_place(Ashley *a, const Ground *g, int x, int z, int ref_y, int yaw)
{
    a->pos[0] = x;
    a->pos[2] = z;
    a->feet = ref_y;
    ground_height(g, x, z, ref_y, STEP_UP, &a->feet);
    a->pos[1] = a->feet;   // (her bind pose stands on the origin; her bbox does not start there)
    a->yaw = yaw;
    a->state = ASH_FOLLOW;
    a->frame = 0;
    a->clip = -1;
    a->hp = ASHLEY_HP;
    a->wait = 0;
    a->ladder = 0;
}

static void play(Ashley *a, int clip);

void ashley_use_ladder(Ashley *a, const int foot[3], const int dir[2], int top, int yaw, int up)
{
    if (a->state == ASH_DOWN || a->wait)
        return;
    memcpy(a->lad_foot, foot, sizeof(a->lad_foot));
    a->lad_dir[0] = dir[0];
    a->lad_dir[1] = dir[1];
    a->lad_top = top;
    a->lad_yaw = yaw;
    a->lad_up = up;
    a->lad_t = 0;
    a->ladder = LADDER_GOING;
}

// Where her climb starts (on its floor) and ends.
static void ladder_end(const Ashley *a, int start, int p[3])
{
    int in = start ? !a->lad_up : a->lad_up;   // up: from the foot to the top; down: the other way
    p[0] = a->lad_foot[0] + (in ? mulf32(a->lad_dir[0], LADDER_IN) : 0);
    p[2] = a->lad_foot[2] + (in ? mulf32(a->lad_dir[1], LADDER_IN) : 0);
    p[1] = in ? a->lad_top : a->lad_foot[1];
}

static void walk_to(Ashley *a, const Ground *g, int tx, int tz, int near, int *dist)
{
    int dx = tx - a->pos[0], dz = tz - a->pos[2];
    *dist = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
    int speed = 0;
    if (*dist > near)
    {
        // Turn towards it, then walk (or run, when far).
        int want = (int)(atan2f((float)dx, (float)dz) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
        int diff = ((want - a->yaw + DEGREES_IN_CIRCLE / 2) & (DEGREES_IN_CIRCLE - 1)) - DEGREES_IN_CIRCLE / 2;   // (the short way: a turn is 32768)
        a->yaw = (a->yaw + (diff > TURN ? TURN : diff < -TURN ? -TURN : diff)) & (DEGREES_IN_CIRCLE - 1);
        speed = *dist > FOLLOW_RUN ? RUN_SPEED : WALK_SPEED;
        if (speed > *dist)
            speed = *dist;
    }
    play(a, speed == 0 ? ASH_CLIP_IDLE : speed == RUN_SPEED ? ASH_CLIP_RUN : ASH_CLIP_WALK);
    if (speed)
    {
        a->pos[0] += mulf32(sinLerp(a->yaw), speed);
        a->pos[2] += mulf32(cosLerp(a->yaw), speed);
        ground_collide_n(g, &a->pos[0], &a->pos[2], a->feet + STEP_UP, a->feet + HEIGHT, RADIUS, 1);
        ground_height(g, a->pos[0], a->pos[2], a->feet, STEP_UP, &a->feet);
        a->pos[1] = a->feet;
    }
}

// The ladder after Leon: to its start, the climb (the clip, backwards going down), then off it.
// Returns 1 while it has her.
static int ladder_update(Ashley *a, const Ground *g)
{
    int p[3];
    if (a->ladder == LADDER_GOING)
    {
        int dist;
        ladder_end(a, 1, p);
        walk_to(a, g, p[0], p[2], LADDER_NEAR, &dist);
        if (dist <= LADDER_NEAR || ++a->lad_t > LADDER_GIVE_UP)
        {
            // On it: the clip moves her from the foot (the model stays there) up and in.
            a->pos[0] = a->lad_foot[0];
            a->pos[2] = a->lad_foot[2];
            a->feet = a->pos[1] = a->lad_foot[1];
            a->yaw = a->lad_yaw;
            a->clip = -1;
            play(a, ASH_CLIP_CLIMB);
            a->ladder = LADDER_ON;
        }
        else
        {
            int len = inttof32(a->anims.frames[a->clip]);
            if (a->frame >= len)
                a->frame -= len;
        }
        return 1;
    }
    if (a->ladder == LADDER_ON)
    {
        if (a->frame >= inttof32(a->anims.frames[ASH_CLIP_CLIMB]) - 1)
        {
            ladder_end(a, 0, p);
            a->pos[0] = p[0];
            a->pos[2] = p[2];
            a->feet = p[1];
            ground_height(g, p[0], p[2], p[1] + STEP_UP, STEP_UP, &a->feet);
            a->pos[1] = a->feet;
            a->ladder = 0;
            a->clip = -1;
            play(a, ASH_CLIP_IDLE);
            return 0;
        }
        return 1;
    }
    return 0;
}

static void play(Ashley *a, int clip)
{
    if (clip != a->clip)
    {
        a->clip = clip;
        a->frame = 0;
    }
}

void ashley_update(Ashley *a, const Ground *g, const int leon_pos[3], int leon_yaw)
{
    int len = inttof32(a->anims.frames[a->clip >= 0 ? a->clip : 0]);
    a->frame += ANIM_STEP;
    if (a->state == ASH_DOWN)
    {
        if (a->frame > len - 1)
            a->frame = len - 1;   // lies still
        return;
    }
    if (a->state == ASH_HURT)
    {
        if (a->frame >= len)
            a->state = ASH_FOLLOW;
        return;
    }

    if (ladder_update(a, g))
        return;
    if (a->duck)
    {
        // Leon aims her way: she ducks (crouched, hands over her head) and stays put.
        play(a, ASH_CLIP_DUCK);
        len = inttof32(a->anims.frames[a->clip]);
        if (a->frame >= len)
            a->frame -= len;
        return;
    }
    if (a->wait)
    {
        int wx = leon_pos[0] - a->pos[0], wz = leon_pos[2] - a->pos[2], wy = leon_pos[1] - a->pos[1];
        if (wy > -WAIT_NEAR && wy < WAIT_NEAR && (s64)wx * wx + (s64)wz * wz < (s64)WAIT_NEAR * WAIT_NEAR)
            a->wait = 0;
        play(a, ASH_CLIP_IDLE);
        len = inttof32(a->anims.frames[a->clip]);
        if (a->frame >= len)
            a->frame -= len;
        return;
    }
    // She keeps to his left, a little behind (the camera is over his right shoulder).
    int fx = sinLerp(leon_yaw), fz = cosLerp(leon_yaw);
    int tx = leon_pos[0] - mulf32(fx, BEHIND) + mulf32(fz, BESIDE);
    int tz = leon_pos[2] - mulf32(fz, BEHIND) - mulf32(fx, BESIDE);
    int dx = tx - a->pos[0], dz = tz - a->pos[2];
    int dist = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
    int speed = 0;
    if (a->state == ASH_FOLLOW && dist > FOLLOW_NEAR)
    {
        // Turn towards him, then walk (or run, when far behind).
        int want = (int)(atan2f((float)dx, (float)dz) * (DEGREES_IN_CIRCLE / 2) / 3.14159265f);
        int diff = ((want - a->yaw + DEGREES_IN_CIRCLE / 2) & (DEGREES_IN_CIRCLE - 1)) - DEGREES_IN_CIRCLE / 2;   // (the short way: a turn is 32768)
        a->yaw = (a->yaw + (diff > TURN ? TURN : diff < -TURN ? -TURN : diff)) & (DEGREES_IN_CIRCLE - 1);
        speed = dist > FOLLOW_RUN ? RUN_SPEED : WALK_SPEED;
    }
    play(a, speed == 0 ? ASH_CLIP_IDLE : speed == RUN_SPEED ? ASH_CLIP_RUN : ASH_CLIP_WALK);
    if (speed)
    {
        a->pos[0] += mulf32(sinLerp(a->yaw), speed);
        a->pos[2] += mulf32(cosLerp(a->yaw), speed);
        ground_collide_n(g, &a->pos[0], &a->pos[2], a->feet + STEP_UP, a->feet + HEIGHT, RADIUS, 1);
        ground_height(g, a->pos[0], a->pos[2], a->feet, STEP_UP, &a->feet);
        a->pos[1] = a->feet;   // (her bind pose stands on the origin; her bbox does not start there)
    }
    len = inttof32(a->anims.frames[a->clip]);
    if (a->frame >= len)
        a->frame -= len;   // loops
}

int ashley_hit(Ashley *a, int damage)
{
    if (a->state == ASH_DOWN)
        return 0;
    a->hp -= damage;
    a->state = a->hp <= 0 ? ASH_DOWN : ASH_HURT;
    a->clip = -1;
    play(a, a->state == ASH_DOWN ? ASH_CLIP_DOWN : ASH_CLIP_HURT);
    if (a->hp < 0)
        a->hp = 0;
    return a->state == ASH_DOWN;
}

int ashley_raycast(const Ashley *a, const int p[3], const int d[3], int *t_best)
{
    // Vertical cylinder, as for the Ganados.
    s64 aa = ((s64)d[0] * d[0] + (s64)d[2] * d[2]) >> 12;
    if (aa == 0 || a->state == ASH_DOWN)
        return 0;
    s64 fx = p[0] - a->pos[0], fz = p[2] - a->pos[2];
    s64 b = (fx * d[0] + fz * d[2]) >> 12;
    s64 c = ((fx * fx + fz * fz) >> 12) - (((s64)RADIUS * RADIUS) >> 12);
    s64 disc = b * b - aa * c;
    if (disc < 0)
        return 0;
    s64 t = ((-b - (s64)sqrt64(disc)) << 12) / aa;
    if (t <= 0 || t >= *t_best)
        return 0;
    s64 y = p[1] + ((d[1] * t) >> 12) - a->feet;
    if (y < 0 || y > (a->duck ? DUCK_HEIGHT : HEIGHT))   // ducked: shots go over her
        return 0;
    *t_best = (int)t;
    return 1;
}

void ashley_push(const Ashley *a, int *x, int *z, int r)
{
    int ox = *x - a->pos[0], oz = *z - a->pos[2], min = r + RADIUS;
    int d = (int)sqrt64((u64)((s64)ox * ox + (s64)oz * oz));
    if (d > 0 && d < min)
    {
        *x = a->pos[0] + mulf32(divf32(ox, d), min);
        *z = a->pos[2] + mulf32(divf32(oz, d), min);
    }
}

void ashley_draw(Ashley *a, u32 poly_fmt)
{
    if (!view_visible(a->pos, inttof32(10)))
        return;
    int clip = a->clip >= 0 ? a->clip : 0, frame = a->frame;
    if (a->ladder == LADDER_ON && !a->lad_up)   // (going down: the climb backwards)
        frame = inttof32(a->anims.frames[clip]) - 1 - frame;
    if (a->ladder == LADDER_ON && frame > inttof32(a->anims.frames[clip]) - 1)
        frame = inttof32(a->anims.frames[clip]) - 1;
#ifdef TEST_ASH_VIEW   // test builds: each clip in turn, 2.5 s each
    static u32 t;
    t++;
    clip = (t / 150) % a->anims.count;
    a->clip = clip;   // (shown in the FPS field)
    frame = (int)(t % 150) * ANIM_STEP % inttof32(a->anims.frames[clip] > 0 ? a->anims.frames[clip] : 1);
#endif
    // The pose at the 20 Hz animation rate (every third frame), or when the clip changes.
    static int last_clip = -1;
    static u32 tick;
    if (clip != last_clip || tick++ % 3 == 0)
        anim_pose(&a->anims, clip, frame, &a->model, a->pose);
    last_clip = clip;
    model_draw(&a->model, a->pos, a->yaw, poly_fmt, (const int (*)[12])a->pose);
}
