/*
 *  Number Definition
 */

#ifdef MU_DEF
#ifndef MU_NUM_DEF
#define MU_NUM_DEF
#include "mu.h"


// Definition of number type
typedef float num_t;


#endif
#else
#ifndef MU_NUM_H
#define MU_NUM_H
#include "var.h"


// Number creating macros
mu_inline num_t num_float(float n)   { return (num_t)n; }
mu_inline num_t num_double(double n) { return num_float((float)n); }
mu_inline num_t num_int(int_t n)     { return num_float((float)n); }
mu_inline num_t num_uint(uint_t n)   { return num_float((float)n); }

mu_inline var_t vint(int_t n)     { return vnum(num_int(n)); }
mu_inline var_t vuint(uint_t n)   { return vnum(num_uint(n)); }
mu_inline var_t vfloat(float n)   { return vnum(num_float(n)); }
mu_inline var_t vdouble(double n) { return vnum(num_double(n)); }

// Number accessing macros
mu_inline int_t  num_getint(num_t n)    { return (int_t)n; }
mu_inline uint_t num_getuint(num_t n)   { return (uint_t)n; }
mu_inline float  num_getfloat(num_t n)  { return (float)n; }
mu_inline double num_getdouble(num_t n) { return (double)n; }

mu_inline int_t  getint(var_t v)    { return num_getint(getnum(v)); }
mu_inline uint_t getuint(var_t v)   { return num_getuint(getnum(v)); }
mu_inline float  getfloat(var_t v)  { return num_getfloat(getnum(v)); }
mu_inline double getdouble(var_t v) { return num_getdouble(getnum(v)); }


// Hashing and equality for numbers
bool num_equals(num_t a, num_t b);
hash_t num_hash(num_t n);

// Number parsing and representation
num_t num_parse(const data_t **off, const data_t *end);
str_t *num_repr(num_t n, eh_t *eh);


// Check to see if number is equivalent to its hash
mu_inline bool num_ishash(num_t n) { return num_getuint(n) == num_hash(n); }
mu_inline bool ishash(var_t v) { return isnum(v) && num_ishash(getnum(v)); }


// Obtains ascii value
mu_inline int_t num_val(data_t s) {
    if (s >= '0' && s <= '9')
        return s - '0';
    else if (s >= 'a' && s <= 'f')
        return s - 'a' + 10;
    else if (s >= 'A' && s <= 'F')
        return s - 'A' + 10;
    else
        return 0xff;
}

mu_inline data_t num_ascii(int_t i) {
    if (i < 10)
        return '0' + i;
    else
        return 'a' + (i-10);
}


#endif
#endif
