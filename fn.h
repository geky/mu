/*
 * First class function variable type
 */

#ifndef MU_FN_H
#define MU_FN_H
#include "mu.h"


// Function constants
#define MU_ID fn_id()
mu_pure mu_t fn_id(void);

// Definition of C Function types
struct code;
typedef mc_t mbfn_t(mu_t *frame);
typedef mc_t msfn_t(mu_t closure, mu_t *frame);

// Conversion operations
mu_t fn_fromcode(struct code *c, mu_t closure);
mu_t fn_frombfn(mc_t args, mbfn_t *bfn);
mu_t fn_fromsfn(mc_t args, msfn_t *sfn, mu_t closure);

// Function calls
mc_t fn_tcall(mu_t f, mc_t fc, mu_t *frame);
void fn_fcall(mu_t f, mc_t fc, mu_t *frame);

// Iteration
bool fn_next(mu_t f, mc_t fc, mu_t *frame);

// Function operations
mu_t fn_bind(mu_t f, mu_t args);
mu_t fn_comp(mu_t fs);

mu_t fn_map(mu_t f, mu_t iter);
mu_t fn_filter(mu_t f, mu_t iter);
mu_t fn_reduce(mu_t f, mu_t iter, mu_t inits);

bool fn_any(mu_t f, mu_t iter);
bool fn_all(mu_t f, mu_t iter);

// Iterators and generators
mu_t fn_range(mu_t start, mu_t stop, mu_t step);
mu_t fn_repeat(mu_t value, mu_t times);
mu_t fn_cycle(mu_t iter, mu_t times);

// Iterator manipulation
mu_t fn_zip(mu_t iters);
mu_t fn_chain(mu_t iters);
mu_t fn_tee(mu_t iter, mu_t n);

mu_t fn_take(mu_t cond, mu_t iter);
mu_t fn_drop(mu_t cond, mu_t iter);

// Iterator ordering
mu_t fn_min(mu_t iter);
mu_t fn_max(mu_t iter);

mu_t fn_reverse(mu_t iter);
mu_t fn_sort(mu_t iter);

// String representation
mu_t fn_repr(mu_t f);


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
        msfn_t *sfn;       // scoped c function
        struct code *code; // compiled mu code
    };
};


// C Function creating functions
mu_inline mu_t mcode(struct code *c, mu_t closure) {
    return fn_fromcode(c, closure);
}

mu_inline mu_t mbfn(mc_t args, mbfn_t *bfn) {
    return fn_frombfn(args, bfn);
}

mu_inline mu_t msfn(mc_t args, msfn_t *sfn, mu_t closure) {
    return fn_fromsfn(args, sfn, closure);
}

#define mcfn(args, bfn) ({                      \
    static const struct fn _c =                 \
        {0, args, MU_BFN - MU_FN, 0, {bfn}};    \
                                                \
    (mu_t)((muint_t)&_c + MU_BFN);              \
})


// Function access
mu_inline struct code *fn_code(mu_t m) {
    return ((struct fn *)((muint_t)m - MU_FN))->code;
}

mu_inline mu_t fn_closure(mu_t m) {
    return ((struct fn *)((muint_t)m - MU_FN))->closure;
}


// Function reference counting
mu_inline struct code *code_inc(struct code *c) {
    ref_inc(c);
    return c;
}

mu_inline void code_dec(struct code *c) {
    extern void code_destroy(struct code *);
    if (ref_dec(c))
        code_destroy(c);
}

mu_inline mu_t fn_inc(mu_t f) {
    mu_assert(mu_isfn(f));
    ref_inc(f);
    return f;
}

mu_inline void fn_dec(mu_t f) {
    mu_assert(mu_isfn(f));
    return mu_dec(f);
}


#endif
