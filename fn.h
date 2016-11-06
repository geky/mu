/*
 * First class functions
 */

#ifndef MU_FN_H
#define MU_FN_H
#include "mu.h"
#include "buf.h"


// Function frame type
//typedef uint8_t mcnt_t;

// Definition of C Function types
struct code;
typedef mcnt_t mbfn_t(mu_t *frame);
typedef mcnt_t msbfn_t(mu_t closure, mu_t *frame);


// Creation functions
mu_t fn_create(mu_t code, mu_t closure);

// Conversion operations
mu_t fn_frombfn(mcnt_t args, mbfn_t *bfn);
mu_t fn_fromsbfn(mcnt_t args, msbfn_t *sbfn, mu_t closure);

// Function calls
mcnt_t fn_tcall(mu_t f, mcnt_t fc, mu_t *frame);
void fn_fcall(mu_t f, mcnt_t fc, mu_t *frame);
mu_t fn_vcall(mu_t f, mcnt_t fc, va_list args);
mu_t fn_call(mu_t f, mcnt_t fc, ...);

// Iteration
bool fn_next(mu_t f, mcnt_t fc, mu_t *frame);


// Function tags
enum fn_flags {
    FN_BUILTIN = 1 << 0, // C builtin function
    FN_SCOPED  = 1 << 1, // Closure attached to function
    FN_WEAK    = 1 << 2, // Closure is weakly referenced
};

// Definition of code structure used to represent the
// executable component of Mu functions.
struct code {
    mcnt_t args;    // argument count
    uint8_t flags;  // function flags
    muintq_t regs;  // number of registers
    muintq_t scope; // size of scope

    mlen_t icount;  // number of immediate values
    mlen_t bcount;  // number of bytecode instructions

    mu_t data[];    // data that follows code header
                    // immediate values
                    // bytecode
};

// Code reference counting
mu_inline bool mu_iscode(mu_t m) {
    extern void code_destroy(mu_t);
    return mu_isbuf(m) && buf_dtor(m) == code_destroy;
}

mu_inline mu_t code_inc(mu_t c) {
    mu_assert(mu_iscode(c));
    return buf_inc(c);
}

mu_inline void code_dec(mu_t c) {
    mu_assert(mu_iscode(c));
    buf_dec(c);
}

// Code access functions
mu_inline struct code *code_header(mu_t c) {
    return ((struct code *)buf_data(c));
}

mu_inline mlen_t code_imms_len(mu_t c) {
    return ((struct code *)buf_data(c))->icount;
}

mu_inline mlen_t code_bcode_len(mu_t c) {
    return ((struct code *)buf_data(c))->bcount;
}

mu_inline mu_t *code_imms(mu_t c) {
    return ((struct code *)buf_data(c))->data;
}

mu_inline void *code_bcode(mu_t c) {
    return code_imms(c) + code_imms_len(c);
}


// Definition of the function type
//
// Functions are stored as function pointers paired with closures.
// Additionally several flags are defined to specify how the
// function should be called.
struct fn {
    mref_t ref;     // reference count
    mcnt_t args;    // argument count
    uint8_t flags;  // function flags

    mu_t closure;   // function closure

    union {
        mbfn_t *bfn;    // c function
        msbfn_t *sbfn;  // scoped c function
        mu_t code;      // compiled mu code
    } fn;
};

// Function creating functions
mu_inline mu_t mbfn(mcnt_t args, mbfn_t *bfn) {
    return fn_frombfn(args, bfn);
}

mu_inline mu_t msbfn(mcnt_t args, msbfn_t *sbfn, mu_t closure) {
    return fn_fromsbfn(args, sbfn, closure);
}

// Function reference counting
mu_inline mu_t fn_inc(mu_t f) {
    mu_assert(mu_isfn(f));
    ref_inc(f);
    return f;
}

mu_inline void fn_dec(mu_t f) {
    mu_assert(mu_isfn(f));
    extern void fn_destroy(mu_t);
    if (ref_dec(f)) {
        fn_destroy(f);
    }
}

// Function access
mu_inline mu_t fn_code(mu_t m) {
    if (!(((struct fn *)((muint_t)m - MTFN))->flags & FN_BUILTIN)) {
        return code_inc(((struct fn *)((muint_t)m - MTFN))->fn.code);
    } else {
        return 0;
    }
}

mu_inline mu_t fn_closure(mu_t m) {
    return mu_inc(((struct fn *)((muint_t)m - MTFN))->closure);
}


// Function constant macro
#define MBFN(name, args, bfn)                                               \
mu_pure mu_t name(void) {                                                   \
    static const struct fn inst = {0, args, FN_BUILTIN, 0, {bfn}};          \
    return (mu_t)((muint_t)&inst + MTFN);                                   \
}


#endif
