{
    static BotGoal bg;
    static int b_frames, b_replan, b_k, b_path_ok, b_stuck, b_stuck_n, b_last[3], b_deaths, b_at_t, b_press_t;
    static u32 b_hash_prev;
    static char b_msg[48];
    static int b_dead_seen, b_goal_age, b_noA, b_mode_prev, b_aim_t, b_unstick;
    held = 0;
    b_frames++;
    for (int b = 0; b < BN_BLOCKS; b++)
        if (bn_block_t[b] > 0)
            bn_block_t[b]--;
    if (mode == MODE_PLAY && b_mode_prev != MODE_PLAY)
        b_noA = 45;
    b_mode_prev = mode;
    for (int k = 0; k < 8; k++)
        for (int i = 0; i < 64; i++)
            if (bot_skip_t[k][i])
                bot_skip_t[k][i]--;
    if (mode == MODE_CUTIN || mode == MODE_MESSAGE || mode == MODE_RESULT)
    {
        if (b_frames % 24 < 2)
            held = KEY_A;
    }
    else if (mode == MODE_GAMEOVER)
    {
        if (!b_dead_seen)
            b_deaths++;
        b_dead_seen = 1;
        if (b_frames % 40 < 2)
            held = KEY_A;
    }
    else if (mode == MODE_PLAY && ctrl == CTRL_LEON && clear_t < 0)
    {
        b_dead_seen = 0;
        const StageFoes *sf = &stage_foes[cur_stage];
        int at[3] = { leon_pos[0], leon_feet, leon_pos[2] };
        int eye[3] = { leon_pos[0], leon_feet + inttof32(13), leon_pos[2] };
        u32 hash = mis_flags * 31u + foe_groups * 7u + ganados.kills;
        for (int i = 0; i < 8; i++)
            hash = hash * 131u + ev_played[i] + items_held[i] * 3u;
        if (hash != b_hash_prev)
        {
            memset(bot_done, 0, sizeof(bot_done));
            memset(bn_door_bad, 0, sizeof(bn_door_bad));
            for (int i = 0; i < sf->events && i < 64; i++)
                if (ev_bit(ev_played, i))
                    bot_done[i] = 1;
            b_hash_prev = hash;
        }
        int gal_behind = 0, gal_alive = 0;
        for (int i = 0; i < gal_count; i++)
            gal_alive |= gals[i].on && gals[i].state != GAL_DEAD;
        int t_kind = 0, t_idx = -1, t_pos[3] = { 0, 0, 0 }, t_dist = inttof32(70), t_chest = 0;
        for (int i = 0; i < ganados.count; i++)
        {
            const Enemy *e = &ganados.e[i];
            if (!e->active || e->state >= ENEMY_DYING || e->falling)
                continue;
            int p[3] = { e->pos[0], e->feet + inttof32(10), e->pos[2] };
            int d = bot_dist(at, p), dy = e->feet - leon_feet;
            if ((e->thrower && d > inttof32(50)) || dy > inttof32(40) || dy < -inttof32(40) || d >= t_dist || bot_skip_t[7][i] || !bot_sees(eye, p))
                continue;
            t_kind = 1; t_idx = i; t_dist = d; t_chest = p[1];
            for (int k = 0; k < 3; k++) t_pos[k] = p[k];
        }
        for (int i = 0; i < giant_count; i++)
        {
            const Giant *gi = &giants[i];
            if (gi->state == GIANT_DEAD || gi->state == GIANT_KNEEL || gi->state == GIANT_RISE)
                continue;
            int p[3] = { gi->pos[0], gi->feet + inttof32(28), gi->pos[2] };
            int d = bot_dist(at, p);
            if (d >= t_dist + inttof32(30) || !bot_sees(eye, p))
                continue;
            t_kind = 2; t_idx = i; t_dist = d; t_chest = p[1];
            for (int k = 0; k < 3; k++) t_pos[k] = p[k];
        }
        for (int i = 0; i < gal_count; i++)
        {
            const Garrador *ga = &gals[i];
            if (!ga->on || ga->state == GAL_DEAD || BOT_LEAVE_GAL)
                continue;
            int p[3] = { ga->pos[0], ga->feet + inttof32(10), ga->pos[2] };
            int d = bot_dist(at, p);
            if (d >= t_dist || !bot_sees(eye, p))
                continue;
            t_kind = 3; t_idx = i; t_dist = d; t_chest = ga->feet + inttof32(14);
            gal_behind = (((s64)sinLerp(ga->yaw) * (leon_pos[0] - ga->pos[0]) + (s64)cosLerp(ga->yaw) * (leon_pos[2] - ga->pos[2])) >> 12) < -(d / 3);
            for (int k = 0; k < 3; k++) t_pos[k] = p[k];
        }
        if (ver.on && ver.state != VER_DEAD)
        {
            int p[3] = { ver.pos[0], ver.feet + inttof32(10), ver.pos[2] };
            int d = bot_dist(at, p);
            if (d < t_dist && bot_sees(eye, p))
            {
                t_kind = 4; t_idx = 0; t_dist = d; t_chest = p[1];
                for (int k = 0; k < 3; k++) t_pos[k] = p[k];
            }
        }
        Giant *gk = giant_at_hand(leon_pos);
        int kickable = enemies_kickable(&ganados, leon_pos, leon_yaw);
        static int b_flee_t, b_flee_to[3];
        int thr[3] = { 0, 0, 0 }, threat = 0;
        for (int i = 0; i < giant_count; i++)
            if ((giants[i].state != GIANT_DEAD && giants[i].state != GIANT_KNEEL && giants[i].state != GIANT_RISE)
                && bot_dist(at, giants[i].pos) < inttof32(30))
            { threat = 1; for (int k = 0; k < 3; k++) thr[k] = giants[i].pos[k]; }
        for (int i = 0; i < ganados.count; i++)
            if (ganados.e[i].boss && ganados.e[i].active && ganados.e[i].state < ENEMY_HIT && bot_dist(at, ganados.e[i].pos) < inttof32(16))
            { threat = 1; for (int k = 0; k < 3; k++) thr[k] = ganados.e[i].pos[k]; }
        for (int i = 0; i < gal_count; i++)
            if (gals[i].on && gals[i].state != GAL_DEAD && bot_dist(at, gals[i].pos) < inttof32(14))
            { threat = 1; for (int k = 0; k < 3; k++) thr[k] = gals[i].pos[k]; }
        if (ver.on && ver.state != VER_DEAD && bot_dist(at, ver.pos) < inttof32(24))
        { threat = 1; for (int k = 0; k < 3; k++) thr[k] = ver.pos[k]; }
        static int b_flee_far, b_flee_from[3];
        if (threat)
        {
            b_flee_far = 0;
            for (int i = 0; i < giant_count; i++)
                if (giants[i].state != GIANT_DEAD && bot_dist(at, giants[i].pos) < inttof32(30)) b_flee_far = 1;
            for (int k = 0; k < 3; k++) b_flee_from[k] = thr[k];
        }
        if (!threat && b_flee_t > 0 && bot_dist(at, b_flee_from) < (b_flee_far ? inttof32(55) : inttof32(40)))
        {
            threat = 1;
            for (int k = 0; k < 3; k++) thr[k] = b_flee_from[k];
        }
        if (threat && react == REACT_NONE && !gk && (b_flee_t <= 0 || b_flee_t % 20 == 0))
        {
            bn_flee = thr;
            bn_plan(cur_stage, at, at, 0, 0);
            bn_flee = NULL;
            if (b_flee_far && bn_n_last > 50)
            {
                s64 cx = 0, cz = 0;
                for (int q = 0; q < bn_n_last; q++) { cx += bn_node[q].x; cz += bn_node[q].z; }
                int c[3] = { (int)(cx / bn_n_last), leon_feet, (int)(cz / bn_n_last) };
                int gx = c[0] - thr[0], gz = c[2] - thr[2], gl = (int)sqrt64((u64)((s64)gx * gx + (s64)gz * gz));
                if (gl > 0)
                {
                    int tgt[3] = { c[0] + (int)((s64)gx * inttof32(35) / gl), leon_feet, c[2] + (int)((s64)gz * inttof32(35) / gl) };
                    bn_plan(cur_stage, at, tgt, inttof32(8), inttof32(10));
                }
            }
            int k = bn_path_n > 14 ? 14 : bn_path_n - 1;
            if (k > 0)
            {
                b_flee_to[0] = bn_node[bn_path[k]].x;
                b_flee_to[1] = bn_node[bn_path[k]].feet;
                b_flee_to[2] = bn_node[bn_path[k]].z;
                if (b_flee_t <= 0)
                    b_flee_t = 50;
            }
        }
        if (react == REACT_HELD)
            held = (b_frames & 1) ? KEY_A : KEY_B;
        else if (react != REACT_NONE)
            held = 0;
        else if (gk)
        {
            held = (b_frames % 8 < 2) ? KEY_A : 0;
            snprintf(b_msg, sizeof(b_msg), "cut");
        }
        else if (kickable >= 0 && !aim_w)
        {
            held = (b_frames % 8 < 2) ? KEY_A : 0;
            snprintf(b_msg, sizeof(b_msg), "kick");
        }
        else if (b_flee_t > 0)
        {
            b_flee_t--;
            if (gun_state == GUN_READY && ammo < MAG_SIZE / 2 && spare > 0 && b_frames % 30 == 0)
                held |= KEY_Y;
            int want = bot_yaw_to(at, b_flee_to), diff = bot_angle_diff(want, leon_yaw);
            held = (held & KEY_Y) | (diff > degreesToAngle(12) ? KEY_LEFT : diff < -degreesToAngle(12) ? KEY_RIGHT : 0)
                   | (abs(diff) < degreesToAngle(60) ? KEY_UP | (gal_alive ? 0 : KEY_B) : 0);
            if (bot_dist(at, b_flee_to) < inttof32(2))
                b_flee_t = 0;
            snprintf(b_msg, sizeof(b_msg), "flee d%d", f32toint(bot_dist(at, thr)));
        }
        else if (t_idx >= 0)
        {
            int want = bot_yaw_to(at, t_pos), diff = bot_angle_diff(want, leon_yaw);
            int no_rounds = ammo == 0 && spare == 0;
            snprintf(b_msg, sizeof(b_msg), "fight %d:%d d%d", t_kind, t_idx, f32toint(t_dist));
            if (no_rounds)
            {
                if (t_dist > inttof32(7))
                    held = (diff > degreesToAngle(10) ? KEY_LEFT : diff < -degreesToAngle(10) ? KEY_RIGHT : 0) | (abs(diff) < degreesToAngle(40) ? KEY_UP : 0);
                else
                    held = KEY_L | (diff > degreesToAngle(15) ? KEY_LEFT : diff < -degreesToAngle(15) ? KEY_RIGHT : 0) | ((b_frames % 12 < 2) ? KEY_A : 0);
            }
            else if (t_kind == 3 && !gal_behind && t_dist > inttof32(12))
            {
                const Garrador *ga = &gals[t_idx];
                int bp[3] = { ga->pos[0] - mulf32(sinLerp(ga->yaw), inttof32(18)), ga->feet, ga->pos[2] - mulf32(cosLerp(ga->yaw), inttof32(18)) };
                int w2 = bot_yaw_to(at, bp), d2 = bot_angle_diff(w2, leon_yaw);
                held = (d2 > degreesToAngle(12) ? KEY_LEFT : d2 < -degreesToAngle(12) ? KEY_RIGHT : 0) | (abs(d2) < degreesToAngle(45) ? KEY_UP : 0);
                snprintf(b_msg, sizeof(b_msg), "gal-round d%d", f32toint(bot_dist(at, bp)));
            }
            else if (gun_state == GUN_HOLSTERED)
                held = ((b_frames % 20 < 2) ? KEY_X : 0) | (diff > 0 ? KEY_LEFT : KEY_RIGHT);
            else if (abs(diff) > degreesToAngle(10))
                held = diff > 0 ? KEY_LEFT : KEY_RIGHT;
            else
            {
                held = KEY_R;
                if (diff > degreesToAngle(1)) held |= KEY_LEFT;
                else if (diff < -degreesToAngle(1)) held |= KEY_RIGHT;
                float pitch = atan2f((float)(t_chest - (leon_feet + inttof32(13))), (float)t_dist) * 57.3f;
                int want_tilt = (int)(pitch / 50.0f * 4096.0f);
                if (want_tilt > AIM_TILT_RANGE) want_tilt = AIM_TILT_RANGE;
                if (want_tilt < -AIM_TILT_RANGE) want_tilt = -AIM_TILT_RANGE;
                if (aim_tilt < want_tilt - inttof32(1) / 16) held |= KEY_UP;
                else if (aim_tilt > want_tilt + inttof32(1) / 16) held |= KEY_DOWN;
                int on = (t_kind == 1 && laser_enemy == t_idx) || (t_kind == 2 && laser_giant == t_idx + 1)
                         || (t_kind == 3 && laser_gal == t_idx + 1) || (t_kind == 4 && laser_ver);
                if (ammo == 0 && spare > 0)
                    held |= (b_frames % 30 < 2) ? KEY_Y : 0;
                else if (on && aim_w >= inttof32(1) && (b_frames & 3) == 0 && (t_kind != 3 || gal_behind))
                {
                    held |= KEY_A;
                    b_aim_t = 0;
                }
                if (t_kind == 1 && aim_w >= inttof32(1) && ++b_aim_t > 180)
                {
                    bot_skip_t[7][t_idx] = 300;
                    b_aim_t = 0;
                }
            }
        }
        else
        {
            int kneel = -1;
            for (int i = 0; i < giant_count; i++)
                if (giants[i].state == GIANT_KNEEL)
                    kneel = i;
            if (bg.kind == BG_NONE || b_frames % 300 == 0 || kneel >= 0)
            {
                BotGoal best = { BG_NONE, 0, { 0, 0, 0 }, 0, 0, 0 };
                int best_cost = 0x7fffffff;
                #define BOT_CAND(K, I, X, Y, Z, R, DY, P, PEN) do { \
                    if (!bot_skip_t[K][(I) & 63]) { int p_[3] = { X, Y, Z }; int c_ = f32toint(bot_dist(at, p_)) + (PEN) - ((K) == bg.kind && (I) == bg.idx ? 60 : 0); \
                        if (c_ < best_cost) { best_cost = c_; best = (BotGoal){ K, I, { X, Y, Z }, R, DY, P }; } } } while (0)
                if (kneel >= 0)
                    BOT_CAND(BG_GIANT, kneel, giants[kneel].pos[0], giants[kneel].feet, giants[kneel].pos[2], inttof32(20), inttof32(20), 0, -1000);
                for (int i = 0; i < DROPS_MAX; i++)
                {
                    const Drop *d = &drops.d[i];
                    if (d->active && !d->delay && !d->aloft && bot_dist(at, d->pos) < inttof32(50))
                        BOT_CAND(BG_DROP, i, d->pos[0], d->pos[1], d->pos[2], inttof32(2), inttof32(10), 1, 0);
                }
                if (ash.on && ash.wait)
                    BOT_CAND(BG_ASHLEY, 0, ash.pos[0], ash.feet, ash.pos[2], inttof32(20), inttof32(10), 0, 0);
                for (int i = 0; i < sf->events && i < 64; i++)
                {
                    const StageEvent *ev = &sf->event[i];
                    if (ev_bit(ev_deleted, i) || ev_bit(ev_played, i) || bot_done[i] || ev->get == GET_AUTO)
                        continue;
                    if (ev->get == GET_WITH_ASHLEY && (!ash.on || ash.wait))
                        continue;
                    if (ev->cond != COND_DESTROYED && ev->cond != COND_WALL && !ev_cond(sf, ev, at))
                        continue;
                    int r = ev->reach * 16 * 4 / 5, press = ev->get != GET_TOUCH;
                    if (r < inttof32(3)) r = inttof32(3);
                    int matters = ev->count > 0;
                    for (int j = 0; j < sf->events && !matters; j++)
                        matters = sf->event[j].cond == COND_EVENT && sf->event[j].value == i;
                    int pen = matters ? 0 : 250;
                    if (ev->ctg == EV_POINT && ev->trg >= 0 && ev->trg < sf->points)
                    {
                        int py = sf->point[ev->trg][1] - 12;
                        if (cur_stage == ASH_STAGE_E && ev->trg == 4)
                            py = sf->point[ev->trg][1] + 68;
                        BOT_CAND(BG_EVENT, i, inttof32(sf->point[ev->trg][0]), inttof32(py), inttof32(sf->point[ev->trg][2]), r, inttof32(10), press, pen);
                    }
                    else if (ev->ctg == EV_RECORD && foe_record_down(ev->trg))
                    {
                        for (int f = 0; f < foe_n; f++)
                            if (foe_list[f]->rec == ev->trg)
                                BOT_CAND(BG_EVENT, i, inttof32(foe_at[f][0]), inttof32(foe_at[f][1]), inttof32(foe_at[f][2]), r, inttof32(12), press, pen);
                    }
                    else if (ev->ctg == EV_OBJECT && ev->trg >= 0 && ev->trg < (int)sizeof(thing_obj) && thing_obj[ev->trg] >= 0)
                    {
                        const Obj *o = &objs.o[thing_obj[ev->trg]];
                        if (o->alive)
                            BOT_CAND(BG_CRATE, thing_obj[ev->trg], o->pos[0], o->pos[1], o->pos[2], inttof32(8), inttof32(8), 1, 0);
                        else
                            BOT_CAND(BG_EVENT, i, o->pos[0], o->pos[1], o->pos[2], r, inttof32(10), press, pen);
                    }
                }
                for (int i = 0; i < objs.count; i++)
                    if (objs.o[i].kind == OBJ_CRATE && objs.o[i].alive && bot_dist(at, objs.o[i].pos) < inttof32(40))
                        BOT_CAND(BG_CRATE, i, objs.o[i].pos[0], objs.o[i].pos[1], objs.o[i].pos[2], inttof32(8), inttof32(8), 1, 60);
                for (int i = 0; i < ganados.count; i++)
                    if (ganados.e[i].active && ganados.e[i].state < ENEMY_DYING && !ganados.e[i].thrower)
                    {
                        int named = 0, f = slot_foe[i];
                        for (int e = 0; f >= 0 && e < sf->events && !named; e++)
                            named = (sf->event[e].ctg == EV_RECORD && sf->event[e].trg == foe_list[f]->rec)
                                    || (sf->event[e].cond == COND_RECORD_DOWN && sf->event[e].value == foe_list[f]->rec);
                        BOT_CAND(BG_HUNT, i, ganados.e[i].pos[0], ganados.e[i].feet, ganados.e[i].pos[2], inttof32(12), inttof32(6), 0, named ? 20 : 200);
                    }
                for (int i = 0; i < gal_count && !BOT_LEAVE_GAL; i++)
                    if (gals[i].on && gals[i].state != GAL_DEAD)
                        BOT_CAND(BG_HUNT, 16 + i, gals[i].pos[0], gals[i].feet, gals[i].pos[2], inttof32(30), inttof32(20), 0, 220);
                for (int i = 0; i < giant_count; i++)
                    if (giants[i].state != GIANT_DEAD)
                        BOT_CAND(BG_HUNT, 20 + i, giants[i].pos[0], giants[i].feet, giants[i].pos[2], inttof32(40), inttof32(20), 0, 220);
                #undef BOT_CAND
                if (best.kind != bg.kind || best.idx != bg.idx)
                {
                    bg = best;
                    b_replan = 0;
                    b_at_t = 0;
                    b_goal_age = 0;
                    b_stuck_n = 0;
                }
            }
            if (bg.kind != BG_NONE && ++b_goal_age > 40 * 60)
            {
                bot_skip_t[bg.kind][bg.idx & 63] = 3600;
                bg.kind = BG_NONE;
                b_goal_age = 0;
            }
            if (b_unstick > 0)
            {
                b_unstick--;
                held = KEY_DOWN | ((b_frames / 25) & 1 ? KEY_LEFT : 0);
                if (b_unstick == 0)
                    b_replan = 0;
                snprintf(b_msg, sizeof(b_msg), "unstick");
            }
            else if (drops_in_reach(&drops, at) >= 0)
            {
                held = (b_frames % 16 < 2) ? KEY_A : 0;
                snprintf(b_msg, sizeof(b_msg), "pick");
            }
            else if (bg.kind == BG_NONE)
            {
                held = 0;
                snprintf(b_msg, sizeof(b_msg), "idle");
            }
            else
            {
                if (--b_replan <= 0)
                {
                    b_path_ok = bn_plan(cur_stage, at, bg.pos, bg.reach, bg.dyok);
                    b_k = 1;
                    b_replan = 45;
                    if (b_path_ok == 0 && !bn_truncated)
                    {
                        bot_skip_t[bg.kind][bg.idx & 63] = 6000;
                        bg.kind = BG_NONE;
                    }
                    if (b_path_ok < 0)
                        b_unstick = 50;
                    else if (b_path_ok <= 0 && bn_path_n <= 2)
                    {
                        bot_skip_t[bg.kind][bg.idx & 63] = 1800;
                        bg.kind = BG_NONE;
                    }
                }
                int gd = bot_dist(at, bg.pos), gdy = leon_feet - bg.pos[1];
                int arrived = gd <= bg.reach + inttof32(bg.kind == BG_EVENT ? 3 : 1) && gdy <= bg.dyok && gdy >= -bg.dyok;
                snprintf(b_msg, sizeof(b_msg), "%c%d d%d p%d/%d", "-DAECHG"[bg.kind], bg.idx, f32toint(gd), b_path_ok, bn_path_n);
                if (bg.kind != BG_NONE && arrived)
                {
                    b_at_t++;
                    if (bg.kind == BG_CRATE)
                    {
                        int want = bot_yaw_to(at, bg.pos), diff = bot_angle_diff(want, leon_yaw);
                        int w, act = objects_action(&objs, leon_pos, leon_feet, leon_yaw, &w);
                        if (act == ACT_KICK_CRATE)
                            held = (b_frames % 10 < 2) ? KEY_A : 0;
                        else
                            held = (diff > 0 ? KEY_LEFT : KEY_RIGHT) | (abs(diff) < degreesToAngle(20) && gd > inttof32(5) ? KEY_UP : 0);
                    }
                    else if (bg.kind == BG_EVENT && sf->event[bg.idx].get == GET_WITH_ASHLEY && !ev_ashley_by(at))
                    {
                        held = 0;
                        b_at_t = b_at_t > 60 ? 60 : b_at_t;
                        snprintf(b_msg, sizeof(b_msg), "wait ash");
                    }
                    else if (bg.kind == BG_EVENT && bg.press && b_at_t > 30 && b_at_t < 200 && gd > inttof32(1))
                    {
                        int want = bot_yaw_to(at, bg.pos), diff = bot_angle_diff(want, leon_yaw);
                        held = (diff > degreesToAngle(10) ? KEY_LEFT : diff < -degreesToAngle(10) ? KEY_RIGHT : 0)
                               | (abs(diff) < degreesToAngle(30) && (b_frames & 3) == 0 ? KEY_UP : 0) | ((b_frames % 20 < 2) ? KEY_A : 0);
                    }
                    else if (bg.press && (b_frames % 20 < 2))
                    {
                        held = KEY_A;
                        if (bg.kind == BG_EVENT && b_at_t > 240)
                            bot_done[bg.idx] = 1;
                    }
                    if (bg.kind == BG_EVENT && !bg.press && b_at_t > 90)
                        bot_done[bg.idx] = 1;
                    if (b_at_t > 300)
                    {
                        bot_skip_t[bg.kind][bg.idx & 63] = 1200;
                        if (bg.kind == BG_EVENT)
                            bot_done[bg.idx] = 1;
                        bg.kind = BG_NONE;
                    }
                }
                else if (bg.kind != BG_NONE && bn_path_n > 1)
                {
                    static int b_best_gd, b_prog_t, b_noprog, b_prog_goal = -1;
                    int goal_id = bg.kind * 64 + bg.idx;
                    if (goal_id != b_prog_goal)
                    {
                        b_prog_goal = goal_id;
                        b_best_gd = 0x7fffffff;
                        b_prog_t = b_noprog = 0;
                    }
                    if (gd + abs(gdy) < b_best_gd - inttof32(2))
                    {
                        b_best_gd = gd + abs(gdy);
                        b_prog_t = 0;
                    }
                    else if (++b_prog_t > 480)
                    {
                        const BnNode *nb = &bn_node[bn_path[b_k < bn_path_n ? b_k : bn_path_n - 1]];
                        for (int b = 0; b < BN_BLOCKS; b++)
                            if (bn_block_t[b] <= 0)
                            {
                                bn_block[b][0] = nb->x;
                                bn_block[b][1] = leon_feet;
                                bn_block[b][2] = nb->z;
                                bn_block_t[b] = 3600;
                                break;
                            }
                        b_prog_t = 0;
                        b_best_gd = 0x7fffffff;
                        b_replan = 0;
                        if (++b_noprog > 5)
                        {
                            bot_skip_t[bg.kind][bg.idx & 63] = 2400;
                            bg.kind = BG_NONE;
                        }
                    }
                    int k = b_k < bn_path_n ? b_k : bn_path_n - 1;
                    for (int j = k; j < bn_path_n && j < k + 8; j++)
                    {
                        const BnNode *nd = &bn_node[bn_path[j]];
                        int p[3] = { nd->x, nd->feet, nd->z };
                        if (bot_dist(at, p) < inttof32(2) && (nd->kind == BN_WALK || j == 0))
                            k = j + 1;
                    }
                    if (k >= bn_path_n)
                        k = bn_path_n - 1;
                    b_k = k;
                    const BnNode *next = &bn_node[bn_path[k]];
                    if (next->kind != BN_WALK)
                    {
                        const BnNode *from_nd = &bn_node[bn_path[k - 1 >= 0 ? k - 1 : 0]];
                        int fp[3] = { from_nd->x, from_nd->feet, from_nd->z };
                        int want;
                        if (next->kind == BN_JUMP)
                            want = degreesToAngle(jumps[next->arg].yaw);
                        else
                        {
                            const Obj *o = &objs.o[next->arg];
                            int dir[3] = { 0, 0, 0 }, sg = next->kind == BN_LADDER_UP ? 1 : -1;
                            int p2[3] = { leon_pos[0] + sg * o->dir[0], 0, leon_pos[2] + sg * o->dir[1] };
                            (void)dir;
                            want = bot_yaw_to(at, p2);
                        }
                        int diff = bot_angle_diff(want, leon_yaw);
                        if (bot_dist(at, fp) > inttof32(2))
                        {
                            int wy = bot_yaw_to(at, fp), wd = bot_angle_diff(wy, leon_yaw);
                            held = (wd > degreesToAngle(12) ? KEY_LEFT : wd < -degreesToAngle(12) ? KEY_RIGHT : 0) | (abs(wd) < degreesToAngle(50) ? KEY_UP : 0);
                        }
                        else if (abs(diff) > degreesToAngle(8))
                            held = diff > 0 ? KEY_LEFT : KEY_RIGHT;
                        else
                            held = (b_frames % 16 < 2) ? KEY_A : 0;
                    }
                    else
                    {
                        int look = k;
                        for (int j = k; j < bn_path_n && j < k + 3 && bn_node[bn_path[j]].kind == BN_WALK; j++)
                            look = j;
                        const BnNode *tn = &bn_node[bn_path[look]];
                        int tp[3] = { tn->x, tn->feet, tn->z };
                        int want = bot_yaw_to(at, tp), diff = bot_angle_diff(want, leon_yaw);
                        held = diff > degreesToAngle(12) ? KEY_LEFT : diff < -degreesToAngle(12) ? KEY_RIGHT : 0;
                        if (abs(diff) < degreesToAngle(50))
                            held |= KEY_UP | (bn_path_n - k > 15 && !gal_alive ? KEY_B : 0);
                        if (gun_state == GUN_READY && b_frames % 60 == 0)
                            held |= KEY_X;
                        {
                            int dw, da = objects_action(&objs, leon_pos, leon_feet, leon_yaw, &dw);
                            if (da == ACT_OPEN_DOOR && b_frames % 20 < 2)
                                held = (held & ~KEY_X) | KEY_A;
                        }
                    }
                    if (b_frames % 180 == 0)
                    {
                        if (bot_dist(at, b_last) < inttof32(3))
                        {
                            b_stuck++;
                            for (int q = 0; q < objs.count; q++)
                                if (objs.o[q].kind == OBJ_SWING && objs.o[q].locked && b_stuck >= 2
                                    && abs(objs.o[q].hinge[0] - leon_pos[0]) < inttof32(16) && abs(objs.o[q].hinge[1] - leon_pos[2]) < inttof32(16))
                                    bn_door_bad[q] = 1;
                            int w, act = objects_action(&objs, leon_pos, leon_feet, leon_yaw, &w);
                            if (act == ACT_OPEN_DOOR && b_stuck < 3)
                                held = KEY_A;
                            if (act != ACT_OPEN_DOOR || b_stuck >= 2)
                                for (int b = 0; b < BN_BLOCKS; b++)
                                    if (bn_block_t[b] <= 0)
                                    {
                                        bn_block[b][0] = leon_pos[0] + mulf32(sinLerp(leon_yaw), inttof32(3));
                                        bn_block[b][1] = leon_feet;
                                        bn_block[b][2] = leon_pos[2] + mulf32(cosLerp(leon_yaw), inttof32(3));
                                        bn_block_t[b] = 2400;
                                        break;
                                    }
                            b_replan = 0;
                            if (b_stuck > 4)
                            {
                                bot_skip_t[bg.kind][bg.idx & 63] = 900;
                                bg.kind = BG_NONE;
                                b_stuck = 0;
                            }
                        }
                        else
                            b_stuck = 0;
                        for (int i = 0; i < 3; i++)
                            b_last[i] = at[i];
                    }
                }
            }
        }
        if (t_idx < 0 && react == REACT_NONE && gun_state == GUN_READY && ammo < MAG_SIZE / 2 && spare > 0 && b_frames % 40 == 0)
            held = (held & ~KEY_R) | KEY_Y;
        else if (t_idx < 0 && react == REACT_NONE && gun_state == GUN_HOLSTERED && ammo < MAG_SIZE / 2 && spare > 0 && b_frames % 40 == 0)
            held |= KEY_X;
        char line[64];
        int keys = 0, played = 0;
        for (int id = 80; id <= 86; id++)
            keys += item_held(id);
        for (int i = 0; i < sf->events && i < 64; i++)
            played += ev_bit(ev_played, i);
        snprintf(line, sizeof(line), "%s t%d x%d k%d e%d %d,%d,%d", b_msg, (int)(play_frames / 60), b_deaths, keys, played, f32toint(leon_pos[0]), f32toint(leon_feet), f32toint(leon_pos[2]));
        if (b_noA > 0)
        {
            b_noA--;
            held &= ~KEY_A;
        }
        text_line(19, 2, 2, TEXT_WHITE, line);
        char evs[64];
        int ne = 0;
        for (int i = 0; i < sf->events && i < 40 && ne < 62; i++)
        {
            const StageEvent *ev = &sf->event[i];
            char c = ev->get == GET_AUTO || ev->ctg == EV_START || ev->ctg == EV_END ? '_'
                   : ev_bit(ev_played, i) ? 'P' : ev_bit(ev_deleted, i) ? 'X' : bot_done[i] ? 'D'
                   : bot_skip_t[BG_EVENT][i] ? 'S' : (ev->cond != COND_DESTROYED && ev->cond != COND_WALL && !ev_cond(sf, ev, at)) ? 'c' : '.';
            evs[ne++] = c;
        }
        evs[ne] = 0;
        text_line(18, 2, 14, TEXT_WHITE, evs);
        char wl[48];
        int nk = bn_path_n > 1 && b_k < bn_path_n ? bn_node[bn_path[b_k]].kind : -1;
        snprintf(wl, sizeof(wl), "%d,%d,%d n%d k%d/%d", f32toint(leon_pos[0]), f32toint(leon_feet), f32toint(leon_pos[2]), nk, b_k, bn_path_n);
        text_line(17, 2, 26, TEXT_WHITE, wl);
        char gl[64];
        int gs = -1, grec = -1, gseen = -1;
        if (bg.kind == BG_HUNT && bg.idx < ganados.count)
        {
            const Enemy *e = &ganados.e[bg.idx];
            int p[3] = { e->pos[0], e->feet + inttof32(10), e->pos[2] };
            gs = e->state;
            grec = slot_foe[bg.idx] >= 0 ? foe_list[slot_foe[bg.idx]]->rec : -1;
            gseen = bot_sees(eye, p);
        }
        int dj = -1;
        for (int j = 1; j < bn_path_n && dj < 0; j++)
            if (abs(bn_node[bn_path[j]].feet - bn_node[bn_path[j - 1]].feet) > inttof32(6))
                dj = j;
        if (dj >= 0)
            snprintf(gl, sizeof(gl), "g%d,%d,%d s%d r%d v%d D%d:%d %d,%d,%d", f32toint(bg.pos[0]), f32toint(bg.pos[1]), f32toint(bg.pos[2]), gs, grec, gseen,
                     dj, bn_node[bn_path[dj]].kind, f32toint(bn_node[bn_path[dj]].x), f32toint(bn_node[bn_path[dj]].feet), f32toint(bn_node[bn_path[dj]].z));
        else
            snprintf(gl, sizeof(gl), "g%d,%d,%d s%d r%d v%d", f32toint(bg.pos[0]), f32toint(bg.pos[1]), f32toint(bg.pos[2]), gs, grec, gseen);
        text_line(16, 2, 38, TEXT_WHITE, gl);
    }
}
