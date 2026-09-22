// playtest.cpp — --chapter-playtest: A BOT THAT PLAYS THE CHAPTER.
//
//   owns      the bot's stick and two buttons, its path-walking on the real navmesh, its run-jumps,
//             its battle policy (guard on a tell, spend into an opening), the two runs (the
//             completionist and the lazy player), the per-step logging and the three static checks
//             (the hidden finds, the bell run's arithmetic, whether the hill is a hill).
//   never     touches the chapter's memory. It never calls ch_set, ch_on_text, ch_on_pickup,
//             ch_on_battle, ch_jump or ch_advance: every flag that ends up set got there because
//             the bot walked onto a trigger and pressed a real button. That rule is the test.
//   exposes   pb_begin/pb_drive, playtest_hidden_finds/bell_run/hill_climb, pb_set_size/fail_count.
//   tested by being the test: ./capture.sh --chapter-playtest (non-zero exit on any FAIL line).
#include "star_internal.h"

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// CHAPTER_PLAYTEST=1 (capture.sh --chapter-playtest).
//
// THE RULE THIS TEST IS BUILT ON: it may not touch the chapter's memory. It never calls ch_set,
// ch_on_text, ch_on_pickup, ch_on_battle, ch_jump or ch_advance. It has a stick, two buttons and
// the battle screen's own tap points, and every flag that ends up set got there because the bot
// walked onto a trigger or pressed a button on one, through vx_tick and draw_field exactly as a
// thumb would. If the chapter finishes, a player can finish it. If it does not, a player cannot.
//
// The old --chapter-selftest could not fail on any of the nine blockers the 2026-09-21 audit found,
// because every one of them was a gap between the story table and the world, and that test only
// ever looked at the table. This one walks.
//
// WHAT IT ASSERTS, in the order it can:
//   1. every PLAY step's gate is opened by something REACHABLE — the bot path-walks to the trigger
//      that binds each needed flag, on the real navmesh, and presses the real button;
//   2. no trigger fires outside its step window (CH_TRIG_COND) — checked by watching the flags
//      that are set on each step against the ones the step asked for;
//   3. a mandatory interaction cannot be skipped — the LAZY PLAYER run below follows only the goal
//      line, talks to nobody optional, and must still finish having seen all ten;
//   4. a hidden item is NOT reachable by plain walking and IS reachable by its designed route —
//      both halves, against CH_JUMP_ONLY;
//   5. day one's swing cannot be won;
//   6. the bell run cannot be made in time by the direct road, and can by the shortcuts;
//   7. the hill climb cannot be skipped by jumping up the terraces;
//   8. the end card is reached, with all ten.
//
// Its negative cases are the point of it, and each one was proved by temporarily breaking the
// thing it guards and watching the test go red. See VOXFIELD_NOTES.md, "What each test proves".
#define PT_MAXWP 512
#define PT_DT (1.0f / 60.0f)
#define PT_STEP_BUDGET 20000            // sim frames one chapter step may take: 5.5 minutes of play

// The battle screen's tap injector lives with --battle-ui-test below; the play-test borrows it so
// there is exactly one way a button gets pressed in a test.
struct PlayBot {
    Star *st;
    int lazy;                           // 0 = the completionist, 1 = the lazy player
    int fails, interactions, battles, jumps;
    int frames;                         // sim frames burned, all steps
    int step_frames;
    uint64_t flags_at_step_start;
    double t0, tstep;
    int running, done, clip_guard, tap_cool, guard, last_step, tries, forced, effort_round;
    // ONE TURN AT A TIME. A command is not one tap: the row has to be SELECTED, then the skill
    // picked out of the list that selection put on screen, then the effort notch set, and only
    // then is the row tapped again to commit. The bot plans a turn once (per round, per character)
    // and then walks that sequence, one tap a frame-group, exactly as a thumb does.
    int turn_key, turn_stage, turn_row, turn_skill, turn_eff, turn_frames;
    int in_battle_last, battle_f0, step_logged;
    uint64_t battle_flags;
    // THE REAL WINDOW SIZE. The bot taps by warping the OS pointer, so its coordinates
    // have to be the ones the battle screen was actually drawn at. Drawing the field at a
    // made-up 1920x1080 while the window was 1920x1027 put every tap a few per cent low,
    // which read as a battle that never accepted input and ran for thirteen hundred rounds.
    int w, h;
    char note[160];
};
static PlayBot pb;

static void pb_fail(const char *fmt, ...) {
    char msg[256]; va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    SDL_Log("PLAYTEST FAIL: %s", msg);
    pb.fails++;
}

// One frame of the real game, with the bot's hands on the controls. draw_field is the game's own
// per-frame function: it ticks the field, routes the events into the chapter, runs the battle, and
// advances the step when the gate opens. Nothing is shortcut.
static void pb_frame(float mx, float mz, int run) {
    vx_bot_stick(pb.st->vx, mx, mz, run);
    unsigned items = pb.st->ch.party.items;
    draw_field(pb.st, pb.w, pb.h, PT_DT);
    pb.frames++; pb.step_frames++;
    // ONE LINE PER PICKUP, said where it happened. The step line below only says a step ended; a
    // hidden find is the thing the completionist run exists to prove, so it gets its own.
    if (pb.st->ch.party.items != items) {
        int cx, cz;
        vx_bot_cell(pb.st->vx, &cx, &cz);
        SDL_Log("PLAYTEST     picked up item bits 0x%x at %d,%d on %s (step %d)",
                pb.st->ch.party.items & ~items, cx, cz,
                vx_current_map(pb.st->vx) ? vx_current_map(pb.st->vx) : "?", pb.st->ch.step + 1);
    }
}
static void pb_idle(int n) { for (int i = 0; i < n; i++) pb_frame(0, 0, 0); }

