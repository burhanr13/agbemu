#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "apu.h"
#include "arm_isa.h"
#include "cartridge.h"
#include "debugger.h"
#include "emulator.h"
#include "gba.h"
#include "thumb_isa.h"
#include "types.h"

char wintitle[200];

static inline void center_screen_in_window(int windowW, int windowH, SDL_Rect* dst) {
    if (windowW > windowH * GBA_SCREEN_W / GBA_SCREEN_H) {
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

int main(int argc, char** argv) {

    if (emulator_init(argc, argv) < 0) return -1;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);

    SDL_GameController* controller = NULL;
    if (SDL_NumJoysticks() > 0) {
        controller = SDL_GameControllerOpen(0);
    }

    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_CreateWindowAndRenderer(GBA_SCREEN_W * 2, GBA_SCREEN_H * 2, SDL_WINDOW_RESIZABLE, &window,
                                &renderer);
    snprintf(wintitle, 199, "agbemu | %s | %.2lf FPS", agbemu.romfilenodir, 0.0);
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

    agbemu.running = !agbemu.debugger;
    while (true) {
        while (agbemu.running) {
            Uint64 cur_time;
            Uint64 elapsed;
            bool play_audio = !(agbemu.pause || agbemu.mute || agbemu.uncap || agbemu.gba->stop) &&
                              (agbemu.gba->io.nr52 & (1 << 7));

            if (!(agbemu.pause || agbemu.gba->stop)) {
                do {
                    while (!agbemu.gba->stop && !agbemu.gba->ppu.frame_complete) {
                        gba_step(agbemu.gba);
                        if (agbemu.gba->apu.samples_full) {
                            if (play_audio) {
                                SDL_QueueAudio(audio, agbemu.gba->apu.sample_buf,
                                               sizeof agbemu.gba->apu.sample_buf);
                            }
                            agbemu.gba->apu.samples_full = false;
                        }
                    }
                    agbemu.gba->ppu.frame_complete = false;
                    frame++;

                    cur_time = SDL_GetPerformanceCounter();
                    elapsed = cur_time - prev_time;
                } while (agbemu.uncap && elapsed < frame_ticks);
            }

            void* pixels;
            int pitch;
            SDL_LockTexture(texture, NULL, &pixels, &pitch);
            gba_convert_screen((hword*) agbemu.gba->ppu.screen, pixels);
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
                if (e.type == SDL_QUIT) agbemu.running = false;
                if (e.type == SDL_KEYDOWN) hotkey_press(e.key.keysym.sym);
            }
            update_input_keyboard(agbemu.gba);
            if (controller) update_input_controller(agbemu.gba, controller);
            update_keypad_irq(agbemu.gba);

            cur_time = SDL_GetPerformanceCounter();
            elapsed = cur_time - prev_time;
            Sint64 wait = frame_ticks - elapsed;

            if (play_audio) {
                while (SDL_GetQueuedAudioSize(audio) >= 16 * SAMPLE_BUF_LEN) SDL_Delay(1);
            } else if (wait > 0 && !agbemu.uncap) {
                SDL_Delay(wait * 1000 / SDL_GetPerformanceFrequency());
            }
            cur_time = SDL_GetPerformanceCounter();
            elapsed = cur_time - prev_fps_update;
            if (elapsed >= SDL_GetPerformanceFrequency() / 2) {
                double fps =
                    (double) SDL_GetPerformanceFrequency() * (frame - prev_fps_frame) / elapsed;
                snprintf(wintitle, 199, "agbemu | %s | %.2lf FPS", agbemu.romfilenodir, fps);
                SDL_SetWindowTitle(window, wintitle);
                prev_fps_update = cur_time;
                prev_fps_frame = frame;
            }
            prev_time = cur_time;
        }

        if (agbemu.debugger) {
            debugger_run();
        } else {
            break;
        }
    }

    emulator_quit();

    if (controller) SDL_GameControllerClose(controller);

    SDL_CloseAudioDevice(audio);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}
