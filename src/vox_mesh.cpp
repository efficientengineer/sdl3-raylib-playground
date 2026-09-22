// vox_mesh.cpp — the voxel grid becomes chunk VBOs.
// OWNS: face culling between blocks, the greedy-ish quad emission, per-vertex AO, the baked lamp
//       term, shape (slope/step) geometry, and the per-chunk bounds the renderer culls against.
// NEVER: compiles a shader, binds a program, or draws anything. It fills VxChunk vertex buffers.
// EXPOSES: vx_mesh_all, vx_lamp_at.
// Tested by: capture.sh --vox-selftest (triangle counts and mesh time per map) and the three image
// captures. See src/notes/rendering.md, "Meshing".
#include "vox_internal.h"

// ───────────────────────── meshing ─────────────────────────
// Once, at load, into one static VBO per 16x16 chunk. Hidden faces are culled; every vertex carries
// its baked ambient occlusion, the sun term for its face, whether a column shadows it, and the lamp
// light at that corner. Nothing about the light is computed per frame (CLAUDE.md: no realtime
// lighting) — the ambient level and the AO strength stay live because they are applied in the shader
// to numbers that were baked.

static const int FN[6][3] = { {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0} };
static const int F_OPP[6] = { F_BOT, F_TOP, F_NORTH, F_SOUTH, F_WEST, F_EAST };

// The face term, from the REAL normal, so a slope is lit between the two faces it lies between
// rather than snapping to one of them. The six axis normals reproduce the old FACE_L table exactly:
// top 1.00, bottom 0.42, south 0.90, north 0.56, east 0.78, west 0.66.
static float vx_face_light(float nx, float ny, float nz) {
    float horiz = 0.725f + 0.17f * nz + 0.06f * nx;
    float vert = ny > 0 ? 1.00f : 0.42f;
    float k = ny < 0 ? -ny : ny;
    return horiz + (vert - horiz) * k;
}

static VxVert *vx_tmp = nullptr;
static int vx_tmp_cap = 0, vx_tmp_n = 0;
static void tmp_push(const VxVert *v6) {
    if (vx_tmp_n + 6 > vx_tmp_cap) {
        vx_tmp_cap = vx_tmp_cap ? vx_tmp_cap * 2 : 262144;
        vx_tmp = (VxVert *)realloc(vx_tmp, sizeof(VxVert) * (size_t)vx_tmp_cap);
    }
    if (!vx_tmp) return;
    memcpy(vx_tmp + vx_tmp_n, v6, sizeof(VxVert) * 6);
    vx_tmp_n += 6;
}

static bool vx_opaque_at(VoxField *v, int x, int y, int z) { return blk_opaque(get_blk(v, x, y, z)); }
// For ambient occlusion only a full opaque cube occludes; a slope, slab, post or pane does not, which
// keeps the corners under a roof from going black.
static bool vx_ao_solid(VoxField *v, int x, int y, int z) {
    return blk_opaque(get_blk(v, x, y, z)) && SH_OF(get_shp(v, x, y, z)) == SH_CUBE;
}
// A face may be culled only against a neighbour that FILLS the shared boundary. When in doubt this
// says no and the face is drawn — the rule that stops shaped neighbours punching holes in the world.
static bool vx_covered(VoxField *v, int x, int y, int z, int face) {
    int nx = x + FN[face][0], ny = y + FN[face][1], nz = z + FN[face][2];
    unsigned char nb = get_blk(v, nx, ny, nz);
    if (!blk_opaque(nb)) return false;
    return shape_covers(get_shp(v, nx, ny, nz), F_OPP[face]);
}

void vx_lamp_at(VoxField *v, float x, float y, float z, float *lamp) {
    float s = 0;
    for (int i = 0; i < v->light_count; i++) {
        VxLight *l = &v->lights[i];
        float dx = x - l->x, dy = y - l->y, dz = z - l->z;
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        float f = 1.0f - d / (l->r > 0.001f ? l->r : 0.001f);
        if (f > 0) s += l->level * f * f;
    }
    *lamp = s > 1 ? 1 : s;
}

