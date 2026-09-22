// game.cpp — the game itself: the screens, the fades, and the GameAPI host.cpp calls.
//
//   owns      game_create (and every capture/self-test environment switch it reads), game_destroy,
//             the hot-reload blob, game_tick's screen switch and fade resolver, the dialogue
//             capture path, the map.flag and capture.flag polls, and the title and end cards.
//   never     draws a page, a menu or the world — it calls cutscene.cpp, chapter.cpp, dev_panel.cpp
//             and the test drivers, and holds no rule of its own beyond "which screen is up".
//   exposes   the GameAPI functions star_logic.cpp hands to the host.
//   tested by every suite: each one enters through game_create's environment switches.
#include "star_internal.h"

// New Game starts at CS_CHAPTERS[0]; the cutscene player reads one chapter's scene list at a
// time, so it never had to learn what a chapter is. Declared in star_internal.h.
const CsScene *CS_CUR = CS_CHAPTERS[0].scenes;
extern const int CS_CUR_COUNT = CS_CHAPTERS[0].count;

void draw_title(Star *st, int w, int h, float dt) {
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    ImFont *font = ImGui::GetFont();
    if (st->title_t == 0.0f) mus_start(CS_WONDER);                         // once, so the dev mood buttons still work here
    st->title_t += dt;
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2((float)w, (float)h), IM_COL32(6, 6, 22, 255), IM_COL32(6, 6, 22, 255), IM_COL32(0, 0, 0, 255), IM_COL32(0, 0, 0, 255));
    for (int i = 0; i < 70; i++) {                                         // a quiet starfield
        uint32_t k = (uint32_t)i * 2654435761u;
        float x = (float)(k % 1000) / 1000.0f * w, y = (float)((k >> 10) % 1000) / 1000.0f * h * 0.75f;
        float tw = 0.55f + 0.45f * sinf(st->title_t * (0.6f + (k % 7) * 0.2f) + i);
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 3, y + 3), with_alpha(IM_COL32(200, 210, 255, 255), tw));
    }
    float m = (float)(w < h ? w : h);
    const char *title = "THE FAIR COPY", *sub = CH_TITLE;
    float ts = m * 0.115f, ss = m * 0.042f;
    ImVec2 tsz = font->CalcTextSizeA(ts, FLT_MAX, 0, title), ssz = font->CalcTextSizeA(ss, FLT_MAX, 0, sub);
    float y = h * 0.28f;
    dl->AddText(font, ts, ImVec2((w - tsz.x) * 0.5f + ts * 0.05f, y + ts * 0.05f), IM_COL32(20, 30, 120, 255), title);
    dl->AddText(font, ts, ImVec2((w - tsz.x) * 0.5f, y), IM_COL32(240, 215, 120, 255), title);
    dl->AddText(font, ss, ImVec2((w - ssz.x) * 0.5f, y + ts * 1.25f), IM_COL32(190, 200, 235, 255), sub);
    if (fmodf(st->title_t, 1.4f) < 0.9f) {
        const char *tap = "Tap to begin";
        ImVec2 z = font->CalcTextSizeA(ss, FLT_MAX, 0, tap);
        dl->AddText(font, ss, ImVec2((w - z.x) * 0.5f, h * 0.68f), IM_COL32_WHITE, tap);
    }
    if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && st->title_t > 0.5f && st->fade_to_scene < 0) {
        au_play(V_BELL, note_freq(81), 1.2f, 0.3f);
        // New Game. The chapter starts at CH_STEPS[0], which is a PLAY block in the yard — the
        // chapter opens on the stick in your hand, not on a scene — so we fade to the FIELD and
        // let ch_apply_step put the party on the right map, in the right light, with the goal up.
        ch_new_game(&st->ch);
        st->fade_to_scene = CS_CUR_COUNT;
    }
}

