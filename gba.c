#include "gba.h"

#include <stdio.h>
#include <string.h>

#include "arm7tdmi.h"

void init_gba(GBA* gba, Cartridge* cart) {
    memset(gba, 0, sizeof *gba);
    gba->cart = cart;
    gba->cpu.master = gba;

    gba->cpu.pc = 0x08000000;
}

void gba_load_bios(GBA* gba, char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return;

    fread(gba->bios.b, 1, BIOS_SIZE, fp);
    fclose(fp);
}

byte gba_readb(GBA* gba, word addr, int* cycles) {
    word region = addr >> 24;
    word rom_addr = addr % 1 << 27;
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            if (addr < BIOS_SIZE) {
                return gba->bios.b[addr];
            }
            break;
        case R_EWRAM:
            if (addr < EWRAM_SIZE) {
                return gba->ewram.b[addr];
            }
            break;
        case R_IWRAM:
            if (addr < IWRAM_SIZE) {
                return gba->iwram.b[addr];
            }
            break;
        case R_IO:

            break;
        case R_CRAM:
            if (addr < CRAM_SIZE) {
                return gba->cram.b[addr];
            }
            break;
        case R_VRAM:
            if (addr < VRAM_SIZE) {
                return gba->vram.b[addr];
            }
            break;
        case R_OAM:
            if (addr < OAM_SIZE) {
                return gba->oam.b[addr];
            }
            break;
        case R_ROM0:
        case R_ROM0EX:
        case R_ROM1:
        case R_ROM1EX:
        case R_ROM2:
        case R_ROM2EX:
            if (rom_addr < gba->cart->rom_size) {
                return gba->cart->rom.b[rom_addr];
            }
            break;
        case R_SRAM:
            if (addr < gba->cart->ram_size) {
                return gba->cart->ram.b[addr];
            }
            break;
    }
    return 0;
}

hword gba_readh(GBA* gba, word addr, int* cycles) {
    word region = addr >> 24;
    word rom_addr = addr % 1 << 27;
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            if (addr < BIOS_SIZE) {
                return gba->bios.h[addr >> 1];
            }
            break;
        case R_EWRAM:
            if (addr < EWRAM_SIZE) {
                return gba->ewram.h[addr >> 1];
            }
            break;
        case R_IWRAM:
            if (addr < IWRAM_SIZE) {
                return gba->iwram.h[addr >> 1];
            }
            break;
        case R_IO:

            break;
        case R_CRAM:
            if (addr < CRAM_SIZE) {
                return gba->cram.h[addr >> 1];
            }
            break;
        case R_VRAM:
            if (addr < VRAM_SIZE) {
                return gba->vram.h[addr >> 1];
            }
            break;
        case R_OAM:
            if (addr < OAM_SIZE) {
                return gba->oam.h[addr >> 1];
            }
            break;
        case R_ROM0:
        case R_ROM0EX:
        case R_ROM1:
        case R_ROM1EX:
        case R_ROM2:
        case R_ROM2EX:
            if (rom_addr < gba->cart->rom_size) {
                return gba->cart->rom.h[rom_addr >> 1];
            }
            break;
        case R_SRAM:
            if (addr < gba->cart->ram_size) {
                return gba->cart->ram.h[addr >> 1];
            }
            break;
    }
    return 0;
}

word gba_read(GBA* gba, word addr, int* cycles) {
    word region = addr >> 24;
    word rom_addr = addr % 1 << 27;
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            if (addr < BIOS_SIZE) {
                return gba->bios.w[addr >> 2];
            }
            break;
        case R_EWRAM:
            if (addr < EWRAM_SIZE) {
                return gba->ewram.w[addr >> 2];
            }
            break;
        case R_IWRAM:
            if (addr < IWRAM_SIZE) {
                return gba->iwram.w[addr >> 2];
            }
            break;
        case R_IO:

            break;
        case R_CRAM:
            if (addr < CRAM_SIZE) {
                return gba->cram.w[addr >> 2];
            }
            break;
        case R_VRAM:
            if (addr < VRAM_SIZE) {
                return gba->vram.w[addr >> 2];
            }
            break;
        case R_OAM:
            if (addr < OAM_SIZE) {
                return gba->oam.w[addr >> 2];
            }
            break;
        case R_ROM0:
        case R_ROM0EX:
        case R_ROM1:
        case R_ROM1EX:
        case R_ROM2:
        case R_ROM2EX:
            if (rom_addr < gba->cart->rom_size) {
                return gba->cart->rom.w[rom_addr >> 2];
            }
            break;
        case R_SRAM:
            if (addr < gba->cart->ram_size) {
                return gba->cart->ram.w[addr >> 2];
            }
            break;
    }
    return 0;
}

