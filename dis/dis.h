/*
 * Mu disassembly library for debugging
 *
 * Copyright (c) 2016 Christopher Haster
 * Distributed under the MIT license in mu.h
 */
#ifndef MU_DIS_H
#define MU_DIS_H
#include "mu/mu.h"


// Disassemble bytecode for debugging and introspection, outputs to stdout
// Does not consume its argument to help when debugging
void mu_dis(mu_t mu);

// Disassembler module in Mu
#define MU_DIS_KEY      mu_dis_key_def()
#define MU_DIS          mu_dis_def()
#define MU_DIS_MODULE   mu_dis_module_def()
MU_DEF(mu_dis_key_def)
MU_DEF(mu_dis_def)
MU_DEF(mu_dis_module_def)


#endif
