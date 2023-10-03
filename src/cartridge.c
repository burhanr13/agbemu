#include "cartridge.h"

#include <stdio.h>
#include <stdlib.h>

Cartridge* create_cartridge(char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;

    Cartridge* cart = malloc(sizeof *cart);

    fseek(fp, 0, SEEK_END);
    cart->rom_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    cart->rom.b = malloc(cart->rom_size);
    fread(cart->rom.b, 1, cart->rom_size, fp);
    fclose(fp);
    cart->ram.b = NULL;
    cart->ram_size = 0;

    return cart;
}

void destroy_cartridge(Cartridge* cart) {
    free(cart->rom.b);
    free(cart->ram.b);
    free(cart);
}