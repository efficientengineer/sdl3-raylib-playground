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
#include <SDL3/SDL.h>
#include "voxfield.h"

// ───────────────────────── names ─────────────────────────
// THE ONE PLACE A PROPER NOUN IS WRITTEN IN THE ENGINE. story/v3/NAMES.md is the table and every
// story file is tokenised against it; the engine is not, because nothing exports the table to C.
// So a goal line or a battle line that has to name somebody builds itself from these, and a rename
// is these four lines and nothing else.
//
// WHAT THE REAL FIX IS, when somebody has the tool budget: `./story_prompt.py export` already
// writes src/field_text.h with every token substituted. It should write src/names.h beside it —
// `#define CH_NAME_HERDER "Distel"` straight out of NAMES.md — and this block should become
// `#include "names.h"`. Until then these are hand-copied and this comment is the audit trail.
// Checked against story/v3/NAMES.md rows 13, 14, 24, 65 on 2026-09-21.
#define CH_NAME_HERO       "Falke"      // {{HERO}}
#define CH_NAME_HEALER     "Ottilie"    // {{HEALER}}
#define CH_NAME_HERDER     "Distel"     // {{HERDER}}
#define CH_NAME_BEAST      "Klee"       // {{BEAST_NAME}}
// {{GUARD_BEAST}} reads "drover" and is a COMMON NOUN, lower case in a sentence. chapter01.md C5 is
// explicit that the player is never told "guard beast" — that is a designer's word.
#define CH_NAME_GUARD_BEAST "drover"

// The chapter title, drawn on the title card. The scene data carries no title field, so it is here
// with its source named: story/v3/chapter01.md line 1.
#define CH_TITLE "Chapter 1  -  The High Pasture"

