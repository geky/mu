/*
 *  Number Definition
 */

#ifndef MU_NUM_H
#define MU_NUM_H
#include "mu.h"
#include "types.h"
#include <math.h>


// Definition of number type
typedef float num_t;

// Number constants
#define minf mnum(INFINITY)
#define mninf mnum(-INFINITY)


// Number creating macros
mu_inline mu_t mnum(num_t n) {
    return (mu_t)(MU_NUM + (~7 &
        ((union { num_t n; uint_t u; })n).u));
}

mu_inline mu_t mint(int_t n)     { return mnum((num_t)n); }
mu_inline mu_t muint(uint_t n)   { return mnum((num_t)n); }
mu_inline mu_t mfloat(float n)   { return mnum((num_t)n); }
mu_inline mu_t mdouble(double n) { return mnum((num_t)n); }

// Number accessing macros
mu_inline num_t num_num(mu_t m) {
    return ((union { uint_t u; num_t n; })
        ((uint_t)m - MU_NUM)).n;
}

mu_inline int_t  num_int(mu_t m)    { return (int_t) num_num(m); }
mu_inline uint_t num_uint(mu_t m)   { return (uint_t)num_num(m); }
mu_inline float  num_float(mu_t m)  { return (float) num_num(m); }
mu_inline double num_double(mu_t m) { return (double)num_num(m); }


// Hashing and equality for numbers
bool num_equals(mu_t a, mu_t b);
hash_t num_hash(mu_t n);

// Number parsing and representation
mu_t num_parse(const byte_t **off, const byte_t *end);
mu_t num_repr(mu_t n);


// Check to see if number is equivalent to its hash
mu_inline bool mu_ishash(mu_t m) {
    return mu_isnum(m) && num_uint(m) == num_hash(m); 
}


// TODO move to lexer?
// Obtains ascii value
mu_inline int_t num_val(byte_t s) {
    if (s >= '0' && s <= '9')
        return s - '0';
    else if (s >= 'a' && s <= 'f')
        return s - 'a' + 10;
    else if (s >= 'A' && s <= 'F')
        return s - 'A' + 10;
    else
        return 0xff;
}

mu_inline byte_t num_ascii(int_t i) {
    if (i < 10)
        return '0' + i;
    else
        return 'a' + (i-10);
}


#endif
