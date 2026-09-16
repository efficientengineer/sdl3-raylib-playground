#include "perf.h"
#include "text.h"
#include <SDL3/SDL.h>
#include <stdio.h>

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#else
#include <OpenGL/gl3.h>
#endif

static int visible;
static Uint64 frame_start;
static double fps;
static double frame_ms;

static int draw_calls, triangles, vertices;
static int last_draw_calls, last_triangles, last_vertices;

static int fps_frames;
static Uint64 fps_last;

void perf_init(void) {
    visible = 0;
    fps = 0;
    frame_ms = 0;
    fps_frames = 0;
    fps_last = SDL_GetPerformanceCounter();
}

void perf_frame_begin(void) {
    frame_start = SDL_GetPerformanceCounter();
    draw_calls = 0;
    triangles = 0;
    vertices = 0;
}

void perf_add_draw_call(int tris, int verts) {
    draw_calls++;
    triangles += tris;
    vertices += verts;
}

void perf_frame_end(void) {
    Uint64 now = SDL_GetPerformanceCounter();
    double freq = (double)SDL_GetPerformanceFrequency();
    frame_ms = (now - frame_start) / freq * 1000.0;

    last_draw_calls = draw_calls;
    last_triangles = triangles;
    last_vertices = vertices;

    fps_frames++;
    double elapsed = (now - fps_last) / freq;
    if (elapsed >= 0.5) {
        fps = fps_frames / elapsed;
        fps_frames = 0;
        fps_last = now;
    }
}

int perf_visible(void) { return visible; }
void perf_toggle(void) { visible = !visible; }

void perf_draw(int sw, int sh) {
    if (!visible) return;

    float scx = 2.0f / sw, scy = 2.0f / sh;
    float x = 12.0f;
    float y = 120.0f;
    float line = 32.0f;

    char buf[64];

    text_begin();

    snprintf(buf, sizeof buf, "FPS: %.0f", fps);
    float r = fps >= 50 ? 0.3f : (fps >= 30 ? 1.0f : 1.0f);
    float g = fps >= 50 ? 1.0f : (fps >= 30 ? 0.8f : 0.3f);
    float b = fps >= 50 ? 0.3f : (fps >= 30 ? 0.0f : 0.3f);
    text_draw(buf, x, y, scx, scy, r, g, b);
    y += line;

    snprintf(buf, sizeof buf, "Frame: %.1f ms", frame_ms);
    text_draw(buf, x, y, scx, scy, 0.8f, 0.8f, 0.8f);
    y += line;

    snprintf(buf, sizeof buf, "Draws: %d", last_draw_calls);
    text_draw(buf, x, y, scx, scy, 0.8f, 0.8f, 0.8f);
    y += line;

    snprintf(buf, sizeof buf, "Tris: %d", last_triangles);
    text_draw(buf, x, y, scx, scy, 0.8f, 0.8f, 0.8f);
    y += line;

    snprintf(buf, sizeof buf, "Verts: %d", last_vertices);
    text_draw(buf, x, y, scx, scy, 0.8f, 0.8f, 0.8f);

    text_flush();
}