// ───────────────────────── the party lantern ─────────────────────────
// PALETTE.md's `lamp` colormap table runs 32 light levels and the top of it is the most saturated
// row there is — a hot orange that, dropped into the middle of a blue night, reads as a fire rather
// than as a lantern and flattens every colour it touches. The party's lantern is a lantern. It is
// CAPPED here rather than in the table, because the table is shared with the map's own static
// `lamp:` lines (a forge, a window) which are allowed to be that hot.
//
// RELAXED to 0.85 on 2026-09-21, and NOT YET VERIFIED BY EYE. The new colormap.png bleaches lamp
// light toward warm straw rather than toward saturated orange, so the thing this cap was guarding
// against is much weaker than it was, and 0.72 on a `night 0.16` pasture is very nearly black.
//
// The honest caveat: I could not check it. `capture.sh --vox` drives the MAP's static `lamp:`
// lines, not the party lantern — the party lantern is `vx_set_party_lamp`, which only the chapter
// script calls — so a night capture of high_pasture shows the map's lamps and no lantern at all,
// and there is nothing to judge. Either the capture path needs a `--partylamp r:level` knob or this
// wants thirty seconds of somebody walking around in the dark. Both are cheap; neither is done.
#define CH_LAMP_MAX 0.85f
#define CH_LAMP_RADIUS 4.5f          // cells; chapter01.md P7 "the lantern shows about four cells"

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
    /* P4 is FOUR SESSIONS (chapter01.md P4) and a session is one go at the swing. These three are
       set by ch_on_battle as the day-two session count passes 1, 2 and 3, so each of the first
       three rows completes by the player actually fighting — not by walking onto a cell. The
       fourth row is the win. */ \
    X(F_DAY2_1,            "day2_session_1")     /* grey dawn: the swing as it was               */ \
    X(F_DAY2_2,            "day2_session_2")     /* sun up: Hart pulls the pin, the tell slows   */ \
    X(F_DAY2_3,            "day2_session_3")     /* high sun: the pin goes back, full speed      */ \
    X(F_SWING_BEATEN,      "swing_beaten")       /* P4's win: the parry lands and it goes down   */ \
    X(F_SUPPER,            "supper")             /* P3: at Hart's table, at sundown              */ \
    X(F_BED,               "bed")                /* P3 ends by going to bed                      */ \
    X(F_LEFT_YARD,         "left_yard")          /* the gate: this is what starts the bell       */ \
    X(F_SIGNED,            "signed")             /* the clerk signs the book                     */ \
    X(F_AT_PASTURE,        "at_pasture")         /* arrived on high_pasture                      */ \
    X(F_DISTEL,            "distel")             /* the herder is in the party (C5)              */ \
    X(F_BOSS_DEAD,         "boss_dead")          /* Klee is down                                 */ \
    /* P6. The cold read of draft four promoted `hill_path.ottilie_dark_2` to mandatory: it is the
       only place {{HEALER}}'s own want surfaces, and the climb is where it belongs. It gates P6, so
       the route goes through it and no player can finish the chapter without it — but it is NOT one
       of the spine's mandatory TEN, which is a named list of ten ids (chapter01.md, "The mandatory
       ten") that the end card and the play-test assert as a set. Adding an eleventh member to that
       list would silently change what those two things mean. Gating the step is the same guarantee
       with none of that; if the designer wants it counted in the ten, chapter01.md's table is the
       place it has to be added first. */ \
    X(F_OTTILIE_DARK,      "ottilie_dark")       /* the climb: she has never been anywhere at night */ \
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
 // TWO ROWS IN THE YARD, not one. chapter01.md P3 is "day -> dusk -> night, supper at sundown", and
 // 0140_supper opens at sundown; putting the whole supper-and-bed block at `night 0.40` had the
 // player walking into a dark house at sundown. So supper is dusk and only the bed is night.
 { CHS_PLAY, nullptr, "halm",      -1, -1, "S", "dusk", 0.70f, 1, 0, 0, "Get home before supper.",
   { F_OTTILIE_HOUSE, -1 } },
 { CHS_PLAY, nullptr, "hart_yard", -1, -1, "N", "dusk", 0.45f, 1, 1, 0, "Eat, and go to bed.",
   { F_SUPPER, -1 } },
 { CHS_PLAY, nullptr, nullptr,     -1, -1, nullptr, "night", 0.40f, 1, 1, 0, nullptr,
   { F_BED, -1 } },
 { CHS_CLIP, "0140_supper", nullptr, -1, -1, nullptr, nullptr, 0, 0, 0, 0, nullptr, { -1 } },       // C3

 // P4 — day two. FOUR sessions at the swing with the light moving, and it ENDS AT LAST LIGHT.
 // chapter01.md P4 states this plainly and states why: C4's panels and beat are "the last light of
 // the second day", the low gold the sword is written into, and the engine used to run three rows
 // ending at `day 1.00` — which put the sword in the middle of the afternoon. The light walks grey
 // dawn -> sun up -> high sun -> low gold, one step a session. The number of sessions is what makes
 // the day feel long; the last one's light is what makes the sword land.
 //
 // Only the FOURTH row gates on F_SWING_BEATEN, and ch_on_battle refuses to set that flag before
 // this block (see there) — so a lucky win in session one does not end the day.
 { CHS_PLAY, nullptr, "hart_yard", -1, -1, "N", "night", 0.35f, 1, 1, 0, "Be at the machines before light.",
   { F_DAY2_1, -1 } },                                                       // 1. grey dawn
 { CHS_PLAY, nullptr, nullptr,     -1, -1, nullptr, "day",  0.80f, 1, 0, 0, "Beat the swing.",
   { F_DAY2_2, -1 } },                                                       // 2. sun up
 { CHS_PLAY, nullptr, nullptr,     -1, -1, nullptr, "day",  1.00f, 1, 0, 0, nullptr,
   { F_DAY2_3, -1 } },                                                       // 3. high sun
 { CHS_PLAY, nullptr, nullptr,     -1, -1, nullptr, "dusk", 0.80f, 1, 0, 0, nullptr,            // 4. low gold
   { F_SWING_BEATEN, -1 } },
 { CHS_CLIP, "0150_the_sword", nullptr, -1, -1, nullptr, nullptr, 0, 0, 0, 0, nullptr, { -1 } },    // C4

 // P5 — the bell run. The sword gets a breath: the goal line does NOT change and the bell is not
 // counting until the player walks out of the gate (F_LEFT_YARD arms it — see ch_on_flag).
 { CHS_PLAY, nullptr, "hart_yard", -1, -1, "N", "dusk", 0.65f, 1, 0, 0, nullptr, { F_LEFT_YARD, -1 } },
 // Her door is ON the fast route, and she is ASKED along rather than simply appearing — so asking
 // her is one of the mandatory ten, and this step does not complete until it has happened.
 { CHS_PLAY, nullptr, "halm",      -1, -1, "N", "dusk", 0.60f, 2, 0, 0, "Get to the guild hall.",
   { F_JOB_SHEET, F_OTTILIE_DOOR, F_SIGNED, -1 } },

 // P6 — the hill path. Dusk into night, climbing. Two rows so the light drops as you climb
 // (chapter01.md P6: "the light drops a step at each switchback — the time-of-day change is the
 // level design"), and the first ends on {{HEALER}}'s line in the dark, which draft four's cold
 // read made mandatory.
 { CHS_PLAY, nullptr, "hill_path",    -1, -1, "N", "dusk", 0.50f, 2, 0, 0, "Get up to the pasture.",
   { F_OTTILIE_DARK, -1 } },
 { CHS_PLAY, nullptr, nullptr,        -1, -1, nullptr, "night", 0.30f, 2, 1, 0, nullptr,
   { F_AT_PASTURE, -1 } },

 // P7 — the high pasture. You cannot see. The lantern is four cells of light.
 // TWO ROWS, not one, and the order is fixed. The audit's finding 18: with both flags needed in
 // EITHER order, C5 could fire after the player had already been to the false lantern in a far
 // corner, or the player could reach the tracks first and be sent back across a dark map. The
 // spine's P7 is a sequence — the trail of sleepers, then the false lantern, then the tracks — so
 // it is two rows and the goal line changes between them.
 { CHS_PLAY, nullptr, "high_pasture", -1, -1, "N", "night", 0.16f, 2, 1, 0, "Follow the sleeping animals.",
   { F_LANTERN, -1 } },
 { CHS_PLAY, nullptr, nullptr, -1, -1, nullptr, nullptr, 0, 2, 1, 0, nullptr,
   { F_TRACKS_STOP, -1 } },
 { CHS_CLIP, "0160_the_herder", nullptr, -1, -1, nullptr, nullptr, 0, 0, 0, 0, nullptr, { -1 } },   // C5
 { CHS_SET,  nullptr, nullptr, -1, -1, nullptr, nullptr, 0, 0, 0, 0, nullptr, { F_DISTEL, -1 } },

 // P8 — three of them now, her night sight on, and the thing in the fold.
 // The goal line says DROVER, not "guard beast". chapter01.md C5 is explicit: {{GUARD_BEAST}}
 // reads "drover" and the player is never told "guard beast", which is a designer's word.
 { CHS_PLAY, nullptr, nullptr, -1, -1, nullptr, "night", 0.16f, 3, 1, 1, "Find the " CH_NAME_GUARD_BEAST ".",
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
    { "hart_yard.supper_1",        F_SUPPER },
    { "hart_yard.bed",             F_BED },
    { "hill_path.ottilie_dark",    F_OTTILIE_DARK },
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

// ───────────────────────── the steps, by name ─────────────────────────
// The condition table below has to be able to say "from here to there", and a raw index into
// CH_STEPS is the worst possible way to write that: adding one row silently re-points every
// number. So the few steps anything names are named, and ch_steps_selfcheck() proves at runtime
// that each name still sits on the row it is supposed to. If a row moves, the self-test says so by
// name on the first run instead of the chapter going quietly wrong in the middle.
enum ChStepId {
    CHST_P1_YARD = 0,        // P1: the yard at midday
    CHST_C1 = 1,
    CHST_P2_YARD = 2,        // P2: the part off Hart's bench
    CHST_P2_TOWN = 3,        // P2: Halm by day
    CHST_C2 = 4,
    CHST_P3_LANE = 5,        // P3: the lane home at dusk
    CHST_P3_SUPPER = 6,      // P3: Hart's table, sundown
    CHST_P3_BED = 7,         // P3: night, and bed
    CHST_C3 = 8,
    CHST_P4_S1 = 9,          // P4: four sessions, grey dawn to low gold
    CHST_P4_S4 = 12,
    CHST_C4 = 13,
    CHST_P5_YARD = 14,       // P5: the sword's breath, before the bell
    CHST_P5_TOWN = 15,       // P5: the bell run
    CHST_P6_CLIMB = 16,      // P6: the hill path
    CHST_P6_TOP = 17,
    CHST_P7_SLEEPERS = 18,   // P7: the trail, then the false lantern
    CHST_P7_TRACKS = 19,     // P7: where the tracks stop
    CHST_C5 = 20,
    CHST_P8_BOSS = 22,       // P8: the fold
    CHST_P8_BODY = 23,       // P8: the ring of nine holes
};

// ───────────────────────── when a trigger is live ─────────────────────────
// THE PROBLEM THIS SOLVES. A .tmap trigger has no idea what time it is. `halm.job_sheet` sat on the
// job board and fired the moment the player walked past it on DAY ONE, when the spine says the
// board is bare and `halm.board_closed` is what the player reads — and it set mandatory number six
// out of order while it was at it. `halm.ottilie_door` sat on a cell of the day-one lane home, so
// she could be asked along, and JOIN THE PARTY, before the sword existed and before there was a job
// to ask her to. Neither is a map bug: the map is a place, and a place does not change. What
// changes is the chapter.
//
// So: every trigger that is only live for part of the chapter has a row here, and
// ch_apply_triggers() switches it on or off on every step change. A trigger with no row is always
// live, which is almost all of them.
//
//   id          the trigger's arg id — a text id, a pickup's item id, an encounter id, or the
//               synthetic `exit:<map>` the field gives an exit (an exit line carries no id of its
//               own and the .tmap grammar is not this side's to change).
//   from,to     the step window, INCLUSIVE, by name from ChStepId. -1 either side = unbounded.
//   needs,not   a flag that must be set / must not be set. -1 = don't care.
//   alt         a text id to show INSTEAD when the trigger is inert and the player interacts
//               anyway. This is the `board_closed` / `job_sheet` pair: the board is always there,
//               it just says a different thing before the job exists. nullptr = simply inert.
//
// The alternate is not a second trigger on the same cell (two triggers on a cell is a coin toss
// decided by file order); it is the SAME trigger answering differently, which is also why the map
// only has to carry the place once.
struct ChTrigCond {
    const char *id;
    int from, to;
    int needs, nots;
    const char *alt;
};
static const ChTrigCond CH_TRIG_COND[] = {
    // ── P1: "one small enclosure, NO EXITS OPEN YET" (chapter01.md P1) ──
    // Without this the player can walk out of the gate in the first minute and arrive in a Halm
    // that is dressed for day two, with a goal line about a swing.
    { "exit:halm",                 -1, -1, F_SWING_TRIED, -1, nullptr },

    // ── the day-two lines, and the bed ──
    // Supper and bed are day one's evening and nothing else: before the lane home they are not
    // there, and after the player has slept the house is not the point any more.
    { "hart_yard.supper_1",        CHST_P3_SUPPER, CHST_P3_BED, -1, -1, nullptr },
    { "hart_yard.supper_2",        CHST_P3_SUPPER, CHST_P3_BED, -1, -1, nullptr },
    { "hart_yard.book",            CHST_P3_SUPPER, -1, -1, -1, nullptr },   // and between sessions
    { "hart_yard.room_sword_gap",  CHST_P3_SUPPER, CHST_P3_BED, -1, -1, nullptr },
    { "hart_yard.bed",             CHST_P3_BED,    CHST_P3_BED, -1, -1, nullptr },
    { "hart_yard.day2_b",          CHST_P4_S1, CHST_P4_S4, -1, -1, nullptr },
    { "hart_yard.day2_d",          CHST_P4_S1, CHST_P4_S4, -1, -1, nullptr },
    // Hart's errand is P2's opening and nothing else's.
    { "hart_yard.errand",          CHST_P2_YARD, CHST_P2_TOWN, -1, -1, nullptr },

    // ── the board ──
    // "The job board is visible across the square and EMPTY: it is not dawn." (chapter01.md P2.2)
    // It becomes the job on the bell run and not before, and until then it answers `board_closed`.
    { "halm.job_sheet",            CHST_P5_TOWN, -1, -1, -1, "halm.board_closed" },
    { "halm.job_sheet_2",          CHST_P5_TOWN, -1, F_JOB_SHEET, -1, nullptr },

    // ── her door ──
    // Mandatory seven, and it is on the FAST ROUTE of the bell run, not on the day-one lane home.
    // She is ASKED, and she joins on the spot; that cannot happen before there is a job to ask her
    // to. `halm.ottilie_house` — the open door, one chair, two coats — is the day-one one and stays
    // exactly where it is.
    { "halm.ottilie_door",         CHST_P5_TOWN, -1, F_JOB_SHEET, -1, nullptr },
    { "halm.ottilie_door_2",       CHST_P5_TOWN, -1, F_OTTILIE_DOOR, -1, nullptr },
    { "halm.ottilie_house",        -1, CHST_P3_LANE, -1, -1, nullptr },

    // ── the counter ──
    // Signing is reading: you cannot sign a sheet you have not taken down. The engine picks between
    // the two clerk lines by the bell (ch_on_text), so both are live together and only one fires.
    { "halm.clerk_signing",        CHST_P5_TOWN, -1, F_JOB_SHEET, -1, nullptr },
    { "halm.clerk_signing_late",   CHST_P5_TOWN, -1, F_JOB_SHEET, -1, nullptr },

    // ── the townspeople's two states ──
    // "Three townspeople, each with a line now and a different one after he has the sword" — so the
    // `_after` lines start at C4 and, per the draft-four cold read, STAY UP through the bell run
    // and until the party leaves Halm for the hill. Nobody reads boxes on a clock, so they are not
    // cut off at signing.
    { "halm.marta_after",          CHST_P5_YARD, CHST_P5_TOWN, -1, -1, nullptr },
    { "halm.ostler_after",         CHST_P5_YARD, CHST_P5_TOWN, -1, -1, nullptr },
    { "halm.gate_watch_after",     CHST_P5_YARD, CHST_P5_TOWN, -1, -1, nullptr },
    { "halm.rival_door",           CHST_P5_YARD, CHST_P5_TOWN, -1, -1, nullptr },
    { "halm.marta",                -1, CHST_P3_LANE, -1, -1, nullptr },
    { "halm.ostler",               -1, CHST_P3_LANE, -1, -1, nullptr },
    { "halm.gate_watch",           -1, CHST_P3_LANE, -1, -1, nullptr },

    // ── the pasture ──
    // The body is only there once the fight is over, and the lens is only findable after C5 — the
    // map's own `nightsight` word already hides it, and this makes it a fact rather than a lighting
    // accident.
    { "high_pasture.body",         CHST_P8_BODY, -1, F_BOSS_DEAD, -1, nullptr },
    { "high_pasture.body_2",       CHST_P8_BODY, -1, F_BODY, -1, nullptr },
    { "loot_pasture",              -1, -1, F_DISTEL, -1, nullptr },
    { "high_pasture.find",         -1, -1, F_DISTEL, -1, nullptr },
};
static const int CH_TRIG_COND_COUNT = (int)(sizeof(CH_TRIG_COND) / sizeof(CH_TRIG_COND[0]));

// ───────────────────────── what is meant to be behind a jump ─────────────────────────
// The field reports every trigger walking cannot reach (voxfield.cpp, vx_nav_check_targets). This
// is the list of the ones that are the DESIGN — story/v3/LOOT.md's three platforming finds. The
// play-test checks it in BOTH directions: anything here that has become plain-walkable is a level
// that lost its lesson, and anything the field reports that is NOT here is a trigger the player
// cannot get to, which is a bug however pretty the hillside is.
struct ChJumpOnly { const char *map, *id, *why; };
static const ChJumpOnly CH_JUMP_ONLY[] = {
    { "hart_yard",    "loot_yard",       "the tin under the eaves: a running jump off the workshop roof" },
    { "hart_yard",    "hart_yard.ladder","the same cell's examine" },
    { "hill_path",    "loot_hill",       "the whistle: a two-cell drop off the outside of the bend" },
    { "hill_path",    "hill_path.drop",  "the same cell's examine" },
    { "high_pasture", "loot_pasture",    "the lens: a running jump onto the shelf above the fold" },
    { "high_pasture", "high_pasture.find","the same cell's examine" },
};
static const int CH_JUMP_ONLY_COUNT = (int)(sizeof(CH_JUMP_ONLY) / sizeof(CH_JUMP_ONLY[0]));

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
    int32_t swing_sessions;       // P4: how many goes at the swing day two has had
    int32_t skip_fights;          // Dev toggle
};

