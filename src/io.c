#include "io.h"

#include <stdio.h>

#include "gba.h"

byte io_readb(IO* io, word addr) {
    hword h = io_readh(io, addr & ~1);
    if (addr & 1) {
        return h >> 8;
    } else return h;
}

void io_writeb(IO* io, word addr, byte data) {
    if (addr == POSTFLG) {
        io->postflg = data;
    } else if (addr == HALTCNT) {
        if (data & (1 << 7)) {
            io->master->stop = true;
        } else {
            io->master->halt = true;
        }
    } else {
        hword h;
        if (addr & 1) {
            h = data << 8;
            h |= io->b[addr & ~1];
        } else {
            h = data;
            h |= io->b[addr | 1] << 8;
        }
        io_writeh(io, addr & ~1, h);
    }
}

hword io_readh(IO* io, word addr) {
    if (BG0HOFS <= addr && addr <= BG3Y_H) {
        return 0;
    }
    return io->h[addr >> 1];
}

void io_writeh(IO* io, word addr, hword data) {
    switch (addr) {
        case DISPSTAT:
            io->dispstat.h &= 0b111;
            io->dispstat.h |= data & ~0b111;
            break;
        case BG2X_L:
        case BG2X_H:
        case BG2Y_L:
        case BG2Y_H:
        case BG3X_L:
        case BG3X_H:
        case BG3Y_L:
        case BG3Y_H: {
            word w;
            if (addr & 0b10) {
                w = data << 16;
                w |= io->h[(addr>>1) & ~1];
            } else {
                w = data;
                w |= io->h[(addr >> 1) | 1];
            }
            io_writew(io, addr & ~0b11, w);
            break;
        }
        case KEYINPUT:
            break;
        case IF:
            io->ifl.h &= ~data;
            break;
        case POSTFLG:
            io_writeb(io, addr, data);
            io_writeb(io, addr | 1, data >> 8);
        default:
            io->h[addr >> 1] = data;
    }
}

word io_readw(IO* io, word addr) {
    return io_readh(io, addr) | (io_readh(io, addr | 2) << 16);
}

void io_writew(IO* io, word addr, word data) {
    switch (addr) {
        case BG2X_L:
            io->bgaff[0].x = data;
            io->master->ppu.bgaffintr[0].x = data;
            break;
        case BG2Y_L:
            io->bgaff[0].y = data;
            io->master->ppu.bgaffintr[0].y = data;
            break;
        case BG3X_L:
            io->bgaff[1].x = data;
            io->master->ppu.bgaffintr[1].x = data;
            break;
        case BG3Y_L:
            io->bgaff[1].y = data;
            io->master->ppu.bgaffintr[1].y = data;
            break;
        default:
            io_writeh(io, addr, data);
            io_writeh(io, addr | 2, data >> 16);
    }
}