#include "gba.h"

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

const int CART_WAITS[4] = {5, 4, 3, 9};

void gba_clear_ptrs(GBA* gba) {
    gba->cart = NULL;
    gba->cpu.master = NULL;
    gba->ppu.master = NULL;
    gba->apu.master = NULL;
    gba->dmac.master = NULL;
    gba->tmc.master = NULL;
    gba->io.master = NULL;
    gba->sched.master = gba;
    gba->bios.b = NULL;
}

void gba_set_ptrs(GBA* gba, Cartridge* cart, byte* bios) {
    gba->cart = cart;
    gba->cpu.master = gba;
    gba->ppu.master = gba;
    gba->apu.master = gba;
    gba->dmac.master = gba;
    gba->tmc.master = gba;
    gba->io.master = gba;
    gba->sched.master = gba;
    gba->bios.b = bios;
}

void init_gba(GBA* gba, Cartridge* cart, byte* bios, bool bootbios) {
    memset(gba, 0, sizeof *gba);
    memset(&cart->st, 0, sizeof cart->st);

    gba_set_ptrs(gba, cart, bios);

    gba->dmac.active_dma = 4;

    update_cart_waits(gba);

    add_event(&gba->sched, EVENT_PPU_HDRAW, 0);

    if (!bootbios) {
        gba->cpu.banked_sp[B_SVC] = 0x3007fe0;
        gba->cpu.banked_sp[B_IRQ] = 0x3007fa0;
        gba->cpu.sp = 0x3007f00;

        gba->io.bgaff[0].pa = 1 << 8;
        gba->io.bgaff[0].pd = 1 << 8;
        gba->io.bgaff[1].pa = 1 << 8;
        gba->io.bgaff[1].pd = 1 << 8;
        gba->io.soundbias.bias = 0x200;

        gba->last_bios_val = 0xe129f000;

        gba->cpu.pc = 0x08000000;
        gba->cpu.cpsr.m = M_SYSTEM;
        cpu_flush(&gba->cpu);
    } else {
        cpu_handle_interrupt(&gba->cpu, I_RESET);
    }
}

byte* load_bios(char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;
    byte* bios = malloc(BIOS_SIZE);

    (void) !fread(bios, 1, BIOS_SIZE, fp);
    fclose(fp);
    return bios;
}

void update_cart_waits(GBA* gba) {
    gba->cart_n_waits[0] = CART_WAITS[gba->io.waitcnt.rom0];
    gba->cart_n_waits[1] = CART_WAITS[gba->io.waitcnt.rom1];
    gba->cart_n_waits[2] = CART_WAITS[gba->io.waitcnt.rom2];
    gba->cart_n_waits[3] = CART_WAITS[gba->io.waitcnt.sram];
    gba->cart_s_waits[0] = gba->io.waitcnt.rom0s ? 2 : 3;
    gba->cart_s_waits[1] = gba->io.waitcnt.rom1s ? 2 : 5;
    gba->cart_s_waits[2] = gba->io.waitcnt.rom2s ? 2 : 9;
}

int get_waitstates(GBA* gba, word addr, bool w, bool seq) {
    word region = addr >> 24;
    if (region < 8) {
        int waits = 1;
        if (region == R_EWRAM) waits = 3;
        if (w && (region == R_EWRAM || region == R_PRAM || region == R_VRAM)) {
            waits += waits;
        }
        if (!gba->prefetch_halted) gba->prefetcher_cycles += waits;
        return waits;
    } else if (region < 16) {
        int i = (region >> 1) & 0b11;
        if (i == 3) return gba->cart_n_waits[3];

        int n_waits = gba->cart_n_waits[i];
        int s_waits = gba->cart_s_waits[i];
        int total = 0;

        if (gba->io.waitcnt.prefetch &&
            gba->prefetcher_cycles % s_waits == s_waits - 1)
            total += 1;

        gba->next_prefetch_addr = -1;
        gba->prefetcher_cycles = 0;

        if (addr % 0x20000 == 0) seq = false;

        if (seq) {
            total += s_waits;
        } else {
            total += n_waits;
        }
        if (w) {
            total += s_waits;
        }
        return total;
    } else {
        return 1;
    }
}