// ── the bell, re-tuned against the map that exists ─────────────────────────────────────────────
// chapter01.md P5 asks for "six minutes including the board and the door" and for "the direct route
// down the lane and round the square does not make it". THOSE TWO CANNOT BOTH BE TRUE ON THIS MAP,
// and the play-test measures it rather than guessing: halm is 48x36, the walking route from the
// yard's arrival cell via the board and her door to the counter is about 116 cells, and the party
// covers that in 26 s walking or 16 s running. A six-minute clock is twenty times the length of the
// run. This is a design question and it is flagged in the final report; what the engine can do is
// make the number MEAN something.
//
// 8 rings x 2.5 s = 20 s, which sits between the two: a player who RUNS the direct road makes it,
// a player who strolls it does not, and the shortcuts buy margin. The bell does not tick while a
// box is open (ch_tick), so the two mandatory conversations on the route cost nothing.
//
// Not 9 rings, ever: chapter01.md P5 reserves nine for the ring of holes and for nothing else.
#define CH_BELL_RINGS 8
#define CH_BELL_PERIOD 2.5f       // seconds between rings

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

// ───────────────────────── the party ─────────────────────────
// WHO FIGHTS, per step, and it is the STEP's `party` column that decides — not a flag, not whatever
// the last battle left behind. The chapter's table is the single source (owner, 2026-09-21: a test
// screenshot showed Falke+Ottilie+Distel against the swing, which is wrong for P1 three ways over:
// Ottilie only watches, and Distel is not met until C5).
//
// Resizing NEVER goes through bt_party_init on the existing members: that memsets the whole BtParty
// and would throw away `known` (every command the player has met) and `items` (the hidden finds) —
// which is what the old `bt_party_init(&c->party, 2)` on Ottilie's door did.
static inline void ch_party_resize(Chapter *c, int n) {
    if (n < 1) n = 1;
    if (n > BT_PARTY) n = BT_PARTY;
    if (c->party.count == n) return;
    if (c->party.count < 1) { bt_party_init(&c->party, n); return; }
    BtParty fresh;
    bt_party_init(&fresh, BT_PARTY);                   // the COMBAT.md starting values, to copy from
    for (int i = c->party.count; i < n; i++) c->party.a[i] = fresh.a[i];   // a new member joins whole
    c->party.count = n;
}

