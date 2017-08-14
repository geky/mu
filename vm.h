/*
 * Mu virtual machine
 */

#ifndef MU_VM_H
#define MU_VM_H
#include "config.h"
#include "types.h"
#include "fn.h"


// Mu uses bytecode as an intermediary representation
// during compilation. The virtual machine then converts
// these opcodes into implementation specific instructions.
// 
// The bytecode represents the execution of a register based 
// machine that operates on Mu values. Operations can have from 
// 1 to 3 operands named d, a, and b and their ranges are limited 
// by the underlying virtual machine's implementation.
// 
// In order to ensure correct reference counting without significantly
// more bytecode, some operations consume their arguments. Reference
// counting is indicated by a trailing +/-. Because of reference 
// counting, registers may or may not contain valid Mu values, and values
// may remain in use after decrementing.
// 
// The special register r0 contains the scope of the current function.
typedef enum mop {
/*  opcode      encoding  operation                  description                        */
    MU_OP_IMM     = 0x6, /* rd = imms[a]               loads immediate                    */
    MU_OP_FN      = 0x7, /* rd = fn(fns[a], r0)        creates new function in the scope  */
    MU_OP_TBL     = 0x8, /* rd = tbl(a)                creates new empty table            */

    MU_OP_MOVE    = 0x3, /* rd = ra                    moves register                     */
    MU_OP_DUP     = 0x4, /* rd = ra+                   copies and increments register     */
    MU_OP_DROP    = 0x5, /* rd-                        decrements register                */

    MU_OP_LOOKUP  = 0xa, /* rd = ra[rb-]               table lookup                       */
    MU_OP_LOOKDN  = 0x9, /* rd = ra-[rb-]              table lookup which drops table     */
    MU_OP_INSERT  = 0xb, /* ra[rb-] = rd-              nonrecursive table insert          */
    MU_OP_ASSIGN  = 0xc, /* ra[rb-] = rd-              recursive table assign             */

    MU_OP_JUMP    = 0xf, /* pc = pc + a                jumps to pc offset                 */
    MU_OP_JTRUE   = 0xe, /* if (rd)  pc = pc + a       conditionally jumps if not nil     */
    MU_OP_JFALSE  = 0xd, /* if (!rd) pc = pc + a       conditionally jumps if nil         */

    MU_OP_CALL    = 0x2, /* rd..d+b-1 = rd-(rd+1..d+a) performs function call             */
    MU_OP_TCALL   = 0x1, /* return rd-(rd+1..d+a)      performs tail recursive call       */
    MU_OP_RET     = 0x0, /* return rd..d+b-1           returns values                     */
} mop_t;


// Encode opcode
void mu_encode(void (*emit)(void *, mbyte_t), void *p,
               mop_t op, mint_t d, mint_t a, mint_t b);

// Replace jump with actual jump distance and returns previous jump value
// Note: Currently can not change size of jump instruction
mint_t mu_patch(void *c, mint_t j);

// Execute bytecode
mcnt_t mu_exec(mu_t code, mu_t scope, mu_t *frame);


// Disassemble bytecode for debugging and introspection
// currently outputs to stdout
void mu_dis(mu_t code);


#endif
