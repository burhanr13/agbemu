#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "cartridge.h"
#include "gba.h"
#include "types.h"

word bkpt;
bool lg, dbg;
char* romfile;
bool uncap;

char wintitle[200];

void read_args(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (char* f = &argv[i][1]; *f; f++) {
                switch (*f) {
                    case 'l':
                        lg = true;
                        break;
                    case 'd':
                        lg = true;
                        dbg = true;
                        break;
                    case 'b':
                        if (*(f + 1) || i + 1 == argc ||
                            sscanf(argv[i + 1], "0x%x", &bkpt) < 1) {
                            printf("Provide valid hex address with -b\n");
                        }
                        break;
                    case 'u':
                        uncap = true;
                        break;
                    default:
                        printf("Invalid flag\n");
                }
            }
        } else {
            romfile = argv[i];
        }
    }
}

void update_input(GBA* gba) {
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
}

int main(int argc, char** argv) {

    read_args(argc, argv);
    if (!romfile) {
        printf("Usage: agbemu [-dlu] [-b <bkpt>] <romfile>\n");
        return 0;
    }

    GBA* gba = malloc(sizeof *gba);
    Cartridge* cart = create_cartridge(romfile);
    if (!cart) {
        free(gba);
        printf("Invalid rom file\n");
        return -1;
    }
    byte* bios = load_bios("bios.bin");
    if (!bios) {
        free(gba);
        destroy_cartridge(cart);
        printf("No bios found. Make sure 'bios.bin' in current directory.\n");
        return -1;
    }
    init_gba(gba, cart, bios);

    char* romfilenodir = romfile + strlen(romfile);
    while (romfilenodir > romfile && *(romfilenodir - 1) != '/') romfilenodir--;

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

    Uint64 prev_time = SDL_GetPerformanceCounter();
    Uint64 frame = 0;
    bool running = true;
    while (running) {

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
        }
        update_input(gba);

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

        frame++;
        Uint64 cur_time = SDL_GetPerformanceCounter();
        Uint64 diff = cur_time - prev_time;
        double diffsec = (double) diff / SDL_GetPerformanceFrequency();
        int wait = 1000 * (1.0 / 60 - diffsec);
        if (wait > 0 && !uncap) {
            SDL_Delay(wait);
            cur_time = SDL_GetPerformanceCounter();
            diff = cur_time - prev_time;
        }
        double fps = (double) SDL_GetPerformanceFrequency() / diff;
        if (frame % 60 == 0) {
            snprintf(wintitle, 199, "agbemu | %s | FPS: %.2lf", romfilenodir, fps);
            SDL_SetWindowTitle(window, wintitle);
        }
        prev_time = cur_time;
    }

    destroy_cartridge(cart);
    free(gba);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}