#include "arm_isa.h"

#include <stdio.h>

#include "arm7tdmi.h"
#include "gba.h"

ArmExecFunc arm_lookup[1 << 12];

void arm_generate_lookup() {
    for (int i = 0; i < 1 << 12; i++) {
        arm_lookup[i] =
            arm_decode_instr((ArmInstr){(((i & 0xf) << 4) | (i >> 4 << 20))});
    }
}

ArmExecFunc arm_decode_instr(ArmInstr instr) {
    if (instr.sw_intr.c1 == 0b1111) {
        return exec_arm_sw_intr;
    } else if (instr.branch.c1 == 0b101) {
        return exec_arm_branch;
    } else if (instr.block_trans.c1 == 0b100) {
        return exec_arm_block_trans;
    } else if (instr.undefined.c1 == 0b011 && instr.undefined.c2 == 1) {
        return exec_arm_undefined;
    } else if (instr.single_trans.c1 == 0b01) {
        return exec_arm_single_trans;
    } else if (instr.branch_ex.c1 == 0b00010010 &&
               instr.branch_ex.c3 == 0b0001) {
        return exec_arm_branch_ex;
    } else if (instr.swap.c1 == 0b00010 && instr.swap.c2 == 0b00 &&
               instr.swap.c4 == 0b1001) {
        return exec_arm_swap;
    } else if (instr.multiply.c1 == 0b000000 && instr.multiply.c2 == 0b1001) {
        return exec_arm_multiply;
    } else if (instr.multiply_long.c1 == 0b00001 &&
               instr.multiply_long.c2 == 0b1001) {
        return exec_arm_multiply_long;
    } else if (instr.half_trans.c1 == 0b000 && instr.half_trans.c2 == 1 &&
               instr.half_trans.c3 == 1) {
        return exec_arm_half_trans;
    } else if (instr.psr_trans.c1 == 0b00 && instr.psr_trans.c2 == 0b10 &&
               instr.psr_trans.c3 == 0) {
        return exec_arm_psr_trans;
    } else {
        return exec_arm_data_proc;
    }
}

bool eval_cond(Arm7TDMI* cpu, ArmInstr instr) {
    if (instr.cond == C_AL) return true;
    switch (instr.cond) {
        case C_EQ:
            return cpu->cpsr.z;
        case C_NE:
            return !cpu->cpsr.z;
        case C_CS:
            return cpu->cpsr.c;
        case C_CC:
            return !cpu->cpsr.c;
        case C_MI:
            return cpu->cpsr.n;
        case C_PL:
            return !cpu->cpsr.n;
        case C_VS:
            return cpu->cpsr.v;
        case C_VC:
            return !cpu->cpsr.v;
        case C_HI:
            return cpu->cpsr.c && !cpu->cpsr.z;
        case C_LS:
            return !cpu->cpsr.c || cpu->cpsr.z;
        case C_GE:
            return cpu->cpsr.n == cpu->cpsr.v;
        case C_LT:
            return cpu->cpsr.n != cpu->cpsr.v;
        case C_GT:
            return !cpu->cpsr.z && (cpu->cpsr.n == cpu->cpsr.v);
        case C_LE:
            return cpu->cpsr.z || (cpu->cpsr.n != cpu->cpsr.v);
        default:
            return true;
    }
}

void arm_exec_instr(Arm7TDMI* cpu) {
    ArmInstr instr = cpu->cur_instr;
    if (!eval_cond(cpu, instr)) {
        cpu_fetch_instr(cpu);
        return;
    }

    arm_lookup[(((instr.w >> 4) & 0xf) | (instr.w >> 20 << 4)) % (1 << 12)](
        cpu, instr);
}

word arm_shifter(Arm7TDMI* cpu, byte shift, word operand, word* carry) {
    word shift_type = (shift >> 1) & 0b11;
    word shift_amt = shift >> 3;
    if (shift_amt) {
        switch (shift_type) {
            case S_LSL:
                *carry = (operand >> (32 - shift_amt)) & 1;
                return operand << shift_amt;
            case S_LSR:
                *carry = (operand >> (shift_amt - 1)) & 1;
                return operand >> shift_amt;
            case S_ASR:
                *carry = (operand >> (shift_amt - 1)) & 1;
                return (sword) operand >> shift_amt;
            case S_ROR:
                *carry = (operand >> (shift_amt - 1)) & 1;
                return (operand >> shift_amt) | (operand << (32 - shift_amt));
        }
    } else {
        switch (shift_type) {
            case S_LSL:
                return operand;
            case S_LSR:
                *carry = operand >> 31;
                return 0;
            case S_ASR:
                *carry = operand >> 31;
                return (operand >> 31) ? -1 : 0;
            case S_ROR:
                *carry = operand & 1;
                return (operand >> 1) | (cpu->cpsr.c << 31);
        }
    }
    return 0;
}

