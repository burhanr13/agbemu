#include "ppu.h"

void tick_ppu(PPU* ppu) {

}

word ppu_read(PPU* ppu, word addr) {
    switch (addr) {
        case 0: // dispcnt
            return ppu->dispcnt.h;
            break;
        default:
            return 0;
    }
}

void ppu_write(PPU* ppu, word addr, word data) {
    switch (addr) {
        case 0: // dispcnt
            ppu->dispcnt.h = data;
    }
}