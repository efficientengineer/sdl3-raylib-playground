// vox_palette.cpp — PALETTE.md's 256 colours, the ramps, the colormap and indexed art upload.
// OWNS: story/palette/master.hex, ramps.md and colormap.png; the CPU colour lookup the lighting
//       maths uses; turning an indexed PNG into an R8 texture; and reading a tileset's tiles.md
//       (the SHAPE of each entry only — how big a stamp is and what kind it is).
// NEVER: builds a world, meshes, or draws. It knows nothing about cells, triggers or the party.
// EXPOSES: vx_load_palette/_ramps/_lut/_colormap, vx_ramp_by_name, vx_table_by_name,
//          vx_cpu_colour, vx_tex_indexed, vx_load_tileset, def_by_name.
// Tested by: capture.sh --vox-selftest (every map's palette loads) and the colormap identity check
// in voxfield.cpp. See src/notes/rendering.md.
#include "vox_internal.h"

// ───────────────────────── palette + ramps + colormap ─────────────────────────

bool vx_load_palette(VoxField *v) {
    size_t sz = 0;
    char *text = (char *)vx_read("palette/master.hex", &sz);
    if (!text) { SDL_Log("voxfield: no story/palette/master.hex — falling back to built-in colours"); return false; }
    int n = 0;
    for (char *c = text; *c && n < 256; c++) {
        if (*c != '#') continue;
        unsigned x = 0;
        if (sscanf(c + 1, "%6x", &x) != 1) continue;
        v->pal[n][0] = (unsigned char)(x >> 16); v->pal[n][1] = (unsigned char)((x >> 8) & 255);
        v->pal[n][2] = (unsigned char)(x & 255);
        n++; c += 6;
    }
    SDL_free(text);
    if (n < 16) { SDL_Log("voxfield: master.hex has %d colours — ignoring", n); return false; }
    for (int i = n; i < 256; i++) { v->pal[i][0] = v->pal[i][1] = v->pal[i][2] = 0; }
    for (int i = 0; i < 256; i++)
        rgb_to_oklab(v->pal[i][0] / 255.0f, v->pal[i][1] / 255.0f, v->pal[i][2] / 255.0f,
                     &v->pal_lab[i][0], &v->pal_lab[i][1], &v->pal_lab[i][2]);
    v->pal_ok = true;
    SDL_Log("voxfield: palette master.hex — %d colours", n);
    return true;
}

// master.json's ramps, by name. A block's colours are steps along one of these; nothing in this file
// ever writes a hex value (PALETTE.md).
void vx_load_ramps(VoxField *v) {
    size_t sz = 0;
    char *js = (char *)vx_read("palette/master.json", &sz);
    if (!js) { SDL_Log("voxfield: no master.json — block colours will be flat greys"); return; }
    for (const char *c = strstr(js, "\"ramps\""); c; ) {
        c = strstr(c, "\"name\"");
        if (!c || v->ramp_count >= 24) break;
        const char *q = strchr(c + 6, '"'); const char *e = q ? strchr(q + 1, '"') : nullptr;
        if (!q || !e) break;
        int len = (int)(e - q - 1); if (len > 39) len = 39;
        memcpy(v->ramp_name[v->ramp_count], q + 1, (size_t)len);
        v->ramp_name[v->ramp_count][len] = 0;
        const char *ix = strstr(e, "\"indices\"");
        if (!ix) break;
        const char *b = strchr(ix, '[');
        int k = 0;
        for (const char *p = b; p && *p && *p != ']' && k < 80; p++) {
            if ((*p >= '0' && *p <= '9') && (p == b || !(p[-1] >= '0' && p[-1] <= '9'))) {
                int val = atoi(p);
                if (val >= 0 && val < 256) v->ramp_idx[v->ramp_count][k++] = (unsigned char)val;
            }
        }
        v->ramp_len[v->ramp_count] = k;
        if (k) v->ramp_count++;
        c = strchr(ix, ']');
        if (!c) break;
    }
    SDL_free(js);
    SDL_Log("voxfield: %d palette ramps", v->ramp_count);
}

int vx_ramp_by_name(VoxField *v, const char *want) {
    if (!want || !want[0]) return -1;
    for (int i = 0; i < v->ramp_count; i++) if (!strcmp(v->ramp_name[i], want)) return i;
    for (int i = 0; i < v->ramp_count; i++) if (strstr(v->ramp_name[i], want)) return i;
    return -1;
}

