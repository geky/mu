/*
 * Table variable type
 */

#ifdef MU_DEF
#ifndef MU_TBL_DEF
#define MU_TBL_DEF
#include "mu.h"


// Hash type definition
typedef uint_t hash_t;

// Definition of Mu's table type
typedef mu_aligned struct tbl tbl_t;


#endif
#else
#ifndef MU_TBL_H
#define MU_TBL_H
#include "types.h"
#include "err.h"


// Each table is composed of an array of values 
// with a stride for keys/values. If keys/values 
// is not stored in the array it is implicitely 
// stored as a range/offset based on the specified 
// offset and length.
struct tbl {
    ref_t ref; // reference count

    len_t len;  // count of non-nil entries
    len_t nils; // count of nil entries
    uintq_t npw2;   // log2 of capacity
    uintq_t stride; // type of table

    tbl_t *tail; // tail chain of tables

    union {
        uint_t offset; // offset for implicit ranges
        mu_t *array;  // pointer to stored data
    };
};


// Table pointer manipulation with the ro flag
mu_inline tbl_t *tbl_ro(tbl_t *t) {
    return (tbl_t *)(MU_RO | (uint_t)t);
}

mu_inline bool tbl_isro(tbl_t *t) {
    return MU_RO & (uint_t)t;
}

mu_inline tbl_t *tbl_read(tbl_t *t) {
    return (tbl_t *)(~MU_RO & (uint_t)t);
}

mu_inline tbl_t *tbl_write(tbl_t *t) {
    if (tbl_isro(t))
        mu_err_readonly();
    else
        return t;
}


// Table creating functions and macros
tbl_t *tbl_create(len_t size);
tbl_t *tbl_extend(len_t size, tbl_t *parent);
void tbl_destroy(tbl_t *t);


mu_inline mu_t mntbl(len_t l) { return mtbl(tbl_create(l)); }

mu_inline len_t tbl_getlen(tbl_t *t) { return tbl_read(t)->len; }

// Table reference counting
mu_inline void tbl_inc(tbl_t *t) { ref_inc((void *)tbl_read(t)); }
mu_inline void tbl_dec(tbl_t *t) { ref_dec((void *)tbl_read(t), 
                                           (void (*)(void *))tbl_destroy); }


// Recursively looks up a key in the table
// returns either that value or nil
mu_t tbl_lookup(tbl_t *t, mu_t key);

// Recursively looks up either a key or index
// if key is not found
mu_t tbl_lookdn(tbl_t *t, mu_t key, hash_t i);

// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
void tbl_assign(tbl_t *t, mu_t key, mu_t val);

// Inserts a value in the table with the given key
// without decending down the tail chain
void tbl_insert(tbl_t *t, mu_t key, mu_t val);

// Performs iteration on a table
fn_t *tbl_iter(tbl_t *t);

// Table representation
str_t *tbl_repr(tbl_t *t);


// Macro for iterating through a table in c
// Assign names for k and v, and pass in the 
// block to execute for each pair in tbl
#define tbl_for_begin(k, v, tbl) {                  \
    mu_t k;                                         \
    mu_t v;                                         \
    tbl_t *_t = tbl_read(tbl);                      \
    uint_t _i, _c = _t->len;                        \
                                                    \
    for (_i=0; _c; _i++) {                          \
        switch (_t->stride) {                       \
            case 0:                                 \
                k = muint(_i);                      \
                v = muint(_t->offset + _i);         \
                break;                              \
            case 1:                                 \
                k = muint(_i);                      \
                v = _t->array[_i];                  \
                break;                              \
            case 2:                                 \
                k = _t->array[2*_i  ];              \
                v = _t->array[2*_i+1];              \
                if (isnil(k) || isnil(v))           \
                    continue;                       \
                break;                              \
        }                                           \
{
#define tbl_for_end                                 \
}                                                   \
        _c--;                                       \
    }                                               \
}


#endif
#endif
