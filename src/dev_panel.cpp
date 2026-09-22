// dev_panel.cpp — the Dev window and the phone<->dev message log.
//
//   owns      dev_log.txt (the newest MAX_MSGS lines, never the oldest — keeping the oldest once
//             silently dropped every new DEV line past the cap), the Messages tab, the scene and
//             chapter controls, the mood audition buttons, and the battle/field dev boxes it hosts.
//   never     appears in a capture or a self-test run's verdict: it is drawn and ignored. Nothing
//             in here may change a rule; a button either jumps the chapter or sends a message.
//   exposes   dev/dev_load/dev_send/dev_step/draw_dev.
//   tested by --robustness (it is drawn every frame of the sweep) and by the owner's thumb.
#include "star_internal.h"

// ───────────────────────── Dev messages (phone <-> dev chat, see CLAUDE.md) ─────────────────────────

// The log itself. game.cpp clears it on create; chapter.cpp and the Dev window both write to it.
DevChat dev;
const char *dev_log_path() {
    static char path[600];
    if (!path[0]) snprintf(path, sizeof(path), "%sdev_log.txt", pref_path());
    return path;
}

void dev_load() {                                  // keeps the newest MAX_MSGS lines
    int before = dev.count;
    dev.count = 0;
    size_t size = 0;
    char *data = (char *)SDL_LoadFile(dev_log_path(), &size);
    if (!data) return;
    int total = 0;
    for (size_t i = 0; i < size; i++) if (data[i] == '\n') total++;
    int skip = total - MAX_MSGS;
    char *p = data, *end = data + size;
    while (p < end) {
        char *nl = (char *)memchr(p, '\n', end - p);
        if (!nl) nl = end;
        int len = (int)(nl - p);
        if (skip-- <= 0 && dev.count < MAX_MSGS && len > 15 && p[0] == '[' &&
            (!memcmp(p + 1, "DEV ", 4) || !memcmp(p + 1, "YOU ", 4))) {
            DevMsg *m = &dev.msgs[dev.count++];
            m->from_dev = (p[1] == 'D');
            memcpy(m->stamp, p + 5, 8); m->stamp[8] = 0;
            int n = len - 15 < MSG_LEN - 1 ? len - 15 : MSG_LEN - 1;
            memcpy(m->text, p + 15, n); m->text[n] = 0;
        }
        p = nl + 1;
    }
    SDL_free(data);
    if (dev.count != before) dev.scroll = true;
}

void dev_send(const char *text) {
    dev_load();                                           // re-read so lines appended by dev_msg.sh survive
    time_t now = time(nullptr);
    struct tm *tm = localtime(&now);
    SDL_IOStream *f = SDL_IOFromFile(dev_log_path(), "wb");
    if (!f) return;
    char line[MSG_LEN + 32];
    for (int i = dev.count >= MAX_MSGS ? 1 : 0; i < dev.count; i++) {
        int n = snprintf(line, sizeof(line), "[%s %s] %s\n", dev.msgs[i].from_dev ? "DEV" : "YOU", dev.msgs[i].stamp, dev.msgs[i].text);
        SDL_WriteIO(f, line, n);
    }
    int n = snprintf(line, sizeof(line), "[YOU %02d:%02d:%02d] %s\n", tm->tm_hour, tm->tm_min, tm->tm_sec, text);
    SDL_WriteIO(f, line, n);
    SDL_CloseIO(f);
    dev_load();
}
void dev_step(Star *st, int step) {
    if (step < 0) step = 0;
    if (step >= CH_STEP_COUNT) step = CH_STEP_COUNT - 1;
    if (!st->ch.started) ch_new_game(&st->ch);
    st->in_battle = 0;
    ch_jump(&st->ch, st->vx, step);
    const ChStep *t = &CH_STEPS[st->ch.step];
    if (t->kind == CHS_END) { st->screen = SCR_END; st->title_t = 0; return; }
    if (st->screen != SCR_FIELD) enter_field(st);       // enter_field may retire a CLIP: re-read
    if (st->ch.step != step) ch_jump(&st->ch, st->vx, step);
    if (CH_STEPS[st->ch.step].kind == CHS_CLIP) field_scene(st, CH_STEPS[st->ch.step].arg);
}

