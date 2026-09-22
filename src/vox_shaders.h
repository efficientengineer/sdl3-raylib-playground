// vox_shaders.h — every GLSL source string the voxel field compiles, and nothing else.
// OWNS: the world vertex/fragment shaders (and the cutaway variant), the billboard pair, the
//       fullscreen quad, blur, HD-2D post, sky, nav-overlay and overdraw-probe shaders.
// NEVER: contains C++ code, GL calls or state — it is text. Included once, by vox_render.cpp.
// EXPOSES: VX_VS, VX_FS_HEAD/_CUT/_BODY, VX_SPR_VS/FS, VX_QUAD_VS, VX_BLUR_FS, VX_POST_FS,
//          VX_SKY_FS, VX_NAV_VS/FS, VX_OD_FS, VX_SHADOW_VS/FS.
// The rule that outlives this file: no loops anywhere in GLSL — a small `for` was miscompiled by
// the Mac driver once in this repo and the habit stays. VX_PREFIX (the #version line) is in
// vox_render.cpp, which is what pastes these together.
#pragma once
static const char *VX_SHADOW_VS =
    "layout(location=0) in vec3 a_pos;\n"
    "uniform mat4 u_lmvp;\n"
    "void main(){ gl_Position = u_lmvp * vec4(a_pos, 1.0); }\n";
static const char *VX_SHADOW_FS = "void main(){}\n";

static const char *VX_VS =
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in vec2 a_uw;\n"             // the pattern basis, in world units
    "layout(location=2) in uvec4 a_meta;\n"          // type, face, ao(0..255), spare
    "layout(location=3) in vec2 a_light;\n"          // face term, baked lamp
    "layout(location=4) in vec3 a_nrm;\n"
    "uniform mat4 u_mvp;\n"
    "uniform mat4 u_view;\n"
    "uniform mat4 u_lmvp;\n"
    "uniform float u_snap, u_noff;\n"
    "out vec3 v_world;\n"
    "flat out uint v_type;\n"
    "flat out uint v_face;\n"
    "out float v_ao;\n"
    "out vec2 v_light;\n"
    "out vec2 v_uw;\n"
    "out float v_spare;\n"
    "out vec3 v_nrm;\n"
    "out vec3 v_lpos;\n"
    "out float v_viewz;\n"
    "void main(){\n"
    "  v_world = a_pos;\n"
    "  v_uw = a_uw;\n"
    "  v_type = a_meta.x; v_face = a_meta.y; v_ao = float(a_meta.z) / 255.0;\n"
    "  v_spare = float(a_meta.w);\n"
    "  v_light = a_light;\n"
    "  vec3 n = normalize(a_nrm);\n"
    "  v_nrm = n;\n"
    // Snapped: the shadow is looked up at the 1/16 texel grid, so its edge is a pixel-art staircase
    // that belongs to the same grid the patterns are drawn on. Smooth: the true position.
    "  vec3 sp = mix(a_pos, (floor(a_pos * 16.0) + 0.5) / 16.0, u_snap);\n"
    "  vec4 lp = u_lmvp * vec4(sp + n * u_noff, 1.0);\n"
    "  v_lpos = lp.xyz / lp.w * 0.5 + 0.5;\n"
    "  vec4 vp = u_view * vec4(a_pos, 1.0);\n"
    "  v_viewz = -vp.z;\n"
    "  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
    "}\n";

