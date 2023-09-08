#include "io.h"

byte iA_readb(IO* io, word addr) {
    hword h = iA_readh(io, addr & ~1);
    if (addr & 1) {
        return h >> 8;
    } else return h;
}

void iA_writeb(IO* io, word addr, byte data) {
    hword h;
    if(addr & 1) {
        h = data << 8;
        h |= io->b[addr & ~1];
    } else {
        h = data;
        h |= io->b[addr | 1] << 8;
    }
    iA_writeh(io, addr & ~1, h);
}

hword iA_readh(IO* io, word addr) {
    switch (addr) {
        default:
            return io->h[addr >> 1];
    }
}

void iA_writeh(IO* io, word addr, hword data) {
    switch(addr) {
        case DISPSTAT:
            io->dispstat.h &= 0b111;
            io->dispstat.h |= data & ~0b111;
            break;
        default:
            io->h[addr >> 1] = data;
    }
}

word iA_readw(IO* io, word addr) {
    return iA_readh(io, addr) | (iA_readh(io, addr | 2) << 16);
}

void iA_writew(IO* io, word addr, word data) {
    iA_writeh(io, addr, data);
    iA_writeh(io, addr | 2, data >> 16);
}