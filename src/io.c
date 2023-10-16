#include "io.h"

#include <stdio.h>

#include "dma.h"
#include "gba.h"

byte io_readb(IO* io, word addr) {
    hword h = io_readh(io, addr & ~1);
    if (addr & 1) {
        return h >> 8;
    } else return h;
}

void io_writeb(IO* io, word addr, byte data) {
    if (addr == POSTFLG) {
        io->postflg = data;
    } else if (addr == HALTCNT) {
        if (data & (1 << 7)) {
            io->master->stop = true;
        } else {
            io->master->halt = true;
        }
    } else {
        hword h;
        if (addr & 1) {
            h = data << 8;
            h |= io->b[addr & ~1];
        } else {
            h = data;
            h |= io->b[addr | 1] << 8;
        }
        io_writeh(io, addr & ~1, h);
    }
}

hword io_readh(IO* io, word addr) {
    if ((BG0HOFS <= addr && addr <= BLDY)) {
        io->master->openbus = true;
        return 0;
    }
    if (DMA0SAD <= addr && addr <= DMA3CNT_H) {
        switch (addr) {
            case DMA0CNT_H:
            case DMA1CNT_H:
            case DMA2CNT_H:
            case DMA3CNT_H:
                return io->h[addr >> 1];
            case DMA0CNT_L:
            case DMA1CNT_L:
            case DMA2CNT_L:
            case DMA3CNT_L:
                return 0;
            default:
                io->master->openbus = true;
                return 0;
        }
    }
    switch (addr) {
        case TM0CNT_L:
        case TM1CNT_L:
        case TM2CNT_L:
        case TM3CNT_L: {
            int i = (addr - TM0CNT_L) / (TM1CNT_L - TM0CNT_L);
            update_timer_count(&io->master->tmc, i);
            return io->master->tmc.counter[i];
        }
    }
    return io->h[addr >> 1];
}

