/*
 * First class function variable type
 */

#ifndef MU_FN_H
#define MU_FN_H
#include "mu.h"
#include "types.h"
#include "frame.h"


// Flags used to define operation of functions
struct fn_flags {
    uintq_t regs;   // number of registers
    uintq_t scope;  // size of scope to allocate
    frame_t args;   // argument count
    uintq_t type;   // function type
};

// Definition of C Function types
typedef frame_t bfn_t(mu_t *frame);
typedef frame_t sbfn_t(mu_t closure, mu_t *frame);

// Definition of code structure used to represent
// executable, but not instantiated functions
struct code {
    ref_t ref;  // reference count

    len_t bcount; // number of bytes in bytecode
    len_t icount; // number of immediate values
    len_t fcount; // number of code structures

    struct fn_flags flags;

    // immediates, code structures, and bytecode follow
    // the code header in order
    void *data[];
};

// Definition of Mu function type
//
// Functions are stored as function pointers paired with
// closure tables. Additionally several flags are defined
// to specify how the function should be called and how
// much storage to allocate
mu_aligned struct fn {
    ref_t ref; // reference count

    struct fn_flags flags;

    mu_t closure; // function closure

    union {
        bfn_t *bfn;        // c function
        sbfn_t *sbfn;      // scoped c function
        struct code *code; // compiled mu code
    };
};


// C Function creating functions and macros
mu_t mbfn(frame_t args, bfn_t *bfn);
mu_t msbfn(frame_t args, sbfn_t *sbfn, mu_t closure);

// Mu Function creating functions
mu_t fn_create(struct code *code, mu_t closure);
void fn_destroy(mu_t f);

mu_t fn_parse_expr(mu_t code, mu_t closure);
mu_t fn_parse_fn(mu_t code, mu_t closure);
mu_t fn_parse_module(mu_t code, mu_t closure);

// Function reference counting
mu_inline mu_t fn_inc(mu_t m) { ref_inc(m); return m; }
mu_inline void fn_dec(mu_t m) { if (ref_dec(m)) fn_destroy(m); }


// Mu Code handling functions. Code can be created 
// with the parsing functions in parse.c
void code_destroy(struct code *code);

// This is a workaround for how the parser stores code
// and how the vm accesses functions
// TODO remove these?
mu_inline mu_t mfn_(struct fn *f) {
    return (mu_t)((uint_t)f + MU_FN);
}

mu_inline struct fn *fn_fn_(mu_t m) {
    return ((struct fn *)((uint_t)m - MU_FN));
}

mu_inline struct code *fn_code_(mu_t m) {
    return ((struct fn *)((uint_t)m - MU_FN))->code;
}

// Code reference counting
mu_inline struct code *code_inc(struct code *c) { ref_inc(c); return c; }
mu_inline void code_dec(struct code *c) { if (ref_dec(c)) code_destroy(c); }

// Access to code properties
mu_inline mu_t *code_imms(struct code *code) { 
    return (mu_t *)code->data; 
}

mu_inline struct code **code_fns(struct code *code) { 
    return (struct code **)&code->data[code->icount];
}

mu_inline const void *code_bcode(struct code *code) {
    return (const void *)&code->data[code->icount + code->fcount];
}


// C interface for calling functions
void fn_fcall(mu_t f, frame_t c, mu_t *frame);
mu_t fn_vcall(mu_t f, frame_t c, va_list args);
mu_t fn_call(mu_t f, frame_t c, ...);

// Function representation
mu_t fn_repr(mu_t f);


#endif
