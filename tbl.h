/*
 * Table variable type
 */

#ifndef V_TBL
#define V_TBL

#include "var.h"
#include "num.h"

#include <assert.h>

#define tbl_maxlen ((1<<16)-1)


// Table pointers contain a ro flag
// can't simply be dereferenced
union tbl_ptr {
    tbl_t *tbl;
    bool ro : 1;
};

// Each table is composed of two arrays 
// each can hide as a range with an offset
union tbl_array {
    uint32_t bits;
    bool range : 1;

    struct { uint16_t padd; uint16_t off; };
    var_t *array;
};  

struct tbl {
    struct tbl *tail; // tail chain of tables

    uint16_t nulls; // count of null entries
    uint16_t len;   // count of keys in use
    int32_t mask;   // size of entries - 1

    union tbl_array keys; // array of keys
    union tbl_array vals; // array of values
};


// Functions for managing tables
// Each table is preceeded with a reference count
// which is used as its handle in a var
var_t tbl_create(uint16_t size);
tbl_t *tblp_create(uint16_t size);

// Called by garbage collector to clean up
void tbl_destroy(void *);

// Recursively looks up a key in the table
// returns either that value or null
var_t tbl_lookup(var_t, var_t key);
var_t tblp_lookup(tbl_t *, var_t key);

// Sets a value in the table with the given key
// decends down the tail chain until its found
void tbl_set(var_t, var_t key, var_t val);
void tblp_set(tbl_t *, var_t key, var_t val);

// Sets a value in the table with the given key
// without decending down the tail chain
void tbl_assign(var_t, var_t key, var_t val);
void tblp_assign(tbl_t *, var_t key, var_t val);


// Returns a string representation of the table
var_t tbl_repr(var_t v);



// accessing table pointers with the ro flag
static inline bool tblp_isro(tbl_t *tbl) {
    union tbl_ptr tblp = { tbl };
    return tblp.ro;
}

static inline tbl_t *tblp_readp(tbl_t *tbl) {
    union tbl_ptr tblp = { tbl };
    tblp.ro = 0;

    return tblp.tbl;
}

static inline tbl_t *tblp_writep(tbl_t *tbl) {
    union tbl_ptr tblp = { tbl };
    assert(!tblp.ro); // TODO error on const tbl

    return tblp.tbl;
}


// Table array accessing
var_t *tbl_realizerange(uint16_t off, uint16_t len, int32_t cap);

static inline var_t tbl_getarray(tbl_t *tbl, uint32_t i, union tbl_array *a) {
    if (a->range) {
        if (i - a->off < tbl->len + tbl->nulls)
            return vnum(i-a->off);
        else 
            return vnull;
    }

    return a->array[i];
}

static inline void tbl_setarray(tbl_t *tbl, uint32_t i, var_t v, union tbl_array *a) {
    if (a->range) {
        if (v.type == TYPE_NUM && num_equals(v, vnum(i))) {
            if (i - a->off <= tbl->len + tbl->nulls) {
                return;
            } else if (tbl->len + tbl->nulls == 0 && i <= tbl_maxlen) {
                a->off = i;
                return;
            }
        }

        a->array = tbl_realizerange(a->off, tbl->len+tbl->nulls, tbl->mask+1);
    }

    a->array[i] = v;
    var_incref(v);
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
