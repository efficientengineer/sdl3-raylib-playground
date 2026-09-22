// chapter.cpp — the CHAPTER's runtime: the field screen, and what the story does with it.
//
//   owns      entering and leaving the field, the fade in and out of a clip, handing the field's
//             events (a scene, an encounter zone, a pickup, a text id) to the chapter's memory, the
//             battle hand-off, the goal line, and the chapter's own logic self-test.
//   never     owns the chapter's DATA — that is chapter01.h, and it stays a header of tables — and
//             never draws the world itself: voxfield.cpp does, and hands back only what is story.
//   exposes   enter_field/field_scene/draw_field/draw_goal/scene_by_id/chapter_selftest and the two
//             flag->id lookups the play-test needs.
//   tested by ./capture.sh --chapter-logic-selftest (the table) and --chapter-playtest (the walk).
#include "star_internal.h"

// ───────────────────────── The world ─────────────────────────
// Title -> intro -> field. The field owns its own renderer, controls and dialogue box (voxfield.cpp);
// the only things it hands back are "play this talk scene" and "the player walked into an encounter
// zone", because those belong to the game, not the map.

int scene_by_id(const char *id) {
    for (int i = 0; i < CS_CUR_COUNT; i++) if (!strcmp(CS_CUR[i].id, id)) return i;
    return -1;
}

void enter_field(Star *st) {
    if (!st->vx) st->vx = vx_create();
    star_free_textures(st);                       // the page art goes; the field has its own
    st->screen = SCR_FIELD;
    st->to_field = false;
    st->from_field = false;
    mus_start(CS_WONDER);
    // Coming back from a clip: that clip WAS the current step, so the chapter moves on now. This is
    // the one place a CLIP step is retired, which is why a clip can be re-entered safely if the
    // player reloads mid-scene.
    if (st->ch.started && st->ch.step < CH_STEP_COUNT && CH_STEPS[st->ch.step].kind == CHS_CLIP) {
        ch_advance(&st->ch, st->vx);
        if (CH_STEPS[st->ch.step].kind == CHS_END) { st->screen = SCR_END; st->title_t = 0; }
    } else {
        ch_apply_step(&st->ch, st->vx);           // re-assert light, party, lantern, goal line
    }
}

// A field asked for a cutscene. If it is not in the playlist yet, say so in the field's own box
// rather than fading to a blank scene.
void field_scene(Star *st, const char *id) {
    int idx = scene_by_id(id);
    if (idx >= 0 && CS_CUR[idx].line_count > 0) { st->fade_to_scene = idx; st->from_field = true; return; }
    char msg[160];
    snprintf(msg, sizeof(msg), "(scene \"%s\" is not in the playlist yet)", id);
    if (st->vx) vx_message(st->vx, msg);
}

