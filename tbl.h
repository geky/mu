/*
 * Table data structure
 */

#ifndef MU_TBL_H
#define MU_TBL_H
#include "mu.h"


// Table creation functions
mu_t tbl_create(muint_t size);
mu_t tbl_extend(muint_t size, mu_t tail);

// Conversion operations
mu_t tbl_fromlist(mu_t *list, muint_t n);
mu_t tbl_frompairs(mu_t (*pairs)[2], muint_t n);
mu_t tbl_fromiter(mu_t iter);

// Changing the tail of the table
void tbl_settail(mu_t t, mu_t tail);

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
mu_t tbl_dump(mu_t t, mu_t depth);

// Array-like manipulations
mu_t tbl_concat(mu_t a, mu_t b, mu_t offset);
mu_t tbl_subset(mu_t t, mint_t lower, mint_t upper);

void tbl_push(mu_t t, mu_t v, mint_t i);
mu_t tbl_pop(mu_t t, mint_t i);

// Set operations
mu_t tbl_and(mu_t a, mu_t b);
mu_t tbl_or(mu_t a, mu_t b);
mu_t tbl_xor(mu_t a, mu_t b);
mu_t tbl_diff(mu_t a, mu_t b);

// Table flags
enum tbl_flags {
    TBL_LINEAR = 1 << 0, // stored as linear array
};

// Definition of Mu's table type
//
// Each table is composed of an array of values
// with a stride for keys/values. If keys/values
// is not stored in the array it is implicitely
// stored as a range/offset based on the specified
// offset and length.
struct tbl {
    mref_t ref;     // reference count
    muintq_t npw2;  // log2 of capacity
    uint8_t flags;  // table flags
    mlen_t len;     // count of non-nil entries
    mlen_t nils;    // count of nil entries

    mu_t tail;      // tail chain of tables
    mu_t *array;    // pointer to stored data
};


// Table reference counting
mu_inline mu_t tbl_inc(mu_t m) {
    mu_assert(mu_istbl(m));
    ref_inc(m);
    return m;
}

mu_inline void tbl_dec(mu_t m) {
    mu_assert(mu_istbl(m));
    extern void tbl_destroy(mu_t);
    if (ref_dec(m)) {
        tbl_destroy(m);
    }
}

// Table access functions
mu_inline mlen_t tbl_getlen(mu_t m) {
    return ((struct tbl *)((muint_t)m - MTTBL))->len;
}

mu_inline mu_t tbl_gettail(mu_t m) {
    return ((struct tbl *)((muint_t)m - MTTBL))->tail;
}


// Table constant macros
#define MU_GEN_LIST(name, ...)                                              \
mu_pure mu_t name(void) {                                                   \
    static mu_t ref = 0;                                                    \
    static mu_t (*const gen[])(void) = __VA_ARGS__;                         \
    static struct tbl inst;                                                 \
                                                                            \
    extern mu_t tbl_initlist(struct tbl *, mu_t (*const *)(void), muint_t); \
    if (!ref) {                                                             \
        ref = tbl_initlist(&inst, gen, sizeof gen / sizeof(gen[0]));        \
    }                                                                       \
                                                                            \
    return ref;                                                             \
}

#define MU_GEN_TBL(name, ...)                                               \
mu_pure mu_t name(void) {                                                   \
    static mu_t ref = 0;                                                    \
    static mu_t (*const gen[][2])(void) = __VA_ARGS__;                      \
    static struct tbl inst;                                                 \
                                                                            \
    extern mu_t tbl_initpairs(                                              \
            struct tbl *, mu_t (*const (*)[2])(void), muint_t);             \
    ref = tbl_initpairs(&inst, gen, sizeof gen / sizeof(gen[0]));           \
    if (!ref) {                                                             \
    }                                                                       \
                                                                            \
    return ref;                                                             \
}


#endif
