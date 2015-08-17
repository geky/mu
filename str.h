/*
 *  String Definition
 */

#ifndef MU_STR_H
#define MU_STR_H
#include "mu.h"
#include "types.h"
#include <string.h>


// Definition of Mu's string types
// Each string is stored as a length and array of data.
//
// Strings must be interned before use in tables, and once interned,
// strings cannot be mutated without breaking things.
//
// Temporary mutable strings can be created and used through the
// mstr set of functions which store this info internally
struct str {
    mref_t ref;     // reference count
    mlen_t len;     // length of string
    mbyte_t data[]; // string data
};

// String access functions
mu_inline mlen_t str_len(mu_t m) {
    return ((struct str *)((muint_t)m - MU_STR))->len;
}

mu_inline const mbyte_t *str_bytes(mu_t m) {
    return ((struct str *)((muint_t)m - MU_STR))->data;
}

// String creation functions
mu_t mnstr(const mbyte_t *s, muint_t len);
mu_t mzstr(const char *s);

#define mcstr(s) ({                             \
    static mu_t _m = 0;                         \
    static const struct {                       \
        mref_t ref; mlen_t len;                 \
        mbyte_t data[(sizeof s)-1];             \
    } _c = {0, (sizeof s)-1, {s}};              \
                                                \
    if (!_m)                                    \
        _m = mstr_intern((mbyte_t *)_c.data,    \
                         (sizeof s)-1);         \
    _m;                                         \
})


// Functions for handling temporary mutable strings
mbyte_t *mstr_create(muint_t len);
mu_t mstr_intern(mbyte_t *s, muint_t len);

void mstr_insert(mbyte_t **s, muint_t *i, mbyte_t c);
void mstr_concat(mbyte_t **s, muint_t *i, mu_t c);
void mstr_ncat(mbyte_t **s, muint_t *i, const mbyte_t *c, muint_t len);
void mstr_zcat(mbyte_t **s, muint_t *i, const char *c);


// Reference counting
mu_inline mbyte_t *mstr_inc(mbyte_t *s) {
    ref_inc(s - mu_offset(struct str, data)); return s;
}

mu_inline void mstr_dec(mbyte_t *s) {
    extern void mstr_destroy(mbyte_t *);
    if (ref_dec(s - mu_offset(struct str, data))) mstr_destroy(s);
}


// Bitwise operations
mu_t str_not(mu_t a);
mu_t str_and(mu_t a, mu_t b);
mu_t str_or(mu_t a, mu_t b);
mu_t str_xor(mu_t a, mu_t b);
mu_t str_shl(mu_t a, mu_t b);
mu_t str_shr(mu_t a, mu_t b);

// Arithmetic operations
mu_t str_neg(mu_t a);
mu_t str_add(mu_t a, mu_t b);
mu_t str_sub(mu_t a, mu_t b);
mu_t str_mul(mu_t a, mu_t b);
mu_t str_div(mu_t a, mu_t b);
mu_t str_mod(mu_t a, mu_t b);

// Concatenation
mu_t str_concat(mu_t a, mu_t b);

// String iteration
mu_t str_iter(mu_t s);
bool str_next(mu_t s, muint_t *i, mu_t *c);

// String representation
mu_t str_repr(mu_t s);


// Reference counting
mu_inline mu_t str_inc(mu_t m) {
    mu_assert(mu_isstr(m));
    ref_inc(m); return m;
}

mu_inline void str_dec(mu_t m) {
    mu_assert(mu_isstr(m));
    extern void str_destroy(mu_t);
    if (ref_dec(m)) str_destroy(m);
}


#endif
