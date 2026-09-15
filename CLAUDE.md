# CLAUDE.md

SDL3 + OpenGL playground. C11 own code, C++ deps allowed.

## Switches

- **TARGET: Android** — build and deploy to phone only. Don't build desktop.

## Style

- Own code: C11, `.c` files. C++ deps linked but not authored.
- Flat `src/`. No subdirs until ten files.
- One `CMakeLists.txt` shared by desktop and Android.
- FetchContent for SDL3, cglm. Single headers in `third_party/`.

## Commands

```bash
./build.sh [target]           # desktop build+run (default: combined_demo)
./deploy.sh                   # android APK → phone (Wi-Fi adb)
./compile_shaders.sh          # GLSL 450 → SPIR-V → glsl330, glsl300es, msl, hlsl
```

## Shader pipeline

Write GLSL 450 in `shaders/`. Run `./compile_shaders.sh`. Output in `shaders/compiled/`.
Tools: glslang (GLSL→SPIR-V), SPIRV-Cross (SPIR-V→everything else). Installed via brew.

## Libraries

- **SDL3** — windowing, input, lifecycle (FetchContent)
- **cglm** — math: matrices, vectors, quaternions, SIMD (FetchContent)
- **par_shapes** — procedural meshes: spheres, cylinders, tori, knots
- **stb_image/write/truetype** — image I/O, font rasterization (Roboto bundled)
- **FastNoiseLite** — procedural noise: Perlin, simplex, cellular
- **sokol_gfx** — GPU abstraction (GL/GLES/Metal/WebGPU); use with SDL3 windowing
- **SPIRV-Cross** — shader cross-compilation (build tool, not linked)

Single headers need `#define ...IMPLEMENTATION` once — that's in `src/third_party_impl.c`.

## Decisions

- SDL3 for windowing, not sokol_app — more mature, better Android lifecycle.
- sokol_gfx for GPU abstraction when needed, layered on SDL3's GL context.
- No raylib — too high level; raw GL preferred.
- Generation only — no mesh/asset loaders (no cgltf).
- FastNoiseLite, not FastNoise2 — C-compatible; FN2 is C++17.
- Procedural textures in shaders, not CPU libraries.
- Roboto font bundled — open source, consistent cross-platform.
- JDK 21 via brew for Android builds — AS bundled JDK 25 breaks Gradle 8.9.
- No realtime lighting — bake light into vertex colors/textures.
- Readability via: height fog, distance fog, good procedural textures.

## Index

- `src/` — all C source (text.h/text.c = font renderer)
- `shaders/` — GLSL 450 source; `compiled/` = generated, gitignored
- `assets/` — Roboto-Regular.ttf (also copied to android assets)
- `third_party/` — single-header libs (stb, par_shapes, FastNoiseLite, sokol)
- `android/` — Gradle project, SDLActivity, reuses root CMake
