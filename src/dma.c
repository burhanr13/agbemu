#include "dma.h"

#include "gba.h"

#include <stdio.h>

extern bool lg;

void dma_enable(DMAController* dmac, int i) {
    dmac->dma[i].active = false;
    dma_update_active(dmac);
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
    } else if (dmac->master->cart->eeprom_mask && !dmac->master->cart->eeprom_size_set &&
               (dmac->dma[i].sptr & dmac->master->cart->eeprom_mask) ==
                   dmac->master->cart->eeprom_mask) {
        if (dmac->dma[i].ct == 9) {
            cart_set_eeprom_size(dmac->master->cart, false);
        }
        if (dmac->dma[i].ct == 17) {
            cart_set_eeprom_size(dmac->master->cart, true);
        }
    }

    if (dmac->master->io.dma[i].cnt.start == DMA_ST_IMM) {
        dmac->dma[i].active = true;
        dma_update_active(dmac);
        tick_components(dmac->master, 2);
        if ((dmac->dma[i].sptr & (1 << 27)) && (dmac->dma[i].dptr & (1 << 27)))
            tick_components(dmac->master, 2);
    }
}

void dma_activate(DMAController* dmac, int i) {
    if (!dmac->master->io.dma[i].cnt.enable || dmac->dma[i].active) return;
    dmac->dma[i].active = true;
    dma_update_active(dmac);
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

    tick_components(dmac->master, 2);
    if ((dmac->dma[i].sptr & (1 << 27)) && (dmac->dma[i].dptr & (1 << 27)))
        tick_components(dmac->master, 2);
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

void dma_step(DMAController* dmac, int i) {
    if (i > 0) dmac->dma[i].sptr %= 1 << 28;
    else dmac->dma[i].sptr %= 1 << 27;
    if (i < 3) dmac->dma[i].dptr %= 1 << 27;
    else dmac->dma[i].dptr %= 1 << 28;

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
    dmac->dma[i].ct--;
    if (dmac->dma[i].ct == 0) {
        dmac->dma[i].active = false;
        dma_update_active(dmac);
        dmac->dma[i].sound = false;

        if (!dmac->master->io.dma[i].cnt.repeat) {
            dmac->master->io.dma[i].cnt.enable = 0;
        }

        if (dmac->master->io.dma[i].cnt.irq) dmac->master->io.ifl.dma |= (1 << i);
    }
}

void dma_transh(DMAController* dmac, int i, word daddr, word saddr) {
    tick_components(dmac->master, get_waitstates(dmac->master, saddr, D_HWORD));
    hword data = bus_readh(dmac->master, saddr);
    if (dmac->master->openbus || saddr < BIOS_SIZE) data = dmac->dma[i].bus_val;
    else {
        dmac->dma[i].bus_val = data * 0x00010001;
    }
    tick_components(dmac->master, get_waitstates(dmac->master, daddr, D_HWORD));
    bus_writeh(dmac->master, daddr, data);
}

void dma_transw(DMAController* dmac, int i, word daddr, word saddr) {
    tick_components(dmac->master, get_waitstates(dmac->master, saddr, D_WORD));
    word data = bus_readw(dmac->master, saddr);
    if (dmac->master->openbus || saddr < BIOS_SIZE) data = dmac->dma[i].bus_val;
    else dmac->dma[i].bus_val = data;
    tick_components(dmac->master, get_waitstates(dmac->master, daddr, D_WORD));
    bus_writew(dmac->master, daddr, data);
}

void dma_update_active(DMAController* dmac) {
    for (int i = 0; i < 4; i++) {
        if (dmac->dma[i].active) {
            dmac->any_active = true;
            dmac->active_dma = i;
            return;
        }
    }
    dmac->any_active = false;
}