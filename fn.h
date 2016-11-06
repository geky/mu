/*
 * First class functions
 */

#ifndef MU_FN_H
#define MU_FN_H
#include "mu.h"
#include "buf.h"


// Definition of C Function types
typedef mcnt_t mbfn_t(mu_t *frame);
typedef mcnt_t msbfn_t(mu_t closure, mu_t *frame);


// Conversion operations
mu_t mu_fn_frombfn(mcnt_t args, mbfn_t *bfn);
mu_t mu_fn_fromsbfn(mcnt_t args, msbfn_t *sbfn, mu_t closure);
mu_t mu_fn_fromcode(mu_t code, mu_t closure);

// Function calls
mcnt_t mu_fn_tcall(mu_t f, mcnt_t fc, mu_t *frame);
void mu_fn_fcall(mu_t f, mcnt_t fc, mu_t *frame);
mu_t mu_fn_vcall(mu_t f, mcnt_t fc, va_list args);
mu_t mu_fn_call(mu_t f, mcnt_t fc, ...);

// Iteration
bool mu_fn_next(mu_t f, mcnt_t fc, mu_t *frame);


// Function tags
enum mu_fn_flags {
    MFN_BUILTIN = 1 << 0, // C builtin function
    MFN_SCOPED  = 1 << 1, // Closure attached to function
    MFN_WEAK    = 1 << 2, // Closure is weakly referenced
};

// Definition of code structure used to represent the
// executable component of Mu functions.
struct mcode {
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
    extern void mu_code_destroy(mu_t);
    return mu_isbuf(m) && mu_buf_getdtor(m) == mu_code_destroy;
}

mu_inline mu_t mu_code_inc(mu_t c) {
    mu_assert(mu_iscode(c));
    return mu_buf_inc(c);
}

mu_inline void mu_code_dec(mu_t c) {
    mu_assert(mu_iscode(c));
    mu_buf_dec(c);
}

// Code access functions
mu_inline struct mcode *mu_code_getheader(mu_t c) {
    return ((struct mcode *)mu_buf_getdata(c));
}

mu_inline mlen_t mu_code_getimmslen(mu_t c) {
    return ((struct mcode *)mu_buf_getdata(c))->icount;
}

mu_inline mlen_t mu_code_getbcodelen(mu_t c) {
    return ((struct mcode *)mu_buf_getdata(c))->bcount;
}

mu_inline mu_t *mu_code_getimms(mu_t c) {
    return ((struct mcode *)mu_buf_getdata(c))->data;
}

mu_inline void *mu_code_getbcode(mu_t c) {
    return mu_code_getimms(c) + mu_code_getimmslen(c);
}


// Definition of the function type
//
// Functions are stored as function pointers paired with closures.
// Additionally several flags are defined to specify how the
// function should be called.
struct mfn {
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

// Function reference counting
mu_inline mu_t mu_fn_inc(mu_t f) {
    mu_assert(mu_isfn(f));
    mu_ref_inc(f);
    return f;
}

mu_inline void mu_fn_dec(mu_t f) {
    mu_assert(mu_isfn(f));
    extern void mu_fn_destroy(mu_t);
    if (mu_ref_dec(f)) {
        mu_fn_destroy(f);
    }
}

// Function access
mu_inline mu_t mu_fn_getcode(mu_t m) {
    if (!(((struct mfn *)((muint_t)m - MTFN))->flags & MFN_BUILTIN)) {
        return mu_code_inc(((struct mfn *)((muint_t)m - MTFN))->fn.code);
    } else {
        return 0;
    }
}

mu_inline mu_t mu_fn_getclosure(mu_t m) {
    return mu_inc(((struct mfn *)((muint_t)m - MTFN))->closure);
}


// Function constant macro
#define MU_GEN_BFN(name, args, bfn)                                         \
mu_pure mu_t name(void) {                                                   \
    static const struct mfn inst = {0, args, MFN_BUILTIN, 0, {bfn}};          \
    return (mu_t)((muint_t)&inst + MTFN);                                   \
}


#endif
