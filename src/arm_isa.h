#ifndef ARM_ISA_H
#define ARM_ISA_H

#include <stdio.h>

#include "types.h"

enum { C_EQ, C_NE, C_CS, C_CC, C_MI, C_PL, C_VS, C_VC, C_HI, C_LS, C_GE, C_LT, C_GT, C_LE, C_AL };

enum {
    A_AND,
    A_EOR,
    A_SUB,
    A_RSB,
    A_ADD,
    A_ADC,
    A_SBC,
    A_RSC,
    A_TST,
    A_TEQ,
    A_CMP,
    A_CMN,
    A_ORR,
    A_MOV,
    A_BIC,
    A_MVN
};

enum { S_LSL, S_LSR, S_ASR, S_ROR };

typedef struct _Arm7TDMI Arm7TDMI;

typedef union {
    word w;
    struct {
        word instr : 28;
        word cond : 4;
    };
    struct {
        word op2 : 12;
        word rd : 4;
        word rn : 4;
        word s : 1;
        word opcode : 4;
        word i : 1;
        word c1 : 2; // 00
        word cond : 4;
    } data_proc;
    struct {
        word op2 : 12;
        word rd : 4;
        word c : 1;
        word x : 1;
        word s : 1;
        word f : 1;
        word c3 : 1; // 0
        word op : 1;
        word p : 1;
        word c2 : 2; // 10
        word i : 1;
        word c1 : 2; // 00
        word cond : 4;
    } psr_trans;
    struct {
        word rm : 4;
        word c2 : 4; // 1001
        word rs : 4;
        word rn : 4;
        word rd : 4;
        word s : 1;
        word a : 1;
        word c1 : 6; // 000000
        word cond : 4;
    } multiply;
    struct {
        word rm : 4;
        word c2 : 4; // 1001
        word rs : 4;
        word rdlo : 4;
        word rdhi : 4;
        word s : 1;
        word a : 1;
        word u : 1;
        word c1 : 5; // 00001
        word cond : 4;
    } multiply_long;
    struct {
        word rm : 4;
        word c4 : 4; // 1001
        word c3 : 4;
        word rd : 4;
        word rn : 4;
        word c2 : 2; // 00
        word b : 1;
        word c1 : 5; // 00010
        word cond : 4;
    } swap;
    struct {
        word rn : 4;
        word c3 : 4; // 0001
        word c2 : 12;
        word c1 : 8; // 00010010
        word cond : 4;
    } branch_ex;
    struct {
        word offlo : 4;
        word c3 : 1; // 1
        word h : 1;
        word s : 1;
        word c2 : 1; // 1
        word offhi : 4;
        word rd : 4;
        word rn : 4;
        word l : 1;
        word w : 1;
        word i : 1;
        word u : 1;
        word p : 1;
        word c1 : 3; // 000
        word cond : 4;
    } half_trans;
    struct {
        word offset : 12;
        word rd : 4;
        word rn : 4;
        word l : 1;
        word w : 1;
        word b : 1;
        word u : 1;
        word p : 1;
        word i : 1;
        word c1 : 2; // 01
        word cond : 4;
    } single_trans;
    struct {
        word u2 : 4;
        word c2 : 1; // 1
        word u1 : 20;
        word c1 : 3; // 011
        word cond : 4;
    } undefined;
    struct {
        word rlist : 16;
        word rn : 4;
        word l : 1;
        word w : 1;
        word s : 1;
        word u : 1;
        word p : 1;
        word c1 : 3; // 100
        word cond : 4;
    } block_trans;
    struct {
        word offset : 24;
        word l : 1;
        word c1 : 3; // 101
        word cond : 4;
    } branch;
    struct {
        word arg : 24;
        word c1 : 4; // 1111
        word cond : 4;
    } sw_intr;
} ArmInstr;

typedef void (*ArmExecFunc)(Arm7TDMI*, ArmInstr);

extern ArmExecFunc arm_lookup[1 << 12];

void arm_generate_lookup();
ArmExecFunc arm_decode_instr(ArmInstr instr);

void arm_exec_instr(Arm7TDMI* cpu);

void exec_arm_data_proc(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_psr_trans(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_multiply(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_multiply_long(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_swap(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_branch_ex(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_half_trans(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_single_trans(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_undefined(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_block_trans(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_branch(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_sw_intr(Arm7TDMI* cpu, ArmInstr instr);

void arm_disassemble(ArmInstr instr, word addr, FILE* out);

#endif