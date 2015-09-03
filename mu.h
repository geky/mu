/*
 * Variable types and definitions
 */

#ifndef MU_TYPES_H
#define MU_TYPES_H
#include "config.h"
#include "mem.h"
#include <stdarg.h>


// Smallest addressable unit
typedef unsigned char mbyte_t;

// Length type for strings/tables
typedef muinth_t mlen_t;


// Three bit type specifier located in lowest bits of each variable
// 3b00x indicates type is not reference counted
// 3b100 is the only mutable variable
// 3b11x are currently reserved
enum mtype {
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

// Nil is stored as null
#define mnil  ((mu_t)0)

// Access to type and general components
mu_inline enum mtype mu_type(mu_t m) { return 7 & (muint_t)m; }
mu_inline mref_t mu_ref(mu_t m) { return *(mref_t *)(~7 & (muint_t)m); }

// Properties of variables
mu_inline bool mu_isnil(mu_t m) { return !m; }
mu_inline bool mu_isnum(mu_t m) { return mu_type(m) == MU_NUM; }
mu_inline bool mu_isstr(mu_t m) { return mu_type(m) == MU_STR; }
mu_inline bool mu_istbl(mu_t m) { return (6 & mu_type(m)) == MU_TBL; }
mu_inline bool mu_isfn(mu_t m)  { return mu_type(m) >= MU_FN;  }
mu_inline bool mu_isref(mu_t m) { return 6 & (muint_t)m; }

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


// Multiple variables can be passed in a frame,
// which is a small array of MU_FRAME elements.
// 
// If more than MU_FRAME elements need to be passed
// a table containing the true elements is used instead.
#define MU_FRAME 4

// Type for specifying frame counts.
//
// The value of 0xf indicates a table is used.
// For function calls, the frame count is split into two 
// nibbles for arguments and return values, in that order.
typedef uint8_t mc_t;

// Conversion between different frame types
void mu_fto(mc_t dc, mc_t sc, mu_t *frame);

mu_inline muint_t mu_fcount(mc_t fc) {
    return (fc == 0xf) ? 1 : fc;
}

mu_inline muint_t mu_fsize(mc_t fc) {
    return sizeof(mu_t) * mu_fcount(fc);
}


// Standard functions are provided as C functions as well as
// Mu functions in readonly builtins table
mu_const mu_t mu_builtins(void);

// Type casts
mu_t mu_num(mu_t m);
mu_t mu_str(mu_t m);
mu_t mu_tbl(mu_t m, mu_t tail);
mu_t mu_fn(mu_t m);

// Table related functions performed on variables
mu_t mu_lookup(mu_t m, mu_t key);
void mu_insert(mu_t m, mu_t key, mu_t val);
void mu_assign(mu_t m, mu_t key, mu_t val);

// Function calls performed on variables
mc_t mu_tcall(mu_t m, mc_t fc, mu_t *frame);
void mu_fcall(mu_t m, mc_t fc, mu_t *frame);
mu_t mu_vcall(mu_t m, mc_t fc, va_list args);
mu_t mu_call(mu_t m, mc_t fc, ...);

// Comparison operation
bool mu_is(mu_t a, mu_t type);
mint_t mu_cmp(mu_t a, mu_t b);

// Arithmetic operations
mu_t mu_pos(mu_t a);
mu_t mu_neg(mu_t a);
mu_t mu_add(mu_t a, mu_t b);
mu_t mu_sub(mu_t a, mu_t b);
mu_t mu_mul(mu_t a, mu_t b);
mu_t mu_div(mu_t a, mu_t b);

mu_t mu_abs(mu_t a);
mu_t mu_floor(mu_t a);
mu_t mu_ceil(mu_t a);
mu_t mu_idiv(mu_t a, mu_t b);
mu_t mu_mod(mu_t a, mu_t b);

mu_t mu_pow(mu_t a, mu_t b);
mu_t mu_log(mu_t a, mu_t b);

mu_t mu_cos(mu_t a);
mu_t mu_acos(mu_t a);
mu_t mu_sin(mu_t a);
mu_t mu_asin(mu_t a);
mu_t mu_tan(mu_t a);
mu_t mu_atan(mu_t a);
mu_t mu_atan2(mu_t a, mu_t b);

// Bitwise/Set operations
mu_t mu_not(mu_t a);
mu_t mu_and(mu_t a, mu_t b);
mu_t mu_or(mu_t a, mu_t b);
mu_t mu_xor(mu_t a, mu_t b);
mu_t mu_diff(mu_t a, mu_t b);

mu_t mu_shl(mu_t a, mu_t b);
mu_t mu_shr(mu_t a, mu_t b);

// String representation
mu_t mu_parse(mu_t m);
mu_t mu_repr(mu_t m);
mu_t mu_dump(mu_t m, mu_t depth, mu_t indent);

mu_t mu_bin(mu_t m);
mu_t mu_oct(mu_t m);
mu_t mu_deci(mu_t m);
mu_t mu_hex(mu_t m);

// String operations
mu_t mu_find(mu_t m, mu_t sub);
mu_t mu_replace(mu_t m, mu_t sub, mu_t rep, mu_t max);

mu_t mu_split(mu_t m, mu_t delim);
mu_t mu_join(mu_t iter, mu_t delim);

mu_t mu_pad(mu_t m, mu_t len, mu_t pad);
mu_t mu_strip(mu_t m, mu_t dir, mu_t pad);

// Data structure operations
mlen_t mu_len(mu_t m);
mu_t mu_tail(mu_t m);
mu_t mu_ro(mu_t m);

void mu_push(mu_t m, mu_t v, mu_t i);
mu_t mu_pop(mu_t m, mu_t i);

mu_t mu_concat(mu_t a, mu_t b, mu_t offset);
mu_t mu_subset(mu_t a, mu_t lower, mu_t upper);

// Function operations
mu_t mu_bind(mu_t m, mu_t args);
mu_t mu_comp(mu_t ms);

mu_t mu_map(mu_t m, mu_t iter);
mu_t mu_filter(mu_t m, mu_t iter);
mu_t mu_reduce(mu_t m, mu_t iter, mu_t inits);

bool mu_any(mu_t m, mu_t iter);
bool mu_all(mu_t m, mu_t iter);

// Iterators and generators
mu_t mu_iter(mu_t m);
mu_t mu_pairs(mu_t m);

mu_t mu_range(mu_t start, mu_t stop, mu_t step);
mu_t mu_repeat(mu_t value, mu_t times);
mu_t mu_cycle(mu_t iter, mu_t times);

// Iterator manipulation
mu_t mu_zip(mu_t iters);
mu_t mu_chain(mu_t iters);
mu_t mu_tee(mu_t iter, mu_t n);

mu_t mu_take(mu_t m, mu_t iter);
mu_t mu_drop(mu_t m, mu_t iter);

bool mu_any(mu_t m, mu_t iter);
bool mu_all(mu_t m, mu_t iter);

// Iterator ordering operations
mu_t mu_min(mu_t iter);
mu_t mu_max(mu_t iter);

mu_t mu_reverse(mu_t iter);
mu_t mu_sort(mu_t iter);

// Random number generation
mu_t mu_seed(mu_t m);
mu_t mu_random(void);


#endif
