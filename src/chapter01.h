// chapter01.h — the chapter script: what happens, in what order, and what has to have happened
// before the story is allowed to move on. story/v3/chapter01.md (the spine, blocks P1-P8 and clip
// slots C1-C6) is the contract; this file is that document turned into a table.
//
// THE POINT OF THE TABLE. Everything the orchestrator is likely to want to change — the order of
// the steps, which map a block plays on, the time of day, the goal line, which interactions gate a
// step — is one row of CH_STEPS below. Adding a beat is a row; reordering is moving a row. No
// control flow anywhere else in the game knows the chapter's shape.
//
// The three pieces:
//   FLAGS   — one bit each, in a uint64_t. The story's memory. The mandatory ten come first.
//   STEPS   — an ordered list of PLAY (walk around a map until the required flags are set),
//             CLIP (play a cutscene and come back), SET (set a flag), END (the end card).
//   BINDING — which field-text id sets which flag, so a `message`/`npc` trigger the writer already
//             wrote is the thing that opens the gate. No new authoring is needed to gate a step.
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "battle.h"
#include "voxfield.h"

// ───────────────────────── flags ─────────────────────────
// X(name, id) — the id is what a binding row and the Dev panel show. The first ten are THE
// MANDATORY TEN from the spine: the chapter cannot be finished without every one of them.
#define CH_FLAG_LIST \
    /* the mandatory ten, in spine order */ \
    X(F_MACHINE_SWING,     "machine_swing")      /* 1  the swing examined: P1's objective        */ \
    X(F_BENCH_PART,        "bench_part")         /* 2  the part taken off Hart's bench           */ \
    X(F_LAMP_CHARM,        "lamp_charm")         /* 3  the lamp charm in the road                */ \
    X(F_GUILD_HALL_DOOR,   "guild_hall_door")    /* 4  fires on entering the guild hall          */ \
    X(F_OTTILIE_HOUSE,     "ottilie_house")      /* 5  her open door on the lane home            */ \
    X(F_JOB_SHEET,         "job_sheet")          /* 6  fires on taking the sheet down            */ \
    X(F_OTTILIE_DOOR,      "ottilie_door")       /* 7  she is asked along, and joins             */ \
    X(F_LANTERN,           "lantern")            /* 8  the false lantern, stepped toward         */ \
    X(F_TRACKS_STOP,       "tracks_stop")        /* 9  where the tracks stop; C5 fires here      */ \
    X(F_BODY,              "body")               /* 10 the ring of nine holes; C6 waits for it   */ \
    /* progress the steps turn on */ \
    X(F_SWING_TRIED,       "swing_tried")        /* P1 ends having fought the swing, not won it  */ \
    X(F_SWING_BEATEN,      "swing_beaten")       /* P4's win: the parry lands and it goes down   */ \
    X(F_BED,               "bed")                /* P3 ends by going to bed                      */ \
    X(F_LEFT_YARD,         "left_yard")          /* the gate: this is what starts the bell       */ \
    X(F_SIGNED,            "signed")             /* the clerk signs the book                     */ \
    X(F_AT_PASTURE,        "at_pasture")         /* arrived on high_pasture                      */ \
    X(F_DISTEL,            "distel")             /* the herder is in the party (C5)              */ \
    X(F_BOSS_DEAD,         "boss_dead")          /* Klee is down                                 */ \
    /* the four hidden finds; they are optional and never gate a step */ \
    X(F_LOOT_YARD,         "loot_yard") \
    X(F_LOOT_TEACHING,     "loot_teaching_find") \
    X(F_LOOT_HILL,         "loot_hill") \
    X(F_LOOT_PASTURE,      "loot_pasture")

enum ChFlag {
#define X(n, s) n,
    CH_FLAG_LIST
#undef X
    CH_FLAG_COUNT
};
static const char *CH_FLAG_NAME[] = {
#define X(n, s) s,
    CH_FLAG_LIST
#undef X
};
#define CH_MANDATORY 10                       // the first ten flags are the spine's mandatory ten

// ───────────────────────── steps ─────────────────────────

enum ChKind {
    CHS_PLAY = 0,     // walk around `map` until every flag in `need` is set
    CHS_CLIP,         // play cutscene `arg`, then come back to the field where we left it
    CHS_SET,          // set flag `need[0]` and fall straight through to the next step
    CHS_END,          // the end-of-chapter card
};

