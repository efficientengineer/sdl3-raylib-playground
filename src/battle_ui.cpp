// battle_ui.cpp — §3 THE SCREEN. Draws battle_rules.cpp's state and decides nothing.
//
//   owns      the enemy and party sprites, the backdrop, the health and stamina bars, the tell and
//             OPEN markers, the command menu, the effort slider, the skill list, the log, the
//             results banner, the keyboard, the recorded hit rects, and the Dev panel's battle box.
//   never     changes a rule. Every number it shows and every row it offers comes from §1/§2; the
//             one thing it writes back is the player's choice (ui_cmd/ui_effort/ui_target).
//   exposes   bt_draw, bt_input (battle_internal.h) and the bt_ui_* accessors (battle.h), which are
//             the hit rects the UI test taps rather than recomputing the layout for itself.
//   tested by ./capture.sh --battle-ui-test (3k taps, no stuck-fire) and battle_tool --capture.

#include "battle_internal.h"

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// §3  THE SCREEN.  Draws §2's state and decides nothing.
// ═══════════════════════════════════════════════════════════════════════════════════════════════
#if BT_DRAW

static ImU32 bt_col(int idx, ImU32 fallback) {
    uint32_t c = bt_pal(idx, fallback);
    return (ImU32)c;
}
static ImU32 bt_rgb(int r, int g, int bl, int a = 255) { return IM_COL32(r, g, bl, a); }

// One sprite, loaded once at bt_start and never again: no per-frame file IO, no per-frame upload.
// The field-sprite contract (engine agent, 2026-09-21): story/field/sprites/<id>.png, indexed on
// the master palette, index 0 transparent, anchor BOTTOM-CENTRE, 64 px to the map cell, a single
// still by default. It is NOT a walker sheet — no direction rows. An optional sidecar
// story/field/sprites/<id>.json carries {"frames":N,"frame":[w,h],...} and the FRAME SIZE IS READ
// FROM IT, never divided out. Falling back to story/field/walkers/<id>.png (which does exist for
// some cast members) means the 3-column walker layout, so the two cases are kept apart.
unsigned bt_load_sprite(const char *id, int *ow, int *oh, int *ofw, int *ofh) {
    char rel[160];
    size_t sz = 0; void *data = 0;
    bool walker = false;
    snprintf(rel, sizeof(rel), "field/sprites/%s.png", id);
    data = bt_read(rel, &sz);
    if (!data) {
        walker = true;
        snprintf(rel, sizeof(rel), "field/walkers/%s.png", id);
        data = bt_read(rel, &sz);
    }
    if (!data) return 0;
    int w = 0, h = 0, c = 0;
    unsigned char *px = stbi_load_from_memory((const unsigned char *)data, (int)sz, &w, &h, &c, 4);
    SDL_free(data);
    if (!px || w < 4 || h < 4) { if (px) stbi_image_free(px); return 0; }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    // NEAREST both ways: the owner's pixels stay pixels.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(px);
    *ow = w; *oh = h;
    // Frame size: the sidecar's `frame` when there is one, the walker's 3 columns when we fell
    // back to a walk sheet, otherwise the whole image (a single still, which is the default).
    *ofw = w; *ofh = h;
    if (walker) { *ofw = w / 3; *ofh = (int)(w / 3 * 1.5f); if (*ofh > h) *ofh = h; }
    else {
        char jrel[160]; size_t jsz = 0;
        snprintf(jrel, sizeof(jrel), "field/sprites/%s.json", id);
        char *j = (char *)bt_read(jrel, &jsz);
        if (j) {
            const char *f = strstr(j, "\"frame\"");
            int fw = 0, fh = 0;
            if (f && sscanf(f, "\"frame\"%*[^[][%d,%d", &fw, &fh) == 2 && fw > 0 && fh > 0) { *ofw = fw; *ofh = fh; }
            SDL_free(j);
        }
    }
    return (unsigned)tex;
}

// A walker sheet is 3 columns; frame 0 of row 0 is the south-facing stand. A sprites/ image is
// taken whole. We only ever need one frame, so this is a UV rectangle, not a cut.
// Frame 0 is what a battle needs: a UV rectangle, never a cut.
static void bt_sprite_uv(int w, int h, int fw, int fh, ImVec2 *uv0, ImVec2 *uv1) {
    if (fw <= 0 || fh <= 0 || fw > w || fh > h) { *uv0 = ImVec2(0, 0); *uv1 = ImVec2(1, 1); return; }
    *uv0 = ImVec2(0, 0);
    *uv1 = ImVec2(fw / (float)w, fh / (float)h);
}

