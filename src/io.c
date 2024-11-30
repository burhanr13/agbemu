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
    if (BG0HOFS <= addr && addr < SOUND1CNT_L) {
        if (addr == WININ || addr == WINOUT || addr == BLDCNT ||
            addr == BLDALPHA) {
            return io->h[addr >> 1];
        }
        io->master->openbus = true;
        return 0;
    }
    if (SOUNDBIAS + 4 <= addr && addr < WAVERAM) {
        io->master->openbus = true;
        return 0;
    }
    if (FIFO_A <= addr && addr < TM0CNT_L) {
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
    if (TM3CNT_H < addr && addr < 0x120) {
        io->master->openbus = true;
        return 0;
    }
    if (0x15c <= addr && addr < IE) {
        io->master->openbus = true;
        return 0;
    }
    if (IME + 4 <= addr) {
        if ((addr & ~0b11) == POSTFLG) {
            return io->h[addr >> 1];
        }
        io->master->openbus = true;
        return 0;
    }
    switch (addr) {
        case SOUND1CNT_X:
        case SOUND2CNT_H:
        case SOUND3CNT_X:
            return io->h[addr >> 1] & ~NRX34_WVLEN;
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
    if ((addr & ~0b11) == BG2X || (addr & ~0b11) == BG2Y ||
        (addr & ~0b11) == BG3X || (addr & ~0b11) == BG3Y ||
        (addr & ~0b11) == FIFO_A || (addr & ~0b11) == FIFO_B) {
        io->h[addr >> 1] = data;
        io_writew(io, addr & ~0b11, io->w[addr >> 2]);
        return;
    }
    switch (addr) {
        case DISPSTAT:
            io->dispstat.h &= 0b111;
            io->dispstat.h |= data & ~0b111;
            break;
        case BG0CNT:
            io->bgcnt[0].h = data;
            io->bgcnt[0].overflow = 0;
            break;
        case BG1CNT:
            io->bgcnt[1].h = data;
            io->bgcnt[1].overflow = 0;
            break;
        case WININ:
            io->winin = data;
            io->wincnt[0].unused = 0;
            io->wincnt[1].unused = 0;
            break;
        case WINOUT:
            io->winout = data;
            io->wincnt[2].unused = 0;
            io->wincnt[3].unused = 0;
            break;
        case BLDCNT:
            io->bldcnt.h = data;
            io->bldcnt.unused = 0;
            break;
        case BLDALPHA:
            io->bldalpha.h = data;
            io->bldalpha.unused1 = 0;
            io->bldalpha.unused2 = 0;
            break;
        case SOUND1CNT_L:
            if ((data & NR10_PACE) == 0) io->master->apu.ch1_sweep_pace = 0;
            io->nr10 = data & 0b01111111;
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
                io->master->apu.ch1_duty_index = 0;
                io->master->apu.ch1_env_counter = 0;
                io->master->apu.ch1_env_pace = io->nr12 & NRX2_PACE;
                io->master->apu.ch1_env_dir = io->nr12 & NRX2_DIR;
                io->master->apu.ch1_volume = (io->nr12 & NRX2_VOL) >> 4;
                io->master->apu.ch1_sweep_pace = (io->nr10 & NR10_PACE) >> 4;
                io->master->apu.ch1_sweep_counter = 0;
                remove_event(&io->master->sched, EVENT_APU_CH1_REL);
                ch1_reload(&io->master->apu);
            }
            io->nr14 |= data & NRX4_LEN_ENABLE;
            break;
        case SOUND1CNT_X + 2:
            break;
        case SOUND2CNT_L:
            io->master->apu.ch2_len_counter = data & NRX1_LEN;
            io->nr21 = data & NRX1_DUTY;
            data >>= 8;
            if (!(data & 0b11111000)) io->master->apu.ch2_enable = false;
            io->nr22 = data;
            break;
        case SOUND2CNT_L + 2:
            break;
        case SOUND2CNT_H:
            io->master->apu.ch2_wavelen = data & NRX34_WVLEN;
            io->sound2cnth = data & NRX34_WVLEN;
            data >>= 8;
            if ((io->nr22 & 0b11111000) && (data & NRX4_TRIGGER)) {
                io->master->apu.ch2_enable = true;
                io->master->apu.ch2_duty_index = 0;
                io->master->apu.ch2_env_counter = 0;
                io->master->apu.ch2_env_pace = io->nr22 & NRX2_PACE;
                io->master->apu.ch2_env_dir = io->nr22 & NRX2_DIR;
                io->master->apu.ch2_volume = (io->nr22 & NRX2_VOL) >> 4;
                remove_event(&io->master->sched, EVENT_APU_CH2_REL);
                ch2_reload(&io->master->apu);
            }
            io->nr24 |= data & NRX4_LEN_ENABLE;
            break;
        case SOUND2CNT_H + 2:
            break;
        case SOUND3CNT_L:
            if (!(data & 0b10000000)) io->master->apu.ch3_enable = false;
            if ((io->nr30 & (1 << 6)) != (data & (1 << 6)))
                waveram_swap(&io->master->apu);
            io->nr30 = data & 0b11100000;
            break;
        case SOUND3CNT_H:
            io->master->apu.ch3_len_counter = data;
            data >>= 8;
            io->nr32 = data & 0b11100000;
            break;
        case SOUND3CNT_X:
            io->master->apu.ch3_wavelen = data & NRX34_WVLEN;
            io->sound3cntx = data & NRX34_WVLEN;
            data >>= 8;
            if ((io->nr30 & 0b10000000) && (data & NRX4_TRIGGER)) {
                io->master->apu.ch3_enable = true;
                io->master->apu.ch3_sample_index = 0;
                remove_event(&io->master->sched, EVENT_APU_CH3_REL);
                ch3_reload(&io->master->apu);
            }
            io->nr34 |= data & NRX4_LEN_ENABLE;
            break;
        case SOUND3CNT_X + 2:
            break;
        case SOUND4CNT_L:
            io->master->apu.ch4_len_counter = data & NRX1_LEN;
            data >>= 8;
            if (!(data & 0b11111000)) io->master->apu.ch4_enable = false;
            io->nr42 = data;
            break;
        case SOUND4CNT_L + 2:
            break;
        case SOUND4CNT_H:
            io->nr43 = data;
            data >>= 8;
            if ((io->nr42 & 0b11111000) && (data & NRX4_TRIGGER)) {
                io->master->apu.ch4_enable = true;
                io->master->apu.ch4_lfsr = 0;
                io->master->apu.ch4_env_counter = 0;
                io->master->apu.ch4_env_pace = io->nr42 & NRX2_PACE;
                io->master->apu.ch4_env_dir = io->nr42 & NRX2_DIR;
                io->master->apu.ch4_volume = (io->nr42 & NRX2_VOL) >> 4;
                remove_event(&io->master->sched, EVENT_APU_CH4_REL);
                ch4_reload(&io->master->apu);
            }
            io->nr44 = data & NRX4_LEN_ENABLE;
            break;
        case SOUND4CNT_H + 2:
            break;
        case SOUNDCNT_L:
            io->nr50 = data & 0b01110111;
            data >>= 8;
            io->nr51 = data;
            break;
        case SOUNDCNT_H:
            io->soundcnth.h = data;
            if (io->soundcnth.cha_reset) {
                io->soundcnth.cha_reset = 0;
                io->master->apu.fifo_a_size = 0;
            }
            if (io->soundcnth.chb_reset) {
                io->soundcnth.chb_reset = 0;
                io->master->apu.fifo_b_size = 0;
            }
            io->soundcnth.unused = 0;
            break;
        case SOUNDCNT_X:
            if (data & (1 << 7)) {
                if (!io->nr52) {
                    io->nr52 = 1 << 7;
                    apu_enable(&io->master->apu);
                }
            } else {
                io->nr52 = 0;
                apu_disable(&io->master->apu);
            }
            break;
        case SOUNDCNT_X + 2:
            break;
        case SOUNDBIAS + 2:
            break;
        case DMA0CNT_H:
        case DMA1CNT_H:
        case DMA2CNT_H:
        case DMA3CNT_H: {
            int i = (addr - DMA0CNT_H) / (DMA1CNT_H - DMA0CNT_H);
            bool prev_ena = io->dma[i].cnt.enable;
            io->dma[i].cnt.h = data;
            io->dma[i].cnt.unused = 0;
            if (i < 3) io->dma[i].cnt.drq = 0;
            if (!prev_ena && io->dma[i].cnt.enable) {
                dma_enable(&io->master->dmac, i);
            }
            break;
        }
        case TM0CNT_L:
        case TM1CNT_L:
        case TM2CNT_L:
        case TM3CNT_L: {
            int i = (addr - TM0CNT_L) / (TM1CNT_L - TM0CNT_L);
            io->master->tmc.written_cnt_l[i] = data;
            add_event(&io->master->sched, EVENT_TM0_WRITE_L + i,
                      io->master->sched.now + 1);
            break;
        }
        case TM0CNT_H:
        case TM1CNT_H:
        case TM2CNT_H:
        case TM3CNT_H: {
            int i = (addr - TM0CNT_H) / (TM1CNT_H - TM0CNT_H);
            io->master->tmc.written_cnt_h[i] = data;
            add_event(&io->master->sched, EVENT_TM0_WRITE_H + i,
                      io->master->sched.now + 1);
            break;
        }
        case SIOCNT:
            if (data & (1 << 7)) {
                data &= ~(1 << 7);
                if (data & (1 << 14)) io->ifl.serial = 1;
            }
            io->h[addr >> 1] = data;
            break;
        case KEYINPUT:
            break;
        case KEYCNT:
            io->keycnt.h = data;
            update_keypad_irq(io->master);
            break;
        case 0x136:
        case 0x142:
        case 0x15a:
            break;
        case IF:
            io->ifl.h &= ~data;
            break;
        case WAITCNT:
            io->waitcnt.w = data;
            io->waitcnt.gamepaktype = 0;
            update_cart_waits(io->master);
            io->master->prefetcher_cycles = 0;
            io->master->next_prefetch_addr = -1;
            break;
        case WAITCNT + 2:
            break;
        case IME + 2:
            break;
        case POSTFLG:
            io_writeb(io, addr, data);
            io_writeb(io, addr | 1, data >> 8);
            break;
        case POSTFLG + 2:
            break;
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
            io->bgaff[0].x = (sword) (data << 4) >> 4;
            io->master->ppu.bgaffintr[0].x = io->bgaff[0].x;
            break;
        case BG2Y:
            io->bgaff[0].y = (sword) (data << 4) >> 4;
            io->master->ppu.bgaffintr[0].y = io->bgaff[0].y;
            break;
        case BG3X:
            io->bgaff[1].x = (sword) (data << 4) >> 4;
            io->master->ppu.bgaffintr[1].x = io->bgaff[1].x;
            break;
        case BG3Y:
            io->bgaff[1].y = (sword) (data << 4) >> 4;
            io->master->ppu.bgaffintr[1].y = io->bgaff[1].y;
            break;
        case FIFO_A:
            fifo_a_push(&io->master->apu, data);
            break;
        case FIFO_B:
            fifo_b_push(&io->master->apu, data);
            break;
        default:
            io_writeh(io, addr, data);
            io_writeh(io, addr | 2, data >> 16);
    }
}