#define CH_NEED 4

struct ChStep {
    int kind;
    const char *arg;          // CLIP: the scene id in cutscene_data.h. Otherwise unused.
    const char *map;          // PLAY: the map. nullptr = stay on whatever map we are already on.
    int sx, sz;               // PLAY: the spawn cell. -1,-1 = use the map's own `spawn:` line.
    const char *face;         // PLAY: "S"/"W"/"E"/"N" for that spawn.
    const char *light;        // PLAY: the colormap table — day, dusk, night. nullptr = leave it.
    float level;              // PLAY: the ambient level for that table.
    int party;                // PLAY: how many walk together (1..3). 0 = leave it.
    int lamp;                 // PLAY: 1 = the party carries the lantern (the dark maps)
    int night_sight;          // PLAY: 1 = Distel's night sight is on
    const char *goal;         // PLAY: the goal line, <= 6 words. nullptr = leave the old one up.
    int need[CH_NEED];        // flags that must all be set before the step completes. -1 ends it.
};

// The chapter. Rows map one-to-one onto story/v3/chapter01.md's blocks; the block each row belongs
// to is in the comment. A PLAY row with no `need` completes as soon as it is entered, which is how
// a pure time-of-day or goal-line change is written.
static const ChStep CH_STEPS[] = {
 // P1 — the yard at midday. Three machines in a row; the swing is the one you do not beat today.
 { CHS_PLAY, nullptr, "hart_yard", -1, -1, "N", "day",  1.00f, 1, 0, 0, "Beat the swing.",
   { F_MACHINE_SWING, F_SWING_TRIED, -1 } },
 { CHS_CLIP, "0110_the_yard", nullptr, -1, -1, nullptr, nullptr, 0, 0, 0, 0, nullptr, { -1 } },   // C1

 // P2 — the errand. The part comes off Hart's bench, then Halm by day.
 { CHS_PLAY, nullptr, "hart_yard", -1, -1, "N", "day",  1.00f, 1, 0, 0, "Deliver the part. Buy bread.",
   { F_BENCH_PART, -1 } },
 { CHS_PLAY, nullptr, "halm",      -1, -1, "N", "day",  1.00f, 1, 0, 0, nullptr,
   { F_LAMP_CHARM, F_GUILD_HALL_DOOR, -1 } },
 { CHS_CLIP, "0130_the_counter", nullptr, -1, -1, nullptr, nullptr, 0, 0, 0, 0, nullptr, { -1 } },  // C2

 // P3 — the lane home at dusk, then supper and bed.
 { CHS_PLAY, nullptr, "halm",      -1, -1, "S", "dusk", 0.70f, 1, 0, 0, "Get home before supper.",
   { F_OTTILIE_HOUSE, -1 } },
 { CHS_PLAY, nullptr, "hart_yard", -1, -1, "N", "night",0.40f, 1, 1, 0, "Eat, and go to bed.",
   { F_BED, -1 } },
 { CHS_CLIP, "0140_supper", nullptr, -1, -1, nullptr, nullptr, 0, 0, 0, 0, nullptr, { -1 } },       // C3

 // P4 — day two. Four sessions at the swing with the light moving; the last one is the win.
 { CHS_PLAY, nullptr, "hart_yard", -1, -1, "N", "night",0.35f, 1, 1, 0, "Be at the machines before light.",
   { -1 } },
 { CHS_PLAY, nullptr, nullptr,     -1, -1, nullptr, "dusk", 0.75f, 1, 0, 0, nullptr, { -1 } },
 { CHS_PLAY, nullptr, nullptr,     -1, -1, nullptr, "day",  1.00f, 1, 0, 0, nullptr,
   { F_SWING_BEATEN, -1 } },
 { CHS_CLIP, "0150_the_sword", nullptr, -1, -1, nullptr, nullptr, 0, 0, 0, 0, nullptr, { -1 } },    // C4

 // P5 — the bell run. The sword gets a breath: the goal line does NOT change and the bell is not
 // counting until the player walks out of the gate (F_LEFT_YARD arms it — see ch_on_flag).
 { CHS_PLAY, nullptr, "hart_yard", -1, -1, "N", "dusk", 0.65f, 1, 0, 0, nullptr, { F_LEFT_YARD, -1 } },
 // Her door is ON the fast route, and she is ASKED along rather than simply appearing — so asking
 // her is one of the mandatory ten, and this step does not complete until it has happened.
 { CHS_PLAY, nullptr, "halm",      -1, -1, "N", "dusk", 0.60f, 2, 0, 0, "Get to the guild hall.",
   { F_JOB_SHEET, F_OTTILIE_DOOR, F_SIGNED, -1 } },

 // P6 — the hill path. Dusk into night, climbing.
 { CHS_PLAY, nullptr, "hill_path",    -1, -1, "N", "dusk", 0.50f, 2, 0, 0, "Get up to the pasture.",
   { F_AT_PASTURE, -1 } },

 // P7 — the high pasture. You cannot see. The lantern is four cells of light.
 { CHS_PLAY, nullptr, "high_pasture", -1, -1, "N", "night", 0.16f, 2, 1, 0, "Follow the sleeping animals.",
   { F_LANTERN, F_TRACKS_STOP, -1 } },
 { CHS_CLIP, "0160_the_herder", nullptr, -1, -1, nullptr, nullptr, 0, 0, 0, 0, nullptr, { -1 } },   // C5
 { CHS_SET,  nullptr, nullptr, -1, -1, nullptr, nullptr, 0, 0, 0, 0, nullptr, { F_DISTEL, -1 } },

 // P8 — three of them now, her night sight on, and the thing in the fold.
 { CHS_PLAY, nullptr, nullptr, -1, -1, nullptr, "night", 0.16f, 3, 1, 1, "Find the guard beast.",
   { F_BOSS_DEAD, -1 } },
 { CHS_PLAY, nullptr, nullptr, -1, -1, nullptr, nullptr,  0,    3, 1, 1, nullptr, { F_BODY, -1 } },
 { CHS_CLIP, "0180_what_was_on_it", nullptr, -1, -1, nullptr, nullptr, 0, 0, 0, 0, nullptr, { -1 } }, // C6
 { CHS_END,  nullptr, nullptr, -1, -1, nullptr, nullptr, 0, 0, 0, 0, nullptr, { -1 } },
};
static const int CH_STEP_COUNT = (int)(sizeof(CH_STEPS) / sizeof(CH_STEPS[0]));

