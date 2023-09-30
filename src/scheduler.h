#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"

typedef enum {
    PPU_CLBK_HDRAW,
    PPU_CLBK_HBLANK,
} PPUCallback;

typedef struct {
    dword time;
    PPUCallback callback;
} PPUEvent;

typedef struct _GBA GBA;

typedef struct {
    GBA* master;

    PPUEvent ppu_next;
} Scheduler;

void tick_scheduler(Scheduler* sched);

#endif