static const char *VX_FS_HEAD =
    "in vec3 v_world;\n"
    "flat in uint v_type;\n"
    "flat in uint v_face;\n"
    "in float v_ao;\n"
    "in vec2 v_light;\n"
    "in vec2 v_uw;\n"
    "in float v_spare;\n"
    "in vec3 v_nrm;\n"
    "in vec3 v_lpos;\n"
    "in float v_viewz;\n"
    "uniform sampler2D u_lut;\n"
    "uniform sampler2D u_cmap;\n"
    "uniform sampler2DShadow u_shadow;\n"
    "uniform float u_amb, u_ao, u_levels, u_cmaph, u_rowa, u_rowb, u_time;\n"
    "uniform float u_fog, u_fognear, u_fogfar;\n"
    "uniform float u_shstr, u_shsoft, u_shtexel;\n"
    "uniform vec3 u_sundir;\n"
    "uniform vec3 u_sky;\n"
    "uniform vec4 u_player;\n"                 // screen x, y, view z, cutaway radius in px
    // The benchmark's knobs. All three are uniform branches — one value for the whole draw, so the
    // wavefront never diverges — and all three sit at the shipping look unless a benchmark moved them.
    "uniform float u_qpattern, u_qpcf, u_qwater;\n"
    // The one DYNAMIC point light (vx_set_party_lamp): xyz = world position, w = radius in world
    // units; u_dlev is its level, 0 with the lamp out. A uniform branch — one value for the whole
    // draw, so no wavefront ever diverges on it.
    "uniform vec4 u_dlamp;\n"
    "uniform float u_dlev;\n"
    "out vec4 o;\n"
    // Four rotated-poisson taps, UNROLLED (this repo has been bitten by a driver miscompiling a
    // GLSL loop), each one hardware-PCF'd, so the edge is soft without being mush.
    "float shadow_at(vec3 lp, float soft){\n"
    "  if (lp.z > 1.0 || lp.x < 0.0 || lp.x > 1.0 || lp.y < 0.0 || lp.y > 1.0) return 1.0;\n"
    "  float r = u_shtexel * soft;\n"
    "  if (u_qpcf < 1.5) return texture(u_shadow, vec3(lp.xy, lp.z));\n"
    "  float s = texture(u_shadow, vec3(lp.xy + vec2( 0.94, 0.34) * r, lp.z));\n"
    "  s += texture(u_shadow, vec3(lp.xy + vec2(-0.85, 0.52) * r, lp.z));\n"
    "  s += texture(u_shadow, vec3(lp.xy + vec2(-0.20,-0.98) * r, lp.z));\n"
    "  s += texture(u_shadow, vec3(lp.xy + vec2( 0.40,-0.30) * r * 0.4, lp.z));\n"
    "  return s * 0.25;\n"
    "}\n"
    "float h21(vec2 p){ p = fract(p * vec2(127.11, 311.7)); p += dot(p, p + 34.77); return fract(p.x * p.y); }\n"
    "float vn(vec2 p){ vec2 i = floor(p), f = fract(p); f = f*f*(3.0-2.0*f);\n"
    "  float a = h21(i), b = h21(i+vec2(1,0)), c = h21(i+vec2(0,1)), d = h21(i+vec2(1,1));\n"
    "  return mix(mix(a,b,f.x), mix(c,d,f.x), f.y); }\n"
    "vec3 look(float idx, float row){ return texture(u_cmap, vec2((idx+0.5)/256.0, (row+0.5)/u_cmaph)).rgb; }\n"
    "void main(){\n";

// The cutaway, and it is a SEPARATE PROGRAM. It used to sit inside the one world shader under a
// uniform branch — but a fragment shader that contains `discard` anywhere is a shader the GPU cannot
// assume writes depth at the rasterized value, so Adreno turns LRZ (its early-Z / hidden-surface
// removal) OFF for the whole draw. Every chunk of the town was then shaded in full — ten hash
// evaluations and four shadow taps a fragment — for pixels a nearer wall went on to cover.
//
// So: two programs from the same source, one with this block and one without, and the cutaway one is
// used only for the chunks that could actually hold the fragments it discards (nearer than the party
// and overlapping the cutaway circle on screen — vx_chunk_needs_cut). Everything else runs the
// discard-free variant and keeps early-Z. The pixels that come out are identical either way, because
// a chunk that fails that test has no fragment the block would have discarded.
static const char *VX_FS_CUT =
    "  if (u_player.w > 0.5 && v_viewz < u_player.z - 0.6) {\n"
    "    float dsc = length(gl_FragCoord.xy - u_player.xy) / u_player.w;\n"
    "    if (dsc < 1.0) { if (dsc < 0.76 || h21(floor(gl_FragCoord.xy / 3.0)) > (dsc - 0.76) / 0.24) discard; }\n"
    "  }\n";