// ───────────────────────── binding: what the world says -> what the story remembers ─────────────
// A field-text id the writer already wrote is what sets a flag. `_2` continuation lines are not
// listed: the first box of a pair is enough. Nothing here needs new authoring in text.md.
struct ChBind { const char *text_id; int flag; };
static const ChBind CH_BINDS[] = {
    { "hart_yard.machine_swing",   F_MACHINE_SWING },
    { "hart_yard.bench_part",      F_BENCH_PART },
    { "hart_yard.bed",             F_BED },
    { "halm.lamp_charm",           F_LAMP_CHARM },
    { "halm.guild_hall_door",      F_GUILD_HALL_DOOR },
    { "halm.ottilie_house",        F_OTTILIE_HOUSE },
    { "halm.job_sheet",            F_JOB_SHEET },
    { "halm.ottilie_door",         F_OTTILIE_DOOR },
    { "halm.clerk_signing",        F_SIGNED },
    { "halm.clerk_signing_late",   F_SIGNED },      // late is still signed: the run has no fail state
    { "high_pasture.lantern",      F_LANTERN },
    { "high_pasture.tracks_stop",  F_TRACKS_STOP },
    { "high_pasture.body",         F_BODY },
};
static const int CH_BIND_COUNT = (int)(sizeof(CH_BINDS) / sizeof(CH_BINDS[0]));

// A `pickup` trigger's item id -> the flag it sets.
static const ChBind CH_ITEM_BINDS[] = {
    { "bench_part",         F_BENCH_PART },
    { "loot_yard",          F_LOOT_YARD },
    { "loot_teaching_find", F_LOOT_TEACHING },
    { "loot_hill",          F_LOOT_HILL },
    { "loot_pasture",       F_LOOT_PASTURE },
};
static const int CH_ITEM_BIND_COUNT = (int)(sizeof(CH_ITEM_BINDS) / sizeof(CH_ITEM_BINDS[0]));