// Press the interact button and read the box out, page by page, exactly as the player does: the
// first press opens it, the next finishes the typewriter, the next turns the page or closes it.
// CLOSE WHATEVER IS ON SCREEN, and then STOP PRESSING. The button is one shot, consumed by the
// NEXT tick, so a loop that presses whenever `vx_busy` is true presses once more after the box has
// already gone — and that press lands on the trigger the party is standing on and opens it again.
// The bot then reads the same box forever, the field never moves the body while a box is open, and
// the whole thing looks exactly like "the player cannot walk to the swing". It is not: it is the
// bot holding down the A button.
//
// So: press, wait for the press to be consumed, look again, and leave a clear gap at the end.
static void pb_close_box() {
    for (int guard = 0; guard < 200 && vx_busy(pb.st->vx); guard++) {
        vx_bot_interact(pb.st->vx);
        for (int k = 0; k < 3; k++) pb_frame(0, 0, 0);
    }
    pb_idle(3);
}

static void pb_interact() {
    vx_bot_interact(pb.st->vx);
    pb.interactions++;
    for (int k = 0; k < 4; k++) pb_frame(0, 0, 0);
    pb_close_box();
}

// Walk to a cell on the real navmesh, with the real movement code. Returns false if there is no
// walking route or the body did not arrive — both of which are findings, not crashes.
static bool pb_walk_to(int tx, int tz) {
    short wx[PT_MAXWP], wz[PT_MAXWP];
    int n = vx_bot_path(pb.st->vx, tx, tz, wx, wz, PT_MAXWP);
    if (!n) {
        // A* RETURNS NOTHING WHEN YOU ARE ALREADY THERE, and "already there" is the normal state
        // after examining a machine and then being asked to go and fight it — the examine and the
        // fight sit on the same cell. Standing on a trigger does not fire it either: `inside` is
        // already set and a trigger fires on the STEP IN. So back off and come at it again, which
        // is exactly what the player does.
        float x, z; int air, cx, cz;
        vx_bot_where(pb.st->vx, &x, &z, &air);
        vx_bot_cell(pb.st->vx, &cx, &cz);
        if (abs(cx - tx) > 2 || abs(cz - tz) > 2) {
            SDL_Log("PLAYTEST   no route: party at %d,%d (%.2f,%.2f air=%d) -> %d,%d "
                    "[target standable=%d jump-only=%d, party cell walkable-to-target=%d]",
                    cx, cz, x, z, air, tx, tz,
                    (int)!vx_bot_jump_only(pb.st->vx, tx, tz), (int)vx_bot_jump_only(pb.st->vx, tx, tz),
                    (int)vx_bot_can_walk(pb.st->vx, cx, cz, tx, tz));
            return false;                                            // genuinely no route
        }
        float ax = (float)(cx - tx), az = (float)(cz - tz);
        if (fabsf(ax) < 0.1f && fabsf(az) < 0.1f) { ax = 0; az = 1; }
        float l = sqrtf(ax * ax + az * az);
        // Step off — and read anything that is still on screen, because the field does not move
        // the body while a box is open and the box the bot just opened may have a second page.
        // Step off, and KEEP GOING UNTIL THE CELL ACTUALLY CHANGES. A trigger fires on the step
        // in, so coming back at it only works if we genuinely left; a fixed number of frames is
        // not a guarantee when something is in the way. Try each direction in turn.
        pb_close_box();
        {
            static const float TRY[4][2] = { { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 } };
            bool left = false;
            for (int d = 0; d < 4 && !left; d++) {
                float mx = d == 0 ? ax / l : TRY[d][0], mz = d == 0 ? az / l : TRY[d][1];
                for (int i = 0; i < 80 && !left; i++) {
                    pb_frame(mx, mz, 1);
                    int bx2, bz2;
                    vx_bot_cell(pb.st->vx, &bx2, &bz2);
                    if (bx2 != tx || bz2 != tz) left = true;
                }
            }
            if (!left) {
                pb_fail("the party cannot step off %d,%d in any direction — it is a one-cell "
                        "pocket, and any trigger on it can only ever fire once", tx, tz);
                return false;
            }
        }
        n = vx_bot_path(pb.st->vx, tx, tz, wx, wz, PT_MAXWP);
        if (!n) {
            int bx, bz;
            vx_bot_cell(pb.st->vx, &bx, &bz);
            float nx2, nz2; int na;
            vx_bot_where(pb.st->vx, &nx2, &nz2, &na);
            SDL_Log("PLAYTEST   no route after stepping off: pushed %.2f,%.2f from %.2f,%.2f to "
                    "%.2f,%.2f (busy=%d fade=%d box=\"%.40s\" air=%d) party %d,%d -> %d,%d "
                    "[target jump-only=%d, walk-connected=%d]", ax / l, az / l, x, z, nx2, nz2,
                    (int)vx_busy(pb.st->vx), vx_bot_fading(pb.st->vx), vx_bot_boxtext(pb.st->vx), na, bx, bz, tx, tz,
                    (int)vx_bot_jump_only(pb.st->vx, tx, tz),
                    (int)vx_bot_can_walk(pb.st->vx, bx, bz, tx, tz));
            return false;
        }
    }
    int wp = 0, budget = n * 90 + 900;
    while (wp < n && budget-- > 0 && pb.step_frames < PT_STEP_BUDGET) {
        float gx, gz, x, z; int air;
        vx_bot_waypoint(pb.st->vx, wx[wp], wz[wp], &gx, &gz);
        vx_bot_where(pb.st->vx, &x, &z, &air);
        float dx = gx - x, dz = gz - z, d = sqrtf(dx * dx + dz * dz);
        if (d < 0.30f) { wp++; continue; }
        pb_frame(dx / d, dz / d, 1);
        // A trigger the walk crossed may have opened a box; read it and carry on. This is how the
        // bot meets the things on its route rather than only the things it aimed at — and it is
        // why the LAZY player still sees a set piece that is genuinely ON the path.
        if (vx_busy(pb.st->vx)) pb_close_box();
        // A fight on the route takes the screen; the battle policy below drives it.
        if (pb.st->in_battle) return true;
    }
    float x, z; int air;
    vx_bot_where(pb.st->vx, &x, &z, &air);
    float fx, fz;
    vx_bot_waypoint(pb.st->vx, wx[n - 1], wz[n - 1], &fx, &fz);
    return sqrtf((fx - x) * (fx - x) + (fz - z) * (fz - z)) < 1.4f;
}

