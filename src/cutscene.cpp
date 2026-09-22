// cutscene.cpp — THE PLAYER: the manga page and the talk box, drawn line by line.
//
//   owns      panel textures and the portrait cache (one file per expression, with the base
//             portrait as the fallback), the panel reveal and its arrival flash, the page clear on
//             `---`, the typewriter and its blips, pagination and the double marker, the speaker
//             portrait's slide-in and its expression pop, and the Narrator's nameless centred box.
//   never     decides which scene plays or what happens after one — that is chapter.cpp and
//             game.cpp. It only ever draws the line it is on and says when the line is finished.
//   exposes   star_goto/star_advance/star_free_textures/draw_scene, and the little drawing helpers
//             (draw_typed, draw_box, ease_out, with_alpha) the other screens share.
//   tested by ./capture.sh --clips-selftest (every scene tapped through), --dialog (one box), and
//             --robustness H (every field-text id measured in the real box).
#include "star_internal.h"

// ───────────────────────── Files and textures ─────────────────────────

const char *pref_path() {
    static char path[512];
    if (!path[0]) {
        const char *p = SDL_GetPrefPath(PREF_ORG, PREF_APP);
        snprintf(path, sizeof(path), "%s", p ? p : "");
    }
    return path;
}

Tex tex_load(const char *file) {
    Tex t = {0, 0, 0};
    char path[768];
    size_t size = 0;
    snprintf(path, sizeof(path), "%scutscenes/%s", pref_path(), file);
    void *data = SDL_LoadFile(path, &size);
    if (!data) {
        snprintf(path, sizeof(path), "cutscenes/%s", file);
        data = SDL_LoadFile(path, &size);
    }
    if (!data) {                      // desktop: straight out of the repo, which is what a capture run reads
        if (!strncmp(file, "portrait_", 9)) snprintf(path, sizeof(path), "story/portraits/%s", file + 9);
        else snprintf(path, sizeof(path), "story/panels/%s", file);
        data = SDL_LoadFile(path, &size);
    }
    if (!data) { SDL_Log("cutscene: %s not found", file); return t; }
    int n = 0;
    unsigned char *px = stbi_load_from_memory((const unsigned char *)data, (int)size, &t.w, &t.h, &n, 4);
    SDL_free(data);
    if (!px) { SDL_Log("cutscene: %s failed to decode", file); return t; }
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    // Panels and portraits are full-resolution PAINTINGS shown small — a 550x850 portrait lands in a
    // ~133x206 box, a 4x minification. Plain LINEAR takes four samples of sixteen and aliases; NEAREST
    // magnification made the edges crawl. Mipmaps are what a downscaled painting wants.
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(px);
    return t;
}
void star_free_textures(Star *st) {
    for (int i = 0; i < MAX_PANELS; i++) if (st->tex[i].id) glDeleteTextures(1, &st->tex[i].id);
    // Only OWNED slots hold a texture of their own: a slot whose expression file was missing carries
    // a copy of the base portrait's Tex, and deleting that id twice would kill the base's texture.
    for (int i = 0; i < MAX_FACES; i++) if (st->face[i].id && st->face_owned[i]) glDeleteTextures(1, &st->face[i].id);
    if (st->backdrop.id) glDeleteTextures(1, &st->backdrop.id);
    memset(st->tex, 0, sizeof(st->tex));
    memset(st->face, 0, sizeof(st->face));
    memset(st->face_file, 0, sizeof(st->face_file));
    memset(st->face_owned, 0, sizeof(st->face_owned));
    st->backdrop = Tex{0, 0, 0};
    st->tex_scene = -1;
    st->face_cur = nullptr;
    st->expr_cur[0] = 0;
}

static bool same_file(const char *a, const char *b) { return a == b || (a && b && !strcmp(a, b)); }

// Document text and unattributed lines: a box with no speaker name and no portrait, text centred.
// Panel and talk scenes use it as freely as narration scenes do.
bool line_is_narrator(const CsLine *ln) { return ln->speaker && !strcmp(ln->speaker, CS_NARRATOR); }

