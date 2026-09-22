// battle_rules.cpp — the battle's TABLES and its SIMULATION, plus the public bt_* API that drives
// them. story/v3/COMBAT.md §1-§9 is the contract; the round is written out in battle_internal.h.
//
//   owns      §0 the palette and the RNG, §1 the enemy/skill/encounter/party tables, §2 the turn
//             logic — effort and stamina, the parry, the opening, WINDED, damage, the enemy phase,
//             the round resolve — and §4 the public API (bt_create/start/tick/done/force_*).
//   never     draws, reads input, touches GL or ImGui, or knows what a menu row looks like. The
//             separation is a requirement, not a nicety: bt_selftest drives this file headless.
//   exposes   battle.h's public API, and battle_internal.h's simulation calls to the other three.
//   tested by ./capture.sh --battle-selftest (every rule), --chapter-playtest (in the real game).
//
// Later chapters add ROWS to §1, not code anywhere else.

#include "battle_internal.h"


// HEADLESS. bt_selftest sets this, and it is the switch that keeps the simulation free of GL: no
// texture is created, no file is read, nothing touches a context that does not exist.
bool bt_headless = false;

// A tiny deterministic RNG, so the selftest is reproducible and no fight depends on rand().
uint32_t bt_rng_state = 0x1234567u;
uint32_t bt_rand() { bt_rng_state = bt_rng_state * 1664525u + 1013904223u; return bt_rng_state >> 8; }

uint32_t bt_pal(int i, uint32_t fallback) {
    if (!BT_PAL_OK || i < 0 || i >= BT_PAL_N) return fallback;
    return BT_PAL[i];
}

#if BT_DRAW
const char *bt_pref() {
    static char path[512]; static bool got = false;
    if (!got) { const char *p = SDL_GetPrefPath(PREF_ORG, PREF_APP); snprintf(path, sizeof(path), "%s", p ? p : ""); got = true; }
    return path;
}
// Phone pref path first (fast_reload.sh pushes there), then the APK assets, then the repo on desktop.
void *bt_read(const char *rel, size_t *size) {
    char path[768];
    snprintf(path, sizeof(path), "%s%s", bt_pref(), rel);
    void *d = SDL_LoadFile(path, size);
    if (!d) d = SDL_LoadFile(rel, size);
    if (!d) { snprintf(path, sizeof(path), "story/%s", rel); d = SDL_LoadFile(path, size); }
    return d;
}

