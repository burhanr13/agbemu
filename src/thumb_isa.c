#include "thumb_isa.h"

#include "arm7tdmi.h"

ArmInstr thumb_decode_instr(ThumbInstr instr) {
    ArmInstr dec;
    dec.cond = C_AL;
    dec.instr = 0;

    switch (instr.n3) {
        case 0:
        case 1:
            if (instr.shift.op < 0b11) {
                dec.data_proc.c1 = 0b00;
                dec.data_proc.i = 0;
                dec.data_proc.opcode = A_MOV;
                dec.data_proc.s = 1;
                dec.data_proc.rd = instr.shift.rd;
                dec.data_proc.op2 = instr.shift.rs;
                dec.data_proc.op2 |= instr.shift.op << 5;
                dec.data_proc.op2 |= instr.shift.offset << 7;
            } else {
                dec.data_proc.c1 = 0b00;
                dec.data_proc.i = instr.add.i;
                dec.data_proc.opcode = (instr.add.op) ? A_SUB : A_ADD;
                dec.data_proc.s = 1;
                dec.data_proc.rn = instr.add.rs;
                dec.data_proc.rd = instr.add.rd;
                dec.data_proc.op2 = instr.add.op2;
            }
            break;
        case 2:
        case 3:
            dec.data_proc.c1 = 0b00;
            dec.data_proc.i = 1;
            dec.data_proc.s = 1;
            dec.data_proc.rn = instr.alu_imm.rd;
            dec.data_proc.rd = instr.alu_imm.rd;
            dec.data_proc.op2 = instr.alu_imm.offset;
            switch (instr.alu_imm.op) {
                case 0:
                    dec.data_proc.opcode = A_MOV;
                    break;
                case 1:
                    dec.data_proc.opcode = A_CMP;
                    break;
                case 2:
                    dec.data_proc.opcode = A_ADD;
                    break;
                case 3:
                    dec.data_proc.opcode = A_SUB;
                    break;
            }
            break;
        case 4:
            switch (instr.n2 >> 2) {
                case 0:
                    dec.data_proc.c1 = 0b00;
                    dec.data_proc.i = 0;
                    dec.data_proc.opcode = instr.alu.opcode;
                    dec.data_proc.s = 1;
                    dec.data_proc.rn = instr.alu.rd;
                    dec.data_proc.rd = instr.alu.rd;
                    dec.data_proc.op2 = instr.alu.rs;
                    switch (instr.alu.opcode) {
                        case T_LSL:
                            dec.data_proc.opcode = A_MOV;
                            dec.data_proc.op2 = instr.alu.rd;
                            dec.data_proc.op2 |= 1 << 4;
                            dec.data_proc.op2 |= S_LSL << 5;
                            dec.data_proc.op2 |= instr.alu.rs << 8;
                            break;
                        case T_LSR:
                            dec.data_proc.opcode = A_MOV;
                            dec.data_proc.op2 = instr.alu.rd;
                            dec.data_proc.op2 |= 1 << 4;
                            dec.data_proc.op2 |= S_LSR << 5;
                            dec.data_proc.op2 |= instr.alu.rs << 8;
                            break;
                        case T_ASR:
                            dec.data_proc.opcode = A_MOV;
                            dec.data_proc.op2 = instr.alu.rd;
                            dec.data_proc.op2 |= 1 << 4;
                            dec.data_proc.op2 |= S_ASR << 5;
                            dec.data_proc.op2 |= instr.alu.rs << 8;
                            break;
                        case T_ROR:
                            dec.data_proc.opcode = A_MOV;
                            dec.data_proc.op2 = instr.alu.rd;
                            dec.data_proc.op2 |= 1 << 4;
                            dec.data_proc.op2 |= S_ROR << 5;
                            dec.data_proc.op2 |= instr.alu.rs << 8;
                            break;
                        case T_NEG:
                            dec.data_proc.i = 1;
                            dec.data_proc.opcode = A_RSB;
                            dec.data_proc.rn = instr.alu.rs;
                            dec.data_proc.op2 = 0;
                            break;
                        case T_MUL:
                            dec.multiply.c1 = 0;
                            dec.multiply.a = 0;
                            dec.multiply.s = 1;
                            dec.multiply.rd = instr.alu.rd;
                            dec.multiply.rn = 0;
                            dec.multiply.rs = instr.alu.rs;
                            dec.multiply.c2 = 0b1001;
                            dec.multiply.rm = instr.alu.rd;
                            break;
                    }
                    break;
                case 1:
                    dec.data_proc.c1 = 0b00;
                    dec.data_proc.i = 0;
                    dec.data_proc.s = 0;
                    dec.data_proc.rn = instr.hi_ops.rd | (instr.hi_ops.h1 << 3);
                    dec.data_proc.rd = instr.hi_ops.rd | (instr.hi_ops.h1 << 3);
                    dec.data_proc.op2 =
                        instr.hi_ops.rs | (instr.hi_ops.h2 << 3);
                    switch(instr.hi_ops.op) {
                        case 0:
                            dec.data_proc.opcode = A_ADD;
                            break;
                        case 1:
                            dec.data_proc.opcode = A_CMP;
                            dec.data_proc.s = 1;
                            break;
                        case 2:
                            dec.data_proc.opcode = A_MOV;
                            break;
                        case 3:
                            dec.branch_ex.c1 = 0b000100101111111111110001;
                            dec.branch_ex.rn =
                                instr.hi_ops.rs | (instr.hi_ops.h2 << 3);
                            break;
                    }
                    break;
                case 2:
                case 3:
                    dec.single_trans.c1 = 0b01;
                    dec.single_trans.i = 0;
                    dec.single_trans.p = 1;
                    dec.single_trans.u = 1;
                    dec.single_trans.w = 0;
                    dec.single_trans.b = 0;
                    dec.single_trans.l = 1;
                    dec.single_trans.rn = 15;
                    dec.single_trans.rd = instr.ldst_pc.rd;
                    dec.single_trans.offset = instr.ldst_pc.offset << 2;
                    break;
            }
            break;
        case 5:
            if (instr.ldst_reg.c2 == 0) {
                dec.single_trans.c1 = 0b01;
                dec.single_trans.i = 1;
                dec.single_trans.p = 1;
                dec.single_trans.u = 1;
                dec.single_trans.w = 0;
                dec.single_trans.b = instr.ldst_reg.b;
                dec.single_trans.l = instr.ldst_reg.l;
                dec.single_trans.rn = instr.ldst_reg.rb;
                dec.single_trans.rd = instr.ldst_reg.rd;
                dec.single_trans.offset = instr.ldst_reg.ro;
            } else {
                dec.half_transr.c1 = 0b000;
                dec.half_transr.p = 1;
                dec.half_transr.u = 1;
                dec.half_transr.i = 0;
                dec.half_transr.w = 0;
                dec.half_transr.l = instr.ldst_s.s | instr.ldst_s.h;
                dec.half_transr.rn = instr.ldst_s.rb;
                dec.half_transr.rd = instr.ldst_s.rd;
                dec.half_transr.c2 = 0b00001;
                dec.half_transr.s = instr.ldst_s.s;
                dec.half_transr.h = ~instr.ldst_s.s | instr.ldst_s.h;
                dec.half_transr.c3 = 1;
                dec.half_transr.rm = instr.ldst_s.ro;
            }
            break;
        case 6:
        case 7:
            dec.single_trans.c1 = 0b01;
            dec.single_trans.i = 0;
            dec.single_trans.p = 1;
            dec.single_trans.u = 1;
            dec.single_trans.w = 0;
            dec.single_trans.b = instr.ldst_imm.b;
            dec.single_trans.l = instr.ldst_imm.l;
            dec.single_trans.rn = instr.ldst_imm.rb;
            dec.single_trans.rd = instr.ldst_imm.rd;
            dec.single_trans.offset = instr.ldst_imm.offset;
            break;
        case 8:
            dec.half_transi.c1 = 0b000;
            dec.half_transi.p = 1;
            dec.half_transi.u = 1;
            dec.half_transi.i = 1;
            dec.half_transi.w = 0;
            dec.half_transi.l = instr.ldst_h.l;
            dec.half_transi.rn = instr.ldst_h.rb;
            dec.half_transi.rd = instr.ldst_h.rd;
            dec.half_transi.offhi = instr.ldst_h.offset >> 4;
            dec.half_transi.c2 = 1;
            dec.half_transi.s = 0;
            dec.half_transi.h = 1;
            dec.half_transi.c3 = 1;
            dec.half_transi.offlo = instr.ldst_h.offset;
            break;
        case 9:
            dec.single_trans.c1 = 0b01;
            dec.single_trans.i = 1;
            dec.single_trans.p = 1;
            dec.single_trans.u = 1;
            dec.single_trans.w = 0;
            dec.single_trans.b = 0;
            dec.single_trans.l = instr.ldst_sp.l;
            dec.single_trans.rn = 13;
            dec.single_trans.rd = instr.ldst_sp.rd;
            dec.single_trans.offset = instr.ldst_sp.offset << 2;
            break;
        case 10:
            dec.data_proc.c1 = 0b00;
            dec.data_proc.i = 1;
            dec.data_proc.opcode = A_ADD;
            dec.data_proc.s = 0;
            dec.data_proc.rd = instr.ldst_addr.rd;
            dec.data_proc.rn = (instr.ldst_addr.sp) ? 13 : 15;
            dec.data_proc.op2 = instr.ldst_addr.offset << 2;
            break;
        case 11:
            if (instr.add_sp.c1 == 0b10110000) {
                dec.data_proc.c1 = 0b00;
                dec.data_proc.i = 1;
                dec.data_proc.opcode = (instr.add_sp.s) ? A_SUB : A_ADD;
                dec.data_proc.s = 0;
                dec.data_proc.rd = 13;
                dec.data_proc.rn = 13;
                dec.data_proc.op2 = instr.add_sp.offset;
            } else {
                dec.block_trans.c1 = 0b100;
                dec.block_trans.p = (instr.push_pop.l) ? 0 : 1;
                dec.block_trans.u = instr.push_pop.l;
                dec.block_trans.s = 0;
                dec.block_trans.w = 1;
                dec.block_trans.l = instr.push_pop.l;
                dec.block_trans.rn = 13;
                dec.block_trans.rlist = instr.push_pop.rlist;
                if(instr.push_pop.r) {
                    if(instr.push_pop.l) {
                        dec.block_trans.rlist |= (1 << 15);
                    }else {
                        dec.block_trans.rlist |= (1 << 14);
                    }
                }
            }
            break;
        case 12:
            dec.block_trans.c1 = 0b100;
            dec.block_trans.p = 0;
            dec.block_trans.u = 1;
            dec.block_trans.s = 0;
            dec.block_trans.w = 0;
            dec.block_trans.l = instr.ldst_m.l;
            dec.block_trans.rn = instr.ldst_m.rb;
            dec.block_trans.rlist = instr.ldst_m.rlist;
            break;
        case 13:
            if (instr.b_cond.cond < 0b1111) {
                dec.cond = instr.b_cond.cond;
                dec.branch.c1 = 0b101;
                dec.branch.l = 0;
                word offset = instr.b_cond.offset;
                if (offset & (1 << 7)) offset |= 0xffffff00;
                dec.branch.offset = offset;
            } else {
                dec.sw_intr.c1 = 0b1111;
            }
            break;
        case 14:
            dec.branch.c1 = 0b101;
            dec.branch.l = 0;
            word offset = instr.branch.offset;
            if (offset & (1 << 10)) offset |= 0xfffff800;
            dec.branch.offset = offset;
            break;
        case 15:
            dec.branch.c1 = 0b101;
            dec.branch.l = 1;
            if(instr.branch_l.h) {
                dec.branch.offset = instr.branch_l.offset;
            } else {
                word offset = instr.branch_l.offset;
                if (offset & (1 << 10)) offset |= 0xfffff800;
                dec.branch.offset = offset << 11;
            }
            break;
    }

    return dec;
}