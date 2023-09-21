#include "arm_isa.h"

#include <stdio.h>

#include "arm7tdmi.h"
#include "gba.h"

bool eval_cond(Arm7TDMI* cpu, ArmInstr instr) {
    switch (instr.cond) {
        case C_AL:
            return true;
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
        cpu_fetch(cpu);
        return;
    }

    if (instr.sw_intr.c1 == 0b1111) {
        exec_arm_sw_intr(cpu, instr);
    } else if (instr.branch.c1 == 0b101) {
        exec_arm_branch(cpu, instr);
    } else if (instr.block_trans.c1 == 0b100) {
        exec_arm_block_trans(cpu, instr);
    } else if (instr.undefined.c1 == 0b011 && instr.undefined.c2 == 1) {
        log_error(cpu->master, "undefined instruction", 0);
        exec_arm_undefined(cpu, instr);
    } else if (instr.single_trans.c1 == 0b01) {
        exec_arm_single_trans(cpu, instr);
    } else if (instr.branch_ex.c1 == 0b000100101111111111110001) {
        exec_arm_branch_ex(cpu, instr);
    } else if (instr.swap.c1 == 0b00010 && instr.swap.c2 == 0b00 &&
               instr.swap.c3 == 0b00001001) {
        exec_arm_swap(cpu, instr);
    } else if (instr.multiply.c1 == 0b000000 && instr.multiply.c2 == 0b1001) {
        exec_arm_multiply(cpu, instr);
    } else if (instr.multiply_long.c1 == 0b00001 &&
               instr.multiply_long.c2 == 0b1001) {
        exec_arm_multiply_long(cpu, instr);
    } else if (instr.half_transr.c1 == 0b000 && instr.half_transr.i == 0 &&
               instr.half_transr.c2 == 0b00001 && instr.half_transr.c3 == 1) {
        exec_arm_half_trans(cpu, instr);
    } else if (instr.half_transi.c1 == 0b000 && instr.half_transi.i == 1 &&
               instr.half_transi.c2 == 1 && instr.half_transi.c3 == 1) {
        exec_arm_half_trans(cpu, instr);
    } else if (instr.psr_trans.c1 == 0b00 && instr.psr_trans.c2 == 0b10 &&
               instr.psr_trans.c3 == 0) {
        exec_arm_psr_trans(cpu, instr);
    } else if (instr.data_proc.c1 == 0b00) {
        exec_arm_data_proc(cpu, instr);
    } else {
        log_error(cpu->master, "illegal instruction", 0);
        cpu_fetch(cpu);
    }
}

