#include "enemy.h"

// Order of animations in ganado_anim.bin (re4zeebo -DsAnim ...#6,7,12,21,9,8,10)
#define ANIM_IDLE  0
#define ANIM_WALK  1
#define ANIM_HIT   2
#define ANIM_DEATH 3
#define ANIM_SWIPE 4   // #9: both arms swing forward (reach ~8 units, frames 9-12)
#define ANIM_LUNGE 5   // #8: steps in reaching for Leon (arms out at frames 9-21)
#define ANIM_HOLD  6   // #10: holding on and biting, loops
static const u8 state_anim[] = { ANIM_IDLE, ANIM_WALK, ANIM_SWIPE, ANIM_LUNGE, ANIM_HOLD,
                                 ANIM_HIT, ANIM_DEATH, ANIM_DEATH };

#define ENEMY_HP      3
#define FAR_STEP      inttof32(100)      // walking farther than this from Leon: half-rate moves
#define HAND_NODE     14                 // right wrist (the skeleton is laid out like Leon's)
#define SIGHT         inttof32(120)      // starts walking towards Leon inside this distance
#define NOTICE_NEAR   inttof32(40)       // watching ones notice him this close whichever way they face
#define ALERT_SPREAD  inttof32(80)       // ...and those this near one that noticed him notice too
#define STOP          inttof32(7)        // ...and stops this close to him
#define WALK_SPEED    floattof32(0.11)   // per 60 Hz frame (~7 units/s, a shamble)
#define TURN_SPEED    degreesToAngle(2)
#define RADIUS        floattof32(2.2)    // body cylinder for each other and shots
#define WALL_RADIUS   floattof32(3.2)    // ...and against walls: shoulders and swinging arms stay out of them
#define BODY_RADIUS   inttof32(6)        // a body falling (backwards) and lying: kept this far off walls
#define HEIGHT        inttof32(17)
#define HEAD_FROM     floattof32(14.5)   // shots above this height (from the feet) hit the head
#define STEP_UP       inttof32(3)
#define ANIM_STEP     (inttof32(20) / 60)   // animations run at 20 frames/s

// Attacks. The hands reach ~8 units in front of the feet; Leon's body adds ~2.5.
#define ATTACK_DIST   inttof32(10)       // starts an attack inside this distance...
#define REACH         floattof32(10.5)   // ...which connects if Leon is still inside this
#define SWIPE_FROM    inttof32(9)        // swipe: frames where the arms are out
#define SWIPE_TO      inttof32(13)
#define LUNGE_FROM    inttof32(9)        // lunge: frames where the hands can catch him
#define LUNGE_TO      inttof32(21)
#define COOLDOWN      240                // frames before the same Ganado attacks again (+0..1.5 s)
#define GROUP_REST    75                 // frames after any attack before the next one starts
#define RELEASE_PUSH  inttof32(3)        // shoved back this far when Leon breaks free
#ifndef LUNGE_PERCENT                    // share of attacks that are lunges (grab attempts);
#define LUNGE_PERCENT 40                 // test builds: -DLUNGE_PERCENT=100
#endif

// Armed Ganados use the weapon animation sets for standing, walking and their swing
// (ch_gna_w00: one-handed sickle/hatchet, ch_gna_w01: pitchfork held in both hands); hit
// reactions and going down come from the base set.
typedef struct { int set, idle, walk, swing; } WeaponClips;
static const WeaponClips weapon_clips[WEAPON_KINDS] = {
    { -1, 0, 0, 0 }, { 0, 0, 1, 2 }, { 0, 0, 1, 2 }, { 1, 0, 1, 2 } };
#define WEAPON_REACH  floattof32(11.5)   // a weapon reaches a little further than hands...
#define FORK_REACH    floattof32(14)     // ...a pitchfork further still
#define DAZE_FRAMES   90                 // a head shot leaves it dazed (kickable) this long
#define KICK_RANGE    inttof32(10)       // Leon's kick reaches this far in front of him
#define KICK_PUSH     inttof32(4)
#define THROW_RANGE   inttof32(90)       // throwers throw at Leon within this distance, in sight...
#define THROW_EVERY   200                // ...every 3.3 s (+ up to 1.5 s)

