#ifndef PPU_H
#define PPU_H

#include "types.h"

#define GBA_SCREEN_W 240
#define GBA_SCREEN_H 160
#define DOTS_W 308
#define LINES_H 228

typedef union {
    hword h;
    struct {
        hword num : 10;
        hword hflip : 1;
        hword vflip : 1;
        hword palette : 4;
    };
} Tile;

typedef struct _GBA GBA;

typedef struct {
    GBA* master;

    hword screen[GBA_SCREEN_H][GBA_SCREEN_W];
    int lx;
    int ly;

    struct {
        word x;
        word y;
    } bgaffintr[2];

    bool frame_complete;
} PPU;

void tick_ppu(PPU* ppu);

void draw_bg_line(PPU* ppu);

void on_vblank(PPU* ppu);
void on_hblank(PPU* ppu);

#endif