void gba_writeb(GBA* gba, word addr, byte b, int* cycles) {
    word region = addr >> 24;
    word rom_addr = addr % 1 << 27;
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            if (addr < BIOS_SIZE) {
                gba->bios.b[addr] = b;
            }
            break;
        case R_EWRAM:
            if (addr < EWRAM_SIZE) {
                gba->ewram.b[addr] = b;
            }
            break;
        case R_IWRAM:
            if (addr < IWRAM_SIZE) {
                gba->iwram.b[addr] = b;
            }
            break;
        case R_IO:

            break;
        case R_CRAM:
            if (addr < CRAM_SIZE) {
                gba->cram.b[addr] = b;
            }
            break;
        case R_VRAM:
            if (addr < VRAM_SIZE) {
                gba->vram.b[addr] = b;
            }
            break;
        case R_OAM:
            if (addr < OAM_SIZE) {
                gba->oam.b[addr] = b;
            }
            break;
        case R_ROM0:
        case R_ROM0EX:
        case R_ROM1:
        case R_ROM1EX:
        case R_ROM2:
        case R_ROM2EX:
            if (rom_addr < gba->cart->rom_size) {
                gba->cart->rom.b[rom_addr] = b;
            }
            break;
        case R_SRAM:
            if (addr < gba->cart->ram_size) {
                gba->cart->ram.b[addr] = b;
            }
            break;
    }
}

void gba_writeh(GBA* gba, word addr, hword h, int* cycles) {
    word region = addr >> 24;
    word rom_addr = addr % 1 << 27;
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            if (addr < BIOS_SIZE) {
                gba->bios.h[addr >> 1] = h;
            }
            break;
        case R_EWRAM:
            if (addr < EWRAM_SIZE) {
                gba->ewram.h[addr >> 1] = h;
            }
            break;
        case R_IWRAM:
            if (addr < IWRAM_SIZE) {
                gba->iwram.h[addr >> 1] = h;
            }
            break;
        case R_IO:

            break;
        case R_CRAM:
            if (addr < CRAM_SIZE) {
                gba->cram.h[addr >> 1] = h;
            }
            break;
        case R_VRAM:
            if (addr < VRAM_SIZE) {
                gba->vram.h[addr >> 1] = h;
            }
            break;
        case R_OAM:
            if (addr < OAM_SIZE) {
                gba->oam.h[addr >> 1] = h;
            }
            break;
        case R_ROM0:
        case R_ROM0EX:
        case R_ROM1:
        case R_ROM1EX:
        case R_ROM2:
        case R_ROM2EX:
            if (rom_addr < gba->cart->rom_size) {
                gba->cart->rom.h[rom_addr >> 1] = h;
            }
            break;
        case R_SRAM:
            if (addr < gba->cart->ram_size) {
                gba->cart->ram.h[addr >> 1] = h;
            }
            break;
    }
}

void gba_write(GBA* gba, word addr, word w, int* cycles) {
    word region = addr >> 24;
    word rom_addr = addr % 1 << 27;
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            if (addr < BIOS_SIZE) {
                gba->bios.w[addr >> 2] = w;
            }
            break;
        case R_EWRAM:
            if (addr < EWRAM_SIZE) {
                gba->ewram.w[addr >> 2] = w;
            }
            break;
        case R_IWRAM:
            if (addr < IWRAM_SIZE) {
                gba->iwram.w[addr >> 2] = w;
            }
            break;
        case R_IO:

            break;
        case R_CRAM:
            if (addr < CRAM_SIZE) {
                gba->cram.w[addr >> 2] = w;
            }
            break;
        case R_VRAM:
            if (addr < VRAM_SIZE) {
                gba->vram.w[addr >> 2] = w;
            }
            break;
        case R_OAM:
            if (addr < OAM_SIZE) {
                gba->oam.w[addr >> 2] = w;
            }
            break;
        case R_ROM0:
        case R_ROM0EX:
        case R_ROM1:
        case R_ROM1EX:
        case R_ROM2:
        case R_ROM2EX:
            if (rom_addr < gba->cart->rom_size) {
                gba->cart->rom.w[rom_addr >> 2] = w;
            }
            break;
        case R_SRAM:
            if (addr < gba->cart->ram_size) {
                gba->cart->ram.w[addr >> 2] = w;
            }
            break;
    }
}

void tick_gba(GBA* gba) {
    cpu_tick(&gba->cpu);
}