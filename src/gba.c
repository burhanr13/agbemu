#include "gba.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apu.h"
#include "arm7tdmi.h"
#include "dma.h"
#include "io.h"
#include "ppu.h"
#include "scheduler.h"
#include "timer.h"
#include "types.h"

extern bool lg, dbg, bootbios;

const int CART_WAITS[4] = {5, 4, 3, 9};

void init_gba(GBA* gba, Cartridge* cart, byte* bios) {
    memset(gba, 0, sizeof *gba);
    memset(&cart->st, 0, sizeof cart->st);
    gba->cart = cart;
    gba->cpu.master = gba;
    gba->ppu.master = gba;
    gba->apu.master = gba;
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

        gba->last_bios_val = 0xe129f000;

        gba->io.bgaff[0].pa = 1 << 8;
        gba->io.bgaff[0].pd = 1 << 8;
        gba->io.bgaff[1].pa = 1 << 8;
        gba->io.bgaff[1].pd = 1 << 8;
        gba->io.soundbias.bias = 0x200;
    }

    gba->next_rom_addr = -1;

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
    word rom_addr = addr % (1 << 25);
    if (region < 8) {
        int waits = 1;
        if (region == R_EWRAM) waits = 3;
        if (d == D_WORD && (region == R_EWRAM || region == R_PRAM || region == R_VRAM)) waits *= 2;
        return waits;
    } else if (region < 16) {
        switch (region) {
            case R_ROM0:
            case R_ROM0EX: {
                int n_waits = CART_WAITS[gba->io.waitcnt.rom0];
                int s_waits = gba->io.waitcnt.rom0s ? 2 : 3;
                int total = 0;
                if (rom_addr == gba->next_rom_addr) {
                    if (gba->io.waitcnt.prefetch) s_waits = 1;
                    total += s_waits;
                } else {
                    total += n_waits;
                }
                if (d == D_WORD) {
                    total += s_waits;
                }
                return total;
            }
            case R_ROM1:
            case R_ROM1EX: {
                int n_waits = CART_WAITS[gba->io.waitcnt.rom1];
                int s_waits = gba->io.waitcnt.rom1s ? 2 : 5;
                int total = 0;
                if (rom_addr == gba->next_rom_addr) {
                    if (gba->io.waitcnt.prefetch) s_waits = 1;
                    total += s_waits;
                } else {
                    total += n_waits;
                }
                if (d == D_WORD) {
                    total += s_waits;
                }
                return total;
            }
            case R_ROM2:
            case R_ROM2EX: {
                int n_waits = CART_WAITS[gba->io.waitcnt.rom2];
                int s_waits = gba->io.waitcnt.rom2s ? 2 : 9;
                int total = 0;
                if (rom_addr == gba->next_rom_addr) {
                    if (gba->io.waitcnt.prefetch) s_waits = 1;
                    total += s_waits;
                } else {
                    total += n_waits;
                }
                if (d == D_WORD) {
                    total += s_waits;
                }
                return total;
            }
            case R_SRAM:
            case R_SRAMEX:
                return CART_WAITS[gba->io.waitcnt.sram];
            default:
                return 1;
        }
    } else return 1;
}

static inline hword read_rom_oob(word addr) {
    return (addr >> 1) & 0xffff;
}

byte bus_readb(GBA* gba, word addr) {
    gba->openbus = false;
    word region = addr >> 24;
    word rom_addr = addr % (1 << 25);
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            if (addr < BIOS_SIZE) {
                if (gba->cpu.pc < BIOS_SIZE) {
                    gba->last_bios_val = gba->bios.w[addr >> 2];
                    return gba->bios.b[addr];
                } else return gba->last_bios_val >> (8 * (addr % 4));
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
            if (addr >= VRAM_SIZE) addr -= 0x8000;
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
            if (gba->cart->eeprom_mask &&
                (rom_addr & gba->cart->eeprom_mask) == gba->cart->eeprom_mask) {
                return cart_read_eeprom(gba->cart);
            }
            gba->next_rom_addr = (rom_addr & ~1) + 2;
            if (rom_addr < gba->cart->rom_size) {
                return gba->cart->rom.b[rom_addr];
            } else return read_rom_oob(addr);
            break;
        case R_SRAM:
        case R_SRAMEX:
            return cart_read_sram(gba->cart, addr);
            break;
    }
    gba->openbus = true;
    log_error(gba, "invalid byte read", (region << 24) | addr);
    return 0;
}

