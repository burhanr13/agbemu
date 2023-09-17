#ifndef ARM7TDMI_H
#define ARM7TDMI_H

#include "arm_isa.h"
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

typedef enum {
    I_RESET,
    I_UND,
    I_SWI,
    I_PABT,
    I_DABT,
    I_ADDR,
    I_IRQ,
    I_FIQ
} CpuInterrupt;

typedef struct _GBA GBA;

typedef struct _Arm7TDMI {
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

    word banked_r8_12[2][5];
    word banked_sp[B_CT];
    word banked_lr[B_CT];
    word banked_spsr[B_CT];

    ArmInstr cur_instr;
    ArmInstr next_instr;

} Arm7TDMI;

void tick_cpu(Arm7TDMI* cpu);

void cpu_fetch(Arm7TDMI* cpu);
void cpu_flush(Arm7TDMI* cpu);

void cpu_update_mode(Arm7TDMI* cpu, CpuMode old);
void cpu_handle_interrupt(Arm7TDMI* cpu, CpuInterrupt intr);

byte cpu_readb(Arm7TDMI* cpu, word addr);
hword cpu_readh(Arm7TDMI* cpu, word addr);
word cpu_read(Arm7TDMI* cpu, word addr);
void cpu_writeb(Arm7TDMI* cpu, word addr, byte b);
void cpu_writeh(Arm7TDMI* cpu, word addr, hword h);
void cpu_write(Arm7TDMI* cpu, word addr, word w);

void cpu_internal_cycle(Arm7TDMI* cpu);

void print_cpu_state(Arm7TDMI* cpu);

#endif