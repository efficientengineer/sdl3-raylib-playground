// vox_triggers.cpp — what a cell does to you, and the box that says so.
// OWNS: the trigger list's queries (live? at this cell? collect the whole cell in order), acting on
//       one, draining the queue, starting a map change, the message box's text and the event queue
//       the chapter script reads.
// NEVER: draws the box (vox_render.cpp) and never knows what a flag, a goal or a step is — it
//        reports what the player did and the chapter decides what it means.
// EXPOSES: vx_fire, vx_say/_say2, trig_live, trig_at, trig_collect, trig_act, trig_drain,
//          on_enter_cell, start_map_change, npc_near.
// Tested by: capture.sh --chapter-playtest and --vox-walktest. See src/notes/world.md, "Triggers".
#include "vox_internal.h"

// ───────────────────────── walking ─────────────────────────

// The NPC nearest to a point, within `r` cells. NPCs are circles now, not cells.
int npc_near(VoxField *v, float x, float z, float r) {
    int best = -1; float bd = r * r;
    for (int i = 0; i < v->npc_count; i++) {
        float dx = v->npcs[i].x - x, dz = v->npcs[i].z - z;
        float d = dx * dx + dz * dz;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

void vx_fire(VoxField *v, int kind, const char *arg, const char *arg2);

// `report` is false only when the chapter itself asked for the line (vx_say_id): the field reports
// what the PLAYER did, and echoing the chapter's own line back at it would gate on nothing.
void vx_say2(VoxField *v, const char *who, const char *id, bool report) {
    const char *text = nullptr, *name = nullptr;
    for (int i = 0; i < FIELD_TEXT_COUNT; i++)
        if (!strcmp(FIELD_TEXT[i].id, id)) { text = FIELD_TEXT[i].text; name = FIELD_TEXT[i].name; break; }
    if (text) snprintf(v->msg, sizeof(v->msg), "%s", text);
    else snprintf(v->msg, sizeof(v->msg), "[%s]", id);
    const char *speaker = (name && name[0]) ? name : who;
    snprintf(v->msg_who, sizeof(v->msg_who), "%s", speaker ? speaker : "");
    v->msg_t = 0; v->msg_page = 0; v->msg_typing = true;
    // Every story text id the field shows is reported. This is how a mandatory examine is observed;
    // it goes through the QUEUE, so a pickup that shows a line and takes an item reports both.
    if (report) vx_fire(v, VXE_TEXT, id, "");
}

void vx_say(VoxField *v, const char *who, const char *id) { vx_say2(v, who, id, true); }

void vx_message(VoxField *v, const char *text) {
    snprintf(v->msg, sizeof(v->msg), "%s", text ? text : "");
    v->msg_who[0] = 0; v->msg_t = 0; v->msg_page = 0; v->msg_typing = true;
}

// The event queue. vx_tick hands the game ONE event a tick and pops it here; nothing is dropped
// unless a single tick somehow produces more than VX_EVQ of them, which is logged.
void vx_fire(VoxField *v, int kind, const char *arg, const char *arg2) {
    if (v->evq_n >= VX_EVQ) { SDL_Log("voxfield: event queue full — dropped kind %d (%s)", kind, arg ? arg : ""); return; }
    VxEvent *e = &v->evq[v->evq_n++];
    e->kind = kind;
    snprintf(e->arg, sizeof(e->arg), "%s", arg ? arg : "");
    snprintf(e->arg2, sizeof(e->arg2), "%s", arg2 ? arg2 : "");
}

// A trigger the player can see and act on right now: not consumed by the chapter, not already
// taken, and — if it carries `nightsight` — only while night sight is on.
bool trig_live(VoxField *v, const VxTrig *g) {
    if (g->off || g->taken) return false;
    if (g->night && v->ns_add <= 0.0f) return false;
    return true;
}

int trig_at(VoxField *v, int x, int z, unsigned mask) {
    for (int i = 0; i < v->trig_count; i++) {
        VxTrig *g = &v->trigs[i];
        if (x < g->x || z < g->z || x >= g->x + g->w || z >= g->z + g->d) continue;
        if (!(mask & TGM(g->kind))) continue;
        if (!trig_live(v, g)) continue;
        return i;
    }
    return -1;
}

// EVERY live trigger on a cell, in the order one press delivers them. The order is the contract:
//   1. every MESSAGE — what the place says, before anything changes;
//   2. the PICKUP — its own box, then the item, so taking something always reads as a consequence
//      of having looked at it;
//   3. the DOOR last, because it takes the screen and nothing after it would ever be seen.
// Returns how many were written. Anything not in `mask` is skipped, so the cell ahead and the cell
// underfoot keep the different rules they always had.
int trig_collect(VoxField *v, int x, int z, unsigned mask, int *out, int cap) {
    static const int ORDER[3] = { TG_MESSAGE, TG_PICKUP, TG_DOOR };
    int n = 0;
    for (int k = 0; k < 3 && n < cap; k++) {
        for (int i = 0; i < v->trig_count && n < cap; i++) {
            VxTrig *g = &v->trigs[i];
            if (g->kind != ORDER[k]) continue;
            if (x < g->x || z < g->z || x >= g->x + g->w || z >= g->z + g->d) continue;
            if (!(mask & TGM(g->kind))) continue;
            if (!trig_live(v, g)) continue;
            out[n++] = i;
        }
    }
    return n;
}

static void start_map_change(VoxField *v, const char *map, int x, int z, int f);

// Act on ONE trigger, exactly as a press on it always did.
static void trig_act(VoxField *v, int ti) {
    VxTrig *g = &v->trigs[ti];
    if (g->kind == TG_DOOR) start_map_change(v, g->map, g->ax, g->az, g->af);
    else if (g->kind == TG_PICKUP) {
        // The box opens FIRST and unconditionally — it is the player's feedback, and it must not
        // depend on the event surviving. Then the item is reported, and the trigger is spent: a
        // pickup is takeable exactly once.
        g->taken = 1;
        vx_say(v, nullptr, g->arg2);
        vx_fire(v, VXE_PICKUP, g->arg, g->arg2);
    }
    else vx_say(v, nullptr, g->arg);
}

// Take the next trigger off the cell's queue and act on it. A door empties the queue, because the
// map is about to change and anything still in it belongs to a place we have left.
void trig_drain(VoxField *v) {
    while (v->exam_qn > 0) {
        int ti = v->exam_q[0];
        for (int i = 1; i < v->exam_qn; i++) v->exam_q[i - 1] = v->exam_q[i];
        v->exam_qn--;
        if (!trig_live(v, &v->trigs[ti])) continue;          // consumed since the press
        if (v->trigs[ti].kind == TG_DOOR) v->exam_qn = 0;
        trig_act(v, ti);
        return;
    }
}

static void start_map_change(VoxField *v, const char *map, int x, int z, int f) {
    snprintf(v->to_map, sizeof(v->to_map), "%s", map);
    v->to_x = x; v->to_z = z; v->to_f = f;
    v->fade = 1; v->fade_dir = 1;
}

// ── triggers, still cell-based ──
// A trigger is a rectangle of CELLS in the .tmap and stays one: the story reads them that way. They
// fire when the leader's cell changes on the ground; an exit whose rectangle touches the map border
// fires in the air too, so a jump off the edge of the map still takes you to the next one.

static bool vx_trig_at_edge(VoxField *v, const VxTrig *g) {
    return g->x <= 0 || g->z <= 0 || g->x + g->w >= v->mw || g->z + g->d >= v->md;
}

void on_enter_cell(VoxField *v, bool grounded) {
    v->steps++;
    int x = v->act[0].tx, z = v->act[0].tz;
    for (int i = 0; i < v->trig_count; i++) {
        VxTrig *g = &v->trigs[i];
        bool in = x >= g->x && z >= g->z && x < g->x + g->w && z < g->z + g->d;
        bool was = g->inside;
        g->inside = in;
        if (!in || was) continue;
        if (!trig_live(v, g)) { g->inside = false; continue; }
        if (!grounded && !(g->kind == TG_EXIT && vx_trig_at_edge(v, g))) { g->inside = false; continue; }
        if (g->kind == TG_EXIT) { start_map_change(v, g->map, g->ax, g->az, g->af); return; }
        if (g->kind == TG_DOOR && v->act[0].facing == 3) { start_map_change(v, g->map, g->ax, g->az, g->af); return; }
        if (g->kind == TG_ZONE) vx_fire(v, VXE_ZONE, g->arg, "");
        if (g->kind == TG_TRAP) vx_say(v, nullptr, g->arg);
        if (g->kind == TG_SCENE) vx_fire(v, VXE_SCENE, g->arg, "");
        if (g->kind == TG_FIGHT) vx_fire(v, VXE_FIGHT, g->arg, "");
        if (g->kind == TG_GOAL) vx_fire(v, VXE_GOAL, g->arg, "");
    }
}

