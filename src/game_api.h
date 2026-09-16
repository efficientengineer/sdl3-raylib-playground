#pragma once
#include <stddef.h>

struct GameAPI {
    void *state;
    void *(*create)(float dpi_scale);
    void (*destroy)(void *state);
    void (*tick)(void *state, int w, int h, float dpi_scale);
    void (*on_save_event)(void *state);
    size_t (*serialize)(void *state, void *buf, size_t buf_size);
    void (*deserialize)(void *state, const void *buf, size_t size);
    int (*wants_quit)(void *state);
};

typedef GameAPI (*GetGameAPIFunc)();

#define GAME_API_EXPORT extern "C" __attribute__((visibility("default")))
