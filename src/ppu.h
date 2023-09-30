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

enum { OBJ_MODE_NORMAL, OBJ_MODE_SEMITRANS, OBJ_MODE_WINOBJ };

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
    int ly;

    hword bgline[4][GBA_SCREEN_W];
    hword objline[4][GBA_SCREEN_W];
    byte slayer[GBA_SCREEN_W];
    byte wlayer[GBA_SCREEN_W];

    struct {
        word x;
        word y;
    } bgaffintr[2];

    bool draw_bg[4];
    bool draw_obj[4];

    int obj_cycles;

    bool frame_complete;
} PPU;

void render_bg_lines(PPU* ppu);
void render_obj_lines(PPU* ppu);

void draw_line(PPU* ppu);

void on_hdraw(PPU* ppu);
void on_vblank(PPU* ppu);
void on_hblank(PPU* ppu);

#endif