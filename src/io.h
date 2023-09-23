#ifndef IO_H
#define IO_H

#include "types.h"

#define IO_SIZE 0x400

enum {
    // display control
    DISPCNT = 0x000,
    DISPSTAT = 0x004,
    VCOUNT = 0x006,
    // bg control
    BG0CNT = 0x008,
    BG1CNT = 0x00a,
    BG2CNT = 0x00c,
    BG3CNT = 0x00e,
    BG0HOFS = 0x010,
    BG0VOFS = 0x012,
    BG1HOFS = 0x014,
    BG1VOFS = 0x016,
    BG2HOFS = 0x018,
    BG2VOFS = 0x01a,
    BG3HOFS = 0x01c,
    BG3VOFS = 0x01e,
    BG2PA = 0x020,
    BG2PB = 0x022,
    BG2PC = 0x024,
    BG2PD = 0x026,
    BG2X = 0x028,
    BG2Y = 0x02c,
    BG3PA = 0x030,
    BG3PB = 0x032,
    BG3PC = 0x034,
    BG3PD = 0x036,
    BG3X = 0x038,
    BG3Y = 0x03c,
    // window control
    WIN0H = 0x040,
    WIN1H = 0x042,
    WIN0V = 0x044,
    WIN1V = 0x046,
    WININ = 0x048,
    WINOUT = 0x04a,

    // dma control
    DMA0SAD = 0x0b0,
    DMA0DAD = 0x0b4,
    DMA0CNT_L = 0x0b8,
    DMA0CNT_H = 0x0ba,
    DMA1SAD = 0x0bc,
    DMA1DAD = 0x0c0,
    DMA1CNT_L = 0x0c4,
    DMA1CNT_H = 0x0c6,
    DMA2SAD = 0x0c8,
    DMA2DAD = 0x0cc,
    DMA2CNT_L = 0x0d0,
    DMA2CNT_H = 0x0d2,
    DMA3SAD = 0x0d4,
    DMA3DAD = 0x0d8,
    DMA3CNT_L = 0x0dc,
    DMA3CNT_H = 0x0de,

    // timer control
    TM0CNT_L = 0x100,
    TM0CNT_H = 0x102,
    TM1CNT_L = 0x104,
    TM1CNT_H = 0x106,
    TM2CNT_L = 0x108,
    TM2CNT_H = 0x10a,
    TM3CNT_L = 0x10c,
    TM3CNT_H = 0x10e,

    // key control
    KEYINPUT = 0x130,
    KEYCNT = 0x132,

    // system/interrupt control
    IE = 0x200,
    IF = 0x202,
    WAITCNT = 0x204,
    IME = 0x208,
    POSTFLG = 0x300,
    HALTCNT = 0x301
};

typedef struct _GBA GBA;

typedef struct {
    GBA* master;
    union {
        byte b[IO_SIZE];
        hword h[IO_SIZE >> 1];
        word w[IO_SIZE >> 2];
        struct {
            union {
                hword h;
                struct {
                    hword bg_mode : 3;
                    hword reserved : 1;
                    hword frame_sel : 1;
                    hword hblank_free : 1;
                    hword obj_tilemap : 1;
                    hword forced_blank : 1;
                    hword bg_enable : 4;
                    hword obj_enable : 1;
                    hword win_enable : 2;
                    hword winobj_enable : 1;
                };
            } dispcnt;
            hword unused_002;
            union {
                hword h;
                struct {
                    hword vblank : 1;
                    hword hblank : 1;
                    hword vcounteq : 1;
                    hword vblank_irq : 1;
                    hword hblank_irq : 1;
                    hword vcount_irq : 1;
                    hword unused : 2;
                    hword lyc : 8;
                };
            } dispstat;
            hword vcount;
            union {
                hword h;
                struct {
                    hword priority : 2;
                    hword tile_base : 2;
                    hword unused : 2;
                    hword mosaic : 1;
                    hword palette : 1;
                    hword tilemap_base : 5;
                    hword overflow : 1;
                    hword size : 2;
                };
            } bgcnt[4];
            struct {
                hword hofs;
                hword vofs;
            } bgtext[4];
            struct {
                shword pa;
                shword pb;
                shword pc;
                shword pd;
                sword x;
                sword y;
            } bgaff[2];
            byte gap0xx[DMA0SAD - BG3Y - 4];
            struct {
                word sad;
                word dad;
                hword ct;
                union {
                    hword h;
                    struct {
                        hword unused : 5;
                        hword dadcnt : 2;
                        hword sadcnt : 2;
                        hword repeat : 1;
                        hword wsize : 1;
                        hword drq : 1;
                        hword start : 2;
                        hword irq : 1;
                        hword enable : 1;
                    };
                } cnt;
            } dma[4];
            byte unused_0xx[TM0CNT_L - DMA3CNT_H - 2];
            struct {
                hword reload;
                union {
                    hword h;
                    struct {
                        hword rate : 2;
                        hword countup : 1;
                        hword unused : 3;
                        hword irq : 1;
                        hword enable : 1;
                        hword unused1 : 8;
                    };
                } cnt;
            } tm[4];
            byte gap1xx[KEYINPUT - TM3CNT_H - 2];
            union {
                hword h;
                struct {
                    hword a : 1;
                    hword b : 1;
                    hword select : 1;
                    hword start : 1;
                    hword right : 1;
                    hword left : 1;
                    hword up : 1;
                    hword down : 1;
                    hword r : 1;
                    hword l : 1;
                    hword unused : 6;
                };
            } keyinput;
            union {
                hword h;
                struct {
                    hword a : 1;
                    hword b : 1;
                    hword select : 1;
                    hword start : 1;
                    hword right : 1;
                    hword left : 1;
                    hword up : 1;
                    hword down : 1;
                    hword r : 1;
                    hword l : 1;
                    hword unused : 4;
                    hword irq_enable : 1;
                    hword irq_cond : 1;
                };
            } keycnt;
            byte gap2xx[IE - KEYCNT - 2];
            union {
                hword h;
                struct {
                    hword vblank : 1;
                    hword hblank : 1;
                    hword vcounteq : 1;
                    hword timer : 4;
                    hword serial : 1;
                    hword dma : 4;
                    hword keypad : 1;
                    hword gamepak : 1;
                    hword unused : 2;
                };
            } ie;
            union {
                hword h;
                struct {
                    hword vblank : 1;
                    hword hblank : 1;
                    hword vcounteq : 1;
                    hword timer : 4;
                    hword serial : 1;
                    hword dma : 4;
                    hword keypad : 1;
                    hword gamepak : 1;
                    hword unused : 2;
                };
            } ifl;
            union {
                word w;
            } waitcnt;
            word ime;
            byte unused_2xx[POSTFLG - IME - 4];
            byte postflg;
            byte haltcnt;
        };
    };
} IO;

byte io_readb(IO* io, word addr);
void io_writeb(IO* io, word addr, byte data);

hword io_readh(IO* io, word addr);
void io_writeh(IO* io, word addr, hword data);

word io_readw(IO* io, word addr);
void io_writew(IO* io, word addr, word data);

#endif