static const char *VX_FS_BODY =
    "  int ty = int(v_type);\n"
    "  int pat = int(texelFetch(u_lut, ivec2(12, ty), 0).r * 255.0 + 0.5);\n"
    "  if (u_qpattern < 0.5) pat = 99;\n"
    "  bool top = (v_face == 0u || v_face == 1u);\n"
    "  vec2 P = v_uw;\n"
    "  float vfrac = top ? 1.0 : fract(v_world.y * 2.0 + 0.0001);\n"
    "  vec2 T = floor(P * 16.0);\n"              // the block's own 16x16 pixel grid, in world space
    "  float s = 0.5; bool use_top = top; float glint = 0.0;\n"
    "  if (pat == 0) {\n"                        // mottle: earth, gravel, mud
    "    float n = vn(T / 3.1) * 0.6 + h21(T * 0.71) * 0.4;\n"
    "    s = n < 0.30 ? 0.22 : n < 0.52 ? 0.44 : n < 0.74 ? 0.62 : n < 0.92 ? 0.80 : 1.0;\n"
    "  } else if (pat == 1) {\n"                 // grass: clumps on top, a lip of turf over the sides
    "    float n = vn(T / 2.6) * 0.65 + h21(T * 0.37) * 0.35;\n"
    "    if (!top && vfrac < 0.80) {\n"
    "      use_top = false;\n"
    "      float m = vn(T / 3.0) * 0.6 + h21(T * 0.53) * 0.4;\n"
    "      s = m < 0.34 ? 0.24 : m < 0.60 ? 0.46 : m < 0.84 ? 0.66 : 0.86;\n"
    "    } else {\n"
    "      use_top = true;\n"
    "      float edge = top ? 1.0 : smoothstep(0.80, 0.86, vfrac + h21(T * 0.91) * 0.05);\n"
    "      s = n < 0.26 ? 0.18 : n < 0.48 ? 0.42 : n < 0.70 ? 0.62 : n < 0.88 ? 0.82 : 1.0;\n"
    "      s = mix(0.45, s, edge);\n"
    "    }\n"
    "  } else if (pat == 2) {\n"                 // cobble: a warped grid, dark mortar, a lit top lip
    "    vec2 Q = T + vec2(vn(T / 5.0) * 1.7, vn(T / 6.0 + 4.0) * 1.3);\n"
    "    vec2 cs = vec2(6.0, 4.0);\n"
    "    float row = floor(Q.y / cs.y);\n"
    "    float ox = (h21(vec2(row, 2.0)) * 0.7 + mod(row, 2.0) * 0.4) * cs.x;\n"
    "    vec2 cell = vec2(floor((Q.x + ox) / cs.x), row);\n"
    "    vec2 lp = vec2(mod(Q.x + ox, cs.x), mod(Q.y, cs.y));\n"
    "    s = 0.56 + (h21(cell) - 0.5) * 0.45;\n"
    "    if (lp.x < 1.0 || lp.y < 1.0) s = 0.10;\n"
    "    else if (lp.y < 2.0) s += 0.18;\n"
    "  } else if (pat == 3) {\n"                 // planks
    "    float pw = 4.0;\n"
    "    float pi = floor((top ? T.y : T.x) / pw), lp = mod((top ? T.y : T.x), pw);\n"
    "    s = 0.55 + (h21(vec2(pi, 3.0)) - 0.5) * 0.36;\n"
    "    if (lp < 1.0) s = 0.14;\n"
    "    float along = top ? T.x : T.y;\n"
    "    if (mod(along + h21(vec2(pi, floor(along / 22.0))) * 12.0, 22.0) < 1.0) s = 0.20;\n"
    "  } else if (pat == 4) {\n"                 // roof tiles: short courses, each with a lit lip
    "    float ch2 = 3.0;\n"
    "    float row = floor(T.y / ch2), lp = mod(T.y, ch2);\n"
    "    float ox = mod(row, 2.0) * 2.0;\n"
    "    float cell = floor((T.x + ox) / 4.0);\n"
    "    s = 0.55 + (h21(vec2(cell, row)) - 0.5) * 0.30;\n"
    "    if (lp < 1.0) s = 0.12;\n"
    "    else if (lp < 1.8) s += 0.22;\n"
    "    if (mod(T.x + ox, 4.0) < 1.0) s -= 0.16;\n"
    "  } else if (pat == 5) {\n"                 // water: bands quantised to 6 fps, with hard glints
    "    float tq = floor(u_time * 6.0) / 6.0 * u_qwater;\n"
    "    float a = vn(vec2(P.x * 1.4 - tq * 0.7, P.y * 0.8));\n"
    "    float b = vn(vec2(P.x * 3.1 - tq * 1.3, P.y * 1.9 + tq * 0.2));\n"
    "    s = 0.18;\n"
    "    if (a > 0.46) s = 0.42;\n"
    "    if (b > 0.60) s = 0.70;\n"
    "    float sp = h21(T + floor(tq * 6.0) * 7.0);\n"
    "    if (sp > 0.992 && b > 0.5) { s = 1.0; glint = 1.0; }\n"
    "  } else if (pat == 6) {\n"                 // thatch: long combed straw
    "    float n = vn(vec2(T.x * 0.8, T.y * 0.12));\n"
    "    s = n < 0.34 ? 0.28 : n < 0.58 ? 0.52 : n < 0.82 ? 0.72 : 0.92;\n"
    "    if (mod(T.y, 5.0) < 1.0) s -= 0.22;\n"
    "  } else if (pat == 7) {\n"                 // crop rows
    "    float lp = mod(T.y, 4.0);\n"
    "    s = 0.34 + vn(T / 7.0) * 0.10;\n"
    "    if (lp < 1.5) s = 0.66;\n"
    "    if (lp < 2.5 && h21(vec2(floor(T.x / 2.0), floor(T.y / 4.0))) > 0.45) s = 0.90;\n"
    "  } else if (pat == 8) {\n"                 // leaves: chunky blobs, a little sky through them
    "    float n = vn(T / 3.4) * 0.7 + h21(T * 0.29) * 0.3;\n"
    "    s = n < 0.30 ? 0.12 : n < 0.50 ? 0.38 : n < 0.70 ? 0.60 : n < 0.88 ? 0.82 : 1.0;\n"
    "  } else if (pat == 9) {\n"                 // bark
    "    float n = vn(vec2(T.x * 1.6, T.y * 0.20));\n"
    "    s = n < 0.36 ? 0.18 : n < 0.62 ? 0.46 : n < 0.85 ? 0.70 : 0.92;\n"
    "  } else if (pat == 11) {\n"                // dressed stone: big courses
    "    vec2 cs = vec2(8.0, 5.0);\n"
    "    float row = floor(T.y / cs.y);\n"
    "    float ox = mod(row, 2.0) * 4.0;\n"
    "    vec2 cell = vec2(floor((T.x + ox) / cs.x), row);\n"
    "    vec2 lp = vec2(mod(T.x + ox, cs.x), mod(T.y, cs.y));\n"
    "    s = 0.56 + (h21(cell) - 0.5) * 0.30;\n"
    "    if (lp.x < 1.0 || lp.y < 1.0) s = 0.14;\n"
    "    else if (lp.y < 2.0) s += 0.16;\n"
    "  } else if (pat == 12) {\n"                // plaster: nearly flat, a few worn patches
    "    float n = vn(T / 6.0);\n"
    "    s = 0.76 + (n - 0.5) * 0.34;\n"
    "    if (h21(floor(T / 7.0) + 3.3) > 0.90) s -= 0.26;\n"
    "  } else {\n"
    "    s = 0.62;\n"
    "  }\n"
    // the shallows: a lighter band of water near the bank, and a dark one under the north bank, both
    // from a per-vertex distance-to-bank baked at mesh time
    "  if (pat == 5) {\n"
    "    float shal = mod(v_spare, 128.0) / 127.0;\n"
    "    float nb = step(127.5, v_spare);\n"
    "    s = clamp(s + shal * 0.30 - nb * 0.34, 0.0, 1.0);\n"
    "  }\n"
    "  int step6 = int(clamp(floor(s * 5.0 + 0.5), 0.0, 5.0));\n"
    "  float idx = texelFetch(u_lut, ivec2(step6 + (use_top ? 0 : 6), ty), 0).r * 255.0;\n"
    "  float ao = 1.0 - (1.0 - v_ao) * u_ao;\n"
    "  float lamp = v_light.y;\n"
    // THE LANTERN'S CAP, and it is a look decision (owner: warm straw, not neon). Two numbers:
    // the falloff is a SMOOTHSTEP, not a square — a square puts nearly all of the light in the
    // first cell and the pool reads as a hard bright disc about two cells across — and the level is
    // capped below the top of the `lamp` colormap row, because the top of that row is the paper
    // white the table ends on and a pool that reaches it is a headlight. 0.62 keeps the brightest
    // straw inside the warm part of the ramp.
    "  if (u_dlev > 0.0005) {\n"
    "    float dd = distance(v_world, u_dlamp.xyz) / max(u_dlamp.w, 0.001);\n"
    "    float ff = clamp(1.0 - dd, 0.0, 1.0);\n"
    "    ff = ff * ff * (3.0 - 2.0 * ff);\n"
    "    lamp = min(" VX_LAMP_CAP_S ", lamp + u_dlev * ff);\n"
    "  }\n"
    // The cast shadow is a LIGHT LEVEL, not a multiply toward black: it pulls the colormap lookup
    // down the table, so shade stays a cool palette colour (PALETTE.md / D19).
    "  float ndl = clamp(dot(v_nrm, u_sundir), 0.0, 1.0);\n"
    "  float sh = (ndl > 0.02 && u_shstr > 0.002) ? shadow_at(v_lpos, u_shsoft) : 1.0;\n"
    "  float shk = 1.0 - (1.0 - sh) * u_shstr * 0.55;\n"
    "  float lv = clamp(u_amb * v_light.x * ao * shk, 0.0, 1.0);\n"
    "  float warm = clamp((lamp - u_amb) * 1.8, 0.0, 1.0);\n"
    "  lv = max(lv, lamp);\n"
    "  float f = (1.0 - lv) * (u_levels - 1.0);\n"
    "  float r0 = floor(f), fr = f - r0, r1 = min(r0 + 1.0, u_levels - 1.0);\n"
    "  vec3 cA = mix(look(idx, u_rowa + r0), look(idx, u_rowa + r1), fr);\n"
    "  vec3 col = warm > 0.002 ? mix(cA, mix(look(idx, u_rowb + r0), look(idx, u_rowb + r1), fr), warm) : cA;\n"
    "  if (glint > 0.5) col = mix(col, vec3(1.0), 0.45);\n"
    "  float fg = u_fog * smoothstep(u_fognear, u_fogfar, v_viewz);\n"
    "  col = mix(col, u_sky, clamp(fg, 0.0, 0.92));\n"
    "  o = vec4(col, 1.0);\n"
    "}\n";