// The chainsaw man: ch_cha.mcas clips for standing, walking and the chainsaw swing; hit
// reactions and going down are the Ganados' (the skeletons match). The pistol barely hurts
// him, and he walks faster than the villagers.
#ifndef BOSS_IDLE
#define BOSS_IDLE     0
#define BOSS_WALK     1
#define BOSS_SWING    4
#endif
#define BOSS_HP       40
#define BOSS_SPEED    floattof32(0.16)
#define BOSS_REACH    floattof32(12.5)

static int set_len(const AnimSet *a, int anim)
{
    return inttof32(a->frames[anim] > 0 ? a->frames[anim] : 1);
}

static int anim_len(const Enemies *s, int anim)
{
    return set_len(s->anims, anim);
}

// The animation set and clip enemy e plays in its current state.
static const AnimSet *clip_of(const Enemies *s, const Enemy *e, int *anim)
{
    if (e->boss && e->state <= ENEMY_SWIPE)
    {
        *anim = e->state == ENEMY_IDLE ? BOSS_IDLE : e->state == ENEMY_WALK ? BOSS_WALK : BOSS_SWING;
        return s->boss_anims;
    }
    const WeaponClips *w = &weapon_clips[e->weapon];
    if (e->weapon && s->weapon_anims[w->set] && e->state <= ENEMY_SWIPE)
    {
        *anim = e->state == ENEMY_IDLE ? w->idle : e->state == ENEMY_WALK ? w->walk : w->swing;
        return s->weapon_anims[w->set];
    }
    *anim = state_anim[e->state];
    return s->anims;
}

static int state_len(const Enemies *s, const Enemy *e)
{
    int anim;
    const AnimSet *a = clip_of(s, e, &anim);
    return set_len(a, anim);
}

static u32 rnd(Enemies *s)
{
    s->rng = s->rng * 1664525 + 1013904223;
    return s->rng >> 16;
}

#define CORPSE_FRAMES 300                 // a body lies this long, then goes (polygon budget)
#define FALL_MIN      inttof32(6)         // floors lower than this below the feet: fall to them
#define GRAVITY       floattof32(0.06)    // per frame, per frame

void enemies_init(Enemies *s, const Model *m, const AnimSet *a, const Ground *g, const Objects *o,
                  const int (*xyz)[3], int count, int yaw)
{
    s->model = m;
    s->anims = a;
    s->ground = g;
    s->objects = o;
    s->count = count > ENEMY_MAX ? ENEMY_MAX : count;
    s->rng = 12345;
    for (int i = 0; i < s->count; i++)
    {
        enemies_respawn(s, i, xyz[i], yaw);
        s->e[i].active = 0;
        s->e[i].frame = inttof32(i * 7);   // out of step with each other
        s->e[i].drop = -1;
    }
    s->attacker = -1;
    s->rest = 0;
    s->kills = 0;
    s->tick = 0;
    s->sight = SIGHT;
    s->watch = 0;
    s->ally = NULL;
}

void enemies_perch(Enemies *s, int i, int y)
{
    Enemy *e = &s->e[i];
    e->feet = inttof32(y);
    e->pos[1] = e->feet - s->model->bbox_min[1];
}

// A thrower sees Leon: no wall or floor between its hand and his chest (else it waits: its
// missiles would otherwise come over every wall of the stage, wherever he is).
static int sees(const Enemies *s, const Enemy *e, const int t[3])
{
    int from[3] = { e->pos[0], e->feet + inttof32(14), e->pos[2] };
    int to[3] = { t[0], t[1] + inttof32(6), t[2] };
    int d[3] = { to[0] - from[0], to[1] - from[1], to[2] - from[2] };
    int len = (int)sqrt64((u64)((s64)d[0] * d[0] + (s64)d[1] * d[1] + (s64)d[2] * d[2]));
    if (len <= 0)
        return 1;
    for (int k = 0; k < 3; k++)
        d[k] = divf32(d[k], len);
    int hit[3];
    return !ground_raycast(s->ground, from, d, len - inttof32(3), hit, RAY_SHOT);
}

