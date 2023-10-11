#include "cartridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Cartridge* create_cartridge(char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;

    Cartridge* cart = malloc(sizeof *cart);

    fseek(fp, 0, SEEK_END);
    cart->rom_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    cart->rom.b = malloc(cart->rom_size + 32);
    fread(cart->rom.b, 1, cart->rom_size, fp);
    fclose(fp);

    cart->sav_type = SAV_NONE;
    cart->sav_size = 0;

    for (int i = 0; i < cart->rom_size >> 2; i++) {
        if (!strncmp((void*) &cart->rom.w[i], "SRAM_V", 6)) {
            cart->sav_type = SAV_SRAM;
            cart->sav_size = SRAM_SIZE;
            break;
        }
        if (!strncmp((void*) &cart->rom.w[i], "EEPROM_V", 8)) {
            cart->sav_type = SAV_EEPROM;
            cart->big_eeprom = true;
            cart->sav_size = EEPROM_SIZE_L;
            break;
        }
        if (!strncmp((void*) &cart->rom.w[i], "FLASH_V", 7)) {
            cart->sav_type = SAV_FLASH;
            cart->big_flash = false;
            cart->sav_size = FLASH_BK_SIZE;
            break;
        }
        if (!strncmp((void*) &cart->rom.w[i], "FLASH512_V", 10)) {
            cart->sav_type = SAV_FLASH;
            cart->big_flash = false;
            cart->sav_size = FLASH_BK_SIZE;
            break;
        }
        if (!strncmp((void*) &cart->rom.w[i], "FLASH1M_V", 9)) {
            cart->sav_type = SAV_FLASH;
            cart->big_flash = true;
            cart->sav_size = FLASH_BK_SIZE * 2;
            break;
        }
    }

    if (cart->sav_size) cart->sram = malloc(cart->sav_size);

    return cart;
}

void destroy_cartridge(Cartridge* cart) {
    if (cart->sav_size) free(cart->sram);

    free(cart->rom.b);
    free(cart);
}

byte cart_read_sram(Cartridge* cart, hword addr) {
    if (cart->sav_type == SAV_SRAM) {
        return cart->sram[addr % SRAM_SIZE];
    } else if (cart->sav_type == SAV_FLASH) {
        return 0;
    } else {
        return 0;
    }
}

void cart_write_sram(Cartridge* cart, hword addr, byte b) {
    if (cart->sav_type == SAV_SRAM) {
        cart->sram[addr % SRAM_SIZE] = b;
    }
}