/*
 * First class function variable type
 */

#ifdef MU_DEF
#ifndef MU_FN_DEF
#define MU_FN_DEF
#include "mu.h"


// Definition of C Function types
typedef union mu bfn_t(tbl_t *args);
typedef union mu sbfn_t(tbl_t *args, tbl_t *scope);

// Definition of Mu function type
typedef mu_aligned struct fn fn_t;


#endif
#else
#ifndef MU_FN_H
#define MU_FN_H
#include "types.h"
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
fn_t *fn_bfn(bfn_t *f);
fn_t *fn_sbfn(sbfn_t *f, tbl_t *scope);

mu_inline mu_t mbfn(bfn_t *f) { 
    return mfn(fn_bfn(f)); 
}

mu_inline mu_t msbfn(sbfn_t *f, tbl_t *s) { 
    return mfn(fn_sbfn(f, s)); 
}

// Mu Function creating functions
fn_t *fn_create(tbl_t *args, mu_t code);
fn_t *fn_create_expr(tbl_t *args, mu_t code);
fn_t *fn_create_nested(tbl_t *args, parse_t *p);
void fn_destroy(fn_t *f);

// Function reference counting
mu_inline void fn_inc(fn_t *f) { ref_inc((void *)f); }
mu_inline void fn_dec(fn_t *f) { ref_dec((void *)f, 
                                         (void (*)(void *))fn_destroy); }


// Closure handling functions
fn_t *fn_closure(fn_t *f, tbl_t *scope);

// Call a function. Each function call takes a table of arguments, 
// and returns a single variable.
mu_t fn_call(fn_t *f, tbl_t *args);
mu_t fn_call_in(fn_t *f, tbl_t *args, tbl_t *scope);

// Function representation
str_t *fn_repr(fn_t *f);


#endif
#endif