void enemies_make_thrower(Enemies *s, int i)
{
    s->e[i].thrower = 1;
    s->e[i].weapon = WEAPON_NONE;   // (the missile is in the hand)
    s->e[i].cooldown = 120 + i * 30;
}

void enemies_set_look(Enemies *s, const Model *m, const AnimSet *a, const AnimSet *one_hand)
{
    s->model = m;
    s->anims = a;
    s->weapon_anims[0] = one_hand;
    for (int i = 0; i < s->count; i++)
        s->e[i].pose_dirty = 1;
}

void enemies_spawn_boss(Enemies *s, int i, const int xyz[3], int yaw, const Model *m, const AnimSet *a)
{
    s->boss_model = m;
    s->boss_anims = a;
    enemies_respawn(s, i, xyz, yaw);
    Enemy *e = &s->e[i];
    e->boss = 1;
    e->weapon = WEAPON_NONE;   // the chainsaw is part of his model
    e->hp = BOSS_HP;
    e->pose_dirty = 1;
}

void enemies_arm(Enemies *s, const Model *const models[WEAPON_KINDS], const AnimSet *one_hand, const AnimSet *fork)
{
    for (int k = 0; k < WEAPON_KINDS; k++)
        s->weapon_model[k] = models[k];
    s->weapon_anims[0] = one_hand;
    s->weapon_anims[1] = fork;
}

void enemies_respawn(Enemies *s, int i, const int xyz[3], int yaw)
{
    Enemy *e = &s->e[i];
    e->pos[0] = inttof32(xyz[0]);
    e->pos[2] = inttof32(xyz[2]);
    e->feet = 0;
    // The floor at the spawn height (ground, walkway or roof).
    ground_height(s->ground, e->pos[0], e->pos[2], inttof32(xyz[1]), STEP_UP, &e->feet);
    e->pos[1] = e->feet - s->model->bbox_min[1];
    e->active = 1;
    e->dead_frames = 0;
    e->falling = 0;
    e->yaw = yaw;
    e->state = ENEMY_IDLE;
    e->frame = 0;
    e->hp = ENEMY_HP;
    e->cooldown = 0;
    e->struck = 0;
    e->spotted = 0;
    e->spot_told = 0;
    e->daze = 0;
    e->boss = 0;
    e->thrower = 0;
    // A quarter bare-handed (they grab), the rest armed.
    u32 r = rnd(s) % 100;
    e->weapon = r < 25 ? WEAPON_NONE : r < 55 ? WEAPON_SICKLE : r < 75 ? WEAPON_AXE : WEAPON_FORK;
#ifdef TEST_WEAPON   // test builds: everyone carries this one
    e->weapon = TEST_WEAPON;
#endif
    e->pose_dirty = 1;
}

static void set_state(Enemies *s, int i, int state)
{
    if (state == ENEMY_DYING && s->e[i].state < ENEMY_DYING)
        s->kills++;
    Enemy *e = &s->e[i];
    if (state == ENEMY_DYING && !e->thrower)
    {
        // The body falls back its length: moved off the walls first, so it lies on the floor,
        // not half in a wall.
        ground_collide_n(s->ground, &e->pos[0], &e->pos[2], e->feet + STEP_UP, e->feet + HEIGHT, BODY_RADIUS, 2);
        ground_height(s->ground, e->pos[0], e->pos[2], e->feet + STEP_UP, STEP_UP * 2, &e->feet);
        e->pos[1] = e->feet - s->model->bbox_min[1];
    }
    if (s->attacker == i && state != ENEMY_SWIPE && state != ENEMY_LUNGE && state != ENEMY_GRAB)
    {
        s->attacker = -1;   // the attack is over: someone else may go, after a pause
        s->rest = GROUP_REST;
        e->cooldown = COOLDOWN + rnd(s) % 90;
    }
    if (state != ENEMY_HIT)
        e->daze = 0;
    e->state = state;
    e->frame = 0;
    e->struck = 0;
    e->pose_dirty = 1;
}