void io_writeh(IO* io, word addr, hword data) {
    if ((addr & ~0b11) == BG2X || (addr & ~0b11) == BG2Y || (addr & ~0b11) == BG3X ||
        (addr & ~0b11) == BG3Y) {
        word w;
        if (addr & 0b10) {
            w = data << 16;
            w |= io->h[(addr >> 1) & ~1];
        } else {
            w = data;
            w |= io->h[(addr >> 1) | 1] << 16;
        }
        io_writew(io, addr & ~0b11, w);
        return;
    }
    switch (addr) {
        case DISPSTAT:
            io->dispstat.h &= 0b111;
            io->dispstat.h |= data & ~0b111;
            break;
        case SOUND1CNT_L:
            if ((data & NR10_PACE) == 0) io->master->apu.ch1_sweep_pace = 0;
            io->nr10 = data;
            break;
        case SOUND1CNT_H:
            io->master->apu.ch1_len_counter = data & NRX1_LEN;
            io->nr11 = data & NRX1_DUTY;
            data >>= 8;
            if (!(data & 0b11111000)) io->master->apu.ch1_enable = false;
            io->nr12 = data;
            break;
        case SOUND1CNT_X:
            io->master->apu.ch1_wavelen = data & NRX34_WVLEN;
            io->sound1cntx = data & NRX34_WVLEN;
            data >>= 8;
            if ((io->nr12 & 0b11111000) && (data & NRX4_TRIGGER)) {
                io->master->apu.ch1_enable = true;
                io->master->apu.ch1_counter = io->master->apu.ch1_wavelen;
                io->master->apu.ch1_duty_index = 0;
                io->master->apu.ch1_env_counter = 0;
                io->master->apu.ch1_env_pace = io->nr12 & NRX2_PACE;
                io->master->apu.ch1_env_dir = io->nr12 & NRX2_DIR;
                io->master->apu.ch1_volume = (io->nr12 & NRX2_VOL) >> 4;
                io->master->apu.ch1_sweep_pace = (io->nr10 & NR10_PACE) >> 4;
                io->master->apu.ch1_sweep_counter = 0;
            }
            io->nr14 |= data & NRX4_LEN_ENABLE;
            break;
        case SOUND2CNT_L:
            io->master->apu.ch2_len_counter = data & NRX1_LEN;
            io->nr21 = data & NRX1_DUTY;
            data >>= 8;
            if (!(data & 0b11111000)) io->master->apu.ch2_enable = false;
            io->nr22 = data;
            break;
        case SOUND2CNT_H:
            io->master->apu.ch2_wavelen = data & NRX34_WVLEN;
            io->sound2cnth = data & NRX34_WVLEN;
            data >>= 8;
            if ((io->nr22 & 0b11111000) && (data & NRX4_TRIGGER)) {
                io->master->apu.ch2_enable = true;
                io->master->apu.ch2_counter = io->master->apu.ch2_wavelen;
                io->master->apu.ch2_duty_index = 0;
                io->master->apu.ch2_env_counter = 0;
                io->master->apu.ch2_env_pace = io->nr22 & NRX2_PACE;
                io->master->apu.ch2_env_dir = io->nr22 & NRX2_DIR;
                io->master->apu.ch2_volume = (io->nr22 & NRX2_VOL) >> 4;
            }
            io->nr24 |= data & NRX4_LEN_ENABLE;
            break;
        case SOUND3CNT_L:
            if (!(data & 0b10000000)) io->master->apu.ch3_enable = false;
            io->nr30 = data & 0b10000000;
            break;
        case SOUND3CNT_H:
            io->master->apu.ch3_len_counter = data;
            data >>= 8;
            io->nr32 = data & 0b01100000;
            break;
        case SOUND3CNT_X:
            io->master->apu.ch3_wavelen = data & NRX34_WVLEN;
            io->sound3cntx = data & NRX34_WVLEN;
            data >>= 8;
            if ((io->nr30 & 0b10000000) && (data & NRX4_TRIGGER)) {
                io->master->apu.ch3_enable = true;
                io->master->apu.ch3_counter = io->master->apu.ch3_wavelen;
                io->master->apu.ch3_sample_index = 0;
            }
            io->nr34 |= data & NRX4_LEN_ENABLE;
            break;
        case SOUND4CNT_L:
            io->master->apu.ch4_len_counter = data & NRX1_LEN;
            data >>= 8;
            if (!(data & 0b11111000)) io->master->apu.ch4_enable = false;
            io->nr42 = data;
            break;
        case SOUND4CNT_H:
            io->nr43 = data;
            data >>= 8;
            if ((io->nr42 & 0b11111000) && (data & NRX4_TRIGGER)) {
                io->master->apu.ch4_enable = true;
                io->master->apu.ch4_counter = 0;
                io->master->apu.ch4_lfsr = 0;
                io->master->apu.ch4_env_counter = 0;
                io->master->apu.ch4_env_pace = io->nr42 & NRX2_PACE;
                io->master->apu.ch4_env_dir = io->nr42 & NRX2_DIR;
                io->master->apu.ch4_volume = (io->nr42 & NRX2_VOL) >> 4;
            }
            io->nr44 = data & NRX4_LEN_ENABLE;
            break;
        case DMA0CNT_H:
        case DMA1CNT_H:
        case DMA2CNT_H:
        case DMA3CNT_H: {
            int i = (addr - DMA0CNT_H) / (DMA1CNT_H - DMA0CNT_H);
            bool prev_ena = io->dma[i].cnt.enable;
            io->dma[i].cnt.h = data;
            if (!prev_ena && io->dma[i].cnt.enable) {
                dma_enable(&io->master->dmac, i);
            }
            if (!io->dma[i].cnt.enable) {
                io->master->dmac.dma[i].active = false;
                dma_update_active(&io->master->dmac);
            }
            break;
        }
        case TM0CNT_H:
        case TM1CNT_H:
        case TM2CNT_H:
        case TM3CNT_H: {
            int i = (addr - TM0CNT_H) / (TM1CNT_H - TM0CNT_H);
            io->master->tmc.new_tmcnt[i] = data;
            add_event(&io->master->sched, &(Event){io->master->cycles + 1, EVENT_TM0_W + i});
        }
        case KEYINPUT:
            break;
        case IF:
            io->ifl.h &= ~data;
            break;
        case POSTFLG:
            io_writeb(io, addr, data);
            io_writeb(io, addr | 1, data >> 8);
        default:
            io->h[addr >> 1] = data;
    }
}

word io_readw(IO* io, word addr) {
    return io_readh(io, addr) | (io_readh(io, addr | 2) << 16);
}

void io_writew(IO* io, word addr, word data) {
    switch (addr) {
        case BG2X:
            io->bgaff[0].x = data;
            io->master->ppu.bgaffintr[0].x = data;
            break;
        case BG2Y:
            io->bgaff[0].y = data;
            io->master->ppu.bgaffintr[0].y = data;
            break;
        case BG3X:
            io->bgaff[1].x = data;
            io->master->ppu.bgaffintr[1].x = data;
            break;
        case BG3Y:
            io->bgaff[1].y = data;
            io->master->ppu.bgaffintr[1].y = data;
            break;
        default:
            io_writeh(io, addr, data);
            io_writeh(io, addr | 2, data >> 16);
    }
}