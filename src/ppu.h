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

enum { OBJ_NORMAL, OBJ_SEMITRANS, OBJ_OBJWIN };

typedef struct {
    union {
        hword attr0;
        struct {
            hword y : 8;
            hword aff : 1;
            hword disable_double : 1;
            hword mode : 2;
            hword mosaic : 1;
            hword palmode : 1;
            hword shape : 2;
        };
    };
    union {
        hword attr1;
        struct {
            hword x : 9;
            hword unused : 3;
            hword hflip : 1;
            hword vflip : 1;
            hword size : 2;
        };
        struct {
            hword _x : 9;
            hword affparamind : 5;
            hword _size : 2;
        };
    };
    union {
        hword attr2;
        struct {
            hword tilenum : 10;
            hword priority : 2;
            hword palnum : 4;
        };
    };
    hword affparam;
} ObjAttr;

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