// The backdrop: a gradient/banding built from the palette ramps, varied by the map. No art needed
// and none blocked on.
static void bt_draw_backdrop(Battle *b, ImDrawList *dl, float w, float h) {
    // The map seeds a hue; the band's height sets a luminance. The colour is then the nearest
    // entry in master.hex to that pair, so the backdrop is varied per map, is always dark at the
    // top, and never contains a colour the palette does not have (PALETTE.md).
    uint32_t seed = 2166136261u;
    for (const char *p = b->map; *p; p++) seed = (seed ^ (uint32_t)(unsigned char)*p) * 16777619u;
    float hue = (seed % 360) / 360.0f;
    int bands = 10;
    for (int i = 0; i < bands; i++) {
        float f = i / (float)(bands - 1);
        float want_l = 22.0f + 58.0f * f;              // dark sky -> lighter ground
        int best = -1; float bs = 1e9f;
        for (int k = 2; k < BT_PAL_N; k++) {
            uint32_t c = bt_pal(k, 0);
            if (!c) continue;
            float r = (float)(c & 255), g = (float)((c >> 8) & 255), bl = (float)((c >> 16) & 255);
            float l = 0.30f * r + 0.59f * g + 0.11f * bl;
            float mx = r > g ? (r > bl ? r : bl) : (g > bl ? g : bl);
            float mn = r < g ? (r < bl ? r : bl) : (g < bl ? g : bl);
            float sat = mx > 0 ? (mx - mn) / mx : 0;
            float hh = 0;
            if (mx > mn) {
                if (mx == r) hh = fmodf((g - bl) / (mx - mn), 6.0f) / 6.0f;
                else if (mx == g) hh = (((bl - r) / (mx - mn)) + 2.0f) / 6.0f;
                else hh = (((r - g) / (mx - mn)) + 4.0f) / 6.0f;
                if (hh < 0) hh += 1.0f;
            }
            float hd = fabsf(hh - hue); if (hd > 0.5f) hd = 1.0f - hd;
            float sc = fabsf(l - want_l) + hd * 90.0f * sat + sat * 20.0f;
            if (sc < bs) { bs = sc; best = k; }
        }
        float y0 = h * (i / (float)bands), y1 = h * ((i + 1) / (float)bands);
        dl->AddRectFilled(ImVec2(0, y0), ImVec2(w, y1), bt_col(best, bt_rgb(24 + i * 5, 26 + i * 5, 34 + i * 4)));
    }
}

// A battler with no art yet: a flat palette-ramp silhouette with the NAME under it.
static void bt_draw_silhouette(ImDrawList *dl, ImVec2 c, float sw, float sh, ImU32 body, ImU32 edge) {
    dl->AddRectFilled(ImVec2(c.x - sw * 0.34f, c.y - sh * 0.55f), ImVec2(c.x + sw * 0.34f, c.y + sh * 0.5f), body, sw * 0.12f);
    dl->AddCircleFilled(ImVec2(c.x, c.y - sh * 0.66f), sw * 0.22f, body, 20);
    dl->AddRect(ImVec2(c.x - sw * 0.34f, c.y - sh * 0.55f), ImVec2(c.x + sw * 0.34f, c.y + sh * 0.5f), edge, sw * 0.12f, 0, 2.0f);
}

static void bt_bar(ImDrawList *dl, float x, float y, float w, float h, float frac, ImU32 fill, ImU32 back) {
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), back);
    if (frac > 0) dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w * (frac > 1 ? 1 : frac), y + h), fill);
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(0, 0, 0, 180));
}

static void bt_text(ImDrawList *dl, float size, ImVec2 p, ImU32 col, const char *s) {
    ImFont *f = ImGui::GetFont();
    dl->AddText(f, size, ImVec2(p.x + size * 0.07f, p.y + size * 0.07f), IM_COL32(0, 0, 0, 200), s);
    dl->AddText(f, size, p, col, s);
}

static const char *bt_field_text(const char *id, const char **name) {
    for (int i = 0; i < FIELD_TEXT_COUNT; i++)
        if (!strcmp(FIELD_TEXT[i].id, id)) { if (name) *name = FIELD_TEXT[i].name; return FIELD_TEXT[i].text; }
    if (name) *name = "";
    return "";
}