// Small dev overlay: a corner button opening the phone <-> dev message log plus scene test controls.
void draw_dev(Star *st, int w, int h, float dt, float dpi) {
    ImGuiStyle &s = ImGui::GetStyle();
    float k = dpi * 1.3f;
    ImGui::GetIO().FontGlobalScale = 1.0f;
    s.FramePadding = ImVec2(8 * k, 6 * k); s.ItemSpacing = ImVec2(6 * k, 4 * k); s.WindowPadding = ImVec2(8 * k, 8 * k);
    s.ScrollbarSize = 12 * k; s.WindowRounding = 4 * k; s.FrameRounding = 3 * k; s.WindowBorderSize = 0; s.TouchExtraPadding = ImVec2(4 * k, 4 * k);

    dev.poll_t += dt;
    if (dev.poll_t > (dev.open ? 0.5f : 3.0f)) { dev.poll_t = 0; dev_load(); }

    ImGuiWindowFlags bare = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
    // The two corner buttons: Settings (the player's) and Dev (the owner's). Settings is drawn first
    // so it sits to the LEFT of Dev, and it is here rather than inside the Dev panel on purpose —
    // it belongs to whoever is holding the phone, on the title, in a cutscene and in the field alike.
    ImGui::SetNextWindowPos(ImVec2(w - 4 * k, 4 * k), ImGuiCond_Always, ImVec2(1, 0));
    ImGui::Begin("##devbtn", nullptr, bare | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.28f, 0.18f, g_set.open ? 0.9f : 0.35f));
    if (ImGui::Button("Settings")) g_set.open = !g_set.open;
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.5f, dev.open ? 0.9f : 0.35f));
    if (ImGui::Button(dev.open ? "Close" : "Dev")) { dev.open = !dev.open; dev.scroll = true; }
    ImGui::PopStyleColor();
    ImGui::End();

    if (g_set.open) {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(w * (w > h ? 0.46f : 0.86f), 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.10f, 0.97f));
        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted("Audio");
        int pct = (int)(g_set.volume * 100.0f + 0.5f);
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12);
        if (ImGui::SliderInt("Master volume", &pct, 0, 100, "%d %%")) {
            g_set.volume = pct / 100.0f;
            g_set.dirty = true; g_set.save_t = 0;
        }
        bool mute = g_set.muted;
        if (ImGui::Checkbox("Mute", &mute)) { g_set.muted = mute; g_set.dirty = true; g_set.save_t = 0; }
        if (g_set.env_forced) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.45f, 1.0f), "muted by STAR_MUTE");
        }
        ImGui::Separator();
        if (ImGui::Button("Close")) g_set.open = false;
        ImGui::End();
        ImGui::PopStyleColor();
    }

    if (!dev.open) return;

    ImGui::SetNextWindowPos(ImVec2(w * 0.02f, h * 0.06f));
    ImGui::SetNextWindowSize(ImVec2(w * 0.96f, h * (w > h ? 0.88f : 0.42f)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.10f, 0.96f));
    ImGui::Begin("Dev", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
    // The intro-era buttons are GONE (owner, 2026-09-21: "the Dev tools next scene just goes to the
    // next scene. And the next after that, no gameplay between them"). There is no scene list to
    // walk any more — the chapter table is the only order the game has — so the controls below step
    // the CHAPTER, and a CLIP step plays its scene and hands back to the PLAY step after it.
    if (ImGui::Button("Title")) { st->screen = SCR_TITLE; st->title_t = 0; dev.open = false; }

    // ── chapter one ───────────────────────────────────────────────────────────────────────────
    // Where the story thinks it is, and a way to put it anywhere. Jumping to a step sets every flag
    // the steps before it gated on, so the world is consistent with having played them.
    ImGui::SeparatorText("Chapter one");
    {
        const ChStep *s = &CH_STEPS[st->ch.step < 0 ? 0 : st->ch.step];
        static const char *KIND[] = { "PLAY", "CLIP", "SET", "END" };
        ImGui::Text("step %d/%d  %s  %s   goal: %s", st->ch.step + 1, CH_STEP_COUNT, KIND[s->kind],
                    s->kind == CHS_CLIP ? s->arg : (s->map ? s->map : "(same map)"),
                    st->ch.goal[0] ? st->ch.goal : "-");
        ImGui::Text("mandatory ten: %d/%d   party %d   %s",
                    CH_MANDATORY - ch_mandatory_missing(&st->ch), CH_MANDATORY,
                    st->ch.party.count, st->ch.bell_started ? "bell running" : "bell idle");
        ImGui::TextDisabled("skipping a step SETS the flags it gated on, so the gates stay consistent");
        if (ImGui::Button("Restart chapter")) { ch_new_game(&st->ch); enter_field(st); dev.open = false; }
        ImGui::SameLine();
        if (ImGui::Button("< prev step") && st->ch.step > 0) { dev_step(st, st->ch.step - 1); dev.open = false; }
        ImGui::SameLine();
        if (ImGui::Button("next step >")) { dev_step(st, st->ch.step + 1); dev.open = false; }
        ImGui::SameLine();
        bool skip = st->ch.skip_fights != 0;
        if (ImGui::Checkbox("Skip fights", &skip)) st->ch.skip_fights = skip ? 1 : 0;
        // Every step, so any block can be reached in one tap.
        if (ImGui::BeginCombo("Jump to step", "...")) {
            for (int i = 0; i < CH_STEP_COUNT; i++) {
                char lbl[96];
                const ChStep *t = &CH_STEPS[i];
                snprintf(lbl, sizeof(lbl), "%2d  %s  %s", i + 1, KIND[t->kind],
                         t->kind == CHS_CLIP ? t->arg : (t->goal ? t->goal : (t->map ? t->map : "-")));
                if (ImGui::Selectable(lbl, i == st->ch.step)) { dev_step(st, i); dev.open = false; }
            }
            ImGui::EndCombo();
        }
        // The flags, so a gate that will not open can be seen and forced.
        if (ImGui::TreeNode("Flags")) {
            for (int i = 0; i < CH_FLAG_COUNT; i++) {
                bool on = ch_has(&st->ch, i);
                ImGui::PushID(i);
                if (ImGui::Checkbox("##f", &on)) {
                    if (on) ch_set(&st->ch, i); else st->ch.flags &= ~(1ull << i);
                }
                ImGui::PopID();
                ImGui::SameLine();
                ImGui::TextColored(i < CH_MANDATORY ? ImVec4(0.95f, 0.85f, 0.45f, 1) : ImVec4(0.7f, 0.75f, 0.85f, 1),
                                   "%s%s", CH_FLAG_NAME[i], i < CH_MANDATORY ? "  (mandatory)" : "");
            }
            ImGui::TreePop();
        }
        if (st->bat) { char cmd[128]; if (bt_dev_ui(st->bat, cmd, sizeof(cmd))) dev_send(cmd); }
    }
    ImGui::SameLine();
    if (ImGui::Button("Field")) { st->fade_to_scene = -1; st->fade = 0.0f; enter_field(st); dev.open = false; }
    static const char *mood_names[CS_MOOD_COUNT] = {"wonder", "dread", "tense", "confront", "sorrow", "hope"};
    ImGui::SameLine();
    ImGui::TextDisabled("  mood:");
    for (int i = 0; i < CS_MOOD_COUNT; i++) {
        if (w > h || i % 3) ImGui::SameLine();
        bool cur = mus.on && mus.target == i;
        if (cur) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.3f, 1));
        if (ImGui::Button(mood_names[i])) mus_start(i);
        if (cur) ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    { bool pm = st->port_motion; if (ImGui::Checkbox("portrait motion", &pm)) st->port_motion = pm; }
    ImGui::Separator();
    if (st->screen == SCR_FIELD) {
        char line[224];
        if (st->vx) { if (vx_dev_ui(st->vx, line, sizeof(line))) dev_send(line); ImGui::Separator(); }
    }
    float input_h = ImGui::GetFrameHeightWithSpacing() + 4 * k;
    ImGui::BeginChild("##msgs", ImVec2(0, -input_h));
    for (int i = 0; i < dev.count; i++) {
        ImGui::PushStyleColor(ImGuiCol_Text, dev.msgs[i].from_dev ? ImVec4(0.45f, 0.8f, 1, 1) : ImVec4(0.55f, 1, 0.55f, 1));
        ImGui::TextWrapped("[%s] %s: %s", dev.msgs[i].stamp, dev.msgs[i].from_dev ? "DEV" : "YOU", dev.msgs[i].text);
        ImGui::PopStyleColor();
    }
    if (dev.scroll) { ImGui::SetScrollHereY(1.0f); dev.scroll = false; }
    ImGui::EndChild();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80 * k);
    bool enter = ImGui::InputText("##in", dev.input, MSG_LEN, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::Button("Send") || enter) && dev.input[0]) { dev_send(dev.input); dev.input[0] = 0; }
    ImGui::End();
    ImGui::PopStyleColor();
}
