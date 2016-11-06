/*
 * String type
 */

#ifndef MU_STR_H
#define MU_STR_H
#include "mu.h"
#include "buf.h"


// String creation functions
mu_t str_create(const mbyte_t *s, muint_t n);
mu_t str_intern(mu_t buf, muint_t n);

// Conversion operations
mu_t str_fromnum(mu_t n);
mu_t str_fromiter(mu_t iter);

// Formatting
mu_t str_vformat(const char *f, va_list args);
mu_t str_format(const char *f, ...);

// Comparison operation
mint_t str_cmp(mu_t a, mu_t b);

// String operations
mu_t str_concat(mu_t a, mu_t b);
mu_t str_subset(mu_t s, mint_t lower, mint_t upper);

// String iteration
bool str_next(mu_t s, muint_t *i, mu_t *c);
mu_t str_iter(mu_t s);

// String representation
mu_t str_parse(const mbyte_t **pos, const mbyte_t *end);
mu_t str_repr(mu_t s);

mu_t str_bin(mu_t s);
mu_t str_oct(mu_t s);
mu_t str_hex(mu_t s);


// Definition of Mu's string types
// Storage follows identical layout of buf type.
// Strings must be interned before use in tables, and once interned,
// strings cannot be mutated without breaking things.
struct str {
    mref_t ref;     // reference count
    mlen_t len;     // length of string
    mbyte_t data[]; // data follows
};


// String creation functions
#define mstr(...) str_format(__VA_ARGS__)


// Reference counting
mu_inline mu_t str_inc(mu_t m) {
    mu_assert(mu_isstr(m));
    ref_inc(m);
    return m;
}

mu_inline void str_dec(mu_t m) {
    mu_assert(mu_isstr(m));
    extern void str_destroy(mu_t);
    if (ref_dec(m)) {
        str_destroy(m);
    }
}

// String access functions
// we don't define a string struct 
mu_inline mlen_t str_len(mu_t m) {
    return ((struct str *)((muint_t)m - MTSTR))->len;
}

mu_inline const mbyte_t *str_data(mu_t m) {
    return ((struct str *)((muint_t)m - MTSTR))->data;
}


// String constant macro
#define MSTR(name, s)                                                       \
mu_pure mu_t name(void) {                                                   \
    static mu_t ref = 0;                                                    \
    static const struct {                                                   \
        mref_t ref;                                                         \
        mlen_t len;                                                         \
        mbyte_t data[sizeof s > 1 ? (sizeof s)-1 : 1];                      \
    } inst = {0, (sizeof s)-1, s};                                          \
                                                                            \
    extern mu_t str_init(const struct str *);                               \
    if (!ref) {                                                             \
        ref = str_init((const struct str *)&inst);                          \
    }                                                                       \
                                                                            \
    return ref;                                                             \
}


#endif
