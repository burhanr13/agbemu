#ifndef PPU_H
#define PPU_H

#include "types.h"

#define GBA_SCREEN_W 240
#define GBA_SCREEN_H 160
#define DOTS_W 308
#define LINES_H 228

typedef struct _GBA GBA;

typedef struct {
    GBA* master;

    hword screen[GBA_SCREEN_H][GBA_SCREEN_W];
    int lx;
    int ly;
    bool frame_complete;
} PPU;

void tick_ppu(PPU* ppu);

void draw_bg_line(PPU* ppu);

#endif