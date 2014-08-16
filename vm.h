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

// Instruction components
typedef uint8_t vop_t;
typedef uint16_t varg_t;
typedef int16_t vsarg_t;

enum {
    VOP_OP    = 0xf8, // Opcode in top 5 bits
    VOP_FLAGS = 0x07, // Flags in bottom 3 bits

    VOP_ARG   = 0x01, // Indicates this opcode uses an argument
};


enum vop {
/*  opcode    encoding      arg         stack   result      description                                     */

    VVAR    = 0x10 << 3, // var index   +1      var[i]      places constant variable on stack
    VFN     = 0x11 << 3, // var index   +1      fn(var[i])  places function bound with scope on the stack
    VNIL    = 0x12 << 3, // -           +1      nil         places nil on the stack
    VTBL    = 0x13 << 3, // -           +1      []          creates a new table on the stack
    VSCOPE  = 0x14 << 3, // -           +1      scope       places the scope on the stack
    VARGS   = 0x15 << 3, // -           +1      args        places the args on the stack

    VDUP    = 0x16 << 3, // stack index +1      s[i]        duplicates the specified element on the stack
    VDROP   = 0x17 << 3, // -           -1      -           pops element off the stack

    VJUMP   = 0x18 << 3, // offset      -       -           adds signed offset to pc
    VJFALSE = 0x1a << 3, // offset      -1      -           jump if top of stack is nil
    VJTRUE  = 0x1b << 3, // offset      -1      -           jump if top of stack is not nil

    VLOOKUP = 0x04 << 3, // -           -1      s1[s0]      looks up s1[s0] onto stack
    VLOOKDN = 0x05 << 3, // index       -1      s1[s0|i]    looks up either s2[s1] or index s2[s0]

    VASSIGN = 0x08 << 3, // -           -3      -           assigns s2[s1] with s0 recursively
    VINSERT = 0x09 << 3, // -           -2      s2          inserts s2[s1] with s0 nonrecursively
    VADD    = 0x0a << 3, // -           -1      s1          adds s0 to s1

    VITER   = 0x0c << 3, // -           -       iter(s0)    obtains iterator onto stack

    VCALL   = 0x03 << 3, // -           -1      s1(s0)      calls function s1(s0) onto stack
    VTCALL  = 0x02 << 3, // -           -2      ret s1(s0)  returns tailcall of function s1(s0)   
    VRET    = 0x01 << 3, // -           -1      ret s0      returns s0
    VRETN   = 0x00 << 3, // -           -       ret nil     returns nil
};



// Function definitions must be created by 
// machine implementations

// Return the size taken by the specified opcode
// Note: size of the jump opcode currently can not change
// based on argument, because this is not handled by the parser
int vcount(vop_t op, varg_t arg);

// Encode the specified opcode and return its size
void vencode(str_t *code, vop_t op, varg_t arg);

// Execute the bytecode
var_t vexec(fn_t *f, tbl_t *args, tbl_t *scope);


#endif