// How near a water voxel is to its bank, 0 (mid-channel) .. 1 (touching), and whether the bank it is
// nearest is the NORTH one — the camera-facing bank, which wants a dark band under it.
static void vx_water_edge(VoxField *v, int x, int z, float *shallow, float *north) {
    int best = 99, bn = 0;
    for (int r = 1; r <= 6; r++) {
        for (int d = 0; d < 4; d++) {
            int cx = (x + DIRV[d][0] * r) / VX_VPC, cz = (z + DIRV[d][1] * r) / VX_VPC;
            if (cx < 0 || cz < 0 || cx >= v->mw || cz >= v->md) continue;
            if (get_blk(v, x + DIRV[d][0] * r, 0, z + DIRV[d][1] * r) == B_AIR) continue;
            bool water = false;
            for (int y = 0; y < VX_VY; y++) if (get_blk(v, x + DIRV[d][0] * r, y, z + DIRV[d][1] * r) == B_WATER) water = true;
            if (water) continue;
            if (r < best) { best = r; bn = (d == D_N); }
        }
        if (best <= r) break;
    }
    *shallow = best > 6 ? 0.0f : 1.0f - (float)(best - 1) / 6.0f;
    *north = (best <= 2 && bn) ? 1.0f : 0.0f;
}

// One quad (or, with c[3] == c[2], one triangle) of one voxel. Corners are in VOXEL-LOCAL 0..1
// coordinates; `cullface` is the cell boundary the quad lies in, or -1 for an interior surface.
struct VxQuadCtx { VoxField *v; int x, y, z; unsigned char b; };
static void vx_emit_quad(VxQuadCtx *q, const float lc[4][3], const float n[3], int cullface) {
    VoxField *v = q->v;
    if (cullface >= 0 && vx_covered(v, q->x, q->y, q->z, cullface)) return;
    float c[4][3];
    for (int i = 0; i < 4; i++) {
        c[i][0] = ((float)q->x + lc[i][0]) * VOX_S;
        c[i][1] = ((float)q->y + lc[i][1]) * VOX_S;
        c[i][2] = ((float)q->z + lc[i][2]) * VOX_S;
    }
    // Winding is DERIVED, never typed: the cross product of the first two edges must agree with the
    // quad's own normal, else it is flipped. This is why no face can vanish under back-face culling.
    {
        float e1[3] = { c[1][0]-c[0][0], c[1][1]-c[0][1], c[1][2]-c[0][2] };
        float e2[3] = { c[2][0]-c[0][0], c[2][1]-c[0][1], c[2][2]-c[0][2] };
        float ax = e1[1]*e2[2]-e1[2]*e2[1], ay = e1[2]*e2[0]-e1[0]*e2[2], az = e1[0]*e2[1]-e1[1]*e2[0];
        if (ax*n[0] + ay*n[1] + az*n[2] < 0) {
            for (int k = 0; k < 3; k++) { float t = c[1][k]; c[1][k] = c[3][k]; c[3][k] = t; }
        }
    }
    // the face id: the dominant axis of the normal. It picks the top or side palette ramp and, for a
    // wall, the fraction up the block that the grass lip uses.
    int face;
    {
        float ax = n[0] < 0 ? -n[0] : n[0], ay = n[1] < 0 ? -n[1] : n[1], az = n[2] < 0 ? -n[2] : n[2];
        if (ay >= ax && ay >= az) face = n[1] > 0 ? F_TOP : F_BOT;
        else if (az >= ax)        face = n[2] > 0 ? F_SOUTH : F_NORTH;
        else                      face = n[0] > 0 ? F_EAST : F_WEST;
    }
    bool axis = (n[0]*n[0] + n[1]*n[1] + n[2]*n[2]) > 0.999f &&
                ((n[0] == 0 && n[1] == 0) || (n[0] == 0 && n[2] == 0) || (n[1] == 0 && n[2] == 0));
    // the tangent basis for AO
    float tu[3], tw[3];
    if (n[1] > 0.9f || n[1] < -0.9f) { tu[0]=1;tu[1]=0;tu[2]=0; }
    else { float l = sqrtf(n[2]*n[2] + n[0]*n[0]); tu[0]= n[2]/l; tu[1]=0; tu[2]= -n[0]/l; }
    tw[0] = n[1]*tu[2] - n[2]*tu[1]; tw[1] = n[2]*tu[0] - n[0]*tu[2]; tw[2] = n[0]*tu[1] - n[1]*tu[0];
    float ctr[3] = { (c[0][0]+c[1][0]+c[2][0]+c[3][0]) * 0.25f,
                     (c[0][1]+c[1][1]+c[2][1]+c[3][1]) * 0.25f,
                     (c[0][2]+c[1][2]+c[2][2]+c[3][2]) * 0.25f };
    float shade = vx_face_light(n[0], n[1], n[2]);
    float shallow = 0, north = 0;
    if (q->b == B_WATER && face == F_TOP) vx_water_edge(v, q->x, q->z, &shallow, &north);
    VxVert corner[4];
    for (int i = 0; i < 4; i++) {
        // the pattern basis. The six axis faces keep exactly the mapping they had, so nothing that
        // already looked right moved; a sloped face gets its own basis, with the run measured as arc
        // length so a roof course is the same width on the pitch as it is on the flat.
        float U, W;
        if (axis && (face == F_TOP || face == F_BOT))      { U = c[i][0]; W = c[i][2]; }
        else if (axis && (face == F_SOUTH || face == F_NORTH)) { U = c[i][0]; W = -c[i][1]; }
        else if (axis)                                     { U = c[i][2]; W = -c[i][1]; }
        else {
            float hl = sqrtf(n[0]*n[0] + n[2]*n[2]);
            float rx = hl > 0.001f ? n[0]/hl : 0.0f, rz = hl > 0.001f ? n[2]/hl : 1.0f;
            U = c[i][0]*rz - c[i][2]*rx;
            if (n[1] > 0.3f) W = (c[i][0]*rx + c[i][2]*rz) / (n[1] < 0.2f ? 0.2f : n[1]);
            else W = -c[i][1];
        }
        // classic three-neighbour ambient occlusion, generalised: sample the two edge neighbours and
        // the diagonal of the cell just outside the surface at this corner.
        float base[3] = { c[i][0]/VOX_S + n[0]*0.5f, c[i][1]/VOX_S + n[1]*0.5f, c[i][2]/VOX_S + n[2]*0.5f };
        float dv[3] = { c[i][0]-ctr[0], c[i][1]-ctr[1], c[i][2]-ctr[2] };
        float su = (dv[0]*tu[0]+dv[1]*tu[1]+dv[2]*tu[2]) < 0 ? -0.6f : 0.6f;
        float sw = (dv[0]*tw[0]+dv[1]*tw[1]+dv[2]*tw[2]) < 0 ? -0.6f : 0.6f;
        bool s1 = vx_ao_solid(v, (int)floorf(base[0]+tu[0]*su), (int)floorf(base[1]+tu[1]*su), (int)floorf(base[2]+tu[2]*su));
        bool s2 = vx_ao_solid(v, (int)floorf(base[0]+tw[0]*sw), (int)floorf(base[1]+tw[1]*sw), (int)floorf(base[2]+tw[2]*sw));
        bool cc = vx_ao_solid(v, (int)floorf(base[0]+tu[0]*su+tw[0]*sw), (int)floorf(base[1]+tu[1]*su+tw[1]*sw), (int)floorf(base[2]+tu[2]*su+tw[2]*sw));
        int ao = (s1 && s2) ? 0 : 3 - ((s1?1:0) + (s2?1:0) + (cc?1:0));
        float lamp = 0;
        vx_lamp_at(v, c[i][0], c[i][1], c[i][2], &lamp);
        if (q->b == B_WINDOW) lamp = 0.9f;             // lit from inside; the bloom catches it at night
        corner[i].x = c[i][0]; corner[i].y = c[i][1]; corner[i].z = c[i][2];
        corner[i].u = U; corner[i].w = W;
        corner[i].type = q->b; corner[i].face = (unsigned char)face;
        corner[i].lit = (unsigned char)(shade * 255.0f + 0.5f);
        corner[i].warm = (unsigned char)(lamp * 255.0f + 0.5f);
        corner[i].ao = (unsigned char)(ao * 85);
        corner[i].spare = (unsigned char)((int)(shallow * 127.0f) | (north > 0.5f ? 128 : 0));
        corner[i].nx = (signed char)(n[0] * 127.0f); corner[i].ny = (signed char)(n[1] * 127.0f);
        corner[i].nz = (signed char)(n[2] * 127.0f); corner[i].npad = 0;
    }
    VxVert q6[6];
    q6[0] = corner[0]; q6[1] = corner[1]; q6[2] = corner[2];
    q6[3] = corner[0]; q6[4] = corner[2]; q6[5] = corner[3];
    tmp_push(q6);
}