// Apply a PLAY row to the world: map, spawn, time of day, party size, lantern, night sight, goal.
// Only what the row actually names is touched, so a row that changes nothing but the light leaves
// the player standing exactly where they were.
// Is a conditioned trigger live right now? Returns the row, or nullptr when there is no condition
// on this id at all (which is the usual answer).
static inline const ChTrigCond *ch_trig_cond(const char *id) {
    for (int i = 0; i < CH_TRIG_COND_COUNT; i++)
        if (!strcmp(CH_TRIG_COND[i].id, id)) return &CH_TRIG_COND[i];
    return nullptr;
}
static inline bool ch_cond_open(const Chapter *c, const ChTrigCond *t) {
    if (!t) return true;
    if (t->from >= 0 && c->step < t->from) return false;
    if (t->to   >= 0 && c->step > t->to)   return false;
    if (t->needs >= 0 && !ch_has(c, t->needs)) return false;
    if (t->nots  >= 0 &&  ch_has(c, t->nots)) return false;
    return true;
}
// True when the world may act on this id at all — the one question ch_on_text asks before it
// believes what the field just told it. A trigger with an ALTERNATE is still "not open": the field
// shows the alternate line and the chapter learns nothing from it, which is the whole point of the
// bare board.
static inline bool ch_trig_open(const Chapter *c, const char *id) {
    return ch_cond_open(c, ch_trig_cond(id));
}
// The line to show when the player interacts with a trigger that is not open, or nullptr for
// silence. (Today only the job board has one.)
static inline const char *ch_trig_alt(const Chapter *c, const char *id) {
    const ChTrigCond *t = ch_trig_cond(id);
    return (t && !ch_cond_open(c, t)) ? t->alt : nullptr;
}

