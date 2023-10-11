#ifndef TIMER_H
#define TIMER_H

#include "io.h"
#include "types.h"

typedef struct _GBA GBA;

typedef struct {
    GBA* master;

    dword set_time[4];
    hword counter[4];
    hword new_tmcnt[4];
} TimerController;

void update_timer_count(TimerController* tmc, int i);
void update_timer_reload(TimerController* tmc, int i);

void write_timer(IO* io, int i, hword new_tmcnt);

void reload_timer(TimerController* tmc, int i);

#endif