#include "gba.h"

#include <stdio.h>
#include <string.h>

#include "arm7tdmi.h"
#include "io.h"

void init_gba(GBA* gba, Cartridge* cart) {
    memset(gba, 0, sizeof *gba);
    gba->cart = cart;
    gba->cpu.master = gba;
    gba->ppu.master = gba;
    if(!gba_load_bios(gba, "bios.bin")) {
        printf("no bios file found\n");
    }

    gba->cpu.pc = 0x08000000;
    gba->cpu.cpsr.m = M_SYSTEM;
    gba->cpu.banked_sp[B_SVC] = 0x3007fe0;
    gba->cpu.banked_sp[B_IRQ] = 0x3007fa0;
    gba->cpu.sp = 0x3007f00;
}

bool gba_load_bios(GBA* gba, char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return false;

    fread(gba->bios.b, 1, BIOS_SIZE, fp);
    fclose(fp);
    return true;
}

byte gba_readb(GBA* gba, word addr, int* cycles) {
    word region = addr >> 24;
    word rom_addr = addr % (1 << 25);
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            if (addr < BIOS_SIZE) {
                return gba->bios.b[addr];
            }
            break;
        case R_EWRAM:
            return gba->ewram.b[addr % EWRAM_SIZE];
            break;
        case R_IWRAM:
            return gba->iwram.b[addr % IWRAM_SIZE];
            break;
        case R_IO:
            if (addr < IO_SIZE) {
                return io_readb(&gba->io, addr);
            }
            break;
        case R_CRAM:
            return gba->cram.b[addr % CRAM_SIZE];
            break;
        case R_VRAM:
            addr %= 0x20000;
            if (addr > VRAM_SIZE) addr -= 0x8000;
            return gba->vram.b[addr];
            break;
        case R_OAM:
            return gba->oam.b[addr % OAM_SIZE];
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
    word rom_addr = addr % (1 << 25);
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            if (addr < BIOS_SIZE) {
                return gba->bios.h[addr >> 1];
            }
            break;
        case R_EWRAM:
            return gba->ewram.h[addr % EWRAM_SIZE >> 1];
            break;
        case R_IWRAM:
            return gba->iwram.h[addr % IWRAM_SIZE >> 1];
            break;
        case R_IO:
            if (addr < IO_SIZE) {
                return io_readh(&gba->io, addr & ~1);
            }
            break;
        case R_CRAM:
            return gba->cram.h[addr % CRAM_SIZE >> 1];
            break;
        case R_VRAM:
            addr %= 0x20000;
            if (addr > VRAM_SIZE) addr -= 0x8000;
            return gba->vram.h[addr >> 1];
            break;
        case R_OAM:
            return gba->oam.h[addr % OAM_SIZE >> 1];
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
    word rom_addr = addr % (1 << 25);
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            if (addr < BIOS_SIZE) {
                return gba->bios.w[addr >> 2];
            }
            break;
        case R_EWRAM:
            return gba->ewram.w[addr % EWRAM_SIZE >> 2];
            break;
        case R_IWRAM:
            return gba->iwram.w[addr % IWRAM_SIZE >> 2];
            break;
        case R_IO:
            if (addr < IO_SIZE) {
                return io_readw(&gba->io, addr & ~0b11);
            }
            break;
        case R_CRAM:
            return gba->cram.w[addr % CRAM_SIZE >> 2];
            break;
        case R_VRAM:
            addr %= 0x20000;
            if (addr > VRAM_SIZE) addr -= 0x8000;
            return gba->vram.w[addr >> 2];
            break;
        case R_OAM:
            return gba->oam.w[addr % OAM_SIZE >> 2];
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
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            break;
        case R_EWRAM:
            gba->ewram.b[addr % EWRAM_SIZE] = b;
            break;
        case R_IWRAM:
            gba->iwram.b[addr % IWRAM_SIZE] = b;
            break;
        case R_IO:
            if (addr < IO_SIZE) {
                io_writeb(&gba->io, addr, b);
            }
            break;
        case R_CRAM:
            gba->cram.b[addr % CRAM_SIZE] = b;
            break;
        case R_VRAM:
            addr %= 0x20000;
            if (addr > VRAM_SIZE) addr -= 0x8000;
            gba->vram.b[addr] = b;
            break;
        case R_OAM:
            gba->oam.b[addr % OAM_SIZE] = b;
            break;
        case R_ROM0:
        case R_ROM0EX:
        case R_ROM1:
        case R_ROM1EX:
        case R_ROM2:
        case R_ROM2EX:
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
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            break;
        case R_EWRAM:
            gba->ewram.h[addr % EWRAM_SIZE >> 1] = h;
            break;
        case R_IWRAM:
            gba->iwram.h[addr % IWRAM_SIZE >> 1] = h;
            break;
        case R_IO:
            if (addr < IO_SIZE) {
                io_writeh(&gba->io, addr & ~1, h);
            }
            break;
        case R_CRAM:
            gba->cram.h[addr % CRAM_SIZE >> 1] = h;
            break;
        case R_VRAM:
            addr %= 0x20000;
            if (addr > VRAM_SIZE) addr -= 0x8000;
            gba->vram.h[addr >> 1] = h;
            break;
        case R_OAM:
            gba->oam.h[addr % OAM_SIZE >> 1] = h;
            break;
        case R_ROM0:
        case R_ROM0EX:
        case R_ROM1:
        case R_ROM1EX:
        case R_ROM2:
        case R_ROM2EX:
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
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            break;
        case R_EWRAM:
            gba->ewram.w[addr % EWRAM_SIZE >> 2] = w;
            break;
        case R_IWRAM:
            gba->iwram.w[addr % IWRAM_SIZE >> 2] = w;
            break;
        case R_IO:
            if (addr < IO_SIZE) {
                io_writew(&gba->io, addr & ~0b11, w);
            }
            break;
        case R_CRAM:
            gba->cram.w[addr % CRAM_SIZE >> 2] = w;
            break;
        case R_VRAM:
            addr %= 0x20000;
            if (addr > VRAM_SIZE) addr -= 0x8000;
            gba->vram.w[addr >> 2] = w;
            break;
        case R_OAM:
            gba->oam.w[addr % OAM_SIZE >> 2] = w;
            break;
        case R_ROM0:
        case R_ROM0EX:
        case R_ROM1:
        case R_ROM1EX:
        case R_ROM2:
        case R_ROM2EX:
            break;
        case R_SRAM:
            if (addr < gba->cart->ram_size) {
                gba->cart->ram.w[addr >> 2] = w;
            }
            break;
    }
}

void tick_gba(GBA* gba) {
    if (gba->cycles % 4 == 0) tick_ppu(&gba->ppu);
    tick_cpu(&gba->cpu);

    gba->cycles++;
}