void exec_arm_data_proc(Arm7TDMI* cpu, ArmInstr instr) {
    word op2;
    bool shiftr = false;
    word z, c = cpu->cpsr.c, n, v = cpu->cpsr.v;
    if (instr.data_proc.i) {
        if (cpu->cpsr.t) {
            op2 = instr.data_proc.op2;
        } else {
            op2 = instr.data_proc.op2 & 0xff;
            word shift_amt = instr.data_proc.op2 >> 8;
            shift_amt *= 2;
            if (shift_amt) {
                c = (op2 >> (shift_amt - 1)) & 1;
                op2 = (op2 >> shift_amt) | (op2 << (32 - shift_amt));
            }
        }
    } else {
        word shift = instr.data_proc.op2 >> 4;
        word shift_type = (shift >> 1) & 0b11;
        word shift_amt;
        if (shift & 1) {
            cpu_fetch(cpu);
            shiftr = true;
            word rs = shift >> 4;
            shift_amt = cpu->r[rs] & 0xff;
            cpu_internal_cycle(cpu);
        } else {
            shift_amt = shift >> 3;
        }
        word rm = instr.data_proc.op2 & 0b1111;
        op2 = cpu->r[rm];

        if (!(shift_amt == 0 && shiftr)) {
            switch (shift_type) {
                case S_LSL:
                    if (shift_amt > 32) c = 0;
                    else if (shift_amt > 0) c = (op2 >> (32 - shift_amt)) & 1;
                    if (shift_amt >= 32) {
                        op2 = 0;
                    } else {
                        op2 <<= shift_amt;
                    }
                    break;
                case S_LSR:
                    if (shift_amt == 0) shift_amt = 32;
                    if (shift_amt > 32) c = 0;
                    else c = (op2 >> (shift_amt - 1)) & 1;
                    if (shift_amt >= 32) {
                        op2 = 0;
                    } else {
                        op2 >>= shift_amt;
                    }
                    break;
                case S_ASR:
                    if (shift_amt == 0) shift_amt = 32;
                    if (shift_amt > 32) shift_amt = 32;
                    sword sop2 = op2;
                    c = (sop2 >> (shift_amt - 1)) & 1;
                    if (shift_amt == 32) {
                        sop2 = (c) ? -1 : 0;
                    } else {
                        sop2 >>= shift_amt;
                    }
                    op2 = sop2;
                    break;
                case S_ROR:
                    if (shift_amt == 0) {
                        c = op2 & 1;
                        op2 >>= 1;
                        op2 |= cpu->cpsr.c << 31;
                    } else {
                        c = (op2 >> (shift_amt - 1)) & 1;
                        op2 = (op2 >> shift_amt) | (op2 << (32 - shift_amt));
                    }
                    break;
            }
        }
    }
    word op1 = cpu->r[instr.data_proc.rn];
    if (instr.data_proc.rn == 15 && instr.data_proc.rd != 15) op1 &= ~0b10;
    if (!shiftr) cpu_fetch(cpu);

    word res = 0;
    bool arith = false;
    bool sub = false;
    bool rev = false;
    bool car = false;
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
            sub = true;
            break;
        case A_RSB:
            arith = true;
            sub = true;
            rev = true;
            break;
        case A_ADD:
            arith = true;
            break;
        case A_ADC:
            arith = true;
            car = true;
            break;
        case A_SBC:
            arith = true;
            sub = true;
            car = true;
            break;
        case A_RSC:
            arith = true;
            sub = true;
            rev = true;
            car = true;
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
            sub = true;
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
        if (rev) {
            word tmp = op1;
            op1 = op2;
            op2 = tmp;
        }
        if (sub) op2 = ~op2;
        dword dop1 = op1;
        dword dop2 = op2;
        if ((sub && (!car || cpu->cpsr.c)) || (!sub && car && cpu->cpsr.c))
            dop2++;
        dword dres = dop1 + dop2;
        res = dres;
        c = (dres > 0xffffffff) ? 1 : 0;
        v = (op1 >> 31) == (op2 >> 31) && (op1 >> 31) != (res >> 31);
    }
    z = (res == 0) ? 1 : 0;
    n = (res >> 31) & 1;

    if (instr.data_proc.s) {
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
    }

    if (save) {
        cpu->r[instr.data_proc.rd] = res;
        if (instr.data_proc.rd == 15) cpu_flush(cpu);
    }
}

void exec_arm_multiply(Arm7TDMI* cpu, ArmInstr instr) {
    cpu_fetch(cpu);
    word op = cpu->r[instr.multiply.rs];
    for (int i = 1; i <= 4; i++) {
        cpu_internal_cycle(cpu);
        if ((op >> 8 * i) == 0 || (-op >> 8 * i) == 0) break;
    }
    word res = cpu->r[instr.multiply.rm] * cpu->r[instr.multiply.rs];
    if (instr.multiply.a) {
        cpu_internal_cycle(cpu);
        res += cpu->r[instr.multiply.rn];
    }
    cpu->r[instr.multiply.rd] = res;
    if (instr.multiply.s) {
        cpu->cpsr.z = (cpu->r[instr.multiply.rd] == 0) ? 1 : 0;
        cpu->cpsr.n = (cpu->r[instr.multiply.rd] >> 31) & 1;
    }
}

