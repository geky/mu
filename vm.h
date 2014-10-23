/*
 * Mu's virtual machine
 */

#ifndef MU_VM
#define MU_VM

#include "var.h"
#include "tbl.h"
#include "fn.h"


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
typedef uint8_t mop_t;
typedef uint16_t marg_t;
typedef int16_t msarg_t;

enum {
    MOP_OP    = 0xf8, // Opcode in top 5 bits
    MOP_FLAGS = 0x07, // Flags in bottom 3 bits

    MOP_ARG   = 0x01, // Indicates this opcode uses an argument
};


enum mop {
/*  opcode    encoding      arg         stack   result      description                                     */

    MVAR    = 0x10 << 3, // var index   +1      var[i]      places constant variable on stack
    MFN     = 0x11 << 3, // var index   +1      fn(var[i])  places function bound with scope on the stack
    MNIL    = 0x12 << 3, // -           +1      nil         places nil on the stack
    MTBL    = 0x13 << 3, // -           +1      []          creates a new table on the stack
    MSCOPE  = 0x14 << 3, // -           +1      scope       places the scope on the stack
    MARGS   = 0x15 << 3, // -           +1      args        places the args on the stack

    MDUP    = 0x16 << 3, // stack index +1      s[i]        duplicates the specified element on the stack
    MDROP   = 0x17 << 3, // -           -1      -           pops element off the stack

    MJUMP   = 0x18 << 3, // offset      -       -           adds signed offset to pc
    MJFALSE = 0x1a << 3, // offset      -1      -           jump if top of stack is nil
    MJTRUE  = 0x1b << 3, // offset      -1      -           jump if top of stack is not nil

    MLOOKUP = 0x04 << 3, // -           -1      s1[s0]      looks up s1[s0] onto stack
    MLOOKDN = 0x05 << 3, // index       -1      s1[s0|i]    looks up either s2[s1] or index s2[s0]

    MASSIGN = 0x08 << 3, // -           -3      -           assigns s2[s1] with s0 recursively
    MINSERT = 0x09 << 3, // -           -2      s2          inserts s2[s1] with s0 nonrecursively
    MAPPEND = 0x0a << 3, // -           -1      s1          adds s0 to s1

    MITER   = 0x0c << 3, // -           -       iter(s0)    obtains iterator onto stack

    MCALL   = 0x03 << 3, // -           -1      s1(s0)      calls function s1(s0) onto stack
    MTCALL  = 0x02 << 3, // -           -2      ret s1(s0)  returns tailcall of function s1(s0)   
    MRET    = 0x01 << 3, // -           -1      ret s0      returns s0
    MRETN   = 0x00 << 3, // -           -       ret nil     returns nil
};



// Function definitions must be created by 
// machine implementations

// Return the size taken by the specified opcode
// Note: size of the jump opcode currently can not change
// based on argument, because this is not handled by the parser
int mu_count(mop_t op, marg_t arg);

// Encode the specified opcode and return its size
void mu_encode(str_t *code, mop_t op, marg_t arg);

// Execute the bytecode
var_t mu_exec(fn_t *f, tbl_t *args, tbl_t *scope, eh_t *eh);


#endif
