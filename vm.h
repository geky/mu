/*
 * V's virtual machine
 */

#ifndef V_VM
#define V_VM

#include "var.h"
#include "tbl.h"
#include "fn.h"


/* V compiles to bytecode during interpretation
 * Bytecode is represented as in 8 bits with optional 
 * tailing arguments. The virtual machine is composed 
 * of a general purpose stack and 4 registers with 
 * specific uses.
 *
 * The following is the encoding for the virtual machine.
 *
 * vconst v - [0000 ---1] [ 16 bit var index ] - puts constant on stack
 * vtbl     - [0010 ---0] - puts new table on stack
 * vscope   - [0011 ---0] - puts scope on stack
 *
 * vpush    - [0100 ---0] - pushes s0 onto stack
 * vpop     - [0101 ---0] - pops from stack
 * vjump o  - [0110 ---1] [ 16 bit off ] - adds signed offset to pc
 * vjn o    - [0111 ---1] [ 16 bit off ] - jumps if s0 is null
 * 
 * vlookup  - [1000 ---0] - looks up s1[s0] onto stack
 * vset     - [1001 ---0] - sets s2[s1] with s0 recursing down tail chains
 * vassign  - [1010 ---0] - assigns s2[s1] with s0 nonrecursively
 * vadd     - [1011 ---0] - adds s0 to last index of s1
 * 
 * vcall    - [1100 ---0] - calls function s1(s0) onto stack
 * vtcall   - [1101 ---0] - returns tail call of function s1(s0)
 * vret     - [1110 ---0] - returns s0
 * vretn    - [1111 ---0] - returns null
 *
 */


// Opcode definitions
enum vop {
    VCONST  = 0x00,
    VTBL    = 0x20,
    VSCOPE  = 0x30,

    VPUSH   = 0x40,
    VPOP    = 0x50,
    VJUMP   = 0x60,
    VJN     = 0x70,

    VLOOKUP = 0x80,
    VSET    = 0x90,
    VASSIGN = 0xa0,
    VADD    = 0xb0,

    VCALL   = 0xc0,
    VTCALL  = 0xd0,
    VRET    = 0xe0,
    VRETN   = 0xf0
};

enum {
    VOP_OP  = 0xf0,
    VOP_ARG = 0x01,
};


int vcount(uint8_t *code, enum vop op, uint16_t arg);
int vencode(uint8_t *code, enum vop op, uint16_t arg);

var_t vexec(fn_t *f, var_t scope);


#endif
