/*
 * Number type
 */

#ifndef MU_NUM_H
#define MU_NUM_H
#include "mu.h"


// Conversion operations
mu_t mu_num_fromfloat(mfloat_t);

// Comparison operation
mint_t mu_num_cmp(mu_t, mu_t);

// Arithmetic operations
mu_t mu_num_neg(mu_t);
mu_t mu_num_add(mu_t, mu_t);
mu_t mu_num_sub(mu_t, mu_t);
mu_t mu_num_mul(mu_t, mu_t);
mu_t mu_num_div(mu_t, mu_t);
mu_t mu_num_idiv(mu_t, mu_t);
mu_t mu_num_mod(mu_t, mu_t);
mu_t mu_num_pow(mu_t, mu_t);
mu_t mu_num_log(mu_t, mu_t);

mu_t mu_num_abs(mu_t);
mu_t mu_num_floor(mu_t);
mu_t mu_num_ceil(mu_t);

mu_t mu_num_cos(mu_t);
mu_t mu_num_acos(mu_t);
mu_t mu_num_sin(mu_t);
mu_t mu_num_asin(mu_t);
mu_t mu_num_tan(mu_t);
mu_t mu_num_atan(mu_t, mu_t);

// Bitwise operations
mu_t mu_num_not(mu_t);
mu_t mu_num_and(mu_t, mu_t);
mu_t mu_num_or(mu_t, mu_t);
mu_t mu_num_xor(mu_t, mu_t);

mu_t mu_num_shl(mu_t, mu_t);
mu_t mu_num_shr(mu_t, mu_t);

// Number representation
mu_t mu_num_parse(const mbyte_t **pos, const mbyte_t *end);
mu_t mu_num_repr(mu_t);

mu_t mu_num_bin(mu_t);
mu_t mu_num_oct(mu_t);
mu_t mu_num_hex(mu_t);


// Number creating functions
mu_inline mu_t mu_num_fromuint(muint_t n) {
    return (mu_t)(MTNUM + (~7 &
        ((union { mfloat_t n; muint_t u; }){(mfloat_t)n}).u));
}

mu_inline mu_t mu_num_fromint(mint_t n) {
    return (mu_t)(MTNUM + (~7 &
        ((union { mfloat_t n; muint_t u; }){(mfloat_t)n}).u));
}

// Number accessing functions
mu_inline mfloat_t mu_num_getfloat(mu_t m) {
    return ((union { muint_t u; mfloat_t n; }){(muint_t)m - MTNUM}).n;
}

mu_inline muint_t mu_num_getuint(mu_t m) {
    return (muint_t)mu_num_getfloat(m);
}

mu_inline mint_t  mu_num_getint(mu_t m)  {
    return (mint_t)mu_num_getfloat(m);
}


// Number constant macro
#define MU_GEN_FLOAT(name, num)                                             \
mu_pure mu_t name(void) {                                                   \
    return (mu_t)(MTNUM + (~7 &                                             \
        ((union { mfloat_t n; muint_t u; }){(mfloat_t)num}).u));            \
}

#define MU_GEN_UINT(name, num) MU_GEN_FLOAT(name, (muint_t)num)
#define MU_GEN_INT(name, num)  MU_GEN_FLOAT(name, (mint_t)num)


#endif
