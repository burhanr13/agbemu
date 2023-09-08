#include "arm7tdmi.h"

#include <stdio.h>

#include "arm_isa.h"
#include "gba.h"
#include "thumb_isa.h"
#include "types.h"

void tick_cpu(Arm7TDMI* cpu) {
    if (cpu->cycles == 0) {
        if (!cpu->cpsr.i && cpu->master->io.ime &&
            (cpu->master->io.ie.h & cpu->master->io.ifl.h)) {
            cpu_handle_interrupt(cpu, I_IRQ);
        } else {
            arm_exec_instr(cpu);
        }
    }
    if (cpu->cycles > 0) cpu->cycles--;
}

void cpu_fetch(Arm7TDMI* cpu) {
    cpu->cur_instr = cpu->next_instr;
    if (cpu->cpsr.t) {
        cpu->next_instr =
            thumb_decode_instr((ThumbInstr){cpu_readh(cpu, cpu->pc)});
        cpu->pc += 2;
    } else {
        cpu->next_instr.w = cpu_read(cpu, cpu->pc);
        cpu->pc += 4;
    }
}

void cpu_flush(Arm7TDMI* cpu) {
    if (cpu->cpsr.t) {
        cpu->pc &= ~1;
        cpu->cur_instr =
            thumb_decode_instr((ThumbInstr){cpu_readh(cpu, cpu->pc)});
        cpu->pc += 2;
        cpu->next_instr =
            thumb_decode_instr((ThumbInstr){cpu_readh(cpu, cpu->pc)});
        cpu->pc += 2;
    } else {
        cpu->pc &= ~0b11;
        cpu->cur_instr.w = cpu_read(cpu, cpu->pc);
        cpu->pc += 4;
        cpu->next_instr.w = cpu_read(cpu, cpu->pc);
        cpu->pc += 4;
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

void cpu_handle_interrupt(Arm7TDMI* cpu, CpuInterrupt intr) {
    CpuMode old = cpu->cpsr.m;
    word spsr = cpu->cpsr.w;
    switch (intr) {
        case I_RESET:
        case I_SWI:
        case I_ADDR:
            cpu->cpsr.m = M_SVC;
            break;
        case I_PABT:
        case I_DABT:
            cpu->cpsr.m = M_ABT;
            break;
        case I_UND:
            cpu->cpsr.m = M_UND;
            break;
        case I_IRQ:
            cpu->cpsr.m = M_IRQ;
            break;
        case I_FIQ:
            cpu->cpsr.m = M_FIQ;
            break;
    }
    cpu_update_mode(cpu, old);
    cpu->spsr = spsr;
    cpu->lr = cpu->pc;
    if (!cpu->cpsr.t) cpu->lr -= 4;
    cpu_fetch(cpu);
    cpu->cpsr.t = 0;
    cpu->cpsr.i = 1;
    cpu->pc = 4 * intr;
    cpu_flush(cpu);
}

byte cpu_readb(Arm7TDMI* cpu, word addr) {
    cpu->cycles++;
    return gba_readb(cpu->master, addr, &cpu->cycles);
}

hword cpu_readh(Arm7TDMI* cpu, word addr) {
    cpu->cycles++;
    return gba_readh(cpu->master, addr, &cpu->cycles);
}

word cpu_read(Arm7TDMI* cpu, word addr) {
    cpu->cycles++;
    return gba_read(cpu->master, addr, &cpu->cycles);
}

void cpu_writeb(Arm7TDMI* cpu, word addr, byte b) {
    cpu->cycles++;
    gba_writeb(cpu->master, addr, b, &cpu->cycles);
}

void cpu_writeh(Arm7TDMI* cpu, word addr, hword h) {
    cpu->cycles++;
    gba_writeh(cpu->master, addr, h, &cpu->cycles);
}

void cpu_write(Arm7TDMI* cpu, word addr, word w) {
    cpu->cycles++;
    gba_write(cpu->master, addr, w, &cpu->cycles);
}

void cpu_internal_cycle(Arm7TDMI* cpu) {
    cpu->cycles++;
}