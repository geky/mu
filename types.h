/*
 * Variable types and definitions
 */

#ifndef MU_TYPES_H
#define MU_TYPES_H
#include "mu.h"
#include "mem.h"
#include <stdarg.h>


// Smallest addressable unit
typedef unsigned char byte_t;
#define MU_MAXBYTE UCHAR_MAX

// Length type for strings/tables
typedef uinth_t len_t;
#define MU_MAXLEN MU_MAXUINTH

// Type of hash results
typedef uint_t hash_t;
#define MU_MAXHASH MU_MAXUINT

// Type for specifying frame counts
typedef uint8_t frame_t;


// Three bit type specifier located in lowest bits of each variable
// 3b00x indicates type is not reference counted
// 3b100 is the only mutable variable
// 3b11x are currently reserved
enum mu_type {
    MU_NIL  = 0, // nil
    MU_NUM  = 1, // number
    MU_STR  = 4, // string
    MU_TBL  = 2, // table
    MU_RTBL = 3, // readonly table
    MU_FN   = 5, // function
    MU_BFN  = 6, // builtin function
    MU_SBFN = 7, // scoped builtin function
};

// Declaration of mu type
// It doesn't necessarily point to anything, but using a
// void * would risk unwanted implicit conversions.
typedef struct mu *mu_t;


// Definitions of literal variables
#define mnil  ((mu_t)0)

// Access to type and general components
mu_inline enum mu_type mu_type(mu_t m) { return 7 & (uint_t)m; }
mu_inline ref_t mu_ref(mu_t m) { return *(ref_t *)(~7 & (uint_t)m); }

// Properties of variables
mu_inline bool mu_isnil(mu_t m) { return !m; }
mu_inline bool mu_isnum(mu_t m) { return mu_type(m) == MU_NUM; }
mu_inline bool mu_isstr(mu_t m) { return mu_type(m) == MU_STR; }
mu_inline bool mu_isfn(mu_t m)  { return mu_type(m) >= MU_FN;  }
mu_inline bool mu_istbl(mu_t m) { return (6 & mu_type(m)) == MU_TBL; }
mu_inline bool mu_isref(mu_t m) { return 6 & (uint_t)m; }


// Reference counting
mu_inline mu_t mu_inc(mu_t m) {
    if (mu_isref(m))
        ref_inc(m);

    return m;
}

mu_inline void mu_dec(mu_t m) {
    extern void (*const mu_destroy_table[6])(mu_t);

    if (mu_isref(m) && ref_dec(m))
        mu_destroy_table[mu_type(m)-2](m);
}


// Returns a string representation of the variable
mu_t mu_repr(mu_t m);

// Performs iteration on variables
mu_t mu_iter(mu_t m);

// Table related functions performed on variables
mu_t mu_lookup(mu_t m, mu_t key);
void mu_insert(mu_t m, mu_t key, mu_t val);
void mu_assign(mu_t m, mu_t key, mu_t val);

// Function calls performed on variables
frame_t mu_tcall(mu_t m, frame_t fc, mu_t *frame);
void mu_fcall(mu_t m, frame_t fc, mu_t *frame);
mu_t mu_vcall(mu_t m, frame_t fc, va_list args);
mu_t mu_call(mu_t m, frame_t fc, ...);


#endif
