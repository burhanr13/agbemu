#ifndef APU_H
#define APU_H

#include "types.h"

#define APU_DIV_PERIOD 32768

#define SAMPLE_FREQ 32768
#define SAMPLE_PERIOD ((1 << 24) / SAMPLE_FREQ)
#define SAMPLE_BUF_LEN 1024

enum { NRX1_LEN = 0b00111111, NRX1_DUTY = 0b11000000 };

enum { NRX2_PACE = 0b00000111, NRX2_DIR = 1 << 3, NRX2_VOL = 0b11110000 };

enum { NRX4_WVLEN_HI = 0b00000111, NRX4_LEN_ENABLE = 1 << 6, NRX4_TRIGGER = 1 << 7 };

#define NRX34_WVLEN (NRX4_WVLEN_HI << 8 | 0xff)

enum { NR10_SLOP = 0b00000111, NR10_DIR = 1 << 3, NR10_PACE = 0b01110000 };

enum { NR43_DIV = 0b00000111, NR43_WIDTH = 1 << 3, NR43_SHIFT = 0b11110000 };

typedef struct _GBA GBA;

typedef struct {
    GBA* master;

    hword apu_div;

    float sample_buf[SAMPLE_BUF_LEN];
    int sample_ind;
    bool samples_full;

    bool ch1_enable;
    hword ch1_wavelen;
    byte ch1_duty_index;
    byte ch1_env_counter;
    byte ch1_env_pace;
    bool ch1_env_dir;
    byte ch1_volume;
    byte ch1_len_counter;
    byte ch1_sweep_pace;
    byte ch1_sweep_counter;

    bool ch2_enable;
    hword ch2_wavelen;
    byte ch2_duty_index;
    byte ch2_env_counter;
    byte ch2_env_pace;
    bool ch2_env_dir;
    byte ch2_volume;
    byte ch2_len_counter;

    bool ch3_enable;
    hword ch3_wavelen;
    byte ch3_sample_index;
    byte ch3_len_counter;
    byte waveram[0x10];

    bool ch4_enable;
    hword ch4_lfsr;
    byte ch4_env_counter;
    byte ch4_env_pace;
    bool ch4_env_dir;
    byte ch4_volume;
    byte ch4_len_counter;

    sbyte fifo_a[32];
    sbyte fifo_b[32];

    byte fifo_a_size;
    byte fifo_b_size;
} APU;

void apu_enable(APU* apu);
void apu_disable(APU* apu);

void apu_new_sample(APU* apu);

void ch1_reload(APU* apu);
void ch2_reload(APU* apu);
void ch3_reload(APU* apu);
void ch4_reload(APU* apu);

void waveram_swap(APU* apu);

void fifo_a_push(APU* apu, word samples);
void fifo_a_pop(APU* apu);

void fifo_b_push(APU* apu, word samples);
void fifo_b_pop(APU* apu);

void apu_div_tick(APU* apu);

void tick_apu(APU* apu);

#endif