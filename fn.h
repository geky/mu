/*
 * First class function variable type
 */

#ifdef MU_DEF
#ifndef MU_FN_DEF
#define MU_FN_DEF
#include "mu.h"
#include "err.h"


// Definition of C Function types
typedef union var bfn_t(tbl_t *args, eh_t *eh);
typedef union var sbfn_t(tbl_t *args, tbl_t *scope, eh_t *eh);

// Definition of Mu function type
typedef mu_aligned struct fn fn_t;


#endif
#else
#ifndef MU_FN_H
#define MU_FN_H
#include "var.h"
#define MU_DEF
#include "parse.h"
#undef MU_DEF


struct fn {
    ref_t ref; // reference count

    uintq_t stack; // amount of stack usage
    uintq_t type;  // function type

    tbl_t *closure; // function closure
    tbl_t *imms;    // immediate variables

    union {
        bfn_t *bfn;   // c function
        sbfn_t *sbfn; // scoped c function
        str_t *bcode; // compiled mu bytecode
    };
};


// C Function creating functions and macros
fn_t *fn_bfn(bfn_t *f, eh_t *eh);
fn_t *fn_sbfn(sbfn_t *f, tbl_t *scope, eh_t *eh);

mu_inline var_t vbfn(bfn_t *f, eh_t *eh) { 
    return vfn(fn_bfn(f, eh)); 
}

mu_inline var_t vsbfn(sbfn_t *f, tbl_t *s, eh_t *eh) { 
    return vfn(fn_sbfn(f, s, eh)); 
}

// Mu Function creating functions
fn_t *fn_create(tbl_t *args, var_t code, eh_t *eh);
fn_t *fn_create_expr(tbl_t *args, var_t code, eh_t *eh);
fn_t *fn_create_nested(tbl_t *args, parse_t *p, eh_t *eh);
void fn_destroy(fn_t *f);

// Function reference counting
mu_inline void fn_inc(fn_t *f) { ref_inc((void *)f); }
mu_inline void fn_dec(fn_t *f) { ref_dec((void *)f, (void*)fn_destroy); }


// Closure handling functions
fn_t *fn_closure(fn_t *f, tbl_t *scope, eh_t *eh);

// Call a function. Each function call takes a table of arguments, 
// and returns a single variable.
var_t fn_call(fn_t *f, tbl_t *args, eh_t *eh);
var_t fn_call_in(fn_t *f, tbl_t *args, tbl_t *scope, eh_t *eh);

// Function representation
str_t *fn_repr(fn_t *f, eh_t *eh);


#endif
#endif
