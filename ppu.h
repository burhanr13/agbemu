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
    int dot;

    union {
        hword h;
        struct {
            hword bg_mode : 3;
            hword reserved : 1;
            hword frame_sel : 1;
            hword hblank_free : 1;
            hword obj_tilemap : 1;
            hword forced_blank : 1;
            hword bg0_enable : 1;
            hword bg1_enable : 1;
            hword bg2_enable : 1;
            hword bg3_enable : 1;
            hword obj_enable : 1;
            hword win0_enable : 1;
            hword win1_enable : 1;
            hword winobj_enable : 1;
        };

    } dispcnt;
    union {
        hword h;
        
    } dispstat;
} PPU;

void tick_ppu(PPU* ppu);

word ppu_read(PPU* ppu, word addr);
void ppu_write(PPU* ppu, word addr, word data);

#endif