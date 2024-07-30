#include "dma.h"

#include "gba.h"

#include <stdio.h>

void dma_enable(DMAController* dmac, int i) {
    dmac->dma[i].sptr = dmac->master->io.dma[i].sad;
    dmac->dma[i].dptr = dmac->master->io.dma[i].dad;
    if (dmac->master->io.dma[i].cnt.wsize) {
        dmac->dma[i].sptr &= ~0b11;
        dmac->dma[i].dptr &= ~0b11;
    } else {
        dmac->dma[i].sptr &= ~1;
        dmac->dma[i].dptr &= ~1;
    }
    dmac->dma[i].ct = dmac->master->io.dma[i].ct;
    if (i < 3) {
        dmac->dma[i].ct %= 0x4000;
        if (dmac->dma[i].ct == 0) dmac->dma[i].ct = 0x4000;
    } else if (dmac->master->cart->eeprom_mask &&
               !dmac->master->cart->eeprom_size_set &&
               (dmac->dma[i].sptr & dmac->master->cart->eeprom_mask) ==
                   dmac->master->cart->eeprom_mask) {
        if (dmac->dma[i].ct == 9) {
            cart_set_eeprom_size(dmac->master->cart, false);
        }
        if (dmac->dma[i].ct == 17) {
            cart_set_eeprom_size(dmac->master->cart, true);
        }
    }

    if (i > 0) dmac->dma[i].sptr %= 1 << 28;
    else dmac->dma[i].sptr %= 1 << 27;
    if (i < 3) dmac->dma[i].dptr %= 1 << 27;
    else dmac->dma[i].dptr %= 1 << 28;

    if (dmac->master->io.dma[i].cnt.start == DMA_ST_IMM) {
        add_event(&dmac->master->sched, EVENT_DMA0 + i,
                  dmac->master->sched.now + 2);
    }
}

void dma_activate(DMAController* dmac, int i) {
    if (!dmac->master->io.dma[i].cnt.enable) return;

    if (dmac->master->io.dma[i].cnt.dadcnt == DMA_ADCNT_INR) {
        dmac->dma[i].dptr = dmac->master->io.dma[i].dad;
        if (dmac->master->io.dma[i].cnt.wsize) {
            dmac->dma[i].dptr &= ~0b11;
        } else {
            dmac->dma[i].dptr &= ~1;
        }
    }
    if (dmac->dma[i].sound) dmac->dma[i].ct = 4;
    else dmac->dma[i].ct = dmac->master->io.dma[i].ct;
    if (i < 3) {
        dmac->dma[i].ct %= 0x4000;
        if (dmac->dma[i].ct == 0) dmac->dma[i].ct = 0x4000;
    }

    if (i > 0) dmac->dma[i].sptr %= 1 << 28;
    else dmac->dma[i].sptr %= 1 << 27;
    if (i < 3) dmac->dma[i].dptr %= 1 << 27;
    else dmac->dma[i].dptr %= 1 << 28;

    add_event(&dmac->master->sched, EVENT_DMA0 + i,
              dmac->master->sched.now + 2);
}

void update_addr(word* addr, int adcnt, int wsize) {
    if (*addr >> 24 >= R_ROM0 && *addr >> 24 < R_SRAM) {
        *addr += wsize;
        return;
    }

    switch (adcnt) {
        case DMA_ADCNT_INC:
        case DMA_ADCNT_INR:
            (*addr) += wsize;
            break;
        case DMA_ADCNT_DEC:
            (*addr) -= wsize;
            break;
    }
}

void dma_run(DMAController* dmac, int i) {
    if (dmac->master->bus_locks || i > dmac->active_dma) {
        dmac->dma[i].waiting = true;
        return;
    }

    dmac->dma[i].initial = true;
    do {
        dmac->active_dma = i;
        if (dmac->master->io.dma[i].cnt.wsize || dmac->dma[i].sound) {
            dma_transw(dmac, i, dmac->dma[i].dptr, dmac->dma[i].sptr);
        } else {
            dma_transh(dmac, i, dmac->dma[i].dptr, dmac->dma[i].sptr);
        }
        update_addr(&dmac->dma[i].sptr, dmac->master->io.dma[i].cnt.sadcnt,
                    2 << dmac->master->io.dma[i].cnt.wsize);
        if (!dmac->dma[i].sound)
            update_addr(&dmac->dma[i].dptr, dmac->master->io.dma[i].cnt.dadcnt,
                        2 << dmac->master->io.dma[i].cnt.wsize);
        dmac->dma[i].initial = false;
    } while (--dmac->dma[i].ct > 0);
    dmac->master->prefetch_halted = false;

    dmac->dma[i].sound = false;
    dmac->active_dma = 4;

    if (!dmac->master->io.dma[i].cnt.repeat) {
        dmac->master->io.dma[i].cnt.enable = 0;
    }

    if (dmac->master->io.dma[i].cnt.irq) dmac->master->io.ifl.dma |= (1 << i);

    bus_unlock(dmac->master, 4);
}

void dma_transh(DMAController* dmac, int i, word daddr, word saddr) {
    bus_lock(dmac->master);
    tick_components(
        dmac->master,
        get_waitstates(dmac->master, saddr, false, !dmac->dma[i].initial),
        true);
    hword data = bus_readh(dmac->master, saddr);
    if (dmac->master->openbus || saddr < BIOS_SIZE) data = dmac->dma[i].bus_val;
    else {
        dmac->dma[i].bus_val = data * 0x00010001;
    }
    dmac->master->cpu.bus_val = data * 0x00010001;
    dmac->master->prefetch_halted = true;
    tick_components(dmac->master,
                    get_waitstates(dmac->master, daddr, false,
                                   !dmac->dma[i].initial || (saddr & 1 << 27)),
                    true);
    bus_writeh(dmac->master, daddr, data);
    bus_unlock(dmac->master, i);
}

void dma_transw(DMAController* dmac, int i, word daddr, word saddr) {
    bus_lock(dmac->master);
    tick_components(
        dmac->master,
        get_waitstates(dmac->master, saddr, true, !dmac->dma[i].initial), true);
    word data = bus_readw(dmac->master, saddr);
    if (dmac->master->openbus || saddr < BIOS_SIZE) data = dmac->dma[i].bus_val;
    else dmac->dma[i].bus_val = data;
    dmac->master->cpu.bus_val = data;
    dmac->master->prefetch_halted = true;
    tick_components(dmac->master,
                    get_waitstates(dmac->master, daddr, true,
                                   !dmac->dma[i].initial || (saddr & 1 << 27)),
                    true);
    bus_writew(dmac->master, daddr, data);
    bus_unlock(dmac->master, i);
}