// An axis-aligned box inside one voxel, in local 0..1 coordinates. A face flush with the voxel
// boundary is cullable against the neighbour; a face inside the voxel never is.
static void vx_emit_box(VxQuadCtx *q, float x0, float y0, float z0, float x1, float y1, float z1) {
    const float EPS = 0.0005f;
    for (int f = 0; f < 6; f++) {
        float c[4][3]; float n[3] = { (float)FN[f][0], (float)FN[f][1], (float)FN[f][2] };
        int cull = -1;
        switch (f) {
            case F_TOP:   if (y1 > 1 - EPS) cull = f;
                c[0][0]=x0;c[0][1]=y1;c[0][2]=z0; c[1][0]=x1;c[1][1]=y1;c[1][2]=z0;
                c[2][0]=x1;c[2][1]=y1;c[2][2]=z1; c[3][0]=x0;c[3][1]=y1;c[3][2]=z1; break;
            case F_BOT:   if (y0 < EPS) cull = f;
                c[0][0]=x0;c[0][1]=y0;c[0][2]=z1; c[1][0]=x1;c[1][1]=y0;c[1][2]=z1;
                c[2][0]=x1;c[2][1]=y0;c[2][2]=z0; c[3][0]=x0;c[3][1]=y0;c[3][2]=z0; break;
            case F_SOUTH: if (z1 > 1 - EPS) cull = f;
                c[0][0]=x0;c[0][1]=y0;c[0][2]=z1; c[1][0]=x1;c[1][1]=y0;c[1][2]=z1;
                c[2][0]=x1;c[2][1]=y1;c[2][2]=z1; c[3][0]=x0;c[3][1]=y1;c[3][2]=z1; break;
            case F_NORTH: if (z0 < EPS) cull = f;
                c[0][0]=x1;c[0][1]=y0;c[0][2]=z0; c[1][0]=x0;c[1][1]=y0;c[1][2]=z0;
                c[2][0]=x0;c[2][1]=y1;c[2][2]=z0; c[3][0]=x1;c[3][1]=y1;c[3][2]=z0; break;
            case F_EAST:  if (x1 > 1 - EPS) cull = f;
                c[0][0]=x1;c[0][1]=y0;c[0][2]=z1; c[1][0]=x1;c[1][1]=y0;c[1][2]=z0;
                c[2][0]=x1;c[2][1]=y1;c[2][2]=z0; c[3][0]=x1;c[3][1]=y1;c[3][2]=z1; break;
            default:      if (x0 < EPS) cull = f;
                c[0][0]=x0;c[0][1]=y0;c[0][2]=z0; c[1][0]=x0;c[1][1]=y0;c[1][2]=z1;
                c[2][0]=x0;c[2][1]=y1;c[2][2]=z1; c[3][0]=x0;c[3][1]=y1;c[3][2]=z0; break;
        }
        vx_emit_quad(q, c, n, cull);
    }
}

