/*
 * Mu's virtual machine
 */

#ifdef MU_DEF
#ifndef MU_VM_DEF
#define MU_VM_DEF

#include "mu.h"


/* Mu uses bytecode as an intermediary representation
 * during compilation. It is either executed directly
 * or then converted to opcodes for the underlying machine.
 *
 * The bytecode assumes the underlying machine is a stack
 * based architecture where the word size is a var_t. Because
 * function calling is performed by passing tables, no other 
 * assumptions are needed and a simple virtual machine can be 
 * implemented with just a stack pointer and program counter.
 *
 * Bytecode is represented in 8 bits with optional tailing arguments
 * that can go up to 16 bits. Only 5 bits are used for encoding 
 * opcodes, the other 3 are used for flags that may help code generation.
 *
 */

// Instruction components
typedef uint16_t arg_t;
typedef int16_t sarg_t;

enum {
    MU_OP    = 0xf8, // Opcode in top 5 bits
    MU_FLAGS = 0x07, // Flags in bottom 3 bits
};

enum opflags {
    MU_ARG   = 0x01, // Indicates this opcode uses an argument
};


typedef enum op {
/*  opcode    encoding   arg    stack   result      description                                     */
    OP_VAR    = 0x10, // index  +1      var[i]      places constant variable on stack
    OP_FN     = 0x11, // index  +1      fn(var[i])  places function bound with scope on the stack
    OP_NIL    = 0x12, // -      +1      nil         places nil on the stack
    OP_TBL    = 0x13, // -      +1      []          creates a new table on the stack
    OP_SCOPE  = 0x14, // -      +1      scope       places the scope on the stack
    OP_ARGS   = 0x15, // -      +1      args        places the args on the stack

    OP_DUP    = 0x16, // index  +1      s[i]        duplicates the specified element on the stack
    OP_DROP   = 0x17, // -      -1      -           pops element off the stack

    OP_JUMP   = 0x18, // offset -       -           adds signed offset to pc
    OP_JFALSE = 0x1a, // offset -1      -           jump if top of stack is nil
    OP_JTRUE  = 0x1b, // offset -1      -           jump if top of stack is not nil

    OP_LOOKUP = 0x04, // -      -1      s1[s0]      looks up s1[s0] onto stack
    OP_LOOKDN = 0x05, // index  -1      s1[s0/i]    looks up either s2[s1] or index s2[s0]

    OP_ASSIGN = 0x08, // -      -3      -           assigns s2[s1] with s0 recursively
    OP_INSERT = 0x09, // -      -2      s2          inserts s2[s1] with s0 nonrecursively
    OP_APPEND = 0x0a, // -      -1      s1          adds s0 to s1

    OP_ITER   = 0x0c, // -      -       iter(s0)    obtains iterator onto stack

    OP_CALL   = 0x03, // -      -1      s1(s0)      calls function s1(s0) onto stack
    OP_TCALL  = 0x02, // -      -2      ret s1(s0)  returns tailcall of function s1(s0)   
    OP_RET    = 0x01, // -      -1      ret s0      returns s0
    OP_RETN   = 0x00, // -      -       ret nil     returns nil
} op_t;


#endif
#else
#ifndef MU_VM_H
#define MU_VM_H
#define MU_DEF
#include "vm.h"
#include "var.h"
#include "tbl.h"
#include "fn.h"
#undef MU_DEF


// Function definitions must be created by 
// machine implementations

// Return the size taken by the specified opcode
// Note: size of the jump opcode currently can not change
// based on argument, because this is not handled by the parser
int mu_size(op_t op, arg_t arg);

// Encode the specified opcode and return its size
void mu_encode(mstr_t *code, op_t op, arg_t arg);

// Execute the bytecode
var_t mu_exec(fn_t *f, tbl_t *args, tbl_t *scope, eh_t *eh);


#endif
#endif
