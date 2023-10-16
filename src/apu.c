#include "apu.h"

#include <stdio.h>

#include "gba.h"
#include "io.h"

byte duty_cycles[] = {0b11111110, 0b01111110, 0b01111000, 0b10000001};

byte get_sample_ch1(APU* apu) {
    return (duty_cycles[(apu->master->io.nr11 & NRX1_DUTY) >> 6] & (1 << (apu->ch1_duty_index & 7)))
               ? apu->ch1_volume
               : 0;
}

byte get_sample_ch2(APU* apu) {
    return (duty_cycles[(apu->master->io.nr21 & NRX1_DUTY) >> 6] & (1 << (apu->ch2_duty_index & 7)))
               ? apu->ch2_volume
               : 0;
}

byte get_sample_ch3(APU* apu) {
    byte wvram_index = (apu->ch3_sample_index & 0b00011110) >> 1;
    byte sample = apu->master->io.waveram[wvram_index];
    if (apu->ch3_sample_index & 1) {
        sample &= 0x0f;
    } else {
        sample &= 0xf0;
        sample >>= 4;
    }
    byte out_level = (apu->master->io.nr32 & 0b01100000) >> 5;
    if (out_level) {
        return sample >> (out_level - 1);
    } else {
        return 0;
    }
}

byte get_sample_ch4(APU* apu) {
    return (apu->ch4_lfsr & 1) ? apu->ch4_volume : 0;
}

