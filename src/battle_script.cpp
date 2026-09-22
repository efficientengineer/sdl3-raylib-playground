// battle_script.cpp — §9 THE BOSS. The one fight that is written rather than rolled.
//
//   owns      Klee's two phases: phase one (hold it open, three cycles, cannot be lost, attacking
//             does nothing), the scripted break and turn with their mid-fight text ids, the swap to
//             the klee2 row, and the phase-two restart that never costs the player the whole fight.
//   never     decides an ordinary round. Everything a non-boss fight does is battle_rules.cpp; this
//             file is called once at the end of each resolve and only acts when boss_phase is set.
//   exposes   bt_boss_script, bt_boss_restart_phase_two (battle_internal.h).
//   tested by ./capture.sh --battle-selftest (the boss section) and --chapter-playtest.
//
// The teaching order (§7a) lives in the tables' `grants` and in bt_build_rows, not here: an idea
// arrives by being used, never by being explained.

#include "battle_internal.h"

// ── the boss script (§9) ───────────────────────────────────────────────────────────────────────
void bt_boss_script(Battle *b) {
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
void bt_boss_restart_phase_two(Battle *b) {
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