static const char *VX_SPR_VS =
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_par;\n"          // lit, warm, alpha, kind
    "uniform mat4 u_mvp;\n"
    "uniform mat4 u_view;\n"
    "uniform mat4 u_lmvp;\n"
    "out vec2 v_uv; out vec4 v_par; out float v_viewz; out vec3 v_lpos;\n"
    "void main(){ v_uv = a_uv; v_par = a_par;\n"
    "  vec4 vp = u_view * vec4(a_pos, 1.0); v_viewz = -vp.z;\n"
    "  vec4 lp = u_lmvp * vec4(a_pos + vec3(0.0, 0.12, 0.0), 1.0);\n"
    "  v_lpos = lp.xyz / lp.w * 0.5 + 0.5;\n"
    "  gl_Position = u_mvp * vec4(a_pos, 1.0); }\n";

static const char *VX_SPR_FS =
    "in vec2 v_uv; in vec4 v_par; in float v_viewz; in vec3 v_lpos;\n"
    "uniform sampler2D u_tex; uniform sampler2D u_cmap;\n"
    "uniform sampler2DShadow u_shadow;\n"
    "uniform float u_amb, u_levels, u_cmaph, u_rowa, u_rowb, u_indexed;\n"
    "uniform float u_shstr, u_shsoft, u_shtexel;\n"
    "uniform float u_fog, u_fognear, u_fogfar; uniform vec3 u_sky;\n"
    "out vec4 o;\n"
    "vec3 look(float idx, float row){ return texture(u_cmap, vec2((idx+0.5)/256.0, (row+0.5)/u_cmaph)).rgb; }\n"
    // A character walking into a roof's or a tree's shadow darkens with it: the same map, sampled a
    // little above the feet so the ground they stand on is what decides.
    "float spr_shadow(vec3 lp){\n"
    "  if (lp.z > 1.0 || lp.x < 0.0 || lp.x > 1.0 || lp.y < 0.0 || lp.y > 1.0) return 1.0;\n"
    "  float r = u_shtexel * max(u_shsoft, 1.5);\n"
    "  float s = texture(u_shadow, vec3(lp.xy + vec2( 0.94, 0.34) * r, lp.z));\n"
    "  s += texture(u_shadow, vec3(lp.xy + vec2(-0.85, 0.52) * r, lp.z));\n"
    "  s += texture(u_shadow, vec3(lp.xy + vec2(-0.20,-0.98) * r, lp.z));\n"
    "  s += texture(u_shadow, vec3(lp.xy, lp.z));\n"
    "  return s * 0.25;\n"
    "}\n"
    "void main(){\n"
    "  int kind = int(v_par.w + 0.5);\n"
    "  if (kind == 1) {\n"                        // a blob shadow: no texture, a soft disc
    "    float d = length(v_uv - 0.5) * 2.0;\n"
    "    float a = (1.0 - smoothstep(0.55, 1.0, d)) * v_par.z;\n"
    "    if (a < 0.02) discard;\n"
    "    o = vec4(0.0, 0.0, 0.0, a); return; }\n"
    "  if (kind == 3) {\n"                        // a PLACEHOLDER field sprite: one flat palette band
    "    float idx = v_par.x * 255.0;\n"
    "    float lamp = v_par.y;\n"
    "    float lv = max(clamp(u_amb, 0.0, 1.0), lamp);\n"
    "    float warm = clamp((lamp - u_amb) * 1.8, 0.0, 1.0);\n"
    "    float f = (1.0 - lv) * (u_levels - 1.0);\n"
    "    float r0 = floor(f), fr = f - r0, r1 = min(r0 + 1.0, u_levels - 1.0);\n"
    "    vec3 cA = mix(look(idx, u_rowa + r0), look(idx, u_rowa + r1), fr);\n"
    "    vec3 c3 = warm > 0.002 ? mix(cA, mix(look(idx, u_rowb + r0), look(idx, u_rowb + r1), fr), warm) : cA;\n"
    "    float e = min(min(v_uv.x, 1.0 - v_uv.x), min(v_uv.y, 1.0 - v_uv.y));\n"
    "    if (e < 0.06) c3 *= 0.45;\n"
    "    float fg3 = u_fog * smoothstep(u_fognear, u_fogfar, v_viewz);\n"
    "    o = vec4(mix(c3, u_sky, clamp(fg3, 0.0, 0.92)), 1.0); return; }\n"
    "  if (kind == 2) {\n"                        // an additive lamp glow
    "    float d = length(v_uv - 0.5) * 2.0;\n"
    "    float a = pow(clamp(1.0 - d, 0.0, 1.0), 2.2) * v_par.z;\n"
    "    if (a < 0.004) discard;\n"
    "    o = vec4(vec3(1.0, 0.86, 0.60) * a, 0.0); return; }\n"
    "  vec4 t = texture(u_tex, v_uv);\n"
    "  vec3 col;\n"
    "  if (u_indexed > 0.5) {\n"
    "    float idx = t.r * 255.0;\n"
    "    if (idx < 0.5) discard;\n"
    "    float lamp = v_par.y;\n"
    "    float sh = u_shstr > 0.002 ? spr_shadow(v_lpos) : 1.0;\n"
    "    float shk = 1.0 - (1.0 - sh) * u_shstr * 0.55;\n"
    "    float lv = max(clamp(u_amb * v_par.x * shk, 0.0, 1.0), lamp);\n"
    "    float warm = clamp((lamp - u_amb) * 1.8, 0.0, 1.0);\n"
    "    float f = (1.0 - lv) * (u_levels - 1.0);\n"
    "    float r0 = floor(f), fr = f - r0, r1 = min(r0 + 1.0, u_levels - 1.0);\n"
    "    vec3 cA = mix(look(idx, u_rowa + r0), look(idx, u_rowa + r1), fr);\n"
    "    col = warm > 0.002 ? mix(cA, mix(look(idx, u_rowb + r0), look(idx, u_rowb + r1), fr), warm) : cA;\n"
    "  } else { if (t.a < 0.5) discard; col = t.rgb * v_par.x; }\n"
    "  float fg = u_fog * smoothstep(u_fognear, u_fogfar, v_viewz);\n"
    "  col = mix(col, u_sky, clamp(fg, 0.0, 0.92));\n"
    "  o = vec4(col, 1.0);\n"
    "}\n";

