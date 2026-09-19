// field.h — the walkable world (2.5D, Octopath style). See FIELD.md, the contract this builds to,
// and src/FIELD_NOTES.md for the navmesh and camera-zone additions the owner asked for.
// star_logic.cpp owns the screens and the cutscene player; field.cpp owns the map, the renderer and
// the touch controls, and hands back the few things only the game knows how to do (play a scene).
#pragma once
#include <stdint.h>

struct Field;                        // opaque: everything lives in field.cpp

enum FieldEventKind {
    FE_NONE = 0,
    FE_SCENE,                        // ev.arg is a cutscene id; the game plays it and comes back
    FE_ZONE,                         // ev.arg is an encounter table name; logged until battle exists
};

struct FieldEvent { int kind; char arg[64]; };

enum CamMode { CAM_FOLLOW = 0, CAM_FIXED, CAM_RAIL };

// One camera zone: a rectangle in map units plus the shot to use inside it. The Dev sliders edit the
// live copy of the zone the player is standing in, and "print zone" logs it back in map syntax.
struct CamZone {
    char id[24];                             // stable name: capture and painting files are keyed on it
    float x, z, w, d;                        // rectangle in map units
    int32_t mode;
    int32_t ortho;                           // 1 = orthographic; `fov` is then ignored
    float ortho_h;                           // half-height of the ortho box, in world units
    float yaw, pitch, fov, dist, height;     // follow
    float cx, cy, cz, tx, ty, tz;            // fixed: eye and target
    int32_t pan;                             // fixed: keep the position, turn to hold the player
    float ax, ay, az, bx, by, bz;            // rail: eye slides A -> B with the player's progress
};

// Everything a hot reload needs to put the player back where they stood, plus the camera the owner
// was tuning. POD, memcpy'd into the reload blob by star_logic.cpp; every field is validated back in.
struct FieldSave {
    char map[32];
    float x, z;                      // position in map units
    int32_t poly;                    // navmesh polygon, re-resolved from x/z if the mesh changed
    int32_t facing;                  // 0 S, 1 W, 2 E, 3 N in world terms
    int32_t zone_idx, nav_debug, fill_width, show_blockers, see_on;
    float see_radius, walk_radius;
    CamZone zone;                    // the live (possibly slider-edited) zone
};

Field *field_create();
void field_destroy(Field *f);

// Loads story/field/maps/<name>.map from the phone's files/ then the APK assets. Falls back to a
// tiny built-in map so a missing or broken file is never a crash. Uses the map's own spawn.
bool field_load_map(Field *f, const char *name);

// Renders the world into its FBO, draws the FBO plus the touch UI into ImGui's background draw
// list, and returns at most one event. `ui_blocked` suppresses input while the Dev panel is open.
void field_tick(Field *f, int w, int h, float dt, bool ui_blocked, FieldEvent *ev);

// Coordinates, map picker, nav toggle and the live camera sliders. Returns true and fills `out`
// when the owner pressed "print zone" (the caller logs it to the Dev messages).
bool field_dev_ui(Field *f, char *out, int cap);

void field_message(Field *f, const char *text);    // show a line in the field dialogue box

// Framing watchdog: true once when the player has been off screen, so the caller can log it to the
// Dev messages. Rate-limited inside; it is a debug aid, not an error path.
bool field_take_warning(Field *f, char *out, int cap);

// Headless-ish capture, for the desktop path: load `map`, place the camera at zone `zone` exactly as
// authored, render at 640x360 x scale and write `out_path` (plus `<out>_depth.png`). No phone needed.
void field_capture_to(Field *f, const char *map, const char *zone, int scale, const char *out_path);
bool field_capture_done(Field *f);

void field_save(Field *f, FieldSave *s);
void field_restore(Field *f, const FieldSave *s);

int field_map_count();
const char *field_map_name_at(int i);