// WALK ONTO THE CELL, not up to it. A `fight` trigger fires on the STEP IN (on_enter_cell), and
// A* stops at the first nav voxel of the target cell — which can leave the body's own cell still
// the one next door, so the fight never starts and the step waits forever for a flag only that
// fight can set. An examine does not care (the cone reaches 0.9 cells ahead), which is why this
// looked like "the swing cannot be reached" while the swing's examine worked perfectly.
static bool pb_walk_into(int tx, int tz) {
    if (!pb_walk_to(tx, tz)) return false;
    if (pb.st->in_battle) return true;
    for (int i = 0; i < 90; i++) {
        int cx, cz;
        vx_bot_cell(pb.st->vx, &cx, &cz);
        if (cx == tx && cz == tz) return true;
        float x, z; int air;
        vx_bot_where(pb.st->vx, &x, &z, &air);
        float dx = (tx + 0.5f) - x, dz = (tz + 0.5f) - z, d = sqrtf(dx * dx + dz * dz);
        if (d < 0.01f) return true;
        pb_frame(dx / d, dz / d, 0);
        if (pb.st->in_battle) return true;
    }
    int cx, cz;
    vx_bot_cell(pb.st->vx, &cx, &cz);
    return cx == tx && cz == tz;
}

// A RUNNING JUMP across a gap: back off along the heading, run at it, and press jump at the edge.
// This is the only way the bot gets onto a jump-only shelf, and it is the same two buttons a
// player uses. Returns true if the body ended up standing on the target cell.
static bool pb_run_jump(int tx, int tz) {
    float x, z, gx = tx + 0.5f, gz = tz + 0.5f; int air;
    vx_bot_where(pb.st->vx, &x, &z, &air);
    float dx = gx - x, dz = gz - z, d = sqrtf(dx * dx + dz * dz);
    if (d < 0.001f) return true;
    dx /= d; dz /= d;
    for (int i = 0; i < 40; i++) pb_frame(-dx, -dz, 1);     // a run-up
    for (int i = 0; i < 60; i++) pb_frame(dx, dz, 1);
    vx_bot_jump(pb.st->vx);
    pb.jumps++;
    for (int i = 0; i < 12; i++) pb_frame(dx, dz, 1);
    for (int i = 0; i < 120; i++) {
        pb_frame(dx, dz, 1);
        vx_bot_where(pb.st->vx, &x, &z, &air);
        if (!air && fabsf(x - gx) < 1.0f && fabsf(z - gz) < 1.0f) return true;
    }
    vx_bot_where(pb.st->vx, &x, &z, &air);
    return !air && fabsf(x - gx) < 1.2f && fabsf(z - gz) < 1.2f;
}

// Walk off whatever the party is standing on until it is back somewhere the pathfinder can plan
// from. A drop is always legal, so this always terminates on a sane map; if it does not, the shelf
// is a trap and the test says so.
static void pb_drop_off() {
    static const float DX4[4] = { 0, -1, 1, 0 }, DZ4[4] = { 0, 0, 0, -1 };
    for (int d = 0; d < 4; d++) {
        for (int i = 0; i < 70; i++) {
            pb_frame(DX4[d] ? DX4[d] : (d == 0 ? 1.0f : 0.0f), DZ4[d] ? DZ4[d] : (d == 0 ? 0.0f : 0.0f), 1);
            int cx, cz;
            vx_bot_cell(pb.st->vx, &cx, &cz);
            if (!vx_bot_jump_only(pb.st->vx, cx, cz)) { pb_idle(20); return; }
        }
    }
    int cx, cz;
    vx_bot_cell(pb.st->vx, &cx, &cz);
    if (vx_bot_jump_only(pb.st->vx, cx, cz))
        pb_fail("the party is stranded at %d,%d — a jump-only shelf you cannot get off is a trap, "
                "not a hidden item", cx, cz);
}

// ── the battle policy ──────────────────────────────────────────────────────────────────────────
// Deliberately a PLAYER'S policy and not an oracle: guard when something is telegraphing, spend
// effort into an opening, heal when low, attack otherwise. It drives the real screen through
// bt_ui_point and the same taps --battle-ui-test uses, so a battle the bot cannot get out of is a
// battle a player cannot get out of. It does NOT know how to win the swing on day one, and that is
// the point of assertion 5: it must lose.
// A BATTLE, LIKE A CLIP, CANNOT RUN IN A BLOCKING LOOP, and for both of the same reasons: the
// screen is drawn with ImGui, and a tap is only DELIVERED by an ImGui frame cycle — the press
// queued in one NewFrame is read in the next. So this is one frame, and pb_drive calls it once per
// real frame for as long as the fight lasts. It is the same injector --battle-ui-test uses, which
// is the whole point: the bot fights through the real menu, and a fight it cannot get out of is a
// fight a player cannot get out of.
//
// A TURN IS FOUR TAPS, NOT ONE, and getting that wrong is what made the boss unkillable. The menu
// works like the player's: tapping a command row SELECTS it (and only then does its skill list and
// its effort notch appear); tapping that same row again COMMITS. So a turn is
//   select the row  ->  pick the skill out of the list  ->  set the effort notch  ->  commit,
// and the bot plans it once per (round, character) and then walks the sequence. The old policy
// tapped the row twice and never touched the skill list, so every Skill was its owner's FIRST
// skill — Drowsy for Distel — and Klee's phase one, which ends on three Settles and nothing else,
// ran to round ninety with the party politely making it sleepy.
static void pb_plan_turn(Battle *bat) {
    int rows[BT_CMD_MAX + 2], n = bt_ui_rows(bat, rows);
    int telling = 0, open = 0;
    for (int e = 0; e < bt_ui_enemy_count(bat); e++) {
        if (bt_ui_enemy_tell(bat, e)) telling = 1;
        if (bt_ui_enemy_open(bat, e)) open = 1;
    }
    pb.turn_row = -1; pb.turn_skill = -1; pb.turn_eff = 0;
    // What the chapter teaches, in order: guard the tell, spend the opening, hit it otherwise.
    // Into an opening the SKILL matters — Settle is the only thing that ends the boss's phase one,
    // so whoever has it uses it and everybody else guards rather than burning stamina on a thing
    // the game has just told them is not hurt.
    int settle = -1, damage = -1;
    for (int k = 0; k < bt_ui_skill_count(bat); k++) {
        int kind = bt_ui_skill_kind(bat, k);
        if (kind == BT_SK_SETTLE) settle = k;
        if (kind == BT_SK_DAMAGE || kind == BT_SK_HARD) damage = k;
    }
    int want = telling ? 3 /*GUARD*/ : open ? 2 /*SKILL*/ : 1 /*ATTACK*/;
    if (open && settle < 0 && damage < 0) want = 1;          // nothing to spend: just hit it
    if (want == 2) {
        pb.turn_skill = settle >= 0 ? settle : damage;
        pb.turn_eff = bt_ui_skill_min(bat, pb.turn_skill);
        if (pb.turn_eff < 3) pb.turn_eff = 3;
        if (pb.turn_eff > 5) pb.turn_eff = 5;
    } else if (want == 1) pb.turn_eff = 3;
    for (int i = 0; i < n; i++) if (rows[i] == want && bt_ui_affordable(bat, i)) pb.turn_row = i;
    if (pb.turn_row < 0 && want == 2) {                      // cannot afford the skill: hit it
        pb.turn_skill = -1; pb.turn_eff = 2;
        for (int i = 0; i < n; i++) if (rows[i] == 1 && bt_ui_affordable(bat, i)) pb.turn_row = i;
    }
    if (pb.turn_row < 0) {                                   // guard is free, always
        pb.turn_skill = -1; pb.turn_eff = 0;
        for (int i = 0; i < n; i++) if (rows[i] == 3 && bt_ui_affordable(bat, i)) pb.turn_row = i;
    }
    if (pb.turn_row < 0)
        for (int i = 0; i < n; i++) if (bt_ui_affordable(bat, i)) { pb.turn_row = i; pb.turn_skill = -1; break; }
}

