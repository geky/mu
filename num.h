/*
 *  Number Definition
 */

#ifndef MU_NUM_H
#define MU_NUM_H
#include "mu.h"
#include "types.h"
#include <math.h>


// Number constants
#define minf mfloat(INFINITY)
#define mninf mfloat(-INFINITY)


// Number creating macros
mu_inline mu_t mfloat(mfloat_t n) {
    return (mu_t)(MU_NUM + (~7 &
        ((union { mfloat_t n; muint_t u; })n).u));
}

mu_inline mu_t mint(mint_t n)   { return mfloat((mfloat_t)n); }
mu_inline mu_t muint(muint_t n) { return mfloat((mfloat_t)n); }

// Number accessing macros
mu_inline mfloat_t num_float(mu_t m) {
    return ((union { muint_t u; mfloat_t n; })
        ((muint_t)m - MU_NUM)).n;
}

mu_inline mint_t  num_int(mu_t m)  { return (mint_t)num_float(m); }
mu_inline muint_t num_uint(mu_t m) { return (muint_t)num_float(m); }


// Number representation
mu_t num_repr(mu_t n);


#endif
