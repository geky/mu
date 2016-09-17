/*
 * Mu virtual machine
 */

#ifndef MU_VM_H
#define MU_VM_H
#include "mu.h"
#include "fn.h"
#include "ops.h"


// Encode opcode
void mu_encode(void (*emit)(void *, mbyte_t), void *p,
               enum op op, mint_t d, mint_t a, mint_t b);

// Replace jump with actual jump distance and returns previous jump value
// Note: Currently can not change size of jump instruction
mint_t mu_patch(void *c, mint_t j);

// Execute bytecode
mcnt_t mu_exec(struct code *c, mu_t scope, mu_t *frame);


// Disassemble bytecode for debugging and introspection
// currently outputs to stdout
void mu_dis(struct code *code);


#endif
