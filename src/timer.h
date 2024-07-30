#ifndef TIMER_H
#define TIMER_H

#include "types.h"

typedef struct _GBA GBA;
typedef struct _IO IO;

typedef struct {
    GBA* master;

    dword set_time[4];
    hword counter[4];
    hword ena_count[4];
    hword written_cnt_h[4];
    hword written_cnt_l[4];
} TimerController;

void update_timer_count(TimerController* tmc, int i);
void update_timer_reload(TimerController* tmc, int i);

void enable_timer(TimerController* tmc, int i);

void timer_write_l(IO* io, int i);
void timer_write_h(IO* io, int i);

void reload_timer(TimerController* tmc, int i);

#endif