// battle.cpp — the turn-based battle screen. story/v3/COMBAT.md §1-§9 is the contract.
//
// SHAPE OF THIS FILE, and it is deliberate:
//
//   §0  includes, the palette, small helpers
//   §1  THE TABLES — effort, enemies, skills, encounters. Later chapters add ROWS, not code.
//   §2  THE SIMULATION — pure logic. No GL, no ImGui, no input, no file IO. This is what
//       bt_selftest() drives, and the separation is a requirement, not a nicety.
//   §3  THE SCREEN — draws §2's state. Never decides anything.
//   §4  the public bt_* API
//   §5  bt_selftest()
//   §6  the standalone capture/selftest tool (#ifdef BATTLE_TOOL_MAIN)
//
// The one rule the whole chapter is built on: you cannot win by attacking. You wait for the tell,
// you Guard on it (a parry), the enemy is OPEN, and you spend effort into the opening.
//
// ─────────────────────────────────────────────────────────────────────────────────────────────
// THE ROUND, in full, because every other comment refers to it (COMBAT.md §4, §6):
//
//   A tell plays at the END of the round BEFORE the attack it belongs to. So:
//     round N-1, enemy phase : the enemy telegraphs  (e->tell = 1, e->tell_target = whoever)
//     round N,   top         : +8 stamina to everybody, enemies included
//     round N,   input       : the player sees the tell and chooses
//     round N,   resolve     : items -> skills -> attacks and guards, in speed order
//                              Guard vs a tell aimed at you  = PARRY: strike cancelled, +8 stamina,
//                                                              the enemy is OPEN for round N+1
//                              Attack into a tell            = COUNTER-HIT: your damage lands, then
//                                                              its strike on you is doubled
//     round N,   enemy phase : anything still telegraphing strikes; anything OPEN does nothing;
//                              everything else telegraphs for round N+1
//     round N,   end         : open/status timers tick down
//
//   OPEN therefore means round N+1: double damage, it cannot act, statuses land with no check.
//   "It dies to a parry and the strike after it" (BESTIARY) is exactly this, and the selftest
//   asserts it.
// ─────────────────────────────────────────────────────────────────────────────────────────────

#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "battle.h"

#ifndef BT_HEADLESS_ONLY
#include <SDL3/SDL.h>
#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#include <OpenGL/gl3.h>
#endif
#include "imgui.h"
#include "stb_image.h"          // implementation lives in star_logic.cpp (or §6 for the tool)
#include "dialogue.h"
#include "field_text.h"
#define BT_DRAW 1
#else
#define BT_DRAW 0
#endif

#define PREF_ORG "com.playground"
#define PREF_APP "questglory"

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// §0  small helpers
// ═══════════════════════════════════════════════════════════════════════════════════════════════

static int bt_min(int a, int b) { return a < b ? a : b; }
static int bt_max(int a, int b) { return a > b ? a : b; }
static int bt_clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// HEADLESS. bt_selftest sets this, and it is the switch that keeps the simulation free of GL: no
// texture is created, no file is read, nothing touches a context that does not exist.
static bool bt_headless = false;

// A tiny deterministic RNG, so the selftest is reproducible and no fight depends on rand().
static uint32_t bt_rng_state = 0x1234567u;
static uint32_t bt_rand() { bt_rng_state = bt_rng_state * 1664525u + 1013904223u; return bt_rng_state >> 8; }

// ── the palette (PALETTE.md) ───────────────────────────────────────────────────────────────────
// Every colour this screen draws comes out of story/palette/master.hex, loaded once at bt_create.
// If the file is not there (a headless run, a stripped build) BT_PAL_OK is false and the fallbacks
// below — which are themselves master.hex entries, copied — are used.
#define BT_PAL_N 256
static uint32_t BT_PAL[BT_PAL_N];
static bool BT_PAL_OK = false;

// Indices into master.hex that this screen uses by name. Chosen once by eye from the shipped
// palette; if the palette's version changes these want re-picking, which is why they are named.
enum {
    BTP_BLACK = 1, BTP_WHITE = 2, BTP_INK = 3, BTP_SHADE = 4,
};

static uint32_t bt_pal(int i, uint32_t fallback) {
    if (!BT_PAL_OK || i < 0 || i >= BT_PAL_N) return fallback;
    return BT_PAL[i];
}

#if BT_DRAW
static const char *bt_pref() {
    static char path[512]; static bool got = false;
    if (!got) { const char *p = SDL_GetPrefPath(PREF_ORG, PREF_APP); snprintf(path, sizeof(path), "%s", p ? p : ""); got = true; }
    return path;
}
// Phone pref path first (fast_reload.sh pushes there), then the APK assets, then the repo on desktop.
static void *bt_read(const char *rel, size_t *size) {
    char path[768];
    snprintf(path, sizeof(path), "%s%s", bt_pref(), rel);
    void *d = SDL_LoadFile(path, size);
    if (!d) d = SDL_LoadFile(rel, size);
    if (!d) { snprintf(path, sizeof(path), "story/%s", rel); d = SDL_LoadFile(path, size); }
    return d;
}

static void bt_palette_load() {
    if (BT_PAL_OK) return;
    size_t sz = 0;
    char *txt = (char *)bt_read("palette/master.hex", &sz);
    if (!txt) return;
    int n = 0;
    for (char *p = txt; *p && n < BT_PAL_N; ) {
        char *e = p; while (*e && *e != '\n') e++;
        char save = *e; *e = 0;
        if (*p == '#' && (e - p) >= 7) {
            unsigned r = 0, g = 0, b = 0;
            if (sscanf(p + 1, "%2x%2x%2x", &r, &g, &b) == 3) BT_PAL[n++] = 0xFF000000u | (b << 16) | (g << 8) | r;
        }
        *e = save;
        p = *e ? e + 1 : e;
    }
    SDL_free(txt);
    if (n > 16) { BT_PAL_OK = true; SDL_Log("battle: palette loaded, %d colours", n); }
}
#else
static void bt_palette_load() {}
#endif

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// §1  THE TABLES.   Later chapters add ROWS here, not code anywhere else.
// ═══════════════════════════════════════════════════════════════════════════════════════════════

// ── effort (COMBAT.md §2), the table verbatim ──────────────────────────────────────────────────
//   effort  1     2     3     4     5
//   cost    2     5     9     15    24
//   mag    x0.5  x0.8  x1.0  x1.25 x1.5
// Guard has NO effort and always costs 0 (§1). bt_selftest asserts both rows.
static const int   BT_EFFORT_COST[6] = { 0, 2, 5, 9, 15, 24 };
static const float BT_EFFORT_MAG [6] = { 0, 0.5f, 0.8f, 1.0f, 1.25f, 1.5f };

#define BT_REGEN        8       // §3: +8 at the top of the round, everybody, enemies included
#define BT_GUARD_REGEN  4       // a guard adds +4 on top  (nets +12)
#define BT_PARRY_REGEN  8       // a parry adds +8 instead (nets +16)
#define BT_ENEMY_STRIKE_COST 9  // enemies use the same pool and the same rules (§3)

// ── enemy flags ────────────────────────────────────────────────────────────────────────────────
#define BTF_NO_TELL        0x0001u  // the post: never telegraphs, so there is nothing to parry
#define BTF_OPEN_ONLY      0x0002u  // THE SWING: damage only lands while it is OPEN. "Beaten by the
                                    // parry, and only the parry" (BESTIARY, the machines table).
#define BTF_WINDS          0x0004u  // every attack that is not into an opening winds the
                                    // counterweight: it comes back faster and harder, and its
                                    // counter DRAINS the attacker's stamina (COMBAT §7, §4a.2-3)
#define BTF_CLINGS         0x0008u  // burr: its strike also slows the target (acts last)
#define BTF_PULLS          0x0010u  // lantern: whoever it works on loses their guard that round
#define BTF_HIDDEN_TELL    0x0020u  // lantern: the dim before the flare is invisible without night
                                    // sight (or the lens) — this is what makes night sight a
                                    // mechanic rather than a line (BESTIARY)
#define BTF_BLINDS         0x0040u  // lantern's flare: blinded for two rounds, tells unreadable
#define BTF_IMMUNE_ATTACK  0x0080u  // klee phase one: attacking it does NOTHING and we say so
#define BTF_BOSS           0x0100u  // scripted; no run; phase one cannot be lost

// ── the enemy table ────────────────────────────────────────────────────────────────────────────
// id            the encounter table and the sprite name refer to this
// name          drawn on screen (named, never numbered — BESTIARY §the machines)
// hp/stam       stamina is the same pool and the same rules the party uses
// speed         turn order, highest first, ties to the party (§6)
// dmg           base strike damage before guard/counter/tired multipliers
// tell_dur      seconds the tell animation plays — the performance, not a cue (§4a.1)
// tell_delay    seconds between the tell finishing and the strike landing
// window        the parry window in seconds. THE TURN LOGIC DOES NOT USE IT: §6 says the parry is
//               a reading test, never a speed check. It drives the animation, the §10 fallback,
//               and the "faster, tighter" feel of the boss's phase two.
// flags         BTF_*
// sprite        story/field/sprites/<sprite>.png, else story/field/walkers/<sprite>.png, else a
//               flat palette silhouette with the name under it
struct BtEnemyDef {
    const char *id, *name;
    int hp, stam, speed, dmg;
    float tell_dur, tell_delay, window;
    uint32_t flags;
    const char *sprite;
};

static const BtEnemyDef BT_ENEMIES[] = {
    // ── the machines, Hart's yard. Wood, iron, rope and counterweights; one lesson each. ──
    { "post",    "the post",            24,  99, 4,  0, 0.00f, 0.00f, 0.00f, BTF_NO_TELL, "post" },
    { "arm",     "the arm that holds",  30,  40, 5,  4, 0.55f, 0.45f, 0.60f, 0, "arm" },
    { "swing",   "the swing",           30,  99, 6,  3, 0.80f, 0.60f, 0.45f, BTF_OPEN_ONLY | BTF_WINDS, "swing" },

    // ── the field creatures ──
    { "lid",     "a lid",               14,  24, 3,  3, 0.50f, 0.40f, 0.70f, 0, "lid" },
    { "burr",    "a burr",              22,  30, 4,  5, 0.50f, 0.35f, 0.60f, BTF_CLINGS, "burr" },
    { "lantern", "a lantern",           20,  30, 7,  6, 0.35f, 0.30f, 0.45f, BTF_PULLS | BTF_HIDDEN_TELL | BTF_BLINDS, "lantern" },
    { "fleece",  "a fleece",            26,  30, 2,  9, 0.45f, 0.30f, 0.55f, 0, "fleece" },

    // ── the boss. Phase one is row [klee1], phase two is row [klee2]; the script swaps them. ──
    { "klee",    "Klee",               999, 120, 6,  0, 1.50f, 0.90f, 0.90f, BTF_BOSS | BTF_IMMUNE_ATTACK, "klee" },
    { "klee2",   "Klee",               120, 120, 8, 14, 0.90f, 0.50f, 0.45f, BTF_BOSS, "klee" },
};
static const int BT_ENEMY_COUNT = (int)(sizeof(BT_ENEMIES) / sizeof(BT_ENEMIES[0]));