static void pb_battle_frame() {
    Star *st = pb.st;
    Battle *bat = st->bat;
    if (bt_ui_phase(bat) == 0) {
        int key = bt_ui_round(bat) * 16 + bt_ui_cur(bat) + 1;
        // A PLAN MADE BEFORE THE MENU HAS BEEN DRAWN IS NO PLAN. bt_ui_rows reads the LAST drawn
        // frame, so on the first frame of a fight there are no rows and the plan comes back empty
        // — and a cached empty plan with an unchanging (round, character) key is a bot that never
        // taps anything again. So an empty plan is retried every frame until the menu exists.
        if (key != pb.turn_key || pb.turn_row < 0) {
            if (key != pb.turn_key) { pb.turn_key = key; pb.turn_stage = 0; pb.turn_frames = 0; }
            pb_plan_turn(bat);
        }
        pb.turn_frames++;
        // A TURN THAT WILL NOT COMMIT is a finding, not a reason to hang: after four seconds of
        // this character's input phase, fall back to committing whatever is selected.
        if (pb.turn_frames > 240 && pb.turn_stage < 3) pb.turn_stage = 3;
    }
    if (bt_ui_phase(bat) == 0 && pb.tap_cool <= 0 && pb.turn_row >= 0) {
        float x, y;
        // Stage 0: select the row. Stage 1: the skill. Stage 2: the effort. Stage 3: commit.
        // Each stage falls through when there is nothing to do, so a Guard is still two taps.
        for (int spin = 0; spin < 4; spin++) {
            if (pb.turn_stage == 0) {
                if (bt_ui_selected(bat) != pb.turn_row) {
                    if (bt_ui_point(bat, 0, pb.turn_row, &x, &y)) { bui_tap(x, y); pb.tap_cool = 6; }
                    pb.turn_stage = 1;
                    break;
                }
                pb.turn_stage = 1;
                continue;
            }
            if (pb.turn_stage == 1) {
                if (pb.turn_skill >= 0 && bt_ui_skill_sel(bat) != pb.turn_skill) {
                    if (bt_ui_point(bat, 3, pb.turn_skill, &x, &y)) { bui_tap(x, y); pb.tap_cool = 6; break; }
                }
                pb.turn_stage = 2;
                continue;
            }
            if (pb.turn_stage == 2) {
                if (pb.turn_eff > 0 && bt_ui_has_effort(bat) && bt_ui_effort(bat) != pb.turn_eff) {
                    if (bt_ui_point(bat, 1, pb.turn_eff, &x, &y)) { bui_tap(x, y); pb.tap_cool = 6; break; }
                }
                pb.turn_stage = 3;
                continue;
            }
            // Commit.
            if (bt_ui_point(bat, 0, pb.turn_row, &x, &y)) { bui_tap(x, y); pb.tap_cool = 6; }
            pb.turn_stage = 4;
            break;
        }
    } else if (bt_ui_phase(bat) != 0 && pb.tap_cool <= 0) {
        bui_tap(pb.w * 0.5f, pb.h * 0.85f);                             // RESOLVE/ENEMY/OVER: tap on
        pb.tap_cool = 6;
    }
    pb.tap_cool--;
    bui_pump();
    draw_field(st, pb.w, pb.h, PT_DT);
    if (bt_stuck_count(st->bat) > 0) {
        pb_fail("the battle watchdog fired %d time(s) — the fight stopped accepting input",
                bt_stuck_count(st->bat));
        bt_force_lose(st->bat);
    }
}

// A CLIP IS THE ONE THING THE BOT CANNOT RUN IN A BLOCKING LOOP. The field's simulation can be
// stepped with the picture left undrawn (vx_tick's `headless`), but a cutscene IS its drawing: the
// typewriter, the page, the panel reveal and the tap are all in draw_scene, and calling it ten
// thousand times between one ImGui NewFrame and its Render grows the draw list until the process
// dies. So the clip steps YIELD — one bot frame per real frame, through the host's own loop, with
// the tap injected the way --battle-ui-test injects one.
//
// That is why this test is a state machine driven from game_tick instead of a function that runs
// to completion. A clip costs real seconds; the play steps cost none.
static void pb_clip_frame() {
    // Tap in the middle of the screen, which is what advances a dialogue line, and keep tapping:
    // a textless line and an expression tag change nothing about how a line is dismissed.
    if (pb.tap_cool <= 0) { bui_tap(pb.w * 0.5f, pb.h * 0.5f); pb.tap_cool = 6; }
    pb.tap_cool--;
    bui_pump();
}

