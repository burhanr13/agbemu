#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>

#include "cartridge.h"
#include "gba.h"

int main(int argc, char** argv) {
    if (argc < 2) return -1;
    
    GBA* gba = malloc(sizeof *gba);
    Cartridge* cart = create_cartridge(argv[1]);
    if (!cart) return -1;
    init_gba(gba, cart);

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_CreateWindowAndRenderer(GBA_SCREEN_W * 4, GBA_SCREEN_H * 4, 0, &window,
                                &renderer);
    SDL_SetWindowTitle(window, "agbemu");
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR555,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             GBA_SCREEN_W, GBA_SCREEN_H);

    bool running = true;
    while (running) {

        SDL_Event e;
        while(SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
        }

        while(!gba->ppu.frame_complete) {
            tick_gba(gba);
        }
        gba->ppu.frame_complete = false;

        void* pixels;
        int pitch;
        SDL_LockTexture(texture, NULL, &pixels, &pitch);
        memcpy(pixels, gba->ppu.screen, sizeof gba->ppu.screen);
        SDL_UnlockTexture(texture);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        
        SDL_Delay(10);
    }

    destroy_cartridge(cart);
    free(gba);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    
    SDL_Quit();

    return 0;
}