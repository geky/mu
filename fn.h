/*
 * First class function variable type
 */

#ifdef MU_DEF
#ifndef MU_FN_DEF
#define MU_FN_DEF
#include "mu.h"

// Definition of Mu function type
typedef mu_aligned struct fn fn_t;
    
#endif
#else
#ifndef MU_FN_H
#define MU_FN_H
#include "types.h"
#define MU_DEF
#include "vm.h"
#undef MU_DEF


// Definition of C Function types
typedef c_t bfn_t(mu_t *frame);
typedef c_t sbfn_t(tbl_t *closure, mu_t *frame);

// Flags used to define operation of functions
struct fn_flags {
    uintq_t stack;  // amount of stack usage
    uintq_t scope;  // size of scope to allocate
    c_t args;       // argument count
    uintq_t type;   // function type
};

// Definition of code structure used to represent
// executable, but not instantiated functions
typedef struct code {
    ref_t ref;  // reference count

    len_t bcount; // number of bytes in bytecode
    len_t icount; // number of immediate values
    len_t fcount; // number of code structures

    struct fn_flags flags;

    // immediates, code structures, and bytecode follow
    // the code header in order
    void *data[];
} code_t;

// Functions are stored as function pointers paired with
// closure tables. Additionally several flags are defined
// to specify how the function should be called and how
// much storage to allocate
struct fn {
    ref_t ref; // reference count

    struct fn_flags flags;

    tbl_t *closure; // function closure

    union {
        bfn_t *bfn;        // c function
        sbfn_t *sbfn;      // scoped c function
        struct code *code; // compiled mu code
    };
};


// C Function creating functions and macros
fn_t *fn_bfn(c_t args, bfn_t *bfn);
fn_t *fn_sbfn(c_t args, sbfn_t *sbfn, tbl_t *closure);

mu_inline mu_t mbfn(c_t args, bfn_t *bfn) {
    return mfn(fn_bfn(args, bfn));
}

mu_inline mu_t msbfn(c_t args, sbfn_t *sbfn, tbl_t *closure) { 
    return mfn(fn_sbfn(args, sbfn, closure)); 
}


// Mu Function creating functions
fn_t *fn_create(code_t *code, tbl_t *closure);
void fn_destroy(fn_t *f);

fn_t *fn_parse_expr(str_t *code, tbl_t *closure);
fn_t *fn_parse_fn(str_t *code, tbl_t *closure);
fn_t *fn_parse_module(str_t *code, tbl_t *closure);

// Function reference counting
mu_inline fn_t *fn_inc(fn_t *fn) { return ref_inc(fn); }
mu_inline void fn_dec(fn_t *fn) {
    ref_dec(fn, (void (*)(void *))fn_destroy); 
}

// Mu Code handling functions. Code can be created 
// with the parsing functions in parse.c
void code_destroy(code_t *code);

// Code reference counting
mu_inline code_t *code_inc(code_t *c) { return ref_inc((void *)c); }
mu_inline void code_dec(code_t *c) { 
    ref_dec(c, (void (*)(void *))code_destroy);
}

// Access to code properties
mu_inline mu_t *code_imms(code_t *code) { 
    return (mu_t *)code->data; 
}

mu_inline struct code **code_fns(code_t *code) { 
    return (struct code **)&code->data[code->icount];
}

mu_inline const data_t *code_bcode(code_t *code) {
    return (const data_t *)&code->data[code->icount + code->fcount];
}


// C interface for calling functions
void fn_fcall(fn_t *fn, c_t c, mu_t *frame);

mu_t fn_call(fn_t *fn, c_t c, ...);


// Function representation
str_t *fn_repr(fn_t *f);


#endif
#endif