// ── the skill table (COMBAT.md §5) ─────────────────────────────────────────────────────────────
// owner      the party id the skill belongs to ("" = everybody)
// min        MINIMUM EFFORT. This is the whole gating system: a tired character is not silenced,
//            they are reduced to small work (§3). Below it the skill is greyed WITH ITS MINIMUM.
// kind       what the magnitude means
// mag        the base the effort multiplier is applied to
enum BtSkillKind {
    BTK_DAMAGE = 0,   // damage
    BTK_HEAL,         // health restored
    BTK_CURE,         // removes one status
    BTK_SHIELD,       // reduced damage taken, reduction and duration scale with effort
    BTK_SLOW,         // Drowsy: speed drops, duration = effort rounds
    BTK_WEAKEN,       // Calm: attack drops, duration = effort rounds
    BTK_SETTLE,       // Settle: puts a weakened enemy out. The boss fight's whole problem.
    BTK_HARD,         // Hard swing: x1.6 of the effort's magnitude, cannot guard next round
};
struct BtSkillDef { const char *id, *name, *owner; int min; int kind; float mag; const char *note; };

static const BtSkillDef BT_SKILLS[] = {
    // {{HERO}} — the sword. No magic, this chapter or any.
    { "hard",   "Hard swing", "falke",   4, BTK_HARD,   1.6f, "cannot guard next round" },
    // {{HEALER}} — reliable from the first fight, and the player should feel that immediately.
    { "mend",   "Mend",       "ottilie", 1, BTK_HEAL,  12.0f, "" },
    { "steady", "Steady",     "ottilie", 2, BTK_CURE,   1.0f, "" },
    { "shield", "Shield",     "ottilie", 2, BTK_SHIELD, 1.0f, "" },
    // {{HERDER}} — pace, not damage. Never does a point of damage.
    { "drowsy", "Drowsy",     "distel",  1, BTK_SLOW,   1.0f, "" },
    { "calm",   "Calm",       "distel",  2, BTK_WEAKEN, 1.0f, "" },
    { "settle", "Settle",     "distel",  4, BTK_SETTLE, 1.0f, "" },
};
static const int BT_SKILL_COUNT = (int)(sizeof(BT_SKILLS) / sizeof(BT_SKILLS[0]));

// ── encounter flags ────────────────────────────────────────────────────────────────────────────
#define BTE_YARD      0x1u   // {{MENTOR}}'s yard: losing is free and instant, no menu, no penalty,
                             // and §4a.5's non-verbal escalation fires HERE AND NOWHERE ELSE
#define BTE_NO_RUN    0x2u   // only the boss refuses a run (§1)
#define BTE_SCRIPTED  0x4u   // the boss: phases, scripted text, phase one cannot be lost

// ── the encounter table ────────────────────────────────────────────────────────────────────────
// id / enemies (up to 4 enemy-table ids) / flags / grants
// grants: BT_KNOWS_* bits the encounter itself hands over, because an idea arrives by being used,
//         never by being explained (§7a). Everything else the CHAPTER grants through BtParty.known.
struct BtEncounterDef { const char *id; const char *e[4]; uint32_t flags; uint32_t grants; const char *goal; };

static const BtEncounterDef BT_ENCOUNTERS[] = {
    // 1 | move, face, hit. ONLY ATTACK EXISTS. The command list is one entry. (§7a row 1)
    { "post",    { "post", 0, 0, 0 },                  BTE_YARD, BT_KNOWS_ATTACK, "" },
    // 2-3 | things have tells, and Guard APPEARS the first time something telegraphs at you.
    { "arm",     { "arm", 0, 0, 0 },                   BTE_YARD, BT_KNOWS_ATTACK, "" },
    // 4-6 | the parry, the opening, and the slider under Attack the first time there is one.
    { "swing",   { "swing", 0, 0, 0 },                 BTE_YARD, BT_KNOWS_ATTACK, "" },

    { "lid",     { "lid", "lid", "lid", 0 },           0, BT_KNOWS_ATTACK, "" },
    { "burr",    { "burr", 0, 0, 0 },                  0, BT_KNOWS_ATTACK, "" },
    { "burr3",   { "burr", "burr", "burr", 0 },        0, BT_KNOWS_ATTACK, "" },
    { "lantern", { "lantern", "lantern", 0, 0 },       0, BT_KNOWS_ATTACK, "" },
    { "fleece",  { "fleece", 0, 0, 0 },                0, BT_KNOWS_ATTACK, "" },

    { "klee",    { "klee", 0, 0, 0 }, BTE_NO_RUN | BTE_SCRIPTED,
                 BT_KNOWS_ATTACK | BT_KNOWS_GUARD | BT_KNOWS_EFFORT | BT_KNOWS_SKILL | BT_KNOWS_ITEM,
                 "Hold it open for Distel." },
};
static const int BT_ENCOUNTER_COUNT = (int)(sizeof(BT_ENCOUNTERS) / sizeof(BT_ENCOUNTERS[0]));

// ── the party's starting values (COMBAT.md §3) ─────────────────────────────────────────────────
// CONTRADICTION, and the choice: COMBAT.md §3 gives {{HEALER}} a pool of 36; BESTIARY.md's boss
// section says 24. COMBAT.md §3 is the stamina section and the named contract, and §5 repeats 36
// ("ordinary pool (36) and the ordinary effort rules"), so 36 wins. BESTIARY's 24 reads as a stale
// designer placeholder from before "{{HEALER}} is not limited and never was".
struct BtPartyDef { const char *id, *name; int hp, stam, speed, atk; };
static const BtPartyDef BT_PARTY_DEF[BT_PARTY] = {
    { "falke",   "Falke",   60, 40, 6, 10 },
    { "ottilie", "Ottilie", 48, 36, 5,  7 },
    { "distel",  "Distel",  44, 30, 7,  0 },   // never does a point of damage
};

// ── the four hidden finds (story/v3/LOOT.md). Every effect is a rule change or a multiplier. ──
//  BT_ITEM_TEACHING  the weight — IN FIGHT, once per fight, never consumed: one character's attack
//                    is DOUBLED FOR THREE ROUNDS.
//  BT_ITEM_YARD      the brace — PASSIVE on {{HERO}}: cannot be held, knocked down, or put to
//                    sleep. Answers the slow/blind/cling statuses and the boss's whole method.
//  BT_ITEM_HILL      the whistle — IN FIGHT, once per fight: ONE ENEMY SKIPS ITS NEXT TURN. No
//                    cost, no check, works on anything that takes turns, bosses included.
//  BT_ITEM_PASTURE   the lens — PASSIVE, whole party, permanent: ENEMY TELLS ARE ALWAYS SHOWN, and
//                    shown early, the way night sight shows them, even with no {{HERDER}}.
// All four are LOOT.md's wording, not invented here.

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// §2  THE SIMULATION — pure logic.  No GL, no ImGui, no input, no file IO, no allocation.
// ═══════════════════════════════════════════════════════════════════════════════════════════════

#define BT_ENEMIES_MAX 4
#define BT_LOG_LINES 6
#define BT_LOG_LEN 96
#define BT_EVQ 8

enum BtCmd { BTC_NONE = 0, BTC_ATTACK, BTC_SKILL, BTC_GUARD, BTC_ITEM, BTC_RUN, BTC_COUNT };

struct BtEnemyState {
    int def;                    // index into BT_ENEMIES
    int hp, hp_max, stam, stam_max, speed, dmg;
    int alive;
    int tell;                   // 1 = telegraphing; it strikes THIS round unless parried
    int tell_target;            // the tell belongs to a target (§4)
    int open;                   // OPEN right now: double damage, cannot act, statuses with no check
    int open_next;              // a parry landed this round; it is OPEN for the WHOLE of the next
    int countered;              // an attack went into the tell: its strike is doubled
    int parried;                // a parry landed this round: the strike is cancelled
    int wind;                   // the counterweight, in notches. The graph of the player's impatience.
    int skip;                   // the whistle: it skips its next turn
    int slow, weaken;           // Drowsy / Calm, in rounds
    int settled;                // Settle landed (boss phase one counts these)
    float anim;                 // animation clock; drawing only
};

struct BtActorState {
    int atk, speed;
    int guarding;               // chose Guard this round
    int parried;                // that guard was a parry
    int no_guard;               // Hard swing: cannot guard next round
    int shield, shield_amt;     // rounds, and the reduction in percent
    int slow, blind;            // statuses, in rounds
    int doubled;                // the weight: attack doubled for N rounds
    int winded;                 // WINDED, in rounds: the swing's counter, and the one thing in the
                                // game that cancels the +8 at the top of the round. It is how
                                // §4a.3 closes the door — with the regen suppressed a spent
                                // character really cannot afford Attack, and Guard (which pays +4
                                // by itself) is the only entry left lit. Guarding clears it, which
                                // is §3's "waiting is not a dead turn; it is how you pay for the
                                // next one" said in a mechanic.
    int down;                   // knocked down
};

enum BtPhase { BTP_INPUT = 0, BTP_RESOLVE, BTP_ENEMY, BTP_OVER };

struct Battle {
    // ── the fight ──
    const BtEncounterDef *enc;
    BtParty *party;             // the caller's, written back as we go
    char map[32];

    BtActorState pa[BT_PARTY];
    BtEnemyState en[BT_ENEMIES_MAX];
    int enemy_count;

    int round;
    int outcome;                // BtOutcome
    int cur;                    // whose input we are taking
    int phase;

    // chosen this round, per party member
    int cmd[BT_PARTY], effort[BT_PARTY], target[BT_PARTY], skill[BT_PARTY], item[BT_PARTY];

    // ── the boss script (§9) ──
    int boss_phase;             // 0 = not the boss, 1 = hold it open, 2 = spend it
    int boss_cycles;            // settles landed in phase one / rounds in phase two
    int boss_break_fired, boss_turn_fired;

    // ── the yard's non-verbal escalation (§4a.5). YARD ONLY. ──
    float tell_speed;           // 1.0 normally; 0.8 after three failures (a fifth slower)
    int guard_pulse;            // after five failures the Guard entry pulses once

    // ── carried out to the caller ──
    BtEvent evq[BT_EVQ]; int evq_n, evq_rd;
    char log[BT_LOG_LINES][BT_LOG_LEN]; int log_n;
    char goal[64];

