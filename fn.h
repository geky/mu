/*
 * First class function variable type
 */

#ifndef MU_FN_H
#define MU_FN_H
#include "mu.h"
#include "types.h"
#include "frame.h"


// Definition of C Function types
typedef mc_t mbfn_t(mu_t *frame);
typedef mc_t msbfn_t(mu_t closure, mu_t *frame);

// Definition of code structure used to represent the
// executable component of Mu functions.
struct code {
    mref_t ref;     // reference count
    mc_t args;      // argument count
    muintq_t type;  // function type
    muintq_t regs;  // number of registers
    muintq_t scope; // size of scope

    mlen_t icount;  // number of immediate values
    mlen_t fcount;  // number of code objects
    mlen_t bcount;  // number of bytecode instructions

    union {               // data that follows code header
        mu_t imms;        // immediate values
        struct code *fns; // code objects
        mbyte_t bcode;    // bytecode
    } data[];
};

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

// Code reference counting
mu_inline struct code *code_inc(struct code *c) {
    ref_inc(c); return c;
}

mu_inline void code_dec(struct code *c) {
    extern void code_destroy(struct code *);
    if (ref_dec(c)) code_destroy(c);
}


// Definition of the function type
//
// Functions are stored as function pointers paired with closures.
// Additionally several flags are defined to specify how the
// function should be called.
struct fn {
    mref_t ref;    // reference count
    mc_t args;     // argument count
    muintq_t type; // function type

    mu_t closure;  // function closure

    union {
        mbfn_t *bfn;       // c function
        msbfn_t *sbfn;     // scoped c function
        struct code *code; // compiled mu code
    };
};

// Function access functions
mu_inline struct code *fn_code(mu_t m) {
    return code_inc(((struct fn *)((muint_t)m - MU_FN))->code);
}

mu_inline mu_t fn_closure(mu_t m) {
    return mu_inc(((struct fn *)((muint_t)m - MU_FN))->closure);
}

// C Function creating functions
mu_t mfn(struct code *c, mu_t closure);
mu_t mbfn(mc_t args, mbfn_t *bfn);
mu_t msbfn(mc_t args, msbfn_t *sbfn, mu_t closure);

#define mcfn(args, bfn) ({                      \
    static const struct fn _c =                 \
        {0, args, MU_BFN - MU_FN, 0, {bfn}};    \
                                                \
    (mu_t)((muint_t)&_c + MU_BFN);              \
})


// Function calls
mc_t fn_tcall(mu_t f, mc_t fc, mu_t *frame);
mc_t bfn_tcall(mu_t f, mc_t fc, mu_t *frame);
mc_t sbfn_tcall(mu_t f, mc_t fc, mu_t *frame);

void fn_fcall(mu_t f, mc_t fc, mu_t *frame);
void bfn_fcall(mu_t f, mc_t fc, mu_t *frame);
void sbfn_fcall(mu_t f, mc_t fc, mu_t *frame);

// Function operations
mu_t fn_bind(mu_t f, mu_t args);
mu_t fn_map(mu_t f, mu_t m);
mu_t fn_filter(mu_t f, mu_t m);
mu_t fn_reduce(mu_t f, mu_t m, mu_t inits);
mu_t fn_repr(mu_t f);


// Function reference counting
mu_inline mu_t fn_inc(mu_t m) {
    mu_assert(mu_isfn(m));
    ref_inc(m); return m;
}

mu_inline void fn_dec(mu_t m) {
    mu_assert(mu_isfn(m));
    extern void (*const mu_destroy_table[6])(mu_t);
    if (ref_dec(m)) mu_destroy_table[mu_type(m)-2](m);
}


#endif