void exec_arm_multiply_long(Arm7TDMI* cpu, ArmInstr instr) {
    cpu_fetch(cpu);
    cpu_internal_cycle(cpu);
    word op = cpu->r[instr.multiply_long.rs];
    for (int i = 1; i <= 4; i++) {
        cpu_internal_cycle(cpu);
        if ((op >> 8 * i) == 0 ||
            ((-op >> 8 * i) == 0 && instr.multiply_long.u))
            break;
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
        cpu_internal_cycle(cpu);
        res += (dword) cpu->r[instr.multiply_long.rdlo] |
               ((dword) cpu->r[instr.multiply_long.rdhi] << 32);
    }
    if (instr.multiply_long.s) {
        cpu->cpsr.z = (res == 0) ? 1 : 0;
        cpu->cpsr.n = (res >> 63) & 1;
    }
    cpu->r[instr.multiply_long.rdlo] = res;
    cpu->r[instr.multiply_long.rdhi] = res >> 32;
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
    cpu_fetch(cpu);
}

void exec_arm_swap(Arm7TDMI* cpu, ArmInstr instr) {
    word addr = cpu->r[instr.swap.rn];
    cpu_fetch(cpu);
    if (instr.swap.b) {
        byte data;
        cpu_internal_cycle(cpu);
        data = cpu_readb(cpu, addr);
        byte tmp = data;
        data = cpu->r[instr.swap.rm];
        cpu_writeb(cpu, addr, data);
        cpu->r[instr.swap.rd] = tmp;
    } else {
        word data;
        cpu_internal_cycle(cpu);
        data = cpu_readw(cpu, addr);
        word rot = (addr % 4) * 8;
        data = data >> rot | data << (32 - rot);
        word tmp = data;
        data = cpu->r[instr.swap.rm];
        cpu_writew(cpu, addr, data);
        cpu->r[instr.swap.rd] = tmp;
    }
}

void exec_arm_branch_ex(Arm7TDMI* cpu, ArmInstr instr) {
    cpu_fetch(cpu);
    cpu->pc = cpu->r[instr.branch_ex.rn];
    cpu->cpsr.t = cpu->r[instr.branch_ex.rn] & 1;
    cpu_flush(cpu);
}

void exec_arm_half_trans(Arm7TDMI* cpu, ArmInstr instr) {
    word addr = cpu->r[instr.half_transi.rn];
    word offset;
    if (instr.half_transi.i) {
        offset = instr.half_transi.offlo | (instr.half_transi.offhi << 4);
    } else {
        offset = cpu->r[instr.half_transr.rm];
    }
    cpu_fetch(cpu);

    if (!instr.half_transi.u) offset = -offset;
    word wback = addr + offset;
    if (instr.half_transi.p) addr = wback;

    if (instr.half_transi.s) {
        sword data;
        if (instr.half_transi.l) {
            cpu_internal_cycle(cpu);
            if (instr.half_transi.w || !instr.half_transi.p) {
                cpu->r[instr.half_transi.rn] = wback;
            }
            if (instr.half_transi.h) {
                data = (shword) cpu_readh(cpu, addr);
                word rot = (addr % 2) * 8;
                data >>= rot;
            } else {
                data = (sbyte) cpu_readb(cpu, addr);
            }
            cpu->r[instr.half_transi.rd] = data;
        }
    } else if (instr.half_transi.h) {
        word data;
        if (instr.half_transi.l) {
            cpu_internal_cycle(cpu);
            if (instr.half_transi.w || !instr.half_transi.p) {
                cpu->r[instr.half_transi.rn] = wback;
            }
            data = cpu_readh(cpu, addr);
            word rot = (addr % 2) * 8;
            data = data >> rot | data << (32 - rot);
            cpu->r[instr.half_transi.rd] = data;
        } else {
            data = cpu->r[instr.half_transi.rd];
            cpu_writeh(cpu, addr, data);
            if (instr.half_transi.w || !instr.half_transi.p) {
                cpu->r[instr.half_transi.rn] = wback;
            }
        }
    }

    if (instr.half_transi.rd == 15 && instr.half_transi.l) cpu_flush(cpu);
}

