#include "gba.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arm7tdmi.h"
#include "dma.h"
#include "io.h"
#include "ppu.h"
#include "scheduler.h"
#include "timer.h"
#include "types.h"

extern bool lg, dbg, bootbios;

void init_gba(GBA* gba, Cartridge* cart, byte* bios) {
    memset(gba, 0, sizeof *gba);
    gba->cart = cart;
    gba->cpu.master = gba;
    gba->ppu.master = gba;
    gba->dmac.master = gba;
    gba->tmc.master = gba;
    gba->io.master = gba;
    gba->sched.master = gba;

    gba->bios.b = bios;

    gba->cpu.cpsr.m = M_SYSTEM;

    if (!bootbios) {
        gba->cpu.pc = 0x08000000;
        gba->cpu.banked_sp[B_SVC] = 0x3007fe0;
        gba->cpu.banked_sp[B_IRQ] = 0x3007fa0;
        gba->cpu.sp = 0x3007f00;

        gba->io.bgaff[0].pa = 1 << 8;
        gba->io.bgaff[0].pd = 1 << 8;
        gba->io.bgaff[1].pa = 1 << 8;
        gba->io.bgaff[1].pd = 1 << 8;
    }

    add_event(&gba->sched, &(Event){0, EVENT_PPU_HDRAW});
}

byte* load_bios(char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;
    byte* bios = malloc(BIOS_SIZE);

    fread(bios, 1, BIOS_SIZE, fp);
    fclose(fp);
    return bios;
}

int get_waitstates(GBA* gba, word addr, DataWidth d) {
    word region = addr >> 24;
    word cart_addr = addr % (1 << 25);
    switch (region) {
        case R_BIOS:
            return 1;
        case R_EWRAM:
            switch (d) {
                case D_BYTE:
                case D_HWORD:
                    return 3;
                case D_WORD:
                    return 6;
            }
        case R_IWRAM:
            return 1;
        case R_IO:
            return 1;
        case R_PRAM:
        case R_VRAM:
            switch (d) {
                case D_BYTE:
                case D_HWORD:
                    return 1;
                case D_WORD:
                    return 2;
            }
        case R_OAM:
            return 1;
        case R_ROM0:
        case R_ROM0EX:
        case R_ROM1:
        case R_ROM1EX:
        case R_ROM2:
        case R_ROM2EX:
            if (cart_addr < gba->cart->rom_size) {
                switch (d) {
                    case D_BYTE:
                    case D_HWORD:
                        return 5;
                    case D_WORD:
                        return 8;
                }
            } else return 1;
        case R_SRAM:
            if (cart_addr < gba->cart->ram_size) {
                return 5;
            } else return 1;
        default:
            return 1;
    }
}

byte bus_readb(GBA* gba, word addr) {
    word region = addr >> 24;
    word cart_addr = addr % (1 << 25);
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
        case R_PRAM:
            return gba->pram.b[addr % PRAM_SIZE];
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
            if (cart_addr < gba->cart->rom_size) {
                return gba->cart->rom.b[cart_addr];
            }
            break;
        case R_SRAM:
            if (cart_addr < gba->cart->ram_size) {
                return gba->cart->ram.b[cart_addr];
            }
            break;
    }
    log_error(gba, "invalid byte read", (region << 24) | addr);
    return 0;
}

hword bus_readh(GBA* gba, word addr) {
    word region = addr >> 24;
    word cart_addr = addr % (1 << 25);
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
        case R_PRAM:
            return gba->pram.h[addr % PRAM_SIZE >> 1];
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
            if (cart_addr < gba->cart->rom_size) {
                return gba->cart->rom.h[cart_addr >> 1];
            }
            break;
        case R_SRAM:
            if (cart_addr < gba->cart->ram_size) {
                return gba->cart->ram.b[cart_addr] * 0x0101;
            }
            break;
    }
    log_error(gba, "invalid hword read", (region << 24) | addr);
    return 0;
}

word bus_readw(GBA* gba, word addr) {
    word region = addr >> 24;
    word cart_addr = addr % (1 << 25);
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
        case R_PRAM:
            return gba->pram.w[addr % PRAM_SIZE >> 2];
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
            if (cart_addr < gba->cart->rom_size) {
                return gba->cart->rom.w[cart_addr >> 2];
            }
            break;
        case R_SRAM:
            if (cart_addr < gba->cart->ram_size) {
                return gba->cart->ram.b[cart_addr] * 0x01010101;
            }
            break;
    }
    log_error(gba, "invalid word read", (region << 24) | addr);
    return 0;
}

void bus_writeb(GBA* gba, word addr, byte b) {
    word region = addr >> 24;
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            log_error(gba, "invalid byte write to bios", (region << 24) | addr);
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
        case R_PRAM:
            gba->pram.h[addr % PRAM_SIZE >> 1] = b * 0x0101;
            break;
        case R_VRAM:
            addr %= 0x20000;
            if (addr > VRAM_SIZE) addr -= 0x8000;
            if (addr < 0x10000 || (addr < 0x14000 && gba->io.dispcnt.bg_mode >= 3))
                gba->vram.h[addr >> 1] = b * 0x0101;
            break;
        case R_OAM:
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
        default:
            log_error(gba, "invalid byte write", (region << 24) | addr);
    }
}

void bus_writeh(GBA* gba, word addr, hword h) {
    word region = addr >> 24;
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            log_error(gba, "invalid halfword write to bios", (region << 24) | addr);
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
        case R_PRAM:
            gba->pram.h[addr % PRAM_SIZE >> 1] = h;
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
                gba->cart->ram.b[addr] = h >> (8 * (addr & 1));
            }
            break;
        default:
            log_error(gba, "invalid halfword write", (region << 24) | addr);
    }
}

void bus_writew(GBA* gba, word addr, word w) {
    word region = addr >> 24;
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            log_error(gba, "invalid word write to bios", (region << 24) | addr);
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
        case R_PRAM:
            gba->pram.w[addr % PRAM_SIZE >> 2] = w;
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
                gba->cart->ram.b[addr] = w >> (8 * (addr & 0b11));
            }
            break;
        default:
            log_error(gba, "invalid word write", (region << 24) | addr);
    }
}

void tick_components(GBA* gba, int cycles) {
    run_scheduler(&gba->sched, cycles);
}

void gba_step(GBA* gba) {
    if (gba->dmac.any_active) {
        for (int i = 0; i < 4; i++) {
            if (gba->dmac.dma[i].active) {
                dma_step(&gba->dmac, i);
                return;
            }
        }
    }
    if (gba->io.ie.h & gba->io.ifl.h) {
        if (gba->halt || ((gba->io.ime & 1) && !gba->cpu.cpsr.i)) {
            gba->halt = false;
            cpu_handle_interrupt(&gba->cpu, I_IRQ);
            return;
        }
    }
    if (!gba->halt) {
        cpu_step(&gba->cpu);
        return;
    }
    run_next_event(&gba->sched);
}

void log_error(GBA* gba, char* mess, word addr) {
    if (lg) {
        printf("Error: %s, addr=0x%08x\n", mess, addr);
        print_cpu_state(&gba->cpu);
        if (dbg) raise(SIGINT);
    }
}