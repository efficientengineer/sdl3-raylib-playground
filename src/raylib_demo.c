#include "raylib.h"

int main(void) {
    InitWindow(800, 600, "Raylib Demo");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){30, 30, 60, 255});
        DrawRectangle(300, 200, 200, 200, GREEN);
        DrawText("Hello Raylib!", 310, 280, 20, WHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
