/*
 * Mu's virtual machine
 */

#ifdef MU_DEF
#ifndef MU_VM_DEF
#define MU_VM_DEF
#include "mu.h"


// The number of elements that can be stored in a frame.
// Passing more than this number of elements results in passing
// a full table.
// Note: This amount of space is allocated on the stack every 
// non-tail function call.
#define MU_FRAME 4

// Type of the count for frames. Must be smallest size able 
// to hold both the input and output arguments
typedef unsigned char c_t;

// Macros for encoding the call format for functions
mu_inline c_t mu_c(c_t a, c_t r) { return ((0xf & a) << 4) | (0xf & r); }
mu_inline c_t mu_args(c_t c) { return 0xf & (c >> 4); }
mu_inline c_t mu_rets(c_t c) { return 0xf & c; }


#endif
#else
#ifndef MU_VM_H
#define MU_VM_H
#include "types.h"
#define MU_DEF
#include "vm.h"
#include "ops.h"
#undef MU_DEF


// Encode the specified opcode and return its size
// Note: size of the jump opcodes currently can not change based on argument
void mu_encode(void (*emit)(void *, data_t), void *p,
               op_t op, uint_t arg);

// Conversion between frame types
void mu_fconvert(c_t sc, mu_t *sframe, c_t dc, mu_t *dframe);

// Execute the bytecode
void mu_exec(fn_t *fn, c_t c, mu_t *frame);


#endif
#endif