// The animations run at 20 frames/s, so a pose only needs recomputing every third
// 60 Hz frame; enemies take turns (i + tick) so the cost is spread out. State changes
// recompute at once.
static void update_pose(Enemies *s, Enemy *e)
{
    int anim;
    const AnimSet *a = clip_of(s, e, &anim);
    int frame = e->frame;
    if (e->state == ENEMY_DEAD)
        frame = anim_len(s, ANIM_DEATH) - 1;   // last frame (frames past the end would wrap)
    anim_pose(a, anim, frame, s->model, e->pose);
    e->pose_dirty = 0;
}

static void move(Enemies *s, Enemy *e, int dx, int dz)
{
    e->pos[0] += dx;
    e->pos[2] += dz;
    // (two passes: in a corner one wall's push can put it into the other)
    ground_collide_n(s->ground, &e->pos[0], &e->pos[2], e->feet + STEP_UP, e->feet + HEIGHT, WALL_RADIUS, 2);
    if (s->objects)
        objects_collide(s->objects, &e->pos[0], &e->pos[2], e->feet, RADIUS);
    if (e->falling)
        return;
    int floor = e->feet;
    ground_height(s->ground, e->pos[0], e->pos[2], e->feet, STEP_UP, &floor);
    if (floor < e->feet - FALL_MIN)
    {
        // Walked off a ledge or roof: drop (see enemies_update()).
        e->falling = 1;
        e->fall_to = floor;
        e->fall_vy = 0;
        return;
    }
    e->feet = floor;
    e->pos[1] = e->feet - s->model->bbox_min[1];
}

void enemies_activate(Enemies *s, int i, const int *at)
{
    Enemy *e = &s->e[i];
    if (at)
    {
        e->pos[0] = at[0];
        e->pos[2] = at[1];
        ground_height(s->ground, e->pos[0], e->pos[2], e->feet, STEP_UP, &e->feet);
        e->pos[1] = e->feet - s->model->bbox_min[1];
    }
    e->active = 1;
    e->pose_dirty = 1;
}

int enemies_active_standing(const Enemies *s)
{
    int n = 0;
    for (int i = 0; i < s->count; i++)
        n += s->e[i].active && s->e[i].state < ENEMY_DYING;
    return n;
}

int enemies_left(const Enemies *s)
{
    int n = 0;
    for (int i = 0; i < s->count; i++)
        n += s->e[i].state < ENEMY_DYING;
    return n;
}