void exec_arm_data_proc(Arm7TDMI* cpu, ArmInstr instr) {
    word op1, op2;

    word z, c = cpu->cpsr.c, n, v = cpu->cpsr.v;
    if (instr.data_proc.i) {
        if (cpu->cpsr.t) {
            op2 = instr.data_proc.op2;
        } else {
            op2 = instr.data_proc.op2 & 0xff;
            word shift_amt = instr.data_proc.op2 >> 8;
            if (shift_amt) {
                shift_amt *= 2;
                c = (op2 >> (shift_amt - 1)) & 1;
                op2 = (op2 >> shift_amt) | (op2 << (32 - shift_amt));
            }
        }
        op1 = cpu->r[instr.data_proc.rn];
        cpu_fetch_instr(cpu);
    } else {
        word rm = instr.data_proc.op2 & 0b1111;
        word shift = instr.data_proc.op2 >> 4;

        if (shift & 1) {
            cpu_fetch_instr(cpu);
            cpu_internal_cycle(cpu, 1);
            op2 = cpu->r[rm];

            word rs = shift >> 4;
            word shift_amt = cpu->r[rs] & 0xff;

            if (shift_amt >= 32) {
                switch ((shift >> 1) & 0b11) {
                    case S_LSL:
                        if (shift_amt == 32) c = op2 & 1;
                        else c = 0;
                        op2 = 0;
                        break;
                    case S_LSR:
                        if (shift_amt == 32) c = op2 >> 31;
                        else c = 0;
                        op2 = 0;
                        break;
                    case S_ASR:
                        if (op2 >> 31) {
                            c = 1;
                            op2 = -1;
                        } else {
                            c = 0;
                            op2 = 0;
                        }
                        break;
                    case S_ROR:
                        shift_amt %= 32;
                        c = (op2 >> (shift_amt - 1)) & 1;
                        op2 = (op2 >> shift_amt) | (op2 << (32 - shift_amt));
                        break;
                }
            } else if (shift_amt > 0) {
                op2 =
                    arm_shifter(cpu, (shift & 0b111) | shift_amt << 3, op2, &c);
            }

            op1 = cpu->r[instr.data_proc.rn];
        } else {
            op2 = arm_shifter(cpu, shift, cpu->r[rm], &c);
            op1 = cpu->r[instr.data_proc.rn];
            cpu_fetch_instr(cpu);
        }
    }
    if (instr.data_proc.rn == 15 && instr.data_proc.rd != 15) op1 &= ~0b10;

    if (instr.data_proc.s) {
        word res = 0;
        bool arith = false;
        word car = 0;
        word tmp;
        bool save = true;
        switch (instr.data_proc.opcode) {
            case A_AND:
                res = op1 & op2;
                break;
            case A_EOR:
                res = op1 ^ op2;
                break;
            case A_SUB:
                arith = true;
                op2 = ~op2;
                car = 1;
                break;
            case A_RSB:
                arith = true;
                tmp = op1;
                op1 = op2;
                op2 = ~tmp;
                car = 1;
                break;
            case A_ADD:
                arith = true;
                break;
            case A_ADC:
                arith = true;
                car = cpu->cpsr.c;
                break;
            case A_SBC:
                arith = true;
                op2 = ~op2;
                car = cpu->cpsr.c;
                break;
            case A_RSC:
                arith = true;
                tmp = op1;
                op1 = op2;
                op2 = ~tmp;
                car = cpu->cpsr.c;
                break;
            case A_TST:
                res = op1 & op2;
                save = false;
                break;
            case A_TEQ:
                res = op1 ^ op2;
                save = false;
                break;
            case A_CMP:
                arith = true;
                op2 = ~op2;
                car = 1;
                save = false;
                break;
            case A_CMN:
                arith = true;
                save = false;
                break;
            case A_ORR:
                res = op1 | op2;
                break;
            case A_MOV:
                res = op2;
                break;
            case A_BIC:
                res = op1 & ~op2;
                break;
            case A_MVN:
                res = ~op2;
                break;
        }

        if (arith) {
            res = op1 + op2;
            c = (op1 > res) || (res > res + car);
            res += car;
            v = (op1 >> 31) == (op2 >> 31) && (op1 >> 31) != (res >> 31);
        }
        z = (res == 0) ? 1 : 0;
        n = (res >> 31) & 1;

        if (instr.data_proc.rd == 15) {
            CpuMode mode = cpu->cpsr.m;
            if (!(mode == M_USER || mode == M_SYSTEM)) {
                cpu->cpsr.w = cpu->spsr;
                cpu_update_mode(cpu, mode);
            }
        } else {
            cpu->cpsr.z = z;
            cpu->cpsr.n = n;
            cpu->cpsr.c = c;
            cpu->cpsr.v = v;
        }
        if (save) {
            cpu->r[instr.data_proc.rd] = res;
            if (instr.data_proc.rd == 15) cpu_flush(cpu);
        }
    } else {
        word rd = instr.data_proc.rd;
        switch (instr.data_proc.opcode) {
            case A_AND:
                cpu->r[rd] = op1 & op2;
                break;
            case A_EOR:
                cpu->r[rd] = op1 ^ op2;
                break;
            case A_SUB:
                cpu->r[rd] = op1 - op2;
                break;
            case A_RSB:
                cpu->r[rd] = op2 - op1;
                break;
            case A_ADD:
                cpu->r[rd] = op1 + op2;
                break;
            case A_ADC:
                cpu->r[rd] = op1 + op2 + cpu->cpsr.c;
                break;
            case A_SBC:
                cpu->r[rd] = op1 - op2 - 1 + cpu->cpsr.c;
                break;
            case A_RSC:
                cpu->r[rd] = op2 - op1 - 1 + cpu->cpsr.c;
                break;
            case A_TST:
                return;
            case A_TEQ:
                return;
            case A_CMP:
                return;
            case A_CMN:
                return;
            case A_ORR:
                cpu->r[rd] = op1 | op2;
                break;
            case A_MOV:
                cpu->r[rd] = op2;
                break;
            case A_BIC:
                cpu->r[rd] = op1 & ~op2;
                break;
            case A_MVN:
                cpu->r[rd] = ~op2;
                break;
        }
        if (instr.data_proc.rd == 15) cpu_flush(cpu);
    }
}

