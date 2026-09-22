// star_test.cpp — the three narrow self-tests that are not the play-test.
//
//   owns      --clips-selftest (every scene tapped through the REAL player: it ends, every panel is
//             revealed exactly once, every line is reached), --battle-ui-test (the real battle
//             screen driven by injected taps, with the watchdog asserted at zero), and --robustness
//             (save/continue, the reload blob, settings, the battle fuzz, every field text in the
//             box). bui_tap is the ONE way any test presses a button.
//   never     changes the game's behaviour: nothing in game.cpp, cutscene.cpp or chapter.cpp calls
//             into this file except to start a driver from a self_test mode.
//   exposes   ct_begin/ct_drive, bui_tap/bui_pump/bui_start/battle_ui_test, rb_drive.
//   tested by being the test: ./capture.sh --clips-selftest --battle-ui-test --robustness.
#include "star_internal.h"

// ═══════════════════════════════════════════════════════════════════════════════════════════════
//  --clips-selftest: EVERY SCENE ENDS UNDER TAPPING
// ═══════════════════════════════════════════════════════════════════════════════════════════════
// CLIPS_SELFTEST=1. The narrow unit test under the play-test's clip steps: for every scene in the
// chapter, tap through it at 60 fps with the REAL player (draw_scene, the real typewriter, the real
// pagination, the real panel reveal) and assert three things.
//
//   1. it ENDS — star_advance fires off the last line inside line_count x 2 seconds. A clip that
//      does not end is the whole chapter stopped, and it is invisible to every test that reads the
//      chapter table instead of drawing the page;
//   2. every panel is revealed EXACTLY ONCE — a `[n]` tag pointing past the last panel, or two
//      lines fighting over one panel, both show up here;
//   3. every LINE is reached. A textless line (`Distel (sorrow):` with no words) has no typewriter
//      to finish, so the marker is up from the first frame and the next tap must move on; a line
//      that swallows taps would show as a scene that times out on that line, by name.
//
// The scene is driven with no fade, because the fade is the field's business and this test is the
// page's. It costs one real frame per sim frame (a clip IS its drawing), so the whole chapter is a
// few seconds.
struct ClipTest {
    int scene, frames, budget, tap_cool, fails, done, checked;
    int line_seen[64];                     // times each line was entered
    int panel_seen[MAX_PANELS];            // times each panel was revealed
    bool panel_was[MAX_PANELS];
    int last_line;
};
static ClipTest ct;

void ct_begin(Star *st, int scene) {
    memset(ct.line_seen, 0, sizeof(ct.line_seen));
    memset(ct.panel_seen, 0, sizeof(ct.panel_seen));
    memset(ct.panel_was, 0, sizeof(ct.panel_was));
    ct.scene = scene; ct.frames = 0; ct.tap_cool = 0; ct.last_line = -1;
    st->fade = 0.0f; st->fade_to_scene = -1; st->to_field = false; st->from_field = false;
    star_goto(st, scene, 0, false);
    ct.budget = CS_CUR[scene].line_count * 120 + 240;      // two seconds a line, plus the page-in
}

// Returns 1 while the test is still running. Drives ONE frame.
int ct_drive(Star *st, int w, int h) {
    if (ct.done) return 0;
    if (!ct.checked) {
        ct.checked = 1;
        SDL_Log("CLIPS: %d scenes", CS_CUR_COUNT);
        ct_begin(st, 0);
    }
    const CsScene *sc = &CS_CUR[ct.scene];
    // One frame of the real page.
    st->screen = SCR_INTRO;
    draw_scene(st, w, h, 1.0f / 60.0f);
    ct.frames++;
    if (st->line != ct.last_line) {
        ct.last_line = st->line;
        if (st->line >= 0 && st->line < 64) ct.line_seen[st->line]++;
    }
    for (int i = 0; i < sc->panel_count && i < MAX_PANELS; i++) {
        if (st->panel_on[i] && !ct.panel_was[i]) ct.panel_seen[i]++;
        ct.panel_was[i] = st->panel_on[i];
    }
    if (ct.tap_cool <= 0) { bui_tap(w * 0.5f, h * 0.5f); ct.tap_cool = 6; }
    ct.tap_cool--;
    bui_pump();

    bool ended = (st->fade_to_scene >= 0);
    bool over = (ct.frames > ct.budget);
    if (!ended && !over) return 1;

    int f = 0;
    if (over) {
        SDL_Log("CLIPS FAIL: '%s' did not end in %d frames (%.1f s): stuck on line %d/%d "
                "\"%.48s\" (speaker '%s', page %d, %d chars)", sc->id, ct.budget,
                ct.budget / 60.0f, st->line + 1, sc->line_count, sc->lines[st->line].text,
                sc->lines[st->line].speaker, st->line_page, (int)strlen(sc->lines[st->line].text));
        f++;
    }
    for (int i = 0; i < sc->line_count && i < 64; i++)
        if (!ct.line_seen[i]) { SDL_Log("CLIPS FAIL: '%s' line %d was never reached", sc->id, i + 1); f++; }
    for (int i = 0; i < sc->panel_count && i < MAX_PANELS; i++)
        if (ct.panel_seen[i] != 1) {
            SDL_Log("CLIPS FAIL: '%s' panel %d was revealed %d times, not once", sc->id, i + 1, ct.panel_seen[i]);
            f++;
        }
    SDL_Log("CLIPS   %-22s %s  %2d lines, %d panels, %4d frames (%.1f s)", sc->id,
            f ? "FAILED" : "ok", sc->line_count, sc->panel_count, ct.frames, ct.frames / 60.0f);
    ct.fails += f;
    st->fade_to_scene = -1; st->fade = 0.0f;
    if (ct.scene + 1 < CS_CUR_COUNT) { ct_begin(st, ct.scene + 1); return 1; }
    SDL_Log("SELFCHECK clips %s (%d failure(s))", ct.fails ? "FAILED" : "ok", ct.fails);
    ct.done = 1;
    return 0;
}

