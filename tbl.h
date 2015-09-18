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
mu_t tbl_fromnum(mu_t n);
mu_t tbl_fromntbl(muint_t n, mu_t (*pairs)[2]);
mu_t tbl_fromztbl(mu_t (*pairs)[2]);
mu_t tbl_fromnlist(muint_t n, mu_t *list);
mu_t tbl_fromzlist(mu_t *list);
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

    mu_t tail;      // tail chain of tables
    mu_t *array;    // pointer to stored data
};


// Table creating functions
mu_inline mu_t mxntbl(mu_t tail, muint_t n, mu_t (*pairs)[2]) {
    mu_t t = tbl_fromntbl(n, pairs);
    tbl_inherit(t, tail);
    return t;
}

mu_inline mu_t mxztbl(mu_t tail, mu_t (*pairs)[2]) {
    mu_t t = tbl_fromztbl(pairs);
    tbl_inherit(t, tail);
    return t;
}

#define mxmtbl(tail, ...) ({                        \
    mu_t _p[][2] = __VA_ARGS__;                     \
    mxntbl(tail, sizeof _p / (2*sizeof(mu_t)), _p); \
})

#define mxctbl(tail, ...) ({                        \
    static mu_t _m = 0;                             \
    if (!_m)                                        \
        _m = tbl_const(mxmtbl(tail, __VA_ARGS__));  \
    _m;                                             \
})

mu_inline mu_t mntbl(muint_t n, mu_t (*pairs)[2]) {
    return tbl_fromntbl(n, pairs);
}

mu_inline mu_t mztbl(mu_t (*pairs)[2]) {
    return tbl_fromztbl(pairs);
}

#define mmtbl(...) ({                           \
    mu_t _p[][2] = __VA_ARGS__;                 \
    mntbl(sizeof _p / (2*sizeof(mu_t)), _p);    \
})

#define mctbl(...) ({                           \
    static mu_t _m = 0;                         \
    if (!_m)                                    \
        _m = tbl_const(mmtbl(__VA_ARGS__));     \
    _m;                                         \
})

mu_inline mu_t mnlist(muint_t n, mu_t *list) {
    return tbl_fromnlist(n, list);
}

mu_inline mu_t mzlist(mu_t *list) {
    return tbl_fromzlist(list);
}

#define mmlist(...) ({                          \
    mu_t _l[] = __VA_ARGS__;                    \
    mnlist(sizeof _l / sizeof(mu_t), _l);       \
})

#define mclist(...) ({                          \
    static mu_t _m = 0;                         \
    if (!_m)                                    \
        _m = tbl_const(mmlist(__VA_ARGS__));    \
    _m;                                         \
})


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


// Conversion to readonly table
mu_inline mu_t tbl_const(mu_t t) {
    return (mu_t)((MU_RTBL^MU_TBL) | (muint_t)t);
}

// Table access functions
mu_inline mlen_t tbl_len(mu_t m) {
    return ((struct tbl *)(~7 & (muint_t)m))->len;
}

mu_inline mu_t tbl_tail(mu_t m) {
    return tbl_inc(((struct tbl *)(~7 & (muint_t)m))->tail);
}


#endif