void bt_draw(Battle *b, int W, int H, float dt, bool ui_blocked) {
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    ImFont *font = ImGui::GetFont();
    float w = (float)W, h = (float)H;
    float S = h * 0.040f;                      // one text size, everything sized off it
    b->t += dt;

    bt_draw_backdrop(b, dl, w, h);

    ImU32 lit   = bt_rgb(255, 245, 220);
    ImU32 grey  = bt_rgb(112, 116, 128);
    ImU32 gold  = bt_rgb(255, 216, 74);
    ImU32 red   = bt_rgb(196, 64, 56);
    ImU32 green = bt_rgb(96, 176, 96);
    ImU32 blue  = bt_rgb(88, 128, 200);
    ImU32 back  = bt_rgb(18, 18, 26, 210);

    // ── the enemy group, left (COMBAT.md: the party on the right, enemies on the left) ──
    for (int i = 0; i < b->enemy_count; i++) {
        BtEnemyState *e = &b->en[i];
        const BtEnemyDef *d = bt_edef(b, i);
        float cx = w * (0.13f + 0.14f * i), cy = h * 0.28f;
        float sw = w * 0.12f, sh = h * 0.26f;
        if (!e->alive) continue;
        e->anim += dt;

        // OPEN: it is visibly wide open and cannot move. The stagger is the prompt (§7a row 4).
        float lean = e->open ? sinf(b->t * 3.0f) * sw * 0.12f + sw * 0.18f : 0.0f;
        ImVec2 c(cx + lean, cy);
        int ti = BT_PARTY + i;
        if (b->tex[ti]) {
            ImVec2 uv0, uv1; bt_sprite_uv(b->texw[ti], b->texh[ti], b->texfw[ti], b->texfh[ti], &uv0, &uv1);
            dl->AddImage((ImTextureID)(intptr_t)b->tex[ti], ImVec2(c.x - sw * 0.5f, c.y - sh * 0.7f),
                         ImVec2(c.x + sw * 0.5f, c.y + sh * 0.5f), uv0, uv1);
        } else {
            bt_draw_silhouette(dl, c, sw, sh, bt_col(20 + i * 5, bt_rgb(84, 72, 72)), bt_rgb(20, 18, 22));
        }
        if (e->open) {
            dl->AddRect(ImVec2(c.x - sw * 0.6f, c.y - sh * 0.8f), ImVec2(c.x + sw * 0.6f, c.y + sh * 0.6f), gold, 4.0f, 0, 3.0f);
            bt_text(dl, S * 1.1f, ImVec2(cx - sw * 0.3f, cy - sh * 1.05f), gold, "OPEN");
        }
        // The tell indicator: unmistakable at a glance. Its size is the performance, not a cue.
        bool show_tell = bt_tell_visible(b, i, b->cur);
        if (show_tell) {
            float pulse = 0.6f + 0.4f * sinf(b->t * 6.0f / (b->tell_speed > 0 ? b->tell_speed : 1.0f));
            float r = sw * (0.55f + 0.10f * pulse);
            dl->AddCircle(ImVec2(c.x, c.y - sh * 0.15f), r, IM_COL32(255, 96, 72, (int)(220 * pulse)), 24, 4.0f);
            bt_text(dl, S * 1.0f, ImVec2(cx - sw * 0.34f, cy + sh * 0.58f), red, "!  tell");
        }
        // Name, hp, and — with night sight — its stamina bar (§5).
        bt_text(dl, S * 0.85f, ImVec2(cx - sw * 0.55f, cy + sh * 0.72f), lit, d->name);
        bt_bar(dl, cx - sw * 0.55f, cy + sh * 0.72f + S * 1.25f, sw * 1.1f, S * 0.30f,
               e->hp_max ? e->hp / (float)e->hp_max : 0, red, back);
        if (bt_night_sight(b))
            bt_bar(dl, cx - sw * 0.55f, cy + sh * 0.72f + S * 1.70f, sw * 1.1f, S * 0.22f,
                   e->stam_max ? e->stam / (float)e->stam_max : 0, blue, back);
        if (e->wind > 0) {
            // The counterweight climbing the post: a graph of the player's own impatience, and it
            // is standing in the middle of the yard.
            for (int k = 0; k < e->wind && k < 12; k++)
                dl->AddRectFilled(ImVec2(cx + sw * 0.62f, cy + sh * 0.5f - k * S * 0.32f),
                                  ImVec2(cx + sw * 0.76f, cy + sh * 0.5f - k * S * 0.32f - S * 0.22f), red);
        }
    }

    // ── the party, right ──
    for (int i = 0; i < b->party->count; i++) {
        BtActor *a = &b->party->a[i];
        float cx = w * (0.63f + 0.12f * i), cy = h * 0.40f;
        float sw = w * 0.11f, sh = h * 0.26f;
        ImVec2 c(cx, cy);
        if (b->tex[i]) {
            ImVec2 uv0, uv1; bt_sprite_uv(b->texw[i], b->texh[i], b->texfw[i], b->texfh[i], &uv0, &uv1);
            dl->AddImage((ImTextureID)(intptr_t)b->tex[i], ImVec2(c.x - sw * 0.5f, c.y - sh * 0.7f),
                         ImVec2(c.x + sw * 0.5f, c.y + sh * 0.5f), uv0, uv1);
        } else {
            bt_draw_silhouette(dl, c, sw, sh, bt_col(24 + i * 5, bt_rgb(72, 88, 112)),
                               a->alive ? bt_rgb(20, 18, 22) : red);
        }
        if (!b->tex[i]) bt_text(dl, S * 0.8f, ImVec2(cx - sw * 0.42f, cy + sh * 0.56f), lit, a->name);
        if (b->pa[i].guarding) bt_text(dl, S, ImVec2(cx - sw * 0.3f, cy - sh * 0.95f), blue, "guard");
    }

    // ── the party panel: name, hp, STAMINA under every portrait (§11) ──
    float px0 = w * 0.55f, py0 = h * 0.66f, pw = w * 0.43f, ph = h * 0.31f;
    dl->AddRectFilled(ImVec2(px0, py0), ImVec2(px0 + pw, py0 + ph), back, S * 0.3f);
    dl->AddRect(ImVec2(px0, py0), ImVec2(px0 + pw, py0 + ph), bt_rgb(120, 120, 140), S * 0.3f, 0, 2.0f);
    for (int i = 0; i < b->party->count; i++) {
        BtActor *a = &b->party->a[i];
        float y = py0 + S * 0.5f + i * (ph - S) / (float)bt_max(1, b->party->count);
        ImU32 nm = (i == b->cur && b->phase == BTP_INPUT) ? gold : (a->alive ? lit : grey);
        bt_text(dl, S * 0.95f, ImVec2(px0 + S * 0.5f, y), nm, a->name);
        char num[48];
        snprintf(num, sizeof(num), "%d/%d", a->hp, a->hp_max);
        bt_text(dl, S * 0.8f, ImVec2(px0 + pw * 0.62f, y), lit, num);
        bt_bar(dl, px0 + pw * 0.30f, y + S * 0.15f, pw * 0.28f, S * 0.35f, a->hp / (float)a->hp_max, green, bt_rgb(40, 40, 48));
        bt_bar(dl, px0 + pw * 0.30f, y + S * 0.62f, pw * 0.28f, S * 0.30f, a->stam / (float)a->stam_max, blue, bt_rgb(40, 40, 48));
        snprintf(num, sizeof(num), "%d", a->stam);
        bt_text(dl, S * 0.65f, ImVec2(px0 + pw * 0.60f, y + S * 0.55f), blue, num);
    }

    // ── the goal line, six words or fewer, never two at once (§11) ──
    if (b->goal[0]) bt_text(dl, S * 1.0f, ImVec2(w * 0.04f, h * 0.05f), gold, b->goal);

    // ── the log ──
    for (int i = 0; i < b->log_n; i++)
        bt_text(dl, S * 0.72f, ImVec2(w * 0.34f, h * 0.045f + i * S * 0.88f), lit, b->log[i]);

    // ── a mid-fight story box, drawn with the shared dialogue helpers so it looks like the rest ──
    if (b->pending_text[0]) {
        b->pending_text_t += dt;
        const char *nm = "";
        const char *txt = bt_field_text(b->pending_text, &nm);
        float bx0 = w * 0.08f, by0 = h * 0.62f, bx1 = w * 0.92f, by1 = h * 0.92f;
        dl->AddRectFilled(ImVec2(bx0, by0), ImVec2(bx1, by1), IM_COL32(12, 12, 20, 240), S * 0.4f);
        dl->AddRect(ImVec2(bx0, by0), ImVec2(bx1, by1), gold, S * 0.4f, 0, 2.0f);
        DlgRect r = dlg_content(bx0, by0, bx1, by1, S);
        float y = r.y0;
        if (nm && nm[0]) { bt_text(dl, S, ImVec2(r.x0, y), gold, nm); y += S * DLG_LINE_H; }
        DlgPages pg;
        dlg_paginate(font, S, r.x1 - r.x0, dlg_max_lines(r.y1 - y, S), txt, &pg);
        dlg_draw_text(dl, font, S, ImVec2(r.x0, y), r.x1 - r.x0, lit, pg.beg[0], pg.end[0], 9999, false);
        dlg_marker(dl, r.x1 - S, r.y1 - S * 0.6f, S, pg.count > 1, b->t);
        if (b->pending_text_t > 2.6f) b->pending_text[0] = 0;
    }

    // ── the command list, and the effort slider under the chosen command ──
    if (b->phase == BTP_INPUT && !ui_blocked && b->party->a[b->cur].alive) {
        int who = b->cur;
        BtActor *a = &b->party->a[who];
        float mx0 = w * 0.05f, mw = w * 0.24f;
        // A THUMB, at dpi_scale 3 on the phone. 0.068 of 1080 px is 73 px ≈ 24 dp — under half of
        // the 48 dp a touch target is supposed to be. 0.095 is 103 px ≈ 34 dp of row with the hit
        // rect overhanging it, which measures ~48 dp on the phone.
        float rowh = h * 0.095f;
        float my0 = 0;                 // set below: the block grows UPWARD from a fixed bottom, so
                                       // the slider under it is never pushed off the screen as the
                                       // command list grows (§7a: the list grows, one entry at a time)

        // Build the visible list: an idea that has not arrived is ABSENT, never greyed (§7a).
        int rows[BTC_COUNT], nrows = bt_build_rows(b, who, rows);
        if (b->ui_cmd < 0 || b->ui_cmd >= nrows) b->ui_cmd = 0;
        my0 = h * 0.94f - rowh * 1.7f - nrows * rowh;

        dl->AddRectFilled(ImVec2(mx0 - S * 0.4f, my0 - S * 0.4f),
                          ImVec2(mx0 + mw + S * 0.4f, my0 + nrows * rowh + rowh * 1.5f), back, S * 0.3f);

        for (int r = 0; r < nrows; r++) {
            int c = rows[r];
            float y = my0 + r * rowh;
            bool is_guard = (c == BTC_GUARD);
            int cheap = bt_cheapest_affordable(b, who, c);
            bool afford = is_guard || cheap > 0;
            // Guard is NEVER greyed, and on a tired character it is the only lit entry.
            ImU32 col = afford ? lit : grey;
            if (is_guard) col = lit;
            if (r == b->ui_cmd) dl->AddRectFilled(ImVec2(mx0 - S * 0.2f, y - S * 0.1f), ImVec2(mx0 + mw, y + rowh * 0.85f), IM_COL32(70, 70, 100, 160), S * 0.2f);
            // §4a.5: after five failures the Guard entry pulses once as the tell plays. Yard only.
            if (is_guard && b->guard_pulse) {
                float pl = 0.5f + 0.5f * sinf(b->t * 7.0f);
                dl->AddRect(ImVec2(mx0 - S * 0.25f, y - S * 0.15f), ImVec2(mx0 + mw, y + rowh * 0.85f),
                            IM_COL32(255, 216, 74, (int)(220 * pl)), S * 0.2f, 0, 2.5f);
            }
            bt_text(dl, S * 1.05f, ImVec2(mx0, y), col, BT_CMD_NAME[c]);
            // Anything unaffordable is greyed WITH ITS COST SHOWN (§4a.4, §11).
            if (!afford && !is_guard) {
                char cost[32]; snprintf(cost, sizeof(cost), "%d", BT_EFFORT_COST[1]);
                bt_text(dl, S * 0.8f, ImVec2(mx0 + mw - S * 2.0f, y + S * 0.15f), grey, cost);
            }
        }

        int cmd = rows[b->ui_cmd];
        // The slider appears under the chosen command the first time there is an opening to spend
        // into (§7a row 5). Guard has no effort — it is always effort 1 and costs nothing (§1).
        bool has_effort = (cmd == BTC_ATTACK || cmd == BTC_SKILL) && (b->party->known & BT_KNOWS_EFFORT);
        float sy = my0 + nrows * rowh + S * 0.2f;
        if (has_effort) {
            int eff = bt_clamp(b->ui_effort, 1, 5);
            for (int e = 1; e <= 5; e++) {
                float nx = mx0 + (e - 1) * (mw / 5.0f);
                bool pay = bt_can_pay(b, who, cmd, e);
                ImU32 c = (e == eff) ? gold : (pay ? lit : grey);
                dl->AddRectFilled(ImVec2(nx, sy), ImVec2(nx + mw / 5.0f - S * 0.2f, sy + S * 0.7f),
                                  (e <= eff && pay) ? c : IM_COL32(48, 48, 60, 220), S * 0.15f);
                char n[4]; snprintf(n, sizeof(n), "%d", e);
                bt_text(dl, S * 0.7f, ImVec2(nx + S * 0.3f, sy + S * 0.02f), (e <= eff && pay) ? bt_rgb(20, 20, 26) : c, n);
            }
            // The cost against the bar, and the resulting magnitude, BEFORE you commit (§11).
            char info[64];
            snprintf(info, sizeof(info), "cost %d / %d   x%.2f", BT_EFFORT_COST[eff], a->stam, BT_EFFORT_MAG[eff]);
            bt_text(dl, S * 0.75f, ImVec2(mx0, sy + S * 0.85f), bt_can_pay(b, who, cmd, eff) ? blue : grey, info);
        }

        // ── the skill list: each skill with its MINIMUM EFFORT, greyed below it (§5, §11) ──
        if (cmd == BTC_SKILL) {
            int n = bt_skill_count(a->id);
            for (int k = 0; k < n; k++) {
                const BtSkillDef *s = bt_skill_at(a->id, k);
                float y = my0 + k * S * 1.2f;
                int eff = bt_clamp(b->ui_effort, 1, 5);
                bool ok = eff >= s->min && bt_can_pay(b, who, BTC_SKILL, eff);
                ImU32 c = (k == b->ui_skill) ? gold : (ok ? lit : grey);
                bt_text(dl, S * 0.85f, ImVec2(mx0 + mw + S * 1.0f, y), c, s->name);
                char m[32]; snprintf(m, sizeof(m), "min %d", s->min);
                bt_text(dl, S * 0.7f, ImVec2(mx0 + mw + S * 6.2f, y + S * 0.1f), eff >= s->min ? blue : grey, m);
            }
        }

        // ── touch: one invisible button per row and per notch, big enough for a thumb ──
        // COORDINATE SPACES, and this is the DPI bug the owner's report could have been:
        // everything above is drawn in DRAWABLE pixels (the w/h host.cpp passes, from
        // SDL_GetWindowSizeInPixels), but ImGui's cursor, window and mouse coordinates are
        // DisplaySize — logical points. On Android the two are the same number, so the phone was
        // never wrong here; on a Retina Mac they differ by 2 and every hit rect would sit at twice
        // its drawn position. `U` is that ratio, and it is 1.0 on the phone.
        float U = (ImGui::GetIO().DisplaySize.x > 0.0f) ? ImGui::GetIO().DisplaySize.x / w : 1.0f;
        b->ui_mx0 = mx0; b->ui_my0 = my0; b->ui_mw = mw; b->ui_rowh = rowh;
        b->ui_sy = sy; b->ui_S = S; b->ui_U = U; b->ui_nrows = nrows; b->ui_has_effort = has_effort ? 1 : 0;
        for (int r = 0; r < nrows; r++) b->ui_rows[r] = rows[r];
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        // SCREEN coordinates below, not window ones. The Dev panel's touch style sets
        // WindowPadding to 8 * dpi * 1.3 — about 30 px on the phone — so SetCursorPos(x, y) put
        // every hit rect 30 px right and 30 px down of the row it belongs to. A thumb aimed at
        // Guard landed on Attack. SetCursorScreenPos has no origin to get wrong.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        if (ImGui::Begin("##bt_touch", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar)) {
            for (int r = 0; r < nrows; r++) {
                ImGui::SetCursorScreenPos(ImVec2((mx0 - S * 0.3f) * U, (my0 + r * rowh - S * 0.2f) * U));
                char lbl[24]; snprintf(lbl, sizeof(lbl), "##cmd%d", r);
                if (ImGui::InvisibleButton(lbl, ImVec2((mw + S * 0.6f) * U, rowh * U))) {
                    if (b->ui_cmd == r) b->phase = BTP_RESOLVE;         // a second tap commits
                    else { b->ui_cmd = r; b->ui_effort = bt_clamp(b->party->last_effort[rows[r]], 1, 5); }
                }
            }
            // The effort notches: tappable AND draggable. A drag across them sets the effort under
            // the finger, which is what a slider has to do — five separate buttons only answered a
            // tap that landed and lifted inside one notch.
            if (has_effort) {
                for (int e = 1; e <= 5; e++) {
                    ImGui::SetCursorScreenPos(ImVec2((mx0 + (e - 1) * (mw / 5.0f)) * U, (sy - S * 0.45f) * U));
                    char lbl[24]; snprintf(lbl, sizeof(lbl), "##eff%d", e);
                    if (ImGui::InvisibleButton(lbl, ImVec2((mw / 5.0f) * U, S * 1.6f * U))) b->ui_effort = e;
                }
                if (ImGui::IsMouseDown(0)) {
                    ImVec2 m = ImGui::GetIO().MousePos;
                    float y0 = (sy - S * 0.45f) * U, y1 = y0 + S * 1.6f * U;
                    if (m.y >= y0 && m.y <= y1 && m.x >= mx0 * U && m.x <= (mx0 + mw) * U)
                        b->ui_effort = bt_clamp(1 + (int)((m.x - mx0 * U) / (mw * U / 5.0f)), 1, 5);
                }
            }
            if (cmd == BTC_SKILL) {
                int n = bt_skill_count(a->id);
                for (int k = 0; k < n; k++) {
                    ImGui::SetCursorScreenPos(ImVec2((mx0 + mw + S * 0.8f) * U, (my0 + k * S * 1.2f - S * 0.1f) * U));
                    char lbl[24]; snprintf(lbl, sizeof(lbl), "##sk%d", k);
                    if (ImGui::InvisibleButton(lbl, ImVec2(mw * U, S * 1.05f * U))) b->ui_skill = k;
                }
            }
            // Target: tap an enemy.
            for (int i = 0; i < b->enemy_count; i++) {
                if (!b->en[i].alive) continue;
                ImGui::SetCursorScreenPos(ImVec2((w * (0.13f + 0.14f * i) - w * 0.07f) * U, h * 0.18f * U));
                char lbl[24]; snprintf(lbl, sizeof(lbl), "##tgt%d", i);
                if (ImGui::InvisibleButton(lbl, ImVec2(w * 0.14f * U, h * 0.32f * U))) b->ui_target = i;
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    // ── the result, minimal ──
    if (b->phase == BTP_OVER) {
        const char *s = b->outcome == BT_WIN ? "Won." : b->outcome == BT_FLED ? "Away." :
                        bt_is_yard(b) ? "Again." : "Down.";
        dl->AddRectFilled(ImVec2(w * 0.30f, h * 0.40f), ImVec2(w * 0.70f, h * 0.58f), back, S * 0.4f);
        bt_text(dl, S * 2.0f, ImVec2(w * 0.36f, h * 0.44f), gold, s);
    }
}

// ── keyboard, so the Mac is playable: arrows / 1-5 / Enter / Space ─────────────────────────────
void bt_input(Battle *b, int nrows, const int *rows) {
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput) return;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) b->ui_cmd = (b->ui_cmd + 1) % bt_max(1, nrows);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))   b->ui_cmd = (b->ui_cmd + bt_max(1, nrows) - 1) % bt_max(1, nrows);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) b->ui_effort = bt_clamp(b->ui_effort + 1, 1, 5);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))  b->ui_effort = bt_clamp(b->ui_effort - 1, 1, 5);
    for (int k = 0; k < 5; k++) if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_1 + k))) b->ui_effort = k + 1;
    if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        int n = 0; for (int i = 0; i < b->enemy_count; i++) if (b->en[i].alive) n++;
        if (n) { do { b->ui_target = (b->ui_target + 1) % b->enemy_count; } while (!b->en[b->ui_target].alive); }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Q)) { int n = bt_skill_count(b->party->a[b->cur].id); if (n) b->ui_skill = (b->ui_skill + 1) % n; }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) b->phase = BTP_RESOLVE;
}
#endif  // BT_DRAW
// ── what the UI test needs to know, and nothing more ───────────────────────────────────────────
// Coordinates come back in IMGUI space (the same space a tap arrives in), taken from the last
// frame's real layout. kind: 0 = command row, 1 = effort notch (idx 1..5), 2 = enemy.
int bt_ui_phase(Battle *b) { return b ? b->phase : BTP_OVER; }
int bt_ui_rows(Battle *b, int *rows) {
    if (!b) return 0;
    for (int i = 0; i < b->ui_nrows; i++) rows[i] = b->ui_rows[i];
    return b->ui_nrows;
}
int bt_ui_has_effort(Battle *b) { return b ? b->ui_has_effort : 0; }
int bt_ui_enemy_count(Battle *b) { return b ? b->enemy_count : 0; }