// ── Expressions (the ten ids the export writes: neutral, smile, laugh, biglaugh, concern, sorrow,
// annoyed, angry, shock, resolve). A line's expression names a SECOND portrait file for the same
// speaker — portrait_hart_sorrow.png beside portrait_hart.png — and the line falls back to the
// speaker's plain portrait when that file has not been drawn yet.
//
// CS_HAS_EXPR is defined by a cutscene_data.h whose CsLine carries `const char *expr`. The game
// builds against a header with it and against one without, so the tool side can land separately.
char g_expr_force[16];     // DIALOG_EXPR=<expr>: force one expression, for a Mac capture only

const char *line_expr(const CsLine *ln) {
    if (g_expr_force[0]) return g_expr_force;
#ifdef CS_HAS_EXPR
    return ln->expr ? ln->expr : "";
#else
    (void)ln;
    return "";
#endif
}

// portrait_hart.png + "sorrow" -> portrait_hart_sorrow.png. An empty expression is the base file.
static void face_key(const char *base, const char *expr, char *out, size_t n) {
    if (!expr || !*expr) { snprintf(out, n, "%s", base); return; }
    const char *dot = strrchr(base, '.');
    int stem = dot ? (int)(dot - base) : (int)strlen(base);
    snprintf(out, n, "%.*s_%s%s", stem, base, expr, dot ? dot : "");
}

// The scene's portraits, cached by file name. Loading is LAZY — the first line that asks for an
// expression loads it — and a cache miss is cached too, so a missing (speaker, expression) pair is
// looked for, and logged by tex_load, exactly once per scene.
static const Tex *face_get(Star *st, const char *base, const char *expr) {
    if (!base) return nullptr;
    char key[FACE_KEY];
    face_key(base, expr, key, sizeof(key));
    int slot = -1, free_slot = -1;
    for (int i = 0; i < MAX_FACES; i++) {
        if (!st->face_file[i][0]) { if (free_slot < 0) free_slot = i; continue; }
        if (!strcmp(st->face_file[i], key)) { slot = i; break; }
    }
    if (slot < 0) {
        if (free_slot < 0) return (expr && *expr) ? face_get(st, base, "") : nullptr;   // cache full
        slot = free_slot;
        snprintf(st->face_file[slot], FACE_KEY, "%s", key);
        st->face[slot] = tex_load(key);
        st->face_owned[slot] = st->face[slot].id ? 1 : 0;
        if (!st->face[slot].id && expr && *expr) {                       // not drawn yet: use the base
            const Tex *b = face_get(st, base, "");                       // may take another slot; ours is taken
            if (b) { st->face[slot] = *b; st->face_owned[slot] = 0; }
            SDL_Log("cutscene: no %s, falling back to %s", key, base);
        }
    }
    return st->face[slot].id ? &st->face[slot] : nullptr;
}

// The plain portraits of every speaker in the scene, loaded up front as they always were, so a
// speaker change never waits on a decode. Expression variants come in lazily on the line that uses
// one; a scene with many of them simply fills more of the cache.
void load_faces(Star *st, const CsScene *sc) {
    for (int i = 0; i < sc->line_count; i++)
        if (sc->lines[i].portrait) face_get(st, sc->lines[i].portrait, "");
}

void reveal_panel(Star *st, const CsScene *sc, int n, bool instant) {   // n is 1-based, 0 = none
    if (n < 1 || n > MAX_PANELS || n > sc->panel_count) return;
    if (sc->panels[n - 1].page != st->page) {                              // new page: start from an empty screen
        st->page = sc->panels[n - 1].page;
        memset(st->panel_on, 0, sizeof(st->panel_on));
        st->z_next = 0;
    }
    if (st->panel_on[n - 1]) return;
    st->panel_on[n - 1] = true;
    st->panel_t[n - 1] = instant ? PANEL_IN : 0.0f;
    st->panel_z[n - 1] = st->z_next++;
    if (!instant) au_play(V_THUMP, 0, 0.35f, 0.5f);
}

