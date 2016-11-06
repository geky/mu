/*
 * String type
 */

#ifndef MU_STR_H
#define MU_STR_H
#include "mu.h"
#include "buf.h"


// String creation functions
mu_t mu_str_intern(mu_t buf, muint_t n);

// Conversion operations
mu_t mu_str_fromdata(const mbyte_t *s, muint_t n);
mu_t mu_str_fromiter(mu_t iter);

// Formatting
mu_t mu_str_vformat(const char *f, va_list args);
mu_t mu_str_format(const char *f, ...);

// Comparison operation
mint_t mu_str_cmp(mu_t a, mu_t b);

// String operations
mu_t mu_str_concat(mu_t a, mu_t b);
mu_t mu_str_subset(mu_t s, mint_t lower, mint_t upper);

// String iteration
bool mu_str_next(mu_t s, muint_t *i, mu_t *c);
mu_t mu_str_iter(mu_t s);

// String representation
mu_t mu_str_parse(const mbyte_t **pos, const mbyte_t *end);
mu_t mu_str_repr(mu_t s);

mu_t mu_str_bin(mu_t s);
mu_t mu_str_oct(mu_t s);
mu_t mu_str_hex(mu_t s);


// Definition of Mu's string types
// Storage follows identical layout of buf type.
// Strings must be interned before use in tables, and once interned,
// strings cannot be mutated without breaking things.
struct mstr {
    mref_t ref;     // reference count
    mlen_t len;     // length of string
    mbyte_t data[]; // data follows
};


// Reference counting
mu_inline mu_t mu_str_inc(mu_t m) {
    mu_assert(mu_isstr(m));
    mu_ref_inc(m);
    return m;
}

mu_inline void mu_str_dec(mu_t m) {
    mu_assert(mu_isstr(m));
    extern void mu_str_destroy(mu_t);
    if (mu_ref_dec(m)) {
        mu_str_destroy(m);
    }
}

// String access functions
// we don't define a string struct 
mu_inline mlen_t mu_str_getlen(mu_t m) {
    return ((struct mstr *)((muint_t)m - MTSTR))->len;
}

mu_inline const mbyte_t *mu_str_getdata(mu_t m) {
    return ((struct mstr *)((muint_t)m - MTSTR))->data;
}


// String constant macro
#define MU_GEN_STR(name, s)                                                 \
mu_pure mu_t name(void) {                                                   \
    static mu_t ref = 0;                                                    \
    static const struct {                                                   \
        mref_t ref;                                                         \
        mlen_t len;                                                         \
        mbyte_t data[sizeof s > 1 ? (sizeof s)-1 : 1];                      \
    } inst = {0, (sizeof s)-1, s};                                          \
                                                                            \
    extern mu_t mu_str_init(const struct mstr *);                            \
    if (!ref) {                                                             \
        ref = mu_str_init((const struct mstr *)&inst);                       \
    }                                                                       \
                                                                            \
    return ref;                                                             \
}


#endif