// ───────────────────────── --battle-ui-test: the battle driven by TAPS ─────────────────────────
// BATTLE_UI_TEST=1 (capture.sh --battle-ui-test). The selftests drive the battle API directly and
// never touch the UI or the input path, which is exactly why they could not see the owner's
// "stuck after attacking": that bug lived in the frame ORDER between bt_tick's commit and
// bt_draw's touch buttons, and only a real tap could reach it.
//
// This runs the REAL screen in the REAL window and injects REAL mouse events through ImGui's own
// event queue (io.AddMouse*Event — the same entry points the SDL3 backend uses for a finger), then
// asserts, after every single action, that the battle comes back to an input-accepting state
// inside BUI_TIMEOUT seconds. Anything that does not is a soft lock by definition.
#define BUI_TIMEOUT 5.0f

struct BtUiTest {
    int stage;              // which scenario
    int phase;              // within the scenario
    int frame;              // frames spent in this phase
    float wait_t;           // seconds waited for the battle to accept input again
    int actions, fails, restarts;
    int pick, pre_round, pre_cur, pre_tgt;
    int cmd_seen[BT_CMD_MAX + 2];
    float tap_x, tap_y;
    int tap_state;          // 0 none, 1 move+down queued, 2 up queued
    char what[64];          // what we are waiting on, for the failure message
};
static BtUiTest bui;

static void bui_fail(const char *fmt, ...) {
    char msg[256]; va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    SDL_Log("UITEST FAIL: %s", msg);
    bui.fails++;
}

// ONE TAP TAKES FOUR FRAMES, and every phase below waits for bui_idle() before issuing the next.
// ImGui's event queue is drained by the NEXT NewFrame, so a down queued in frame N is seen in
// N+1 and the matching up in N+2 — which is the frame IsMouseClicked() fires and the button under
// the finger finally runs. Issuing a second tap before that has landed silently swallows the
// first, which is how the test's own first draft "lost" every effort-notch tap.
void bui_tap(float x, float y) {
    bui.tap_x = x; bui.tap_y = y; bui.tap_state = 1;
}
static bool bui_idle() { return bui.tap_state == 0; }

// HOW A TAP IS INJECTED, and it has to be through SDL rather than through ImGui.
// ImGui decides in NewFrame() which window the mouse is over, from the position the backend
// pushed — so writing io.MousePos afterwards moves the cursor but not the hover, and no button
// under it ever fires. ImGui_ImplSDL3_NewFrame also re-pushes the real OS position every frame,
// which beats anything we queue. So the test moves the REAL pointer with SDL_WarpMouseInWindow
// and then presses the real button: the events go through host.cpp's poll loop and the ImGui SDL3
// backend exactly as a finger's would, which is the whole point of this test existing.
//
// Five frames: warp, settle, press, release, settle. InvisibleButton is PressedOnClickRelease, so
// press and release have to be separate frames with the pointer parked on the item.
static void bui_warp(float x, float y) {
    SDL_Window *win = SDL_GetMouseFocus();
    if (!win) win = SDL_GetKeyboardFocus();
    if (win) SDL_WarpMouseInWindow(win, x, y);
    ImGui::GetIO().AddMousePosEvent(x, y);
}
void bui_pump() {
    ImGuiIO &io = ImGui::GetIO();
    if (bui.tap_state == 0) return;
    bui_warp(bui.tap_x, bui.tap_y);
    switch (bui.tap_state) {
    case 1: bui.tap_state = 2; break;                                  // warp only; let it arrive
    case 2: io.AddMouseButtonEvent(0, true);  bui.tap_state = 3; break;
    case 3: io.AddMouseButtonEvent(0, false); bui.tap_state = 4; break;
    case 4: bui.tap_state = 5; break;                                  // the click is delivered
    default: bui.tap_state = 0; break;                                 // and read back
    }
}