void enemies_update(Enemies *s, const int target[3], int vulnerable, EnemyEvents *ev)
{
    ev->swiped_by = ev->grabbed_by = ev->bite = ev->spotted = ev->attacking = ev->swing = ev->ally_hit = ev->thrown = -1;
    if (s->rest > 0)
        s->rest--;
    for (int i = 0; i < s->count; i++)
    {
        Enemy *e = &s->e[i];
        if (!e->active)
            continue;
        if (e->state == ENEMY_DEAD)
        {
            if (++e->dead_frames >= CORPSE_FRAMES)
                e->active = 0;   // the body goes
            continue;
        }
        if (e->cooldown > 0)
            e->cooldown--;
        if (e->falling)
        {
            // Dropping: gravity until the floor below, no steering meanwhile.
            e->fall_vy += GRAVITY;
            e->feet -= e->fall_vy;
            if (e->feet <= e->fall_to)
            {
                e->feet = e->fall_to;
                e->falling = 0;
            }
            e->pos[1] = e->feet - s->model->bbox_min[1];
        }

        // The one they go for: Leon, or Ashley (s->ally) when she is nearer.
        int dx = target[0] - e->pos[0], dz = target[2] - e->pos[2];
        int dist = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));   // f32 (64-bit: far apart overflows 32)
        const int *t = target;
        int on_ally = 0;
        if (s->ally)
        {
            int ax = s->ally[0] - e->pos[0], az = s->ally[2] - e->pos[2];
            int ad = (int)sqrt64((u64)((s64)ax * ax + (s64)az * az));
            if (ad < dist)
            {
                dx = ax;
                dz = az;
                dist = ad;
                t = s->ally;
                on_ally = 1;
            }
        }
        // Facing f = (sin yaw, cos yaw); increasing yaw turns f towards (cos yaw, -sin yaw).
        // cross(f, to) and dot(f, to) are dist * the sine/cosine of the angle to Leon.
        int fx = sinLerp(e->yaw), fz = cosLerp(e->yaw);
        s64 cross = ((s64)fx * dz - (s64)fz * dx) >> 12;
        s64 dot = ((s64)fx * dx + (s64)fz * dz) >> 12;
        // Leon within reach, in front (within ~40 degrees) and on the same level (not up on a
        // walkway while this one is below, or falling).
        int level = t[1] - e->pos[1];
        int same_level = !e->falling && level < inttof32(8) && level > -inttof32(8);
        int reach = e->boss ? BOSS_REACH : e->weapon == WEAPON_FORK ? FORK_REACH : e->weapon ? WEAPON_REACH : REACH;
        int in_reach = dist < reach && dot * 4 > (s64)dist * 3 && same_level;

        if (e->thrower && e->state <= ENEMY_SWIPE)
        {
            // Throwers: face Leon from where they stand and throw now and then; the missile
            // leaves the hand halfway through the swing.
            if (dist > 0 && (cross < -(s64)dist / 28 || (dot < 0 && cross <= 0)))
                e->yaw += TURN_SPEED;
            else if (dist > 0 && (cross > (s64)dist / 28 || dot < 0))
                e->yaw -= TURN_SPEED;
            e->frame += ANIM_STEP;
            if (e->state == ENEMY_SWIPE)
            {
                int len = state_len(s, e);
                if (!e->struck && e->frame >= len / 2)
                {
                    e->struck = 1;
                    ev->thrown = i;
                }
                if (e->frame >= len)
                    set_state(s, i, ENEMY_IDLE);
            }
            else if (e->cooldown == 0 && dist < THROW_RANGE && dot > 0 && sees(s, e, t))   // (a blast can miss: thrown regardless)
            {
                set_state(s, i, ENEMY_SWIPE);
                e->cooldown = THROW_EVERY + rnd(s) % 90;
            }
        }
        else if (e->state == ENEMY_IDLE || e->state == ENEMY_WALK)
        {
            int chase = dist < s->sight;
            if (s->watch)
            {
                // Watching: they notice him close by, or in front of them (within ~60 degrees)
                // within sight and not far above or below; then they don't let him go.
                int dy = t[1] - e->pos[1];
                if (!e->spotted && (dist < NOTICE_NEAR || (dist < s->sight && dot * 2 > dist && dy < inttof32(30) && dy > -inttof32(30))))
                    enemies_alert(s, i);
                chase = e->spotted;
            }
            int want = chase && dist > STOP ? ENEMY_WALK : ENEMY_IDLE;
            if (want != e->state)
                set_state(s, i, want);
            if (want == ENEMY_WALK && !e->spotted)
                e->spotted = 1;
            if (e->spotted && !e->spot_told)
            {
                e->spot_told = 1;
                ev->spotted = i;
            }
            if (chase && dist > 0)
            {
                // Turn towards Leon while he's more than ~2 degrees off, or behind.
                if (cross < -(s64)dist / 28 || (dot < 0 && cross <= 0))
                    e->yaw += TURN_SPEED;
                else if (cross > (s64)dist / 28 || dot < 0)
                    e->yaw -= TURN_SPEED;
            }
            // Close, facing him (within ~15 degrees) and nobody else attacking: go for him,
            // with a lunge (grab attempt) or a swipe.
            if (vulnerable && same_level && s->attacker < 0 && s->rest == 0 && e->cooldown == 0 && dist < reach - (REACH - ATTACK_DIST)
                && dot > 0 && cross * 4 < dist && -cross * 4 < dist)
            {
                // Armed ones swing their weapon; bare hands swipe or lunge for a grab.
                set_state(s, i, !e->weapon && !e->boss && !on_ally && rnd(s) % 100 < LUNGE_PERCENT ? ENEMY_LUNGE : ENEMY_SWIPE);
                s->attacker = i;
                ev->attacking = i;
            }
            else
            {
                if (e->state == ENEMY_WALK)
                {
                    // Far off (out of the camera's way): every other frame, twice the step.
                    int sp = e->boss ? BOSS_SPEED : WALK_SPEED, far = dist > FAR_STEP;
                    if (!far || ((s->tick + i) & 1))
                        move(s, e, mulf32(fx, sp << far), mulf32(fz, sp << far));
                }
                e->frame += ANIM_STEP;
            }
        }
        else if (e->state == ENEMY_SWIPE || e->state == ENEMY_LUNGE)
        {
            e->frame += ANIM_STEP;
            if (e->state == ENEMY_SWIPE)
            {
                // One check while the arms (or the weapon) are out: hit if he's still there.
                // Weapon swings come down in the middle of their clip.
                int len = state_len(s, e);
                int armed = e->weapon || e->boss;
                int from = armed ? len * 2 / 5 : SWIPE_FROM, to = armed ? len * 3 / 5 : SWIPE_TO;
                if (!e->struck && e->frame >= from && e->frame < to)
                {
                    e->struck = 1;
                    ev->swing = i;
                    if (in_reach && on_ally)
                        ev->ally_hit = i;
                    else if (vulnerable && in_reach)
                        ev->swiped_by = i;
                }
            }
            else if (e->frame >= LUNGE_FROM && e->frame < LUNGE_TO && vulnerable && in_reach && !on_ally)
            {
                set_state(s, i, ENEMY_GRAB);
                ev->grabbed_by = i;
            }
            if ((e->state == ENEMY_SWIPE || e->state == ENEMY_LUNGE) && e->frame >= state_len(s, e))
                set_state(s, i, ENEMY_IDLE);
        }
        else if (e->state == ENEMY_GRAB)
        {
            // Holds on until released; bites once every second loop of the hold.
            e->frame += ANIM_STEP;
            if (e->frame >= anim_len(s, ANIM_HOLD))
            {
                e->frame -= anim_len(s, ANIM_HOLD);
                if (++e->struck % 2 == 0)
                    ev->bite = i;
            }
        }
        else
        {
            // Hit reaction / going down play once. Dazed (after a head shot), it stays bent
            // over near the end of the reaction until the daze wears off.
            e->frame += ANIM_STEP;
            if (e->daze > 0)
            {
                e->daze--;
                if (e->frame > anim_len(s, ANIM_HIT) * 2 / 3)
                    e->frame = anim_len(s, ANIM_HIT) * 2 / 3;
            }
            else if (e->frame >= anim_len(s, state_anim[e->state]))
                set_state(s, i, e->state == ENEMY_HIT ? ENEMY_IDLE : ENEMY_DEAD);
        }

        // Keep the living apart.
        for (int j = 0; j < s->count; j++)
        {
            Enemy *o = &s->e[j];
            if (j == i || !o->active || o->state >= ENEMY_DYING || e->state >= ENEMY_DYING || e->state == ENEMY_GRAB || e->thrower)
                continue;
            int ox = e->pos[0] - o->pos[0], oz = e->pos[2] - o->pos[2];
            int d = (int)sqrt64((u64)((s64)ox * ox + (s64)oz * oz));
            if (d > 0 && d < 2 * RADIUS)
            {
                int push = (2 * RADIUS - d) / 2;
                move(s, e, mulf32(divf32(ox, d), push), mulf32(divf32(oz, d), push));
            }
        }

        if (e->pose_dirty || (e->state != ENEMY_DEAD && (s->tick + i) % 3 == 0))
            update_pose(s, e);
    }
    s->tick++;
}