// The block colour LUT: 16 wide, B_COUNT tall, R8. Columns 0..5 are the top's six steps (dark to
// light), 6..11 the sides', 12 the pattern id, 13 the solid flag. One texelFetch in the shader.
void vx_build_lut(VoxField *v) {
    unsigned char px[16 * B_COUNT];
    memset(px, 0, sizeof(px));
    for (int b = 0; b < B_COUNT; b++) {
        const VxBlockDef *d = &BLOCKS[b];
        int rt = vx_ramp_by_name(v, d->top_ramp), rs = vx_ramp_by_name(v, d->side_ramp);
        for (int s = 0; s < 6; s++) {
            float f = s / 5.0f;
            int ti = 1, si = 1;
            if (rt >= 0 && v->ramp_len[rt] > 1) {
                float g = vx_lerp(d->top_lo, d->top_hi, f) * (v->ramp_len[rt] - 1);
                ti = v->ramp_idx[rt][(int)(g + 0.5f)];
            }
            if (rs >= 0 && v->ramp_len[rs] > 1) {
                float g = vx_lerp(d->side_lo, d->side_hi, f) * (v->ramp_len[rs] - 1);
                si = v->ramp_idx[rs][(int)(g + 0.5f)];
            }
            px[b * 16 + s] = (unsigned char)ti;
            px[b * 16 + 6 + s] = (unsigned char)si;
        }
        px[b * 16 + 12] = (unsigned char)d->pattern;
        px[b * 16 + 13] = d->solid ? 1 : 0;
    }
    if (!v->lut) glGenTextures(1, &v->lut);
    glBindTexture(GL_TEXTURE_2D, v->lut);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 16, B_COUNT, 0, GL_RED, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

// story/palette/colormap.png as it stands: 256 wide, one row per (table, level). Row 0 of a table is
// full light. With no file, the rows are synthesised so night still works.
void vx_build_colormap(VoxField *v) {
    static const char *NAMES[6] = { "day", "dusk", "night", "flash", "poison", "stone" };
    v->cmap_levels = VX_LEVELS; v->cmap_tables = 6; v->cmap_h = 256;
    for (int i = 0; i < 6; i++) { snprintf(v->cmap_tname[i], 16, "%s", NAMES[i]); v->cmap_row0[i] = i * 32; }
    v->cmap_lamp_row0 = 0;

    size_t isz = 0;
    void *data = vx_read("palette/colormap.png", &isz);
    unsigned char *file = nullptr;
    int fw = 0, fh = 0, ch = 0;
    if (data) { file = stbi_load_from_memory((const unsigned char *)data, (int)isz, &fw, &fh, &ch, 4); SDL_free(data); }
    if (file && fw != 256) { stbi_image_free(file); file = nullptr; }
    if (file) {
        size_t jsz = 0;
        char *js = (char *)vx_read("palette/colormap.json", &jsz);
        if (js) {
            const char *lv = strstr(js, "\"levels\"");
            int n = 0;
            if (lv && sscanf(lv + 8, " : %d", &n) == 1 && n >= 2 && n <= 64) v->cmap_levels = n;
            int k = 0;
            for (const char *c = strstr(js, "\"table\""); c && k < 8; c = strstr(c + 1, "\"table\"")) {
                const char *q = strchr(c + 7, '"'), *e = q ? strchr(q + 1, '"') : nullptr;
                if (!q || !e) break;
                int len = (int)(e - q - 1); if (len > 15) len = 15;
                char nm[16]; memcpy(nm, q + 1, (size_t)len); nm[len] = 0;
                const char *r0 = strstr(e, "\"row0\"");
                int row0 = 0;
                if (!r0 || sscanf(r0 + 6, " : %d", &row0) != 1) break;
                snprintf(v->cmap_tname[k], 16, "%s", nm);
                v->cmap_row0[k] = row0;
                if (!strcmp(nm, "lamp")) v->cmap_lamp_row0 = row0;
                k++;
            }
            if (k) v->cmap_tables = k;
            SDL_free(js);
        }
        v->cmap_h = fh;
        if (!v->cmap) glGenTextures(1, &v->cmap);
        glBindTexture(GL_TEXTURE_2D, v->cmap);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, fh, 0, GL_RGBA, GL_UNSIGNED_BYTE, file);
        free(v->cmap_px);
        v->cmap_px = (unsigned char *)malloc((size_t)256 * fh * 4);
        if (v->cmap_px) memcpy(v->cmap_px, file, (size_t)256 * fh * 4);
        stbi_image_free(file);
        SDL_Log("voxfield: colormap.png 256x%d, %d tables, %d levels", fh, v->cmap_tables, v->cmap_levels);
    } else {
        // Synthesised: darken toward the table's cast. Good enough that a checkout with no colormap
        // still has a night; the file replaces it the moment it is there.
        static const float CAST[6][3] = { {1,1,1}, {1.05f,0.92f,0.80f}, {0.62f,0.72f,1.0f},
                                          {1.2f,1.2f,1.1f}, {0.8f,1.1f,0.8f}, {0.9f,0.9f,0.95f} };
        int h = 6 * VX_LEVELS;
        unsigned char *px = (unsigned char *)calloc((size_t)256 * h, 4);
        if (!px) return;
        for (int tb = 0; tb < 6; tb++) for (int lv = 0; lv < VX_LEVELS; lv++) {
            float k = 1.0f - (float)lv / (VX_LEVELS - 1) * 0.85f;
            unsigned char *row = px + ((size_t)(tb * VX_LEVELS + lv) * 256) * 4;
            for (int i = 1; i < 256; i++) {
                for (int c = 0; c < 3; c++) {
                    float x = v->pal[i][c] / 255.0f * k * CAST[tb][c];
                    row[i * 4 + c] = (unsigned char)(255.0f * (x < 0 ? 0 : x > 1 ? 1 : x) + 0.5f);
                }
                row[i * 4 + 3] = 255;
            }
        }
        v->cmap_h = h;
        if (!v->cmap) glGenTextures(1, &v->cmap);
        glBindTexture(GL_TEXTURE_2D, v->cmap);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        free(v->cmap_px); v->cmap_px = px;
        SDL_Log("voxfield: colormap synthesised on the CPU (no palette/colormap.png)");
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

int vx_table_by_name(VoxField *v, const char *name) {
    for (int i = 0; i < v->cmap_tables && i < 8; i++) if (!SDL_strcasecmp(v->cmap_tname[i], name)) return i;
    return 0;
}

// The colour a palette index takes under the current light — for the fog and the sky, which are
// solved on the CPU because they are one colour a frame, not one a pixel.
void vx_cpu_colour(VoxField *v, int idx, float light, float out[3]) {
    out[0] = out[1] = out[2] = 0.3f;
    if (!v->cmap_px) return;
    float f = (1.0f - (light < 0 ? 0 : light > 1 ? 1 : light)) * (v->cmap_levels - 1);
    int row = v->cmap_row0[v->light_table < v->cmap_tables ? v->light_table : 0] + (int)(f + 0.5f);
    if (row < 0) row = 0;
    if (row >= v->cmap_h) row = v->cmap_h - 1;
    const unsigned char *p = v->cmap_px + ((size_t)row * 256 + (idx & 255)) * 4;
    for (int c = 0; c < 3; c++) out[c] = p[c] / 255.0f;
}

// ───────────────────────── indexing art on the palette ─────────────────────────

#define VXQ_SLOTS 65536
struct VxQCache { uint32_t key[VXQ_SLOTS]; unsigned char val[VXQ_SLOTS]; int colours; };

static unsigned char vx_quant(VoxField *v, VxQCache *q, uint32_t rgba) {
    if ((rgba >> 24) < 128) return 0;
    uint32_t k = rgba | 0xFF000000u;
    uint32_t s = ((k * 2654435761u) >> 12) & (VXQ_SLOTS - 1);
    int slot = -1;
    for (int probe = 0; probe < 64; probe++, s = (s + 1) & (VXQ_SLOTS - 1)) {
        if (q->key[s] == k) return q->val[s];
        if (!q->key[s]) { slot = (int)s; break; }
    }
    int r = (int)(k & 255), g = (int)((k >> 8) & 255), b = (int)((k >> 16) & 255);
    int best = 1;
    for (int i = 1; i < 256; i++)
        if (v->pal[i][0] == r && v->pal[i][1] == g && v->pal[i][2] == b) { best = i; goto done; }
    {
        float L, A, B2, bd = 1e9f;
        rgb_to_oklab(r / 255.0f, g / 255.0f, b / 255.0f, &L, &A, &B2);
        for (int i = 1; i < 256; i++) {
            float d = (L - v->pal_lab[i][0]) * (L - v->pal_lab[i][0])
                    + (A - v->pal_lab[i][1]) * (A - v->pal_lab[i][1])
                    + (B2 - v->pal_lab[i][2]) * (B2 - v->pal_lab[i][2]);
            if (d < bd) { bd = d; best = i; }
        }
        v->off_pal++;
    }
done:
    if (slot >= 0) { q->key[slot] = k; q->val[slot] = (unsigned char)best; q->colours++; }
    return (unsigned char)best;
}

// RGBA -> an R8 texture of palette indices, the same currency the world is drawn in, so a sprite and
// a block light through one colormap row and never disagree about a colour.
GLuint vx_tex_indexed(VoxField *v, const char *label, const unsigned char *rgba, int w, int h) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (!v->pal_ok) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    } else {
        unsigned char *idx = (unsigned char *)malloc((size_t)w * h);
        VxQCache *q = (VxQCache *)calloc(1, sizeof(VxQCache));
        if (!idx || !q) { free(idx); free(q); return tex; }
        for (size_t i = 0; i < (size_t)w * h; i++) idx[i] = vx_quant(v, q, ((const uint32_t *)rgba)[i]);
        SDL_Log("voxfield: %s indexed — %dx%d, %d colours", label, w, h, q->colours);
        free(q);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, idx);
        free(idx);
    }
    // NEAREST both ways: the owner's pixels stay pixels (the HD-2D pass must not smooth texels).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

