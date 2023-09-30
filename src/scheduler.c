#include "scheduler.h"

#include "gba.h"
#include "ppu.h"

void tick_scheduler(Scheduler* sched) {
    if(sched->ppu_next.time == sched->master->cycles) {
        switch(sched->ppu_next.callback) {
            case PPU_CLBK_HDRAW:
                on_hdraw(&sched->master->ppu);
                break;
            case PPU_CLBK_HBLANK:
                on_hblank(&sched->master->ppu);
                break;
        }
    }
}