void draw_end(Star *st, int w, int h, float dt) {
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    ImFont *font = ImGui::GetFont();
    st->title_t += dt;
    dl->AddRectFilled(ImVec2(0, 0), ImVec2((float)w, (float)h), IM_COL32(0, 0, 0, 255));
    float m = (float)(w < h ? w : h), s = m * 0.06f;
    const char *a = "To be continued", *b = "Tap to return to the title";
    ImVec2 za = font->CalcTextSizeA(s, FLT_MAX, 0, a), zb = font->CalcTextSizeA(s * 0.6f, FLT_MAX, 0, b);
    dl->AddText(font, s, ImVec2((w - za.x) * 0.5f, h * 0.42f), IM_COL32(240, 215, 120, 255), a);
    dl->AddText(font, s * 0.6f, ImVec2((w - zb.x) * 0.5f, h * 0.42f + s * 1.8f), IM_COL32(170, 180, 210, 255), b);
    if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && st->title_t > 0.8f) { st->screen = SCR_TITLE; st->title_t = 0; }
}
// ───────────────────────── GameAPI ─────────────────────────

void *game_create(float dpi_scale) {
    Star *st = (Star *)calloc(1, sizeof(Star));
    // Desktop capture: no window interaction, no phone. Android never sets this, so it is inert there.
    // DIALOG_CAPTURE=<scene>:<line>[:page] — one dialogue box, rendered by the real player and written
    // to build_desktop/dialog_<scene>_l<line>_p<page>.png, so the box can be judged on the longest
    // real lines without a phone. <scene> is an index or a scene id from cutscene_data.h.
    st->port_motion = true;
    const char *dspec = SDL_getenv("DIALOG_CAPTURE");
    if (dspec) snprintf(st->dlg_spec, sizeof(st->dlg_spec), "%s", dspec);
    // DIALOG_EXPR=<expr> forces one expression on every line, so a capture can show a portrait
    // variant (and its fallback) before any scene file has been tagged. Desktop capture path only.
    const char *espec = SDL_getenv("DIALOG_EXPR");
    if (espec) snprintf(g_expr_force, sizeof(g_expr_force), "%s", espec);
    const char *spec = SDL_getenv("VOX_CAPTURE");      // <map>:<w>:<h>:<out.png>, one voxel-field frame
    if (spec) { snprintf(st->cap_spec, sizeof(st->cap_spec), "%s", spec); st->cap_vox = 1; }
    spec = SDL_getenv("VOX_SELFTEST");                 // load every map once and fail loudly
    if (spec && spec[0] == '1') st->cap_vox = 2;
    spec = SDL_getenv("VOX_WALKTEST");                 // <map>|all: drive the movement bot, then quit
    if (spec && spec[0]) { snprintf(st->cap_spec, sizeof(st->cap_spec), "%s", spec); st->cap_vox = 3; }
    // CHAPTER_SELFTEST=1 — the chapter is played through and the battles are tested, then quit.
    // BATTLE_SELFTEST=1 — the battles only. Either way no window input is read and the app exits.
    spec = SDL_getenv("CHAPTER_SELFTEST");
    if (spec && spec[0] == '1') st->self_test = 1;
    spec = SDL_getenv("CHAPTER_LOGIC_SELFTEST");                   // the old name for the same thing
    if (spec && spec[0] == '1') st->self_test = 1;
    // CHAPTER_PLAYTEST=1 — the bot that actually PLAYS it. Two runs, the completionist and the
    // lazy player, plus the three checks that need no run (the hidden finds, the bell run's
    // arithmetic, and whether the hill is a hill).
    spec = SDL_getenv("CHAPTER_PLAYTEST");
    if (spec && spec[0] == '1') st->self_test = 4;
    spec = SDL_getenv("BATTLE_SELFTEST");
    if (spec && spec[0] == '1') st->self_test = 2;
    // CLIPS_SELFTEST=1 — every scene tapped through by the real player. See ct_drive.
    spec = SDL_getenv("CLIPS_SELFTEST");
    if (spec && spec[0] == '1') st->self_test = 5;
    // ROBUSTNESS=1 — the sweep. See rb_drive.
    spec = SDL_getenv("ROBUSTNESS");
    if (spec && spec[0] == '1') st->self_test = 6;
    // BATTLE_UI_TEST=1 — the real battle screen driven by injected TAPS, in a real window.
    spec = SDL_getenv("BATTLE_UI_TEST");
    if (spec && spec[0] == '1') st->self_test = 3;
    st->dpi_scale = dpi_scale;
    st->screen = SCR_TITLE;
    st->fade_to_scene = -1;
    st->tex_scene = -1;
    st->fade = 1.0f;                                   // open from black
    // STAR_NEWGAME=1 — skip the title and open straight on chapter one, step 1, in the yard. This
    // is what the owner's Desktop shortcut uses: one double-click and you are playing. enter_field
    // cannot run yet (there is no VoxField until the first tick), so this only arms the chapter and
    // lets the fade resolver take us to the field.
    spec = SDL_getenv("STAR_NEWGAME");
    if (spec && spec[0] == '1') { ch_new_game(&st->ch); st->fade_to_scene = CS_CUR_COUNT; }
    memset(&dev, 0, sizeof(dev));
    memset(&mus, 0, sizeof(mus));
    memset(&g_set, 0, sizeof(g_set));
    settings_load();                  // before au_init: the gain starts where the file says it is
    au_init();
    dev_load();
    return st;
}