// The HD-2D pass. One full-screen triangle; the blur is a separable 5-tap at half resolution, so the
// expensive part runs on a quarter of the pixels. Every term has its own strength and can be zero.
static const char *VX_QUAD_VS =
    "out vec2 v_uv;\n"
    "void main(){\n"
    "  vec2 p = vec2((gl_VertexID == 2) ? 3.0 : -1.0, (gl_VertexID == 1) ? 3.0 : -1.0);\n"
    "  v_uv = (p + 1.0) * 0.5;\n"
    "  gl_Position = vec4(p, 1.0, 1.0); }\n";

static const char *VX_BLUR_FS =
    "in vec2 v_uv;\n"
    "uniform sampler2D u_tex; uniform vec2 u_dir; uniform float u_thresh;\n"
    "out vec4 o;\n"
    "void main(){\n"
    "  vec4 c0 = texture(u_tex, v_uv);\n"
    "  vec4 c1 = texture(u_tex, v_uv + u_dir);\n"
    "  vec4 c2 = texture(u_tex, v_uv - u_dir);\n"
    "  vec4 c3 = texture(u_tex, v_uv + u_dir * 2.4);\n"
    "  vec4 c4 = texture(u_tex, v_uv - u_dir * 2.4);\n"
    "  vec4 c = c0 * 0.30 + (c1 + c2) * 0.245 + (c3 + c4) * 0.105;\n"
    "  if (u_thresh > 0.0) {\n"
    "    float l = dot(c.rgb, vec3(0.299, 0.587, 0.114));\n"
    "    c.rgb *= smoothstep(u_thresh, u_thresh + 0.22, l); }\n"
    "  o = c; }\n";

