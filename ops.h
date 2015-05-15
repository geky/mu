/*
 * The definition of Mu's opcodes
 */

#ifndef MU_OPS_H
#define MU_OPS_H

#include "mu.h"


/* Mu uses bytecode as an intermediary representation
 * during compilation. The virtual machine then converts
 * these opcodes into implementation specific instructions.
 *
 * The bytecode represents the execution of a register based 
 * machine that operates on Mu values. Operations can have from 
 * 1 to 3 operands named d, a, and b and their ranges are limited 
 * by the underlying virtual machine's implementation.
 *
 * In order to ensure correct reference counting with significantly
 * more bytecode, some operations consume their arguments. Reference
 * counting is indicated by a trailing +/-. Because of reference 
 * counting, registers may or may not contain valid Mu values, and values
 * may remain in use after decrementing.
 *
 * The special register r0 contains the scope of the current function at 
 * all times and is also used for creating unconditional jumps.
 */
typedef enum op {
/*  opcode     encoding  operation                  description                         */
    OP_IMM     = 0x3, /* rd = imms[a]               loads immediate                     */
    OP_FN      = 0x4, /* rd = fn(fns[a], r0)        creates new function in the scope   */
    OP_TBL     = 0x5, /* rd = tbl(a)                creates new empty table             */

    OP_MOVE    = 0x6, /* rd = ra                    moves register                      */
    OP_DUP     = 0x7, /* rd = ra+                   copies and increments register      */
    OP_DROP    = 0x8, /* rd-                        decrements register                 */

    OP_LOOKUP  = 0xa, /* rd = ra[rb-]               table lookup                        */
    OP_LOOKDN  = 0x9, /* rd = ra-[rb-]              table lookup which drops table      */
    OP_INSERT  = 0xb, /* ra[rb-] = rd-              nonrecursive table insert           */
    OP_ASSIGN  = 0xc, /* ra[rb-] = rd-              recursive table assign              */

    OP_JUMP    = 0xf, /* pc = pc + a                jumps to pc offset                  */
    OP_JTRUE   = 0xe, /* if (rd)  pc = pc + a       conditionally jumps if not nil      */
    OP_JFALSE  = 0xd, /* if (!rd) pc = pc + a       conditionally jumps if nil          */

    OP_CALL    = 0x2, /* rd..d+b-1 = rd-(rd+1..d+a) performs function call              */
    OP_TCALL   = 0x1, /* return rd-(rd+1..d+a)      performs tail recursive call        */
    OP_RET     = 0x0, /* return rd..d+b-1           returns values                      */
} op_t;


#endif
