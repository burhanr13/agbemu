#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "apu.h"
#include "cartridge.h"
#include "gba.h"
#include "thumb_isa.h"
#include "types.h"

word bkpt;
bool lg, dbg;
char* romfile;
bool uncap;
bool bootbios;
bool filter;
bool pause;
bool mute;

GBA* gba;
Cartridge* cart;
byte* bios;

char wintitle[200];

static inline void center_screen_in_window(int windowW, int windowH, SDL_Rect* dst) {
    if (windowW > windowH) {
        dst->h = windowH;
        dst->y = 0;
        dst->w = dst->h * GBA_SCREEN_W / GBA_SCREEN_H;
        dst->x = (windowW - dst->w) / 2;
    } else {
        dst->w = windowW;
        dst->x = 0;
        dst->h = dst->w * GBA_SCREEN_H / GBA_SCREEN_W;
        dst->y = (windowH - dst->h) / 2;
    }
}

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
                        if (*(f + 1) || i + 1 == argc || sscanf(argv[i + 1], "0x%x", &bkpt) < 1) {
                            printf("Provide valid hex address with -b\n");
                        }
                        break;
                    case 'u':
                        uncap = true;
                        break;
                    case 'B':
                        bootbios = true;
                        break;
                    case 'f':
                        filter = true;
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

void hotkey_press(SDL_KeyCode key) {
    switch (key) {
        case SDLK_p:
            pause = !pause;
            break;
        case SDLK_m:
            mute = !mute;
            break;
        case SDLK_f:
            filter = !filter;
            break;
        case SDLK_r:
            init_gba(gba, cart, bios);
            pause = false;
            break;
        case SDLK_TAB:
            uncap = !uncap;
            break;
        default:
            break;
    }
}

void update_input_keyboard(GBA* gba) {
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

void update_input_controller(GBA* gba, SDL_GameController* controller) {
    gba->io.keyinput.a &= ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A);
    gba->io.keyinput.b &= ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X);
    gba->io.keyinput.start &= ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START);
    gba->io.keyinput.select &= ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK);
    gba->io.keyinput.left &=
        ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    gba->io.keyinput.right &=
        ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    gba->io.keyinput.up &= ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
    gba->io.keyinput.down &=
        ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    gba->io.keyinput.l &=
        ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    gba->io.keyinput.r &=
        ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
}

byte color_lookup[32];
byte color_lookup_filter[32];

void init_color_lookups() {
    for (int i = 0; i < 32; i++) {
        float c = (float) i / 31;
        color_lookup[i] = c * 255;
        color_lookup_filter[i] = pow(c, 1.7) * 255;
    }
}

void gba_convert_screen(hword* gba_screen, Uint32* screen) {
    for (int i = 0; i < GBA_SCREEN_W * GBA_SCREEN_H; i++) {
        int r = gba_screen[i] & 0x1f;
        int g = (gba_screen[i] >> 5) & 0x1f;
        int b = (gba_screen[i] >> 10) & 0x1f;
        if (filter) {
            screen[i] = color_lookup_filter[r] << 16 | color_lookup_filter[g] << 8 |
                        color_lookup_filter[b] << 0;
        } else {
            screen[i] = color_lookup[r] << 16 | color_lookup[g] << 8 | color_lookup[b] << 0;
        }
    }
}

