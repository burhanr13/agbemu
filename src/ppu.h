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

typedef union {
    hword h;
    struct {
        hword frac : 8;
        hword intg : 7;
        hword sign : 1;
    };
} fix16;

typedef union {
    word w;
    struct {
        word frac : 8;
        word intg : 19;
        word sign : 1;
        word unused : 4;
    };
} fix32;

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

float fix16tofloat(fix16 f);
float fix32tofloat(fix32 f);

#endif