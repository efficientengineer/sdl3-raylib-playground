#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *win = SDL_CreateWindow("SDL3 Demo", 800, 600, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, NULL);

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = 0;
        }
        SDL_SetRenderDrawColor(ren, 30, 30, 60, 255);
        SDL_RenderClear(ren);

        SDL_FRect rect = {300, 200, 200, 200};
        SDL_SetRenderDrawColor(ren, 100, 200, 100, 255);
        SDL_RenderFillRect(ren, &rect);

        SDL_RenderPresent(ren);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