// The ramp family — slope, outer corner, inner corner — as one prism defined by its four corner
// heights: two triangles for the top surface, a trapezoid per side, and a bottom.
static void vx_emit_prism(VxQuadCtx *q, const float h[4]) {
    static const float PX[4] = { 0, 1, 1, 0 }, PZ[4] = { 0, 0, 1, 1 };
    // the top, as two triangles, each with its own normal
    static const int TRI[2][3] = { {0,1,2}, {0,2,3} };
    for (int t = 0; t < 2; t++) {
        float c[4][3];
        for (int i = 0; i < 3; i++) { int k = TRI[t][i]; c[i][0]=PX[k]; c[i][1]=h[k]; c[i][2]=PZ[k]; }
        c[3][0]=c[2][0]; c[3][1]=c[2][1]; c[3][2]=c[2][2];
        float e1[3] = { c[1][0]-c[0][0], c[1][1]-c[0][1], c[1][2]-c[0][2] };
        float e2[3] = { c[2][0]-c[0][0], c[2][1]-c[0][1], c[2][2]-c[0][2] };
        float n[3] = { e1[1]*e2[2]-e1[2]*e2[1], e1[2]*e2[0]-e1[0]*e2[2], e1[0]*e2[1]-e1[1]*e2[0] };
        float l = sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        if (l < 1e-6f) continue;
        n[0]/=l; n[1]/=l; n[2]/=l;
        if (n[1] < 0) { n[0]=-n[0]; n[1]=-n[1]; n[2]=-n[2]; }
        vx_emit_quad(q, c, n, -1);
    }
    // the four sides; each is a trapezoid from y=0 to the two corner heights on that edge
    static const int SIDE[4][2] = { {3,2}, {0,1}, {1,2}, {0,3} };   // S, N, E, W  (in F_* order - 2)
    for (int s = 0; s < 4; s++) {
        int a = SIDE[s][0], b = SIDE[s][1];
        if (h[a] < 0.001f && h[b] < 0.001f) continue;
        int face = s + 2;
        float n[3] = { (float)FN[face][0], (float)FN[face][1], (float)FN[face][2] };
        float c[4][3];
        c[0][0]=PX[a]; c[0][1]=0;    c[0][2]=PZ[a];
        c[1][0]=PX[b]; c[1][1]=0;    c[1][2]=PZ[b];
        c[2][0]=PX[b]; c[2][1]=h[b]; c[2][2]=PZ[b];
        c[3][0]=PX[a]; c[3][1]=h[a]; c[3][2]=PZ[a];
        vx_emit_quad(q, c, n, (h[a] > 0.999f && h[b] > 0.999f) ? face : -1);
    }
    float c[4][3] = { {0,0,1}, {1,0,1}, {1,0,0}, {0,0,0} };
    float n[3] = { 0, -1, 0 };
    vx_emit_quad(q, c, n, F_BOT);
}

