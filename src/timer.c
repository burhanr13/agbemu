#include "timer.h"

#include <stdio.h>

#include "gba.h"
#include "io.h"

const int rates[4] = {0, 63, 255, 1023};

void tick_timers(TimerController* tmc) {
    for (int i = 0; i < 4; i++) {
        if (tmc->master->io.tm[i].cnt.enable &&
            !tmc->master->io.tm[i].cnt.countup &&
            (tmc->master->cycles & rates[tmc->master->io.tm[i].cnt.rate]) ==
                0) {
            tmc->counter[i]++;
            if (tmc->counter[i] == 0) reload_timer(tmc, i);
        }
    }
}

void reload_timer(TimerController* tmc, int i) {
    tmc->counter[i] = tmc->master->io.tm[i].reload;
    if (i + 1 < 4 && tmc->master->io.tm[i + 1].cnt.countup) {
        tmc->counter[i + 1]++;
        if (tmc->counter[i + 1] == 0) reload_timer(tmc, i + 1);
    }
    if (tmc->master->io.tm[i].cnt.irq) tmc->master->io.ifl.timer |= 1 << i;
}