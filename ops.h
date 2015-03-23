/*
 * The definition of Mu's opcodes
 */

#ifndef MU_OPS_H
#define MU_OPS_H

#include "mu.h"


/* Mu uses bytecode as an intermediary representation
 * during compilation. It is either executed directly
 * or then converted to opcodes for the underlying machine.
 *
 * The bytecode assumes the underlying machine is a stack
 * based architecture where the word size is a mu_t. Because
 * function calling is performed by passing tables, no other 
 * assumptions are needed and a simple virtual machine can be 
 * implemented with just a stack pointer and program counter.
 *
 * Bytecode is represented in 8 bits with optional tailing arguments
 * that can go up to 16 bits. Only 5 bits are used for encoding 
 * opcodes, the other 3 are used for flags that may help code generation.
 *
 */
typedef enum op {
/*  opcode     encoding   before      after         description                                 */
    OP_IMM     = 0x18, // -           imm[i]        pushes immediate i
    OP_SYM     = 0x1a, // -           s imm[i]      pushes scope and immediate i
    OP_FN      = 0x1c, // -           fn(imm[i],s)  pushes function i bound with scope
    OP_TBL     = 0x1e, // -           tbl(i)        pushes new table of size i

    OP_DUP     = 0x08, // v0..vi      v0..vi v0..vi duplicates i elements
    OP_PAD     = 0x0a, // -           nil0..nili    pushes i nils
    OP_DROP    = 0x0b, // v0..vi      -             drops i elements

    OP_BIND    = 0x01, // t k         bind(t[k],t)  looks up and binds t[k]
    OP_LOOKUP  = 0x02, // t k         t[k]          looks up t[k]
    OP_INSERT  = 0x04, // v0..vi t k  v1..vi        inserts t[k] = v0
    OP_ASSIGN  = 0x06, // v0..vi t k  v1..vi        assigns t[k] = v0
    OP_FLOOKUP = 0x03, // t k         t t[k]        looks up t[k]
    OP_FINSERT = 0x05, // t k v       t             inserts t[k] = v
    OP_FASSIGN = 0x07, // t k v       t             assigns t[k] = v

    OP_JUMP    = 0x12, // -           -            jumps to pc + i
    OP_JTRUE   = 0x14, // v           v            jumps to pc + i if v is not nil
    OP_JFALSE  = 0x16, // v           v            jumps to pc + i if v is nil

    OP_CALL    = 0x10, // v0..vi f    v0..vj       calls f(v0..vi) -> v0..vj
    OP_TCALL   = 0x0e, // v0..vi f    -            returns f(v0..vi)
    OP_RET     = 0x0c, // v0..vi      -            returns v0..vi
} op_t;


mu_inline bool op_noarg(enum op op)     { return op <= OP_ASSIGN; }
mu_inline bool op_stackarg(enum op op)  { return op >= OP_PAD && op <= OP_CALL; }
mu_inline bool op_immarg(enum op op)    { return op >= OP_IMM; }


extern const char *const op_names[0x20];

mu_inline const char *op_name(enum op op) { return op_names[0x1f & op]; }


#endif
