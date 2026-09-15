# CLAUDE.md

SDL3 + Raylib playground. C11, macOS desktop + Android.

## Style

- C11, no C++. Short files. No abstractions until third use.
- Flat `src/`. No subdirs until ten files.
- One `CMakeLists.txt` shared by desktop and Android.
- FetchContent for deps. No submodules.
- Each demo = one `.c` file with `main()`.

## Commands

```bash
./build.sh [target]           # desktop build+run (default: combined_demo)
./deploy.sh                   # android APK → phone (Wi-Fi adb)
```

## Index

- `src/` — all C source
- `android/` — Gradle project, NativeActivity, reuses root CMake
- `build.sh` — desktop configure+build+run
- `deploy.sh` — android build+install+launch, adb at 192.168.1.217:5555
