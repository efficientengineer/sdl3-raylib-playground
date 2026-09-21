// battle.h — the turn-based battle screen. story/v3/COMBAT.md §1-§9 is the contract.
//
// Owner, 2026-09-21: real JRPG battles, not the timed-interact fallback. The field hands over on a
// `fight <encounter>` trigger or by script; the battle runs itself, draws itself, and hands back an
// outcome. Everything it needs to persist between fights lives in BtParty, which the CHAPTER owns
// (src/chapter01.h) so there is exactly one place that is saved and hot-reloaded.
//
// The one rule the whole chapter is built on: you cannot win by attacking. You wait for the tell,
// you Guard on it (a parry), the enemy is OPEN, and you spend effort into the opening.
#pragma once
#include <stdint.h>

struct Battle;

#define BT_PARTY 3
#define BT_NAME 16

// ── what an idea's arrival looks like (COMBAT.md §7a, the teaching order) ─────────────────────
// A command the player has not met is ABSENT from the menu, never greyed. Greying is only ever
// "you cannot afford this right now", and it always shows the cost. Guard is never greyed.
#define BT_KNOWS_ATTACK 0x1u
#define BT_KNOWS_GUARD  0x2u          // arrives the first time something telegraphs at you
#define BT_KNOWS_EFFORT 0x4u          // arrives the first time there is an opening to spend into
#define BT_KNOWS_SKILL  0x8u
#define BT_KNOWS_ITEM   0x10u
#define BT_KNOWS_RUN    0x20u

// The four hidden finds, whose effects last (chapter01.md). Bits match the chapter's item flags.
#define BT_ITEM_YARD      0x1u
#define BT_ITEM_TEACHING  0x2u
#define BT_ITEM_HILL      0x4u
#define BT_ITEM_PASTURE   0x8u

struct BtActor {
    char id[BT_NAME];                 // "falke", "ottilie", "distel" — also the sprite/portrait id
    char name[BT_NAME];               // the drawn name
    int32_t hp, hp_max;
    int32_t stam, stam_max;           // COMBAT.md §3 pools: Hero 40, Healer 36, Herder 30
    int32_t level, xp;                // only if COMBAT.md asks for them; 0 and unused otherwise
    int32_t alive;
};

// Persisted across battles and across a hot reload. POD.
struct BtParty {
    int32_t count;                    // 1..BT_PARTY
    BtActor a[BT_PARTY];
    uint32_t known;                   // BT_KNOWS_*: which ideas have arrived
    uint32_t items;                   // BT_ITEM_*: the hidden finds the player is carrying
    int32_t last_effort[8];           // the effort slider remembers its last setting per command
    int32_t fail_streak;              // non-verbal escalation (§4a item 5); yard only; resets on a parry
};

enum BtOutcome {
    BT_RUNNING = 0,
    BT_WIN,
    BT_LOSE,                          // in the yard this means "instant retry", not a game over
    BT_FLED,
};

enum BtEventKind {
    BTE_NONE = 0,
    BTE_TEXT,                         // ev.arg is a story/field/text.md id to show as a mid-fight box
                                      // (high_pasture.boss_break, high_pasture.boss_turn)
    BTE_PHASE,                        // ev.arg is a phase name; the boss changed phase
    BTE_GOAL,                         // ev.arg is a new goal line (the boss changes it mid-fight)
};

struct BtEvent { int32_t kind; char arg[64]; };

Battle *bt_create();
void bt_destroy(Battle *b);

// Begin an encounter. `encounter` is an id from the data table in battle.cpp: the three training
// machines `post`, `arm`, `swing`; `lid`, `burr`, `lantern`, `fleece`; and the boss `klee`.
// `map` is the field map it was started from, used only to pick a backdrop from that map's palette.
// Returns false for an unknown encounter id (the caller should carry on as if nothing happened).
bool bt_start(Battle *b, const char *encounter, BtParty *p, const char *map);

// Runs and draws one frame. Returns a BtOutcome; BT_RUNNING until the battle is over. The party's
// hp/stamina/known/fail_streak are written back into the BtParty handed to bt_start as it goes.
int bt_tick(Battle *b, int w, int h, float dt, bool ui_blocked, BtEvent *ev);

// True once the outcome has been returned AND the results screen has been dismissed, i.e. the
// caller may return to the field.
bool bt_done(Battle *b);

// ── what --battle-ui-test drives the real screen with (star_logic.cpp) ─────────────────────────
// Read-only views of the last drawn frame, so the test taps where the menu actually is.
int bt_ui_phase(Battle *b);                  // 0 INPUT, 1 RESOLVE, 2 ENEMY, 3 OVER
// The command ids, so a caller can size an array and name what it tapped. battle.cpp's private
// BtCmd enum is these numbers; bt_cmd_name() is the only place the words live.
#define BT_CMD_FIRST 1
#define BT_CMD_MAX   6
int bt_ui_rows(Battle *b, int *rows);        // the visible commands; returns how many
int bt_ui_has_effort(Battle *b);
int bt_ui_enemy_count(Battle *b);
int bt_ui_selected(Battle *b);
int bt_ui_effort(Battle *b);
int bt_ui_target(Battle *b);
int bt_ui_round(Battle *b);
int bt_ui_cur(Battle *b);                    // whose input the screen is taking
int bt_ui_affordable(Battle *b, int row);    // can this row be committed right now?
const char *bt_cmd_name(int cmd);
// kind: 0 = command row, 1 = effort notch (idx 1..5), 2 = enemy. ImGui coordinates.
bool bt_ui_point(Battle *b, int kind, int idx, float *x, float *y);

// How many times the soft-lock watchdog has fired in this fight. Must be 0; --battle-ui-test
// asserts it, and the Dev panel shows it.
int bt_stuck_count(Battle *b);

// Dev: skip the fight and win it outright. The owner's "skip fights" toggle.
void bt_force_win(Battle *b);
void bt_force_lose(Battle *b);   // Dev/test: end it as a loss

// Dev panel rows for the battle system; returns true if it asked for something (out = a command).
bool bt_dev_ui(Battle *b, char *out, int cap);

// Give a party its COMBAT.md starting values. Called for a New Game.
void bt_party_init(BtParty *p, int count);

// The scripted battle self-test (--battle-selftest). Asserts, with no window and no input:
//   * `swing` cannot be won by attacking only, and IS won by parry -> open -> effort;
//   * the boss `klee` runs both phases and fires boss_break and boss_turn;
//   * stamina costs and effort magnitudes match COMBAT.md's table.
// Prints a step-by-step log. Returns 0 on success, non-zero on the first failure.
int bt_selftest();
