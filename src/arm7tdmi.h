#ifndef ARM7TDMI_H
#define ARM7TDMI_H

#include "types.h"

typedef enum { B_USER, B_FIQ, B_SVC, B_ABT, B_IRQ, B_UND, B_CT } RegBank;
typedef enum {
    M_USER = 0b10000,
    M_FIQ = 0b10001,
    M_IRQ = 0b10010,
    M_SVC = 0b10011,
    M_ABT = 0b10111,
    M_UND = 0b11011,
    M_SYSTEM = 0b11111
} CpuMode;

typedef struct _GBA GBA;

typedef struct {
    GBA* master;

    union {
        word r[16];
        struct {
            word _r[13];
            word sp;
            word lr;
            word pc;
        };
    };

    union {
        word w;
        struct {
            word m : 5; // mode
            word t : 1; // thumb state
            word f : 1; // disable fiq
            word i : 1; // disable irq
            word reserved : 20;
            word v : 1; // overflow
            word c : 1; // carry
            word z : 1; // zero
            word n : 1; // negative
        };
    } cpsr;
    word spsr;

    word banked_sp[B_CT];
    word banked_lr[B_CT];
    word banked_spsr[B_CT];

} Arm7TDMI;

void tick_cpu(Arm7TDMI* cpu);

void cpu_update_mode(Arm7TDMI* cpu, CpuMode old);

byte cpu_readb(Arm7TDMI* cpu, word addr);
hword cpu_readh(Arm7TDMI* cpu, word addr);
word cpu_read(Arm7TDMI* cpu, word addr);
void cpu_writeb(Arm7TDMI* cpu, word addr, byte b);
void cpu_writeh(Arm7TDMI* cpu, word addr, hword h);
void cpu_write(Arm7TDMI* cpu, word addr, word w);

#endif