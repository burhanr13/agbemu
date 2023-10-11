#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "types.h"

typedef enum { SAV_NONE, SAV_SRAM, SAV_FLASH, SAV_EEPROM } SavType;

typedef enum { FLASH_NORM, FLASH_ID, FLASH_ERASE, FLASH_WRITE, FLASH_BANKSEL } FlashMode;

enum {
    SRAM_SIZE = 1 << 15,
    FLASH_BK_SIZE = 1 << 16,
    EEPROM_SIZE_S = 1 << 9,
    EEPROM_SIZE_L = 1 << 13
};

typedef struct {

    char* rom_filename;
    char* sav_filename;

    union {
        byte* b;
        hword* h;
        word* w;
    } rom;
    int rom_size;

    SavType sav_type;
    int sav_size;
    union {
        byte* sram;
        byte (*flash)[FLASH_BK_SIZE];
        dword* eeprom;
    };

    union {
        bool big_flash;
        bool big_eeprom;
    };

    union {
        struct {
            FlashMode mode;
            int state;
            int bank;
        } flash;
    } st;

} Cartridge;

Cartridge* create_cartridge(char* filename);
void destroy_cartridge(Cartridge* cart);

byte cart_read_sram(Cartridge* cart, hword addr);
void cart_write_sram(Cartridge* cart, hword addr, byte b);

byte cart_read_flash(Cartridge* cart, hword addr);
void cart_write_flash(Cartridge* cart, hword addr, byte b);

hword cart_read_eeprom(Cartridge* cart);
void cart_write_eeprom(Cartridge* cart, hword h);

#endif