/*
 * First class function variable type
 */

#ifndef MU_FN_H
#define MU_FN_H
#include "mu.h"
#include "types.h"
#include "frame.h"


// Definition of C Function types
typedef frame_t bfn_t(mu_t *frame);
typedef frame_t sbfn_t(mu_t closure, mu_t *frame);

// Definition of code structure used to represent the 
// executable component of Mu functions.
mu_aligned struct code {
    ref_t ref;      // reference count
    frame_t args;   // argument count
    uintq_t type;   // function type
    uintq_t regs;   // number of registers
    uintq_t scope;  // size of scope

    len_t icount;   // number of immediate values
    len_t fcount;   // number of code objects
    len_t bcount;   // number of bytecode instructions

    union {               // data that follows code header
        mu_t imms;        // immediate values
        struct code *fns; // code objects
        byte_t bcode;     // bytecode
    } data[];
};

// Definition of the function type
//
// Functions are stored as function pointers paired with closures.
// Additionally several flags are defined to specify how the 
// function should be called.
mu_aligned struct fn {
    ref_t ref;    // reference count
    frame_t args; // argument count
    uintq_t type; // function type

    mu_t closure; // function closure

    union {
        bfn_t *bfn;        // c function
        sbfn_t *sbfn;      // scoped c function
        struct code *code; // compiled mu code
    };
};


// C Function creating functions
mu_t mfn(struct code *c, mu_t closure);
mu_t mbfn(frame_t args, bfn_t *bfn);
mu_t msbfn(frame_t args, sbfn_t *sbfn, mu_t closure);

// Function access functions
mu_inline struct code *fn_code(mu_t m) {
    return ((struct fn *)((uint_t)m - MU_FN))->code;
}

mu_inline mu_t fn_closure(mu_t m) {
    return ((struct fn *)((uint_t)m - MU_FN))->closure;
}

// Code access functions
mu_inline mu_t *code_imms(struct code *c) {
    return &c->data[0].imms;
}

mu_inline struct code **code_fns(struct code *c) {
    return &c->data[c->icount].fns;
}

mu_inline void *code_bcode(struct code *c) {
    return (void *)&c->data[c->icount+c->fcount].bcode;
}


// Function reference counting
mu_inline mu_t fn_inc(mu_t m) { ref_inc(m); return m; }
mu_inline void fn_dec(mu_t m) { 
    extern void (*const mu_destroy_table[6])(mu_t);
    if (ref_dec(m)) mu_destroy_table[mu_type(m)-2](m); 
}

// Code reference counting
mu_inline struct code *code_inc(struct code *c) { ref_inc(c); return c; }
mu_inline void code_dec(struct code *c) {
    extern void code_destroy(struct code *);
    if (ref_dec(c)) code_destroy(c); 
}


// Function calls
void fn_fcall(mu_t f, frame_t fc, mu_t *frame);
void bfn_fcall(mu_t f, frame_t fc, mu_t *frame);
void sbfn_fcall(mu_t f, frame_t fc, mu_t *frame);

// Function operations
mu_t fn_bind(mu_t f, mu_t args);
mu_t fn_repr(mu_t f);


#endif
