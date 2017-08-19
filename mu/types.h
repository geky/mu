/*
 * Type definitions for Mu
 *
 * Copyright (c) 2016 Christopher Haster
 * Distributed under the MIT license in mu.h
 */
#ifndef MU_TYPES_H
#define MU_TYPES_H
#include "config.h"


// Definitions of the basic types used in Mu
// Default, half-sized, and quarter sized integer types
//
// Requires sizeof(muint_t) == sizeof(void *)
//
// The other sizes are used for struct packing
#ifdef MU32
typedef int8_t   mintq_t;
typedef uint8_t  muintq_t;
typedef int16_t  minth_t;
typedef uint16_t muinth_t;
typedef int32_t  mint_t;
typedef uint32_t muint_t;
#else
typedef int16_t  mintq_t;
typedef uint16_t muintq_t;
typedef int32_t  minth_t;
typedef uint32_t muinth_t;
typedef int64_t  mint_t;
typedef uint64_t muint_t;
#endif

// Currently the num type is just based on floats,
// although an integer implementation may be possible
//
// Requires sizeof(mfloat_t) <= sizeof(muint_t)
#ifdef MU32
typedef float  mfloat_t;
#else
typedef double mfloat_t;
#endif

// Smallest addressable unit, used for strings and bufs
typedef unsigned char mbyte_t;

// Length type for strings and tables
typedef muinth_t mlen_t;


// Reference type for reference counting, alignment is
// required for tagging pointers with mu types. The size
// doesn't matter too much, because if the reference count
// overflows, the variable just becomes constant.
typedef mu_aligned(8) muinth_t mref_t;

// Smallest allocatable size
#define MU_MINALLOC (4*sizeof(muint_t))

// Tags for mu types. The tag is a three bit type specifier
// located in lowest bits of each variable.
// 3b00x indicates type is not reference counted
typedef enum mtype {
    MTNIL  = 0, // nil
    MTNUM  = 1, // number
    MTSTR  = 3, // string
    MTBUF  = 2, // buffer
    MTDBUF = 6, // managed buffer
    MTTBL  = 4, // table
    MTRTBL = 5, // read-only table
    MTFN   = 7, // function
} mtype_t;

// Here is the heart of Mu, the mu_t type
//
// Doesn't necessarily point to anything, but using a
// void* would risk unwanted implicit conversions.
typedef struct mu *mu_t;

// Access to mu type components
mu_inline mtype_t mu_gettype(mu_t m) { return 7 & (muint_t)m; }
mu_inline mref_t mu_getref(mu_t m) { return *(mref_t *)(~7 & (muint_t)m); }

// Properties of variables
mu_inline bool mu_isnil(mu_t m) { return !m; }
mu_inline bool mu_isnum(mu_t m) { return mu_gettype(m) == MTNUM; }
mu_inline bool mu_isstr(mu_t m) { return mu_gettype(m) == MTSTR; }
mu_inline bool mu_isbuf(mu_t m) { return (3 & (muint_t)m) == MTBUF; }
mu_inline bool mu_istbl(mu_t m) { return (6 & (muint_t)m) == MTTBL; }
mu_inline bool mu_isfn(mu_t m)  { return mu_gettype(m) == MTFN;  }
mu_inline bool mu_isref(mu_t m) { return 6 & (muint_t)m; }

// Reference counting for mu types
//
// Deallocates immediately when reference count hits zero. If a type
// does have zero, this indicates the variable is constant and may be
// statically allocated. As a nice side effect, overflow results in
// constant variables.
mu_inline mu_t mu_inc(mu_t m) {
    if (mu_isref(m)) {
        mref_t *ref = (mref_t *)(~7 & (muint_t)m);
        mref_t count = *ref;

        if (count != 0) {
            count++;
            *ref = count;
        }
    }

    return m;
}

mu_inline void mu_dec(mu_t m) {
    if (mu_isref(m)) {
        mref_t *ref = (mref_t *)(~7 & (muint_t)m);
        mref_t count = *ref;

        if (count != 0) {
            count--;
            *ref = count;

            if (count == 0) {
                extern void mu_destroy(mu_t m);
                mu_destroy(m);
            }
        }
    }
}


// Multiple variables can be passed in a frame,
// which is a small array of MU_FRAME elements.
//
// If more than MU_FRAME elements need to be passed, the
// frame count of 0xf indicates a table containing the true
// elements is passed as the first value in the frame.
// 
// For function calls, the frame count is split into two
// nibbles for arguments and return values, in that order.
#define MU_FRAME 4

// Type for frame counts, for functions calls, the two nibbles
// are used for arguments and return values seperately.
typedef uint8_t mcnt_t;

// Frame operations
mu_inline mlen_t mu_framecount(mcnt_t fc) {
    return (fc > MU_FRAME) ? 1 : fc;
}

mu_inline void mu_framemove(mcnt_t fc, mu_t *dframe, mu_t *sframe) {
    memcpy(dframe, sframe, sizeof(mu_t)*mu_framecount(fc));
}

void mu_frameconvert(mcnt_t sc, mcnt_t dc, mu_t *frame);


// Declaration of mu constants, requires other MU_DEF_* for definition
#define MU_DEF(name) \
extern mu_pure mu_t name(void);


#endif
