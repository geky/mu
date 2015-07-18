/*
 * Mu's virtual machine
 */

#ifndef MU_VM_H
#define MU_VM_H
#include "types.h"
#include "fn.h"
#include "ops.h"


// Encode the specified opcode
// Note: size of the jump opcodes currently can not change based on argument
void mu_encode(void (*emit)(void *, byte_t), void *p,
               op_t op, int_t d, int_t a, int_t b);

// Execute the bytecode
void mu_exec(struct fn *, frame_t c, mu_t *frame);


// Disassemble bytecode for debugging and introspection
// currently just outputs to stdout
// Unsure if this should be kept as is, returned as string
// or just dropped completely.
void mu_dis(struct code *code);


#endif
