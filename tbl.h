/*
 * Table variable type
 */

#ifndef MU_TBL_H
#define MU_TBL_H
#include "mu.h"
#include "types.h"


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

// Table access functions
mu_inline mlen_t tbl_len(mu_t m) {
    return ((struct tbl *)(~7 & (muint_t)m))->len;
}

// Conversion to readonly table
mu_inline mu_t tbl_ro(mu_t m) {
    return (mu_t)((MU_RTBL^MU_TBL) | (muint_t)m);
}

mu_inline bool tbl_isro(mu_t m) {
    return (MU_RTBL^MU_TBL) & (muint_t)m;
}

// Table creating functions
mu_t mntbl(muint_t n, mu_t (*pairs)[2]);

#define mtbl(...) ({                            \
    mu_t _p[][2] = __VA_ARGS__;                 \
    mntbl(sizeof _p / (2*sizeof(mu_t)), _p);    \
})

#define mctbl(...) ({                           \
    static mu_t _m = 0;                         \
                                                \
    if (!_m)                                    \
        _m = tbl_ro(mtbl(__VA_ARGS__));         \
                                                \
    _m;                                         \
})


// Table creating functions and macros
mu_t tbl_create(muint_t size);
mu_t tbl_extend(muint_t size, mu_t parent);

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
mu_t tbl_iter(mu_t t);
bool tbl_next(mu_t t, muint_t *i, mu_t *k, mu_t *v);

// Table representation
mu_t tbl_repr(mu_t t);

// Array-like manipulations
mu_t tbl_concat(mu_t a, mu_t b, mu_t offset);
mu_t tbl_pop(mu_t a, mu_t i);
void tbl_push(mu_t a, mu_t v, mu_t i);


// Table reference counting
mu_inline mu_t tbl_inc(mu_t m) {
    mu_assert(mu_istbl(m));
    ref_inc(m);
    return m;
}

mu_inline void tbl_dec(mu_t m) {
    mu_assert(mu_istbl(m));
    extern void tbl_destroy(mu_t);
    if (ref_dec(m)) tbl_destroy(m);
}


#endif
