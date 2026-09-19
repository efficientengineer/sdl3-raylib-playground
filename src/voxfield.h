// voxfield.h — the voxel + sprite field. src/VOXFIELD_NOTES.md is the contract.
//
// The owner's pivot (2026-09-19): "a voxel and sprite approach. World mostly rendered like Minecraft,
// but we have sprites for characters and detail." The tile field (tilefield.cpp) stays, parked behind
// a Dev button; SCR_FIELD opens this.
//
// The world is a 3D grid of block ids, one block to a walk cell, built at load from the SAME
// story/field/tmaps/*.tmap files the tile field reads — nothing new has to be authored. Blocks are
// flat master-palette colours with procedural pixel detail in the fragment shader; characters, props
// and detail are camera-facing billboards from the existing walker sheets, the tileset atlas and the
// tileset's decal PNGs.
#pragma once
#include <stdint.h>

struct VoxField;

enum VxEventKind {
    VXE_NONE = 0,
    VXE_SCENE,                       // ev.arg is a cutscene id; the game plays it and comes back
    VXE_ZONE,                        // ev.arg is an encounter table name
};

struct VxEvent { int kind; char arg[64]; };

#define VX_PARTY 4

// Everything a hot reload needs to put the party back where it stood, plus the look knobs the owner
// was tuning. POD, memcpy'd into star_logic.cpp's reload blob; every field is validated on the way in.
struct VxSave {
    char map[32];
    int32_t tx[VX_PARTY], ty[VX_PARTY], facing[VX_PARTY];
    int32_t party, steps;
    int32_t dbg_solid, dbg_trig, dbg_coord, noclip;
    int32_t ortho, hd2d, cutaway;
    float pitch, fov, view_h, tilt, ao, detail;
    float amb;
    int32_t light_table;
    float fog, dof, bloom, vignette, grade;
    int32_t motes, res_scale_pct;
};

VoxField *vx_create();
void vx_destroy(VoxField *v);

bool vx_load_map(VoxField *v, const char *name);
void vx_tick(VoxField *v, int w, int h, float dt, bool ui_blocked, VxEvent *ev);
bool vx_dev_ui(VoxField *v, char *out, int cap);
void vx_message(VoxField *v, const char *text);

// Desktop capture (capture.sh --vox): renders one frame at `w`x`h` into a PNG, no phone involved.
void vx_capture_to(VoxField *v, const char *map, int w, int h, const char *out_path);
bool vx_capture_done(VoxField *v);

void vx_save(VoxField *v, VxSave *s);
void vx_restore(VoxField *v, const VxSave *s);

int vx_map_count();
const char *vx_map_name_at(int i);
