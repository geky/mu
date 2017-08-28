/*
 * Mu repl library for presenting a user-interactive interface
 *
 * Copyright (c) 2016 Christopher Haster
 * Distributed under the MIT license in mu.h
 */
#ifndef MU_REPL_H
#define MU_REPL_H
#include "mu/mu.h"


// Length of history to remember in interactive session, history is currently
// shared globally
#ifndef MU_REPL_HISTORY
#define MU_REPL_HISTORY 100
#endif


// By default, Mu will make system calls to read/write to stdin/stdout.
// If set to zero, the term sys functions must be defined.
#ifndef MU_USE_STD_TERM
#define MU_USE_STD_TERM 1
#endif

// Sys functions that must be defined if MU_USE_STD_TERM is 0
int mu_sys_termenter(void);
void mu_sys_termexit(void);

mint_t mu_sys_termread(void *buf, muint_t n);
mint_t mu_sys_termwrite(const void *buf, muint_t n);


// Read data interactively from user, returned as a raw string in a mu buf.
// The scope is used for mu-style tab completion or may be nil to disable.
mu_t mu_repl_read(const char *prompt, mu_t scope);

// Eval user input as a mu statement
void mu_repl_feval(const char *prompt, mu_t scope, mcnt_t fc, mu_t *frame);
mu_t mu_repl_veval(const char *prompt, mu_t scope, mcnt_t fc, va_list args);
mu_t mu_repl_eval(const char *prompt, mu_t scope, mcnt_t fc, ...);

// Eval user input and print the results to the user. This is best for a
// fully interactive session, and the scope can still be used for side effects
void mu_repl(const char *prompt, mu_t scope);


#endif
