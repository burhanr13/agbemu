#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "types.h"

typedef enum { SAV_NONE, SAV_SRAM, SAV_FLASH, SAV_EEPROM } SavType;

typedef enum { FLASH_IDLE, FLASH_ID, FLASH_ERASE, FLASH_WRITE, FLASH_BANKSEL } FlashMode;

typedef enum { EEPROM_IDLE, EEPROM_BRW, EEPROM_ADDR, EEPROM_DATA } EEPROMState;

enum {
    SRAM_SIZE = 1 << 15,
    FLASH_BK_SIZE = 1 << 16,
    EEPROM_SIZE_S = 1 << 9,
    EEPROM_SIZE_L = 1 << 13
};

typedef struct {

    char* rom_filename;
    char* sav_filename;
    char* sst_filename;

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

    word eeprom_mask;

    union {
        struct {
            bool big_flash;
            hword flash_code;
        };
        struct {
            bool big_eeprom;
            bool eeprom_size_set;
            byte eeprom_addr_len;
        };
    };

    union {
        struct {
            FlashMode mode;
            byte state;
            byte bank;
        } flash;
        struct {
            EEPROMState state;
            dword data;
            hword addr;
            byte index;
            bool read;
        } eeprom;
    } st;

} Cartridge;

Cartridge* create_cartridge(char* filename);
void destroy_cartridge(Cartridge* cart);

byte cart_read_sram(Cartridge* cart, hword addr);
void cart_write_sram(Cartridge* cart, hword addr, byte b);

byte cart_read_flash(Cartridge* cart, hword addr);
void cart_write_flash(Cartridge* cart, hword addr, byte b);

void cart_set_eeprom_size(Cartridge* cart, bool big_eeprom);

hword cart_read_eeprom(Cartridge* cart);
void cart_write_eeprom(Cartridge* cart, hword h);

#endif