#include "cartridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void eeprom_reverse_bytes(dword* eeprom, int size) {
    for (int i = 0; i < size; i++) {
        dword x = eeprom[i];
        x = (x & 0xffffffff00000000) >> 32 | (x & 0x00000000ffffffff) << 32;
        x = (x & 0xffff0000ffff0000) >> 16 | (x & 0x0000ffff0000ffff) << 16;
        x = (x & 0xff00ff00ff00ff00) >> 8 | (x & 0x00ff00ff00ff00ff) << 8;
        eeprom[i] = x;
    }
}

Cartridge* create_cartridge(char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;

    Cartridge* cart = calloc(1, sizeof *cart);

    fseek(fp, 0, SEEK_END);
    cart->rom_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    cart->rom.b = malloc(cart->rom_size + 32);
    (void) !fread(cart->rom.b, 1, cart->rom_size, fp);
    fclose(fp);

    cart->sav_type = SAV_NONE;
    cart->sav_size = 0;
    cart->eeprom_mask = 0;

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
            if (cart->rom_size > 0x1000000) {
                cart->eeprom_mask = 0x1ffff00;
            } else {
                cart->eeprom_mask = 0x1000000;
            }
            cart->eeprom_size_set = false;
            cart->eeprom_addr_len = 14;
            break;
        }
        if (!strncmp((void*) &cart->rom.w[i], "FLASH_V", 7)) {
            cart->sav_type = SAV_FLASH;
            cart->big_flash = false;
            cart->sav_size = FLASH_BK_SIZE;
            cart->flash_code = 0xd4bf;
            break;
        }
        if (!strncmp((void*) &cart->rom.w[i], "FLASH512_V", 10)) {
            cart->sav_type = SAV_FLASH;
            cart->big_flash = false;
            cart->sav_size = FLASH_BK_SIZE;
            cart->flash_code = 0xd4bf;
            break;
        }
        if (!strncmp((void*) &cart->rom.w[i], "FLASH1M_V", 9)) {
            cart->sav_type = SAV_FLASH;
            cart->big_flash = true;
            cart->sav_size = FLASH_BK_SIZE * 2;
            cart->flash_code = 0x1362;
            break;
        }
    }

    cart->rom_filename = malloc(strlen(filename) + 1);
    strcpy(cart->rom_filename, filename);
    int i = strrchr(filename, '.') - filename;
    cart->sav_filename = malloc(i + sizeof ".sav");
    strncpy(cart->sav_filename, cart->rom_filename, i);
    strcpy(cart->sav_filename + i, ".sav");
    cart->sst_filename = malloc(i + sizeof ".sst");
    strncpy(cart->sst_filename, cart->rom_filename, i);
    strcpy(cart->sst_filename + i, ".sst");

    if (cart->sav_size) {
        cart->sram = malloc(cart->sav_size);
        FILE* fp = fopen(cart->sav_filename, "rb");
        if (fp) {
            (void) !fread(cart->sram, 1, cart->sav_size, fp);
            fclose(fp);
            if (cart->sav_type == SAV_EEPROM) {
                eeprom_reverse_bytes(cart->eeprom, cart->sav_size / 8);
            }
        } else memset(cart->sram, 0xff, cart->sav_size);
    }

    return cart;
}

void destroy_cartridge(Cartridge* cart) {
    if (cart->sav_size) {
        FILE* fp = fopen(cart->sav_filename, "wb");
        if (fp) {
            if (cart->sav_type == SAV_EEPROM) {
                eeprom_reverse_bytes(cart->eeprom, cart->sav_size / 8);
            }
            fwrite(cart->sram, 1, cart->sav_size, fp);
            fclose(fp);
        }
        free(cart->sram);
    }

    free(cart->rom_filename);
    free(cart->sav_filename);
    free(cart->sst_filename);

    free(cart->rom.b);
    free(cart);
}

byte cart_read_sram(Cartridge* cart, hword addr) {
    if (cart->sav_type == SAV_SRAM) {
        return cart->sram[addr % SRAM_SIZE];
    } else if (cart->sav_type == SAV_FLASH) {
        return cart_read_flash(cart, addr);
    } else {
        return 0xff;
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
        return cart->flash_code >> 8 * (addr & 1);
    } else return cart->flash[cart->st.flash.bank][addr];
}

void cart_write_flash(Cartridge* cart, hword addr, byte b) {
    if (cart->st.flash.mode == FLASH_WRITE) {
        cart->flash[cart->st.flash.bank][addr] = b;
        cart->st.flash.mode = FLASH_IDLE;
        return;
    }
    if (cart->st.flash.mode == FLASH_BANKSEL) {
        cart->st.flash.bank = b & 1;
        cart->st.flash.mode = FLASH_IDLE;
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
                    cart->st.flash.mode = FLASH_IDLE;
                    return;
                }
                if (b == 0x30) {
                    cart->st.flash.state = 0;
                    memset(&cart->flash[cart->st.flash.bank][addr & 0xf000], 0xff, 0x1000);
                    cart->st.flash.mode = FLASH_IDLE;
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
                        cart->st.flash.mode = FLASH_IDLE;
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

void cart_set_eeprom_size(Cartridge* cart, bool big_eeprom) {
    cart->big_eeprom = big_eeprom;
    cart->sav_size = big_eeprom ? EEPROM_SIZE_L : EEPROM_SIZE_S;
    cart->eeprom_addr_len = big_eeprom ? 14 : 6;
    cart->eeprom = realloc(cart->eeprom, cart->sav_size);
    cart->eeprom_size_set = true;
}

hword cart_read_eeprom(Cartridge* cart) {
    switch (cart->st.eeprom.state) {
        case EEPROM_DATA:
            if (cart->st.eeprom.read) {
                hword data = (cart->st.eeprom.data >> (63 - cart->st.eeprom.index)) & 1;
                if (++cart->st.eeprom.index == 64) {
                    cart->st.eeprom.state = EEPROM_IDLE;
                }
                return data;
            } else return 0;
        case EEPROM_IDLE:
            return 1;
        default:
            return 0;
    }
}

void cart_write_eeprom(Cartridge* cart, hword h) {
    switch (cart->st.eeprom.state) {
        case EEPROM_IDLE:
            if (h & 1) cart->st.eeprom.state++;
            break;
        case EEPROM_BRW:
            cart->st.eeprom.read = h & 1;
            cart->st.eeprom.state++;
            cart->st.eeprom.addr = 0;
            cart->st.eeprom.index = 0;
            break;
        case EEPROM_ADDR:
            cart->st.eeprom.addr <<= 1;
            cart->st.eeprom.addr |= h & 1;
            if (++cart->st.eeprom.index == cart->eeprom_addr_len) {
                cart->st.eeprom.addr %= 1 << 10;
                if (cart->st.eeprom.read) {
                    cart->st.eeprom.data = cart->eeprom[cart->st.eeprom.addr];
                    cart->st.eeprom.index = -4;
                } else {
                    cart->st.eeprom.data = 0;
                    cart->st.eeprom.index = 0;
                }
                cart->st.eeprom.state++;
            }
            break;
        case EEPROM_DATA:
            if (!cart->st.eeprom.read) {
                cart->st.eeprom.data <<= 1;
                cart->st.eeprom.data |= h & 1;
                if (++cart->st.eeprom.index == 64) {
                    cart->eeprom[cart->st.eeprom.addr] = cart->st.eeprom.data;
                    cart->st.eeprom.state = EEPROM_IDLE;
                }
            }
            break;
    }
}