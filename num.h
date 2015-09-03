/*
 *  Number Definition
 */

#ifndef MU_NUM_H
#define MU_NUM_H
#include "mu.h"
#include "err.h"
#include "str.h"
#include <math.h>


// Number constants
#define minf    mfloat(INFINITY)
#define mninf   mfloat(-INFINITY)
#define mexp    mfloat(2.71828182845904523536)
#define mpi     mfloat(3.14159265358979323846)


// Number creating macros
mu_t mfloat(mfloat_t n);

mu_inline mu_t mint(mint_t n) {
    union { mfloat_t n; muint_t u; } v = { (mfloat_t)n };
    return (mu_t)(MU_NUM + (~7 & v.u));
}

mu_inline mu_t muint(muint_t n) {
    union { mfloat_t n; muint_t u; } v = { (mfloat_t)n };
    return (mu_t)(MU_NUM + (~7 & v.u));
}

// Number accessing macros
mu_inline mfloat_t num_float(mu_t m) {
    union { muint_t u; mfloat_t n; } v = { (muint_t)m - MU_NUM };
    return v.n;
}

mu_inline mint_t  num_int(mu_t m)  { return (mint_t)num_float(m); }
mu_inline muint_t num_uint(mu_t m) { return (muint_t)num_float(m); }


// Conversion operations
mu_t num_fromstr(mu_t m);

// Comparison operation
mint_t num_cmp(mu_t a, mu_t b);

// Arithmetic operations
mu_t num_neg(mu_t);
mu_t num_add(mu_t, mu_t);
mu_t num_sub(mu_t, mu_t);
mu_t num_mul(mu_t, mu_t);
mu_t num_div(mu_t, mu_t);

mu_t num_abs(mu_t);
mu_t num_floor(mu_t);
mu_t num_ceil(mu_t);
mu_t num_idiv(mu_t, mu_t);
mu_t num_mod(mu_t, mu_t);

mu_t num_pow(mu_t, mu_t);
mu_t num_log(mu_t, mu_t);

mu_t num_cos(mu_t);
mu_t num_acos(mu_t);
mu_t num_sin(mu_t);
mu_t num_asin(mu_t);
mu_t num_tan(mu_t);
mu_t num_atan(mu_t);
mu_t num_atan2(mu_t, mu_t);

// Bitwise operations
mu_t num_not(mu_t);
mu_t num_and(mu_t, mu_t);
mu_t num_or(mu_t, mu_t);
mu_t num_xor(mu_t, mu_t);

mu_t num_shl(mu_t, mu_t);
mu_t num_shr(mu_t, mu_t);

// Random number generation
mu_t num_seed(mu_t);

// Number representation
mu_t num_parse(const mbyte_t **pos, const mbyte_t *end);
mu_t num_repr(mu_t n);

mu_t num_bin(mu_t n);
mu_t num_oct(mu_t n);
mu_t num_hex(mu_t n);


#endif
