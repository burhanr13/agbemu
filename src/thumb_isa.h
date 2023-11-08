#ifndef THUMB_ISA_H
#define THUMB_ISA_H

#include "arm_isa.h"
#include "types.h"

enum {
    T_AND,
    T_EOR,
    T_LSL,
    T_LSR,
    T_ASR,
    T_ADC,
    T_SBC,
    T_ROR,
    T_TST,
    T_NEG,
    T_CMP,
    T_CMN,
    T_ORR,
    T_MUL,
    T_BIC,
    T_MVN
};

typedef union {
    hword h;
    struct {
        hword n0 : 4;
        hword n1 : 4;
        hword n2 : 4;
        hword n3 : 4;
    };
    struct {
        hword rd : 3;
        hword rs : 3;
        hword offset : 5;
        hword op : 2;
        hword c1 : 3; // 000
    } shift;
    struct {
        hword rd : 3;
        hword rs : 3;
        hword op2 : 3;
        hword op : 1;
        hword i : 1;
        hword c1 : 5; // 00011
    } add;
    struct {
        hword offset : 8;
        hword rd : 3;
        hword op : 2;
        hword c1 : 3; // 001
    } alu_imm;
    struct {
        hword rd : 3;
        hword rs : 3;
        hword opcode : 4;
        hword c1 : 6; // 010000
    } alu;
    struct {
        hword rd : 3;
        hword rs : 3;
        hword h2 : 1;
        hword h1 : 1;
        hword op : 2;
        hword c1 : 6; // 010001
    } hi_ops;
    struct {
        hword offset : 8;
        hword rd : 3;
        hword c1 : 5; // 01001
    } ld_pc;
    struct {
        hword rd : 3;
        hword rb : 3;
        hword ro : 3;
        hword c2 : 1; // 0
        hword b : 1;
        hword l : 1;
        hword c1 : 4; // 0101
    } ldst_reg;
    struct {
        hword rd : 3;
        hword rb : 3;
        hword ro : 3;
        hword c2 : 1; // 1
        hword s : 1;
        hword h : 1;
        hword c1 : 4; // 0101
    } ldst_s;
    struct {
        hword rd : 3;
        hword rb : 3;
        hword offset : 5;
        hword l : 1;
        hword b : 1;
        hword c1 : 3; // 011
    } ldst_imm;
    struct {
        hword rd : 3;
        hword rb : 3;
        hword offset : 5;
        hword l : 1;
        hword c1 : 4; // 1000
    } ldst_h;
    struct {
        hword offset : 8;
        hword rd : 3;
        hword l : 1;
        hword c1 : 4; // 1001
    } ldst_sp;
    struct {
        hword offset : 8;
        hword rd : 3;
        hword sp : 1;
        hword c1 : 4; // 1010
    } ld_addr;
    struct {
        hword offset : 7;
        hword s : 1;
        hword c1 : 8; // 10110000
    } add_sp;
    struct {
        hword rlist : 8;
        hword r : 1;
        hword c2 : 2; // 10
        hword l : 1;
        hword c1 : 4; // 1011
    } push_pop;
    struct {
        hword rlist : 8;
        hword rb : 3;
        hword l : 1;
        hword c1 : 4; // 1100
    } ldst_m;
    struct {
        hword offset : 8;
        hword cond : 4;
        hword c1 : 4; // 1101
    } b_cond;
    struct {
        hword arg : 8;
        hword c1 : 8; // 11011111
    } swi;
    struct {
        hword offset : 11;
        hword c1 : 5; // 11100
    } branch;
    struct {
        hword offset : 11;
        hword h : 1;
        hword c1 : 4; // 1111
    } branch_l;
} ThumbInstr;

extern ArmInstr thumb_lookup[1 << 16];

void thumb_generate_lookup();

ArmInstr thumb_decode_instr(ThumbInstr instr);

void thumb_disassemble(ThumbInstr instr, word addr, FILE* out);

#endif