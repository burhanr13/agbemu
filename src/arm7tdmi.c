#include "arm7tdmi.h"

#include "arm_isa.h"
#include "gba.h"
#include "types.h"

void tick_cpu(Arm7TDMI* cpu) {
    if (cpu->cpsr.t) {

    } else {
        arm_exec_instr(cpu);
    }
}

RegBank get_bank(CpuMode mode) {
    switch (mode) {
        case M_USER:
            return B_USER;
        case M_FIQ:
            return B_FIQ;
        case M_IRQ:
            return B_IRQ;
        case M_SVC:
            return B_SVC;
        case M_ABT:
            return B_ABT;
        case M_UND:
            return B_UND;
        case M_SYSTEM:
            return B_USER;
    }
    return B_USER;
}

void cpu_update_mode(Arm7TDMI* cpu, CpuMode old) {
    RegBank old_bank = get_bank(old);
    cpu->banked_sp[old_bank] = cpu->sp;
    cpu->banked_lr[old_bank] = cpu->lr;
    cpu->banked_spsr[old_bank] = cpu->spsr;
    RegBank new_bank = get_bank(cpu->cpsr.m);
    cpu->sp = cpu->banked_sp[new_bank];
    cpu->lr = cpu->banked_lr[new_bank];
    cpu->spsr = cpu->banked_spsr[new_bank];
}

byte cpu_readb(Arm7TDMI* cpu, word addr) {
    return gba_readb(cpu->master, addr, NULL);
}

hword cpu_readh(Arm7TDMI* cpu, word addr) {
    return gba_readh(cpu->master, addr, NULL);
}

word cpu_read(Arm7TDMI* cpu, word addr) {
    return gba_read(cpu->master, addr, NULL);
}

void cpu_writeb(Arm7TDMI* cpu, word addr, byte b) {
    gba_writeb(cpu->master, addr, b, NULL);
}

void cpu_writeh(Arm7TDMI* cpu, word addr, hword h) {
    gba_writeh(cpu->master, addr, h, NULL);
}

void cpu_write(Arm7TDMI* cpu, word addr, word w) {
    gba_write(cpu->master, addr, w, NULL);
}