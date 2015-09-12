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
#define MU_INF  mfloat(INFINITY)
#define MU_NINF mfloat(-INFINITY)
#define MU_E    mfloat(2.71828182845904523536)
#define MU_PI   mfloat(3.14159265358979323846)


// Conversion operations
mu_t num_fromfloat(mfloat_t);
mu_t num_fromuint(muint_t);
mu_t num_fromint(mint_t);
mu_t num_fromstr(mu_t);

// Comparison operation
mint_t num_cmp(mu_t, mu_t);

// Arithmetic operations
mu_t num_neg(mu_t);
mu_t num_add(mu_t, mu_t);
mu_t num_sub(mu_t, mu_t);
mu_t num_mul(mu_t, mu_t);
mu_t num_div(mu_t, mu_t);
mu_t num_idiv(mu_t, mu_t);
mu_t num_mod(mu_t, mu_t);
mu_t num_pow(mu_t, mu_t);
mu_t num_log(mu_t, mu_t);

mu_t num_abs(mu_t);
mu_t num_floor(mu_t);
mu_t num_ceil(mu_t);

mu_t num_cos(mu_t);
mu_t num_acos(mu_t);
mu_t num_sin(mu_t);
mu_t num_asin(mu_t);
mu_t num_tan(mu_t);
mu_t num_atan(mu_t, mu_t);

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
mu_t num_repr(mu_t);

mu_t num_bin(mu_t);
mu_t num_oct(mu_t);
mu_t num_hex(mu_t);


// Number creating macros
mu_inline mu_t mfloat(mfloat_t n) { return num_fromfloat(n); }

mu_inline mu_t muint(muint_t n) {
    return (mu_t)(MU_NUM + (~7 &
        ((union { mfloat_t n; muint_t u; }){(mfloat_t)n}).u));
}

mu_inline mu_t mint(mint_t n) {
    return (mu_t)(MU_NUM + (~7 &
        ((union { mfloat_t n; muint_t u; }){(mfloat_t)n}).u));
}

// Number accessing macros
mu_inline mfloat_t num_float(mu_t m) {
    return ((union { muint_t u; mfloat_t n; }){(muint_t)m - MU_NUM}).n;
}

mu_inline muint_t num_uint(mu_t m) { return (muint_t)num_float(m); }
mu_inline mint_t  num_int(mu_t m)  { return (mint_t)num_float(m); }


#endif
