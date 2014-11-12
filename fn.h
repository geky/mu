/*
 * First class function variable type
 */

#ifdef MU_DEF
#ifndef MU_FN_DEF
#define MU_FN_DEF

#include "mu.h"
#include "var.h"
#include "tbl.h"
#include "err.h"


// Function prefix for placing extra attributes on C builtin
// Mu functions. For instance all Mu functions must be 8 byte
// aligned for types to correctly be encoded
#define mu_fn mu_aligned


// Different of C Function types
typedef mu_fn var_t bfn_t(tbl_t *args, eh_t *eh);
typedef mu_fn var_t sfn_t(tbl_t *args, tbl_t *scope, eh_t *eh);

// Definition of Mu function type
typedef struct fn fn_t;


#endif
#else
#ifndef MU_FN_H
#define MU_FN_H
#define MU_DEF
#include "fn.h"
#include "parse.h"
#undef MU_DEF

#include "mem.h"


typedef struct fn {
    const str_t *bcode; // function bytecode

    len_t stack;    // amount of stack usage
    len_t bcount;   // length of bytecode
    len_t fcount;   // number of stored functions
    len_t vcount;   // number of stored vars

    struct fn **fns;    // nested functions
    var_t *vars;        // stored vars
} fn_t;


// Functions for managing functions
// Each function is preceded with a reference count
// which is used as its handle in a var
fn_t *fn_create(tbl_t *args, var_t code, eh_t *eh);
fn_t *fn_create_nested(tbl_t *args, parse_t *p, eh_t *eh);

// Called by garbage collector to clean up
void fn_destroy(void *);

// Call a function. Each function takes a table
// of arguments, and returns a single variable.
var_t fn_call(fn_t *, tbl_t *args, tbl_t *scope, eh_t *eh);

// Returns a string representation of a function
var_t fn_repr(var_t v, eh_t *eh);


// Function reference counting
mu_inline void fn_inc(void *m) { ref_inc(m); }
mu_inline void fn_dec(void *m) { ref_dec(m, fn_destroy); }


#endif
#endif
