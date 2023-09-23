#ifndef TIMER_H
#define TIMER_H

#include "types.h"

typedef struct _GBA GBA;

typedef struct {
    GBA* master;

    hword counter[4];
} TimerController;

void tick_timers(TimerController* tmc);

void reload_timer(TimerController* tmc, int i);

#endif