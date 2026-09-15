#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "raylib.h"

int main(int argc, char *argv[]) {
    // For now, just use raylib's window — combining at the GL level
    // is the next experiment once both work individually
    InitWindow(800, 600, "Combined SDL3 + Raylib");
    SetTargetFPS(60);

    float x = 400, y = 300;

    while (!WindowShouldClose()) {
        if (IsKeyDown(KEY_RIGHT)) x += 3;
        if (IsKeyDown(KEY_LEFT))  x -= 3;
        if (IsKeyDown(KEY_DOWN))  y += 3;
        if (IsKeyDown(KEY_UP))    y -= 3;

        BeginDrawing();
        ClearBackground((Color){30, 30, 60, 255});
        DrawCircle((int)x, (int)y, 40, RED);
        DrawText("Arrow keys to move", 10, 10, 20, WHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
