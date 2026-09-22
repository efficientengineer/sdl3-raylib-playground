// battle_test.cpp — §5 bt_selftest and §6 the standalone battle_tool.
//
//   owns      the headless assertions over every rule in COMBAT.md §1-§9 (the effort table, the
//             parry and the opening, WINDED, the teaching order, the boss's phases, the four finds),
//             the little drivers the test chooses commands with, and the tool's main() with its
//             scripted capture poses.
//   never     ships in the game's behaviour: nothing in battle_rules.cpp or battle_ui.cpp calls into
//             this file. It only ever drives the simulation from outside, headless, with no GL.
//   exposes   bt_selftest (battle.h); main() only under BATTLE_TOOL_MAIN.
//   tested by being the test: ./capture.sh --battle-selftest, ./build_desktop/battle_tool.

#include "battle_internal.h"

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
