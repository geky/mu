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
 * v - [00] - used as accumulator for values
 * k - [01] - used as key for table operations
 * t - [10] - used as table for table operations
 * f - [11] - used to hold function value
 *
 * The following is the encoding for the virtual machine.
 *
 * vlit r v   - [0000 10 rr] [ 64 bit var ]  - stores following var in `r`
 * vtbl r     - [0001 00 rr] - create a new table in `r`
 * vpush r    - [0010 00 rr] - pushes `r` on the stack
 * vpop r     - [0011 00 rr] - pops `r` off the stack
 * 
 * vjeq o     - [0100 01 --] [ 16 bit offset] - jumps if `v` is null
 * vjne o     - [0101 01 --] [ 16 bit offset] - jumps if `v` is not null
 * vjump o    - [0110 01 --] [ 16 bit offset] - adds signed offset to pc
 *
 * vlookup r  - [1000 00 rr] - looks up `t[k]` and stores into `r`
 * vassign r  - [1001 00 rr] - assigns `t[k]` with `v` nonrecursively
 * vset r     - [1010 00 rr] - sets `t[k]` with `v` recursing down tail chains
 * vadd r     - [1011 00 rr] - adds `v` to last index of `t`
 * 
 * vcall r    - [1100 00 rr] - calls function `f` args `t` result in `r`
 * vtcall     - [1101 00 --] - returns tail call of function `f` args `t`
 * vret       - [1110 00 --] - returns `v`
 * vretn      - [1111 00 --] - returns null
 *
 */


// Opcode definitions
enum vop {
    VLIT    = 0x00,
    VTBL    = 0x10,
    VPUSH   = 0x20,
    VPOP    = 0x30,

    VJEQ    = 0x40,
    VJNE    = 0x50,
    VJUMP   = 0x60,

    VLOOKUP = 0x80,
    VASSIGN = 0x90,
    VSET    = 0xa0,
    VADD    = 0xb0,

    VCALL   = 0xc0,
    VTCALL  = 0xd0,
    VRET    = 0xe0,
    VRETN   = 0xf0
};

enum voparg {
    VARG_OFF = 0x4,
    VARG_VAR = 0x8
};

enum vreg {
    VREG_V = 0x0,
    VREG_K = 0x1,
    VREG_T = 0x2,
    VREG_F = 0x3
};


int vcount(uint8_t *code, enum vop op, void *arg);
int vencode(uint8_t *code, enum vop op, void *arg);

var_t vexec(str_t *bcode, uint16_t stack, tbl_t *scope);


#endif
