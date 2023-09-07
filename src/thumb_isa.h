#ifndef THUMB_ISA_H
#define THUMB_ISA_H

#include "arm_isa.h"
#include "types.h"

typedef union {
    hword h;
} ThumbInstr;

ArmInstr thumb_decode_instr(ThumbInstr instr);

#endif