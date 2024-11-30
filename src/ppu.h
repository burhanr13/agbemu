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
    shword affparam;
} ObjAttr;

enum { WIN0, WIN1, WOUT, WOBJ };
enum { LBG0, LBG1, LBG2, LBG3, LOBJ, LBD, LMAX };

enum { EFF_NONE, EFF_ALPHA, EFF_BINC, EFF_BDEC };

typedef struct _GBA GBA;

typedef struct {
    GBA* master;

    hword screen[GBA_SCREEN_H][GBA_SCREEN_W];
    byte ly;

    hword layerlines[LMAX][GBA_SCREEN_W];
    struct {
        byte priority : 2;
        byte semitrans : 1;
        byte mosaic : 1;
        byte pad : 4;
    } objdotattrs[GBA_SCREEN_W];
    byte window[GBA_SCREEN_W];

    struct {
        sword x;
        sword y;
        sword mosx;
        sword mosy;
    } bgaffintr[2];

    byte bgmos_y;
    byte bgmos_ct;
    byte objmos_y;
    byte objmos_ct;

    bool draw_bg[4];
    bool draw_obj;
    bool in_win[2];
    bool obj_semitrans;
    bool obj_mos;

    int obj_cycles;

    bool frame_complete;
} PPU;

void render_bgs(PPU* ppu);
void render_objs(PPU* ppu);
void render_windows(PPU* ppu);

void draw_scanline(PPU* ppu);

void ppu_hdraw(PPU* ppu);
void ppu_vblank(PPU* ppu);
void ppu_hblank(PPU* ppu);

#endif