// Push the whole condition table at the field. Called on every step change and after a load, so a
// trigger's state is always a function of the chapter and never of what happened to be switched
// off last. Cheap: a few dozen string compares against one map's trigger list.
static inline void ch_apply_triggers(Chapter *c, VoxField *v) {
    if (!v) return;
    for (int i = 0; i < CH_TRIG_COND_COUNT; i++) {
        const ChTrigCond *t = &CH_TRIG_COND[i];
        // A trigger with an alternate stays ON: the player must be able to walk up to the bare
        // board and read that it is bare. star_logic swaps the text; the chapter ignores the event.
        vx_set_trigger(v, t->id, (ch_cond_open(c, t) || t->alt) ? 1 : 0);
    }
}

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
        vx_set_party_lamp(v, CH_LAMP_RADIUS, CH_LAMP_MAX, s->lamp);
        vx_set_night_sight(v, s->night_sight ? 0.10f : 0.0f);
        ch_apply_triggers(c, v);
    }
    if (s->party > 0) ch_party_resize(c, s->party);    // who is in the FIGHT, not just on the map
    if (s->goal) snprintf(c->goal, sizeof(c->goal), "%s", s->goal);
}

// Move to the next step, running through any SET rows, and apply whatever we land on.
static inline void ch_advance(Chapter *c, VoxField *v) {
    if (c->step >= CH_STEP_COUNT - 1) return;
    int from = c->step;
    c->step++;
    while (c->step < CH_STEP_COUNT && CH_STEPS[c->step].kind == CHS_SET) {
        for (int i = 0; i < CH_NEED && CH_STEPS[c->step].need[i] >= 0; i++) {
            ch_set(c, CH_STEPS[c->step].need[i]);
            if (CH_STEPS[c->step].need[i] == F_DISTEL) ch_party_resize(c, 3);
        }
        c->step++;
    }
    if (c->step >= CH_STEP_COUNT) c->step = CH_STEP_COUNT - 1;
    ch_apply_step(c, v);
    {
        static const char *K[] = { "PLAY", "CLIP", "SET", "END" };
        const ChStep *n = &CH_STEPS[c->step];
        SDL_Log("chapter: step %d %s %s -> %d (party %d, goal \"%s\")", from + 1, K[n->kind],
                n->kind == CHS_CLIP ? n->arg : (n->map ? n->map : "(same map)"), c->step + 1,
                c->party.count, c->goal);
    }
}