void enemies_hold_at(Enemies *s, int i, const int pos[3], int dist)
{
    Enemy *e = &s->e[i];
    e->pos[0] = pos[0] - mulf32(sinLerp(e->yaw), dist);
    e->pos[2] = pos[2] - mulf32(cosLerp(e->yaw), dist);
    ground_collide_n(s->ground, &e->pos[0], &e->pos[2], e->feet + STEP_UP, e->feet + HEIGHT, WALL_RADIUS, 2);   // (Leon's back to a wall)
    ground_height(s->ground, e->pos[0], e->pos[2], e->feet, STEP_UP, &e->feet);
    e->pos[1] = e->feet - s->model->bbox_min[1];
}

void enemies_release(Enemies *s, int i)
{
    Enemy *e = &s->e[i];
    if (e->state != ENEMY_GRAB)
        return;
    set_state(s, i, ENEMY_HIT);
    move(s, e, -mulf32(sinLerp(e->yaw), RELEASE_PUSH), -mulf32(cosLerp(e->yaw), RELEASE_PUSH));
}

int enemies_push(const Enemies *s, int *x, int *z, int radius)
{
    int pushed = 0;
    for (int i = 0; i < s->count; i++)
    {
        const Enemy *e = &s->e[i];
        if (!e->active || e->state >= ENEMY_DYING)
            continue;
        int ox = *x - e->pos[0], oz = *z - e->pos[2], min = radius + RADIUS;
        int d = (int)sqrt64((u64)((s64)ox * ox + (s64)oz * oz));
        if (d > 0 && d < min)
        {
            *x = e->pos[0] + mulf32(divf32(ox, d), min);
            *z = e->pos[2] + mulf32(divf32(oz, d), min);
            pushed++;
        }
    }
    return pushed;
}