void draw_field(Star *st, int w, int h, float dt) {
    if (!st->vx) st->vx = vx_create();

    // ── a battle has the screen ───────────────────────────────────────────────────────────────
    // The world is not ticked while a battle runs: the party is frozen where it stood, and the
    // battle draws over everything. Winning or losing hands control straight back to that spot.
    if (st->in_battle) {
        BtEvent bev;
        int out = bt_tick(st->bat, w, h, dt, dev.open, &bev);
        if (bev.kind == BTE_TEXT) vx_say_id(st->vx, bev.arg);
        else if (bev.kind == BTE_GOAL) snprintf(st->ch.goal, sizeof(st->ch.goal), "%s", bev.arg);
        if (out != BT_RUNNING && bt_done(st->bat)) {
            st->in_battle = 0;
            vx_freeze(st->vx, 0);
            ch_on_battle(&st->ch, st->fight_enc, out == BT_WIN);
            // A won fight is consumed so walking back over the cell does not restart it. A lost
            // one is left armed: in the yard that IS the instant retry.
            //
            // EXCEPT THE THREE MACHINES. They are furniture in a yard, not an encounter: Hart
            // built them to be used again, P4 is FOUR SESSIONS at the swing on day two, and
            // consuming the swing the first time it goes down leaves the player standing in front
            // of nothing for the rest of the day with a goal line telling them to beat it. (The
            // chapter play-test found this on step 11, which is session two.)
            bool a_machine = !strcmp(st->fight_enc, "post") || !strcmp(st->fight_enc, "arm") ||
                             !strcmp(st->fight_enc, "swing");
            if (out == BT_WIN && !a_machine) vx_disable_trigger(st->vx, st->fight_enc);
        }
        return;
    }

    VxEvent ev;
    // The box the player is reading is what `vx_busy` reports, and it is read BEFORE the tick so a
    // box that opens this frame already counts: the bell must not tick on the frame a conversation
    // starts. See ch_tick — nobody reads boxes on a clock.
    bool reading = vx_busy(st->vx);
    vx_tick(st->vx, w, h, dt, dev.open, &ev);
    ch_tick(&st->ch, dt, reading || vx_busy(st->vx));

    switch (ev.kind) {
    case VXE_SCENE:  field_scene(st, ev.arg); break;
    case VXE_ZONE:   { char m[128]; snprintf(m, sizeof(m), "encounter %s", ev.arg); dev_send(m); } break;
    case VXE_TEXT:
        // THREE THINGS CAN HAPPEN TO AN EXAMINE, and the chapter decides which.
        //   * the job board before the job exists shows its ALTERNATE line and tells the chapter
        //     nothing — the board is always there, it just says a different thing;
        //   * the guild-hall counter shows the on-time or the late clerk, by the bell;
        //   * everything else is what the trigger said, and the chapter gates on it.
        {
            const char *alt = ch_trig_alt(&st->ch, ev.arg);
            if (alt) { vx_say_id(st->vx, alt); break; }
            const char *line = ch_clerk_line(&st->ch, ev.arg);
            if (strcmp(line, ev.arg)) vx_say_id(st->vx, line);
            // A FLAG CHANGED, SO THE WORLD CHANGED. Half the condition table hangs off a flag that
            // is set in the SAME step as the trigger it unlocks — taking the job sheet down is what
            // makes her door and the guild book live, and all three are inside P5's one row.
            // Re-applying only on a step change left those two inert for the whole of the bell run,
            // which is precisely the step they exist for.
            if (ch_on_text(&st->ch, line)) ch_apply_triggers(&st->ch, st->vx);
        }
        break;
    case VXE_PICKUP: if (ch_on_pickup(&st->ch, ev.arg)) ch_apply_triggers(&st->ch, st->vx); break;
    case VXE_GOAL:   snprintf(st->ch.goal, sizeof(st->ch.goal), "%s", ev.arg); break;
    case VXE_MAP:    ch_on_map(&st->ch, ev.arg); break;
    case VXE_FIGHT:
        if (!st->ch.skip_fights) {
            if (!st->bat) st->bat = bt_create();
            if (bt_start(st->bat, ev.arg, &st->ch.party, vx_current_map(st->vx))) {
                snprintf(st->fight_enc, sizeof(st->fight_enc), "%s", ev.arg);
                st->in_battle = 1;
                vx_freeze(st->vx, 1);
            }
        } else {                                   // Dev "skip fights": count it as a clean win
            ch_on_battle(&st->ch, ev.arg, true);
            vx_disable_trigger(st->vx, ev.arg);
        }
        break;
    default: break;
    }

    // ── the chapter script ────────────────────────────────────────────────────────────────────
    // One rule: when the current step's required interactions have all happened, move on. A CLIP
    // step plays its scene and comes back here; an END step shows the card.
    if (st->ch.started && !vx_busy(st->vx) && ch_step_complete(&st->ch)) {
        const ChStep *s = &CH_STEPS[st->ch.step];
        if (s->kind == CHS_PLAY) {
            ch_advance(&st->ch, st->vx);
            const ChStep *n = &CH_STEPS[st->ch.step];
            if (n->kind == CHS_CLIP) field_scene(st, n->arg);
            else if (n->kind == CHS_END) { st->screen = SCR_END; st->title_t = 0; }
        }
    }
}

