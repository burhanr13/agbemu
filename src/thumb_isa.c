#include "thumb_isa.h"

#include "arm7tdmi.h"

ArmInstr thumb_decode_instr(ThumbInstr instr) {
    ArmInstr dec;
    dec.cond = C_AL;
    dec.instr = 0;

    if ((instr.h & 0xff00) == 0b0100011100000000) {
        dec.branch_ex.c1 = 0b000100101111111111110001;
        dec.branch_ex.rn = (instr.h >> 3) & 0b1111;
    }

    return dec;
}