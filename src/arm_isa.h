#ifndef ARM_ISA_H
#define ARM_ISA_H

#include "types.h"

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
        word c3 : 8; // 00001001
        word rd : 4;
        word rn : 4;
        word c2 : 2; // 00
        word b : 1;
        word c1 : 5; // 00010
        word cond : 4;
    } swap;
    struct {
        word rn : 4;
        word c1 : 24; // 000100101111111111110001
        word cond : 4;
    } branch_ex;
    struct {
        word rm : 4;
        word c3 : 1; // 1
        word h : 1;
        word s : 1;
        word c2 : 5; // 00001
        word rd : 4;
        word rn : 4;
        word l : 1;
        word w : 1;
        word i : 1; // 0
        word u : 1;
        word p : 1;
        word c1 : 3; // 000
        word cond : 4;
    } data_transh_reg;
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
        word i : 1; // 1
        word u : 1;
        word p : 1;
        word c1 : 3; // 000
        word cond : 4;
    } data_transh_imm;
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
    } data_trans;
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
    } data_trans_block;
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

void arm_exec_instr(Arm7TDMI* cpu);

void exec_arm_data_proc(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_multiply(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_multiply_long(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_swap(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_branch_ex(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_data_transh(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_data_trans(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_undefined(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_data_trans_block(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_branch(Arm7TDMI* cpu, ArmInstr instr);
void exec_arm_sw_intr(Arm7TDMI* cpu, ArmInstr instr);

#endif