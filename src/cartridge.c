#include "cartridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Cartridge* create_cartridge(char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;

    Cartridge* cart = calloc(1, sizeof *cart);

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

    if (cart->big_flash) {
        cart->st.flash.code = 0x1362;
    } else {
        cart->st.flash.code = 0xd4bf;
    }

    cart->rom_filename = malloc(strlen(filename) + 1);
    strcpy(cart->rom_filename, filename);
    int i = strrchr(filename, '.') - filename;
    cart->sav_filename = malloc(i + sizeof ".sav");
    strcpy(cart->sav_filename + i, ".sav");

    if (cart->sav_size) {
        cart->sram = malloc(cart->sav_size);
        memset(cart->sram, 0xff, cart->sav_size);
    }

    return cart;
}

void destroy_cartridge(Cartridge* cart) {
    if (cart->sav_size) free(cart->sram);

    free(cart->rom_filename);
    free(cart->sav_filename);

    free(cart->rom.b);
    free(cart);
}

byte cart_read_sram(Cartridge* cart, hword addr) {
    if (cart->sav_type == SAV_SRAM) {
        return cart->sram[addr % SRAM_SIZE];
    } else if (cart->sav_type == SAV_FLASH) {
        return cart_read_flash(cart, addr);
    } else {
        return 0;
    }
}

void cart_write_sram(Cartridge* cart, hword addr, byte b) {
    if (cart->sav_type == SAV_SRAM) {
        cart->sram[addr % SRAM_SIZE] = b;
    } else if (cart->sav_type == SAV_FLASH) {
        cart_write_flash(cart, addr, b);
    }
}

byte cart_read_flash(Cartridge* cart, hword addr) {
    if (cart->st.flash.mode == FLASH_ID) {
        return cart->st.flash.code >> 8 * (addr & 1);
    } else return cart->flash[cart->st.flash.bank][addr];
}

void cart_write_flash(Cartridge* cart, hword addr, byte b) {
    if (cart->st.flash.mode == FLASH_WRITE) {
        cart->flash[cart->st.flash.bank][addr] = b;
        cart->st.flash.mode = FLASH_NORM;
        return;
    }
    if (cart->st.flash.mode == FLASH_BANKSEL) {
        cart->st.flash.bank = b & 1;
        cart->st.flash.mode = FLASH_NORM;
        return;
    }
    switch (cart->st.flash.state) {
        case 0:
            if (addr == 0x5555 && b == 0xaa) cart->st.flash.state = 1;
            break;
        case 1:
            if (addr == 0x2aaa && b == 0x55) cart->st.flash.state = 2;
            break;
        case 2:
            if (cart->st.flash.mode == FLASH_ERASE) {
                if (addr == 0x5555 && b == 0x10) {
                    cart->st.flash.state = 0;
                    memset(cart->flash, 0xff, cart->sav_size);
                    cart->st.flash.mode = FLASH_NORM;
                    return;
                }
                if (b == 0x30) {
                    cart->st.flash.state = 0;
                    memset(&cart->flash[cart->st.flash.bank][addr & 0xf000], 0xff, 0x1000);
                    cart->st.flash.mode = FLASH_NORM;
                    return;
                }
            }
            if (addr == 0x5555) {
                cart->st.flash.state = 0;
                switch (b) {
                    case 0x90:
                        cart->st.flash.mode = FLASH_ID;
                        break;
                    case 0xf0:
                        cart->st.flash.mode = FLASH_NORM;
                        break;
                    case 0x80:
                        cart->st.flash.mode = FLASH_ERASE;
                        break;
                    case 0xa0:
                        cart->st.flash.mode = FLASH_WRITE;
                        break;
                    case 0xb0:
                        if (cart->big_flash) cart->st.flash.mode = FLASH_BANKSEL;
                        break;
                }
            }
            break;
    }
}

hword cart_read_eeprom(Cartridge* cart) {
    return 0;
}

void cart_write_eeprom(Cartridge* cart, hword h) {}