// ───────────────────────── tiles.md: only the shape of each entry ─────────────────────────
// The voxel world needs to know how big a stamp is and what it is called. Colours, atlas indices,
// masks, swatches and decal scatter rules all belong to the tile field; none of it is read here.

int def_by_name(VoxField *v, const char *name) {
    for (int i = 0; i < v->def_count; i++) if (!strcmp(v->defs[i].name, name)) return i;
    return -1;
}

static void parse_solid_rows(VxDef *d, const char *val) {
    memset(d->solid, 0, sizeof(d->solid));
    if (!strcmp(val, "yes")) { for (int r = 0; r < d->h && r < 8; r++) d->solid[r] = (unsigned char)((1 << d->w) - 1); return; }
    if (!strcmp(val, "no")) return;
    int r = 0, c = 0;
    for (const char *p = val; *p && r < 8; p++) {
        if (*p == '/') { r++; c = 0; continue; }
        if (*p == '#' && c < 8) d->solid[r] |= (unsigned char)(1 << c);
        if (*p == '#' || *p == '.') c++;
    }
}

bool vx_load_tileset(VoxField *v, const char *set) {
    if (!strcmp(v->set, set) && v->def_count) return true;
    snprintf(v->set, sizeof(v->set), "%s", set);
    v->def_count = 0;
    char rel[128];
    snprintf(rel, sizeof(rel), "field/tilesets/%s/tiles.md", set);
    size_t sz = 0;
    char *text = (char *)vx_read(rel, &sz);
    if (!text) { SDL_Log("voxfield: no %s — stamps will be 1x1", rel); return false; }
    VxDef *d = nullptr;
    char *save = nullptr;
    for (char *line = SDL_strtok_r(text, "\n", &save); line; line = SDL_strtok_r(nullptr, "\n", &save)) {
        char *s = vx_trim(line);
        if (s[0] == '#' && s[1] == '#' && s[2] != '#') {
            if (v->def_count >= VX_DEFS) { d = nullptr; continue; }
            d = &v->defs[v->def_count++];
            memset(d, 0, sizeof(*d));
            snprintf(d->name, sizeof(d->name), "%s", vx_trim(s + 2));
            d->w = d->h = 1; d->index = -1; d->kind = DK_TILE;
            continue;
        }
        if (!d || s[0] != '-') continue;
        char *val = strchr(s, ':');
        if (!val) continue;
        *val = 0;
        char *key = vx_trim(s + 1);
        val = vx_trim(val + 1);
        if (!strcmp(key, "index")) {
            int i = -1, w = 1, h = 1;
            int n = sscanf(val, "%d %dx%d", &i, &w, &h);
            d->index = (short)i;
            if (n >= 3) { d->w = (short)w; d->h = (short)h; }
        } else if (!strcmp(key, "kind")) {
            d->kind = !strcmp(val, "terrain") ? DK_TERRAIN : !strcmp(val, "decal") ? DK_DECAL : DK_TILE;
        } else if (!strcmp(key, "solid")) parse_solid_rows(d, val);
    }
    SDL_free(text);
    // `- solid: yes` may come before `- index: N WxH`, so re-expand it now the size is known.
    for (int i = 0; i < v->def_count; i++) {
        VxDef *e = &v->defs[i];
        bool any = false;
        for (int r = 0; r < 8; r++) if (e->solid[r]) any = true;
        if (any && e->solid[0] == 1 && e->w > 1) for (int r = 0; r < e->h && r < 8; r++) e->solid[r] = (unsigned char)((1 << e->w) - 1);
    }
    SDL_Log("voxfield: tileset %s — %d entries", set, v->def_count);
    return true;
}

