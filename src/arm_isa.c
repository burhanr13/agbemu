#include "arm_isa.h"

#include <stdio.h>

#include "arm7tdmi.h"

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
    ArmInstr instr = {cpu_read(cpu, cpu->pc)};
    cpu->pc += 4;
    if (!eval_cond(cpu, instr)) {
        return;
    }

    if (instr.sw_intr.c1 == 0b1111) {
        exec_arm_sw_intr(cpu, instr);
    } else if (instr.branch.c1 == 0b101) {
        exec_arm_branch(cpu, instr);
    } else if (instr.data_trans_block.c1 == 0b100) {
        exec_arm_data_trans_block(cpu, instr);
    } else if (instr.undefined.c1 == 0b011 && instr.undefined.c2 == 1) {
        exec_arm_undefined(cpu, instr);
    } else if (instr.data_trans.c1 == 0b01) {
        exec_arm_data_trans(cpu, instr);
    } else if (instr.branch_ex.c1 == 0b000100101111111111110001) {
        exec_arm_branch_ex(cpu, instr);
    } else if (instr.swap.c1 == 0b00010 && instr.swap.c2 == 0b00 &&
               instr.swap.c3 == 0b00001001) {
        exec_arm_swap(cpu, instr);
    } else if (instr.data_transh_reg.c1 == 0b000 &&
               instr.data_transh_reg.i == 0 &&
               instr.data_transh_reg.c2 == 0b00001 &&
               instr.data_transh_reg.c3 == 1) {
        exec_arm_data_transh(cpu, instr);
    } else if (instr.data_transh_imm.c1 == 0b000 &&
               instr.data_transh_imm.i == 1 && instr.data_transh_imm.c2 == 1 &&
               instr.data_transh_imm.c3 == 1) {
        exec_arm_data_transh(cpu, instr);
    } else if (instr.multiply.c1 == 0b000000 && instr.multiply.c2 == 0b1001) {
        exec_arm_multiply(cpu, instr);
    } else if (instr.multiply_long.c1 == 0b00001 &&
               instr.multiply_long.c2 == 0b1001) {
        exec_arm_multiply_long(cpu, instr);
    } else if (instr.data_proc.c1 == 0b00) {
        exec_arm_data_proc(cpu, instr);
    }
}

