/*
 * Table variable type
 */

#ifndef MU_TBL_H
#define MU_TBL_H
#include "mu.h"


// Table creation functions
mu_t tbl_create(muint_t size);
mu_t tbl_extend(muint_t size, mu_t tail);
void tbl_inherit(mu_t t, mu_t tail);

// Conversion operations
mu_t tbl_fromntbl(mu_t (*pairs)[2], muint_t n);
mu_t tbl_fromnlist(mu_t *list, muint_t n);
mu_t tbl_fromnum(mu_t n);
mu_t tbl_fromiter(mu_t iter);

// Recursively looks up a key in the table
// returns either that value or nil
mu_t tbl_lookup(mu_t t, mu_t k);

// Inserts a value in the table with the given key
// without decending down the tail chain
void tbl_insert(mu_t t, mu_t k, mu_t v);

// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
void tbl_assign(mu_t t, mu_t k, mu_t v);

// Performs iteration on a table
bool tbl_next(mu_t t, muint_t *i, mu_t *k, mu_t *v);
mu_t tbl_iter(mu_t t);
mu_t tbl_pairs(mu_t t);

// Table representation
mu_t tbl_parse(const mbyte_t **pos, const mbyte_t *end);
mu_t tbl_repr(mu_t t);
mu_t tbl_dump(mu_t t, mu_t depth, mu_t indent);

// Array-like manipulations
void tbl_push(mu_t t, mu_t v, mu_t i);
mu_t tbl_pop(mu_t t, mu_t i);
mu_t tbl_concat(mu_t a, mu_t b, mu_t offset);
mu_t tbl_subset(mu_t t, mu_t lower, mu_t upper);

// Set operations
mu_t tbl_and(mu_t a, mu_t b);
mu_t tbl_or(mu_t a, mu_t b);
mu_t tbl_xor(mu_t a, mu_t b);
mu_t tbl_diff(mu_t a, mu_t b);


// Definition of Mu's table type
//
// Each table is composed of an array of values
// with a stride for keys/values. If keys/values
// is not stored in the array it is implicitely
// stored as a range/offset based on the specified
// offset and length.
struct tbl {
    mref_t ref;      // reference count
    muintq_t npw2;   // log2 of capacity
    muintq_t linear; // type of table
    mlen_t len;      // count of non-nil entries
    mlen_t nils;     // count of nil entries

    mu_t tail;       // tail chain of tables
    mu_t *array;     // pointer to stored data
};


// Table creating functions
mu_inline mu_t mntbl(muint_t n, mu_t (*pairs)[2]) {
    return tbl_fromntbl(pairs, n);
}

#define mtbl(...)                                           \
    mntbl((mu_t[][2])__VA_ARGS__,                           \
        sizeof (mu_t[][2])__VA_ARGS__ / sizeof(mu_t[2]))

mu_inline mu_t mnlist(mu_t *list, muint_t n) {
    return tbl_fromnlist(list, n);
}

#define mlist(...)                                  \
    mnlist((mu_t[])__VA_ARGS__,                     \
        sizeof (mu_t[])__VA_ARGS__ / sizeof(mu_t))

// Table reference counting
mu_inline mu_t tbl_inc(mu_t m) {
    mu_assert(mu_istbl(m));
    ref_inc(m);
    return m;
}

mu_inline void tbl_dec(mu_t m) {
    mu_assert(mu_istbl(m));
    extern void tbl_destroy(mu_t);
    if (ref_dec(m))
        tbl_destroy(m);
}

// Table access functions
mu_inline mlen_t tbl_len(mu_t m) {
    return ((struct tbl *)((muint_t)m - MTTBL))->len;
}

mu_inline mu_t tbl_tail(mu_t m) {
    return ((struct tbl *)((muint_t)m - MTTBL))->tail;
}

// Table constant macros
#ifdef MU_CONSTRUCTOR
#define MLIST(name, ...)                                                \
static mu_t (*const _mu_gen_##name[])(void) = __VA_ARGS__;              \
static struct tbl _mu_struct_##name;                                    \
                                                                        \
mu_constructor void _mu_init_##name(void) {                             \
    extern mu_t tbl_init_list(struct tbl *,                             \
            mu_t (*const *)(void), muint_t);                            \
    tbl_init_list(&_mu_struct_##name, _mu_gen_##name,                   \
            sizeof _mu_gen_##name / sizeof(mu_t));                      \
}                                                                       \
                                                                        \
mu_pure mu_t name(void) {                                               \
    return (mu_t)((muint_t)&_mu_struct_##name + MTTBL)                  \
}
#else
#define MLIST(name, ...)                                                \
static mu_t _mu_ref_##name = 0;                                         \
static mu_t (*const _mu_gen_##name[])(void) = __VA_ARGS__;              \
static struct tbl _mu_struct_##name;                                    \
                                                                        \
mu_pure mu_t name(void) {                                               \
    extern mu_t tbl_init_list(struct tbl *,                             \
            mu_t (*const *)(void), muint_t);                            \
    if (!_mu_ref_##name) {                                              \
        _mu_ref_##name = tbl_init_list(&_mu_struct_##name,              \
               _mu_gen_##name, sizeof _mu_gen_##name / sizeof(mu_t));   \
    }                                                                   \
                                                                        \
    return _mu_ref_##name;                                              \
}
#endif

#ifdef MU_CONSTRUCTOR
#define MTBL(name, ...)                                                     \
static mu_t (*const _mu_gen_##name[][2])(void) = __VA_ARGS__;               \
static struct tbl _mu_struct_##name;                                        \
                                                                            \
mu_constructor void _mu_init_##name(void) {                                 \
    extern mu_t tbl_init_tbl(struct tbl *,                                  \
            mu_t (*const (*)[2])(void), muint_t);                           \
    tbl_init_tbl(&_mu_struct_##name, _mu_gen_##name,                        \
            sizeof _mu_gen_##name / sizeof(mu_t[2]));                       \
}                                                                           \
                                                                            \
mu_pure mu_t name(void) {                                                   \
    return (mu_t)((muint_t)&_mu_struct_##name + MTTBL);                     \
}
#else
#define MTBL(name, ...)                                                     \
static mu_t _mu_ref_##name = 0;                                             \
static mu_t (*const _mu_gen_##name[][2])(void) = __VA_ARGS__;               \
static struct tbl _mu_struct_##name;                                        \
                                                                            \
mu_pure mu_t name(void) {                                                   \
    extern mu_t tbl_init_tbl(struct tbl *,                                  \
            mu_t (*const (*)[2])(void), muint_t);                           \
    if (!_mu_ref_##name) {                                                  \
        _mu_ref_##name = tbl_init_tbl(&_mu_struct_##name,                   \
                _mu_gen_##name, sizeof _mu_gen_##name / sizeof(mu_t[2]));   \
    }                                                                       \
                                                                            \
    return _mu_ref_##name;                                                  \
}
#endif


#endif