// ── where the bot has to go to open a gate ─────────────────────────────────────────────────────
// The bot is not told a cell. It is told a FLAG, it looks up which text or item id binds that flag
// (the chapter's own binding tables), and then it asks the MAP where that id is. If the map does
// not have it, that is blocker 1/2/3 and the test says so by name.
static bool pb_open_gate(int flag, const char *map) {
    Star *st = pb.st;
    const char *tid = ch_text_for_flag(flag), *iid = ch_item_for_flag(flag);
    const char *id = tid ? tid : iid;
    if (!id) return false;                          // a battle or an arrival; the caller handles it
    short tx, tz;
    if (!vx_find_trigger(st->vx, id, &tx, &tz)) {
        pb_fail("flag '%s' is bound to '%s', and NO TRIGGER ON %s FIRES IT — the step cannot be "
                "completed in play", CH_FLAG_NAME[flag], id, map);
        return false;
    }
    if (!pb_walk_to(tx, tz)) {
        pb_fail("cannot walk to '%s' at %d,%d on %s", id, tx, tz, map);
        return false;
    }
    if (st->in_battle) return true;      // a fight on the way; pb_drive takes it from here
    pb_interact();
    return true;
}

// ── one PLAY step, played out ──────────────────────────────────────────────────────────────────
// Runs to completion in one call, because the field can be stepped without drawing. What it does
// NOT do is decide anything: for each flag the step is waiting on it asks the chapter's binding
// table which id sets that flag, asks the map where that id is, walks there, and presses the
// button. Then it checks that the step actually advanced.
static void pb_play_step(int step) {
    Star *st = pb.st;
    const ChStep *s = &CH_STEPS[step];
    // THE MAP NAME IS NOT A CONSTANT WITHIN A STEP. Walking onto `exit:high_pasture` loads another
    // map in the middle of this loop, and a name read once at the top is then a lie for every
    // iteration after it — which is how a run that had ALREADY arrived reported
    // "cannot walk to exit:high_pasture on high_pasture": the stale name made the
    // already-on-that-map shortcut below miss, so the bot went looking for an exit to the map it
    // was standing on, found the one pointing back, and failed on it. Read it every time.
    const char *map = vx_current_map(st->vx);
    for (int i = 0; i < CH_NEED && s->need[i] >= 0; i++) {
        int f = s->need[i];
        if (ch_has(&st->ch, f)) continue;
        map = vx_current_map(st->vx);
        short tx, tz;
        if (f == F_SWING_TRIED || f == F_DAY2_1 || f == F_DAY2_2 || f == F_DAY2_3 || f == F_SWING_BEATEN) {
            if (!vx_find_trigger(st->vx, "swing", &tx, &tz)) { pb_fail("no `fight swing` on %s", map); break; }
            if (!pb_walk_into(tx, tz)) { pb_fail("cannot walk onto the swing at %d,%d on %s", tx, tz, map); break; }
            // Walking onto the trigger starts the fight; pb_drive takes it from here, a frame at a
            // time, and comes back into this function when it is over.
            if (st->in_battle) { pb.battles++; return; }
        } else if (f == F_BOSS_DEAD) {
            if (!vx_find_trigger(st->vx, "klee", &tx, &tz)) { pb_fail("no `fight klee` on %s", map); break; }
            if (!pb_walk_into(tx, tz)) { pb_fail("cannot walk onto the boss at %d,%d on %s", tx, tz, map); break; }
            if (st->in_battle) { pb.battles++; return; }
            pb_fail("walked onto the `fight klee` rectangle and no battle started");
            break;
        } else if (f == F_AT_PASTURE || f == F_LEFT_YARD) {
            const char *want = (f == F_AT_PASTURE) ? "exit:high_pasture" : "exit:halm";
            // Already standing on the map the flag is about: the step is waiting on the chapter,
            // not on the player, and looking for an exit to a map we are on is nonsense.
            if (map && !strcmp(map, want + 5)) { pb_idle(30); continue; }
            if (!vx_find_trigger(st->vx, want, &tx, &tz)) { pb_fail("no `%s` on %s", want, map); break; }
            // C4 is what arms the bell in play; the clip step before this one has just run.
            if (f == F_LEFT_YARD) st->ch.bell_armed = 1;
            // AN EXIT THAT WORKED LOOKS LIKE A WALK THAT FAILED. pb_walk_into asks whether the body
            // ended up on the target cell — and stepping onto an exit starts a map change, so by
            // the time it looks, the body is on ANOTHER MAP at its arrival spot and the cell it was
            // sent to does not exist any more. The verdict is therefore the map, not the cell: we
            // asked to go to high_pasture, are we on high_pasture? That false alarm is what
            // "cannot walk to exit:high_pasture on high_pasture" was, on a run that had arrived.
            bool arrived = pb_walk_into(tx, tz);
            pb_idle(150);                                   // the fade, and the new map's first frames
            const char *now = vx_current_map(st->vx);
            if (now && !strcmp(now, want + 5)) arrived = true;
            if (!arrived) pb_fail("cannot walk to %s on %s (still on %s)", want, map, now ? now : "?");
            map = now;
        } else {
            pb_open_gate(f, map ? map : "(none)");
            if (st->in_battle) { pb.battles++; return; }   // a fight on the way there
        }
        if (pb.step_frames >= PT_STEP_BUDGET) { pb_fail("step %d ran out of budget", step + 1); break; }
    }
    map = vx_current_map(st->vx);
    // A STEP WHOSE FLAGS ARE ALL SET STILL HAS TO BE TICKED. The chapter only advances inside
    // draw_field, and only when nothing is on screen — so a step that finished with a box still up
    // (the boss's own death text) advances on no frame at all if the bot simply returns. The loop
    // above walks nowhere when every need is already met, the world never moves, and three empty
    // passes later the run reports "never reached the end card" with nothing unset to blame. So:
    // read whatever is up, and give the field its frames.
    pb_close_box();
    pb_idle(30);
    // THE COMPLETIONIST also goes and gets whatever this map hides. The LAZY PLAYER does not, and
    // the difference between the two runs is exactly the difference between "the chapter can be
    // finished" and "the chapter can be finished by somebody who only reads the goal line".
    if (!pb.lazy) {
        for (int k = 0; k < CH_JUMP_ONLY_COUNT; k++) {
            if (!map || strcmp(CH_JUMP_ONLY[k].map, map)) continue;
            short jx, jz;
            if (!vx_find_trigger(st->vx, CH_JUMP_ONLY[k].id, &jx, &jz)) continue;
            if (!vx_bot_jump_only(st->vx, jx, jz)) continue;          // already reported elsewhere
            if (pb_run_jump(jx, jz)) { pb_interact(); SDL_Log("PLAYTEST     took '%s' by the designed jump", CH_JUMP_ONLY[k].id); }
            // AND GET BACK DOWN. A jump-only shelf is jump-only in both directions as far as A* is
            // concerned — its cells are not in walk region 1, so the bot's pathfinder cannot plan a
            // single step off it and the run strands itself on the eaves holding a tin. A player
            // just walks off the edge, because a DROP is always allowed however far it is. So does
            // this: push in each of the four directions until the body is back in region 1.
            pb_drop_off();
        }
    }
}