static const char *VX_POST_FS =
    "in vec2 v_uv;\n"
    "uniform sampler2D u_scene; uniform sampler2D u_blur; uniform sampler2D u_bloom;\n"
    "uniform sampler2D u_depth;\n"
    "uniform float u_dof, u_bloom_k, u_vig, u_grade, u_pdepth, u_near, u_far, u_band, u_ortho;\n"
    "out vec4 o;\n"
    "float lin(float d){ if (u_ortho > 0.5) return u_near + d * (u_far - u_near);\n"
    "  float z = d * 2.0 - 1.0; return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near)); }\n"
    "void main(){\n"
    "  vec3 c = texture(u_scene, v_uv).rgb;\n"
    "  if (u_dof > 0.0) {\n"
    // Tilt shift: sharp in a band around the party's own depth, blurred away from it. The depth
    // texture is the honest version; the band is in world units, so the focus follows the player.
    "    float d = lin(texture(u_depth, v_uv).r);\n"
    "    float k = clamp((abs(d - u_pdepth) - u_band) / (u_band * 2.2), 0.0, 1.0);\n"
    "    c = mix(c, texture(u_blur, v_uv).rgb, k * u_dof); }\n"
    "  if (u_bloom_k > 0.0) c += texture(u_bloom, v_uv).rgb * u_bloom_k;\n"
    "  if (u_grade > 0.0) {\n"
    "    float l = dot(c, vec3(0.299, 0.587, 0.114));\n"
    "    vec3 warm = c * vec3(1.06, 1.01, 0.93), cool = c * vec3(0.92, 0.97, 1.10);\n"
    "    c = mix(c, mix(cool, warm, smoothstep(0.30, 0.80, l)), u_grade); }\n"
    "  if (u_vig > 0.0) {\n"
    "    vec2 q = (v_uv - 0.5) * vec2(1.0, 0.86);\n"
    "    c *= mix(1.0, clamp(1.12 - dot(q, q) * 1.9, 0.0, 1.0), u_vig); }\n"
    "  o = vec4(c, 1.0); }\n";

