/*
 * V's virtual machine
 */

#ifndef V_VM
#define V_VM

#include "var.h"
#include "tbl.h"
#include "fn.h"


/* V uses bytecode as an intermediary representation
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

enum {
    VOP_OP    = 0xf8, // Opcode in top 5 bits
    VOP_FLAGS = 0x07, // Flags in bottom 3 bits

    VOP_ARG   = 0x01, // Indicates this opcode uses an argument
    VOP_END   = 0x04, // Indicates the end of code generation, 
};                    // will only occur in a return opcode



/*
 * The following are the opcode definitions
 * They are kept simply and limited to facilitate 
 * code generation.
 */

enum vop {
/*  opcode    encoding      arg         stack   result      description                                     */

    VVAR    = 0x10 << 3, // var index   +1      var[i]      places constant variable on stack
    VTBL    = 0x12 << 3, // -           +1      []          creates a new table on the stack
    VSCOPE  = 0x13 << 3, // -           +1      scope       places the scope on the stack

    VDUP    = 0x14 << 3, // stack index +1      s[i]        duplicates the specified element on the stack
    VDROP   = 0x15 << 3, // -           -1      -           pops element off the stack

    VJUMP   = 0x18 << 3, // offset      -       -           adds signed offset to pc
    VJFALSE = 0x1a << 3, // offset      -1      -           jump if top of stack is null
    VJTRUE  = 0x1b << 3, // offset      -1      -           jump if top of stack is not null

    VLOOKUP = 0x04 << 3, // -           -1      s1[s0]      looks up s1[s0] onto stack
    VSET    = 0x05 << 3, // -           -3      -           sets s2[s1] with s0 recursively
    VASSIGN = 0x06 << 3, // -           -2      s2          assigns s2[s1] with s0 nonrecursively
    VADD    = 0x07 << 3, // -           -1      s1          adds s0 to s1

    VCALL   = 0x03 << 3, // -           -1      s1(s0)      calls function s1(s0) onto stack
    VTCALL  = 0x02 << 3, // -           -2      ret s1(s0)  returns tailcall of function s1(s0)   
    VRET    = 0x01 << 3, // -           -1      ret s0      returns s0
    VRETN   = 0x00 << 3, // -           -       ret null    returns null
};



// Function definitions must be created by 
// machine implementations
int vcount(uint8_t *code, enum vop op, uint16_t arg);
int vencode(uint8_t *code, enum vop op, uint16_t arg);

var_t vexec(fn_t *f, var_t scope);


#endif
