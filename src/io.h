#ifndef IA_H
#define IA_H

#include "types.h"

#define IA_SIZE 0x400

enum {
    DISPCNT = 0x000,
    DISPSTAT = 0x004,
    VCOUNT = 0x006
};

typedef union {
    byte b[IA_SIZE];
    hword h[IA_SIZE >> 1];
    word w[IA_SIZE >> 2];
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
    };
} IO;

byte iA_readb(IO* io, word addr);
void iA_writeb(IO* io, word addr, byte data);

hword iA_readh(IO* io, word addr);
void iA_writeh(IO* io, word addr, hword data);

word iA_readw(IO* io, word addr);
void iA_writew(IO* io, word addr, word data);

#endif