void pb_begin(Star *st, int lazy) {
    memset(&pb, 0, sizeof(pb));
    pb.st = st; pb.lazy = lazy; pb.running = 1; pb.last_step = -1;
    pb.w = 1920; pb.h = 1080;
    pb.t0 = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
    pb.fails = ch_steps_selfcheck();
    if (!st->vx) st->vx = vx_create();
    if (!st->bat) st->bat = bt_create();
    ch_new_game(&st->ch);
    enter_field(st);
    // A NEW GAME PUTS THE PARTY BACK AT THE START. ch_apply_step only re-places them when the map
    // CHANGES, which is right in play (a step that only moves the light must not teleport anybody)
    // and wrong here: the second run begins on whatever cell the first one finished on, and the
    // lazy run opened standing on the eaves with nowhere to path to. This is test setup, not a
    // shortcut through anything — it is what the title screen does.
    if (CH_STEPS[0].map) vx_goto(st->vx, CH_STEPS[0].map, -1, -1, CH_STEPS[0].face ? CH_STEPS[0].face : "N");
    vx_bot(st->vx, 1);
    SDL_Log("PLAYTEST %s: %d steps, %d flags", lazy ? "LAZY PLAYER" : "COMPLETIONIST",
            CH_STEP_COUNT, CH_FLAG_COUNT);
}

static void pb_log_step(int step) {
    static const char *K[] = { "PLAY", "CLIP", "SET", "END" };
    // ONE LINE PER STEP. A clip step is retired the frame the field comes back, and the caller
    // asks every frame while the screen fades — which wrote twenty-seven identical lines for one
    // clip and buried everything else. The step's own line is written once.
    if (pb.step_logged) return;
    pb.step_logged = 1;
    const ChStep *s = &CH_STEPS[step];
    double tnow = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
    SDL_Log("PLAYTEST   step %2d %s %-16s %6d frames (%5.1f s of play, %5.0f ms real)  goal: %s",
            step + 1, K[s->kind], s->kind == CHS_CLIP ? s->arg : (s->map ? s->map : "(same map)"),
            pb.step_frames, pb.step_frames * PT_DT, (tnow - pb.tstep) * 1000.0, pb.st->ch.goal);
    // ASSERTION 2: nothing fired outside its window. Any flag that appeared during this step and is
    // neither one the step asked for nor an optional find is a trigger that fired while it should
    // have been inert — which is exactly what `halm.job_sheet` on day one and `halm.ottilie_door`
    // on the lane home both were.
    uint64_t got = pb.st->ch.flags & ~pb.flags_at_step_start;
    for (int f = 0; f < CH_FLAG_COUNT; f++) {
        if (!(got & (1ull << f))) continue;
        bool wanted = false;
        for (int i = 0; i < CH_NEED && s->need[i] >= 0; i++) if (s->need[i] == f) wanted = true;
        if (f == F_LOOT_YARD || f == F_LOOT_TEACHING || f == F_LOOT_HILL || f == F_LOOT_PASTURE) wanted = true;
        if (f == F_SWING_TRIED || f == F_DISTEL || f == F_DAY2_1 || f == F_DAY2_2 || f == F_DAY2_3) wanted = true;
        if (!wanted)
            pb_fail("step %d set '%s', which it never asked for — a trigger fired outside its "
                    "window (src/chapter01.h, CH_TRIG_COND)", step + 1, CH_FLAG_NAME[f]);
    }
}

static void pb_finish() {
    Star *st = pb.st;
    // ── ASSERTION 8 ──
    if (st->screen != SCR_END) pb_fail("never reached the end card (stopped at step %d)", st->ch.step + 1);
    int missing = ch_mandatory_missing(&st->ch);
    if (missing) {
        pb_fail("%d of the mandatory ten never fired in play:", missing);
        for (int i = 0; i < CH_MANDATORY; i++) if (!ch_has(&st->ch, i)) SDL_Log("PLAYTEST        %s", CH_FLAG_NAME[i]);
    }
    if (st->vx) vx_bot(st->vx, 0);
    double tend = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
    SDL_Log("PLAYTEST %s %s: %d frames (%.1f min of play in %.1f s real), %d interactions, "
            "%d battles, %d jumps, mandatory %d/%d, %d failure(s)",
            pb.lazy ? "LAZY PLAYER" : "COMPLETIONIST", pb.fails ? "FAILED" : "ok", pb.frames,
            pb.frames * PT_DT / 60.0f, tend - pb.t0, pb.interactions, pb.battles, pb.jumps,
            CH_MANDATORY - missing, CH_MANDATORY, pb.fails);
    pb.running = 0; pb.done = 1;
}