// The goal line: six words at most, at the top of the screen, one at a time. STYLE.md rule 2 — the
// player should never have to be told twice what they are doing. Beside it, when the bell is
// running, the rings left: a counter, not a threat, because the run cannot be failed.
void draw_goal(Star *st, int w, int h) {
    if (st->in_battle || !st->ch.started || !st->ch.goal[0]) return;
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();
    float m = (float)(w < h ? w : h), s = m * 0.042f;
    ImVec2 z = font->CalcTextSizeA(s, FLT_MAX, 0, st->ch.goal);
    float x = (w - z.x) * 0.5f, y = h * 0.035f;
    dl->AddRectFilled(ImVec2(x - s * 0.7f, y - s * 0.28f), ImVec2(x + z.x + s * 0.7f, y + z.y + s * 0.28f),
                      IM_COL32(0, 0, 0, 120), s * 0.3f);
    dl->AddText(font, s, ImVec2(x + 2, y + 2), IM_COL32(0, 0, 0, 200), st->ch.goal);
    dl->AddText(font, s, ImVec2(x, y), IM_COL32(240, 225, 160, 255), st->ch.goal);
    if (st->ch.bell_started) {
        char bell[32];
        int left = CH_BELL_RINGS - st->ch.rings;
        snprintf(bell, sizeof(bell), "%d", left < 0 ? 0 : left);
        ImVec2 bz = font->CalcTextSizeA(s, FLT_MAX, 0, bell);
        dl->AddText(font, s, ImVec2(x + z.x + s * 1.6f, y),
                    left > 0 ? IM_COL32(200, 210, 240, 255) : IM_COL32(200, 120, 110, 255), bell);
        (void)bz;
    }
}

// Put the chapter on a step from the Dev panel. This is the ONLY way the owner moves the story by
// hand now, and it is chapter-shaped rather than scene-shaped:
//   * ch_jump sets every flag the steps before the target gated on, so a skipped gate stays open;
//   * a PLAY step drops you into that map/spawn/light/party with its goal line up;
//   * a CLIP step plays its scene — and because enter_field() retires a CLIP step when the scene
//     ends, tapping through it lands on the PLAY step AFTER it, with gameplay, not on another clip.
// ───────────────────────── the chapter LOGIC self-test ─────────────────────────
// CHAPTER_LOGIC_SELFTEST=1 (capture.sh --chapter-logic-selftest). Plays the whole chapter through
// the SAME entry points the game uses — ch_on_text, ch_on_pickup, ch_on_battle, ch_advance — with
// no window, no input and no world, and asserts it reaches the end card with all ten mandatory
// flags set. It takes milliseconds and it is what you run after editing CH_STEPS.
//
// WHAT IT DOES NOT PROVE, AND WHY THERE IS A SECOND TEST. It sets the flags itself. It cannot tell
// you that a player can walk to the trigger that sets one, that the trigger exists on the map at
// all, that it fires in the right step, that a hidden item is actually hidden, or that the swing
// can be lost on day one and won on day two. For two months `F_BED`, `F_SIGNED` and `F_BODY` gated
// three steps with NO MAP TRIGGER ANYWHERE that could set them, and this test passed every time,
// because it typed the answers in itself. That is what --chapter-playtest below is for.
const char *ch_text_for_flag(int flag) {
    for (int i = 0; i < CH_BIND_COUNT; i++) if (CH_BINDS[i].flag == flag) return CH_BINDS[i].text_id;
    return nullptr;
}
const char *ch_item_for_flag(int flag) {
    for (int i = 0; i < CH_ITEM_BIND_COUNT; i++) if (CH_ITEM_BINDS[i].flag == flag) return CH_ITEM_BINDS[i].text_id;
    return nullptr;
}

