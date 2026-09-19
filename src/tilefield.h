// tilefield.h — the field, Sega style (TILES.md / D18): 32x32 tiles, grid walking, one atlas.
// A new, small module beside field.cpp; that file's 3D and painted-screen work is parked and stays
// reachable from the Dev panel. star_logic.cpp owns the screens; this owns the map, the renderer,
// the controls and its own dialogue box, and hands back only what the game knows how to do.
#pragma once
#include <stdint.h>

struct TileField;

enum TfEventKind {
    TFE_NONE = 0,
    TFE_SCENE,                       // ev.arg is a cutscene id; the game plays it and comes back
    TFE_ZONE,                        // ev.arg is an encounter table name; logged until battle exists
};

struct TfEvent { int kind; char arg[64]; };

#define TF_PARTY 4                   // leader + followers, one tile behind each other

// Everything a hot reload needs to put the party back where it stood. POD, memcpy'd into the reload
// blob by star_logic.cpp; every field is validated on the way back in.
struct TfSave {
    char map[32];
    int32_t tx[TF_PARTY], ty[TF_PARTY], facing[TF_PARTY];
    int32_t party, steps;
    int32_t dbg_solid, dbg_trig, dbg_coord, noclip;
};

TileField *tf_create();
void tf_destroy(TileField *t);

// Loads story/field/tmaps/<name>.tmap from the phone's files/, then the APK assets, then the repo.
// Falls back to a tiny built-in map so a missing or broken file is never a crash.
bool tf_load_map(TileField *t, const char *name);

// Renders the world into its FBO, draws the FBO plus the touch UI into ImGui's background draw
// list, and returns at most one event. `ui_blocked` suppresses input while the Dev panel is open.
void tf_tick(TileField *t, int w, int h, float dt, bool ui_blocked, TfEvent *ev);

// Map picker, debug toggles and the tile coordinates. Returns true and fills `out` when the owner
// pressed a button that wants a line in the Dev messages.
bool tf_dev_ui(TileField *t, char *out, int cap);

void tf_message(TileField *t, const char *text);     // show a line in the field dialogue box

// Desktop capture: load `map` and write the WHOLE map to `out_path` as one PNG, so a map can be
// read as a town without touching the phone. No camera, no letterbox — every tile, every layer.
void tf_capture_to(TileField *t, const char *map, int scale, const char *out_path);
bool tf_capture_done(TileField *t);

void tf_save(TileField *t, TfSave *s);
void tf_restore(TileField *t, const TfSave *s);

int tf_map_count();
const char *tf_map_name_at(int i);
