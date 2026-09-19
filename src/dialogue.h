// dialogue.h — one dialogue box, shared by the cutscene player (star_logic.cpp) and the field's
// message box (tilefield.cpp). Header-only and static: two translation units, no link order.
//
// What it owns:
//   * PAGINATION. A line longer than the box is split into pages at word boundaries, preferring a
//     sentence end in the last quarter of a page so a page break lands where the writing breathes.
//     Before this a long line simply ran out of the bottom of the box and could not be read at all.
//   * THE CONTENT RECT. The box minus a uniform pad on all four sides (0.6 of a line), the speaker
//     name at the top of it and the text under the name — not jammed against the bottom edge with
//     half the box empty above it, which is what it used to do.
//   * The typewriter, the shadowed word-wrapped draw, and the blinking marker: ▼ when the tap moves
//     to the next line, ▼▼ when it only turns the page of the line you are reading.
#pragma once
#include <string.h>
#include "imgui.h"

#define DLG_MAX_PAGES 12
#define DLG_LINE_H    1.3f          // line height as a multiple of the font size
#define DLG_PAD_LINES 0.6f          // the uniform inner pad, in line heights

struct DlgPages {
    const char *beg[DLG_MAX_PAGES];
    const char *end[DLG_MAX_PAGES];
    int count;
};

// The box's inside: `pad` on every side. Everything a box draws lives in here.
struct DlgRect { float x0, y0, x1, y1; };
static inline DlgRect dlg_content(float bx0, float by0, float bx1, float by1, float size) {
    float pad = size * DLG_LINE_H * DLG_PAD_LINES;
    DlgRect r = { bx0 + pad, by0 + pad, bx1 - pad, by1 - pad };
    return r;
}

// How many wrapped lines fit in `h`. At least one: a box too short still shows something.
static inline int dlg_max_lines(float h, float size) {
    int n = (int)((h + size * (DLG_LINE_H - 1.0f) * 0.5f) / (size * DLG_LINE_H));
    return n < 1 ? 1 : n;
}

static inline bool dlg_is_space(char c) { return c == ' ' || c == '\n' || c == '\t'; }
static inline bool dlg_sentence_end(const char *p, const char *end) {
    if (*p != '.' && *p != '!' && *p != '?') return false;
    const char *q = p + 1;
    while (q < end && (*q == '"' || *q == '\'' || *q == ')')) q++;   // "…like that," he said.
    return q >= end || dlg_is_space(*q);
}

// Split `text` into pages of at most `max_lines` wrapped lines at `width`. A page ends at a sentence
// end if one falls in its last quarter; otherwise at the wrap point, which is already a word
// boundary. Never splits a word, and always makes progress.
static inline void dlg_paginate(ImFont *font, float size, float width, int max_lines,
                                const char *text, DlgPages *out) {
    out->count = 0;
    if (!text || !*text) { out->beg[0] = out->end[0] = text ? text : ""; out->count = 1; return; }
    const char *end = text + strlen(text), *p = text;
    while (p < end && out->count < DLG_MAX_PAGES) {
        const char *q = p;
        for (int l = 0; l < max_lines && q < end; l++) {
            const char *brk = font->CalcWordWrapPosition(size, q, end, width);
            if (brk <= q) brk = q + 1;                       // a single word wider than the box
            q = brk;
            while (q < end && dlg_is_space(*q)) q++;
        }
        if (q < end) {                                        // more to come: try to end on a sentence
            float span = (float)(q - p);
            const char *limit = p + (size_t)(span * 0.75f);
            for (const char *s = q - 1; s > limit; s--) {
                if (!dlg_sentence_end(s, end)) continue;
                const char *e = s + 1;
                while (e < end && (*e == '"' || *e == '\'' || *e == ')')) e++;
                q = e;
                break;
            }
        }
        out->beg[out->count] = p;
        out->end[out->count] = q;
        out->count++;
        p = q;
        while (p < end && dlg_is_space(*p)) p++;
    }
    if (!out->count) { out->beg[0] = text; out->end[0] = end; out->count = 1; }
}

// One page, word-wrapped inside `width`, showing its first `chars` characters. Wrapping is computed
// on the whole page so a word never jumps lines while it is being typed. Returns the height drawn.
static inline float dlg_draw_text(ImDrawList *dl, ImFont *font, float size, ImVec2 pos, float width,
                                  ImU32 col, const char *beg, const char *end, int chars, bool center) {
    const char *p = beg;
    float y = pos.y;
    while (p < end && chars > 0) {
        const char *brk = font->CalcWordWrapPosition(size, p, end, width);
        if (brk <= p) brk = p + 1;
        int n = (int)(brk - p);
        const char *show_end = p + (n < chars ? n : chars);
        float x = pos.x;
        if (center) x += (width - font->CalcTextSizeA(size, FLT_MAX, 0.0f, p, brk).x) * 0.5f;
        ImU32 shadow = (IM_COL32(0, 0, 0, 255) & 0x00FFFFFF) | (((col >> 24) & 0xFF) << 24);
        dl->AddText(font, size, ImVec2(x + size * 0.06f, y + size * 0.06f), shadow, p, show_end);
        dl->AddText(font, size, ImVec2(x, y), col, p, show_end);
        chars -= n;
        y += size * DLG_LINE_H;
        p = brk;
        while (p < end && dlg_is_space(*p)) { p++; chars--; }
    }
    return y - pos.y;
}

// How tall a page comes out, so a Narrator block can be centred in the content rect.
static inline float dlg_text_height(ImFont *font, float size, float width, const char *beg, const char *end) {
    int lines = 0;
    for (const char *p = beg; p < end; ) {
        const char *brk = font->CalcWordWrapPosition(size, p, end, width);
        if (brk <= p) brk = p + 1;
        lines++;
        p = brk;
        while (p < end && dlg_is_space(*p)) p++;
    }
    return (lines < 1 ? 1 : lines) * size * DLG_LINE_H;
}

// The blinking marker, bottom right of the content rect. ONE triangle means the tap moves on to the
// next line; TWO stacked mean this line has more pages and the tap only turns the page — the owner
// can tell at a glance whether they are about to lose the rest of a sentence.
static inline void dlg_marker(ImDrawList *dl, float cx, float cy, float size, bool more_pages, float t) {
    if (fmodf(t, 1.0f) >= 0.6f) return;
    float w = size * 0.38f, hgt = size * 0.42f;
    ImU32 col = more_pages ? IM_COL32(255, 216, 74, 255) : IM_COL32_WHITE;
    if (more_pages) {
        dl->AddTriangleFilled(ImVec2(cx - w, cy - hgt * 0.85f), ImVec2(cx + w, cy - hgt * 0.85f), ImVec2(cx, cy - hgt * 0.85f + hgt), col);
        dl->AddTriangleFilled(ImVec2(cx - w, cy + hgt * 0.35f), ImVec2(cx + w, cy + hgt * 0.35f), ImVec2(cx, cy + hgt * 0.35f + hgt), col);
    } else {
        dl->AddTriangleFilled(ImVec2(cx - w, cy), ImVec2(cx + w, cy), ImVec2(cx, cy + hgt), col);
    }
}
