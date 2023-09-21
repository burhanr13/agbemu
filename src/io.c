#include "io.h"

#include <stdio.h>

#include "dma.h"
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
    if ((BG0HOFS <= addr && addr <= BG3Y + 2)) {
        return 0;
    }
    if (DMA0SAD <= addr && addr <= DMA3CNT_H) {
        switch (addr) {
            case DMA0CNT_H:
            case DMA1CNT_H:
            case DMA2CNT_H:
            case DMA3CNT_H:
                return io->h[addr >> 1];
            default:
                return 0;
        }
    }
    return io->h[addr >> 1];
}

void io_writeh(IO* io, word addr, hword data) {
    if ((addr & ~0b11) == BG2X || (addr & ~0b11) == BG2Y ||
        (addr & ~0b11) == BG3X || (addr & ~0b11) == BG3Y) {
        word w;
        if (addr & 0b10) {
            w = data << 16;
            w |= io->h[(addr >> 1) & ~1];
        } else {
            w = data;
            w |= io->h[(addr >> 1) | 1];
        }
        io_writew(io, addr & ~0b11, w);
        return;
    }
    switch (addr) {
        case DISPSTAT:
            io->dispstat.h &= 0b111;
            io->dispstat.h |= data & ~0b111;
            break;
        case DMA0CNT_H:
        case DMA1CNT_H:
        case DMA2CNT_H:
        case DMA3CNT_H: {
            int i = (addr - DMA0CNT_H) / (DMA1CNT_H - DMA0CNT_H);
            bool prev_ena = io->dma[i].cnt.enable;
            io->dma[i].cnt.h = data;
            if(!prev_ena && io->dma[i].cnt.enable)
                dma_enable(&io->master->dmac, i);
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
        case BG2X:
            io->bgaff[0].x = data;
            io->master->ppu.bgaffintr[0].x = data;
            break;
        case BG2Y:
            io->bgaff[0].y = data;
            io->master->ppu.bgaffintr[0].y = data;
            break;
        case BG3X:
            io->bgaff[1].x = data;
            io->master->ppu.bgaffintr[1].x = data;
            break;
        case BG3Y:
            io->bgaff[1].y = data;
            io->master->ppu.bgaffintr[1].y = data;
            break;
        default:
            io_writeh(io, addr, data);
            io_writeh(io, addr | 2, data >> 16);
    }
}