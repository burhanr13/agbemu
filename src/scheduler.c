#include "scheduler.h"

#include <stdio.h>

#include "apu.h"
#include "gba.h"
#include "ppu.h"
#include "timer.h"

void (*apu_events[])(APU*) = {apu_new_sample, ch1_reload, ch2_reload,
                              ch3_reload,     ch4_reload, apu_div_tick};

void run_scheduler_mem(Scheduler* sched, int cycles) {
    dword end_time = sched->now + cycles;
    while (sched->n_events && sched->event_queue[0].time < end_time) {
        if (!sched->master->prefetch_halted)
            sched->master->prefetcher_cycles += 1;
        if (run_next_event(sched) > 0) {
            end_time = sched->now + cycles;
        } else {
            if (!sched->master->prefetch_halted)
                sched->master->prefetcher_cycles -= 1;
        }
    }
    sched->now = end_time;
    while (sched->n_events && sched->event_queue[0].time == end_time) {
        bus_lock(sched->master);
        run_next_event(sched);
    }
}

void run_scheduler_internal(Scheduler* sched, int cycles) {
    dword end_time = sched->now + cycles;
    while (sched->n_events && sched->event_queue[0].time <= end_time) {
        run_next_event(sched);
        if (sched->now > end_time) end_time = sched->now;
    }
    sched->now = end_time;
}

int run_next_event(Scheduler* sched) {
    if (sched->n_events == 0) return 0;

    Event e = sched->event_queue[0];
    sched->n_events--;
    for (int i = 0; i < sched->n_events; i++) {
        sched->event_queue[i] = sched->event_queue[i + 1];
    }

    sched->now = e.time;

    if (e.type < EVENT_TM0_ENA) {
        reload_timer(&sched->master->tmc, e.type);
    } else if (e.type < EVENT_TM0_IRQ) {
        enable_timer(&sched->master->tmc, e.type - EVENT_TM0_ENA);
    } else if (e.type < EVENT_TM0_WRITE_L) {
        sched->master->io.ifl.timer |= 1 << (e.type - EVENT_TM0_IRQ);
    } else if (e.type < EVENT_TM0_WRITE_H) {
        timer_write_l(&sched->master->io, e.type - EVENT_TM0_WRITE_L);
    } else if (e.type < EVENT_DMA0) {
        timer_write_h(&sched->master->io, e.type - EVENT_TM0_WRITE_H);
    } else if (e.type < EVENT_PPU_HDRAW) {
        dma_run(&sched->master->dmac, e.type - EVENT_DMA0);
    } else if (e.type == EVENT_PPU_HDRAW) {
        ppu_hdraw(&sched->master->ppu);
    } else if (e.type == EVENT_PPU_HBLANK) {
        ppu_hblank(&sched->master->ppu);
    } else {
        apu_events[e.type - EVENT_APU_SAMPLE](&sched->master->apu);
    }
    return sched->now - e.time;
}

void add_event(Scheduler* sched, EventType t, dword time) {
    if (sched->n_events == EVENT_MAX) return;

    int i = sched->n_events;
    sched->event_queue[sched->n_events].type = t;
    sched->event_queue[sched->n_events].time = time;
    sched->n_events++;

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

void print_scheduled_events(Scheduler* sched) {
    static char* event_names[EVENT_MAX] = {
        "TM0 reload",     "TM1 reload",     "TM2 reload",     "TM3 reload",
        "TM0 enable",     "TM1 enable",     "TM2 enable",     "TM3 enable",
        "TM0 interrupt",  "TM1 interrupt",  "TM2 interrupt",  "TM3 interrupt",
        "PPU hdraw",      "PPU hblank",     "APU sample",     "APU reload ch1",
        "APU reload ch2", "APU reload ch3", "APU reload ch4", "APU DIV tick"};

    for (int i = 0; i < sched->n_events; i++) {
        printf("%ld => %s\n", sched->event_queue[i].time,
               event_names[sched->event_queue[i].type]);
    }
}