// Winning a `fight` sets a flag, so a battle can gate a step the same way an examine does.
static const ChBind CH_FIGHT_BINDS[] = {
    { "swing", F_SWING_TRIED },      // P1: fighting it at all is the gate; P4 needs the WIN below
    { "klee",  F_BOSS_DEAD },
};
static const int CH_FIGHT_BIND_COUNT = (int)(sizeof(CH_FIGHT_BINDS) / sizeof(CH_FIGHT_BINDS[0]));

// ───────────────────────── state ─────────────────────────
// POD: this is what the reload blob and the save file carry, and it is the whole of the story's
// memory. Nothing chapter-shaped lives anywhere else.
#define CH_GOAL_LEN 64

struct Chapter {
    int32_t step;                 // index into CH_STEPS
    int32_t started;              // 0 until ch_new_game / a load
    uint64_t flags;
    char goal[CH_GOAL_LEN];
    BtParty party;
    // The bell (P5). Armed by C4, STARTED by walking out of the gate, and never a fail state:
    // running out of rings only switches the clerk to his late line.
    int32_t bell_armed, bell_started, rings;
    float bell_t;
    int32_t skip_fights;          // Dev toggle
};

#define CH_BELL_RINGS 12          // how many rings the run is given
#define CH_BELL_PERIOD 9.0f       // seconds between rings

static inline bool ch_has(const Chapter *c, int flag) {
    return flag >= 0 && flag < CH_FLAG_COUNT && (c->flags & (1ull << flag)) != 0;
}
static inline void ch_set(Chapter *c, int flag) {
    if (flag >= 0 && flag < CH_FLAG_COUNT) c->flags |= (1ull << flag);
}
static inline int ch_flag_by_name(const char *s) {
    for (int i = 0; i < CH_FLAG_COUNT; i++) if (!strcmp(CH_FLAG_NAME[i], s)) return i;
    return -1;
}

static inline void ch_new_game(Chapter *c) {
    memset(c, 0, sizeof(*c));
    c->started = 1;
    c->step = 0;
    bt_party_init(&c->party, 1);
}

// Has the current step's gate opened?
static inline bool ch_step_complete(const Chapter *c) {
    if (c->step < 0 || c->step >= CH_STEP_COUNT) return false;
    const ChStep *s = &CH_STEPS[c->step];
    if (s->kind != CHS_PLAY) return true;
    for (int i = 0; i < CH_NEED && s->need[i] >= 0; i++)
        if (!ch_has(c, s->need[i])) return false;
    return true;
}

// Apply a PLAY row to the world: map, spawn, time of day, party size, lantern, night sight, goal.
// Only what the row actually names is touched, so a row that changes nothing but the light leaves
// the player standing exactly where they were.
static inline void ch_apply_step(Chapter *c, VoxField *v) {
    if (c->step < 0 || c->step >= CH_STEP_COUNT) return;
    const ChStep *s = &CH_STEPS[c->step];
    if (s->kind != CHS_PLAY) return;
    if (v) {
        const char *cur = vx_current_map(v);
        if (s->map && (!cur || strcmp(cur, s->map) != 0))
            vx_goto(v, s->map, s->sx, s->sz, s->face ? s->face : "S");
        if (s->light) vx_set_light(v, s->light, s->level);
        if (s->party > 0) vx_set_party(v, s->party);
        vx_set_party_lamp(v, 4.5f, 0.9f, s->lamp);
        vx_set_night_sight(v, s->night_sight ? 0.10f : 0.0f);
    }
    if (s->goal) snprintf(c->goal, sizeof(c->goal), "%s", s->goal);
}

// Move to the next step, running through any SET rows, and apply whatever we land on.
static inline void ch_advance(Chapter *c, VoxField *v) {
    if (c->step >= CH_STEP_COUNT - 1) return;
    c->step++;
    while (c->step < CH_STEP_COUNT && CH_STEPS[c->step].kind == CHS_SET) {
        for (int i = 0; i < CH_NEED && CH_STEPS[c->step].need[i] >= 0; i++)
            ch_set(c, CH_STEPS[c->step].need[i]);
        c->step++;
    }
    if (c->step >= CH_STEP_COUNT) c->step = CH_STEP_COUNT - 1;
    ch_apply_step(c, v);
}

