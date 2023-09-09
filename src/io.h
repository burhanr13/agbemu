#ifndef IA_H
#define IA_H

#include "types.h"

#define IO_SIZE 0x400

enum {
    DISPCNT = 0x000,
    DISPSTAT = 0x004,
    VCOUNT = 0x006,
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
        union {
            hword h;
            struct {
                hword ly : 8;
                hword unused : 8;
            };
        } vcount;
        byte gap[KEYINPUT - VCOUNT - 2];
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