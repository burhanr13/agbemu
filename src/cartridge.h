#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "types.h"

typedef struct {
    union {
        byte* b;
        hword* h;
        word* w;
    } rom;
    int rom_size;

} Cartridge;

Cartridge* create_cartridge(char* filename);
void destroy_cartridge(Cartridge* cart);



byte cart_read_sram(Cartridge* cart, hword addr);
void cart_write_sram(Cartridge* cart, hword addr, byte b);

#endif