void game_destroy(void *state) {
    Star *st = (Star *)state;
    star_free_textures(st);
    if (st->vx) { vx_destroy(st->vx); st->vx = nullptr; }
    if (au_stream) { SDL_DestroyAudioStream(au_stream); au_stream = nullptr; }
    free(st);
}

void game_on_save_event(void *) {}
int game_wants_quit(void *state) { return ((Star *)state)->cap_state == 2; }


size_t game_serialize(void *state, void *buf, size_t buf_size) {
    Star *st = (Star *)state;
    ReloadBlob b;
    memset(&b, 0, sizeof(b));
    b.magic = RELOAD_MAGIC;
    b.screen = st->screen; b.scene = st->scene; b.line = st->line; b.line_page = st->line_page;
    b.to_field = st->to_field ? 1 : 0;
    if (st->vx) { b.has_vx = 1; vx_save(st->vx, &b.vx); }
    b.ch = st->ch;                                     // POD; the whole chapter survives the reload
    if (buf && buf_size >= sizeof(b)) memcpy(buf, &b, sizeof(b));
    return sizeof(b);
}

void game_deserialize(void *state, const void *buf, size_t size) {
    Star *st = (Star *)state;
    ReloadBlob b;
    if (size < sizeof(b)) return;
    memcpy(&b, buf, sizeof(b));
    if (b.magic != RELOAD_MAGIC) return;               // blob from the old game or an older layout: title
    st->fade = 0.0f;
    // The chapter comes back exactly as it was, then is validated: a step index from a build with a
    // different CH_STEPS is clamped rather than trusted, which is the same rule the save file uses.
    st->ch = b.ch;
    if (st->ch.step < 0 || st->ch.step >= CH_STEP_COUNT) st->ch.step = 0;
    if (st->ch.party.count < 1 || st->ch.party.count > BT_PARTY) st->ch.party.count = 1;
    st->in_battle = 0;                                 // a reload never lands you mid-battle
    st->fight_enc[0] = 0;
    bool want_field = (b.screen == SCR_FIELD) || b.to_field;
    if (b.has_vx && want_field) {
        if (!st->vx) st->vx = vx_create();
        vx_restore(st->vx, &b.vx);                     // map and position survive the reload
    }
    if (b.screen == SCR_INTRO && b.scene >= 0 && b.scene < CS_CUR_COUNT && CS_CUR[b.scene].line_count > 0) {
        int line = b.line < 0 ? 0 : b.line >= CS_CUR[b.scene].line_count ? CS_CUR[b.scene].line_count - 1 : b.line;
        star_goto(st, b.scene, line, true);
        // Keep the PAGE too, so a long line being edited comes back on the page you were reading.
        // star_goto validated the line; the page is clamped when the box next paginates, which is the
        // only place that knows how many pages the new text has.
        st->line_page = b.line_page < 0 ? 0 : b.line_page;
        st->to_field = (b.to_field && st->vx != nullptr);
    } else if (b.screen == SCR_END) {
        st->screen = SCR_END;
    } else if (b.screen == SCR_FIELD) {
        st->screen = SCR_FIELD;                        // the map is already loaded; GL objects rebuild lazily
        mus_start(CS_WONDER);
    }
}
void game_tick(void *state, int w, int h, float dpi_scale) {
    Star *st = (Star *)state;
    st->dpi_scale = dpi_scale;
    float dt = ImGui::GetIO().DeltaTime;
    if (dt > 0.1f) dt = 0.1f;

    // The dialogue capture: hold one line on screen and read the frame back. The host renders after
    // this returns, so the read is of the PREVIOUS frame — which is why it waits a few frames first
    // and why nothing on this screen animates once the typewriter is done.
    if (st->dlg_spec[0] && st->cap_state < 2) {
        char id[96] = "", flag[8] = "";
        int line = 0, page = 0;
        sscanf(st->dlg_spec, "%95[^:]:%d:%d:%7s", id, &line, &page, flag);
        st->dlg_as_narr = (flag[0] == 'n');            // the same line drawn with no name and no portrait
        st->dlg_no_port = (flag[0] == 'x') || st->dlg_as_narr;
        st->dlg_small = (flag[0] == 's');
        st->dlg_textless = (flag[0] == 't');
        int scene = -1;
        for (int i = 0; i < CS_CUR_COUNT; i++) if (!strcmp(CS_CUR[i].id, id)) { scene = i; break; }
        if (scene < 0) { scene = atoi(id); if (scene < 0 || scene >= CS_CUR_COUNT) scene = 0; }
        if (line < 0 || line >= CS_CUR[scene].line_count) line = 0;
        if (st->dlg_frames == 0) { star_goto(st, scene, line, true); SDL_Log("dialog capture: scene %d (%s) line %d page %d",
                                                                            scene, CS_CUR[scene].id, line, page); }
        st->screen = SCR_INTRO;
        st->fade = 0.0f; st->fade_to_scene = -1;
        st->line = line; st->line_page = page; st->line_t = 99.0f;      // typing finished, marker steady
        if (++st->dlg_frames >= 6) {
            char out[320], suffix[64] = "";
            if (st->dlg_as_narr) snprintf(suffix, sizeof(suffix), "_narrator");
            else if (st->dlg_no_port) snprintf(suffix, sizeof(suffix), "_noportrait");
            else if (st->dlg_small) snprintf(suffix, sizeof(suffix), "_smallbox");
            if (st->dlg_textless) snprintf(suffix + strlen(suffix), sizeof(suffix) - strlen(suffix), "_textless");
            if (g_expr_force[0]) snprintf(suffix + strlen(suffix), sizeof(suffix) - strlen(suffix), "_%s", g_expr_force);
            snprintf(out, sizeof(out), "build_desktop/dialog_%s_l%d_p%d%s.png", CS_CUR[scene].id, line, page, suffix);
            unsigned char *px = (unsigned char *)malloc((size_t)w * h * 4);
            unsigned char *row = (unsigned char *)malloc((size_t)w * 4);
            if (px && row) {
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
                for (int y = 0; y < h / 2; y++) {                        // GL reads bottom-up
                    memcpy(row, px + (size_t)y * w * 4, (size_t)w * 4);
                    memcpy(px + (size_t)y * w * 4, px + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
                    memcpy(px + (size_t)(h - 1 - y) * w * 4, row, (size_t)w * 4);
                }
                int ok = stbi_write_png(out, w, h, 4, px, w * 4);
                SDL_Log("dialog capture: %s %dx%d %s", out, w, h, ok ? "written" : "FAILED");
            }
            free(px); free(row);
            st->cap_state = 2;
        }
    }
    if (st->self_test == 3 && st->cap_state < 2) {
        battle_ui_test(st, w, h, dt);
        draw_dev(st, w, h, dt, dpi_scale);
        au_update();
        return;
    }
    // ── the robustness sweep ──
    if (st->self_test == 6 && st->cap_state < 2) {
        if (!rb_drive(st, w, h, dt)) st->cap_state = 2;
        au_update();
        return;
    }
    // ── every scene tapped through by the real player ──
    if (st->self_test == 5 && st->cap_state < 2) {
        if (!ct_drive(st, w, h)) st->cap_state = 2;
        au_update();
        return;
    }
    // ── the chapter play-test: a bot with a stick and two buttons ──
    // Three phases. The two runs yield a frame at a time (a clip has to be drawn to be tapped
    // through); the three static checks need no run at all and go first, so a map that has
    // stranded its hidden item says so in the first second rather than after two full playthroughs.
    if (st->self_test == 4 && st->cap_state < 2) {
        static int phase = 0, static_fails = 0, run_fails = 0;
        pb_set_size(w, h);
        if (phase == 0) {
            if (!st->vx) st->vx = vx_create();
            static_fails = playtest_hidden_finds(st) + playtest_bell_run(st) + playtest_hill_climb(st);
            pb_begin(st, 0);
            phase = 1;
        } else if (phase == 1) {
            if (!pb_drive()) { run_fails += pb_fail_count(); pb_begin(st, 1); phase = 2; }
        } else if (phase == 2) {
            if (!pb_drive()) {
                run_fails += pb_fail_count();
                int bad = static_fails + run_fails;
                SDL_Log("SELFCHECK chapter-playtest %s (%d static, %d in play)",
                        bad ? "FAILED" : "ok", static_fails, run_fails);
                st->cap_state = 2;
            }
        }
        if (st->cap_state < 2) {
            // Draw whatever screen we are on, so a clip really is being tapped through the real
            // renderer and a textless line or an expression tag is exercised as the player sees it.
            switch (st->screen) {
            case SCR_TITLE: draw_title(st, w, h, dt); break;
            case SCR_INTRO: draw_scene(st, w, h, dt); break;
            case SCR_END:   draw_end(st, w, h, dt); break;
            case SCR_FIELD: break;                 // the bot ticks the field itself, headless
            }
            if (st->fade_to_scene >= 0) {
                st->fade += dt / 0.45f;
                if (st->fade >= 1.0f) {
                    st->fade = 1.0f;
                    int next = st->fade_to_scene;
                    st->fade_to_scene = -1;
                    if (st->to_field && !st->from_field) enter_field(st);
                    else {
                        bool came = st->from_field;
                        st->from_field = false;
                        while (next < CS_CUR_COUNT && CS_CUR[next].line_count <= 0) next++;
                        if (next < CS_CUR_COUNT) { star_goto(st, next, 0, false); st->to_field = came; }
                        else enter_field(st);
                    }
                }
            } else if (st->fade > 0.0f) { st->fade -= dt / 0.6f; if (st->fade < 0) st->fade = 0; }
            au_update();
            return;
        }
    }
    if (st->self_test && st->cap_state < 2) {
        // Mode 1 (CHAPTER_SELFTEST) proves both, chapter first because it needs nothing. Mode 2
        // (BATTLE_SELFTEST) is the battles alone, and must log its OWN marker — capture.sh greps
        // for "SELFCHECK battle" and would otherwise never see a verdict.
        if (st->self_test == 2) {
            int bad = bt_selftest();
            SDL_Log("SELFCHECK battle %s", bad ? "FAILED" : "ok");
        } else {
            int bad = chapter_selftest() + bt_selftest();
            SDL_Log("SELFCHECK chapter %s", bad ? "FAILED" : "ok");
        }
        st->cap_state = 2;
    } else if (st->cap_vox == 2 && st->cap_state < 2) {
        if (!st->vx) st->vx = vx_create();
        st->screen = SCR_FIELD;
        vx_selftest(st->vx);   // capture.sh --vox-selftest greps the SELFCHECK lines for the verdict
        st->cap_state = 2;
    } else if (st->cap_vox == 3 && st->cap_state < 2) {
        if (!st->vx) st->vx = vx_create();
        st->screen = SCR_FIELD;
        vx_walktest(st->vx, st->cap_spec);   // capture.sh --vox-walktest greps the verdict line
        st->cap_state = 2;
    } else if (st->cap_spec[0] && st->cap_vox && st->cap_state < 2) {
        if (!st->vx) st->vx = vx_create();
        st->screen = SCR_FIELD;
        st->fade = 0.0f;
        st->fade_to_scene = -1;
        if (st->cap_state == 0) {
            char m[64] = "halm", out[256] = "capture.png";
            int cw = 1920, chh = 1080;
            sscanf(st->cap_spec, "%63[^:]:%d:%d:%255s", m, &cw, &chh, out);
            vx_capture_to(st->vx, m, cw, chh, out);
            st->cap_state = 1;
        } else if (vx_capture_done(st->vx)) st->cap_state = 2;
    }

    // map.flag is the field's remote control, and from the title it means "go there": a fresh start
    // after a crash or a force-stop can be put back in the field from the Mac with no taps at all.
    if (st->screen != SCR_FIELD) {
        static float poll = 0;
        poll += dt;
        if (poll > 0.5f) {
            poll = 0;
            const char *pref = SDL_GetPrefPath("com.playground", "questglory");
            char path[600];
            snprintf(path, sizeof(path), "%smap.flag", pref ? pref : "");
            size_t sz = 0;
            void *d = SDL_LoadFile(path, &sz);
            if (d) { SDL_free(d); if (sz) { SDL_Log("field: map.flag from the title — entering the field"); enter_field(st); } }
        }
    }

    switch (st->screen) {
    case SCR_TITLE: draw_title(st, w, h, dt); break;
    case SCR_INTRO: draw_scene(st, w, h, dt); break;
    case SCR_END:   draw_end(st, w, h, dt); break;
    case SCR_FIELD: draw_field(st, w, h, dt); draw_goal(st, w, h); break;
    }

    // Fades between scenes: to black, cut, back up. Music keeps playing across the cut.
    if (st->fade_to_scene >= 0) {
        st->fade += dt / 0.45f;
        if (st->fade >= 1.0f) {
            st->fade = 1.0f;
            int next = st->fade_to_scene;
            st->fade_to_scene = -1;
            if (st->to_field && !st->from_field) {                    // a field conversation just ended
                enter_field(st);
            } else {
                bool came = st->from_field;
                st->from_field = false;
                while (next < CS_CUR_COUNT && CS_CUR[next].line_count <= 0) next++;   // nothing to tap
                if (next < CS_CUR_COUNT) { star_goto(st, next, 0, false); st->to_field = came; }
                else enter_field(st);                                 // the intro is over: out into Halm
            }
        }
    } else if (st->fade > 0.0f) {
        st->fade -= dt / 0.6f;
        if (st->fade < 0.0f) st->fade = 0.0f;
    }
    if (st->fade > 0.0f)
        ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(0, 0), ImVec2((float)w, (float)h), with_alpha(IM_COL32(0, 0, 0, 255), st->fade));

    // A pending capture.flag pulls the game to the field: the field consumes the flag itself, so
    // this only has to get us onto the right screen. Same idea as reload.flag.
    static float cap_poll = 0.0f;
    cap_poll += dt;
    if (cap_poll > 0.5f) {
        cap_poll = 0.0f;
        if (st->screen != SCR_FIELD) {
            char path[600];
            snprintf(path, sizeof(path), "%scapture.flag", pref_path());
            SDL_IOStream *fp = SDL_IOFromFile(path, "rb");
            if (fp) {
                Sint64 sz = SDL_GetIOSize(fp);
                SDL_CloseIO(fp);
                if (sz > 0) { st->fade_to_scene = -1; st->fade = 0.0f; enter_field(st); }
            }
        }
    }

    draw_dev(st, w, h, dt, dpi_scale);
    settings_tick(dt);
    au_update();
}