// Enter `line` of `scene`. `instant` rebuilds the page without animation (used after a hot reload).
void star_goto(Star *st, int scene, int line, bool instant) {
    const CsScene *sc = &CS_CUR[scene];
    if (st->tex_scene != scene) {
        star_free_textures(st);                 // only ever one scene's art is resident: 8 panels + portraits
        for (int i = 0; i < sc->panel_count && i < MAX_PANELS; i++) st->tex[i] = tex_load(sc->panels[i].file);
        if (sc->backdrop) st->backdrop = tex_load(sc->backdrop);
        load_faces(st, sc);
        st->tex_scene = scene;
    }
    if (scene != st->scene || line == 0 || instant) {
        memset(st->panel_on, 0, sizeof(st->panel_on));
        st->z_next = 0;
        st->page = 0;
        for (int i = 0; i < line; i++) reveal_panel(st, sc, sc->lines[i].reveal, true);
    }
    st->screen = SCR_INTRO;
    st->scene = scene;
    st->line = line;
    st->line_page = 0;                     // a new line always starts at its first page
    st->line_t = instant ? 99.0f : 0.0f;
    // A NEW SPEAKER slides in from their end of the box, as before. The SAME speaker changing
    // expression is a different event — the face is already there and only its look changed — so it
    // swaps with a tiny pop instead, which reads as a reaction rather than as somebody arriving.
    const char *ex = line_expr(&sc->lines[line]);
    if (!same_file(st->face_cur, sc->lines[line].portrait)) {
        st->face_cur = sc->lines[line].portrait;
        st->face_t = instant ? FACE_IN : 0.0f;
        st->face_pop = FACE_POP;
    } else if (strcmp(st->expr_cur, ex)) {
        st->face_pop = instant ? FACE_POP : 0.0f;
    }
    snprintf(st->expr_cur, sizeof(st->expr_cur), "%s", ex);
    reveal_panel(st, sc, sc->lines[line].reveal, instant);
    mus_start(sc->lines[line].mood);
}

void star_advance(Star *st) {
    const CsScene *sc = &CS_CUR[st->scene];
    if (st->line + 1 < sc->line_count) { star_goto(st, st->scene, st->line + 1, false); return; }
    st->fade_to_scene = st->scene + 1;                                     // fade out, then next scene or the end
}

// ───────────────────────── Drawing ─────────────────────────

float ease_out(float x) { x = x < 0 ? 0 : x > 1 ? 1 : x; return 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x); }
ImU32 with_alpha(ImU32 c, float a) { a = a < 0 ? 0 : a > 1 ? 1 : a; return (c & 0x00FFFFFF) | ((ImU32)(((c >> 24) & 0xFF) * a) << 24); }

// Draws `text` word-wrapped inside `width`, showing only the first `chars` characters. Wrapping is
// computed on the full string so words never jump lines while they are being typed.
void draw_typed(ImDrawList *dl, ImFont *font, float size, ImVec2 pos, float width, ImU32 col, const char *text, int chars, bool center) {
    const char *end = text + strlen(text), *p = text;
    float y = pos.y;
    while (p < end && chars > 0) {
        const char *brk = font->CalcWordWrapPosition(size, p, end, width);
        if (brk == p) brk = p + 1;
        int n = (int)(brk - p);
        const char *show_end = p + (n < chars ? n : chars);
        float x = pos.x;
        if (center) x += (width - font->CalcTextSizeA(size, FLT_MAX, 0.0f, p, brk).x) * 0.5f;
        dl->AddText(font, size, ImVec2(x + size * 0.06f, y + size * 0.06f), with_alpha(IM_COL32(0, 0, 0, 255), ((col >> 24) & 0xFF) / 255.0f), p, show_end);
        dl->AddText(font, size, ImVec2(x, y), col, p, show_end);
        chars -= n;
        y += size * 1.3f;
        p = brk;
        while (p < end && (*p == ' ' || *p == '\n')) { p++; chars--; }
    }
}

