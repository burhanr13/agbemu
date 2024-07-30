#include "timer.h"

#include <stdio.h>

#include "gba.h"
#include "io.h"

const int RATES[4] = {0, 6, 8, 10};

void update_timer_count(TimerController* tmc, int i) {
    if (!tmc->master->io.tm[i].cnt.enable ||
        tmc->master->io.tm[i].cnt.countup) {
        tmc->set_time[i] = tmc->master->sched.now;
        return;
    }

    int rate = RATES[tmc->master->io.tm[i].cnt.rate];
    tmc->counter[i] +=
        (tmc->master->sched.now >> rate) - (tmc->set_time[i] >> rate);
    tmc->set_time[i] = tmc->master->sched.now;
}

void update_timer_reload(TimerController* tmc, int i) {
    remove_event(&tmc->master->sched, i);

    if (!tmc->master->io.tm[i].cnt.enable || tmc->master->io.tm[i].cnt.countup)
        return;

    int rate = RATES[tmc->master->io.tm[i].cnt.rate];
    dword rel_time =
        (tmc->set_time[i] + ((0x10000 - tmc->counter[i]) << rate)) &
        ~((1 << rate) - 1);
    add_event(&tmc->master->sched, i, rel_time);
}

void enable_timer(TimerController* tmc, int i) {
    tmc->counter[i] = tmc->ena_count[i];
    tmc->set_time[i] = tmc->master->sched.now;
    update_timer_reload(tmc, i);
}

void timer_write_l(IO* io, int i) {
    io->tm[i].reload = io->master->tmc.written_cnt_l[i];
}

void timer_write_h(IO* io, int i) {
    hword data = io->master->tmc.written_cnt_h[i];
    bool prev_ena = io->tm[i].cnt.enable;
    update_timer_count(&io->master->tmc, i);
    io->tm[i].cnt.h = data;
    if (i == 0) io->tm[i].cnt.countup = 0;
    if (!prev_ena && io->tm[i].cnt.enable) {
        io->master->tmc.ena_count[i] = io->tm[i].reload;
        add_event(&io->master->sched, EVENT_TM0_ENA + i,
                  io->master->sched.now + 1);
    } else {
        update_timer_reload(&io->master->tmc, i);
    }
}

void reload_timer(TimerController* tmc, int i) {
    tmc->counter[i] = tmc->master->io.tm[i].reload;
    tmc->set_time[i] = tmc->master->sched.now;
    update_timer_reload(tmc, i);

    if (tmc->master->io.soundcnth.cha_timer == i) {
        fifo_a_pop(&tmc->master->apu);
        if (tmc->master->apu.fifo_a_size <= 16 &&
            tmc->master->io.dma[1].cnt.start == DMA_ST_SPEC) {
            tmc->master->dmac.dma[1].sound = true;
            dma_activate(&tmc->master->dmac, 1);
        }
    }
    if (tmc->master->io.soundcnth.chb_timer == i) {
        fifo_b_pop(&tmc->master->apu);
        if (tmc->master->apu.fifo_b_size <= 16 &&
            tmc->master->io.dma[2].cnt.start == DMA_ST_SPEC) {
            tmc->master->dmac.dma[2].sound = true;
            dma_activate(&tmc->master->dmac, 2);
        }
    }

    if (tmc->master->io.tm[i].cnt.irq)
        add_event(&tmc->master->sched, EVENT_TM0_IRQ + i,
                  tmc->master->sched.now + 3);

    if (i + 1 < 4 && tmc->master->io.tm[i + 1].cnt.enable &&
        tmc->master->io.tm[i + 1].cnt.countup) {
        tmc->counter[i + 1]++;
        if (tmc->counter[i + 1] == 0) reload_timer(tmc, i + 1);
    }
}