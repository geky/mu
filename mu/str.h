/*
 * String type
 */

#ifndef MU_STR_H
#define MU_STR_H
#include "config.h"
#include "types.h"
#include "buf.h"


// Definition of Mu's string types
// Storage follows identical layout of buf type.
// Strings must be interned before use in tables, and once interned,
// strings cannot be mutated without breaking things.
struct mstr {
    mref_t ref;     // reference count
    mlen_t len;     // length of string
    mbyte_t data[]; // data follows
};


// String creation functions
mu_t mu_str_intern(mu_t buf, muint_t n);

// Conversion operations
mu_t mu_str_fromdata(const void *s, muint_t n);
mu_inline mu_t mu_str_fromcstr(const char *s);
mu_inline mu_t mu_str_fromchr(char s);
mu_t mu_str_frommu(mu_t m);

// String access functions
mu_inline mlen_t mu_str_getlen(mu_t m);
mu_inline const void *mu_str_getdata(mu_t m);

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
mu_t mu_str_parsen(const mbyte_t **pos, const mbyte_t *end);
mu_t mu_str_parse(const char *s, muint_t n);
mu_t mu_str_repr(mu_t s);


// String creation stuff
mu_inline mu_t mu_str_fromcstr(const char *s) {
    return mu_str_fromdata(s, strlen(s));
}

mu_inline mu_t mu_str_fromchr(char s) {
    return mu_str_fromdata(&s, 1);
}

// String access functions
// we don't define a string struct 
mu_inline mlen_t mu_str_getlen(mu_t m) {
    return ((struct mstr *)((muint_t)m - MTSTR))->len;
}

mu_inline const void *mu_str_getdata(mu_t m) {
    return ((struct mstr *)((muint_t)m - MTSTR))->data;
}


// String constant macro
#define MU_DEF_STR(name, s)                                                 \
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
