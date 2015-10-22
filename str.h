/*
 *  String Definition
 */

#ifndef MU_STR_H
#define MU_STR_H
#include "mu.h"


// Functions for handling temporary mutable strings
mbyte_t *mstr_create(muint_t len);
mu_t mstr_intern(mbyte_t *s, muint_t len);

void mstr_insert(mbyte_t **s, muint_t *i, mbyte_t c);
void mstr_concat(mbyte_t **s, muint_t *i, mu_t c);
void mstr_ncat(mbyte_t **s, muint_t *i, const mbyte_t *c, muint_t len);
void mstr_zcat(mbyte_t **s, muint_t *i, const char *c);

// Conversion operations
mu_t str_fromnstr(const mbyte_t *s, muint_t len);
mu_t str_fromcstr(const char *s);
mu_t str_frombyte(mbyte_t c);
mu_t str_fromnum(mu_t n);
mu_t str_fromiter(mu_t iter);

// Comparison operation
mint_t str_cmp(mu_t a, mu_t b);

// String operations
mu_t str_concat(mu_t a, mu_t b);
mu_t str_subset(mu_t s, mu_t lower, mu_t upper);

mu_t str_find(mu_t s, mu_t sub);
mu_t str_replace(mu_t s, mu_t sub, mu_t rep, mu_t max);
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
// Each string is stored as a length and array of data.
//
// Strings must be interned before use in tables, and once interned,
// strings cannot be mutated without breaking things.
struct str {
    mref_t ref;     // reference count
    mlen_t len;     // length of string
    mbyte_t data[]; // string data
};


// String creation functions
mu_inline mu_t mnstr(const mbyte_t *s, muint_t len) {
    return str_fromnstr(s, len);
}

mu_inline mu_t mcstr(const char *s) {
    return str_fromcstr(s);
}

// Reference counting
mu_inline mbyte_t *mstr_inc(mbyte_t *s) {
    ref_inc(s - mu_offset(struct str, data));
    return s;
}

mu_inline void mstr_dec(mbyte_t *s) {
    extern void mstr_destroy(mbyte_t *);
    if (ref_dec(s - mu_offset(struct str, data)))
        mstr_destroy(s);
}

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
mu_inline mlen_t str_len(mu_t m) {
    return ((struct str *)((muint_t)m - MTSTR))->len;
}

mu_inline const mbyte_t *str_bytes(mu_t m) {
    return ((struct str *)((muint_t)m - MTSTR))->data;
}

// String constant macro
#ifdef mu_constructor
#define MSTR(name, str)                                         \
static mu_t _mu_ref_##name = 0;                                 \
static const struct {                                           \
    mref_t ref; mlen_t len;                                     \
    mbyte_t data[(sizeof str)-1];                               \
} _mu_val_##name = {0, (sizeof str)-1, str};                    \
                                                                \
mu_constructor void _mu_init_##name(void) {                     \
    _mu_ref_##name = mstr_intern(                               \
        (mbyte_t*)_mu_val_##name.data,                          \
        _mu_val_##name.len);                                    \
}                                                               \
                                                                \
mu_pure mu_t name(void) {                                       \
    return _mu_ref_##name;                                      \
}
#else
#define MSTR(name, str)                                         \
static mu_t _mu_ref_##name = 0;                                 \
static const struct {                                           \
    mref_t ref; mlen_t len;                                     \
    mbyte_t data[(sizeof str)-1];                               \
} _mu_val_##name = {0, (sizeof str)-1, str};                    \
                                                                \
mu_pure mu_t name(void) {                                       \
    if (!_mu_ref_##name) {                                      \
        _mu_ref_##name = mstr_intern(                           \
            (mbyte_t *)_mu_val_##name.data,                     \
            _mu_val_##name.len);                                \
        if (*(mref_t *)((muint_t)_mu_ref_##name - MTSTR))       \
            *(mref_t *)((muint_t)_mu_ref_##name - MTSTR) = 0;   \
    }                                                           \
                                                                \
    return _mu_ref_##name;                                      \
}
#endif


#endif
