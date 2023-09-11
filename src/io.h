#ifndef IA_H
#define IA_H

#include "types.h"

#define IO_SIZE 0x400

enum {
    DISPCNT = 0x000,
    DISPSTAT = 0x004,
    VCOUNT = 0x006,
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
    BG2X_L = 0x028,
    BG2X_H = 0x02a,
    BG2Y_L = 0x02c,
    BG2Y_H = 0x02e,
    BG3PA = 0x030,
    BG3PB = 0x032,
    BG3PC = 0x034,
    BG3PD = 0x036,
    BG3X_L = 0x038,
    BG3X_H = 0x03a,
    BG3Y_L = 0x03c,
    BG3Y_H = 0x03e,
    KEYINPUT = 0x130,
    KEYCNT = 0x132,
    IE = 0x200,
    IF = 0x202,
    WAITCNT = 0x204,
    IME = 0x208
};

typedef union {
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
        union{
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
            hword pa;
            hword pb;
            hword pc;
            hword pd;
            word x;
            word y;
        } bgrot[2];
        byte gap[KEYINPUT - BG3Y_H - 2];
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
        byte unused_1xx[IE - KEYCNT - 2];
        union {
            hword h;
            struct {
                hword vblank : 1;
                hword hblank : 1;
                hword vcounteq : 1;
                hword timer0 : 1;
                hword timer1 : 1;
                hword timer2 : 1;
                hword timer3 : 1;
                hword serial : 1;
                hword dma0 : 1;
                hword dma1 : 1;
                hword dma2 : 1;
                hword dma3 : 1;
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
                hword timer0 : 1;
                hword timer1 : 1;
                hword timer2 : 1;
                hword timer3 : 1;
                hword serial : 1;
                hword dma0 : 1;
                hword dma1 : 1;
                hword dma2 : 1;
                hword dma3 : 1;
                hword keypad : 1;
                hword gamepak : 1;
                hword unused : 2;
            };
        } ifl;
        union {
            word w;
            struct {};
        } waitcnt;
        word ime;
    };
} IO;

byte io_readb(IO* io, word addr);
void io_writeb(IO* io, word addr, byte data);

hword io_readh(IO* io, word addr);
void io_writeh(IO* io, word addr, hword data);

word io_readw(IO* io, word addr);
void io_writew(IO* io, word addr, word data);

#endif