hword bus_readh(GBA* gba, word addr) {
    gba->openbus = false;
    word region = addr >> 24;
    word rom_addr = addr % (1 << 25);
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            if (addr < BIOS_SIZE) {
                if (gba->cpu.pc < BIOS_SIZE) {
                    gba->last_bios_val = gba->bios.w[addr >> 2];
                    return gba->bios.h[addr >> 1];
                } else if (!gba->dmac.any_active)
                    return gba->last_bios_val >> (16 * ((addr >> 1) % 2));
                else {
                    gba->openbus = true;
                    return 0;
                }
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
                return io_readh(&gba->io, addr);
            }
            break;
        case R_PRAM:
            return gba->pram.h[addr % PRAM_SIZE >> 1];
            break;
        case R_VRAM:
            addr %= 0x20000;
            if (addr >= VRAM_SIZE) addr -= 0x8000;
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
            if (gba->cart->eeprom_mask &&
                (rom_addr & gba->cart->eeprom_mask) == gba->cart->eeprom_mask) {
                return cart_read_eeprom(gba->cart);
            }
            gba->next_rom_addr = (rom_addr & ~1) + 2;
            if (rom_addr < gba->cart->rom_size) {
                return gba->cart->rom.h[rom_addr >> 1];
            } else return read_rom_oob(addr & ~1);
            break;
        case R_SRAM:
        case R_SRAMEX:
            return cart_read_sram(gba->cart, addr) * 0x0101;
            break;
    }
    gba->openbus = true;
    log_error(gba, "invalid hword read", (region << 24) | addr);
    return 0;
}

word bus_readw(GBA* gba, word addr) {
    gba->openbus = false;
    word region = addr >> 24;
    word rom_addr = addr % (1 << 25);
    addr %= 1 << 24;
    switch (region) {
        case R_BIOS:
            if (addr < BIOS_SIZE) {
                if (gba->cpu.pc < BIOS_SIZE) {
                    word data = gba->bios.w[addr >> 2];
                    gba->last_bios_val = data;
                    return data;
                } else if (!gba->dmac.any_active) return gba->last_bios_val;
                else {
                    gba->openbus = true;
                    return 0;
                }
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
            if (addr >= VRAM_SIZE) addr -= 0x8000;
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
            if (gba->cart->eeprom_mask &&
                (rom_addr & gba->cart->eeprom_mask) == gba->cart->eeprom_mask) {
                word dat = cart_read_eeprom(gba->cart);
                dat |= cart_read_eeprom(gba->cart) << 16;
                return dat;
            }
            gba->next_rom_addr = (rom_addr & ~0b11) + 4;
            if (rom_addr < gba->cart->rom_size) {
                return gba->cart->rom.w[rom_addr >> 2];
            } else return read_rom_oob((addr & ~0b11) + 2) << 16 | read_rom_oob(addr & ~0b11);
            break;
        case R_SRAM:
        case R_SRAMEX:
            return cart_read_sram(gba->cart, addr) * 0x01010101;
            break;
    }
    gba->openbus = true;
    log_error(gba, "invalid word read", (region << 24) | addr);
    return 0;
}

void bus_writeb(GBA* gba, word addr, byte b) {
    word region = addr >> 24;
    word rom_addr = addr % (1 << 25);
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
            if (addr >= VRAM_SIZE) addr -= 0x8000;
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
            if (gba->cart->eeprom_mask &&
                (rom_addr & gba->cart->eeprom_mask) == gba->cart->eeprom_mask) {
                cart_write_eeprom(gba->cart, b);
            }
            break;
        case R_SRAM:
        case R_SRAMEX:
            cart_write_sram(gba->cart, addr, b);
            break;
        default:
            log_error(gba, "invalid byte write", (region << 24) | addr);
    }
}

void bus_writeh(GBA* gba, word addr, hword h) {
    word region = addr >> 24;
    word rom_addr = addr % (1 << 25);
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
            if (addr >= VRAM_SIZE) addr -= 0x8000;
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
            if (gba->cart->eeprom_mask &&
                (rom_addr & gba->cart->eeprom_mask) == gba->cart->eeprom_mask) {
                cart_write_eeprom(gba->cart, h);
            }
            break;
        case R_SRAM:
        case R_SRAMEX:
            cart_write_sram(gba->cart, addr, h >> (8 * (addr & 1)));
            break;
        default:
            log_error(gba, "invalid halfword write", (region << 24) | addr);
    }
}

void bus_writew(GBA* gba, word addr, word w) {
    word region = addr >> 24;
    word rom_addr = addr % (1 << 25);
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
            if (addr >= VRAM_SIZE) addr -= 0x8000;
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
            if (gba->cart->eeprom_mask &&
                (rom_addr & gba->cart->eeprom_mask) == gba->cart->eeprom_mask) {
                cart_write_eeprom(gba->cart, w);
                cart_write_eeprom(gba->cart, w >> 16);
            }
            break;
        case R_SRAM:
        case R_SRAMEX:
            cart_write_sram(gba->cart, addr, w >> (8 * (addr & 0b11)));
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
        dma_step(&gba->dmac, gba->dmac.active_dma);
        return;
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
    while (!(gba->ppu.frame_complete || gba->apu.samples_full || gba->dmac.any_active ||
             (gba->io.ie.h & gba->io.ifl.h)))
        run_next_event(&gba->sched);
}

void log_error(GBA* gba, char* mess, word addr) {
    // if (lg) {
    //     printf("Error: %s, addr=0x%08x\n", mess, addr);
    //     print_cpu_state(&gba->cpu);
    //     if (dbg) raise(SIGINT);
    // }
}