int main(int argc, char** argv) {

    read_args(argc, argv);
    if (!romfile) {
        printf("Usage: agbemu [options] <romfile>\n");
        return 0;
    }

    gba = malloc(sizeof *gba);
    cart = create_cartridge(romfile);
    if (!cart) {
        free(gba);
        printf("Invalid rom file\n");
        return -1;
    }
    bios = load_bios("bios.bin");
    if (!bios) {
        free(gba);
        destroy_cartridge(cart);
        printf("No bios found. Make sure 'bios.bin' in current directory.\n");
        return -1;
    }

    arm_generate_lookup();
    thumb_generate_lookup();
    init_color_lookups();
    init_gba(gba, cart, bios);

    char* romfilenodir = romfile + strlen(romfile);
    while (romfilenodir > romfile && *(romfilenodir - 1) != '/') romfilenodir--;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);

    SDL_GameController* controller = NULL;
    if (SDL_NumJoysticks() > 0) {
        controller = SDL_GameControllerOpen(0);
    }

    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_CreateWindowAndRenderer(GBA_SCREEN_W * 4, GBA_SCREEN_H * 4, SDL_WINDOW_RESIZABLE, &window,
                                &renderer);
    snprintf(wintitle, 199, "agbemu | %s | %.2lf FPS", romfilenodir, 0.0);
    SDL_SetWindowTitle(window, wintitle);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    SDL_Texture* texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                          GBA_SCREEN_W, GBA_SCREEN_H);

    SDL_AudioSpec audio_spec = {
        .freq = SAMPLE_FREQ, .format = AUDIO_F32, .channels = 2, .samples = SAMPLE_BUF_LEN / 2};
    SDL_AudioDeviceID audio = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    SDL_PauseAudioDevice(audio, 0);

    Uint64 prev_time = SDL_GetPerformanceCounter();
    Uint64 prev_fps_update = prev_time;
    Uint64 prev_fps_frame = 0;
    const Uint64 frame_ticks = SDL_GetPerformanceFrequency() / 60;
    Uint64 frame = 0;

    bool running = true;
    while (running) {
        Uint64 cur_time;
        Uint64 elapsed;
        bool play_audio = !(pause || mute || uncap) && (gba->io.nr52 & (1 << 7));

        if (!pause) {
            do {
                while (!gba->ppu.frame_complete) {
                    gba_step(gba);
                    if (play_audio && gba->apu.samples_full) {
                        SDL_QueueAudio(audio, gba->apu.sample_buf, sizeof gba->apu.sample_buf);
                        gba->apu.samples_full = false;
                    }
                }
                gba->ppu.frame_complete = false;
                frame++;

                cur_time = SDL_GetPerformanceCounter();
                elapsed = cur_time - prev_time;
            } while (uncap && elapsed < frame_ticks);
        }

        void* pixels;
        int pitch;
        SDL_LockTexture(texture, NULL, &pixels, &pitch);
        gba_convert_screen((hword*) gba->ppu.screen, pixels);
        SDL_UnlockTexture(texture);

        int windowW, windowH;
        SDL_GetWindowSize(window, &windowW, &windowH);
        SDL_Rect dst;
        center_screen_in_window(windowW, windowH, &dst);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        SDL_RenderPresent(renderer);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) hotkey_press(e.key.keysym.sym);
        }
        update_input_keyboard(gba);
        if (controller) update_input_controller(gba, controller);

        cur_time = SDL_GetPerformanceCounter();
        elapsed = cur_time - prev_time;
        Sint64 wait = frame_ticks - elapsed;

        if (play_audio) {
            while (SDL_GetQueuedAudioSize(audio) >= 12 * SAMPLE_BUF_LEN) SDL_Delay(1);
        } else if (wait > 0 && !uncap) {
            SDL_Delay(wait * 1000 / SDL_GetPerformanceFrequency());
        }
        cur_time = SDL_GetPerformanceCounter();
        elapsed = cur_time - prev_fps_update;
        if (elapsed >= SDL_GetPerformanceFrequency() / 2) {
            double fps =
                (double) SDL_GetPerformanceFrequency() * (frame - prev_fps_frame) / elapsed;
            snprintf(wintitle, 199, "agbemu | %s | %.2lf FPS", romfilenodir, fps);
            SDL_SetWindowTitle(window, wintitle);
            prev_fps_update = cur_time;
            prev_fps_frame = frame;
        }
        prev_time = cur_time;
    }

    destroy_cartridge(cart);
    free(bios);
    free(gba);

    if (controller) SDL_GameControllerClose(controller);

    SDL_CloseAudioDevice(audio);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}