void exec_arm_single_trans(Arm7TDMI* cpu, ArmInstr instr) {
    word addr = cpu->r[instr.single_trans.rn];
    if (instr.single_trans.rn == 15) addr &= ~0b10;
    word offset;
    if (instr.single_trans.i) {
        word rm = instr.single_trans.offset & 0b1111;
        offset = cpu->r[rm];
        word shift = instr.single_trans.offset >> 4;
        word shift_type = (shift >> 1) & 0b11;
        word shift_amt = shift >> 3;
        switch (shift_type) {
            case S_LSL:
                offset <<= shift_amt;
                break;
            case S_LSR:
                if (shift_amt == 0) {
                    offset = 0;
                } else {
                    offset >>= shift_amt;
                }
                break;
            case S_ASR:
                if (shift_amt == 0) {
                    offset = (offset >> 31) ? -1 : 0;
                } else {
                    sword soff = offset;
                    soff >>= shift_amt;
                    offset = soff;
                }
                break;
            case S_ROR:
                if (shift_amt == 0) {
                    offset >>= 1;
                    offset |= cpu->cpsr.c << 31;
                } else {
                    offset =
                        (offset >> shift_amt) | (offset << (32 - shift_amt));
                }
                break;
        }
    } else {
        offset = instr.single_trans.offset;
    }

    cpu_fetch(cpu);

    if (!instr.single_trans.u) offset = -offset;
    word wback = addr + offset;
    if (instr.single_trans.p) addr = wback;

    if (instr.single_trans.b) {
        byte data;
        if (instr.single_trans.l) {
            cpu_internal_cycle(cpu);
            if (instr.single_trans.w || !instr.single_trans.p) {
                cpu->r[instr.single_trans.rn] = wback;
            }
            data = cpu_readb(cpu, addr);
            cpu->r[instr.single_trans.rd] = data;
        } else {
            data = cpu->r[instr.single_trans.rd];
            cpu_writeb(cpu, addr, data);
            if (instr.single_trans.w || !instr.single_trans.p) {
                cpu->r[instr.single_trans.rn] = wback;
            }
        }
    } else {
        word data;
        if (instr.single_trans.l) {
            cpu_internal_cycle(cpu);
            if (instr.single_trans.w || !instr.single_trans.p) {
                cpu->r[instr.single_trans.rn] = wback;
            }
            data = cpu_readw(cpu, addr);
            word rot = (addr % 4) * 8;
            data = data >> rot | data << (32 - rot);
            cpu->r[instr.single_trans.rd] = data;
        } else {
            data = cpu->r[instr.single_trans.rd];
            cpu_writew(cpu, addr, data);
            if (instr.single_trans.w || !instr.single_trans.p) {
                cpu->r[instr.single_trans.rn] = wback;
            }
        }
    }

    if (instr.single_trans.rd == 15 && instr.single_trans.l) cpu_flush(cpu);
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
    cpu_fetch(cpu);

    bool user_trans =
        instr.block_trans.s &&
        !((instr.block_trans.rlist & (1 << 15)) && instr.block_trans.l);
    CpuMode mode = cpu->cpsr.m;
    if (user_trans) {
        cpu->cpsr.m = M_USER;
        cpu_update_mode(cpu, mode);
    }

    if (instr.block_trans.l) {
        cpu_internal_cycle(cpu);
        if (instr.block_trans.w) cpu->r[instr.block_trans.rn] = wback;
    }

    bool user_untrans = false;

    for (int i = 0; i < rcount; i++, addr += 4) {
        word data;
        if (instr.block_trans.l) {
            data = cpu_readw(cpu, addr);
            cpu->r[rlist[i]] = data;
            if (rlist[i] == 15) {
                if (user_trans) {
                    cpu->cpsr.m = mode;
                    cpu_update_mode(cpu, M_USER);
                    user_untrans = true;
                }
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
            data = cpu->r[rlist[i]];
            cpu_writew(cpu, addr, data);
            if (i == 0 && instr.block_trans.w)
                cpu->r[instr.block_trans.rn] = wback;
        }
    }

    if (user_trans && !user_untrans) {
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
                cpu_fetch(cpu);
                return;
            }
        } else {
            cpu->lr = (cpu->pc - 4) & ~0b11;
        }
    }
    cpu_fetch(cpu);
    cpu->pc = dest;
    cpu_flush(cpu);
}

void exec_arm_sw_intr(Arm7TDMI* cpu, ArmInstr instr) {
    cpu_handle_interrupt(cpu, I_SWI);
}