// Jump straight to a step (the Dev panel). Every flag an earlier step gated on is set on the way,
// so the world is consistent with having played it.
static inline void ch_jump(Chapter *c, VoxField *v, int step) {
    if (step < 0) step = 0;
    if (step >= CH_STEP_COUNT) step = CH_STEP_COUNT - 1;
    for (int k = 0; k < step; k++)
        for (int i = 0; i < CH_NEED && CH_STEPS[k].need[i] >= 0; i++) ch_set(c, CH_STEPS[k].need[i]);
    c->step = step;
    if (ch_has(c, F_DISTEL)) c->party.count = 3;
    else if (ch_has(c, F_OTTILIE_DOOR)) c->party.count = 2;
    ch_apply_step(c, v);
}

// The world reported something. Returns true if a flag changed.
static inline bool ch_on_text(Chapter *c, const char *text_id) {
    for (int i = 0; i < CH_BIND_COUNT; i++)
        if (!strcmp(CH_BINDS[i].text_id, text_id) && !ch_has(c, CH_BINDS[i].flag)) {
            ch_set(c, CH_BINDS[i].flag);
            if (CH_BINDS[i].flag == F_OTTILIE_DOOR && c->party.count < 2) bt_party_init(&c->party, 2);
            return true;
        }
    return false;
}

static inline bool ch_on_pickup(Chapter *c, const char *item_id) {
    for (int i = 0; i < CH_ITEM_BIND_COUNT; i++)
        if (!strcmp(CH_ITEM_BINDS[i].text_id, item_id) && !ch_has(c, CH_ITEM_BINDS[i].flag)) {
            ch_set(c, CH_ITEM_BINDS[i].flag);
            // The hidden finds' effects last, so the battle system sees them as carried items.
            switch (CH_ITEM_BINDS[i].flag) {
            case F_LOOT_YARD:     c->party.items |= BT_ITEM_YARD; break;
            case F_LOOT_TEACHING: c->party.items |= BT_ITEM_TEACHING; break;
            case F_LOOT_HILL:     c->party.items |= BT_ITEM_HILL; break;
            case F_LOOT_PASTURE:  c->party.items |= BT_ITEM_PASTURE; break;
            default: break;
            }
            return true;
        }
    return false;
}

// A battle finished. `won` distinguishes P1's "you fought the swing" from P4's "you beat it".
static inline void ch_on_battle(Chapter *c, const char *encounter, bool won) {
    if (!strcmp(encounter, "swing")) {
        ch_set(c, F_SWING_TRIED);
        if (won) ch_set(c, F_SWING_BEATEN);
        return;
    }
    if (!won) return;
    for (int i = 0; i < CH_FIGHT_BIND_COUNT; i++)
        if (!strcmp(CH_FIGHT_BINDS[i].text_id, encounter)) ch_set(c, CH_FIGHT_BINDS[i].flag);
}

// The player walked onto a map under their own steam.
static inline void ch_on_map(Chapter *c, const char *map) {
    if (!strcmp(map, "high_pasture")) ch_set(c, F_AT_PASTURE);
    if (!strcmp(map, "halm") && c->bell_armed && !c->bell_started) {
        ch_set(c, F_LEFT_YARD);
        c->bell_started = 1; c->bell_t = 0; c->rings = 0;   // the sword's breath is over
    }
}

// Seconds pass. The bell is the only thing in the chapter that runs on a clock.
static inline void ch_tick(Chapter *c, float dt) {
    if (!c->bell_started || c->rings >= CH_BELL_RINGS) return;
    c->bell_t += dt;
    if (c->bell_t >= CH_BELL_PERIOD) { c->bell_t -= CH_BELL_PERIOD; c->rings++; }
}
// True once the board has shut — the clerk's late line, never a failure.
static inline bool ch_bell_late(const Chapter *c) { return c->rings >= CH_BELL_RINGS; }

// Every mandatory interaction accounted for? The end card asserts this; so does --chapter-selftest.
static inline int ch_mandatory_missing(const Chapter *c) {
    int n = 0;
    for (int i = 0; i < CH_MANDATORY; i++) if (!ch_has(c, i)) n++;
    return n;
}