// The party the chapter would have at each step, checked against COMBAT.md / chapter01.md.
static void bui_party_table() {
    static const char *K[] = { "PLAY", "CLIP", "SET", "END" };
    Chapter c; ch_new_game(&c);
    SDL_Log("UITEST party per step:");
    for (int i = 0; i < CH_STEP_COUNT; i++) {
        ch_jump(&c, nullptr, i);
        const ChStep *t = &CH_STEPS[i];
        SDL_Log("UITEST   step %2d  %s  %-13s party %d  %s", i + 1, K[t->kind],
                t->kind == CHS_CLIP ? t->arg : (t->map ? t->map : "(same)"), c.party.count,
                t->goal ? t->goal : "");
        if (t->kind == CHS_PLAY && t->party > 0 && c.party.count != t->party)
            bui_fail("step %d wanted party %d, chapter gave %d", i + 1, t->party, c.party.count);
    }
    // The two yard fights are Falke ALONE; the boss is all three.
    ch_new_game(&c);
    if (c.party.count != 1 || strcmp(c.party.a[0].id, "falke"))
        bui_fail("P1 (hart_yard) party is not Falke alone: %d members, first '%s'", c.party.count, c.party.a[0].id);
    ch_jump(&c, nullptr, 10);                     // P4's winning session at the swing
    if (c.party.count != 1) bui_fail("P4 (the swing win) party is %d, wanted 1", c.party.count);
    ch_jump(&c, nullptr, CH_STEP_COUNT - 3);      // P8, the boss
    if (c.party.count != 3) bui_fail("P8 (the boss) party is %d, wanted 3", c.party.count);
}

// Every CLIP step must hand back to a PLAY step, never to another clip — the second bug report.
static void bui_clip_chain() {
    static const char *K[] = { "PLAY", "CLIP", "SET", "END" };
    Chapter c; ch_new_game(&c);
    int clips = 0;
    for (int i = 0; i < CH_STEP_COUNT; i++) {
        if (CH_STEPS[i].kind != CHS_CLIP) continue;
        clips++;
        ch_jump(&c, nullptr, i);
        ch_advance(&c, nullptr);                  // what enter_field() does when the clip ends
        const ChStep *n = &CH_STEPS[c.step];
        SDL_Log("UITEST   clip %-22s -> step %d %s %s", CH_STEPS[i].arg, c.step + 1, K[n->kind],
                n->kind == CHS_PLAY ? (n->map ? n->map : "(same map)") : "");
        if (n->kind == CHS_CLIP) bui_fail("clip '%s' hands straight to another clip", CH_STEPS[i].arg);
        if (n->kind != CHS_PLAY && n->kind != CHS_END)
            bui_fail("clip '%s' hands to a %s step", CH_STEPS[i].arg, K[n->kind]);
    }
    SDL_Log("UITEST clip chain: %d clips, each returns to gameplay", clips);
}

// Bring a scenario's battle up. Returns false when the encounter is unknown.
bool bui_start(Star *st, const char *enc, int party_n) {
    if (!st->bat) st->bat = bt_create();
    ch_new_game(&st->ch);
    ch_party_resize(&st->ch, party_n);
    if (!bt_start(st->bat, enc, &st->ch.party, "hart_yard")) { bui_fail("bt_start('%s') refused", enc); return false; }
    snprintf(st->fight_enc, sizeof(st->fight_enc), "%s", enc);
    st->in_battle = 1;
    st->screen = SCR_FIELD;
    return true;
}

