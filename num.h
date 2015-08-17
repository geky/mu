/*
 *  Number Definition
 */

#ifndef MU_NUM_H
#define MU_NUM_H
#include "mu.h"
#include "types.h"
#include "err.h"
#include "str.h"
#include <math.h>


// Number constants
#define minf mfloat(INFINITY)
#define mninf mfloat(-INFINITY)


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


// Arithmetic operations
mu_t num_neg(mu_t a);
mu_t num_add(mu_t a, mu_t b);
mu_t num_sub(mu_t a, mu_t b);
mu_t num_mul(mu_t a, mu_t b);
mu_t num_div(mu_t a, mu_t b);
mu_t num_idiv(mu_t a, mu_t b);
mu_t num_mod(mu_t a, mu_t b);
mu_t num_pow(mu_t a, mu_t b);

// Number representation
mu_t num_repr(mu_t n);


#endif