void tick_apu(APU* apu) {
    if (!(apu->master->io.nr52 & 0b10000000)) {
        apu->master->io.nr52 = 0;
        apu->cycles = 0;
        apu->apu_div = 0;
        apu->sample_ind = 0;
        return;
    }

    apu->master->io.nr52 = 0b10000000 | (apu->ch1_enable ? 0b0001 : 0) |
                           (apu->ch2_enable ? 0b0010 : 0) | (apu->ch3_enable ? 0b0100 : 0) |
                           (apu->ch4_enable ? 0b1000 : 0);

    apu->cycles++;

    if (apu->cycles % 8 == 0) {
        apu->ch3_counter++;
        if (apu->ch3_counter == 2048) {
            apu->ch3_counter = apu->ch3_wavelen;
            apu->ch3_sample_index++;
        }
    }

    if (apu->cycles % 16 == 0) {
        apu->ch1_counter++;
        if (apu->ch1_counter == 2048) {
            apu->ch1_counter = apu->ch1_wavelen;
            apu->ch1_duty_index++;
        }

        apu->ch2_counter++;
        if (apu->ch2_counter == 2048) {
            apu->ch2_counter = apu->ch2_wavelen;
            apu->ch2_duty_index++;
        }
    }

    if (apu->cycles % 32 == 0) {
        apu->ch4_counter++;
        int rate = 2 << ((apu->master->io.nr43 & NR43_SHIFT) >> 4);
        if (apu->master->io.nr43 & NR43_DIV) {
            rate *= apu->master->io.nr43 & NR43_DIV;
        }
        if (apu->ch4_counter >= rate) {
            apu->ch4_counter = 0;
            hword bit = (~(apu->ch4_lfsr ^ (apu->ch4_lfsr >> 1))) & 1;
            apu->ch4_lfsr = (apu->ch4_lfsr & ~(1 << 15)) | (bit << 15);
            if (apu->master->io.nr43 & NR43_WIDTH) {
                apu->ch4_lfsr = (apu->ch4_lfsr & ~(1 << 7)) | (bit << 7);
            }
            apu->ch4_lfsr >>= 1;
        }
    }

    if (apu->cycles % SAMPLE_RATE == 0) {
        byte ch1_sample = apu->ch1_enable ? get_sample_ch1(apu) : 0;
        byte ch2_sample = apu->ch2_enable ? get_sample_ch2(apu) : 0;
        byte ch3_sample = apu->ch3_enable ? get_sample_ch3(apu) : 0;
        byte ch4_sample = apu->ch4_enable ? get_sample_ch4(apu) : 0;

        shword l_sample = 0, r_sample = 0;
        if (apu->master->io.nr51 & (1 << 0)) r_sample += ch1_sample;
        if (apu->master->io.nr51 & (1 << 1)) r_sample += ch2_sample;
        if (apu->master->io.nr51 & (1 << 2)) r_sample += ch3_sample;
        if (apu->master->io.nr51 & (1 << 3)) r_sample += ch4_sample;
        if (apu->master->io.nr51 & (1 << 4)) l_sample += ch1_sample;
        if (apu->master->io.nr51 & (1 << 5)) l_sample += ch2_sample;
        if (apu->master->io.nr51 & (1 << 6)) l_sample += ch3_sample;
        if (apu->master->io.nr51 & (1 << 7)) l_sample += ch4_sample;

        l_sample *= 2 * (((apu->master->io.nr50 & 0b01110000) >> 4) + 1);
        r_sample *= 2 * ((apu->master->io.nr50 & 0b00000111) + 1);
        l_sample >>= 2 - apu->master->io.soundcnth.gb_volume;
        r_sample >>= 2 - apu->master->io.soundcnth.gb_volume;

        shword cha_sample = apu->fifo_a[0] * 2;
        shword chb_sample = apu->fifo_b[0] * 2;
        cha_sample <<= apu->master->io.soundcnth.cha_volume;
        chb_sample <<= apu->master->io.soundcnth.chb_volume;
        if (apu->master->io.soundcnth.cha_ena_left) l_sample += cha_sample;
        if (apu->master->io.soundcnth.cha_ena_right) r_sample += cha_sample;
        if (apu->master->io.soundcnth.chb_ena_left) l_sample += chb_sample;
        if (apu->master->io.soundcnth.chb_ena_right) r_sample += chb_sample;

        apu->sample_buf[apu->sample_ind++] = (float) l_sample / 0x600;
        apu->sample_buf[apu->sample_ind++] = (float) r_sample / 0x600;
        if (apu->sample_ind == SAMPLE_BUF_LEN) {
            apu->samples_full = true;
            apu->sample_ind = 0;
        }
    }

    if (apu->cycles % APU_DIV_RATE == 0) {
        apu->apu_div++;

        if (apu->apu_div % 2 == 0) {
            if (apu->master->io.nr14 & NRX4_LEN_ENABLE) {
                apu->ch1_len_counter++;
                if (apu->ch1_len_counter == 64) {
                    apu->ch1_len_counter = 0;
                    apu->ch1_enable = false;
                }
            }

            if (apu->master->io.nr24 & NRX4_LEN_ENABLE) {
                apu->ch2_len_counter++;
                if (apu->ch2_len_counter == 64) {
                    apu->ch2_len_counter = 0;
                    apu->ch2_enable = false;
                }
            }

            if (apu->master->io.nr34 & NRX4_LEN_ENABLE) {
                apu->ch3_len_counter++;
                if (apu->ch3_len_counter == 0) {
                    apu->ch3_enable = false;
                }
            }

            if (apu->master->io.nr44 & NRX4_LEN_ENABLE) {
                apu->ch4_len_counter++;
                if (apu->ch4_len_counter == 64) {
                    apu->ch4_len_counter = 0;
                    apu->ch4_enable = false;
                }
            }
        }
        if (apu->apu_div % 4 == 0) {
            apu->ch1_sweep_counter++;
            if (apu->ch1_sweep_pace && apu->ch1_sweep_counter == apu->ch1_sweep_pace) {
                apu->ch1_sweep_counter = 0;
                apu->ch1_sweep_pace = (apu->master->io.nr10 & NR10_PACE) >> 4;
                hword del_wvlen = apu->ch1_wavelen >> (apu->master->io.nr10 & NR10_SLOP);
                hword new_wvlen = apu->ch1_wavelen;
                if (apu->master->io.nr10 & NR10_DIR) {
                    new_wvlen -= del_wvlen;
                } else {
                    new_wvlen += del_wvlen;
                    if (new_wvlen > 2047) apu->ch1_enable = false;
                }
                if (apu->master->io.nr10 & NR10_SLOP) apu->ch1_wavelen = new_wvlen;
            }
        }
        if (apu->apu_div % 8 == 0) {
            apu->ch1_env_counter++;
            if (apu->ch1_env_pace && apu->ch1_env_counter == apu->ch1_env_pace) {
                apu->ch1_env_counter = 0;
                if (apu->ch1_env_dir) {
                    apu->ch1_volume++;
                    if (apu->ch1_volume == 0x10) apu->ch1_volume = 0xf;
                } else {
                    apu->ch1_volume--;
                    if (apu->ch1_volume == 0xff) apu->ch1_volume = 0x0;
                }
            }

            apu->ch2_env_counter++;
            if (apu->ch2_env_pace && apu->ch2_env_counter == apu->ch2_env_pace) {
                apu->ch2_env_counter = 0;
                if (apu->ch2_env_dir) {
                    apu->ch2_volume++;
                    if (apu->ch2_volume == 0x10) apu->ch2_volume = 0xf;
                } else {
                    apu->ch2_volume--;
                    if (apu->ch2_volume == 0xff) apu->ch2_volume = 0x0;
                }
            }

            apu->ch4_env_counter++;
            if (apu->ch4_env_pace && apu->ch4_env_counter == apu->ch4_env_pace) {
                apu->ch4_env_counter = 0;
                if (apu->ch4_env_dir) {
                    apu->ch4_volume++;
                    if (apu->ch4_volume == 0x10) apu->ch4_volume = 0xf;
                } else {
                    apu->ch4_volume--;
                    if (apu->ch4_volume == 0xff) apu->ch4_volume = 0x0;
                }
            }
        }
    }
}

void fifo_a_push(APU* apu, word samples) {
    for (int i = 0; i < 4; i++, samples >>= 8) {
        if (apu->fifo_a_size == 32) fifo_a_pop(apu);
        apu->fifo_a[apu->fifo_a_size++] = samples & 0xff;
    }
}

void fifo_a_pop(APU* apu) {
    if (apu->fifo_a_size == 0) return;
    apu->fifo_a_size--;
    for (int i = 0; i < apu->fifo_a_size; i++) {
        apu->fifo_a[i] = apu->fifo_a[i + 1];
    }
}

void fifo_b_push(APU* apu, word samples) {
    for (int i = 0; i < 4; i++, samples >>= 8) {
        if (apu->fifo_b_size == 32) fifo_b_pop(apu);
        apu->fifo_b[apu->fifo_b_size++] = samples & 0xff;
    }
}

void fifo_b_pop(APU* apu) {
    if (apu->fifo_b_size == 0) return;
    apu->fifo_b_size--;
    for (int i = 0; i < apu->fifo_b_size; i++) {
        apu->fifo_b[i] = apu->fifo_b[i + 1];
    }
}
