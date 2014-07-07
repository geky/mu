/*
 * Table variable type
 */

#ifndef V_TBL
#define V_TBL

#include "var.h"
#include "num.h"

#include <assert.h>

#define tbl_maxlen ((1<<16)-1)


// Each table is composed of two arrays 
// each can hide as a range with an offset
union tblarr {
    uint32_t bits;
    bool range : 1;

    struct { uint16_t padd; uint16_t off; };
    var_t *array;
};  

struct tbl {
    struct tbl *tail; // tail chain of tables

    uint16_t nils; // count of nil entries
    uint16_t len;  // count of keys in use
    int32_t mask;  // size of entries - 1

    union tblarr keys; // array of keys
    union tblarr vals; // array of values
};


// Functions for managing tables
// Each table is preceeded with a reference count
// which is used as its handle in a var
tbl_t *tbl_create(uint16_t size);

// Called by garbage collector to clean up
void tbl_destroy(void *);


// Recursively looks up a key in the table
// returns either that value or nil
var_t tbl_lookup(tbl_t *, var_t key);

// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
void tbl_assign(tbl_t *, var_t key, var_t val);

// Inserts a value in the table with the given key
// without decending down the tail chain
void tbl_insert(tbl_t *, var_t key, var_t val);

// Sets the next index in the table with the value
void tbl_add(tbl_t *, var_t val);


// Returns a string representation of the table
var_t tbl_repr(var_t v);



// Macro for iterating through a table in c
// Assign names for k and v, and pass in the 
// block to execute for each pair in tbl
#define tbl_for(k, v, tbl, block) {             \
    var_t k;                                    \
    var_t v;                                    \
    tbl_t *_t = tbl;                            \
    int _i;                                     \
                                                \
    for (_i=0; _i <= _t->mask; _i++) {          \
        k = tbl_getkey(_t, _i);                 \
        v = tbl_getval(_t, _i);                 \
                                                \
        if (!var_isnil(k) && !var_isnil(v)) {   \
            block                               \
        }                                       \
    }                                           \
}



// Accessing table pointers with the ro flag
static inline bool tbl_isro(tbl_t *tbl) {
    return 0x1 & (uint32_t)tbl;
}

static inline tbl_t *tbl_ro(tbl_t *tbl) {
    return (tbl_t *)(0x1 | (uint32_t)tbl);
}

static inline tbl_t *tbl_readp(tbl_t *tbl) {
    uint32_t bits = (uint32_t)tbl;
    bits &= ~0x1;
    return (tbl_t *)bits;
}

static inline tbl_t *tbl_writep(tbl_t *tbl) {
    assert(!tbl_isro(tbl)); // TODO error on const tbl
    return tbl;
}


// Table reference counting
static inline void tbl_inc(void *m) { vref_inc(m); }
static inline void tbl_dec(void *m) { vref_dec(m, tbl_destroy); }


// Table array accessing
var_t *tbl_realizerange(uint16_t off, uint16_t len, int32_t cap);

static inline var_t tbl_getarray(tbl_t *tbl, uint32_t i, union tblarr *a) {
    if (a->range) {
        if (i - a->off < tbl->len + tbl->nils)
            return vnum(i-a->off);
        else 
            return vnil;
    }

    return a->array[i];
}

static inline void tbl_setarray(tbl_t *tbl, uint32_t i, var_t v, union tblarr *a) {
    if (a->range) {
        if (v.type == TYPE_NUM && num_equals(v, vnum(i))) {
            if (i - a->off <= tbl->len + tbl->nils) {
                return;
            } else if (tbl->len + tbl->nils == 0 && i <= tbl_maxlen) {
                a->off = i;
                return;
            }
        }

        a->array = tbl_realizerange(a->off, tbl->len+tbl->nils, tbl->mask+1);
    }

    a->array[i] = v;
}

static inline var_t tbl_getkey(tbl_t *tbl, uint32_t i) {
    return tbl_getarray(tbl, i, &tbl->keys);
}

static inline var_t tbl_getval(tbl_t *tbl, uint32_t i) {
    return tbl_getarray(tbl, i, &tbl->vals);
}

static inline void tbl_setkey(tbl_t *tbl, uint32_t i, var_t v) {
    tbl_setarray(tbl, i, v, &tbl->keys);
}

static inline void tbl_setval(tbl_t *tbl, uint32_t i, var_t v) {
    tbl_setarray(tbl, i, v, &tbl->vals);
}


#endif