void exec_arm_psr_trans(Arm7TDMI* cpu, ArmInstr instr) {
    if (instr.psr_trans.op) {
        word op2;
        if (instr.psr_trans.i) {
            op2 = instr.psr_trans.op2 & 0xff;
            word rot = instr.psr_trans.op2 >> 7;
            op2 = (op2 >> rot) | (op2 << (32 - rot));
        } else {
            word rm = instr.psr_trans.op2 & 0b1111;
            op2 = cpu->r[rm];
        }
        word mask = 0;
        if (instr.psr_trans.f) mask |= 0xff000000;
        if (instr.psr_trans.s) mask |= 0x00ff0000;
        if (instr.psr_trans.x) mask |= 0x0000ff00;
        if (instr.psr_trans.c) mask |= 0x000000ff;
        if (cpu->cpsr.m == M_USER) mask &= 0xf0000000;
        op2 &= mask;
        if (instr.psr_trans.p) {
            cpu->spsr &= ~mask;
            cpu->spsr |= op2;
        } else {
            CpuMode m = cpu->cpsr.m;
            cpu->cpsr.w &= ~mask;
            cpu->cpsr.w |= op2;
            cpu_update_mode(cpu, m);
        }
    } else {
        word psr;
        if (instr.psr_trans.p) {
            psr = cpu->spsr;
        } else {
            psr = cpu->cpsr.w;
        }
        cpu->r[instr.psr_trans.rd] = psr;
    }
    cpu_fetch_instr(cpu);
}

void exec_arm_multiply(Arm7TDMI* cpu, ArmInstr instr) {
    cpu_fetch_instr(cpu);
    sword op = cpu->r[instr.multiply.rs];
    int cycles = 0;
    for (int i = 0; i < 4; i++) {
        op >>= 8;
        cycles++;
        if (op == 0 || op == -1) break;
    }
    word res = cpu->r[instr.multiply.rm] * cpu->r[instr.multiply.rs];
    if (instr.multiply.a) {
        cycles++;
        res += cpu->r[instr.multiply.rn];
    }
    cpu_internal_cycle(cpu, cycles);
    cpu->r[instr.multiply.rd] = res;
    if (instr.multiply.s) {
        cpu->cpsr.z = (cpu->r[instr.multiply.rd] == 0) ? 1 : 0;
        cpu->cpsr.n = (cpu->r[instr.multiply.rd] >> 31) & 1;
    }
}