// WHAT A PLAYER CAN SEE ON THE SCREEN, as two numbers. The chapter play-test's battle policy is
// "guard on a tell, spend into an opening, attack otherwise", and it has to read those two things
// the way the player reads them — off the enemy. Without this the bot can only guard, and a bot
// that only guards parries the arm that holds for thirteen hundred rounds without ever killing it,
// which is how these two accessors came to exist.
int bt_ui_enemy_tell(Battle *b, int i) {
    if (!b || i < 0 || i >= b->enemy_count) return 0;
    return b->en[i].alive && b->en[i].tell && !b->en[i].open;
}
int bt_ui_enemy_open(Battle *b, int i) {
    if (!b || i < 0 || i >= b->enemy_count) return 0;
    return b->en[i].alive && (b->en[i].open || b->en[i].open_next);
}
int bt_ui_enemy_alive(Battle *b, int i) {
    if (!b || i < 0 || i >= b->enemy_count) return 0;
    return b->en[i].alive;
}
int bt_ui_selected(Battle *b) { return b ? b->ui_cmd : 0; }
int bt_ui_effort(Battle *b) { return b ? b->ui_effort : 0; }
int bt_ui_target(Battle *b) { return b ? b->ui_target : 0; }
int bt_ui_round(Battle *b) { return b ? b->round : 0; }
int bt_ui_cur(Battle *b) { return b ? b->cur : 0; }
// Is the row at `idx` one the player can actually commit right now? Guard always is — that is the
// guarantee. Anything else needs an effort it can pay for.
int bt_ui_affordable(Battle *b, int idx) {
    if (!b || idx < 0 || idx >= b->ui_nrows) return 0;
    int c = b->ui_rows[idx];
    return (c == BTC_GUARD || bt_cheapest_affordable(b, b->cur, c) > 0) ? 1 : 0;
}
const char *bt_cmd_name(int cmd) { return (cmd > 0 && cmd < BTC_COUNT) ? BT_CMD_NAME[cmd] : "?"; }
// The skill list of whoever the screen is taking input from. See battle.h for why this exists.
static_assert(BTK_DAMAGE == BT_SK_DAMAGE && BTK_SETTLE == BT_SK_SETTLE && BTK_HARD == BT_SK_HARD,
              "battle.h's BT_SK_* must match BtSkillKind");