void bt_palette_load() {
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
void bt_palette_load() {}
#endif
extern const int   BT_EFFORT_COST[6] = { 0, 2, 5, 9, 15, 24 };
extern const float BT_EFFORT_MAG [6] = { 0, 0.5f, 0.8f, 1.0f, 1.25f, 1.5f };
extern const BtEnemyDef BT_ENEMIES[] = {
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
extern const int BT_ENEMY_COUNT = (int)(sizeof(BT_ENEMIES) / sizeof(BT_ENEMIES[0]));
extern const BtSkillDef BT_SKILLS[] = {
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
extern const int BT_SKILL_COUNT = (int)(sizeof(BT_SKILLS) / sizeof(BT_SKILLS[0]));
extern const BtEncounterDef BT_ENCOUNTERS[] = {
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
extern const int BT_ENCOUNTER_COUNT = (int)(sizeof(BT_ENCOUNTERS) / sizeof(BT_ENCOUNTERS[0]));
extern const BtPartyDef BT_PARTY_DEF[BT_PARTY] = {
    { "falke",   "Falke",   60, 40, 6, 10 },
    { "ottilie", "Ottilie", 48, 36, 5,  7 },
    { "distel",  "Distel",  44, 30, 7,  0 },   // never does a point of damage
};
const char *BT_PHASE_NAME[] = { "INPUT", "RESOLVE", "ENEMY", "OVER" };

// ── the log and the event queue ────────────────────────────────────────────────────────────────
void bt_log(Battle *b, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    if (b->log_n >= BT_LOG_LINES) {
        memmove(b->log[0], b->log[1], sizeof(b->log[0]) * (BT_LOG_LINES - 1));
        b->log_n = BT_LOG_LINES - 1;
    }
    vsnprintf(b->log[b->log_n], BT_LOG_LEN, fmt, ap);
    b->log_n++;
    va_end(ap);
}

void bt_emit(Battle *b, int kind, const char *arg) {
    if (b->evq_n >= BT_EVQ) return;
    b->evq[b->evq_n].kind = kind;
    snprintf(b->evq[b->evq_n].arg, sizeof(b->evq[b->evq_n].arg), "%s", arg ? arg : "");
    b->evq_n++;
}
void bt_goal(Battle *b, const char *g) {
    snprintf(b->goal, sizeof(b->goal), "%s", g ? g : "");
    bt_emit(b, BTE_GOAL, b->goal);
}

// ── lookups ────────────────────────────────────────────────────────────────────────────────────
int bt_enemy_def(const char *id) {
    for (int i = 0; i < BT_ENEMY_COUNT; i++) if (!strcmp(BT_ENEMIES[i].id, id)) return i;
    return -1;
}
const BtEncounterDef *bt_encounter(const char *id) {
    for (int i = 0; i < BT_ENCOUNTER_COUNT; i++) if (!strcmp(BT_ENCOUNTERS[i].id, id)) return &BT_ENCOUNTERS[i];
    return 0;
}
const BtEnemyDef *bt_edef(Battle *b, int i) { return &BT_ENEMIES[b->en[i].def]; }
bool bt_is_yard(Battle *b) { return b->enc && (b->enc->flags & BTE_YARD) != 0; }

// {{HERDER}}'s night sight: passive, free, always on — every tell shown one beat earlier, hidden
// tells shown at all, and enemy stamina bars visible (§5). The lens (BT_ITEM_PASTURE) does the
// same thing permanently, for the whole party, with no {{HERDER}} in it (LOOT.md).
bool bt_night_sight(Battle *b) {
    if (b->party && (b->party->items & BT_ITEM_PASTURE)) return true;
    if (!b->party) return false;
    for (int i = 0; i < b->party->count; i++)
        if (b->party->a[i].alive && !strcmp(b->party->a[i].id, "distel")) return true;
    return false;
}
// Is this enemy's tell READABLE by the player right now? (Drawing asks; the simulation does not —
// a tell you could not read is still a tell, and guarding into it still parries. The punishment
// for not reading it is that you did not guard.)
bool bt_tell_visible(Battle *b, int i, int viewer) {
    if (!b->en[i].tell) return false;
    if (viewer >= 0 && b->pa[viewer].blind) return false;
    if (bt_edef(b, i)->flags & BTF_HIDDEN_TELL) return bt_night_sight(b);
    return true;
}

int bt_alive_enemies(Battle *b) {
    int n = 0; for (int i = 0; i < b->enemy_count; i++) if (b->en[i].alive) n++; return n;
}
int bt_alive_party(Battle *b) {
    int n = 0; for (int i = 0; i < b->party->count; i++) if (b->party->a[i].alive) n++; return n;
}
int bt_first_live_enemy(Battle *b) {
    for (int i = 0; i < b->enemy_count; i++) if (b->en[i].alive) return i;
    return -1;
}

// ── affordability and the menu's light (§11, §4a.4) ────────────────────────────────────────────
// Guard is NEVER greyed. It is the one entry that is always lit, and on a tired character it is
// the only one — the closest thing to a hint the chapter contains.
int bt_cost(int cmd, int effort) {
    switch (cmd) {
        case BTC_ATTACK: case BTC_SKILL: return BT_EFFORT_COST[bt_clamp(effort, 1, 5)];
        default: return 0;                            // Guard, Item, Run cost nothing
    }
}
bool bt_can_pay(Battle *b, int who, int cmd, int effort) {
    if (cmd == BTC_GUARD) return true;
    return b->party->a[who].stam >= bt_cost(cmd, effort);
}
// The cheapest effort this character can still pay for with this command, or 0 for "none at all".
int bt_cheapest_affordable(Battle *b, int who, int cmd) {
    for (int e = 1; e <= 5; e++) if (bt_can_pay(b, who, cmd, e)) return e;
    return 0;
}
const BtSkillDef *bt_skill_at(const char *owner, int n) {
    int k = 0;
    for (int i = 0; i < BT_SKILL_COUNT; i++)
        if (!strcmp(BT_SKILLS[i].owner, owner)) { if (k == n) return &BT_SKILLS[i]; k++; }
    return 0;
}
int bt_skill_count(const char *owner) {
    int k = 0; for (int i = 0; i < BT_SKILL_COUNT; i++) if (!strcmp(BT_SKILLS[i].owner, owner)) k++;
    return k;
}

const char *BT_CMD_NAME[BTC_COUNT] = { "", "Attack", "Skill", "Guard", "Item", "Run" };
extern const uint32_t BT_CMD_BIT[BTC_COUNT] = {
    0, BT_KNOWS_ATTACK, BT_KNOWS_SKILL, BT_KNOWS_GUARD, BT_KNOWS_ITEM, BT_KNOWS_RUN
};

// ── THE COMMAND LIST, built in ONE place ───────────────────────────────────────────────────────
// bt_tick and bt_draw both need it and they must agree to the row, so neither builds its own any
// more (they used to, and a disagreement would mean the tap committing a different command from
// the one under the finger).
//
// THE GUARANTEE (owner bug, "stuck after attacking"): the list always contains at least one entry
// the player can actually choose. Guard is free, so the moment nothing else is affordable Guard is
// granted whether or not the teaching order has reached it — an unaffordable Attack with Guard not
// yet "arrived" is a menu of one dead row, which is exactly the soft lock that was reported.
int bt_build_rows(Battle *b, int who, int *rows) {
    const BtActor *a = &b->party->a[who];
    int n = 0;
    for (int pass = 0; pass < 2; pass++) {
        n = 0;
        for (int c = BTC_ATTACK; c < BTC_COUNT; c++) {
            if (c == BTC_RUN && (b->enc->flags & BTE_NO_RUN)) continue;
            if (c == BTC_SKILL && !bt_skill_count(a->id)) continue;
            if (c == BTC_ITEM && !(b->party->items & (BT_ITEM_TEACHING | BT_ITEM_HILL))) continue;
            if (!(b->party->known & BT_CMD_BIT[c])) continue;
            rows[n++] = c;
        }
        bool any = false;
        for (int r = 0; r < n; r++)
            if (rows[r] == BTC_GUARD || bt_cheapest_affordable(b, who, rows[r]) > 0) { any = true; break; }
        if (any) break;
        if (b->party->known & BT_KNOWS_GUARD) break;            // already there; pass two would not help
        b->party->known |= BT_KNOWS_GUARD;
        BT_LOGF("battle: nothing affordable for %s — Guard granted early so the menu is never dead", a->name);
    }
    return n;
}

// ── starting a fight ───────────────────────────────────────────────────────────────────────────
void bt_setup_enemies(Battle *b, const char *const *ids) {
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

void bt_round_begin(Battle *b) {
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
    // WHOSE INPUT. It used to be slot 0 unconditionally; a dead slot 0 left the screen in INPUT
    // with nothing able to commit, which is a soft lock with no way out. Always the first LIVING
    // slot, and if nobody is alive the round is over anyway (bt_resolve checks straight after).
    b->cur = 0;
    while (b->cur < b->party->count && !b->party->a[b->cur].alive) b->cur++;
    if (b->cur >= b->party->count) b->cur = 0;
    b->phase = BTP_INPUT;
    BT_LOGF("battle: round %d begins (party %d alive, %d enemies alive)", b->round,
            bt_alive_party(b), bt_alive_enemies(b));
}

// ── damage ─────────────────────────────────────────────────────────────────────────────────────
void bt_hurt_enemy(Battle *b, int who, int ei, float base, int effort, const char *verb) {
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

void bt_hurt_actor(Battle *b, int who, int amount, const char *src) {
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
void bt_do_skill(Battle *b, int who, int si, int effort) {
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

void bt_do_guard(Battle *b, int who) {
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
        BT_LOGF("battle: PARRY — %s turns aside %s; it is open next round", a->name, bt_edef(b, parried)->name);
        b->party->known |= BT_KNOWS_EFFORT;        // there is an opening to spend into (§7a row 5)
        b->party->fail_streak = 0;                 // both escalations reset the moment a parry lands
        b->tell_speed = 1.0f; b->guard_pulse = 0;
    } else {
        b->pa[who].winded = 0;                                        // waiting pays for the next one
        a->stam = bt_min(a->stam_max, a->stam + BT_GUARD_REGEN);      // nets +12. Never a waste.
        bt_log(b, "%s guards.", a->name);
    }
}

void bt_do_item(Battle *b, int who, int which) {
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
void bt_enemy_phase(Battle *b) {
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
                        BT_LOGF("battle: WINDED %s -%d stam (now %d), wind %d, no regen for %d round(s)",
                                a->name, drain, a->stam, e->wind, b->pa[t].winded);
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
        BT_LOGF("battle: tell — %s draws back at %s", d->name, b->party->a[t].name);
        // Guard APPEARS in the list the first time something telegraphs at you (§7a row 3).
        b->party->known |= BT_KNOWS_GUARD;
    }
}
// ── resolve one whole round ────────────────────────────────────────────────────────────────────
// Order (§6): items -> skills -> attacks and guards -> enemy attacks. Guard is in effect for the
// whole round however slow the character is: the parry is a reading test, never a speed check.
void bt_resolve(Battle *b) {
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
    if (bt_alive_enemies(b) == 0) {
        BT_LOGF("battle: END win — %s, round %d", b->enc->id, b->round);
        b->outcome = BT_WIN; b->phase = BTP_OVER; return;
    }
    if (bt_alive_party(b) == 0) {
        if (b->boss_phase == 2) { BT_LOGF("battle: phase two restart"); bt_boss_restart_phase_two(b); return; }
        BT_LOGF("battle: END lose — %s, round %d%s", b->enc->id, b->round,
                bt_is_yard(b) ? " (yard: instant retry)" : "");
        b->outcome = BT_LOSE; b->phase = BTP_OVER; return;
    }
    bt_round_begin(b);
}
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
    {
        char pnames[96] = ""; size_t o = 0;
        for (int i = 0; i < p->count; i++)
            o += (size_t)snprintf(pnames + o, sizeof(pnames) - o, "%s%s", i ? "+" : "", p->a[i].id);
        char enames[96] = ""; o = 0;
        for (int i = 0; i < b->enemy_count; i++)
            o += (size_t)snprintf(enames + o, sizeof(enames) - o, "%s%s", i ? "+" : "", bt_edef(b, i)->id);
        BT_LOGF("battle: START enc=%s map=%s party=%d [%s] enemies=%d [%s] known=0x%x items=0x%x",
                enc->id, b->map, p->count, pnames, b->enemy_count, enames, p->known, p->items);
    }
    b->last_phase = BTP_INPUT; b->last_cur = 0; b->phase_t = 0.0f; b->stuck_fires = 0; b->aborted = 0;
    bt_round_begin(b);
    return true;
}
// How many times the watchdog has fired this fight. --battle-ui-test asserts it is zero.
int bt_enc_count() { return BT_ENCOUNTER_COUNT; }
const char *bt_enc_id(int i) { return (i >= 0 && i < BT_ENCOUNTER_COUNT) ? BT_ENCOUNTERS[i].id : ""; }
int bt_stuck_count(Battle *b) { return b ? b->stuck_fires : 0; }

bool bt_done(Battle *b) {
    if (!b || b->outcome == BT_RUNNING) return false;
    // In {{MENTOR}}'s yard, losing is free and instant: no menu, no reload (§8). The caller just
    // calls bt_start again.
    if (b->outcome == BT_LOSE && bt_is_yard(b)) return true;
    return b->dismissed != 0;
}

// Dev/test: end the fight as a loss, without having to grind the party down.
void bt_force_lose(Battle *b) {
    if (!b || !b->party) return;
    for (int i = 0; i < b->party->count; i++) { b->party->a[i].hp = 0; b->party->a[i].alive = 0; }
    b->outcome = BT_LOSE;
    b->phase = BTP_OVER;
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
    // ── THE COMMIT, and the bug the owner hit ──────────────────────────────────────────────────
    // This used to live INSIDE `if (b->phase == BTP_INPUT)`, which worked for the keyboard and was
    // a permanent soft lock for touch. bt_input() runs here, so a key press flipped the phase to
    // BTP_RESOLVE and was consumed two lines later in the SAME frame. A TAP is different: the
    // command rows are ImGui InvisibleButtons inside bt_draw(), which runs AFTER this block, so a
    // tap set BTP_RESOLVE at the end of the frame — and on the next frame the phase was no longer
    // BTP_INPUT, so neither this block NOR bt_draw's menu ran again. No commit, no menu, no input:
    // stuck after the first tap on Attack, exactly as reported, with the last log line being the
    // palette load because nothing after it ever logged. The commit is now checked on its own.
    if (b->phase == BTP_RESOLVE && !b->party->a[b->cur].alive) {
        // The chooser died between choosing and committing (a reload, a scripted hit): skip them.
        b->phase = BTP_INPUT;
    }
    if (b->phase == BTP_INPUT || b->phase == BTP_RESOLVE) {
        int rows[BTC_COUNT], nrows = bt_build_rows(b, b->cur, rows);
        if (b->phase == BTP_INPUT && !ui_blocked) bt_input(b, nrows, rows);
        if (b->phase == BTP_RESOLVE) {
            // Commit this character's choice and move on to the next.
            int cmd = nrows > 0 ? rows[bt_clamp(b->ui_cmd, 0, nrows - 1)] : BTC_GUARD;
            int eff = bt_clamp(b->ui_effort, 1, 5);
            if (cmd != BTC_GUARD && !bt_can_pay(b, b->cur, cmd, eff)) {
                int cheap = bt_cheapest_affordable(b, b->cur, cmd);
                if (!cheap) {
                    // Greyed: the tap does nothing, and it SAYS so rather than looking broken.
                    BT_LOGF("battle: %s cannot afford %s (stam %d) — choice ignored",
                            b->party->a[b->cur].name, BT_CMD_NAME[cmd], b->party->a[b->cur].stam);
                    bt_log(b, "%s cannot afford that.", b->party->a[b->cur].name);
                    b->phase = BTP_INPUT;
                    goto drawn;
                }
                eff = cheap;
            }
            b->cmd[b->cur] = cmd;
            b->effort[b->cur] = eff;
            b->skill[b->cur] = b->ui_skill;
            b->item[b->cur] = 0;
            b->target[b->cur] = b->ui_target;
            b->party->last_effort[cmd] = eff;
            BT_LOGF("battle: %s chose %s effort %d cost %d (stam %d) target %d",
                    b->party->a[b->cur].name, BT_CMD_NAME[cmd], cmd == BTC_GUARD ? 0 : eff,
                    bt_cost(cmd, eff), b->party->a[b->cur].stam, b->ui_target);
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
        // Tap anywhere, any key, or simply wait: a results banner is a presentation state and may
        // never be the thing that traps the player.
        if (b->banner_t > 1.2f && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space) ||
                                   ImGui::IsMouseClicked(0) || bt_is_yard(b)))
            b->dismissed = 1;
        if (b->banner_t > 6.0f && !b->dismissed) {
            BT_LOGF("battle: results banner timed out — dismissing");
            b->dismissed = 1;
        }
    }

    // ── THE WATCHDOG ───────────────────────────────────────────────────────────────────────────
    // Anything that is not "waiting for the player" has a time limit. If one is exceeded we name
    // the state in the log and force it forward, so a bug here costs a strange round, never the
    // playthrough. stuck_fires is asserted at zero by --battle-ui-test.
    {
        int key = b->phase * 16 + b->cur;
        if (key != b->last_phase * 16 + b->last_cur) {
            if (b->phase != b->last_phase)
                BT_LOGF("battle: state %s -> %s (round %d, %s)", BT_PHASE_NAME[b->last_phase],
                        BT_PHASE_NAME[b->phase], b->round, b->party->a[b->cur].name);
            b->last_phase = b->phase; b->last_cur = b->cur; b->phase_t = 0.0f;
        } else {
            b->phase_t += dt;
        }
        if (b->phase != BTP_INPUT && b->phase_t > BT_STUCK_S) {
            b->stuck_fires++;
            BT_LOGF("battle: WATCHDOG — stuck in %s for %.1fs (round %d, cur %d, outcome %d). Forcing on.",
                    BT_PHASE_NAME[b->phase], b->phase_t, b->round, b->cur, b->outcome);
            if (b->phase == BTP_OVER) b->dismissed = 1;
            else { b->phase = BTP_INPUT; }
            b->phase_t = 0.0f;
        }
    }
    if (b->aborted && b->outcome == BT_RUNNING) {
        BT_LOGF("battle: ABORTED from the Dev panel");
        b->outcome = BT_FLED; b->phase = BTP_OVER; b->dismissed = 1;
    }
#else
    (void)w; (void)h; (void)dt; (void)ui_blocked;
#endif
    return b->outcome;
}