static void vx_emit_shape(VxQuadCtx *q, unsigned char s) {
    int sh = SH_OF(s), o = OR_OF(s);
    switch (sh) {
        case SH_CUBE: vx_emit_box(q, 0,0,0, 1,1,1); break;
        case SH_SLAB: if (o == SL_BOT) vx_emit_box(q, 0,0,0, 1,0.5f,1); else vx_emit_box(q, 0,0.5f,0, 1,1,1); break;
        case SH_SLOPE: case SH_SLOPE_OUT: case SH_SLOPE_IN: { float h[4]; shape_corner_h(s, h);
vx_emit_prism(q, h); } break;
        case SH_STAIRS: {
            for (int i = 0; i < 3; i++) {
                float t0 = (float)i / 3.0f, hgt = (float)(i + 1) / 3.0f;
                if (o == D_S)      vx_emit_box(q, 0, 0, t0, 1, hgt, 1);
                else if (o == D_N) vx_emit_box(q, 0, 0, 0, 1, hgt, 1.0f - t0);
                else if (o == D_E) vx_emit_box(q, t0, 0, 0, 1, hgt, 1);
                else               vx_emit_box(q, 0, 0, 0, 1.0f - t0, hgt, 1);
            }
        } break;
        case SH_POST: {
            vx_emit_box(q, 0.3f, 0, 0.3f, 0.7f, 1, 0.7f);
            // rails, reaching out to any neighbouring fence or solid cell
            for (int d = 0; d < 4; d++) {
                unsigned char nb = get_blk(q->v, q->x + DIRV[d][0], q->y, q->z + DIRV[d][1]);
                if (!blk_opaque(nb)) continue;
                for (int r = 0; r < 2; r++) {
                    float y0 = r ? 0.62f : 0.26f, y1 = y0 + 0.16f;
                    if (d == D_S)      vx_emit_box(q, 0.38f, y0, 0.6f, 0.62f, y1, 1.0f);
                    else if (d == D_N) vx_emit_box(q, 0.38f, y0, 0.0f, 0.62f, y1, 0.4f);
                    else if (d == D_E) vx_emit_box(q, 0.6f, y0, 0.38f, 1.0f, y1, 0.62f);
                    else               vx_emit_box(q, 0.0f, y0, 0.38f, 0.4f, y1, 0.62f);
                }
            }
        } break;
        case SH_PANE: {
            if (o == PN_S)      vx_emit_box(q, 0, 0, 0.82f, 1, 1, 1);
            else if (o == PN_N) vx_emit_box(q, 0, 0, 0, 1, 1, 0.18f);
            else if (o == PN_E) vx_emit_box(q, 0.82f, 0, 0, 1, 1, 1);
            else if (o == PN_W) vx_emit_box(q, 0, 0, 0, 0.18f, 1, 1);
            else if (o == PN_MIDX) vx_emit_box(q, 0, 0, 0.41f, 1, 1, 0.59f);
            else                   vx_emit_box(q, 0.41f, 0, 0, 0.59f, 1, 1);
        } break;
        default: vx_emit_box(q, 0,0,0, 1,1,1); break;
    }
}

