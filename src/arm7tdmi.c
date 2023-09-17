#include "arm7tdmi.h"

#include <signal.h>
#include <stdio.h>

#include "arm_isa.h"
#include "gba.h"
#include "thumb_isa.h"
#include "types.h"

extern word bkpt;

void tick_cpu(Arm7TDMI* cpu) {
    if (cpu->master->halt) {
        if (cpu->master->io.ie.h & cpu->master->io.ifl.h) {
            cpu->master->halt = false;
            cpu_handle_interrupt(cpu, I_IRQ);
        } else {
            tick_gba(cpu->master);
        }
        return;
    }
    word cur_addr = (cpu->cpsr.t) ? (cpu->pc - 4) : (cpu->pc - 8);
    if (cur_addr == bkpt) {
        printf("Breakpoint at %08x hit\n", bkpt);
        print_cpu_state(cpu);
        // raise(SIGINT);
    }
    if (!cpu->cpsr.i && (cpu->master->io.ime & 1) &&
        (cpu->master->io.ie.h & cpu->master->io.ifl.h)) {
        cpu_handle_interrupt(cpu, I_IRQ);
    } else {
        arm_exec_instr(cpu);
    }
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
    if (old == M_FIQ && cpu->cpsr.m != M_FIQ) {
        for (int i = 0; i < 5; i++) {
            cpu->banked_r8_12[1][i] = cpu->r[8 + i];
            cpu->r[8 + i] = cpu->banked_r8_12[0][i];
        }
    }
    if (old != M_FIQ && cpu->cpsr.m == M_FIQ) {
        for (int i = 0; i < 5; i++) {
            cpu->banked_r8_12[0][i] = cpu->r[8 + i];
            cpu->r[8 + i] = cpu->banked_r8_12[1][i];
        }
    }
}

void cpu_handle_interrupt(Arm7TDMI* cpu, CpuInterrupt intr) {
    // printf("interrupt %d\n", intr);
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
    if (cpu->cpsr.t) {
        if (intr == I_SWI) cpu->lr -= 2;
    } else cpu->lr -= 4;
    cpu_fetch(cpu);
    cpu->cpsr.t = 0;
    cpu->cpsr.i = 1;
    cpu->pc = 4 * intr;
    cpu_flush(cpu);
}

byte cpu_readb(Arm7TDMI* cpu, word addr) {
    run_gba(cpu->master, gba_get_waitstates(cpu->master, addr, D_BYTE));
    return gba_readb(cpu->master, addr);
}

hword cpu_readh(Arm7TDMI* cpu, word addr) {
    run_gba(cpu->master, gba_get_waitstates(cpu->master, addr, D_HWORD));
    return gba_readh(cpu->master, addr);
}

word cpu_read(Arm7TDMI* cpu, word addr) {
    run_gba(cpu->master, gba_get_waitstates(cpu->master, addr, D_WORD));
    return gba_read(cpu->master, addr);
}

void cpu_writeb(Arm7TDMI* cpu, word addr, byte b) {
    run_gba(cpu->master, gba_get_waitstates(cpu->master, addr, D_BYTE));
    gba_writeb(cpu->master, addr, b);
}

void cpu_writeh(Arm7TDMI* cpu, word addr, hword h) {
    run_gba(cpu->master, gba_get_waitstates(cpu->master, addr, D_HWORD));
    gba_writeh(cpu->master, addr, h);
}

void cpu_write(Arm7TDMI* cpu, word addr, word w) {
    run_gba(cpu->master, gba_get_waitstates(cpu->master, addr, D_WORD));
    gba_write(cpu->master, addr, w);
}

void cpu_internal_cycle(Arm7TDMI* cpu) {
    run_gba(cpu->master, 1);
}

char* mode_name(CpuMode m) {
    switch (m) {
        case M_USER:
            return "USER";
        case M_FIQ:
            return "FIQ";
        case M_IRQ:
            return "IRQ";
        case M_SVC:
            return "SVC";
        case M_ABT:
            return "ABT";
        case M_UND:
            return "UND";
        case M_SYSTEM:
            return "SYSTEM";
        default:
            return "ILLEGAL";
    }
}

void print_cpu_state(Arm7TDMI* cpu) {
    printf("r:[");
    for (int i = 0; i < 15; i++) {
        printf("r%d=%08x, ", i, cpu->r[i]);
    }
    printf("pc=%08x]\n", cpu->pc);
    printf("cpsr:%08x(n=%d,z=%d,c=%d,v=%d,i=%d,f=%d,t=%d,m=%s)\n", cpu->cpsr.w,
           cpu->cpsr.n, cpu->cpsr.z, cpu->cpsr.c, cpu->cpsr.v, cpu->cpsr.i,
           cpu->cpsr.v, cpu->cpsr.t,mode_name(cpu->cpsr.m));
}