// Nearest first (the DS keeps 2048 polygons a frame and drops what comes after: those in the
// haze go, not those in Leon's face), at most max of them.
void enemies_draw(Enemies *s, u32 poly_fmt, const int eye[3], int max)
{
    int order[ENEMY_MAX], dist[ENEMY_MAX], n = 0;
    for (int i = 0; i < s->count; i++)
    {
        Enemy *e = &s->e[i];
        if (!e->active || !view_visible(e->pos, inttof32(10)) || (s->hide_below && e->feet < s->hide_below))
            continue;   // (out of view: not sent to the geometry engine)
        int dx = (e->pos[0] - eye[0]) >> 4, dz = (e->pos[2] - eye[2]) >> 4, d = dx * dx + dz * dz;
        int k = n++;
        while (k > 0 && dist[k - 1] > d)
        {
            order[k] = order[k - 1];
            dist[k] = dist[k - 1];
            k--;
        }
        order[k] = i;
        dist[k] = d;
    }
    for (int j = 0; j < n && j < max; j++)
    {
        Enemy *e = &s->e[order[j]];
        if (e->pose_dirty)
            update_pose(s, e);
        model_draw(e->boss ? s->boss_model : s->model, e->pos, e->yaw, poly_fmt, (const int (*)[12])e->pose);
        if (!e->boss && e->weapon && s->weapon_model[e->weapon])
            model_draw_held(s->weapon_model[e->weapon], e->pos, e->yaw, poly_fmt, e->pose[HAND_NODE]);
    }
}

int enemies_raycast(const Enemies *s, const int p[3], const int d[3], int *t_best, int *head)
{
    int found = -1;
    // Horizontal part of the ray, for the circle test. Everything below is scaled down by
    // 4096 once so the squares stay inside 64 bits at any distance on the stage.
    s64 aa = ((s64)d[0] * d[0] + (s64)d[2] * d[2]) >> 12;   // |d.xz|^2
    if (aa == 0)
        return -1;   // straight up/down
    for (int i = 0; i < s->count; i++)
    {
        const Enemy *e = &s->e[i];
        if (!e->active || e->state >= ENEMY_DYING)
            continue;
        // Solve |(p - c).xz + d.xz * t| = RADIUS for the entry point t.
        s64 fx = p[0] - e->pos[0], fz = p[2] - e->pos[2];
        s64 b = (fx * d[0] + fz * d[2]) >> 12;           // f32^2 / 4096
        s64 c = ((fx * fx + fz * fz) >> 12) - (((s64)RADIUS * RADIUS) >> 12);
        s64 disc = b * b - aa * c;
        if (disc < 0)
            continue;
        s64 root = (s64)sqrt64(disc);
        s64 t = ((-b - root) << 12) / aa;   // f32
        if (t <= 0 && ((-b + root) << 12) / aa > 0)
            t = 1;   // the muzzle is inside the body (point blank): hit right there
        if (t <= 0 || t >= *t_best)
            continue;
        s64 y = p[1] + ((d[1] * t) >> 12) - e->feet;
        if (y < 0 || y > HEIGHT)
            continue;
        *t_best = (int)t;
        *head = y > HEAD_FROM;
        found = i;
    }
    return found;
}