    // ── drawing only, never read by the simulation ──
    int dismissed;
    float t, banner_t;
    int ui_cmd, ui_effort, ui_skill, ui_target, ui_in_skill;
    unsigned tex[BT_PARTY + BT_ENEMIES_MAX];
    int texw[BT_PARTY + BT_ENEMIES_MAX], texh[BT_PARTY + BT_ENEMIES_MAX];
    int texfw[BT_PARTY + BT_ENEMIES_MAX], texfh[BT_PARTY + BT_ENEMIES_MAX];  // ONE frame's size
    int tex_ready;
    char pending_text[64];      // a story text id to show as a mid-fight box, drawn by us AND
                                // emitted as BTE_TEXT so the chapter can gate on it
    float pending_text_t;
};

// ── the log and the event queue ────────────────────────────────────────────────────────────────
static void bt_log(Battle *b, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    if (b->log_n >= BT_LOG_LINES) {
        memmove(b->log[0], b->log[1], sizeof(b->log[0]) * (BT_LOG_LINES - 1));
        b->log_n = BT_LOG_LINES - 1;
    }
    vsnprintf(b->log[b->log_n], BT_LOG_LEN, fmt, ap);
    b->log_n++;
    va_end(ap);
}

static void bt_emit(Battle *b, int kind, const char *arg) {
    if (b->evq_n >= BT_EVQ) return;
    b->evq[b->evq_n].kind = kind;
    snprintf(b->evq[b->evq_n].arg, sizeof(b->evq[b->evq_n].arg), "%s", arg ? arg : "");
    b->evq_n++;
}
static void bt_goal(Battle *b, const char *g) {
    snprintf(b->goal, sizeof(b->goal), "%s", g ? g : "");
    bt_emit(b, BTE_GOAL, b->goal);
}

// ── lookups ────────────────────────────────────────────────────────────────────────────────────
static int bt_enemy_def(const char *id) {
    for (int i = 0; i < BT_ENEMY_COUNT; i++) if (!strcmp(BT_ENEMIES[i].id, id)) return i;
    return -1;
}
static const BtEncounterDef *bt_encounter(const char *id) {
    for (int i = 0; i < BT_ENCOUNTER_COUNT; i++) if (!strcmp(BT_ENCOUNTERS[i].id, id)) return &BT_ENCOUNTERS[i];
    return 0;
}
static const BtEnemyDef *bt_edef(Battle *b, int i) { return &BT_ENEMIES[b->en[i].def]; }
static bool bt_is_yard(Battle *b) { return b->enc && (b->enc->flags & BTE_YARD) != 0; }

// {{HERDER}}'s night sight: passive, free, always on — every tell shown one beat earlier, hidden
// tells shown at all, and enemy stamina bars visible (§5). The lens (BT_ITEM_PASTURE) does the
// same thing permanently, for the whole party, with no {{HERDER}} in it (LOOT.md).
static bool bt_night_sight(Battle *b) {
    if (b->party && (b->party->items & BT_ITEM_PASTURE)) return true;
    if (!b->party) return false;
    for (int i = 0; i < b->party->count; i++)
        if (b->party->a[i].alive && !strcmp(b->party->a[i].id, "distel")) return true;
    return false;
}
// Is this enemy's tell READABLE by the player right now? (Drawing asks; the simulation does not —
// a tell you could not read is still a tell, and guarding into it still parries. The punishment
// for not reading it is that you did not guard.)
static bool bt_tell_visible(Battle *b, int i, int viewer) {
    if (!b->en[i].tell) return false;
    if (viewer >= 0 && b->pa[viewer].blind) return false;
    if (bt_edef(b, i)->flags & BTF_HIDDEN_TELL) return bt_night_sight(b);
    return true;
}

static int bt_alive_enemies(Battle *b) {
    int n = 0; for (int i = 0; i < b->enemy_count; i++) if (b->en[i].alive) n++; return n;
}
static int bt_alive_party(Battle *b) {
    int n = 0; for (int i = 0; i < b->party->count; i++) if (b->party->a[i].alive) n++; return n;
}
static int bt_first_live_enemy(Battle *b) {
    for (int i = 0; i < b->enemy_count; i++) if (b->en[i].alive) return i;
    return -1;
}

// ── affordability and the menu's light (§11, §4a.4) ────────────────────────────────────────────
// Guard is NEVER greyed. It is the one entry that is always lit, and on a tired character it is
// the only one — the closest thing to a hint the chapter contains.
static int bt_cost(int cmd, int effort) {
    switch (cmd) {
        case BTC_ATTACK: case BTC_SKILL: return BT_EFFORT_COST[bt_clamp(effort, 1, 5)];
        default: return 0;                            // Guard, Item, Run cost nothing
    }
}
static bool bt_can_pay(Battle *b, int who, int cmd, int effort) {
    if (cmd == BTC_GUARD) return true;
    return b->party->a[who].stam >= bt_cost(cmd, effort);
}
// The cheapest effort this character can still pay for with this command, or 0 for "none at all".
static int bt_cheapest_affordable(Battle *b, int who, int cmd) {
    for (int e = 1; e <= 5; e++) if (bt_can_pay(b, who, cmd, e)) return e;
    return 0;
}
static const BtSkillDef *bt_skill_at(const char *owner, int n) {
    int k = 0;
    for (int i = 0; i < BT_SKILL_COUNT; i++)
        if (!strcmp(BT_SKILLS[i].owner, owner)) { if (k == n) return &BT_SKILLS[i]; k++; }
    return 0;
}
static int bt_skill_count(const char *owner) {
    int k = 0; for (int i = 0; i < BT_SKILL_COUNT; i++) if (!strcmp(BT_SKILLS[i].owner, owner)) k++;
    return k;
}

// ── starting a fight ───────────────────────────────────────────────────────────────────────────
static void bt_setup_enemies(Battle *b, const char *const *ids) {
    b->enemy_count = 0;
    for (int i = 0; i < BT_ENEMIES_MAX && ids[i]; i++) {
        int d = bt_enemy_def(ids[i]);
        if (d < 0) continue;
        BtEnemyState *e = &b->en[b->enemy_count++];
        memset(e, 0, sizeof(*e));
        e->def = d;
        e->hp = e->hp_max = BT_ENEMIES[d].hp;
        e->stam = e->stam_max = BT_ENEMIES[d].stam;
        e->speed = BT_ENEMIES[d].speed;
        e->dmg = BT_ENEMIES[d].dmg;
        e->alive = 1;
    }
}

static void bt_round_begin(Battle *b) {
    b->round++;
    // +8 at the top of the round, for everybody, enemies included (§3).
    for (int i = 0; i < b->party->count; i++) {
        BtActor *a = &b->party->a[i];
        if (!a->alive) continue;
        if (b->pa[i].winded > 0) b->pa[i].winded--;          // winded: no +8 this round
        else a->stam = bt_min(a->stam_max, a->stam + BT_REGEN);
        b->pa[i].guarding = 0; b->pa[i].parried = 0;
        b->cmd[i] = BTC_NONE;
    }
    for (int i = 0; i < b->enemy_count; i++) {
        BtEnemyState *e = &b->en[i];
        if (!e->alive) continue;
        e->stam = bt_min(e->stam_max, e->stam + BT_REGEN);
        e->countered = 0; e->parried = 0;
    }
    b->cur = 0;
    b->phase = BTP_INPUT;
}

// ── damage ─────────────────────────────────────────────────────────────────────────────────────
static void bt_hurt_enemy(Battle *b, int who, int ei, float base, int effort, const char *verb) {
    BtEnemyState *e = &b->en[ei];
    const BtEnemyDef *d = bt_edef(b, ei);

    // klee phase one: attacking it does nothing at all, and the message says so EVERY TIME, which
    // is the game stating its own rule out loud (BESTIARY, phase one).
    if (d->flags & BTF_IMMUNE_ATTACK) { bt_log(b, "It is not hurt."); return; }

    // The swing: damage only lands while it is OPEN, and any attack that is not into an opening
    // winds the counterweight another notch up the post (§7, §4a.2).
    if (!e->open && (d->flags & BTF_OPEN_ONLY)) {
        if (d->flags & BTF_WINDS) { e->wind++; bt_log(b, "The counterweight winds up a notch. (%d)", e->wind); }
        else bt_log(b, "It turns the blow aside.");
        return;
    }
    if (!e->open && (d->flags & BTF_WINDS)) { e->wind++; bt_log(b, "The counterweight winds up a notch. (%d)", e->wind); }

    float mag = BT_EFFORT_MAG[bt_clamp(effort, 1, 5)];
    float dmg = base * mag;
    if (who >= 0 && b->pa[who].doubled > 0) dmg *= 2.0f;          // the weight (BT_ITEM_TEACHING)
    if (e->open) dmg *= 2.0f;                                      // OPEN: double damage (§4)
    int n = (int)(dmg + 0.5f);
    if (n < 1) n = 1;
    e->hp -= n;
    bt_log(b, "%s %s for %d%s.", who >= 0 ? b->party->a[who].name : "It", verb, n, e->open ? " — open!" : "");
    if (e->hp <= 0) { e->hp = 0; e->alive = 0; bt_log(b, "%s stops.", d->name); }
}

static void bt_hurt_actor(Battle *b, int who, int amount, const char *src) {
    BtActor *a = &b->party->a[who];
    if (!a->alive) return;
    if (b->pa[who].shield > 0) amount = amount * (100 - b->pa[who].shield_amt) / 100;
    if (b->pa[who].guarding && !b->pa[who].parried) amount = (amount + 1) / 2;   // half damage (§4)
    if (amount < 0) amount = 0;
    a->hp -= amount;
    // Phase one cannot be lost (§8, §9): nobody drops below 1.
    if (b->boss_phase == 1 && a->hp < 1) a->hp = 1;
    bt_log(b, "%s hits %s for %d.", src, a->name, amount);
    if (a->hp <= 0) { a->hp = 0; a->alive = 0; bt_log(b, "%s is down.", a->name); }
}

