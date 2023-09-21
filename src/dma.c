#include "dma.h"

#include "gba.h"

void dma_enable(DMAController* dmac, int i) {
    dmac->dma[i].active = false;
    dmac->dma[i].sptr = dmac->master->io.dma[i].sad;
    dmac->dma[i].dptr = dmac->master->io.dma[i].dad;
    dmac->dma[i].ct = dmac->master->io.dma[i].ct;
    if (i < 3) {
        dmac->dma[i].ct %= 0x4000;
        if (dmac->dma[i].ct == 0) dmac->dma[i].ct = 0x4000;
    }
}

void dma_activate(DMAController* dmac, int i) {
    if (dmac->dma[i].active) return;
    dmac->dma[i].active = true;
    if (dmac->master->io.dma[i].cnt.dadcnt == DMA_ADCNT_INR)
        dmac->dma[i].dptr = dmac->master->io.dma[i].dad;
    dmac->dma[i].ct = dmac->master->io.dma[i].ct;
    if (i < 3) {
        dmac->dma[i].ct %= 0x4000;
        if (dmac->dma[i].ct == 0) dmac->dma[i].ct = 0x4000;
    }
}

void update_addr(word* addr, int adcnt, int wsize) {
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
    if (dmac->master->io.dma[i].cnt.wsize) {
        dma_transw(dmac, dmac->dma[i].dptr, dmac->dma[i].sptr);
    } else {
        dma_transh(dmac, dmac->dma[i].dptr, dmac->dma[i].sptr);
    }
    update_addr(&dmac->dma[i].sptr, dmac->master->io.dma[i].cnt.sadcnt,
                2 << dmac->master->io.dma[i].cnt.wsize);
    update_addr(&dmac->dma[i].dptr, dmac->master->io.dma[i].cnt.dadcnt,
                2 << dmac->master->io.dma[i].cnt.wsize);
    dmac->dma[i].ct--;
    if (dmac->dma[i].ct == 0) {
        dmac->dma[i].active = false;
        if(!dmac->master->io.dma[i].cnt.repeat)
            dmac->master->io.dma[i].cnt.enable = 0;
        if(dmac->master->io.dma[i].cnt.irq)
            dmac->master->io.ifl.dma |= (1 << i);
    }
}

void dma_transh(DMAController* dmac, word daddr, word saddr) {
    tick_components(dmac->master, get_waitstates(dmac->master, saddr, D_HWORD));
    hword data = bus_readh(dmac->master, saddr);
    tick_components(dmac->master, get_waitstates(dmac->master, daddr, D_HWORD));
    bus_writeh(dmac->master, daddr, data);
}

void dma_transw(DMAController* dmac, word daddr, word saddr) {
    tick_components(dmac->master, get_waitstates(dmac->master, saddr, D_WORD));
    word data = bus_readw(dmac->master, saddr);
    tick_components(dmac->master, get_waitstates(dmac->master, daddr, D_WORD));
    bus_writew(dmac->master, daddr, data);
}