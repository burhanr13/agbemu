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

    union {
        byte* b;
    } ram;
    int ram_size;
} Cartridge;

Cartridge* create_cartridge(char* filename);
void destroy_cartridge(Cartridge* cart);

#endif