void exec_arm_multiply_long(Arm7TDMI* cpu, ArmInstr instr) {
    cpu_fetch_instr(cpu);
    cpu_internal_cycle(cpu, 1);
    sword op = cpu->r[instr.multiply_long.rs];
    int cycles = 0;
    for (int i = 1; i <= 4; i++) {
        cycles++;
        op >>= 8;
        if (op == 0 || (op == -1 && instr.multiply_long.u)) break;
    }
    dword res;
    if (instr.multiply_long.u) {
        sdword sres;
        sres = (sdword) ((sword) cpu->r[instr.multiply_long.rm]) *
               (sdword) ((sword) cpu->r[instr.multiply_long.rs]);
        res = sres;
    } else {
        res = (dword) cpu->r[instr.multiply_long.rm] *
              (dword) cpu->r[instr.multiply_long.rs];
    }
    if (instr.multiply_long.a) {
        cycles++;
        res += (dword) cpu->r[instr.multiply_long.rdlo] |
               ((dword) cpu->r[instr.multiply_long.rdhi] << 32);
    }
    cpu_internal_cycle(cpu, cycles);
    if (instr.multiply_long.s) {
        cpu->cpsr.z = (res == 0) ? 1 : 0;
        cpu->cpsr.n = (res >> 63) & 1;
    }
    cpu->r[instr.multiply_long.rdlo] = res;
    cpu->r[instr.multiply_long.rdhi] = res >> 32;
}

void exec_arm_swap(Arm7TDMI* cpu, ArmInstr instr) {
    word addr = cpu->r[instr.swap.rn];
    cpu_fetch_instr(cpu);
    if (instr.swap.b) {
        cpu->r[instr.swap.rd] = cpu_swapb(cpu, addr, cpu->r[instr.swap.rm]);
    } else {
        cpu->r[instr.swap.rd] = cpu_swapw(cpu, addr, cpu->r[instr.swap.rm]);
    }

    cpu->next_seq = false;
}

void exec_arm_branch_ex(Arm7TDMI* cpu, ArmInstr instr) {
    cpu_fetch_instr(cpu);
    cpu->pc = cpu->r[instr.branch_ex.rn];
    cpu->cpsr.t = cpu->r[instr.branch_ex.rn] & 1;
    cpu_flush(cpu);
}

void exec_arm_half_trans(Arm7TDMI* cpu, ArmInstr instr) {
    word addr = cpu->r[instr.half_trans.rn];
    word offset;
    if (instr.half_trans.i) {
        offset = instr.half_trans.offlo | (instr.half_trans.offhi << 4);
    } else {
        offset = cpu->r[instr.half_trans.offlo];
    }
    cpu_fetch_instr(cpu);

    if (!instr.half_trans.u) offset = -offset;
    word wback = addr + offset;
    if (instr.half_trans.p) addr = wback;

    if (instr.half_trans.s) {
        if (instr.half_trans.l) {
            if (instr.half_trans.w || !instr.half_trans.p) {
                cpu->r[instr.half_trans.rn] = wback;
            }
            if (instr.half_trans.h) {
                cpu->r[instr.half_trans.rd] = cpu_readh(cpu, addr, true);
            } else {
                cpu->r[instr.half_trans.rd] = cpu_readb(cpu, addr, true);
            }
            cpu_internal_cycle(cpu, 1);
            if (instr.half_trans.rd == 15) cpu_flush(cpu);
        }
    } else if (instr.half_trans.h) {
        if (instr.half_trans.l) {
            if (instr.half_trans.w || !instr.half_trans.p) {
                cpu->r[instr.half_trans.rn] = wback;
            }
            cpu->r[instr.half_trans.rd] = cpu_readh(cpu, addr, false);
            cpu_internal_cycle(cpu, 1);
            if (instr.half_trans.rd == 15) cpu_flush(cpu);
        } else {
            cpu_writeh(cpu, addr, cpu->r[instr.half_trans.rd]);
            if (instr.half_trans.w || !instr.half_trans.p) {
                cpu->r[instr.half_trans.rn] = wback;
            }
            cpu->next_seq = false;
        }
    }
}