int chapter_selftest() {
    Chapter c;
    ch_new_game(&c);
    int fails = ch_steps_selfcheck(), clips = 0, battles = 0;
    SDL_Log("CHAPTER %d steps, %d flags (%d mandatory)", CH_STEP_COUNT, CH_FLAG_COUNT, CH_MANDATORY);
    for (int guard = 0; guard < CH_STEP_COUNT * 4; guard++) {
        const ChStep *s = &CH_STEPS[c.step];
        if (s->kind == CHS_END) break;
        if (s->kind == CHS_CLIP) {
            // A clip must name a scene that actually exists in cutscene_data.h, or the chapter
            // stalls on the phone with "(scene ... is not in the playlist yet)".
            int idx = -1;
            for (int i = 0; i < CS_CUR_COUNT; i++) if (!strcmp(CS_CUR[i].id, s->arg)) idx = i;
            if (idx < 0) { SDL_Log("FAIL step %d: CLIP '%s' is not in cutscene_data.h", c.step + 1, s->arg); fails++; }
            else if (CS_CUR[idx].line_count <= 0) { SDL_Log("FAIL step %d: CLIP '%s' has no lines", c.step + 1, s->arg); fails++; }
            else { SDL_Log("  step %2d  CLIP  %-22s %d lines, %d panels", c.step + 1, s->arg,
                           CS_CUR[idx].line_count, CS_CUR[idx].panel_count); clips++; }
            ch_advance(&c, nullptr);
            continue;
        }
        // A PLAY step: do each thing the step is waiting for, the way the world would report it.
        SDL_Log("  step %2d  PLAY  %-22s goal: %s", c.step + 1, s->map ? s->map : "(same)",
                s->goal ? s->goal : "(unchanged)");
        for (int i = 0; i < CH_NEED && s->need[i] >= 0; i++) {
            int f = s->need[i];
            if (ch_has(&c, f)) continue;
            const char *tid = ch_text_for_flag(f), *iid = ch_item_for_flag(f);
            if (tid) { ch_on_text(&c, tid); SDL_Log("          examine  %s", tid); }
            else if (iid) { ch_on_pickup(&c, iid); SDL_Log("          pick up  %s", iid); }
            else if (f == F_SWING_TRIED) { ch_on_battle(&c, "swing", false); battles++; SDL_Log("          fight    swing (lost: P1 is not won)"); }
            // P4's three session flags and the win are all "have another go at the swing". The
            // first three are LOSSES, because day two is four sessions and the last one is the win.
            else if (f == F_DAY2_1 || f == F_DAY2_2 || f == F_DAY2_3) {
                ch_on_battle(&c, "swing", false); battles++;
                SDL_Log("          fight    swing (day-two session %d, lost)", c.swing_sessions);
            }
            else if (f == F_SWING_BEATEN) { ch_on_battle(&c, "swing", true); battles++; SDL_Log("          fight    swing (won: the parry lands, at last light)"); }
            else if (f == F_BOSS_DEAD) { ch_on_battle(&c, "klee", true); battles++; SDL_Log("          fight    klee (boss)"); }
            else if (f == F_AT_PASTURE) { ch_on_map(&c, "high_pasture"); SDL_Log("          walk to  high_pasture"); }
            else if (f == F_LEFT_YARD) { c.bell_armed = 1; ch_on_map(&c, "halm"); SDL_Log("          leave the yard: the bell starts"); }
            else { ch_set(&c, f); SDL_Log("          (no binding for flag '%s' — forced)", CH_FLAG_NAME[f]); fails++; }
            if (!ch_has(&c, f)) { SDL_Log("FAIL step %d: '%s' did not set", c.step + 1, CH_FLAG_NAME[f]); fails++; ch_set(&c, f); }
        }
        if (!ch_step_complete(&c)) { SDL_Log("FAIL step %d did not complete", c.step + 1); fails++; break; }
        ch_advance(&c, nullptr);
    }
    if (CH_STEPS[c.step].kind != CHS_END) { SDL_Log("FAIL: never reached the end card (stopped at step %d)", c.step + 1); fails++; }
    int missing = ch_mandatory_missing(&c);
    if (missing) {
        SDL_Log("FAIL: %d of the mandatory ten never fired:", missing);
        for (int i = 0; i < CH_MANDATORY; i++) if (!ch_has(&c, i)) SDL_Log("        %s", CH_FLAG_NAME[i]);
        fails += missing;
    }
    // The bell must be a soft outcome: running out of rings is the clerk's late line, never a stop.
    if (c.bell_started && !ch_has(&c, F_SIGNED)) { SDL_Log("FAIL: the bell run ended unsigned"); fails++; }
    SDL_Log("CHAPTER SELFTEST %s  (%d clips, %d battles, mandatory %d/%d, %d failure(s))",
            fails ? "FAILED" : "ok", clips, battles, CH_MANDATORY - missing, CH_MANDATORY, fails);
    return fails;
}