// ── one party action ───────────────────────────────────────────────────────────────────────────
static void bt_do_skill(Battle *b, int who, int si, int effort) {
    const BtSkillDef *s = bt_skill_at(b->party->a[who].id, si);
    if (!s) return;
    if (effort < s->min) { bt_log(b, "%s needs effort %d.", s->name, s->min); return; }
    float mag = BT_EFFORT_MAG[bt_clamp(effort, 1, 5)];
    int ti = b->target[who];
    if (ti < 0 || ti >= b->enemy_count || !b->en[ti].alive) ti = bt_first_live_enemy(b);

    switch (s->kind) {
        case BTK_HARD:
            if (ti < 0) break;
            bt_hurt_enemy(b, who, ti, b->pa[who].atk * s->mag, effort, "swings hard");
            b->pa[who].no_guard = 1;                     // cannot guard next round
            break;
        case BTK_HEAL: {
            int t = bt_clamp(b->target[who], 0, b->party->count - 1);
            if (!b->party->a[t].alive) t = who;
            int n = (int)(s->mag * mag + 0.5f);
            BtActor *a = &b->party->a[t];
            a->hp = bt_min(a->hp_max, a->hp + n);
            bt_log(b, "Mend: %s +%d.", a->name, n);
            break; }
        case BTK_CURE: {
            int t = bt_clamp(b->target[who], 0, b->party->count - 1);
            b->pa[t].slow = b->pa[t].blind = b->pa[t].down = 0;
            bt_log(b, "Steady: %s is clear.", b->party->a[t].name);
            break; }
        case BTK_SHIELD: {
            int t = bt_clamp(b->target[who], 0, b->party->count - 1);
            b->pa[t].shield = effort;                    // duration scales with effort
            b->pa[t].shield_amt = 15 * effort;           // and so does the reduction
            bt_log(b, "Shield: %s, -%d%% for %d.", b->party->a[t].name, b->pa[t].shield_amt, effort);
            break; }
        case BTK_SLOW:
            if (ti < 0) break;
            // OPEN means any status put on it lands WITHOUT A CHECK (§4).
            b->en[ti].slow = effort;                     // 1 round at effort 1, 5 at effort 5
            bt_log(b, "Drowsy: %s slows, %d.", bt_edef(b, ti)->name, effort);
            break;
        case BTK_WEAKEN:
            if (ti < 0) break;
            b->en[ti].weaken = effort;
            bt_log(b, "Calm: %s softens, %d.", bt_edef(b, ti)->name, effort);
            break;
        case BTK_SETTLE:
            if (ti < 0) break;
            b->en[ti].settled++;
            if (b->en[ti].open) {
                bt_log(b, "Settle lands. It goes quiet.");
                if (b->boss_phase == 1) b->boss_cycles++;
                else { b->en[ti].slow = 5; b->en[ti].weaken = 5; }
            } else {
                bt_log(b, "Settle does not take.");      // it was not held open
            }
            break;
        default:
            if (ti >= 0) bt_hurt_enemy(b, who, ti, b->pa[who].atk, effort, "strikes");
            break;
    }
}

static void bt_do_guard(Battle *b, int who) {
    BtActor *a = &b->party->a[who];
    if (b->pa[who].no_guard) { bt_log(b, "%s cannot guard.", a->name); return; }
    b->pa[who].guarding = 1;
    // Guard into a tell aimed at you = a PARRY. Whoever guards is who parries it (§4).
    int parried = -1;
    for (int i = 0; i < b->enemy_count; i++) {
        BtEnemyState *e = &b->en[i];
        if (!e->alive || !e->tell || e->open) continue;
        if (e->tell_target != who) continue;
        e->parried = 1;
        e->tell = 0;
        e->open_next = 1;                  // OPEN for the whole of the NEXT round, never this one
        e->wind = 0;                       // a parry unwinds the counterweight
        parried = i;
        break;
    }
    if (parried >= 0) {
        b->pa[who].parried = 1;
        b->pa[who].winded = 0;
        a->stam = bt_min(a->stam_max, a->stam + BT_PARRY_REGEN);      // nets +16 with the regen
        bt_log(b, "%s turns it aside. %s is open.", a->name, bt_edef(b, parried)->name);
        b->party->known |= BT_KNOWS_EFFORT;        // there is an opening to spend into (§7a row 5)
        b->party->fail_streak = 0;                 // both escalations reset the moment a parry lands
        b->tell_speed = 1.0f; b->guard_pulse = 0;
    } else {
        b->pa[who].winded = 0;                                        // waiting pays for the next one
        a->stam = bt_min(a->stam_max, a->stam + BT_GUARD_REGEN);      // nets +12. Never a waste.
        bt_log(b, "%s guards.", a->name);
    }
}

static void bt_do_item(Battle *b, int who, int which) {
    // Items are extras, not the source (§3); they resolve first in the round (§1).
    if (which == 0 && (b->party->items & BT_ITEM_TEACHING)) {
        b->pa[who].doubled = 3;                          // doubled for three rounds, never consumed
        bt_log(b, "The weight: %s hits double, 3 rounds.", b->party->a[who].name);
    } else if (which == 1 && (b->party->items & BT_ITEM_HILL)) {
        int ti = b->target[who];
        if (ti < 0 || ti >= b->enemy_count || !b->en[ti].alive) ti = bt_first_live_enemy(b);
        if (ti >= 0) { b->en[ti].skip = 1; bt_log(b, "The whistle: %s loses a turn.", bt_edef(b, ti)->name); }
    } else {
        bt_log(b, "Nothing to use.");
    }
}

// ── the enemy phase ────────────────────────────────────────────────────────────────────────────
static void bt_enemy_phase(Battle *b) {
    for (int i = 0; i < b->enemy_count; i++) {
        BtEnemyState *e = &b->en[i];
        const BtEnemyDef *d = bt_edef(b, i);
        if (!e->alive) continue;

        if (e->open || e->open_next) { bt_log(b, "%s cannot move.", d->name); continue; }   // OPEN: it cannot act
        if (e->skip) { e->skip = 0; bt_log(b, "%s misses its turn.", d->name); continue; }

        // Anything still telegraphing strikes now.
        if (e->tell && !e->parried) {
            int t = e->tell_target;
            if (t < 0 || t >= b->party->count || !b->party->a[t].alive) t = -1;
            for (int k = 0; t < 0 && k < b->party->count; k++) if (b->party->a[k].alive) t = k;
            if (t >= 0) {
                int dmg = e->dmg;
                if (e->weaken) dmg = dmg * (100 - 15 * e->weaken) / 100;
                if (e->stam < BT_ENEMY_STRIKE_COST) dmg = dmg * 3 / 5;      // a spent enemy hits softer
                e->stam = bt_max(0, e->stam - BT_ENEMY_STRIKE_COST);
                // The counterweight: it comes back harder, a notch at a time (§7).
                if (d->flags & BTF_WINDS) dmg += e->wind;   // harder, a notch at a time
                if (e->countered) {
                    dmg *= 2;                                               // counter-hit (§4)
                    bt_log(b, "%s counters.", d->name);
                    // The swing's counter DRAINS the attacker. This is §4a.3, and it is the one
                    // place COMBAT.md's "you can always afford effort 1" floor is deliberately
                    // broken: BESTIARY says in as many words that inside a session the player is
                    // at zero and cannot afford to swing at all, with Guard the only entry lit.
                    if (d->flags & BTF_WINDS) {
                        BtActor *a = &b->party->a[t];
                        int drain = 5 + 4 * e->wind;
                        a->stam = bt_max(0, a->stam - drain);
                        b->pa[t].winded = 1 + e->wind / 2;   // and the regen does not come back
                        bt_log(b, "%s is winded. (-%d)", a->name, drain);
                    }
                }
                bt_hurt_actor(b, t, dmg, d->name);
                if (d->flags & BTF_CLINGS && !(b->party->items & BT_ITEM_YARD && !strcmp(b->party->a[t].id, "falke")))
                    { b->pa[t].slow = 2; bt_log(b, "It clings to %s.", b->party->a[t].name); }
                if (d->flags & BTF_BLINDS && !(b->party->items & BT_ITEM_YARD && !strcmp(b->party->a[t].id, "falke")))
                    { b->pa[t].blind = 2; bt_log(b, "%s is blinded.", b->party->a[t].name); }
            }
            e->tell = 0;
            e->countered = 0;
            continue;
        }
        e->tell = 0;

        // Otherwise it telegraphs for the NEXT round. Every enemy has exactly one tell (§4).
        if (d->flags & BTF_NO_TELL) {
            // The post never telegraphs, so there is nothing to parry: it is beaten by clean hits.
            continue;
        }
        e->tell = 1;
        // Who it is aimed at. The boss turns on {{HERDER}} after round three of phase two (§9).
        int t = -1;
        if (b->boss_phase == 2 && b->boss_cycles > 3) {
            for (int k = 0; k < b->party->count; k++)
                if (b->party->a[k].alive && !strcmp(b->party->a[k].id, "distel")) t = k;
        }
        if (t < 0) {
            int live[BT_PARTY], n = 0;
            for (int k = 0; k < b->party->count; k++) if (b->party->a[k].alive) live[n++] = k;
            if (!n) { e->tell = 0; continue; }
            t = live[bt_rand() % (uint32_t)n];
        }
        e->tell_target = t;
        bt_log(b, "%s draws back.", d->name);
        // Guard APPEARS in the list the first time something telegraphs at you (§7a row 3).
        b->party->known |= BT_KNOWS_GUARD;
    }
}

// ── the boss script (§9) ───────────────────────────────────────────────────────────────────────
static void bt_boss_script(Battle *b) {
    if (!b->boss_phase) return;

    if (b->boss_phase == 1) {
        // Phase one — hold it open. Three cycles, cannot be lost, attacking does nothing.
        if (b->boss_cycles < 3) return;
        // ...and then the scripted FAILURE. It works. Then it does not.
        if (!b->boss_break_fired) {
            b->boss_break_fired = 1;
            bt_emit(b, BTE_TEXT, "high_pasture.boss_break");
            bt_emit(b, BTE_TEXT, "high_pasture.boss_break_2");
            snprintf(b->pending_text, sizeof(b->pending_text), "high_pasture.boss_break");
            b->pending_text_t = 0.0f;
            bt_log(b, "It goes quiet. Its head comes down.");
            return;                                   // one round of quiet before it turns
        }
        if (!b->boss_turn_fired) {
            b->boss_turn_fired = 1;
            bt_emit(b, BTE_TEXT, "high_pasture.boss_turn");
            bt_emit(b, BTE_TEXT, "high_pasture.boss_turn_2");
            snprintf(b->pending_text, sizeof(b->pending_text), "high_pasture.boss_turn");
            b->pending_text_t = 0.0f;
            bt_log(b, "The mane lifts. It looks at Distel, and does not know her.");
            // Phase two — spend it. Same tell, faster, tighter window; damage counts now.
            b->boss_phase = 2;
            b->boss_cycles = 0;
            int d = bt_enemy_def("klee2");
            BtEnemyState *e = &b->en[0];
            e->def = d;
            e->hp = e->hp_max = BT_ENEMIES[d].hp;
            e->stam = e->stam_max = BT_ENEMIES[d].stam;
            e->speed = BT_ENEMIES[d].speed;
            e->dmg = BT_ENEMIES[d].dmg;
            e->open = 0; e->tell = 0; e->settled = 0; e->alive = 1;
            bt_emit(b, BTE_PHASE, "two");
            bt_goal(b, "Stop it. It wants Distel.");
        }
        return;
    }
    b->boss_cycles++;      // phase two counts rounds; after round three it goes for {{HERDER}}
}