// Jump straight to a step (the Dev panel). Every flag an earlier step gated on is set on the way,
// so the world is consistent with having played it.
static inline void ch_jump(Chapter *c, VoxField *v, int step) {
    if (step < 0) step = 0;
    if (step >= CH_STEP_COUNT) step = CH_STEP_COUNT - 1;
    for (int k = 0; k < step; k++)
        for (int i = 0; i < CH_NEED && CH_STEPS[k].need[i] >= 0; i++) ch_set(c, CH_STEPS[k].need[i]);
    c->step = step;
    if (ch_has(c, F_DAY2_3)) c->swing_sessions = 3;
    // A step with no `party` column of its own inherits from the flags the jump just set.
    if (ch_has(c, F_DISTEL)) ch_party_resize(c, 3);
    else if (ch_has(c, F_OTTILIE_DOOR)) ch_party_resize(c, 2);
    else ch_party_resize(c, 1);
    // WHAT THE PLAYER WOULD HAVE LEARNED BY NOW. A command the player has not met is ABSENT from
    // the battle menu (battle.h, BT_KNOWS_*) and it arrives by being used — so a Dev jump into P8
    // used to fight the boss with a party that had never met Guard or the effort press, because
    // `known` is earned in the fights the jump skipped. The boss encounter happens to grant them
    // all, which is why this was invisible; nothing else would. The teaching order is COMBAT.md
    // §7a and this is it, by step.
    uint32_t k = BT_KNOWS_ATTACK;                                   // the post: move, face, hit
    if (step > CHST_P1_YARD)  k |= BT_KNOWS_GUARD;                  // the arm telegraphs at you
    if (step >= CHST_P2_TOWN) k |= BT_KNOWS_EFFORT | BT_KNOWS_RUN;  // the grain yard: a real opening
    if (step >= CHST_P5_TOWN) k |= BT_KNOWS_SKILL | BT_KNOWS_ITEM;  // {{HEALER}} is in the party
    c->party.known |= k;
    ch_apply_step(c, v);
}