int bt_ui_skill_count(Battle *b) { return b ? bt_skill_count(b->party->a[b->cur].id) : 0; }
static const BtSkillDef *bt_ui_skill(Battle *b, int k) {
    if (!b || k < 0 || k >= bt_skill_count(b->party->a[b->cur].id)) return nullptr;
    return bt_skill_at(b->party->a[b->cur].id, k);
}
const char *bt_ui_skill_name(Battle *b, int k) { const BtSkillDef *s = bt_ui_skill(b, k); return s ? s->name : ""; }
int bt_ui_skill_min(Battle *b, int k)          { const BtSkillDef *s = bt_ui_skill(b, k); return s ? s->min : 0; }
int bt_ui_skill_kind(Battle *b, int k)         { const BtSkillDef *s = bt_ui_skill(b, k); return s ? s->kind : -1; }
int bt_ui_skill_sel(Battle *b)                 { return b ? b->ui_skill : 0; }
bool bt_ui_point(Battle *b, int kind, int idx, float *x, float *y) {
#if BT_DRAW
    if (!b || b->ui_rowh <= 0.0f) return false;
    float U = b->ui_U;
    if (kind == 0) {
        if (idx < 0 || idx >= b->ui_nrows) return false;
        *x = (b->ui_mx0 + b->ui_mw * 0.5f) * U;
        *y = (b->ui_my0 + (idx + 0.4f) * b->ui_rowh) * U;
        return true;
    }
    if (kind == 1) {
        if (!b->ui_has_effort || idx < 1 || idx > 5) return false;
        *x = (b->ui_mx0 + (idx - 0.5f) * (b->ui_mw / 5.0f)) * U;
        *y = (b->ui_sy + b->ui_S * 0.35f) * U;
        return true;
    }
    if (kind == 2) {
        if (idx < 0 || idx >= b->enemy_count || !b->en[idx].alive) return false;
        float w = ImGui::GetIO().DisplaySize.x / (U > 0 ? U : 1.0f);
        float h = ImGui::GetIO().DisplaySize.y / (U > 0 ? U : 1.0f);
        *x = (w * (0.13f + 0.14f * idx)) * U;
        *y = (h * 0.34f) * U;
        return true;
    }
    // The skill list, which is only on screen while the Skill row is selected — the same rect the
    // InvisibleButton above uses, centred.
    if (kind == 3) {
        if (idx < 0 || idx >= bt_skill_count(b->party->a[b->cur].id)) return false;
        *x = (b->ui_mx0 + b->ui_mw + b->ui_S * 0.8f) * U + b->ui_mw * U * 0.5f;
        *y = (b->ui_my0 + idx * b->ui_S * 1.2f - b->ui_S * 0.1f) * U + b->ui_S * 1.05f * U * 0.5f;
        return true;
    }
#else
    (void)b; (void)kind; (void)idx; (void)x; (void)y;
#endif
    return false;
}
bool bt_dev_ui(Battle *b, char *out, int cap) {
#if BT_DRAW
    if (!b) return false;
    bool asked = false;
    ImGui::TextUnformatted("Battle");
    if (b->enc) ImGui::Text("%s  round %d  phase %d", b->enc->id, b->round, b->boss_phase);
    ImGui::Text("state %s  stuck-fires %d", BT_PHASE_NAME[b->phase], b->stuck_fires);
    if (ImGui::Button("Win##bt")) { bt_force_win(b); }
    ImGui::SameLine();
    // A soft lock must never trap the player: this is the hand-operated watchdog.
    if (ImGui::Button("Abort battle##bt")) b->aborted = 1;
    ImGui::SameLine();
    if (ImGui::Button("Know all##bt") && b->party)
        b->party->known |= BT_KNOWS_ATTACK | BT_KNOWS_GUARD | BT_KNOWS_EFFORT | BT_KNOWS_SKILL | BT_KNOWS_ITEM | BT_KNOWS_RUN;
    ImGui::SameLine();
    if (ImGui::Button("All items##bt") && b->party)
        b->party->items |= BT_ITEM_YARD | BT_ITEM_TEACHING | BT_ITEM_HILL | BT_ITEM_PASTURE;
    for (int i = 0; i < BT_ENCOUNTER_COUNT; i++) {
        if (i % 4) ImGui::SameLine();
        char lbl[48]; snprintf(lbl, sizeof(lbl), "%s##btenc", BT_ENCOUNTERS[i].id);
        if (ImGui::Button(lbl)) { snprintf(out, cap, "battle %s", BT_ENCOUNTERS[i].id); asked = true; }
    }
    return asked;
#else
    (void)b; (void)out; (void)cap; return false;
#endif
}