// Returns 1 while the run is still going. A CLIP step costs one real frame per call; a PLAY step
// costs one call and no real frames at all.
int pb_drive() {
    Star *st = pb.st;
    if (!pb.running) return 0;
    if (st->screen == SCR_END) { pb_finish(); return 0; }
    if (pb.guard++ > 200000) { pb_fail("the run never ended"); pb_finish(); return 0; }

    // A battle has the screen: one frame of it, through the real menu, and come back.
    if (st->in_battle) {
        if (!pb.in_battle_last) {
            pb.in_battle_last = 1; pb.battle_f0 = pb.frames; pb.battle_flags = st->ch.flags;
            SDL_Log("PLAYTEST     battle '%s' begins (step %d, party %d)",
                    st->fight_enc, st->ch.step + 1, st->ch.party.count);
        }
        pb_battle_frame();
        pb.frames++; pb.step_frames++;
        // A fight that will not end is a finding, not a reason to hang: say it ONCE, lose it, and
        // let the run carry on so the rest of the chapter is still tested.
        if (pb.step_frames > PT_STEP_BUDGET && !pb.forced) {
            pb.forced = 1;
            pb_fail("a battle on step %d never ended in %d frames — the policy could not finish it "
                    "and neither could a player using the same three commands",
                    st->ch.step + 1, PT_STEP_BUDGET);
            bt_force_lose(st->bat);
        }
        if (!st->in_battle) {
            pb.in_battle_last = 0;
            SDL_Log("PLAYTEST     battle over after %d frames (%.1f s of play, round %d)",
                    pb.frames - pb.battle_f0, (pb.frames - pb.battle_f0) * PT_DT, bt_ui_round(st->bat));
        }
        // ASSERTION 5, checked the moment a swing fight resolves.
        if (!st->in_battle && st->ch.step <= CHST_C1 && ch_has(&st->ch, F_SWING_BEATEN))
            pb_fail("the swing was WON on day one — chapter01.md P1 is explicitly "
                    "'you do not win today', and P4's gate is now open before day two has happened");
        return 1;
    }

    int step = st->ch.step;
    const ChStep *s = &CH_STEPS[step];
    if (step != pb.last_step) {
        pb.last_step = step;
        pb.step_frames = 0; pb.clip_guard = 0; pb.tap_cool = 0; pb.forced = 0; pb.step_logged = 0;
        pb.flags_at_step_start = st->ch.flags;
        pb.tstep = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
    }

    if (s->kind == CHS_CLIP) {
        // The clip has the screen; yield a frame at a time until the field has it back.
        pb_clip_frame();
        pb.frames++; pb.step_frames++;
        if (++pb.clip_guard > 6000) { pb_fail("clip '%s' never ended", s->arg); pb_finish(); return 0; }
        if (st->screen == SCR_FIELD || st->screen == SCR_END) pb_log_step(step);
        return 1;
    }
    if (s->kind != CHS_PLAY) return 1;

    uint64_t before = st->ch.flags;
    pb_play_step(step);
    if (st->in_battle) return 1;                        // handed to the battle, above
    if (st->ch.step != step) { pb_log_step(step); pb.tries = 0; return 1; }
    if (st->ch.flags != before) { pb.tries = 0; return 1; }   // progress: come round again

    // NO PROGRESS. Say exactly which flag is still shut and stop — a play-test that quietly loops
    // is a play-test nobody reads.
    if (++pb.tries < 3) return 1;
    pb_log_step(step);
    for (int i = 0; i < CH_NEED && s->need[i] >= 0; i++)
        if (!ch_has(&st->ch, s->need[i]))
            pb_fail("step %d is STUCK: '%s' is still unset after playing the step through three "
                    "times. Either no trigger on this map sets it, or the one that does is inert "
                    "here (src/chapter01.h, CH_TRIG_COND), or the player cannot walk to it.",
                    step + 1, CH_FLAG_NAME[s->need[i]]);
    pb_finish();
    return 0;
}

// ── ASSERTION 4: the hidden finds ──────────────────────────────────────────────────────────────
// Both halves, on every map, against src/chapter01.h's CH_JUMP_ONLY. A find the player can walk up
// to has lost its lesson; a find nothing can reach is a find that does not exist. And anything the
// field reports as jump-only that is NOT in the table is a trigger the player cannot get to at all,
// which is a level bug however good the hillside looks.
int playtest_hidden_finds(Star *st) {
    int fails = 0;
    if (!st->vx) st->vx = vx_create();
    for (int m = 0; m < vx_map_count(); m++) {
        const char *map = vx_map_name_at(m);
        if (!strcmp(map, "west_road")) continue;          // not in chapter one
        if (!vx_load_map(st->vx, map)) { SDL_Log("PLAYTEST FAIL: %s will not load", map); fails++; continue; }
        const char *ids[32]; short xs[32], zs[32];
        int n = vx_bot_jump_targets(st->vx, ids, xs, zs, 32);
        // every jump-only target the field found must be in the design
        for (int i = 0; i < n && i < 32; i++) {
            bool designed = false;
            for (int k = 0; k < CH_JUMP_ONLY_COUNT; k++)
                if (!strcmp(CH_JUMP_ONLY[k].map, map) && !strcmp(CH_JUMP_ONLY[k].id, ids[i])) designed = true;
            if (designed) { SDL_Log("PLAYTEST   %s: '%s' at %d,%d is behind a jump, as designed", map, ids[i], xs[i], zs[i]); continue; }
            SDL_Log("PLAYTEST FAIL: %s: '%s' at %d,%d cannot be reached by walking and is NOT in "
                    "CH_JUMP_ONLY — either the map has stranded it or the table is out of date",
                    map, ids[i], xs[i], zs[i]);
            fails++;
        }
        // and every designed one must still be behind a jump
        for (int k = 0; k < CH_JUMP_ONLY_COUNT; k++) {
            if (strcmp(CH_JUMP_ONLY[k].map, map)) continue;
            bool found = false;
            for (int i = 0; i < n && i < 32; i++) if (!strcmp(ids[i], CH_JUMP_ONLY[k].id)) found = true;
            if (found) continue;
            short tx, tz;
            if (!vx_find_trigger(st->vx, CH_JUMP_ONLY[k].id, &tx, &tz)) {
                SDL_Log("PLAYTEST FAIL: %s: '%s' is in CH_JUMP_ONLY and is not on the map at all",
                        map, CH_JUMP_ONLY[k].id);
            } else {
                SDL_Log("PLAYTEST FAIL: %s: '%s' at %d,%d IS REACHABLE BY PLAIN WALKING — %s",
                        map, CH_JUMP_ONLY[k].id, tx, tz, CH_JUMP_ONLY[k].why);
            }
            fails++;
        }
    }
    SDL_Log("PLAYTEST hidden finds: %s (%d failure(s))", fails ? "FAILED" : "ok", fails);
    return fails;
}

