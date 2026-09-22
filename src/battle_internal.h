// battle_internal.h — what the four battle_*.cpp files share, and nothing a caller may touch.
//
//   owns      the battle's private types (Battle, BtEnemyState, BtActorState, the table structs),
//             the §1 table declarations, and the cross-file declarations of the simulation.
//   never     declares anything the game outside the battle screen may call — that is battle.h.
//   exposed   to battle_rules.cpp, battle_ui.cpp, battle_script.cpp and battle_test.cpp only.
//
// The file-by-file shape, and the contract (story/v3/COMBAT.md §1-§9 is the design contract):
//   battle_rules.cpp   §0 palette + helpers, §1 the tables, §2 the simulation, §4 the public API
//   battle_ui.cpp      §3 the screen and the bt_ui_* accessors. Draws §2's state, decides nothing.
//   battle_script.cpp  §9 the boss's phases and its mid-fight text
//   battle_test.cpp    §5 bt_selftest and §6 the standalone battle_tool
#pragma once

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

// ── `battle:` logging. One tag, so a phone log tells the whole story of a fight. ───────────────
#if BT_DRAW
#define BT_LOGF(...) SDL_Log(__VA_ARGS__)
#else
#define BT_LOGF(...) do { printf("battle-log: " __VA_ARGS__); printf("\n"); } while (0)
#endif

static int bt_min(int a, int b) { return a < b ? a : b; }
static int bt_max(int a, int b) { return a > b ? a : b; }
static int bt_clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// HEADLESS. bt_selftest sets this, and it is the switch that keeps the simulation free of GL: no
// texture is created, no file is read, nothing touches a context that does not exist.
extern bool bt_headless;
// A tiny deterministic RNG, so the selftest is reproducible and no fight depends on rand().
extern uint32_t bt_rng_state;
uint32_t bt_rand();

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

extern uint32_t BT_PAL[BT_PAL_N];
extern bool BT_PAL_OK;
uint32_t bt_pal(int i, uint32_t fallback);
void bt_palette_load();
#if BT_DRAW
const char *bt_pref();
void *bt_read(const char *rel, size_t *size);
#endif

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// §1  THE TABLES.   Later chapters add ROWS here, not code anywhere else.
// ═══════════════════════════════════════════════════════════════════════════════════════════════

// ── effort (COMBAT.md §2), the table verbatim ──────────────────────────────────────────────────
//   effort  1     2     3     4     5
//   cost    2     5     9     15    24
//   mag    x0.5  x0.8  x1.0  x1.25 x1.5
// Guard has NO effort and always costs 0 (§1). bt_selftest asserts both rows.
extern const int   BT_EFFORT_COST[6];
extern const float BT_EFFORT_MAG [6];

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
extern const BtEnemyDef BT_ENEMIES[];
extern const int BT_ENEMY_COUNT;

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
extern const BtSkillDef BT_SKILLS[];
extern const int BT_SKILL_COUNT;

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
extern const BtEncounterDef BT_ENCOUNTERS[];
extern const int BT_ENCOUNTER_COUNT;

// ── the party's starting values (COMBAT.md §3) ─────────────────────────────────────────────────
// CONTRADICTION, and the choice: COMBAT.md §3 gives {{HEALER}} a pool of 36; BESTIARY.md's boss
// section says 24. COMBAT.md §3 is the stamina section and the named contract, and §5 repeats 36
// ("ordinary pool (36) and the ordinary effort rules"), so 36 wins. BESTIARY's 24 reads as a stale
// designer placeholder from before "{{HEALER}} is not limited and never was".
struct BtPartyDef { const char *id, *name; int hp, stam, speed, atk; };
extern const BtPartyDef BT_PARTY_DEF[BT_PARTY];

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
static_assert(BTC_COUNT == BT_CMD_MAX && BTC_ATTACK == BT_CMD_FIRST, "battle.h's command ids must match BtCmd");

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
extern const char *BT_PHASE_NAME[];
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

    // ── the WATCHDOG (owner bug, 2026-09-21: "stuck after attacking" on the phone) ──────────────
    // A soft lock must never trap the player. Anything that is not an input-accepting state is a
    // PRESENTATION state, and a presentation state that has run for BT_STUCK_S seconds is a bug:
    // we say so loudly with the state's name and force it forward. `stuck_fires` is counted so the
    // UI test and the Dev panel can assert it stayed at zero.
    // Where the menu actually IS, in ImGui coordinates, recorded by the last bt_draw. The UI test
    // taps these rather than recomputing the layout, so the test can never drift from the screen.
    float ui_mx0, ui_my0, ui_mw, ui_rowh, ui_sy, ui_S, ui_U;
    int ui_nrows, ui_rows[BTC_COUNT], ui_has_effort;
    int last_phase, last_cur;
    float phase_t;              // seconds in the current (phase, cur) pair
    int stuck_fires;
    int aborted;                // the Dev panel's "abort battle"
};

#define BT_STUCK_S 5.0f

extern const char *BT_CMD_NAME[BTC_COUNT];
extern const uint32_t BT_CMD_BIT[BTC_COUNT];

// ── the simulation, across the four files ──────────────────────────────────────────────────────
void bt_log(Battle *b, const char *fmt, ...);
void bt_emit(Battle *b, int kind, const char *arg);
void bt_goal(Battle *b, const char *g);
int bt_enemy_def(const char *id);
const BtEncounterDef *bt_encounter(const char *id);
const BtEnemyDef *bt_edef(Battle *b, int i);
bool bt_is_yard(Battle *b);
bool bt_night_sight(Battle *b);
bool bt_tell_visible(Battle *b, int i, int viewer);
int bt_alive_enemies(Battle *b);
int bt_alive_party(Battle *b);
int bt_first_live_enemy(Battle *b);
int bt_cost(int cmd, int effort);
bool bt_can_pay(Battle *b, int who, int cmd, int effort);
int bt_cheapest_affordable(Battle *b, int who, int cmd);
const BtSkillDef *bt_skill_at(const char *owner, int n);
int bt_skill_count(const char *owner);
int bt_build_rows(Battle *b, int who, int *rows);
void bt_setup_enemies(Battle *b, const char *const *ids);
void bt_round_begin(Battle *b);
void bt_hurt_enemy(Battle *b, int who, int ei, float base, int effort, const char *verb);
void bt_hurt_actor(Battle *b, int who, int amount, const char *src);
void bt_do_skill(Battle *b, int who, int si, int effort);
void bt_do_guard(Battle *b, int who);
void bt_do_item(Battle *b, int who, int which);
void bt_enemy_phase(Battle *b);
void bt_resolve(Battle *b);

// ── battle_script.cpp: the boss (§9) ───────────────────────────────────────────────────────────
void bt_boss_script(Battle *b);
void bt_boss_restart_phase_two(Battle *b);

#if BT_DRAW
// ── battle_ui.cpp: the screen (§3) ─────────────────────────────────────────────────────────────
void bt_draw(Battle *b, int W, int H, float dt, bool ui_blocked);
void bt_input(Battle *b, int nrows, const int *rows);
unsigned bt_load_sprite(const char *id, int *ow, int *oh, int *ofw, int *ofh);
#endif
