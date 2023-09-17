#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "cartridge.h"
#include "gba.h"
#include "types.h"

word bkpt;

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("no rom file given\n");
        return -1;
    }

    GBA* gba = malloc(sizeof *gba);
    Cartridge* cart = create_cartridge(argv[1]);
    if (!cart) {
        free(gba);
        printf("invalid rom file\n");
        return -1;
    }
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
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
        }
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        gba->io.keyinput.a = ~keys[SDL_SCANCODE_Z];
        gba->io.keyinput.b = ~keys[SDL_SCANCODE_X];
        gba->io.keyinput.start = ~keys[SDL_SCANCODE_RETURN];
        gba->io.keyinput.select = ~keys[SDL_SCANCODE_RSHIFT];
        gba->io.keyinput.left = ~keys[SDL_SCANCODE_LEFT];
        gba->io.keyinput.right = ~keys[SDL_SCANCODE_RIGHT];
        gba->io.keyinput.up = ~keys[SDL_SCANCODE_UP];
        gba->io.keyinput.down = ~keys[SDL_SCANCODE_DOWN];
        gba->io.keyinput.l = ~keys[SDL_SCANCODE_A];
        gba->io.keyinput.r = ~keys[SDL_SCANCODE_S];

        while (!gba->ppu.frame_complete) {
            tick_cpu(&gba->cpu);
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