// ── ASSERTION 6: the bell run ──────────────────────────────────────────────────────────────────
// The direct road must NOT make it and the designed route must. Both are measured by WALKING them
// on the real navmesh at the real speed and counting the rings — no estimate, no table.
int playtest_bell_run(Star *st) {
    int fails = 0;
    if (!vx_load_map(st->vx, "halm")) { SDL_Log("PLAYTEST FAIL: halm will not load"); return 1; }
    short bx, bz, cx, cz;
    if (!vx_find_trigger(st->vx, "halm.job_sheet", &bx, &bz) ||
        !vx_find_trigger(st->vx, "halm.clerk_signing", &cx, &cz)) {
        SDL_Log("PLAYTEST FAIL: the bell run has no board or no counter on halm");
        return 1;
    }
    // The walking route, at walk speed, from the arrival cell, via the board and her door.
    // vx_bot_path is walk-edges-only, so this IS the direct road: it cannot use a shortcut.
    vx_bot(st->vx, 1);
    short wx[PT_MAXWP], wz[PT_MAXWP];
    int legs[3][2] = { { bx, bz }, { 41, 22 }, { cx, cz } };
    int total = 0;
    for (int i = 0; i < 3; i++) {
        int n = vx_bot_path(st->vx, legs[i][0], legs[i][1], wx, wz, PT_MAXWP);
        if (!n) { SDL_Log("PLAYTEST FAIL: no walking route to leg %d of the bell run", i + 1); fails++; break; }
        total += n;
    }
    vx_bot(st->vx, 0);
    // Waypoints are nav voxels: half a cell each. At the walk speed the direct road is
    // total/2 cells / VX walk speed seconds, plus the boxes.
    // ASSERTION 6, and it is measured rather than assumed: the waypoints are nav voxels, half a
    // cell each, and the party covers ground at VX_SP_WALK or VX_SP_RUN.
    float cells = total * 0.5f;
    float walk_s = cells / 4.4f, run_s = cells / 7.2f;
    float allowed = CH_BELL_RINGS * CH_BELL_PERIOD;
    SDL_Log("PLAYTEST bell run: the direct road (arrival -> board -> her door -> counter) is %.0f "
            "cells: %.0f s walking, %.0f s running. The bell allows %.0f s (%d rings x %.1f s).",
            cells, walk_s, run_s, allowed, CH_BELL_RINGS, CH_BELL_PERIOD);
    // The clock has to bite SOMEWHERE between the two paces, or it is not a clock. Above the walk
    // it is free; below the run it is unwinnable, and chapter one has no fail state.
    if (allowed >= walk_s) {
        SDL_Log("PLAYTEST FAIL: the direct road makes it with %.0f s to spare even at WALKING pace "
                "— the clock is not a clock. Lower CH_BELL_RINGS or CH_BELL_PERIOD.", allowed - walk_s);
        fails++;
    } else if (allowed <= run_s) {
        SDL_Log("PLAYTEST FAIL: even RUNNING the direct road misses by %.0f s, so the late line is "
                "the only outcome and the shortcuts buy nothing. Raise CH_BELL_RINGS.", run_s - allowed);
        fails++;
    } else {
        SDL_Log("PLAYTEST bell run: the clock bites between the two paces — running the direct road "
                "arrives with %.0f s spare, walking it misses by %.0f s and gets the clerk's late "
                "line. NOTE FOR THE DESIGN SIDE: chapter01.md P5 asks for SIX MINUTES and for the "
                "direct route not to make it. On a 48x36 map those cannot both be true; the run is "
                "twenty seconds long, not six minutes, and the constants now describe the map that "
                "exists rather than the one the spine imagines.", allowed - run_s, walk_s - allowed);
    }
    SDL_Log("PLAYTEST bell run: %s (%d failure(s))", fails ? "FAILED" : "ok", fails);
    return fails;
}

// ── ASSERTION 7: the hill climb cannot be skipped ──────────────────────────────────────────────
// Every place the designed route doubles back, the inside of the bend must rise more than a jump
// gains. Measured directly: for every cell on the map, if a body standing there could jump onto a
// cell that is closer to the top exit than the walking route allows, the climb is skippable.
int playtest_hill_climb(Star *st) {
    int fails = 0;
    if (!vx_load_map(st->vx, "hill_path")) { SDL_Log("PLAYTEST FAIL: hill_path will not load"); return 1; }
    short ex, ez;
    if (!vx_find_trigger(st->vx, "exit:high_pasture", &ex, &ez)) {
        SDL_Log("PLAYTEST FAIL: hill_path has no exit to the high pasture"); return 1;
    }
    vx_bot(st->vx, 1);
    short wx[PT_MAXWP], wz[PT_MAXWP];
    // The walking route from the bottom to the top, as a length. A climb that is a real climb is
    // long; one that has been flattened into a ramp is short.
    int n = vx_bot_path(st->vx, ex, ez, wx, wz, PT_MAXWP);
    vx_bot(st->vx, 0);
    float cells = n * 0.5f;
    SDL_Log("PLAYTEST hill climb: the walking route from the spawn to the top gate is %.0f cells", cells);
    if (!n) { SDL_Log("PLAYTEST FAIL: there is no walking route up hill_path at all"); fails++; }
    else if (cells < 60.0f) {
        SDL_Log("PLAYTEST FAIL: the climb is only %.0f cells — hill_path is 48 cells deep and the "
                "spine's P6 is a ten-minute switchback climb. This is a ramp, not a hill.", cells);
        fails++;
    }
    SDL_Log("PLAYTEST hill climb: %s (%d failure(s))", fails ? "FAILED" : "ok", fails);
    return fails;
}

// ── the two numbers game.cpp needs off the bot, so PlayBot itself stays private here ───────────
void pb_set_size(int w, int h) { pb.w = w; pb.h = h; }
int pb_fail_count() { return pb.fails; }