// ImGui 1.92's GL backend binds its OWN sampler object (linear, no mipmap) for every draw, which
// silently overrides whatever glTexParameteri a texture was given — which is why setting
// GL_LINEAR_MIPMAP_LINEAR on a panel changed not one pixel. Panels and portraits are full-resolution
// paintings minified into their boxes (a 480x771 portrait into ~200x304), so they want mipmaps: these
// two callbacks bracket the images with a sampler that has them, and put ImGui's back afterwards.
// Everything else in the draw list — the box, the text, the font atlas, which has no mip levels —
// keeps ImGui's sampler untouched.
static GLuint g_mip_sampler = 0;
static void cb_sampler(const ImDrawList *, const ImDrawCmd *cmd) {
    ImGui_ImplOpenGL3_RenderState *rs = (ImGui_ImplOpenGL3_RenderState *)ImGui::GetPlatformIO().Renderer_RenderState;
    if (!rs || !rs->UseBindSampler) return;                    // a GL2-era path: texture params apply as they are
    if (cmd->UserCallbackData) {
        if (!g_mip_sampler) {
            glGenSamplers(1, &g_mip_sampler);
            glSamplerParameteri(g_mip_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glSamplerParameteri(g_mip_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glSamplerParameteri(g_mip_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(g_mip_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glBindSampler(0, g_mip_sampler);
    } else {
        glBindSampler(0, rs->CurrentSampler);
    }
}
static inline void mip_on(ImDrawList *dl)  { dl->AddCallback(cb_sampler, (void *)1); }
static inline void mip_off(ImDrawList *dl) { dl->AddCallback(cb_sampler, (void *)0); }

void draw_box(ImDrawList *dl, ImVec2 a, ImVec2 b, float u) {        // the blue 16-bit dialogue box
    dl->AddRectFilled(a, b, IM_COL32(8, 8, 20, 255), u * 1.6f);
    dl->AddRectFilledMultiColor(ImVec2(a.x + u, a.y + u), ImVec2(b.x - u, b.y - u),
        IM_COL32(24, 40, 150, 255), IM_COL32(24, 40, 150, 255), IM_COL32(8, 16, 84, 255), IM_COL32(8, 16, 84, 255));
    dl->AddRect(ImVec2(a.x + u * 0.5f, a.y + u * 0.5f), ImVec2(b.x - u * 0.5f, b.y - u * 0.5f), IM_COL32(225, 225, 235, 255), u * 1.4f, 0, u * 0.7f);
    dl->AddRect(ImVec2(a.x + u * 1.3f, a.y + u * 1.3f), ImVec2(b.x - u * 1.3f, b.y - u * 1.3f), IM_COL32(90, 100, 150, 255), u, 0, u * 0.35f);
}

void draw_scene(Star *st, int w, int h, float dt) {
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    ImFont *font = ImGui::GetFont();
    const CsScene *sc = &CS_CUR[st->scene];
    const CsLine *ln = &sc->lines[st->line];
    bool narr = line_is_narrator(ln) || st->dlg_as_narr;   // dlg_* are set only by DIALOG_CAPTURE
    const char *ltext = st->dlg_textless ? "" : ln->text;  // a textless line: name and portrait, no words
    bool port = h > w;
    dl->AddRectFilled(ImVec2(0, 0), ImVec2((float)w, (float)h), IM_COL32(0, 0, 0, 255));

    // Stage and dialogue box. Portrait: 4:5 stage with the box below. Landscape: 16:10 stage, box overlays it.
    float sw, sh, sx, sy, bx0, by0, bx1, by1, text_size;
    if (port) {
        sw = w * 0.97f; sh = sw / 0.8f;
        float box_h = sw * 0.34f, gap = sw * 0.02f, total = sh + gap + box_h;
        if (total > h * 0.96f) { float k = h * 0.96f / total; sw *= k; sh *= k; box_h *= k; gap *= k; total = h * 0.96f; }
        sx = (w - sw) * 0.5f; sy = (h - total) * 0.5f;
        bx0 = sx; bx1 = sx + sw; by0 = sy + sh + gap; by1 = by0 + box_h;
        text_size = sw * 0.046f;
    } else {
        sh = h * 0.98f; sw = sh * 1.6f;
        if (sw > w * 0.98f) { sw = w * 0.98f; sh = sw / 1.6f; }
        sx = (w - sw) * 0.5f; sy = (h - sh) * 0.5f;
        // The box is deep enough for the speaker name and THREE lines of text; it was 0.22 of the
        // stage, which fitted exactly one line under the name and paginated every long line to bits.
        bx0 = sx + sw * 0.04f; bx1 = sx + sw * 0.96f; by0 = sy + sh * (st->dlg_small ? 0.80f : 0.66f); by1 = sy + sh * 0.985f;
        text_size = sh * 0.047f;
    }
    float u = text_size * 0.12f;

    // Talk scenes: no panels, just the named backdrop panel dimmed and centred behind the box.
    if (sc->kind == CS_TALK && st->backdrop.id) {
        float ia = (float)st->backdrop.w / (float)st->backdrop.h, dw = sw, dh = sh;
        if (ia > sw / sh) dh = sw / ia; else dw = sh * ia;
        ImVec2 p0(sx + (sw - dw) * 0.5f, sy + (sh - dh) * 0.5f);
        mip_on(dl);
        dl->AddImage((ImTextureID)(intptr_t)st->backdrop.id, p0, ImVec2(p0.x + dw, p0.y + dh),
                     ImVec2(0, 0), ImVec2(1, 1), IM_COL32(80, 80, 92, 255));   // tint multiplies: dim, slightly cool
        mip_off(dl);
    }

    // Panels, oldest first so newer ones overlap them.
    for (int z = 0; z < st->z_next; z++) {
        for (int i = 0; i < sc->panel_count && i < MAX_PANELS; i++) {
            if (!st->panel_on[i] || st->panel_z[i] != z) continue;
            st->panel_t[i] += dt;
            float a = ease_out(st->panel_t[i] / PANEL_IN);
            const CsRect *r = port ? &sc->panels[i].port : &sc->panels[i].land;
            float slide = (1.0f - a) * sw * 0.05f * ((i & 1) ? 1.0f : -1.0f);
            ImVec2 p0(sx + sw * r->x / 100.0f + slide, sy + sh * r->y / 100.0f);
            ImVec2 p1(p0.x + sw * r->w / 100.0f, p0.y + sh * r->h / 100.0f);
            dl->AddRectFilled(ImVec2(p0.x + u * 2, p0.y + u * 2), ImVec2(p1.x + u * 2, p1.y + u * 2), with_alpha(IM_COL32(0, 0, 0, 170), a));
            if (st->tex[i].id) {
                mip_on(dl);
                dl->AddImage((ImTextureID)(intptr_t)st->tex[i].id, p0, p1, ImVec2(0, 0), ImVec2(1, 1), with_alpha(IM_COL32_WHITE, a));
                mip_off(dl);
            }
            else { dl->AddRectFilled(p0, p1, IM_COL32(24, 22, 36, 255)); dl->AddRect(p0, p1, IM_COL32_WHITE, 0, 0, u); }
            if (a < 1.0f) dl->AddRectFilled(p0, p1, with_alpha(IM_COL32(255, 255, 255, 190), 1.0f - a));   // arrival flash
        }
    }

    // Text. A line waits for its panel to land before typing starts. A line longer than the box is
    // PAGINATED (dialogue.h): the typewriter runs per page, a tap turns the page, and the panel
    // reveal and the mood change already happened in star_goto — that is, on the line's first page
    // only, because turning a page never goes through star_goto.
    st->line_t += dt;
    float type_t = st->line_t - (ln->reveal && !sc->narration ? PANEL_IN * 0.8f : 0.0f);

    float arrow_x = bx1 - text_size * 1.1f;                                // moves in when a portrait sits on the right
    float arrow_y = by1 - text_size * 0.95f;
    bool typing = true, more_pages = false;

    if (sc->narration) {
        float a = ease_out(st->line_t / 0.6f);
        float size = text_size * 1.08f, width = (port ? w * 0.84f : w * 0.7f);
        float top = h * (port ? 0.30f : 0.26f), bot = h * (port ? 0.74f : 0.72f);
        DlgPages pg;
        dlg_paginate(font, size, width, dlg_max_lines(bot - top, size), ltext, &pg);
        if (st->line_page >= pg.count) st->line_page = pg.count - 1;
        const char *pb = pg.beg[st->line_page], *pe = pg.end[st->line_page];
        int total = (int)(pe - pb);
        int chars = type_t <= 0 ? 0 : (int)(type_t * TYPE_CPS);
        typing = chars < total;
        more_pages = st->line_page + 1 < pg.count;
        float th = dlg_text_height(font, size, width, pb, pe);
        float ty = top + ((bot - top) - th) * 0.5f;                        // narration centres in its band
        dlg_draw_text(dl, font, size, ImVec2((w - width) * 0.5f, ty), width,
                      with_alpha(IM_COL32(222, 226, 240, 255), a), pb, pe, chars, true);
        arrow_x = (w + width) * 0.5f - size * 0.5f;
        arrow_y = ty + th + size * 0.2f;
        if (typing && chars > 0) {
            static int last_blip = -1;
            if (chars / 3 != last_blip && pb[chars - 1] != ' ') { last_blip = chars / 3; au_play(V_BLIP, 520.0f, 0.04f, 0.10f); }
        }
    } else {
        draw_box(dl, ImVec2(bx0, by0), ImVec2(bx1, by1), u);
        // The content rect: the box minus one uniform pad on ALL FOUR sides. The name sits at its top
        // and the text under the name. It used to be padded from the top only, which pushed the text
        // onto the bottom edge with a band of empty blue above it.
        DlgRect cr = dlg_content(bx0, by0, bx1, by1, text_size);
        float pad = text_size * DLG_LINE_H * DLG_PAD_LINES;
        // Speaker portrait at one end of the box (Phantasy Star IV field talk), text narrowed to fit.
        // A Narrator line never has one: it is a document, not somebody speaking.
        const Tex *fa = (narr || st->dlg_no_port) ? nullptr : face_get(st, ln->portrait, line_expr(ln));
        if (fa) {
            st->face_t += dt;
            st->face_pop += dt;
            float inset = u * 2.0f, fh = (by1 - by0) - inset * 2.0f;
            float fw = fh * (float)fa->w / (float)fa->h, cap = (bx1 - bx0) * 0.3f;
            if (fw > cap) { fw = cap; fh = fw * (float)fa->h / (float)fa->w; }
            bool right = (ln->side == CS_RIGHT);
            float a = ease_out(st->face_t / FACE_IN);
            float fx = right ? bx1 - inset - fw : bx0 + inset;
            float fy = by0 + ((by1 - by0) - fh) * 0.5f;
            ImVec2 f0(fx + (1.0f - a) * fw * 0.4f * (right ? 1.0f : -1.0f), fy), f1(f0.x + fw, f0.y + fh);
            // PORTRAIT MOTION (Dev toggle). Cheap, no assets: the pop on an expression swap, and one
            // per-expression idle held for as long as the line is up. `lpx` is a logical pixel — the
            // same 2 px on the phone as on the Mac, because the box itself is laid out in real ones.
            if (st->port_motion) {
                float lpx = st->dpi_scale > 0.0f ? st->dpi_scale : 1.0f, ox = 0.0f, oy = 0.0f, scl = 1.0f;
                if (st->face_pop < FACE_POP)                                    // 1.0 -> 1.06 -> 1.0
                    scl = 1.0f + 0.06f * sinf((st->face_pop / FACE_POP) * 3.14159265f);
                const char *ex = line_expr(ln);
                if (!strcmp(ex, "biglaugh")) oy = sinf(st->line_t * 6.0f * 6.2831853f) * 2.0f * lpx;
                else if (!strcmp(ex, "shock")) {                                // one sharp jolt, then still
                    float j = st->line_t < 0.16f ? 1.0f - st->line_t / 0.16f : 0.0f;
                    oy = -j * 5.0f * lpx;
                    ox = j * 2.0f * lpx * (right ? 1.0f : -1.0f);
                } else if (!strcmp(ex, "sorrow")) oy = 2.0f * lpx * (1.0f - expf(-st->line_t * 1.1f));
                ImVec2 c((f0.x + f1.x) * 0.5f, (f0.y + f1.y) * 0.5f);
                f0 = ImVec2(c.x + (f0.x - c.x) * scl + ox, c.y + (f0.y - c.y) * scl + oy);
                f1 = ImVec2(c.x + (f1.x - c.x) * scl + ox, c.y + (f1.y - c.y) * scl + oy);
            }
            dl->AddRectFilled(f0, f1, with_alpha(IM_COL32(6, 8, 22, 255), a));
            mip_on(dl);
            dl->AddImage((ImTextureID)(intptr_t)fa->id, f0, f1, ImVec2(0, 0), ImVec2(1, 1), with_alpha(IM_COL32_WHITE, a));
            mip_off(dl);
            dl->AddRect(f0, f1, with_alpha(IM_COL32(225, 225, 235, 255), a), 0, 0, u * 0.7f);
            dl->AddRect(ImVec2(f0.x + u * 0.8f, f0.y + u * 0.8f), ImVec2(f1.x - u * 0.8f, f1.y - u * 0.8f),
                        with_alpha(IM_COL32(90, 100, 150, 255), a), 0, 0, u * 0.35f);
            if (right) cr.x1 = fx - pad * 0.6f;
            else cr.x0 = fx + fw + pad * 0.6f;
        }
        float name_h = narr ? 0.0f : text_size * DLG_LINE_H;               // the yellow speaker line
        float gutter = text_size * 1.1f;                                   // the marker's own corner
        float tw = (cr.x1 - gutter) - cr.x0, th_avail = (cr.y1 - cr.y0) - name_h;
        DlgPages pg;
        dlg_paginate(font, text_size, tw, dlg_max_lines(th_avail, text_size), ltext, &pg);
        if (st->line_page >= pg.count) st->line_page = pg.count - 1;
        const char *pb = pg.beg[st->line_page], *pe = pg.end[st->line_page];
        int total = (int)(pe - pb);
        int chars = type_t <= 0 ? 0 : (int)(type_t * TYPE_CPS);
        typing = chars < total;
        more_pages = st->line_page + 1 < pg.count;
        // A TEXTLESS LINE — an expression and no words, a reaction beat — is a box with the speaker's
        // name and portrait in it and nothing to type. There is no typewriter to wait through and no
        // timer: the marker is up from the first frame and the tap moves on, exactly as it does on a
        // line whose typing has finished.
        if (total == 0) typing = false;
        if (typing && chars > 0) {
            static int last_blip = -1;
            if (chars / 3 != last_blip && pb[chars - 1] != ' ') { last_blip = chars / 3; au_play(V_BLIP, narr ? 520.0f : 760.0f, 0.04f, 0.10f); }
        }
        if (narr) {                                                        // no name line: the block centres
            float th = dlg_text_height(font, text_size, tw, pb, pe);
            float ty = cr.y0 + ((cr.y1 - cr.y0) - th) * 0.5f;
            dlg_draw_text(dl, font, text_size, ImVec2(cr.x0, ty), tw, IM_COL32(230, 232, 245, 255), pb, pe, chars, true);
        } else {
            dl->AddText(font, text_size * 0.92f, ImVec2(cr.x0, cr.y0), IM_COL32(255, 216, 74, 255), ln->speaker);
            dlg_draw_text(dl, font, text_size, ImVec2(cr.x0, cr.y0 + name_h), tw, IM_COL32_WHITE, pb, pe, chars, false);
        }
        arrow_x = cr.x1 - text_size * 0.45f;
        arrow_y = cr.y1 - text_size * 0.45f;
    }
    if (!typing) dlg_marker(dl, arrow_x, arrow_y, text_size, more_pages, st->line_t);

    // Input: a tap finishes the typing, then turns the page, then moves to the next line.
    // Taps on the dev overlay don't count.
    bool tap = ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && !ImGui::GetIO().WantTextInput;
    if (tap && st->fade_to_scene < 0 && st->fade <= 0.01f) {
        if (typing) st->line_t = 99.0f;
        else if (more_pages) { st->line_page++; st->line_t = 0.0f; au_play(V_BLIP, 640.0f, 0.05f, 0.12f); }
        else star_advance(st);
    }
}