void exec_arm_single_trans(Arm7TDMI* cpu, ArmInstr instr) {
    word addr = cpu->r[instr.single_trans.rn];
    if (instr.single_trans.rn == 15) addr &= ~0b10;
    word offset;
    if (instr.single_trans.i) {
        word rm = instr.single_trans.offset & 0b1111;
        offset = cpu->r[rm];
        byte shift = instr.single_trans.offset >> 4;
        word carry;
        offset = arm_shifter(cpu, shift, offset, &carry);
    } else {
        offset = instr.single_trans.offset;
    }

    cpu_fetch_instr(cpu);

    if (!instr.single_trans.u) offset = -offset;
    word wback = addr + offset;
    if (instr.single_trans.p) addr = wback;

    if (instr.single_trans.b) {
        if (instr.single_trans.l) {
            if (instr.single_trans.w || !instr.single_trans.p) {
                cpu->r[instr.single_trans.rn] = wback;
            }
            cpu->r[instr.single_trans.rd] = cpu_readb(cpu, addr, false);
            cpu_internal_cycle(cpu, 1);
            if (instr.single_trans.rd == 15) cpu_flush(cpu);
        } else {
            cpu_writeb(cpu, addr, cpu->r[instr.single_trans.rd]);
            if (instr.single_trans.w || !instr.single_trans.p) {
                cpu->r[instr.single_trans.rn] = wback;
            }
            cpu->next_seq = false;
        }
    } else {
        if (instr.single_trans.l) {
            if (instr.single_trans.w || !instr.single_trans.p) {
                cpu->r[instr.single_trans.rn] = wback;
            }
            cpu->r[instr.single_trans.rd] = cpu_readw(cpu, addr);
            cpu_internal_cycle(cpu, 1);
            if (instr.single_trans.rd == 15) cpu_flush(cpu);
        } else {
            cpu_writew(cpu, addr, cpu->r[instr.single_trans.rd]);
            if (instr.single_trans.w || !instr.single_trans.p) {
                cpu->r[instr.single_trans.rn] = wback;
            }
            cpu->next_seq = false;
        }
    }
}

void exec_arm_undefined(Arm7TDMI* cpu, ArmInstr instr) {
    cpu_handle_interrupt(cpu, I_UND);
}

void exec_arm_block_trans(Arm7TDMI* cpu, ArmInstr instr) {
    int rcount = 0;
    int rlist[16];
    word addr = cpu->r[instr.block_trans.rn];
    word wback = addr;
    if (instr.block_trans.rlist) {
        for (int i = 0; i < 16; i++) {
            if (instr.block_trans.rlist & (1 << i)) rlist[rcount++] = i;
        }
        if (instr.block_trans.u) {
            wback += 4 * rcount;
        } else {
            wback -= 4 * rcount;
            addr = wback;
        }
    } else {
        rcount = 1;
        rlist[0] = 15;
        if (instr.block_trans.u) {
            wback += 0x40;
        } else {
            wback -= 0x40;
            addr = wback;
        }
    }

    if (instr.block_trans.p == instr.block_trans.u) addr += 4;
    cpu_fetch_instr(cpu);

    bool user_trans =
        instr.block_trans.s &&
        !((instr.block_trans.rlist & (1 << 15)) && instr.block_trans.l);
    CpuMode mode = cpu->cpsr.m;
    if (user_trans) {
        cpu->cpsr.m = M_USER;
        cpu_update_mode(cpu, mode);
    }

    if (instr.block_trans.l) {
        if (instr.block_trans.w) cpu->r[instr.block_trans.rn] = wback;
        for (int i = 0; i < rcount; i++) {
            cpu->r[rlist[i]] = cpu_readm(cpu, addr, i);
        }
        cpu_internal_cycle(cpu, 1);
        if ((instr.block_trans.rlist & (1 << 15)) || !instr.block_trans.rlist) {
            if (instr.block_trans.s) {
                CpuMode mode = cpu->cpsr.m;
                if (!(mode == M_USER || mode == M_SYSTEM)) {
                    cpu->cpsr.w = cpu->spsr;
                    cpu_update_mode(cpu, mode);
                }
            }
            cpu_flush(cpu);
        }
    } else {
        for (int i = 0; i < rcount; i++) {
            cpu_writem(cpu, addr, i, cpu->r[rlist[i]]);
            if (i == 0 && instr.block_trans.w)
                cpu->r[instr.block_trans.rn] = wback;
        }
        cpu->next_seq = false;
    }

    if (user_trans) {
        cpu->cpsr.m = mode;
        cpu_update_mode(cpu, M_USER);
    }
}