static void vx_mesh_chunk(VoxField *v, int cx, int cz) {
    vx_tmp_n = 0;
    int x0 = cx * VX_CHV, z0 = cz * VX_CHV;
    int x1 = x0 + VX_CHV > v->vw ? v->vw : x0 + VX_CHV;
    int z1 = z0 + VX_CHV > v->vd ? v->vd : z0 + VX_CHV;
    float lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f };
    for (int z = z0; z < z1; z++) for (int x = x0; x < x1; x++) for (int y = 0; y < VX_VY; y++) {
        unsigned char b = v->blk[y][z][x];
        if (b == B_AIR) continue;
        VxQuadCtx q = { v, x, y, z, b };
        if (BLOCKS[b].water) {
            // water draws only its surface, and only where there is nothing above it
            if (get_blk(v, x, y + 1, z) != B_AIR) continue;
            float c[4][3] = { {0,0.8f,0}, {1,0.8f,0}, {1,0.8f,1}, {0,0.8f,1} };
            float n[3] = { 0, 1, 0 };
            vx_emit_quad(&q, c, n, -1);
        } else {
            vx_emit_shape(&q, v->shp[y][z][x]);
        }
        for (int k = 0; k < 3; k++) {
            float p = (k == 0 ? x : k == 1 ? y : z) * VOX_S;
            if (p < lo[k]) lo[k] = p;
            if (p + VOX_S > hi[k]) hi[k] = p + VOX_S;
        }
    }
    VxChunk *ch = &v->chunks[cz * VX_CHX + cx];
    if (!ch->vbo) glGenBuffers(1, &ch->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ch->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VxVert) * (size_t)vx_tmp_n, vx_tmp, GL_STATIC_DRAW);
    ch->verts = vx_tmp_n;
    for (int k = 0; k < 3; k++) { ch->lo[k] = lo[k]; ch->hi[k] = hi[k]; }
    v->vert_total += vx_tmp_n;
    v->tris += vx_tmp_n / 3;
}

void vx_mesh_all(VoxField *v) {
    uint64_t t0 = SDL_GetTicksNS();
    v->tris = 0; v->vert_total = 0;
    for (int i = 0; i < VX_CHUNKS; i++) v->chunks[i].verts = 0;
    for (int cz = 0; cz * VX_CHV < v->vd; cz++) for (int cx = 0; cx * VX_CHV < v->vw; cx++) vx_mesh_chunk(v, cx, cz);
    v->mesh_ms = (double)(SDL_GetTicksNS() - t0) / 1e6;
    v->built = true;
    // Load-time cost, reported once in the HUD's mesh/upload row and in the benchmark header.
    vxp.s[VXP_MESH].cpu_ms = vxp.s[VXP_MESH].cpu_avg = v->mesh_ms;
    vxp.c.vbo_mb = (double)v->vert_total * sizeof(VxVert) / (1024.0 * 1024.0);
}

