#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"

typedef enum {
    EVENT_TM0_REL,
    EVENT_TM1_REL,
    EVENT_TM2_REL,
    EVENT_TM3_REL,
    EVENT_TM0_ENA,
    EVENT_TM1_ENA,
    EVENT_TM2_ENA,
    EVENT_TM3_ENA,
    EVENT_PPU_HDRAW,
    EVENT_PPU_HBLANK,
    EVENT_PPU_HBLANK_FLG,
    EVENT_APU_SAMPLE,
    EVENT_APU_CH1_REL,
    EVENT_APU_CH2_REL,
    EVENT_APU_CH3_REL,
    EVENT_APU_CH4_REL,
    EVENT_APU_DIV_TICK,
    EVENT_MAX
} EventType;

typedef struct {
    dword time;
    EventType type;
} Event;

typedef struct _GBA GBA;

typedef struct {
    GBA* master;

    dword now;

    Event event_queue[EVENT_MAX];
    int n_events;
} Scheduler;

void run_scheduler(Scheduler* sched, int cycles);
void run_next_event(Scheduler* sched);

void add_event(Scheduler* sched, Event* e);
void remove_event(Scheduler* sched, EventType t);

#endif