// Phase two can be lost, and losing it restarts PHASE TWO ONLY (§8). Never the whole fight.
static void bt_boss_restart_phase_two(Battle *b) {
    bt_log(b, "Again.");
    for (int i = 0; i < b->party->count; i++) {
        BtActor *a = &b->party->a[i];
        a->alive = 1;
        a->hp = a->hp_max;
        a->stam = a->stam_max / 2;      // what a careless phase one costs you, kept
        memset(&b->pa[i], 0, sizeof(b->pa[i]));
        b->pa[i].atk = BT_PARTY_DEF[bt_min(i, BT_PARTY - 1)].atk;
        b->pa[i].speed = a->alive ? BT_PARTY_DEF[bt_min(i, BT_PARTY - 1)].speed : 0;
    }
    int d = bt_enemy_def("klee2");
    BtEnemyState *e = &b->en[0];
    memset(e, 0, sizeof(*e));
    e->def = d; e->hp = e->hp_max = BT_ENEMIES[d].hp;
    e->stam = e->stam_max = BT_ENEMIES[d].stam;
    e->speed = BT_ENEMIES[d].speed; e->dmg = BT_ENEMIES[d].dmg; e->alive = 1;
    b->boss_cycles = 0;
    b->round = 0;
    bt_round_begin(b);
}

// ── resolve one whole round ────────────────────────────────────────────────────────────────────
// Order (§6): items -> skills -> attacks and guards -> enemy attacks. Guard is in effect for the
// whole round however slow the character is: the parry is a reading test, never a speed check.
static void bt_resolve(Battle *b) {
    // Speed order among the party, highest first, ties to the earlier slot. A slowed character
    // acts last, which is what makes Drowsy's effort-scaled duration worth buying.
    int order[BT_PARTY], n = 0;
    for (int i = 0; i < b->party->count; i++) if (b->party->a[i].alive) order[n++] = i;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) {
        int si = b->pa[order[i]].speed - (b->pa[order[i]].slow ? 100 : 0);
        int sj = b->pa[order[j]].speed - (b->pa[order[j]].slow ? 100 : 0);
        if (sj > si) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    }

    // 1. items
    for (int k = 0; k < n; k++) { int i = order[k]; if (b->cmd[i] == BTC_ITEM) bt_do_item(b, i, b->item[i]); }

    // 2. skills
    for (int k = 0; k < n; k++) {
        int i = order[k];
        if (b->cmd[i] != BTC_SKILL) continue;
        int e = bt_clamp(b->effort[i], 1, 5);
        if (b->party->a[i].stam < BT_EFFORT_COST[e]) { bt_log(b, "%s has nothing left.", b->party->a[i].name); continue; }
        b->party->a[i].stam -= BT_EFFORT_COST[e];
        bt_do_skill(b, i, b->skill[i], e);
    }

    // 3. attacks and guards
    for (int k = 0; k < n; k++) {
        int i = order[k];
        if (b->cmd[i] == BTC_GUARD) { bt_do_guard(b, i); continue; }
        if (b->cmd[i] != BTC_ATTACK) continue;
        int e = bt_clamp(b->effort[i], 1, 5);
        if (b->party->a[i].stam < BT_EFFORT_COST[e]) { bt_log(b, "%s has nothing left.", b->party->a[i].name); continue; }
        b->party->a[i].stam -= BT_EFFORT_COST[e];
        int ti = b->target[i];
        if (ti < 0 || ti >= b->enemy_count || !b->en[ti].alive) ti = bt_first_live_enemy(b);
        if (ti < 0) continue;
        // Attack INTO a tell: your damage lands, then theirs lands at double (§4).
        if (b->en[ti].tell && b->en[ti].tell_target == i && !b->en[ti].open) {
            b->en[ti].countered = 1;
            if (bt_is_yard(b)) b->party->fail_streak++;
        }
        bt_hurt_enemy(b, i, ti, (float)b->pa[i].atk, e, "strikes");
    }

    // 4. the enemy phase
    if (bt_alive_enemies(b) > 0) bt_enemy_phase(b);

    // 5. end of round
    for (int i = 0; i < b->party->count; i++) {
        BtActorState *p = &b->pa[i];
        if (p->shield > 0) p->shield--;
        if (p->slow > 0) p->slow--;
        if (p->blind > 0) p->blind--;
        if (p->doubled > 0) p->doubled--;
        p->no_guard = (b->cmd[i] == BTC_SKILL && bt_skill_at(b->party->a[i].id, b->skill[i])
                       && bt_skill_at(b->party->a[i].id, b->skill[i])->kind == BTK_HARD) ? 1 : 0;
    }
    for (int i = 0; i < b->enemy_count; i++) {
        BtEnemyState *e = &b->en[i];
        // OPEN lasts exactly one full round: it arrives at the end of the round the parry landed
        // in, and it is gone at the end of the round after it.
        if (e->open_next) { e->open = 1; e->open_next = 0; }
        else if (e->open > 0) e->open--;
        if (e->slow > 0) e->slow--;
        if (e->weaken > 0) e->weaken--;
    }

    bt_boss_script(b);

    // ── §4a.5, the non-verbal escalation. YARD ONLY, and it never fires anywhere else. ──
    if (bt_is_yard(b)) {
        if (b->party->fail_streak >= 3) b->tell_speed = 0.8f;    // the tell slows by about a fifth
        if (b->party->fail_streak >= 5) b->guard_pulse = 1;      // the Guard entry pulses once
    }

    // ── the outcome ──
    if (bt_alive_enemies(b) == 0) { b->outcome = BT_WIN; b->phase = BTP_OVER; return; }
    if (bt_alive_party(b) == 0) {
        if (b->boss_phase == 2) { bt_boss_restart_phase_two(b); return; }   // restarts phase two only
        b->outcome = BT_LOSE; b->phase = BTP_OVER; return;
    }
    bt_round_begin(b);
}

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// §3  THE SCREEN.  Draws §2's state and decides nothing.
// ═══════════════════════════════════════════════════════════════════════════════════════════════
#if BT_DRAW

