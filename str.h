/*
 *  String Definition
 */

#ifndef MU_STR_H
#define MU_STR_H
#include "mu.h"
#include "buf.h"


// String creation functions
mu_t str_create(const mbyte_t *s, muint_t n);
mu_t str_intern(mu_t buf, muint_t n);

// Conversion operations
mu_t str_frombyte(mbyte_t c);
mu_t str_fromnum(mu_t n);
mu_t str_fromiter(mu_t iter);

// Formatting
mu_t str_vformat(const char *f, va_list args);
mu_t str_format(const char *f, ...);

// Comparison operation
mint_t str_cmp(mu_t a, mu_t b);

// String operations
mu_t str_concat(mu_t a, mu_t b);
mu_t str_subset(mu_t s, mu_t lower, mu_t upper);

mu_t str_find(mu_t s, mu_t sub);
mu_t str_replace(mu_t s, mu_t sub, mu_t rep);
mu_t str_split(mu_t s, mu_t delim);
mu_t str_join(mu_t iter, mu_t delim);
mu_t str_pad(mu_t s, mu_t len, mu_t pad);
mu_t str_strip(mu_t s, mu_t dir, mu_t pad);

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
    // data follows
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
    if (ref_dec(m))
        str_destroy(m);
}

// String access functions
// we don't define a string struct 
mu_inline mlen_t str_len(mu_t m) {
    return ((struct str *)((muint_t)m - MTSTR))->len;
}

mu_inline const mbyte_t *str_bytes(mu_t m) {
    return (mbyte_t *)((struct str *)((muint_t)m - MTSTR) + 1);
}

// String constant macro
#ifdef MU_CONSTRUCTOR
#define MSTR(name, s)                                       \
static mu_t _mu_ref_##name = 0;                             \
static const struct {                                       \
    struct str str;                                         \
    mbyte_t data[(sizeof s)-1];                             \
} _mu_struct_##name = {{0, (sizeof s)-1}, s};               \
                                                            \
mu_constructor void _mu_init_##name(void) {                 \
    extern mu_t str_init(const struct str *);               \
    _mu_ref_##name = str_init(                              \
            (const struct str *)&_mu_struct_##name);        \
}                                                           \
                                                            \
mu_pure mu_t name(void) {                                   \
    return _mu_ref_##name;                                  \
}
#else
#define MSTR(name, s)                                       \
static mu_t _mu_ref_##name = 0;                             \
static const struct {                                       \
    struct str str;                                         \
    mbyte_t data[(sizeof s)-1];                             \
} _mu_val_##name = {{0, (sizeof s)-1}, s};                  \
                                                            \
mu_pure mu_t name(void) {                                   \
    extern mu_t str_init(const struct str *);               \
    if (!_mu_ref_##name) {                                  \
        _mu_ref_##name = str_init(                          \
                (const struct str *)&_mu_struct_##name);    \
    }                                                       \
                                                            \
    return _mu_ref_##name;                                  \
}
#endif


#endif
