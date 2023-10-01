#include "scheduler.h"

#include <stdio.h>

#include "gba.h"
#include "ppu.h"

void run_scheduler(Scheduler* sched, int cycles) {
    dword end_time = sched->master->cycles + cycles;
    while (sched->n_events && sched->event_queue[0].time <= end_time) {
        run_next_event(sched);
    }
    sched->master->cycles = end_time;
}

void run_next_event(Scheduler* sched) {
    if (sched->n_events == 0) return;

    Event e = sched->event_queue[0];
    sched->n_events--;
    for (int i = 0; i < sched->n_events; i++) {
        sched->event_queue[i] = sched->event_queue[i + 1];
    }
    sched->master->cycles = e.time;
    if (e.type < 4) {
        reload_timer(&sched->master->tmc, e.type);
    } else if (e.type == EVENT_PPU_HDRAW) {
        on_hdraw(&sched->master->ppu);
    } else if (e.type == EVENT_PPU_HBLANK) {
        on_hblank(&sched->master->ppu);
    }
}

void add_event(Scheduler* sched, Event* e) {
    if (sched->n_events == EVENT_MAX) return;

    int i = sched->n_events;
    sched->event_queue[sched->n_events++] = *e;
    while (i > 0 &&
           sched->event_queue[i].time < sched->event_queue[i - 1].time) {
        Event tmp = sched->event_queue[i - 1];
        sched->event_queue[i - 1] = sched->event_queue[i];
        sched->event_queue[i] = tmp;
        i--;
    }
}

void remove_event(Scheduler* sched, EventType t) {
    for (int i = 0; i < sched->n_events; i++) {
        if (sched->event_queue[i].type == t) {
            sched->n_events--;
            for (int j = i; j < sched->n_events; j++) {
                sched->event_queue[j] = sched->event_queue[j + 1];
            }
            return;
        }
    }
}