// The sky: a vertical gradient between two palette indices through the colormap, with a slow band of
// cloud across the top half. Drawn as one full-screen triangle before the world, which is also why
// the fog colour and the horizon are by construction the same colour.
static const char *VX_SKY_FS =
    "in vec2 v_uv;\n"
    "uniform sampler2D u_cmap;\n"
    "uniform float u_levels, u_cmaph, u_rowa, u_amb, u_time, u_horizon, u_cloud;\n"
    "uniform float u_iz, u_ih, u_ic;\n"                  // zenith, horizon, cloud palette indices
    "out vec4 o;\n"
    "float h21(vec2 p){ p = fract(p * vec2(127.11, 311.7)); p += dot(p, p + 34.77); return fract(p.x * p.y); }\n"
    "float vn(vec2 p){ vec2 i = floor(p), f = fract(p); f = f*f*(3.0-2.0*f);\n"
    "  float a = h21(i), b = h21(i+vec2(1,0)), c = h21(i+vec2(0,1)), d = h21(i+vec2(1,1));\n"
    "  return mix(mix(a,b,f.x), mix(c,d,f.x), f.y); }\n"
    "vec3 look(float idx){ float f = (1.0 - clamp(u_amb, 0.0, 1.0)) * (u_levels - 1.0);\n"
    "  float r0 = floor(f), fr = f - r0, r1 = min(r0 + 1.0, u_levels - 1.0);\n"
    "  vec3 a = texture(u_cmap, vec2((idx+0.5)/256.0, (u_rowa + r0 + 0.5)/u_cmaph)).rgb;\n"
    "  vec3 b = texture(u_cmap, vec2((idx+0.5)/256.0, (u_rowa + r1 + 0.5)/u_cmaph)).rgb;\n"
    "  return mix(a, b, fr); }\n"
    "void main(){\n"
    "  float t = clamp((v_uv.y - u_horizon) / max(1.0 - u_horizon, 0.001), 0.0, 1.0);\n"
    "  vec3 c = mix(look(u_ih), look(u_iz), t * t);\n"
    "  if (u_cloud > 0.0) {\n"
    "    vec2 p = vec2(v_uv.x * 5.0 + u_time * 0.012, (v_uv.y - u_horizon) * 9.0);\n"
    "    float n = vn(p) * 0.6 + vn(p * 2.3 + 7.0) * 0.4;\n"
    "    float band = smoothstep(0.12, 0.45, t) * (1.0 - smoothstep(0.55, 1.0, t));\n"
    "    float k = smoothstep(0.55, 0.72, n) * band * u_cloud;\n"
    "    c = mix(c, look(u_ic), k); }\n"
    "  o = vec4(c, 1.0); }\n";

// The overdraw probe. One constant 1/255 a fragment, blended ONE/ONE with the ordinary depth test
// and depth write, so what accumulates in a pixel is the number of times the opaque world WROTE
// that pixel — which is exactly the shading a perfect depth prepass (or a deferred pass) would have
// saved. It shares VX_VS, so it rasterizes the same triangles in the same places; it has no discard,
// so it does not itself defeat early-Z.
// u_step is 1/255 when the number is being measured (so the byte in the pixel IS the write count)
// and something visible when the debug view is being looked at.
// The Nav view (Dev toggle): flat translucent colour in world space. Debug only, one draw call.
static const char *VX_NAV_VS =
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in vec4 a_col;\n"
    "uniform mat4 u_mvp;\n"
    "out vec4 v_col;\n"
    "void main(){ v_col = a_col; gl_Position = u_mvp * vec4(a_pos,1.0); }\n";
static const char *VX_NAV_FS =
    "in vec4 v_col;\nout vec4 o_col;\nvoid main(){ o_col = v_col; }\n";

static const char *VX_OD_FS =
    "uniform float u_step;\n"
    "out vec4 o;\n"
    "void main(){ o = vec4(u_step, u_step * 0.55, u_step * 0.25, 1.0); }\n";
