#ifndef DMA_H
#define DMA_H

#include "types.h"

enum { DMA_ADCNT_INC, DMA_ADCNT_DEC, DMA_ADCNT_FIX, DMA_ADCNT_INR };
enum { DMA_ST_IMM, DMA_ST_VBLANK, DMA_ST_HBLANK, DMA_ST_SPEC };

typedef struct _GBA GBA;

typedef struct {
    GBA* master;

    struct {
        word sptr;
        word dptr;
        word bus_val;
        hword ct;
        bool sound;
        bool initial;
        bool waiting;
    } dma[4];
    byte active_dma;
} DMAController;

void dma_enable(DMAController* dmac, int i);
void dma_activate(DMAController* dmac, int i);

void dma_run(DMAController* dmac, int i);

void dma_transh(DMAController* dmac, int i, word daddr, word saddr);
void dma_transw(DMAController* dmac, int i, word daddr, word saddr);

#endif