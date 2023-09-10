#include "io.h"

byte io_readb(IO* io, word addr) {
    hword h = io_readh(io, addr & ~1);
    if (addr & 1) {
        return h >> 8;
    } else return h;
}

void io_writeb(IO* io, word addr, byte data) {
    hword h;
    if(addr & 1) {
        h = data << 8;
        h |= io->b[addr & ~1];
    } else {
        h = data;
        h |= io->b[addr | 1] << 8;
    }
    io_writeh(io, addr & ~1, h);
}

hword io_readh(IO* io, word addr) {
    if(BG0HOFS <= addr && addr <= BG3Y_H) {
        return 0;
    }
    return io->h[addr >> 1];
}

void io_writeh(IO* io, word addr, hword data) {
    switch(addr) {
        case DISPSTAT:
            io->dispstat.h &= 0b111;
            io->dispstat.h |= data & ~0b111;
            break;
        case KEYINPUT:
            break;
        default:
            io->h[addr >> 1] = data;
    }
}

word io_readw(IO* io, word addr) {
    return io_readh(io, addr) | (io_readh(io, addr | 2) << 16);
}

void io_writew(IO* io, word addr, word data) {
    io_writeh(io, addr, data);
    io_writeh(io, addr | 2, data >> 16);
}