void exec_arm_data_proc(Arm7TDMI* cpu, ArmInstr instr) {
    word op1 = cpu->r[instr.data_proc.rn];
    if (instr.data_proc.rn == 15) op1 += 4;
    word op2;
    word z, c = cpu->cpsr.c, n, v = cpu->cpsr.v;
    if (instr.data_proc.i) {
        op2 = instr.data_proc.op2 & 0xff;
        word shift_amt = instr.data_proc.op2 >> 8;
        shift_amt *= 2;
        op2 = (op2 >> shift_amt) | (op2 << (32 - shift_amt));
    } else {
        word rm = instr.data_proc.op2 & 0b1111;
        op2 = cpu->r[rm];
        if (rm == 15) op2 += 4;
        word shift = instr.data_proc.op2 >> 4;
        word shift_type = (shift >> 1) & 0b11;
        word shift_amt;
        if (shift & 1) {
            word rs = shift >> 4;
            shift_amt = cpu->r[rs] & 0xff;
            if (instr.data_proc.rn == 15) op1 += 4;
            if (rm == 15) op2 += 4;
        } else {
            shift_amt = shift >> 3;
        }
        switch (shift_type) {
            case 0: // LSL
                if (shift_amt) c = (op2 >> (32 - shift_amt)) & 1;
                else c = cpu->cpsr.c;
                op2 <<= shift_amt;
                break;
            case 1: // LSR
                if (shift_amt == 0) shift_amt = 32;
                c = (op2 >> (shift_amt - 1)) & 1;
                op2 >>= shift_amt;
                break;
            case 2: // ASR
                if (shift_amt == 0) shift_amt = 32;
                sword sop2 = op2;
                c = (sop2 >> (shift_amt - 1)) & 1;
                sop2 >>= shift_amt;
                op2 = sop2;
                break;
            case 3:                   // ROR
                if (shift_amt == 0) { // RRX
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

    word res = 0;
    bool arith = false;
    bool save = true;
    switch (instr.data_proc.opcode) {
        case 0: // AND
            res = op1 & op2;
            break;
        case 1: // EOR
            res = op1 ^ op2;
            break;
        case 2: // SUB
            arith = true;
            op2 = -op2;
            break;
        case 3: // RSB
            arith = true;
            op1 = -op1;
            break;
        case 4: // ADD
            arith = true;
            break;
        case 5: // ADC
            arith = true;
            res = cpu->cpsr.c;
            break;
        case 6: // SBC
            arith = true;
            op2 = -op2;
            res = cpu->cpsr.c - 1;
            break;
        case 7: // RSC
            arith = true;
            op1 = -op1;
            res = cpu->cpsr.c - 1;
            break;
        case 8: // TST
            res = op1 & op2;
            save = false;
            break;
        case 9: // TEQ
            res = op1 ^ op2;
            save = false;
            break;
        case 10: // CMP
            arith = true;
            op2 = -op2;
            save = false;
            break;
        case 11: // CMN
            arith = true;
            save = false;
            break;
        case 12: // ORR
            res = op1 | op2;
            break;
        case 13: // MOV
            res = op2;
            break;
        case 14: // BIC
            res = op1 & ~op2;
            break;
        case 15: // MVN
            res = ~op2;
            break;
    }

    if (arith) {
        res += op1 + op2;
        c = (op1 > res) ? 1 : 0;
        sword sop1 = op1, sop2 = op2, sres = res;
        v = ((sop1 > sres && sop2 > 0) || (sop1 < sres && sop2 < 0)) ? 1 : 0;
    }
    z = (res == 0) ? 1 : 0;
    n = (res >> 31) & 1;

    if (save) {
        cpu->r[instr.data_proc.rd] = res;
    }

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

    if (!(save || instr.data_proc.s)) {
        word p = (instr.data_proc.opcode >> 1) & 1;
        if (instr.data_proc.opcode & 1) { // MSR
            word rm = instr.data_proc.op2 & 0b1111;
            if (instr.data_proc.rn & 1) {
                if (p) {
                    cpu->spsr = cpu->r[rm];
                } else {
                    CpuMode mode = cpu->cpsr.m;
                    if (mode > M_USER) {
                        cpu->cpsr.w = cpu->r[rm];
                        cpu_update_mode(cpu, mode);
                    } else {
                        cpu->cpsr.w &= 0x0fffffff;
                        cpu->cpsr.w |= cpu->r[rm] & 0xf0000000;
                    }
                }
            } else {
                word data;
                if (instr.data_proc.i) {
                    data = instr.data_proc.op2 & 0xff;
                    word rot = instr.data_proc.op2 >> 8;
                    rot *= 2;
                    data = (data >> rot) | (data << (32 - rot));
                } else {
                    data = cpu->r[rm];
                }
                if (p) {
                    cpu->spsr &= 0x0fffffff;
                    cpu->spsr |= data & 0xf0000000;
                } else {
                    cpu->cpsr.w &= 0x0fffffff;
                    cpu->cpsr.w |= data & 0xf0000000;
                }
            }
        } else { // MRS
            word psr;
            if (p) {
                psr = cpu->spsr;
            } else {
                psr = cpu->cpsr.w;
            }
            cpu->r[instr.data_proc.rd] = psr;
        }
    }
}

void exec_arm_multiply(Arm7TDMI* cpu, ArmInstr instr) {
    cpu->r[instr.multiply.rd] =
        cpu->r[instr.multiply.rm] * cpu->r[instr.multiply.rs];
    if (instr.multiply.a)
        cpu->r[instr.multiply.rd] += cpu->r[instr.multiply.rn];
    if (instr.multiply.s) {
        cpu->cpsr.z = (cpu->r[instr.multiply.rd] == 0) ? 1 : 0;
        cpu->cpsr.n = (cpu->r[instr.multiply.rd] >> 31) & 1;
    }
}

void exec_arm_multiply_long(Arm7TDMI* cpu, ArmInstr instr) {
    dword res;
    if (instr.multiply_long.u) {
        res = (sdword) cpu->r[instr.multiply_long.rm] *
              (sdword) cpu->r[instr.multiply_long.rs];
    } else {
        res = (dword) cpu->r[instr.multiply_long.rm] *
              (dword) cpu->r[instr.multiply_long.rs];
    }
    if (instr.multiply_long.a) {
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

void exec_arm_swap(Arm7TDMI* cpu, ArmInstr instr) {
    printf("swap ");
}

void exec_arm_branch_ex(Arm7TDMI* cpu, ArmInstr instr) {
    cpu->pc = cpu->r[instr.branch_ex.rn];
    cpu->cpsr.t = cpu->r[instr.branch_ex.rn] & 1;
}

void exec_arm_data_transh(Arm7TDMI* cpu, ArmInstr instr) {
    word addr = cpu->r[instr.data_trans.rn];
    if (instr.data_trans.rn == 15) addr += 4;
    word offset;
    if (instr.data_transh_imm.i) {
        offset =
            instr.data_transh_imm.offlo | (instr.data_transh_imm.offhi << 4);
    } else {
        offset = cpu->r[instr.data_transh_reg.rm];
    }

    if (!instr.data_transh_imm.u) offset = -offset;
    if (instr.data_transh_imm.p) addr += offset;

    if (instr.data_transh_imm.s) {
        sword data;
        if (instr.data_trans.l) {
            if (instr.data_transh_imm.h) {
                data = (shword) cpu_readh(cpu, addr);
            } else {
                data = (sbyte) cpu_readb(cpu, addr);
            }
            cpu->r[instr.data_transh_imm.rd] = data;
        }
    } else if (instr.data_transh_imm.h) {
        hword data;
        if (instr.data_trans.l) {
            data = cpu_readh(cpu, addr);
            cpu->r[instr.data_transh_imm.rd] = data;
        } else {
            data = cpu->r[instr.data_transh_imm.rd];
            if (instr.data_transh_imm.rd == 15) data += 8;
            cpu_writeh(cpu, addr, data);
        }
    }

    if (!instr.data_transh_imm.p) addr += offset;
    if (instr.data_transh_imm.w || !instr.data_transh_imm.p) {
        cpu->r[instr.data_transh_imm.rn] = addr;
    }
}

void exec_arm_data_trans(Arm7TDMI* cpu, ArmInstr instr) {
    word addr = cpu->r[instr.data_trans.rn];
    if (instr.data_trans.rn == 15) addr += 4;
    word offset;
    if (instr.data_trans.i) {
        word rm = instr.data_trans.offset & 0b1111;
        offset = cpu->r[rm];
        word shift = instr.data_trans.offset >> 4;
        word shift_type = (shift >> 1) & 0b11;
        word shift_amt = shift >> 3;
        switch (shift_type) {
            case 0: // LSL
                offset <<= shift_amt;
                break;
            case 1: // LSR
                if (shift_amt == 0) shift_amt = 32;
                offset >>= shift_amt;
                break;
            case 2: // ASR
                if (shift_amt == 0) shift_amt = 32;
                sword soff = offset;
                soff >>= shift_amt;
                offset = soff;
                break;
            case 3:                   // ROR
                if (shift_amt == 0) { // RRX
                    offset >>= 1;
                    offset |= cpu->cpsr.c << 31;
                } else {
                    offset =
                        (offset >> shift_amt) | (offset << (32 - shift_amt));
                }
                break;
        }
    } else {
        offset = instr.data_trans.offset;
    }

    if (!instr.data_trans.u) offset = -offset;
    if (instr.data_trans.p) addr += offset;

    if (instr.data_trans.b) {
        byte data;
        if (instr.data_trans.l) {
            data = cpu_readb(cpu, addr);
            cpu->r[instr.data_trans.rd] = data;
        } else {
            data = cpu->r[instr.data_trans.rd];
            if (instr.data_trans.rd == 15) data += 8;
            cpu_writeb(cpu, addr, data);
        }
    } else {
        word data;
        if (instr.data_trans.l) {
            data = cpu_read(cpu, addr);
            word rot = (addr % 4) * 8;
            data = data >> rot | data << (32 - rot);
            cpu->r[instr.data_trans.rd] = data;
        } else {
            data = cpu->r[instr.data_trans.rd];
            if (instr.data_trans.rd == 15) data += 8;
            cpu_write(cpu, addr, data);
        }
    }

    if (!instr.data_trans.p) addr += offset;
    if (instr.data_trans.w || !instr.data_trans.p) {
        cpu->r[instr.data_trans.rn] = addr;
    }
}

void exec_arm_undefined(Arm7TDMI* cpu, ArmInstr instr) {
    printf("und ");
}

void exec_arm_data_trans_block(Arm7TDMI* cpu, ArmInstr instr) {
    printf("ldm ");
}

void exec_arm_branch(Arm7TDMI* cpu, ArmInstr instr) {
    word offset = instr.branch.offset;
    if (offset & (1 << 23)) offset |= 0xff000000;
    offset <<= 2;
    if (instr.branch.l) cpu->lr = cpu->pc & ~0b11;
    cpu->pc += 4 + offset;
}

void exec_arm_sw_intr(Arm7TDMI* cpu, ArmInstr instr) {
    printf("swi ");
}