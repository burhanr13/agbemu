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
} BgTile;

enum { OBJ_MODE_NORMAL, OBJ_MODE_SEMITRANS, OBJ_MODE_OBJWIN };
enum { OBJ_SHAPE_SQR, OBJ_SHAPE_HORZ, OBJ_SHAPE_VERT };

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
            hword palette : 4;
        };
    };
    hword affparam;
} ObjAttr;

enum { WIN0, WIN1, OBJWIN };
enum { LBG0, LBG1, LBG2, LBG3, LOBJ, LBD };

typedef struct {
    byte priority : 2;
    byte semitrans : 1;
    byte obj0 : 1;
    byte pad : 4;
} ObjDotAttr;

typedef struct _GBA GBA;

typedef struct {
    GBA* master;

    hword screen[GBA_SCREEN_H][GBA_SCREEN_W];
    byte ly;

    hword bgline[4][GBA_SCREEN_W];
    hword objline[GBA_SCREEN_W];
    ObjDotAttr objdotattrs[GBA_SCREEN_W];
    byte window[GBA_SCREEN_W];

    struct {
        word x;
        word y;
    } bgaffintr[2];

    bool draw_bg[4];
    bool draw_obj;

    int obj_cycles;

    bool frame_complete;
} PPU;

void render_bgs(PPU* ppu);
void render_objs(PPU* ppu);
void render_windows(PPU* ppu);

void draw_scanline(PPU* ppu);

void on_hdraw(PPU* ppu);
void on_vblank(PPU* ppu);
void on_hblank(PPU* ppu);

#endif