void exec_arm_branch(Arm7TDMI* cpu, ArmInstr instr) {
    word offset = instr.branch.offset;
    if (offset & (1 << 23)) offset |= 0xff000000;
    if (cpu->cpsr.t) offset <<= 1;
    else offset <<= 2;
    word dest = cpu->pc + offset;
    if (instr.branch.l) {
        if (cpu->cpsr.t) {
            if (offset & (1 << 23)) {
                offset %= 1 << 23;
                cpu->lr += offset;
                dest = cpu->lr;
                cpu->lr = (cpu->pc - 2) | 1;
            } else {
                if (offset & (1 << 22)) dest += 0xff800000;
                cpu->lr = dest;
                cpu_fetch_instr(cpu);
                return;
            }
        } else {
            cpu->lr = (cpu->pc - 4) & ~0b11;
        }
    }
    cpu_fetch_instr(cpu);
    cpu->pc = dest;
    cpu_flush(cpu);
}

void exec_arm_sw_intr(Arm7TDMI* cpu, ArmInstr instr) {
    cpu_handle_interrupt(cpu, I_SWI);
}

void arm_disassemble(ArmInstr instr, word addr, FILE* out) {

    static char* reg_names[16] = {"r0", "r1", "r2", "r3", "r4",  "r5",
                                  "r6", "r7", "r8", "r9", "r10", "r11",
                                  "ip", "sp", "lr", "pc"};
    static char* cond_names[16] = {"eq", "ne", "hs", "lo", "mi", "pl",
                                   "vs", "vc", "hi", "ls", "ge", "lt",
                                   "gt", "le", "",   ""};
    static char* alu_names[16] = {"and", "eor", "sub", "rsb", "add", "adc",
                                  "sbc", "rsc", "tst", "teq", "cmp", "cmn",
                                  "orr", "mov", "bic", "mvn"};
    static char* shift_names[5] = {"lsl", "lsr", "asr", "ror", "rrx"};

    char* cond = cond_names[instr.cond];

    if (instr.sw_intr.c1 == 0b1111) {

        fprintf(out, "swi%s 0x%x", cond, instr.sw_intr.arg);

    } else if (instr.branch.c1 == 0b101) {

        word off = instr.branch.offset;
        if (off & (1 << 23)) off |= 0xff000000;
        fprintf(out, "b%s%s 0x%x", instr.branch.l ? "l" : "", cond,
                addr + 8 + (off << 2));

    } else if (instr.block_trans.c1 == 0b100) {

        if (instr.block_trans.rn == 13 &&
            (instr.block_trans.u != instr.block_trans.p) &&
            instr.block_trans.u == instr.block_trans.l && instr.block_trans.w) {
            fprintf(out, "%s%s {", instr.block_trans.l ? "pop" : "push", cond);
        } else {
            fprintf(out, "%s%s%s%s %s%s, {",
                    instr.block_trans.l ? "ldm" : "stm",
                    instr.block_trans.u ? "i" : "d",
                    instr.block_trans.p ? "b" : "a", cond,
                    reg_names[instr.block_trans.rn],
                    instr.block_trans.w ? "!" : "");
        }
        hword rlist = instr.block_trans.rlist;
        for (int i = 0; i < 16; i++) {
            if (rlist & 1) {
                fprintf(out, "%s", reg_names[i]);
                rlist >>= 1;
                if (rlist) {
                    fprintf(out, ", ");
                }
            } else rlist >>= 1;
        }
        fprintf(out, "}%s", instr.block_trans.s ? "^" : "");

    } else if (instr.undefined.c1 == 0b011 && instr.undefined.c2 == 1) {

        fprintf(out, "undefined");

    } else if (instr.single_trans.c1 == 0b01) {

        if (!instr.single_trans.i && instr.single_trans.rn == 15 &&
            instr.single_trans.p) {
            fprintf(out, "%s%s%s %s, [", instr.single_trans.l ? "ldr" : "str",
                    instr.single_trans.b ? "b" : "", cond,
                    reg_names[instr.single_trans.rd]);

            word offset = instr.single_trans.offset;
            if (!instr.single_trans.u) offset = -offset;
            fprintf(out, "0x%x", addr + 8 + offset);
            fprintf(out, "]%s", instr.single_trans.w ? "!" : "");
        } else {
            fprintf(out, "%s%s%s %s, [%s", instr.single_trans.l ? "ldr" : "str",
                    instr.single_trans.b ? "b" : "", cond,
                    reg_names[instr.single_trans.rd],
                    reg_names[instr.single_trans.rn]);
            if (!instr.single_trans.p) {
                fprintf(out, "]");
            }
            if (instr.single_trans.i) {
                word rm = instr.single_trans.offset & 0b1111;
                word shift = instr.single_trans.offset >> 4;
                fprintf(out, ", %s%s", instr.single_trans.u ? "" : "-",
                        reg_names[rm]);
                if (shift) {
                    word shift_type = (shift >> 1) & 0b11;
                    word shift_amt = (shift >> 3) & 0b11111;
                    if (!shift_amt) {
                        if (shift_type == S_ROR) {
                            shift_type = 4;
                            shift_amt = 1;
                        } else {
                            shift_amt = 32;
                        }
                    }
                    fprintf(out, ", %s #%d", shift_names[shift_type],
                            shift_amt);
                }
            } else if (instr.single_trans.offset) {
                fprintf(out, ", #%s0x%x", instr.single_trans.u ? "" : "-",
                        instr.single_trans.offset);
            }
            if (instr.single_trans.p) {
                fprintf(out, "]%s", instr.single_trans.w ? "!" : "");
            }
        }

    } else if (instr.branch_ex.c1 == 0b00010010 &&
               instr.branch_ex.c3 == 0b0001) {

        fprintf(out, "bx%s %s", cond, reg_names[instr.branch_ex.rn]);

    } else if (instr.swap.c1 == 0b00010 && instr.swap.c2 == 0b00 &&
               instr.swap.c4 == 0b1001) {

        fprintf(out, "swap%s%s %s, %s, [%s]", instr.swap.b ? "b" : "", cond,
                reg_names[instr.swap.rd], reg_names[instr.swap.rm],
                reg_names[instr.swap.rn]);

    } else if (instr.multiply.c1 == 0b000000 && instr.multiply.c2 == 0b1001) {

        if (instr.multiply.a) {
            fprintf(out, "mla%s%s %s, %s, %s, %s", instr.multiply.s ? "s" : "",
                    cond, reg_names[instr.multiply.rd],
                    reg_names[instr.multiply.rm], reg_names[instr.multiply.rs],
                    reg_names[instr.multiply.rn]);
        } else {
            fprintf(out, "mul%s%s %s, %s, %s", instr.multiply.s ? "s" : "",
                    cond, reg_names[instr.multiply.rd],
                    reg_names[instr.multiply.rm], reg_names[instr.multiply.rs]);
        }

    } else if (instr.multiply_long.c1 == 0b00001 &&
               instr.multiply_long.c2 == 0b1001) {

        fprintf(out, "%s%s%s%s %s, %s, %s, %s",
                instr.multiply_long.u ? "s" : "u",
                instr.multiply_long.a ? "mlal" : "mull",
                instr.multiply_long.s ? "s" : "", cond,
                reg_names[instr.multiply_long.rdlo],
                reg_names[instr.multiply_long.rdhi],
                reg_names[instr.multiply_long.rm],
                reg_names[instr.multiply_long.rs]);

    } else if (instr.half_trans.c1 == 0b000 && instr.half_trans.c2 == 1 &&
               instr.half_trans.c3 == 1) {

        if (instr.half_trans.i && instr.half_trans.rn == 15 &&
            instr.half_trans.p) {
            fprintf(out, "%s%s%s%s %s, [", instr.half_trans.l ? "ldr" : "str",
                    instr.half_trans.s ? "s" : "",
                    instr.half_trans.h ? "h" : "b", cond,
                    reg_names[instr.half_trans.rd]);

            word offset =
                instr.half_trans.offlo | (instr.half_trans.offhi << 4);
            if (!instr.half_trans.u) offset = -offset;
            fprintf(out, "0x%x", addr + 8 + offset);
            fprintf(out, "]%s", instr.half_trans.w ? "!" : "");
        } else {
            fprintf(out, "%s%s%s%s %s, [%s", instr.half_trans.l ? "ldr" : "str",
                    instr.half_trans.s ? "s" : "",
                    instr.half_trans.h ? "h" : "b", cond,
                    reg_names[instr.half_trans.rd],
                    reg_names[instr.half_trans.rn]);
            if (!instr.half_trans.p) {
                fprintf(out, "]");
            }
            if (instr.half_trans.i) {
                word offset =
                    instr.half_trans.offlo | (instr.half_trans.offhi << 4);
                if (offset) {
                    fprintf(out, ", #%s0x%x", instr.half_trans.u ? "" : "-",
                            offset);
                }
            } else {
                fprintf(out, ", %s%s", instr.half_trans.u ? "" : "-",
                        reg_names[instr.half_trans.offlo]);
            }
            if (instr.half_trans.p) {
                fprintf(out, "]%s", instr.half_trans.w ? "!" : "");
            }
        }

    } else if (instr.psr_trans.c1 == 0b00 && instr.psr_trans.c2 == 0b10 &&
               instr.psr_trans.c3 == 0) {

        if (instr.psr_trans.op) {
            fprintf(out, "msr%s %s_%s%s, ", cond,
                    instr.psr_trans.p ? "spsr" : "cpsr",
                    instr.psr_trans.c ? "c" : "", instr.psr_trans.f ? "f" : "");
            if (instr.psr_trans.i) {
                word op2 = instr.psr_trans.op2 & 0xff;
                word rot = (instr.psr_trans.op2 >> 8) << 1;
                op2 = (op2 >> rot) | (op2 << (32 - rot));
                fprintf(out, "#0x%x", op2);
            } else {
                fprintf(out, "%s", reg_names[instr.psr_trans.op2 & 0xf]);
            }
        } else {
            fprintf(out, "mrs%s %s, %s", cond, reg_names[instr.psr_trans.rd],
                    instr.psr_trans.p ? "spsr" : "cpsr");
        }

    } else {

        if (instr.data_proc.i && instr.data_proc.rn == 15 &&
            (instr.data_proc.opcode == A_ADD ||
             instr.data_proc.opcode == A_SUB)) {
            fprintf(out, "adr%s%s %s, [", instr.data_proc.s ? "s" : "", cond,
                    reg_names[instr.data_proc.rd]);
            word offset = instr.data_proc.op2 & 0xff;
            word rot = (instr.data_proc.op2 >> 8) << 1;
            offset = (offset >> rot) | (offset << (32 - rot));
            if (instr.data_proc.opcode == A_SUB) offset = -offset;
            fprintf(out, "0x%x]", addr + 8 + offset);
        } else {

            if (instr.data_proc.opcode >> 2 == 0b10) {
                fprintf(out, "%s%s %s", alu_names[instr.data_proc.opcode], cond,
                        reg_names[instr.data_proc.rn]);
            } else {
                fprintf(out, "%s%s%s %s", alu_names[instr.data_proc.opcode],
                        instr.data_proc.s ? "s" : "", cond,
                        reg_names[instr.data_proc.rd]);
                if (!(instr.data_proc.opcode == A_MOV ||
                      instr.data_proc.opcode == A_MVN)) {
                    fprintf(out, ", %s", reg_names[instr.data_proc.rn]);
                }
            }

            if (instr.data_proc.i) {
                word op2 = instr.data_proc.op2 & 0xff;
                word rot = (instr.data_proc.op2 >> 8) << 1;
                fprintf(out, ", #0x%x", (op2 >> rot) | (op2 << (32 - rot)));
            } else {
                word rm = instr.data_proc.op2 & 0b1111;
                word shift = instr.data_proc.op2 >> 4;
                fprintf(out, ", %s", reg_names[rm]);
                word shift_type = (shift >> 1) & 0b11;
                if (shift & 1) {
                    word shift_reg = shift >> 4;
                    fprintf(out, ", %s %s", shift_names[shift_type],
                            reg_names[shift_reg]);
                } else if (shift) {
                    word shift_amt = (shift >> 3) & 0b11111;
                    if (!shift_amt) {
                        if (shift_type == S_ROR) {
                            shift_type = 4;
                            shift_amt = 1;
                        } else {
                            shift_amt = 32;
                        }
                    }
                    fprintf(out, ", %s #%d", shift_names[shift_type],
                            shift_amt);
                }
            }
        }
    }
}