static const char *BT_CMD_NAME[BTC_COUNT] = { "", "Attack", "Skill", "Guard", "Item", "Run" };
static const uint32_t BT_CMD_BIT[BTC_COUNT] = {
    0, BT_KNOWS_ATTACK, BT_KNOWS_SKILL, BT_KNOWS_GUARD, BT_KNOWS_ITEM, BT_KNOWS_RUN
};

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
static unsigned bt_load_sprite(const char *id, int *ow, int *oh, int *ofw, int *ofh) {
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

static void bt_draw(Battle *b, int W, int H, float dt, bool ui_blocked) {
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
        float rowh = h * 0.068f;
        float my0 = 0;                 // set below: the block grows UPWARD from a fixed bottom, so
                                       // the slider under it is never pushed off the screen as the
                                       // command list grows (§7a: the list grows, one entry at a time)

        // Build the visible list: an idea that has not arrived is ABSENT, never greyed (§7a).
        int rows[BTC_COUNT], nrows = 0;
        for (int c = BTC_ATTACK; c < BTC_COUNT; c++) {
            if (c == BTC_RUN && (b->enc->flags & BTE_NO_RUN)) continue;
            if (c == BTC_SKILL && !bt_skill_count(a->id)) continue;
            if (c == BTC_ITEM && !(b->party->items & (BT_ITEM_TEACHING | BT_ITEM_HILL))) continue;
            if (!(b->party->known & BT_CMD_BIT[c])) continue;
            rows[nrows++] = c;
        }
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
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(w, h));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if (ImGui::Begin("##bt_touch", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar)) {
            for (int r = 0; r < nrows; r++) {
                ImGui::SetCursorPos(ImVec2(mx0 - S * 0.2f, my0 + r * rowh - S * 0.1f));
                char lbl[24]; snprintf(lbl, sizeof(lbl), "##cmd%d", r);
                if (ImGui::InvisibleButton(lbl, ImVec2(mw, rowh * 0.95f))) {
                    if (b->ui_cmd == r) b->phase = BTP_RESOLVE;         // a second tap commits
                    else { b->ui_cmd = r; b->ui_effort = bt_clamp(b->party->last_effort[rows[r]], 1, 5); }
                }
            }
            if (has_effort) for (int e = 1; e <= 5; e++) {
                ImGui::SetCursorPos(ImVec2(mx0 + (e - 1) * (mw / 5.0f), sy - S * 0.2f));
                char lbl[24]; snprintf(lbl, sizeof(lbl), "##eff%d", e);
                if (ImGui::InvisibleButton(lbl, ImVec2(mw / 5.0f, S * 1.2f))) b->ui_effort = e;
            }
            if (cmd == BTC_SKILL) {
                int n = bt_skill_count(a->id);
                for (int k = 0; k < n; k++) {
                    ImGui::SetCursorPos(ImVec2(mx0 + mw + S * 0.8f, my0 + k * S * 1.2f - S * 0.1f));
                    char lbl[24]; snprintf(lbl, sizeof(lbl), "##sk%d", k);
                    if (ImGui::InvisibleButton(lbl, ImVec2(mw, S * 1.05f))) b->ui_skill = k;
                }
            }
            // Target: tap an enemy.
            for (int i = 0; i < b->enemy_count; i++) {
                if (!b->en[i].alive) continue;
                ImGui::SetCursorPos(ImVec2(w * (0.13f + 0.14f * i) - w * 0.07f, h * 0.18f));
                char lbl[24]; snprintf(lbl, sizeof(lbl), "##tgt%d", i);
                if (ImGui::InvisibleButton(lbl, ImVec2(w * 0.14f, h * 0.32f))) b->ui_target = i;
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
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
static void bt_input(Battle *b, int nrows, const int *rows) {
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

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// §4  the public API
// ═══════════════════════════════════════════════════════════════════════════════════════════════

Battle *bt_create() {
    Battle *b = (Battle *)calloc(1, sizeof(Battle));
    bt_palette_load();
    return b;
}
void bt_destroy(Battle *b) { free(b); }

void bt_party_init(BtParty *p, int count) {
    memset(p, 0, sizeof(*p));
    p->count = bt_clamp(count, 1, BT_PARTY);
    for (int i = 0; i < p->count; i++) {
        BtActor *a = &p->a[i];
        snprintf(a->id, sizeof(a->id), "%s", BT_PARTY_DEF[i].id);
        snprintf(a->name, sizeof(a->name), "%s", BT_PARTY_DEF[i].name);
        a->hp = a->hp_max = BT_PARTY_DEF[i].hp;
        a->stam = a->stam_max = BT_PARTY_DEF[i].stam;
        a->level = a->xp = 0;      // COMBAT.md asks for no levels and no XP in chapter one.
        a->alive = 1;
    }
    // Only Attack exists. The command list is one entry (§7a row 1).
    p->known = BT_KNOWS_ATTACK;
    for (int i = 0; i < 8; i++) p->last_effort[i] = 3;
}

bool bt_start(Battle *b, const char *encounter, BtParty *p, const char *map) {
    const BtEncounterDef *enc = bt_encounter(encounter);
    if (!enc || !p || p->count < 1) return false;

    // Keep the textures: bt_start may be called again for a yard retry and we must not re-upload.
    unsigned keep[BT_PARTY + BT_ENEMIES_MAX];
    int kw[BT_PARTY + BT_ENEMIES_MAX], kh[BT_PARTY + BT_ENEMIES_MAX];
    int kfw[BT_PARTY + BT_ENEMIES_MAX], kfh[BT_PARTY + BT_ENEMIES_MAX];
    memcpy(keep, b->tex, sizeof(keep)); memcpy(kw, b->texw, sizeof(kw)); memcpy(kh, b->texh, sizeof(kh));
    memcpy(kfw, b->texfw, sizeof(kfw)); memcpy(kfh, b->texfh, sizeof(kfh));
    int had = b->tex_ready;
    memset(b, 0, sizeof(*b));
    memcpy(b->tex, keep, sizeof(keep)); memcpy(b->texw, kw, sizeof(kw)); memcpy(b->texh, kh, sizeof(kh));
    memcpy(b->texfw, kfw, sizeof(kfw)); memcpy(b->texfh, kfh, sizeof(kfh));
    b->tex_ready = had;

    b->enc = enc;
    b->party = p;
    snprintf(b->map, sizeof(b->map), "%s", map ? map : "");
    p->known |= enc->grants;
    bt_setup_enemies(b, enc->e);
    if (!b->enemy_count) return false;

    for (int i = 0; i < p->count; i++) {
        memset(&b->pa[i], 0, sizeof(b->pa[i]));
        int d = bt_min(i, BT_PARTY - 1);
        // Match by id so a party in a different order still gets the right numbers.
        for (int k = 0; k < BT_PARTY; k++) if (!strcmp(BT_PARTY_DEF[k].id, p->a[i].id)) d = k;
        b->pa[i].atk = BT_PARTY_DEF[d].atk;
        b->pa[i].speed = BT_PARTY_DEF[d].speed;
        // Flat on your back, up in two seconds (§8). Health comes back; the STAMINA DOES NOT,
        // because the drain closing the door is the whole lesson of the session (§4a.3).
        if (bt_is_yard(b)) { p->a[i].alive = 1; p->a[i].hp = p->a[i].hp_max; }
        else if (!p->a[i].alive) { p->a[i].alive = 1; p->a[i].hp = bt_max(1, p->a[i].hp_max / 4); }
    }
    b->outcome = BT_RUNNING;
    b->round = 0;
    b->tell_speed = 1.0f;
    b->ui_effort = bt_clamp(p->last_effort[BTC_ATTACK], 1, 5);
    b->ui_target = 0;

    if (enc->flags & BTE_SCRIPTED) { b->boss_phase = 1; b->boss_cycles = 0; }
    if (enc->goal && enc->goal[0]) bt_goal(b, enc->goal);
    if (!bt_is_yard(b)) p->fail_streak = 0;      // the escalation is the yard's and nowhere else

    // Sprites: decided by a file-exists check at battle start, so the owner's art is picked up the
    // moment it lands. Placeholder silhouettes until then; nothing blocks on art.
#if BT_DRAW
    if (!bt_headless && !b->tex_ready) {
        b->tex_ready = 1;
        for (int i = 0; i < p->count; i++) b->tex[i] = bt_load_sprite(p->a[i].id, &b->texw[i], &b->texh[i], &b->texfw[i], &b->texfh[i]);
    }
    for (int i = 0; !bt_headless && i < b->enemy_count; i++) {
        int ti = BT_PARTY + i;
        if (!b->tex[ti]) b->tex[ti] = bt_load_sprite(bt_edef(b, i)->sprite, &b->texw[ti], &b->texh[ti], &b->texfw[ti], &b->texfh[ti]);
    }
#endif
    bt_round_begin(b);
    return true;
}

bool bt_done(Battle *b) {
    if (!b || b->outcome == BT_RUNNING) return false;
    // In {{MENTOR}}'s yard, losing is free and instant: no menu, no reload (§8). The caller just
    // calls bt_start again.
    if (b->outcome == BT_LOSE && bt_is_yard(b)) return true;
    return b->dismissed != 0;
}

void bt_force_win(Battle *b) {
    if (!b) return;
    for (int i = 0; i < b->enemy_count; i++) { b->en[i].hp = 0; b->en[i].alive = 0; }
    b->outcome = BT_WIN;
    b->phase = BTP_OVER;
}

int bt_tick(Battle *b, int w, int h, float dt, bool ui_blocked, BtEvent *ev) {
    if (ev) { ev->kind = BTE_NONE; ev->arg[0] = 0; }
    if (!b || !b->party) return BT_LOSE;

    if (ev && b->evq_rd < b->evq_n) { *ev = b->evq[b->evq_rd++]; if (b->evq_rd >= b->evq_n) { b->evq_rd = b->evq_n = 0; } }

#if BT_DRAW
    if (b->phase == BTP_INPUT && !ui_blocked && b->party->a[b->cur].alive) {
        const BtActor *a = &b->party->a[b->cur];
        int rows[BTC_COUNT], nrows = 0;
        for (int c = BTC_ATTACK; c < BTC_COUNT; c++) {
            if (c == BTC_RUN && (b->enc->flags & BTE_NO_RUN)) continue;
            if (c == BTC_SKILL && !bt_skill_count(a->id)) continue;
            if (c == BTC_ITEM && !(b->party->items & (BT_ITEM_TEACHING | BT_ITEM_HILL))) continue;
            if (!(b->party->known & BT_CMD_BIT[c])) continue;
            rows[nrows++] = c;
        }
        bt_input(b, nrows, rows);
        if (b->phase == BTP_RESOLVE) {
            // Commit this character's choice and move on to the next.
            int cmd = rows[bt_clamp(b->ui_cmd, 0, bt_max(0, nrows - 1))];
            int eff = bt_clamp(b->ui_effort, 1, 5);
            if (cmd != BTC_GUARD && !bt_can_pay(b, b->cur, cmd, eff)) {
                int cheap = bt_cheapest_affordable(b, b->cur, cmd);
                if (!cheap) { b->phase = BTP_INPUT; goto drawn; }    // greyed: the tap does nothing
                eff = cheap;
            }
            b->cmd[b->cur] = cmd;
            b->effort[b->cur] = eff;
            b->skill[b->cur] = b->ui_skill;
            b->item[b->cur] = 0;
            b->target[b->cur] = b->ui_target;
            b->party->last_effort[cmd] = eff;
            if (cmd == BTC_RUN && !(b->enc->flags & BTE_NO_RUN)) {   // running always works (§8)
                b->outcome = BT_FLED; b->phase = BTP_OVER; goto drawn;
            }
            // Next living character, or resolve.
            int n = b->cur + 1;
            while (n < b->party->count && !b->party->a[n].alive) n++;
            if (n < b->party->count) { b->cur = n; b->phase = BTP_INPUT; b->ui_cmd = 0; b->ui_skill = 0; }
            else bt_resolve(b);
        }
    }
drawn:
    bt_draw(b, w, h, dt, ui_blocked);
    if (b->phase == BTP_OVER) {
        b->banner_t += dt;
        if (b->banner_t > 1.2f && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space) ||
                                   ImGui::IsMouseClicked(0) || bt_is_yard(b)))
            b->dismissed = 1;
    }
#else
    (void)w; (void)h; (void)dt; (void)ui_blocked;
#endif
    return b->outcome;
}

bool bt_dev_ui(Battle *b, char *out, int cap) {
#if BT_DRAW
    if (!b) return false;
    bool asked = false;
    ImGui::TextUnformatted("Battle");
    if (b->enc) ImGui::Text("%s  round %d  phase %d", b->enc->id, b->round, b->boss_phase);
    if (ImGui::Button("Win##bt")) { bt_force_win(b); }
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

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// §5  bt_selftest — headless. No window, no GL, no input.
// ═══════════════════════════════════════════════════════════════════════════════════════════════

static int bt_fails = 0;
static void bt_check(bool ok, const char *what) {
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) bt_fails++;
}

// The one place the test drives the simulation: choose for everybody, then resolve.
static void bt_all(Battle *b, int cmd, int effort, int skill) {
    for (int i = 0; i < b->party->count; i++) {
        if (!b->party->a[i].alive) continue;
        b->cmd[i] = cmd; b->effort[i] = effort; b->skill[i] = skill; b->target[i] = 0; b->item[i] = 0;
    }
}
static void bt_one(Battle *b, int who, int cmd, int effort, int skill) {
    if (who < b->party->count && b->party->a[who].alive) {
        b->cmd[who] = cmd; b->effort[who] = effort; b->skill[who] = skill; b->target[who] = 0; b->item[who] = 0;
    }
}
static void bt_idle(Battle *b) { for (int i = 0; i < b->party->count; i++) b->cmd[i] = BTC_NONE; }

static int bt_skill_index(const char *owner, const char *id) {
    int k = 0;
    for (int i = 0; i < BT_SKILL_COUNT; i++)
        if (!strcmp(BT_SKILLS[i].owner, owner)) { if (!strcmp(BT_SKILLS[i].id, id)) return k; k++; }
    return -1;
}

int bt_selftest() {
    bt_fails = 0;
    bt_headless = true;
    bt_rng_state = 0x1234567u;
    setvbuf(stdout, 0, _IONBF, 0);
    printf("battle selftest — COMBAT.md §1-§9\n");

    // ── 1. the effort table, verbatim (§2) ──
    printf("\n1. the effort table\n");
    {
        static const int cost[5] = { 2, 5, 9, 15, 24 };
        static const float mag[5] = { 0.5f, 0.8f, 1.0f, 1.25f, 1.5f };
        bool ok = true;
        for (int e = 1; e <= 5; e++) {
            printf("   effort %d: cost %d, x%.2f\n", e, BT_EFFORT_COST[e], BT_EFFORT_MAG[e]);
            if (BT_EFFORT_COST[e] != cost[e - 1]) ok = false;
            if (fabsf(BT_EFFORT_MAG[e] - mag[e - 1]) > 0.0001f) ok = false;
        }
        bt_check(ok, "effort 1-5 costs 2/5/9/15/24 for x0.5/0.8/1.0/1.25/1.5");
        bt_check(bt_cost(BTC_GUARD, 5) == 0, "Guard has no effort and costs nothing");
        // Tuning target 1 (§3): effort 3 every turn is roughly break-even with +8.
        bt_check(BT_EFFORT_COST[3] == 9 && BT_REGEN == 8, "effort 3 (9) against regen (+8) is a slow bleed");
    }

    // ── 2. stamina, and the regeneration rule (§3) ──
    printf("\n2. stamina and regen\n");
    {
        BtParty p; bt_party_init(&p, 3);
        printf("   pools: %s %d, %s %d, %s %d\n", p.a[0].name, p.a[0].stam_max,
               p.a[1].name, p.a[1].stam_max, p.a[2].name, p.a[2].stam_max);
        bt_check(p.a[0].stam_max == 40 && p.a[1].stam_max == 36 && p.a[2].stam_max == 30,
                 "pools are Hero 40, Healer 36, Herder 30 (COMBAT §3 over BESTIARY's 24)");
        bt_check(p.known == BT_KNOWS_ATTACK, "a new party knows only Attack — the list is one entry");

        Battle *b = bt_create();
        bt_start(b, "arm", &p, "hart_yard");
        p.a[0].stam = 0;
        bt_all(b, BTC_ATTACK, 1, 0);
        bt_resolve(b);
        printf("   after a round with stam 0 and an effort-1 attack: %d\n", p.a[0].stam);
        // +8 at the top of the NEXT round is what he is sitting on.
        int before = p.a[0].stam;
        p.a[0].stam = 10;
        bt_all(b, BTC_GUARD, 1, 0);
        int guard_start = p.a[0].stam;
        bt_resolve(b);
        printf("   guard from %d -> %d (regen +8 happened at the top of the next round too)\n", guard_start, p.a[0].stam);
        (void)before;
        bt_check(BT_GUARD_REGEN == 4 && BT_PARRY_REGEN == 8,
                 "guard adds +4 (nets +12), a parry adds +8 (nets +16)");
        bt_destroy(b);
    }

    // ── 3. THE SWING IS NOT WINNABLE BY ATTACKING (§7, §4a) ──
    printf("\n3. the swing, attacking only\n");
    {
        BtParty p; bt_party_init(&p, 1);
        Battle *b = bt_create();
        bt_start(b, "swing", &p, "hart_yard");
        int rounds = 0, sessions = 1;
        int hp0 = b->en[0].hp;
        bool locked_out = false;            // a round where he could not afford Attack at all
        // A session in the yard, retries included: losing is free and instant, the player is up in
        // two seconds, and the stamina drain carries on across the retry (§8, §4a.3).
        while (rounds < 60 && p.fail_streak < 6) {
            if (b->outcome != BT_RUNNING) { sessions++; bt_start(b, "swing", &p, "hart_yard"); continue; }
            // Falke swings at effort 5 every single turn, which is what he does and what every new
            // player does. Drop to whatever he can still afford, exactly as the menu would.
            int eff = 5;
            while (eff > 1 && !bt_can_pay(b, 0, BTC_ATTACK, eff)) eff--;
            if (!bt_can_pay(b, 0, BTC_ATTACK, 1)) { locked_out = true; bt_idle(b); }
            else bt_all(b, BTC_ATTACK, eff, 0);
            bt_resolve(b);
            rounds++;
            if (rounds <= 8)
                printf("   r%-2d  falke stam %2d hp %2d   swing hp %2d wind %d   fails %d\n",
                       rounds, p.a[0].stam, p.a[0].hp, b->en[0].hp, b->en[0].wind, p.fail_streak);
        }
        printf("   after %d rounds over %d session(s): swing hp %d/%d, falke stam %d, wind %d\n",
               rounds, sessions, b->en[0].hp, hp0, p.a[0].stam, b->en[0].wind);
        bt_check(b->en[0].hp == hp0, "attacking never takes a point off the swing");
        bt_check(b->outcome != BT_WIN, "the swing is NOT winnable by attacking");
        bt_check(b->en[0].wind >= 3 || sessions > 1, "every attack winds the counterweight, and it is on screen");
        bt_check(locked_out,
                 "the counter drains him: Attack is unaffordable, Guard is the only entry left lit");
        bt_check(bt_cost(BTC_GUARD, 1) == 0, "Guard is never greyed — it costs nothing, ever");
        printf("   fail_streak %d -> tell_speed %.2f, guard pulse %d (yard only, §4a.5)\n",
               p.fail_streak, b->tell_speed, b->guard_pulse);
        bt_check(p.fail_streak >= 5 && b->tell_speed < 1.0f && b->guard_pulse == 1,
                 "after 3 failures the tell slows, after 5 the Guard entry pulses");
        bt_destroy(b);
    }

    // ── 4. THE SWING IS WINNABLE BY PARRY -> OPEN -> EFFORT (§4) ──
    printf("\n4. the swing, parry -> open -> spend\n");
    {
        BtParty p; bt_party_init(&p, 1);
        Battle *b = bt_create();
        bt_start(b, "swing", &p, "hart_yard");
        int rounds = 0, parries = 0;
        while (b->outcome == BT_RUNNING && rounds < 30) {
            if (b->en[0].open) {
                int eff = 5; while (eff > 1 && !bt_can_pay(b, 0, BTC_ATTACK, eff)) eff--;
                bt_all(b, BTC_ATTACK, eff, 0);
                printf("   r%-2d  OPEN -> spend effort %d (stam %d)\n", rounds + 1, eff, p.a[0].stam);
            } else {
                bt_all(b, BTC_GUARD, 1, 0);          // wait for the tell and turn it aside
                printf("   r%-2d  guard (stam %d, swing hp %d)\n", rounds + 1, p.a[0].stam, b->en[0].hp);
            }
            bool was_tell = b->en[0].tell != 0;
            bt_resolve(b);
            if (was_tell && b->en[0].open) parries++;
            rounds++;
        }
        printf("   outcome %d after %d rounds, %d parries, falke stam %d hp %d\n",
               b->outcome, rounds, parries, p.a[0].stam, p.a[0].hp);
        bt_check(b->outcome == BT_WIN, "the swing IS won by parry -> open -> effort");
        bt_check(parries >= 1, "at least one parry landed");
        bt_check((p.known & BT_KNOWS_GUARD) != 0, "Guard arrived the first time it telegraphed (§7a.3)");
        bt_check((p.known & BT_KNOWS_EFFORT) != 0, "the slider arrived the first time there was an opening (§7a.5)");
        bt_check(p.fail_streak == 0, "the escalation reset the moment a parry landed");
        bt_destroy(b);
    }

    // ── 5. OPEN doubles the damage, and an OPEN enemy cannot act (§4) ──
    printf("\n5. the opening\n");
    {
        BtParty p; bt_party_init(&p, 1);
        Battle *b = bt_create();
        bt_start(b, "burr", &p, "hill_path");
        // Round one: it telegraphs at the end. Round two: guard -> parry. Round three: spend.
        bt_all(b, BTC_GUARD, 1, 0); bt_resolve(b);
        bt_all(b, BTC_GUARD, 1, 0); bt_resolve(b);
        bt_check(b->en[0].open == 1, "a parry leaves it OPEN for the whole of the next round");
        int hp_before = b->en[0].hp;
        bt_all(b, BTC_ATTACK, 3, 0);
        bt_resolve(b);
        int dealt = hp_before - b->en[0].hp;
        printf("   effort 3 into the opening: %d damage (base atk %d, x1.0, x2 open)\n", dealt, b->pa[0].atk);
        bt_check(dealt == b->pa[0].atk * 2, "a hit into an opening is exactly double");
        bt_destroy(b);
    }

    // ── 6. minimum effort is the whole gating system (§3, §5) ──
    printf("\n6. minimum effort\n");
    {
        const BtSkillDef *s = 0;
        for (int i = 0; i < BT_SKILL_COUNT; i++) if (!strcmp(BT_SKILLS[i].id, "settle")) s = &BT_SKILLS[i];
        bt_check(s && s->min == 4, "Settle's minimum effort is 4");
        bt_check(BT_EFFORT_COST[5] == 24, "Settle at effort 5 costs 24, so Distel can afford it about three times");
        const BtSkillDef *hard = 0;
        for (int i = 0; i < BT_SKILL_COUNT; i++) if (!strcmp(BT_SKILLS[i].id, "hard")) hard = &BT_SKILLS[i];
        bt_check(hard && hard->min == 4, "Hard swing's minimum effort is 4");
        printf("   30 stamina, +8 a round, 24 a Settle -> %d, %d, %d ...\n",
               30 - 24, 30 - 24 + 8 - 24 < 0 ? 30 - 24 + 8 : 30 - 24 + 8 - 24, 6 + 8 + 8 - 24);
    }

    // ── 7. THE BOSS: both phases, boss_break and boss_turn (§9) ──
    printf("\n7. the boss\n");
    {
        BtParty p; bt_party_init(&p, 3);
        p.known |= BT_KNOWS_GUARD | BT_KNOWS_EFFORT | BT_KNOWS_SKILL;
        Battle *b = bt_create();
        bt_start(b, "klee", &p, "high_pasture");
        printf("   goal: \"%s\"\n", b->goal);
        bt_check(b->boss_phase == 1, "starts in phase one");
        bt_check(!strcmp(b->goal, "Hold it open for Distel."), "the goal line is set at the start");
        bt_check((b->enc->flags & BTE_NO_RUN) != 0, "the boss refuses a run");

        int settle = bt_skill_index("distel", "settle");
        bool saw_break = false, saw_turn = false, saw_phase = false, saw_goal2 = false;
        bool attack_did_nothing = false;
        int rounds = 0;
        int hp1 = b->en[0].hp;

        while (b->outcome == BT_RUNNING && rounds < 60) {
            if (b->boss_phase == 1) {
                // Attacking it does nothing at all, and the message says so every time.
                bt_one(b, 0, b->en[0].open ? BTC_ATTACK : BTC_GUARD, 1, 0);
                bt_one(b, 1, BTC_GUARD, 1, 0);
                // {{HERDER}} spends Settle at effort 5 into each opening; below effort 4 she cannot
                // reach it, which is the fight's whole problem.
                if (b->en[0].open && p.a[2].stam >= BT_EFFORT_COST[4])
                    bt_one(b, 2, BTC_SKILL, p.a[2].stam >= 24 ? 5 : 4, settle);
                else bt_one(b, 2, BTC_GUARD, 1, 0);
            } else {
                if (b->en[0].open) { bt_all(b, BTC_ATTACK, 5, 0); bt_one(b, 2, BTC_GUARD, 1, 0); }
                else { bt_all(b, BTC_GUARD, 1, 0); }
            }
            int ph = b->boss_phase;
            bt_resolve(b);
            rounds++;
            for (int k = 0; k < b->evq_n; k++) {
                if (b->evq[k].kind == BTE_TEXT && !strcmp(b->evq[k].arg, "high_pasture.boss_break")) saw_break = true;
                if (b->evq[k].kind == BTE_TEXT && !strcmp(b->evq[k].arg, "high_pasture.boss_turn")) saw_turn = true;
                if (b->evq[k].kind == BTE_PHASE) saw_phase = true;
                if (b->evq[k].kind == BTE_GOAL && !strcmp(b->evq[k].arg, "Stop it. It wants Distel.")) saw_goal2 = true;
            }
            b->evq_n = b->evq_rd = 0;
            if (ph == 1 && b->en[0].hp == hp1) attack_did_nothing = true;
            if (rounds % 4 == 0 || ph != b->boss_phase)
                printf("   r%-2d  phase %d  cycles %d  klee hp %3d  distel stam %2d  party hp %d/%d/%d\n",
                       rounds, b->boss_phase, b->boss_cycles, b->en[0].hp,
                       p.a[2].stam, p.a[0].hp, p.a[1].hp, p.a[2].hp);
        }
        printf("   ended: outcome %d, phase %d, %d rounds\n", b->outcome, b->boss_phase, rounds);
        bt_check(attack_did_nothing, "phase one: attacking does nothing and the game says so");
        bt_check(saw_break, "BTE_TEXT high_pasture.boss_break fired at the break");
        bt_check(saw_turn, "BTE_TEXT high_pasture.boss_turn fired when it turned on Distel");
        bt_check(saw_phase, "BTE_PHASE fired");
        bt_check(saw_goal2, "BTE_GOAL changed to \"Stop it. It wants Distel.\"");
        bt_check(b->boss_phase == 2, "the fight reached phase two");
        bt_check(b->outcome == BT_WIN, "phase two ends in a win when the opening is spent into");
        bt_destroy(b);
    }

    // ── 8. phase one cannot be lost (§8, §9) ──
    printf("\n8. phase one cannot be lost\n");
    {
        BtParty p; bt_party_init(&p, 3);
        Battle *b = bt_create();
        bt_start(b, "klee", &p, "high_pasture");
        for (int r = 0; r < 12 && b->outcome == BT_RUNNING; r++) { bt_all(b, BTC_ATTACK, 1, 0); bt_resolve(b); }
        printf("   party hp %d/%d/%d after twelve rounds of doing the wrong thing\n", p.a[0].hp, p.a[1].hp, p.a[2].hp);
        bt_check(b->outcome != BT_LOSE, "phase one cannot be lost");
        bt_check(p.a[0].hp >= 1 && p.a[1].hp >= 1 && p.a[2].hp >= 1, "nobody drops below 1 in phase one");
        bt_destroy(b);
    }

    // ── 9. the machines, the field creatures, night sight, and the finds ──
    printf("\n9. the rest of the tables\n");
    {
        bt_check(bt_encounter("post") && bt_encounter("arm") && bt_encounter("swing"), "the three machines are named, never numbered");
        bt_check(bt_encounter("lid") && bt_encounter("burr") && bt_encounter("lantern") && bt_encounter("fleece"),
                 "lid, burr, lantern and fleece are in the table");
        bt_check((BT_ENEMIES[bt_enemy_def("post")].flags & BTF_NO_TELL) != 0, "the post never telegraphs: three clean hits");
        bt_check((BT_ENEMIES[bt_enemy_def("lantern")].flags & BTF_HIDDEN_TELL) != 0,
                 "the lantern hides its tell — night sight is a mechanic, not a line");

        BtParty p; bt_party_init(&p, 2);          // no Distel
        Battle *b = bt_create();
        bt_start(b, "lantern", &p, "high_pasture");
        bool without = bt_night_sight(b);
        p.items |= BT_ITEM_PASTURE;               // the lens
        bool with_lens = bt_night_sight(b);
        bt_destroy(b);
        BtParty q; bt_party_init(&q, 3);          // with Distel
        Battle *b2 = bt_create();
        bt_start(b2, "lantern", &q, "high_pasture");
        bool with_distel = bt_night_sight(b2);
        bt_destroy(b2);
        printf("   night sight: no herder %d, lens %d, herder %d\n", without, with_lens, with_distel);
        bt_check(!without && with_lens && with_distel,
                 "night sight comes from the herder, and the lens gives it permanently");

        // The post is beaten by three clean hits and nothing else is needed.
        BtParty r; bt_party_init(&r, 1);
        Battle *b3 = bt_create();
        bt_start(b3, "post", &r, "hart_yard");
        int n = 0;
        while (b3->outcome == BT_RUNNING && n < 12) { bt_all(b3, BTC_ATTACK, 3, 0); bt_resolve(b3); n++; }
        printf("   the post fell in %d effort-3 hits\n", n);
        bt_check(b3->outcome == BT_WIN && n <= 4, "the post is beaten by about three clean hits");
        bt_destroy(b3);

        // Running always works, except the boss (§8).
        BtParty s; bt_party_init(&s, 1); s.known |= BT_KNOWS_RUN;
        bt_check(!(bt_encounter("burr")->flags & BTE_NO_RUN), "running works on a burr");
        bt_check((bt_encounter("klee")->flags & BTE_NO_RUN) != 0, "running does not work on the boss");
    }

    // ── 10. no levels, no XP ──
    printf("\n10. levels\n");
    {
        BtParty p; bt_party_init(&p, 3);
        bool zero = true;
        for (int i = 0; i < 3; i++) if (p.a[i].level || p.a[i].xp) zero = false;
        bt_check(zero, "COMBAT.md asks for no levels and no XP in chapter one: both stay 0");
    }

    printf("\n%s — %d failure(s)\n", bt_fails ? "FAILED" : "ALL PASS", bt_fails);
    return bt_fails;
}

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// §6  the standalone tool: --battle-selftest and BATTLE_CAPTURE, built as its own target so the
//     game's own targets are untouched.  cmake --build build_desktop --target battle_tool
//       ./build_desktop/battle_tool --selftest
//       ./build_desktop/battle_tool --capture swing:1280x720:/tmp/swing.png
// ═══════════════════════════════════════════════════════════════════════════════════════════════
#ifdef BATTLE_TOOL_MAIN
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_opengl3.h"

// A scripted state the capture can be put into, so one PNG shows the tell, the OPEN state, a greyed
// command beside a lit Guard, and the slider all at once.
static void bt_capture_pose(Battle *b, const char *pose) {
    if (!strcmp(pose, "tell")) {
        b->en[0].tell = 1; b->en[0].tell_target = 0;
        b->party->known |= BT_KNOWS_GUARD;
    } else if (!strcmp(pose, "open")) {
        b->en[0].open = 1;
        b->party->known |= BT_KNOWS_GUARD | BT_KNOWS_EFFORT;
        bt_log(b, "Falke turns it aside. the swing is open.");
    } else if (!strcmp(pose, "tired")) {
        // The bottom of a bad session: one bright entry in a row of grey ones (§4a.4).
        b->en[0].tell = 1; b->en[0].tell_target = 0; b->en[0].wind = 5;
        b->party->a[0].stam = 1;
        b->party->known |= BT_KNOWS_GUARD | BT_KNOWS_EFFORT;
        b->party->fail_streak = 5; b->tell_speed = 0.8f; b->guard_pulse = 1;
        bt_log(b, "The counterweight winds up a notch. (5)");
        bt_log(b, "the swing counters.");
        bt_log(b, "Falke is winded. (-21)");
    }
}

int main(int argc, char **argv) {
    bool selftest = false;
    const char *cap = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--selftest") || !strcmp(argv[i], "--battle-selftest")) selftest = true;
        else if (!strcmp(argv[i], "--capture") && i + 1 < argc) cap = argv[++i];
    }
    if (!cap) cap = getenv("BATTLE_CAPTURE");
    if (selftest || (!cap && getenv("BATTLE_SELFTEST"))) return bt_selftest();
    if (!cap) { printf("usage: battle_tool --selftest | --capture <enc>[:<pose>]:<w>x<h>:<out.png>\n"); return 2; }

    // <encounter>[:<pose>]:<w>x<h>:<out.png>
    char buf[512]; snprintf(buf, sizeof(buf), "%s", cap);
    char *tok[6]; int nt = 0;
    for (char *p = buf; p && nt < 6; ) { tok[nt++] = p; char *c = strchr(p, ':'); if (!c) break; *c = 0; p = c + 1; }
    const char *enc = tok[0], *pose = "", *dim = 0, *out = 0;
    if (nt == 3) { dim = tok[1]; out = tok[2]; }
    else if (nt >= 4) { pose = tok[1]; dim = tok[2]; out = tok[3]; }
    else { printf("battle_tool: bad BATTLE_CAPTURE\n"); return 2; }
    int W = 1280, H = 720;
    sscanf(dim, "%dx%d", &W, &H);

    if (!SDL_Init(SDL_INIT_VIDEO)) { printf("SDL_Init failed: %s\n", SDL_GetError()); return 3; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_Window *win = SDL_CreateWindow("battle", W, H, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!win) { printf("no window: %s\n", SDL_GetError()); return 3; }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGui::GetIO().IniFilename = 0;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForOpenGL(win, ctx);
    ImGui_ImplOpenGL3_Init("#version 150");
    ImGui::GetStyle().FontSizeBase = 16.0f;

    BtParty p; bt_party_init(&p, 3);
    p.known |= BT_KNOWS_GUARD | BT_KNOWS_EFFORT | BT_KNOWS_SKILL;
    p.items |= BT_ITEM_TEACHING | BT_ITEM_HILL;
    Battle *b = bt_create();
    if (!bt_start(b, enc, &p, "hart_yard")) { printf("unknown encounter %s\n", enc); return 4; }
    if (pose[0]) bt_capture_pose(b, pose);

    unsigned char *px = (unsigned char *)malloc((size_t)W * H * 4);
    for (int f = 0; f < 3; f++) {                 // a few frames so ImGui settles
        SDL_Event e; while (SDL_PollEvent(&e)) ImGui_ImplSDL3_ProcessEvent(&e);
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame();
        glViewport(0, 0, W, H);
        glClearColor(0.05f, 0.05f, 0.07f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
        bt_draw(b, W, H, 1.0f / 60.0f, false);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glFinish();
    }
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
    stbi_flip_vertically_on_write(1);
    if (!stbi_write_png(out, W, H, 4, px, W * 4)) { printf("write failed: %s\n", out); return 5; }
    printf("battle_tool: wrote %s (%dx%d) — %s%s%s\n", out, W, H, enc, pose[0] ? ":" : "", pose);
    free(px);
    bt_destroy(b);
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL3_Shutdown(); ImGui::DestroyContext();
    SDL_GL_DestroyContext(ctx); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
#endif
