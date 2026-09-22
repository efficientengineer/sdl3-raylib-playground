// star_logic.cpp — the game's entry point, and the one TU that carries the single-header code.
//
//   owns      get_game_api(), the four-line table host.cpp dlopens this library for, and the
//             STB_IMAGE / STB_IMAGE_WRITE implementations — they must exist exactly once in
//             libgame_logic.so, and this is the file every capture path links against.
//   never     holds a rule, a screen or a state of its own. Everything the game does is in
//             game.cpp, cutscene.cpp, chapter.cpp, dev_panel.cpp, settings.cpp and audio.cpp;
//             see src/ENGINE.md for the map.
//   exposes   get_game_api (GAME_API_EXPORT). Nothing else in this file is visible.
//   tested by every suite: they all start by dlopening this library and calling get_game_api.
//
// Built as libgame_logic.so and hot-reloaded by host.cpp (see CLAUDE.md, "Hot Reload Architecture").
// Scene content is generated: edit story/scenes/*.md, run ./story_prompt.py export, hot reload.

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
// The capture path's PNG writer. It lives here, the one TU every capture (dialogue, chapter,
// voxfield) links against.
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "star_internal.h"

GAME_API_EXPORT GameAPI get_game_api() {
    GameAPI api = {};
    api.create = game_create;
    api.destroy = game_destroy;
    api.tick = game_tick;
    api.on_save_event = game_on_save_event;
    api.serialize = game_serialize;
    api.deserialize = game_deserialize;
    api.wants_quit = game_wants_quit;
    return api;
}