int get_fetch_waitstates(GBA* gba, word addr, bool w, bool seq) {
    if (!gba->io.waitcnt.prefetch) return get_waitstates(gba, addr, w, seq);
    word region = addr >> 24;
    word rom_addr = addr % (1 << 25);
    if (region < 8) {
        int waits = 1;
        if (region == R_EWRAM) waits = 3;
        if (w && (region == R_EWRAM || region == R_PRAM || region == R_VRAM)) {
            waits += waits;
        }
        gba->prefetcher_cycles += waits;
        return waits;
    } else if (region < 16) {
        int i = (region >> 1) & 0b11;
        if (i == 3) return gba->cart_n_waits[3];

        int n_waits = gba->cart_n_waits[i];
        int s_waits = gba->cart_s_waits[i];
        int total = 0;
        if (rom_addr == gba->next_prefetch_addr) {
            if (w && gba->prefetcher_cycles >= 2 * s_waits - 1) {
                total += 1;
                gba->prefetcher_cycles -= 2 * s_waits;
                if (gba->prefetcher_cycles < 0) gba->prefetcher_cycles = 0;
                else gba->prefetcher_cycles += 1;
                gba->next_prefetch_addr += 4;
            } else {
                for (int i = 0; i < (w ? 2 : 1); i++) {
                    if (gba->prefetcher_cycles < s_waits) {
                        total += s_waits - gba->prefetcher_cycles;
                        gba->prefetcher_cycles = 0;
                    } else {
                        total += 1;
                        gba->prefetcher_cycles -= s_waits;
                        gba->prefetcher_cycles += 1;
                    }
                    gba->next_prefetch_addr += 2;
                }
            }
        } else {
            gba->prefetcher_cycles = 0;

            total += n_waits;
            gba->next_prefetch_addr = rom_addr + 2;
            if (w) {
                total += s_waits;
                gba->next_prefetch_addr += 2;
            }
        }
        return total;
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
                } else return gba->last_bios_val >> (16 * ((addr >> 1) % 2));
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
                } else return gba->last_bios_val;
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
            if (rom_addr < gba->cart->rom_size) {
                return gba->cart->rom.w[rom_addr >> 2];
            } else
                return read_rom_oob((addr & ~0b11) + 2) << 16 |
                       read_rom_oob(addr & ~0b11);
            break;
        case R_SRAM:
        case R_SRAMEX:
            return cart_read_sram(gba->cart, addr) * 0x01010101;
            break;
    }
    gba->openbus = true;
    return 0;
}

void bus_writeb(GBA* gba, word addr, byte b) {
    word region = addr >> 24;
    word rom_addr = addr % (1 << 25);
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
        case R_PRAM:
            gba->pram.h[addr % PRAM_SIZE >> 1] = b * 0x0101;
            break;
        case R_VRAM:
            addr %= 0x20000;
            if (addr >= VRAM_SIZE) addr -= 0x8000;
            if (addr < 0x10000 ||
                (addr < 0x14000 && gba->io.dispcnt.bg_mode >= 3))
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
            break;
    }
}

void bus_writeh(GBA* gba, word addr, hword h) {
    word region = addr >> 24;
    word rom_addr = addr % (1 << 25);
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
            break;
    }
}

void bus_writew(GBA* gba, word addr, word w) {
    word region = addr >> 24;
    word rom_addr = addr % (1 << 25);
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
            break;
    }
}

void bus_lock(GBA* gba) {
    gba->bus_locks++;
}

void bus_unlock(GBA* gba, int dma_prio) {
    gba->bus_locks = 0;
    for (int i = 0; i < 4 && i < dma_prio; i++) {
        if (gba->dmac.dma[i].waiting) {
            gba->dmac.dma[i].waiting = false;

            if (dma_prio == 5) {
                tick_components(gba, 1, false);
                gba->prefetcher_cycles += 1;
            }
            dma_run(&gba->dmac, i);
            if (dma_prio == 5) {
                tick_components(gba, 1, false);
                gba->prefetcher_cycles += 1;
            }
            break;
        }
    }
}

void tick_components(GBA* gba, int cycles, bool mem) {
    if (mem) {
        run_scheduler_mem(&gba->sched, cycles);
    } else {
        run_scheduler_internal(&gba->sched, cycles);
    }
}

void gba_step(GBA* gba) {
    if (gba->stop) return;

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
    while (!(gba->ppu.frame_complete || gba->apu.samples_full ||
             (gba->io.ie.h & gba->io.ifl.h)))
        run_next_event(&gba->sched);
}

void update_keypad_irq(GBA* gba) {
    if (gba->io.keycnt.irq_cond) {
        if ((~gba->io.keyinput.keys & gba->io.keycnt.keys) ==
            gba->io.keycnt.keys) {
            if (gba->io.keycnt.irq_enable) gba->io.ifl.keypad = 1;
            gba->stop = false;
        }
    } else {
        if (~gba->io.keyinput.keys & gba->io.keycnt.keys) {
            if (gba->io.keycnt.irq_enable) gba->io.ifl.keypad = 1;
            gba->stop = false;
        }
    }
}