void enemies_damage(Enemies *s, int i, int damage)
{
    Enemy *e = &s->e[i];
    if (!e->active || e->state >= ENEMY_DYING)
        return;
    e->hp -= damage;
    set_state(s, i, e->hp <= 0 ? ENEMY_DYING : ENEMY_HIT);
    if (s->watch)
        enemies_alert(s, i);   // shot: he knows where Leon is, and so do those around him
}

void enemies_alert(Enemies *s, int i)
{
    if (s->e[i].spotted)
        return;
    s->e[i].spotted = 1;
    for (int j = 0; j < s->count; j++)
    {
        Enemy *o = &s->e[j];
        int dx = o->pos[0] - s->e[i].pos[0], dz = o->pos[2] - s->e[i].pos[2];
        if (j != i && o->active && !o->spotted && (s64)dx * dx + (s64)dz * dz < (s64)ALERT_SPREAD * ALERT_SPREAD)
            enemies_alert(s, j);
    }
}

int enemies_alive(const Enemies *s)
{
    int n = 0;
    for (int i = 0; i < s->count; i++)
        n += s->e[i].active && s->e[i].state < ENEMY_DYING;
    return n;
}

void enemies_daze(Enemies *s, int i)
{
    Enemy *e = &s->e[i];
    if (e->active && e->state == ENEMY_HIT)
        e->daze = DAZE_FRAMES;
}

// Distance from (pos, yaw) to enemy e if it is in front (within ~45 degrees), else -1.
static int in_front(const Enemy *e, const int pos[3], int yaw)
{
    int dx = e->pos[0] - pos[0], dz = e->pos[2] - pos[2];
    int d = (int)sqrt64((u64)((s64)dx * dx + (s64)dz * dz));
    s64 dot = ((s64)sinLerp(yaw) * dx + (s64)cosLerp(yaw) * dz) >> 12;
    return d > 0 && dot * 10 > (s64)d * 7 ? d : -1;
}

int enemies_kickable(const Enemies *s, const int pos[3], int yaw)
{
    for (int i = 0; i < s->count; i++)
    {
        const Enemy *e = &s->e[i];
        int d;
        if (e->active && e->state == ENEMY_HIT && e->daze > 0 && (d = in_front(e, pos, yaw)) >= 0 && d < KICK_RANGE)
            return i;
    }
    return -1;
}

int enemies_kick(Enemies *s, const int pos[3], int yaw, int damage)
{
    int n = 0;
    for (int i = 0; i < s->count; i++)
    {
        Enemy *e = &s->e[i];
        int d;
        if (!e->active || e->state >= ENEMY_DYING || (d = in_front(e, pos, yaw)) < 0 || d >= KICK_RANGE + RADIUS)
            continue;
        enemies_damage(s, i, damage);
        int dx = e->pos[0] - pos[0], dz = e->pos[2] - pos[2];
        move(s, e, mulf32(divf32(dx, d), KICK_PUSH), mulf32(divf32(dz, d), KICK_PUSH));
        n++;
    }
    return n;
}
int enemies_slash(Enemies *s, const int pos[3], int yaw, int range, int damage)
{
    int best = -1, best_d = range + RADIUS;
    for (int i = 0; i < s->count; i++)
    {
        const Enemy *e = &s->e[i];
        int d;
        if (e->active && e->state < ENEMY_DYING && (d = in_front(e, pos, yaw)) >= 0 && d < best_d)
        {
            best = i;
            best_d = d;
        }
    }
    if (best >= 0)
        enemies_damage(s, best, damage);
    return best;
}