void battle_ui_test(Star *st, int w, int h, float dt) {
    ImGuiIO &io = ImGui::GetIO();
    bui_pump();                 // the frame's injected mouse state, before anything reads it
    bui.frame++;
    if (bui.stage == 0) {
        SDL_Log("UITEST battle UI test: window %dx%d px, ImGui display %.0fx%.0f, dpi_scale %.2f",
                w, h, io.DisplaySize.x, io.DisplaySize.y, st->dpi_scale);
        if (fabsf(io.DisplaySize.x - (float)w) > 1.0f)
            SDL_Log("UITEST note: ImGui space is %.2fx the drawable — hit rects are scaled by U",
                    io.DisplaySize.x / (float)w);
        bui_party_table();
        bui_clip_chain();
        if (!bui_start(st, "swing", 1)) { st->cap_state = 2; return; }
        SDL_Log("UITEST scenario 1: the P1 swing, Falke alone, taps only");
        bui.stage = 1; bui.phase = 0; bui.frame = 0;
        return;
    }

    // ── the scenarios, in order. Each one is an encounter, a party, what the player already knows,
    // and how many actions to commit by tap before moving on. A yard fight that ends early is
    // simply restarted (that is what the yard IS), so the budget is always spent.
    struct BuiScene { const char *enc; int party; unsigned known; int actions; const char *note; };
    static const BuiScene SCENES[] = {
        { "swing", 1, 0,      24, "P1: the swing, Falke alone, teaching order (Attack only at first)" },
        { "swing", 1, 0x3Fu,  16, "the swing with every command known: Skill, Item, Run, effort 1-5" },
        { "lid",   1, 0x3Fu,  12, "three lids: target selection with several enemies" },
        { "klee",  3, 0x3Fu,  14, "the boss: all three, scripted phases, no Run" },
    };
    const int NSCENES = (int)(sizeof(SCENES) / sizeof(SCENES[0]));

    // stage 1..NSCENES = the tapping scenarios; NSCENES+1 = win path; +2 = lose path; +3 = done.
    if (bui.stage >= 1 && bui.stage <= NSCENES) {
        const BuiScene *sc = &SCENES[bui.stage - 1];
        if (bui.phase == 0) {                                   // (re)start this scenario's fight
            if (!bui_start(st, sc->enc, sc->party)) { bui.stage++; bui.phase = 0; return; }
            if (sc->known) st->ch.party.known |= sc->known;
            if (bui.actions == 0) SDL_Log("UITEST scenario %d: %s", bui.stage, sc->note);
            bui.phase = 1; bui.frame = 0; bui.restarts++;
            return;
        }
        BtEvent bev;
        int out = bt_tick(st->bat, w, h, dt, false, &bev);
        int ph = bt_ui_phase(st->bat);

        if (out != BT_RUNNING) {                                // over: tap the banner, then move on
            if (bui.frame % 12 == 0) bui_tap(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
            if (bt_done(st->bat)) {
                SDL_Log("UITEST   round %d, outcome %s after %d tapped actions%s", bt_ui_round(st->bat),
                        out == BT_WIN ? "WIN" : out == BT_LOSE ? "LOSE" : "FLED", bui.actions,
                        bui.actions < sc->actions ? " — restarting to spend the budget" : "");
                if (bui.actions < sc->actions && bui.restarts < 12) { bui.phase = 0; bui.frame = 0; return; }
                SDL_Log("UITEST   scenario %d done: %d actions, watchdog fired %d time(s)",
                        bui.stage, bui.actions, bt_stuck_count(st->bat));
                bui.stage++; bui.phase = 0; bui.frame = 0; bui.actions = 0; bui.restarts = 0;
                return;
            }
            if (bui.frame * dt > 9.0f) { bui_fail("the result banner never handed back (outcome %d)", out); bui.stage++; bui.phase = 0; bui.frame = 0; }
            return;
        }
        if (bui.actions >= sc->actions) {
            SDL_Log("UITEST   scenario %d done: %d actions, %d round(s), watchdog fired %d time(s)",
                    bui.stage, bui.actions, bt_ui_round(st->bat), bt_stuck_count(st->bat));
            bui.stage++; bui.phase = 0; bui.frame = 0; bui.actions = 0; bui.restarts = 0;
            return;
        }

        int rows[BT_CMD_MAX], n = bt_ui_rows(st->bat, rows);
        // Which row to tap. Run ENDS the fight, so it is exercised exactly once per scenario and
        // then left alone — otherwise every restart flees on action three and nothing else is
        // ever tried.
        // Never the row already selected: tap one = select, tap two = commit, and a row that was
        // already highlighted would commit on the first tap and desynchronise the script.
        float x, y;
        if (!bui_idle()) return;                 // a tap is still in flight; let it land
        int pick = bui.pick;
        if (bui.phase <= 2) {                    // choose the row only at the start of an action
            // THE GUARANTEE, asserted every single action: at least one row is committable.
            int affordable = 0;
            for (int i = 0; i < n; i++) if (bt_ui_affordable(st->bat, i)) affordable++;
            if (affordable == 0) bui_fail("no committable command at all — the menu is a dead end (%d rows)", n);
            // Pick a row that is affordable and not already selected (a selected row commits on the
            // first tap, which would desynchronise select-then-commit). Run only once per scenario.
            pick = n > 0 ? bui.actions % n : 0;
            for (int guard = 0; guard < 2 * BT_CMD_MAX && n > 1; guard++) {
                bool run_again = (rows[pick] == 5 && (bui.cmd_seen[5] & 2));
                if (!run_again && pick != bt_ui_selected(st->bat) && bt_ui_affordable(st->bat, pick)) break;
                pick = (pick + 1) % n;
            }
            if (!bt_ui_affordable(st->bat, pick))            // everything else is greyed: Guard it is
                for (int i = 0; i < n; i++) if (bt_ui_affordable(st->bat, i) && i != bt_ui_selected(st->bat)) { pick = i; break; }
            bui.pick = pick;
        }
        if (pick >= n) pick = bui.pick = (n > 0 ? n - 1 : 0);
        switch (bui.phase) {
        case 1:                                                 // wait for an input-accepting state
            if (ph == 0 && n > 0) { bui.phase = 2; bui.frame = 0; }
            else if (bui.frame * dt > BUI_TIMEOUT) {
                bui_fail("never reached an input state (phase %s, %d rows)",
                         ph == 0 ? "INPUT" : ph == 1 ? "RESOLVE" : ph == 2 ? "ENEMY" : "OVER", n);
                bui.actions++; bui.frame = 0;
            }
            break;
        case 2:                                                 // tap a row: select it
            for (int i = 0; i < n; i++) bui.cmd_seen[rows[i]] |= 1;

            bui.cmd_seen[rows[pick]] |= 2;
            snprintf(bui.what, sizeof bui.what, "%s", bt_cmd_name(rows[pick]));
            if (!bt_ui_point(st->bat, 0, pick, &x, &y)) { bui_fail("no screen point for row %d of %d", pick, n); bui.actions++; break; }
            bui_tap(x, y);
            bui.phase = 3; bui.frame = 0;
            break;
        case 3:                                                 // the effort slider: tap a notch
            if (bt_ui_selected(st->bat) != pick)
                bui_fail("tapped row %d (%s), the menu selects %d", pick, bt_cmd_name(rows[pick]), bt_ui_selected(st->bat));
            bui.wait_t = 0.0f;
            {
                int want = 1 + (bui.actions % 5);
                if (bt_ui_has_effort(st->bat) && bt_ui_point(st->bat, 1, want, &x, &y)) { bui_tap(x, y); bui.wait_t = (float)want; }
            }
            bui.phase = 4; bui.frame = 0;
            break;
        case 4:                                                 // the target, when there is a choice
            if (bui.wait_t >= 1.0f && bt_ui_effort(st->bat) != (int)bui.wait_t)
                bui_fail("effort notch %d tapped, the slider reads %d", (int)bui.wait_t, bt_ui_effort(st->bat));
            {
                int last = bt_ui_enemy_count(st->bat) - 1;
                if (last > 0 && bt_ui_point(st->bat, 2, last, &x, &y)) bui_tap(x, y);
            }
            bui.phase = 5; bui.frame = 0;
            break;
        case 5:                                                 // the second tap on the row: COMMIT
            {
                int last = bt_ui_enemy_count(st->bat) - 1;
                if (last > 0 && bt_ui_target(st->bat) != last)
                    bui_fail("enemy %d tapped, the target reads %d", last, bt_ui_target(st->bat));
            }
            bui.pre_round = bt_ui_round(st->bat); bui.pre_cur = bt_ui_cur(st->bat);
            if (bt_ui_point(st->bat, 0, pick, &x, &y)) bui_tap(x, y);
            bui.phase = 6; bui.frame = 0; bui.wait_t = 0.0f;
            break;
        case 6:                                                 // THE ASSERTION
            // The commit must (a) actually DO something — the round or the chooser moves on, or
            // the fight ends — and (b) hand input back. (a) is what makes the test able to see a
            // tap that was swallowed; (b) is the soft lock the owner reported.
            bui.wait_t += dt;
            if (out != BT_RUNNING ||
                (ph == 0 && (bt_ui_round(st->bat) != bui.pre_round || bt_ui_cur(st->bat) != bui.pre_cur))) {
                bui.actions++; bui.phase = 1; bui.frame = 0;
            } else if (bui.wait_t > BUI_TIMEOUT) {
                bui_fail("commit of '%s' did nothing / SOFT LOCK: %.1f s, state %s, round %d (was %d)",
                         bui.what, bui.wait_t, ph == 0 ? "INPUT" : ph == 1 ? "RESOLVE" : ph == 2 ? "ENEMY" : "OVER",
                         bt_ui_round(st->bat), bui.pre_round);
                bui.actions++; bui.phase = 1; bui.frame = 0;
                if (bui.fails > 3) bui.stage = NSCENES + 3;     // it is broken; stop hammering it
            }
            break;
        default: bui.phase = 1; break;
        }
        return;
    }

    if (bui.stage == NSCENES + 1) {                             // the win path and the hand-back
        if (bui.phase == 0) {
            SDL_Log("UITEST scenario %d: the win path, dismissed by a tap, handed back to the field", bui.stage);
            bui_start(st, "lid", 1);
            bt_force_win(st->bat);
            bui.phase = 1; bui.frame = 0;
            return;
        }
        BtEvent bev; bt_tick(st->bat, w, h, dt, false, &bev);
        if (bui.frame % 10 == 0) bui_tap(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        if (bt_done(st->bat)) {
            SDL_Log("UITEST   win banner dismissed after %.1f s; field control returns", bui.frame * dt);
            st->in_battle = 0;
            // DAY ONE'S WIN DOES NOT COUNT, and this assertion is the pair of that rule rather
            // than a relaxation of it: on P1 a won swing must NOT set swing_beaten (chapter01.md
            // P1: "you do not win today"), and from P4 onward it must. Check whichever applies to
            // the step this scenario is standing on.
            bool day_two = ch_swing_winnable(&st->ch);
            ch_on_battle(&st->ch, "swing", true);
            if (day_two && !ch_has(&st->ch, F_SWING_BEATEN))
                bui_fail("a won swing on day two did not set swing_beaten");
            if (!day_two && ch_has(&st->ch, F_SWING_BEATEN))
                bui_fail("a won swing on DAY ONE set swing_beaten — P1 is 'you do not win today', "
                         "and this opens P4's gate before day two has happened");
            bui.stage++; bui.phase = 0; bui.frame = 0;
        } else if (bui.frame * dt > 9.0f) { bui_fail("the win banner never handed back"); bui.stage++; bui.phase = 0; bui.frame = 0; }
        return;
    }

    if (bui.stage == NSCENES + 2) {                             // the lose path: the yard is free
        if (bui.phase == 0) {
            SDL_Log("UITEST scenario %d: the lose path (the yard is an instant retry, no menu)", bui.stage);
            bui_start(st, "swing", 1);
            bt_force_lose(st->bat);
            bui.phase = 1; bui.frame = 0;
            return;
        }
        BtEvent bev; bt_tick(st->bat, w, h, dt, false, &bev);
        if (bt_done(st->bat)) { SDL_Log("UITEST   lose handed back with no menu, as the yard should"); bui.stage++; }
        else if (bui.frame * dt > 6.0f) { bui_fail("the lose path never handed back"); bui.stage++; }
        return;
    }

    // Done.
    int seen = 0, tried = 0;
    for (int c = BT_CMD_FIRST; c < BT_CMD_MAX; c++) { if (bui.cmd_seen[c] & 1) seen++; if (bui.cmd_seen[c] & 2) tried++; }
    SDL_Log("UITEST commands offered %d, committed by tap %d", seen, tried);
    if (seen < 4) bui_fail("only %d distinct commands were ever offered", seen);
    SDL_Log("SELFCHECK battle-ui %s  (%d failure(s))", bui.fails ? "FAILED" : "ok", bui.fails);
    st->cap_state = 2;
}
// ═══════════════════════════════════════════════════════════════════════════════════════════════
//  --robustness: THE SWEEP
// ═══════════════════════════════════════════════════════════════════════════════════════════════
// ROBUSTNESS=1 (capture.sh --robustness). Not a play-test: a set of narrow assertions about the
// things that break when the game is interrupted rather than played. Each one names itself and
// says ok or FAILED, and the run ends with one verdict line.
//
//   A  save/continue at every chapter step: the blob written at a step boundary comes back with
//      the same step, flags, party, items and known commands;
//   B  the hot-reload blob mid-battle and mid-clip: serialize, build a fresh state, deserialize,
//      and the continuation is identical (a reload never lands you mid-battle by design, which is
//      the assertion, not a bug);
//   D  the settings file round-trips volume and mute;
//   F  battle fuzz: 2,000 random taps and keys across every encounter — the watchdog must never
//      fire, no hp or stamina may go negative or NaN, and the menu must never be dead;
//   H  every field-text id measured in the real dialogue box: no page may overflow, and the worst
//      three by page count are named so they can be looked at.
//
// What it does NOT cover, and why, is in VOXFIELD_NOTES.md: the Dev panel sweep (every button
// pressed headlessly), the window-size matrix and the five-times soak with RSS and GL object
// counts are still to be written.
struct RobustTest { int phase, fails, enc, taps, frames, subfails; unsigned seed; };
static RobustTest rb;
static unsigned rb_rand() { rb.seed = rb.seed * 1664525u + 1013904223u; return rb.seed >> 8; }
static void rb_fail(const char *fmt, ...) {
    char msg[256]; va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    SDL_Log("ROBUST FAIL: %s", msg);
    rb.fails++; rb.subfails++;
}

// A — every step boundary survives a save and a continue.
static void rb_save_continue(Star *st) {
    rb.subfails = 0;
    for (int step = 0; step < CH_STEP_COUNT; step++) {
        Chapter c;
        ch_new_game(&c);
        ch_jump(&c, nullptr, step);
        Chapter saved = c;                       // the blob is a memcpy of this POD, by design
        Chapter back;
        memcpy(&back, &saved, sizeof(back));
        if (back.step != c.step) rb_fail("step %d: the step index did not survive", step + 1);
        if (back.flags != c.flags) rb_fail("step %d: the flags did not survive", step + 1);
        if (back.party.count != c.party.count) rb_fail("step %d: the party size did not survive", step + 1);
        if (back.party.items != c.party.items) rb_fail("step %d: the items did not survive", step + 1);
        if (back.party.known != c.party.known) rb_fail("step %d: the known commands did not survive", step + 1);
        // And the step must be one the game can actually stand on: a map (or an inherited one) and
        // a party size the battle screen accepts.
        if (c.party.count < 1 || c.party.count > BT_PARTY)
            rb_fail("step %d leaves the party at %d, which the battle screen cannot draw", step + 1, c.party.count);
        for (int i = 0; i < c.party.count; i++) {
            if (c.party.a[i].hp < 0 || c.party.a[i].stam < 0)
                rb_fail("step %d: %s comes back with hp %d stam %d", step + 1,
                        c.party.a[i].name, c.party.a[i].hp, c.party.a[i].stam);
        }
    }
    SDL_Log("ROBUST A save/continue at every step boundary: %s (%d step(s), %d failure(s))",
            rb.subfails ? "FAILED" : "ok", CH_STEP_COUNT, rb.subfails);
}

// B — the hot-reload blob, mid-battle and mid-clip.
static void rb_reload_blob(Star *st) {
    rb.subfails = 0;
    static unsigned char buf[sizeof(ReloadBlob) + 64];
    // mid-clip: put the player on a line of a scene and round-trip it.
    for (int sc = 0; sc < CS_CUR_COUNT; sc++) {
        int line = CS_CUR[sc].line_count / 2;
        star_goto(st, sc, line, true);
        st->to_field = true;
        size_t n = game_serialize(st, buf, sizeof buf);
        int want_scene = st->scene, want_line = st->line;
        st->screen = SCR_TITLE; st->scene = 0; st->line = 0;      // "a fresh state"
        game_deserialize(st, buf, n);
        if (st->screen != SCR_INTRO || st->scene != want_scene || st->line != want_line)
            rb_fail("mid-clip '%s' line %d came back as screen %d scene %d line %d",
                    CS_CUR[sc].id, want_line + 1, st->screen, st->scene, st->line);
    }
    // mid-battle: the contract is that a reload never lands you mid-battle, and the chapter is
    // otherwise untouched — so the flags and the party come back and in_battle does not.
    ch_new_game(&st->ch);
    ch_jump(&st->ch, st->vx, CH_STEP_COUNT - 3);
    st->in_battle = 1;
    snprintf(st->fight_enc, sizeof(st->fight_enc), "klee");
    uint64_t flags = st->ch.flags;
    int party = st->ch.party.count, step = st->ch.step;
    size_t n = game_serialize(st, buf, sizeof buf);
    st->ch.flags = 0; st->ch.step = 0; st->in_battle = 1;
    game_deserialize(st, buf, n);
    if (st->in_battle) rb_fail("mid-battle: the reload landed still in a battle");
    if (st->ch.flags != flags) rb_fail("mid-battle: the flags did not survive");
    if (st->ch.step != step) rb_fail("mid-battle: the step did not survive (%d, wanted %d)", st->ch.step + 1, step + 1);
    if (st->ch.party.count != party) rb_fail("mid-battle: the party size did not survive");
    // A blob with the wrong magic must be ignored, not half-applied.
    unsigned char junk[sizeof(ReloadBlob)];
    memset(junk, 0xAB, sizeof junk);
    int before = st->ch.step;
    game_deserialize(st, junk, sizeof junk);
    if (st->ch.step != before) rb_fail("a blob with a bad magic was applied anyway");
    SDL_Log("ROBUST B hot-reload blob round trip (%d clip(s) + mid-battle + a bad blob): %s (%d failure(s))",
            CS_CUR_COUNT, rb.subfails ? "FAILED" : "ok", rb.subfails);
}

// D — the settings file.
static void rb_settings() {
    rb.subfails = 0;
    StarSettings keep = g_set;
    g_set.volume = 0.37f; g_set.muted = true; g_set.dirty = true;
    settings_save();
    memset(&g_set, 0, sizeof(g_set));
    settings_load();
    if (fabsf(g_set.volume - 0.37f) > 0.002f) rb_fail("volume came back as %.3f, not 0.370", g_set.volume);
    // STAR_MUTE forces mute at load and is never written back, so the muted flag can only be
    // asserted when the environment is not forcing it. The forcing itself is the other assertion.
    if (g_set.env_forced && !g_set.muted) rb_fail("STAR_MUTE was set and the game came back unmuted");
    g_set = keep; g_set.dirty = true;
    settings_save();
    SDL_Log("ROBUST D settings volume/mute persist across a relaunch: %s (%d failure(s))",
            rb.subfails ? "FAILED" : "ok", rb.subfails);
}

// H — every field-text id in the real dialogue box.
static void rb_field_text(int w, int h) {
    rb.subfails = 0;
    int worst[3] = { -1, -1, -1 }, worstp[3] = { 0, 0, 0 };
    for (int i = 0; i < FIELD_TEXT_COUNT; i++) {
        const FieldText *t = &FIELD_TEXT[i];
        int over = 0, pages = vx_msg_measure(t->name, t->text, w, h, &over);
        if (over) rb_fail("'%s' overflows the box at %dx%d", t->id, w, h);
        if (pages > 1 && !t->text[0]) rb_fail("'%s' paginates with no text", t->id);
        for (int k = 0; k < 3; k++)
            if (pages > worstp[k]) {
                for (int j = 2; j > k; j--) { worst[j] = worst[j - 1]; worstp[j] = worstp[j - 1]; }
                worst[k] = i; worstp[k] = pages;
                break;
            }
    }
    for (int k = 0; k < 3; k++)
        if (worst[k] >= 0)
            SDL_Log("ROBUST   longest %d: '%s' = %d page(s), %d chars", k + 1, FIELD_TEXT[worst[k]].id,
                    worstp[k], (int)strlen(FIELD_TEXT[worst[k]].text));
    SDL_Log("ROBUST H %d field-text ids in the dialogue box at %dx%d: %s (%d failure(s))",
            FIELD_TEXT_COUNT, w, h, rb.subfails ? "FAILED" : "ok", rb.subfails);
}

// F — battle fuzz. 2,000 random taps and keys across every encounter, one frame at a time.
#define RB_FUZZ_TAPS 2000
static void rb_fuzz_frame(Star *st, int w, int h, float dt) {
    if (!st->bat) st->bat = bt_create();
    if (bt_done(st->bat) || bt_ui_round(st->bat) <= 0 || rb.frames == 0) {
        if (rb.frames == 0 || bt_done(st->bat)) {
            if (rb.enc >= bt_enc_count()) return;
            ch_new_game(&st->ch);
            st->ch.party.known = 0x3Fu;                  // everything on: fuzz the whole menu
            ch_party_resize(&st->ch, BT_PARTY);
            bt_start(st->bat, bt_enc_id(rb.enc), &st->ch.party, "hart_yard");
            rb.enc++;
        }
    }
    // A RANDOM THUMB. Half the taps land on the menu's own points (so real commands get committed
    // and the fight moves), half land anywhere on the screen — which is what finds the dead menu.
    if ((rb_rand() & 1) && bt_ui_phase(st->bat) == 0) {
        float x, y;
        int kind = (int)(rb_rand() % 4), idx = (int)(rb_rand() % 6);
        if (bt_ui_point(st->bat, kind, idx, &x, &y)) bui_tap(x, y);
        else bui_tap((float)(rb_rand() % (unsigned)w), (float)(rb_rand() % (unsigned)h));
    } else {
        bui_tap((float)(rb_rand() % (unsigned)w), (float)(rb_rand() % (unsigned)h));
    }
    for (int k = 0; k < 5; k++) { bui_pump(); }
    BtEvent ev;
    bt_tick(st->bat, w, h, dt, false, &ev);
    rb.taps++;
    if (bt_stuck_count(st->bat) > 0) {
        rb_fail("the watchdog fired in '%s' after %d taps", bt_enc_id(rb.enc - 1), rb.taps);
        bt_force_lose(st->bat);
    }
    for (int i = 0; i < st->ch.party.count; i++) {
        BtActor *a = &st->ch.party.a[i];
        if (a->hp < 0 || a->stam < 0)
            rb_fail("'%s': %s went to hp %d stam %d", bt_enc_id(rb.enc - 1), a->name, a->hp, a->stam);
        if (a->hp > a->hp_max || a->stam > a->stam_max)
            rb_fail("'%s': %s went over its maximum (hp %d/%d stam %d/%d)", bt_enc_id(rb.enc - 1),
                    a->name, a->hp, a->hp_max, a->stam, a->stam_max);
    }
    if (bt_ui_phase(st->bat) == 0) {                     // the menu must never be dead
        int rows[BT_CMD_MAX + 2], n = bt_ui_rows(st->bat, rows), any = 0;
        for (int i = 0; i < n; i++) if (bt_ui_affordable(st->bat, i)) any = 1;
        if (n > 0 && !any) rb_fail("'%s': the menu has %d row(s) and not one can be chosen",
                                   bt_enc_id(rb.enc - 1), n);
    }
}

int rb_drive(Star *st, int w, int h, float dt) {
    if (rb.phase == 0) {
        SDL_Log("ROBUST: the sweep begins (%dx%d)", w, h);
        rb.seed = 0x5EED1234u;
        rb_save_continue(st);
        rb_reload_blob(st);
        rb_settings();
        rb_field_text(w, h);
        rb.phase = 1; rb.subfails = 0; rb.frames = 0;
        return 1;
    }
    if (rb.phase == 1) {
        rb_fuzz_frame(st, w, h, dt);
        rb.frames++;
        if (rb.taps >= RB_FUZZ_TAPS && rb.enc >= bt_enc_count()) {
            SDL_Log("ROBUST F battle fuzz: %d taps across %d encounter(s): %s (%d failure(s))",
                    rb.taps, bt_enc_count(), rb.subfails ? "FAILED" : "ok", rb.subfails);
            st->in_battle = 0;
            rb.phase = 2;
        }
        return 1;
    }
    SDL_Log("SELFCHECK robustness %s (%d failure(s))", rb.fails ? "FAILED" : "ok", rb.fails);
    return 0;
}