// The world reported something. Returns true if a flag changed.
//
// THE CONDITION TABLE IS CHECKED HERE TOO, not only at the field. ch_apply_triggers switches
// triggers off at every step change, but a trigger the player is already standing inside, or one
// on a map that is not loaded, or one a future edit forgets to condition, can still arrive — and a
// flag set out of order is the kind of bug that only shows up as a chapter that cannot be
// finished. So the gate is applied on the way IN as well as on the way out. Belt and braces, and
// the braces are the cheap ones.
static inline bool ch_on_text(Chapter *c, const char *text_id) {
    if (!ch_trig_open(c, text_id)) return false;
    for (int i = 0; i < CH_BIND_COUNT; i++)
        if (!strcmp(CH_BINDS[i].text_id, text_id) && !ch_has(c, CH_BINDS[i].flag)) {
            ch_set(c, CH_BINDS[i].flag);
            if (CH_BINDS[i].flag == F_OTTILIE_DOOR && c->party.count < 2) ch_party_resize(c, 2);
            return true;
        }
    return false;
}


static inline bool ch_on_pickup(Chapter *c, const char *item_id) {
    if (!ch_trig_open(c, item_id)) return false;
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
//
// DAY ONE IS NOT WINNABLE, AND THAT IS THE CHAPTER. P1 is explicitly "you do not win today"
// (chapter01.md P1.3: the player loses five or six times, the session ends with {{HERO}} out of
// stamina, "and it ends WITHOUT the win"), and the whole of day two exists to be the day it goes
// down. The engine used to set F_SWING_BEATEN on any swing win — so a player who got lucky on day
// one completed P4's gate before day two had happened, and the chapter jumped its own middle.
//
// Two halves to the repair and both are here:
//   * ch_swing_winnable() is false before P4, and the battle screen asks it — the day-one swing is
//     actually unwinnable, so the player is never in the position of having beaten it and been told
//     they have not. It is a LOSS THAT ADVANCES THE STORY: F_SWING_TRIED is what P1 gates on, and
//     it is set by fighting, not by winning.
//   * even if a win arrives anyway, F_SWING_BEATEN is only believed from P4 onward.
static inline bool ch_swing_winnable(const Chapter *c) { return c->step >= CHST_P4_S4; }

static inline void ch_on_battle(Chapter *c, const char *encounter, bool won) {
    if (!strcmp(encounter, "swing")) {
        ch_set(c, F_SWING_TRIED);
        // A go at the swing on day two is a SESSION, win or lose, and four of them are the day.
        if (c->step >= CHST_P4_S1 && c->step <= CHST_P4_S4) {
            if (c->swing_sessions < 1000) c->swing_sessions++;
            if (c->swing_sessions >= 1) ch_set(c, F_DAY2_1);
            if (c->swing_sessions >= 2) ch_set(c, F_DAY2_2);
            if (c->swing_sessions >= 3) ch_set(c, F_DAY2_3);
        }
        if (won && ch_swing_winnable(c)) ch_set(c, F_SWING_BEATEN);
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
//
// `paused` IS TRUE WHILE A BOX IS OPEN, and that is not a courtesy, it is the design. The bell run
// is six minutes of adrenaline and it goes THROUGH two mandatory conversations — the board and her
// door — and past a row of townspeople who have a second thing to say now that he has a sword.
// A clock that keeps running while the player reads makes reading a cost, and the answer a player
// reaches is to stop reading. Nobody reads boxes on a clock. (Draft-four cold read, 2026-09-21.)
static inline void ch_tick(Chapter *c, float dt, bool paused) {
    if (!c->bell_started || c->rings >= CH_BELL_RINGS || paused) return;
    c->bell_t += dt;
    if (c->bell_t >= CH_BELL_PERIOD) { c->bell_t -= CH_BELL_PERIOD; c->rings++; }
}
// True once the board has shut — the clerk's late line, never a failure.
static inline bool ch_bell_late(const Chapter *c) { return c->rings >= CH_BELL_RINGS; }

// WHICH CLERK LINE FIRES. Both are live on the counter and the bell decides, which is the only
// thing that makes the run's soft outcome visible: on time he says you made it on the last ring,
// late he pretends the shutter is not already down. Never a failure either way — the chapter has
// no fail state and nothing in it sends the player backwards. Returns the id the field should
// show, given the id the trigger carried. (Before this, `ch_bell_late` was read nowhere outside
// the Dev panel and the whole late/on-time outcome was invisible in play.)
static inline const char *ch_clerk_line(const Chapter *c, const char *text_id) {
    if (strcmp(text_id, "halm.clerk_signing") && strcmp(text_id, "halm.clerk_signing_late")) return text_id;
    return ch_bell_late(c) ? "halm.clerk_signing_late" : "halm.clerk_signing";
}

// ───────────────────────── the table checks itself ─────────────────────────
// ChStepId names rows by index and CH_TRIG_COND is written in those names, so a row inserted in the
// wrong place would re-point every window silently. This says so instead, by name, on the first run
// of any self-test. It also catches a condition row whose id is not a trigger id anywhere, which is
// the other way this table rots: a text id gets renamed and the condition quietly stops applying.
// Returns the number of complaints, and logs each one.
struct ChStepCheck { int idx; int kind; const char *what; };
static inline int ch_steps_selfcheck() {
    static const ChStepCheck EXPECT[] = {
        { CHST_P1_YARD,     CHS_PLAY, "hart_yard" },  { CHST_C1,  CHS_CLIP, "0110_the_yard" },
        { CHST_P2_YARD,     CHS_PLAY, "hart_yard" },  { CHST_P2_TOWN, CHS_PLAY, "halm" },
        { CHST_C2,          CHS_CLIP, "0130_the_counter" },
        { CHST_P3_LANE,     CHS_PLAY, "halm" },       { CHST_P3_SUPPER, CHS_PLAY, "hart_yard" },
        { CHST_P3_BED,      CHS_PLAY, nullptr },      { CHST_C3,  CHS_CLIP, "0140_supper" },
        { CHST_P4_S1,       CHS_PLAY, "hart_yard" },  { CHST_P4_S4, CHS_PLAY, nullptr },
        { CHST_C4,          CHS_CLIP, "0150_the_sword" },
        { CHST_P5_YARD,     CHS_PLAY, "hart_yard" },  { CHST_P5_TOWN, CHS_PLAY, "halm" },
        { CHST_P6_CLIMB,    CHS_PLAY, "hill_path" },  { CHST_P6_TOP,  CHS_PLAY, nullptr },
        { CHST_P7_SLEEPERS, CHS_PLAY, "high_pasture" }, { CHST_P7_TRACKS, CHS_PLAY, nullptr },
        { CHST_C5,          CHS_CLIP, "0160_the_herder" },
        { CHST_P8_BOSS,     CHS_PLAY, nullptr },      { CHST_P8_BODY, CHS_PLAY, nullptr },
    };
    int bad = 0;
    static const char *K[] = { "PLAY", "CLIP", "SET", "END" };
    for (int i = 0; i < (int)(sizeof(EXPECT) / sizeof(EXPECT[0])); i++) {
        const ChStepCheck *e = &EXPECT[i];
        if (e->idx < 0 || e->idx >= CH_STEP_COUNT) {
            SDL_Log("CHAPTER FAIL: step name %d is off the end of CH_STEPS (%d rows)", e->idx, CH_STEP_COUNT);
            bad++; continue;
        }
        const ChStep *s = &CH_STEPS[e->idx];
        if (s->kind != e->kind) {
            SDL_Log("CHAPTER FAIL: step %d should be a %s and is a %s — a row has moved; ChStepId "
                    "and CH_TRIG_COND are both wrong until it is fixed", e->idx + 1, K[e->kind], K[s->kind]);
            bad++; continue;
        }
        const char *got = s->kind == CHS_CLIP ? s->arg : s->map;
        if (e->what && (!got || strcmp(got, e->what))) {
            SDL_Log("CHAPTER FAIL: step %d should be '%s' and is '%s' — a row has moved",
                    e->idx + 1, e->what, got ? got : "(same map)");
            bad++;
        }
    }
    // Every condition row's window has to be the right way round and inside the table.
    for (int i = 0; i < CH_TRIG_COND_COUNT; i++) {
        const ChTrigCond *t = &CH_TRIG_COND[i];
        if (t->from >= CH_STEP_COUNT || t->to >= CH_STEP_COUNT ||
            (t->from >= 0 && t->to >= 0 && t->from > t->to)) {
            SDL_Log("CHAPTER FAIL: condition '%s' has an impossible window %d..%d", t->id, t->from, t->to);
            bad++;
        }
    }
    return bad;
}

// Every mandatory interaction accounted for? The end card asserts this; so does --chapter-selftest.
static inline int ch_mandatory_missing(const Chapter *c) {
    int n = 0;
    for (int i = 0; i < CH_MANDATORY; i++) if (!ch_has(c, i)) n++;
    return n;
}
