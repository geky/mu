#include "tbl.h"

#include "mem.h"

#include <string.h>


// finds next capactiy based on load factor of 1.5
static inline int32_t tbl_ncap(int32_t s) {
    return s + (s >> 1);
}

// finds nearest power of two
static inline int32_t tbl_npw2(int32_t s) {
    if (!s)
        return 0;
    else
        return 1 << (32-__builtin_clz(s - 1));
}


// Functions for managing tables
// Each table is preceeded with a reference count
// which is used as its handle in a var
tbl_t *tbl_create(void) {
    tbl_t *tbl = vref_alloc(sizeof(tbl_t));

    tbl->tail = 0;
    tbl->nulls = 0;
    tbl->count = 0;
    tbl->mask = 0;
    tbl->keys = 0;
    tbl->vals = 0;

    return tbl;
}

void tbl_destroy(tbl_t *tbl) {
    int i;

    for (i=0; i <= tbl->mask; i++) {
        if (!var_isnull(tbl->keys[i])) {
            var_decref(tbl->keys[i]);
            var_decref(tbl->vals[i]);
        }
    }

    vdealloc(tbl->keys);
    vdealloc(tbl->vals);
}


// Creates preallocated table or array
tbl_t *tbl_alloc_array(uint16_t size) {
    tbl_t *tbl = vref_alloc(sizeof(tbl_t));
    int32_t cap = tbl_npw2(tbl_ncap(size));

    tbl->mask = cap - 1;
    tbl->keys = 0;
    tbl->vals = valloc(cap * sizeof(var_t));

    tbl->tail = 0;
    tbl->nulls = 0;
    tbl->count = 0;

    return tbl;
}

tbl_t *tbl_alloc_table(uint16_t size) {
    tbl_t *tbl = vref_alloc(sizeof(tbl_t));
    int32_t cap = tbl_npw2(tbl_ncap(size));

    tbl->mask = cap - 1;
    tbl->keys = valloc(cap * sizeof(var_t));
    tbl->vals = valloc(cap * sizeof(var_t));

    memset(tbl->keys, 0, cap * sizeof(var_t));

    tbl->tail = 0;
    tbl->nulls = 0;
    tbl->count = 0;

    return tbl;
}


// Recursively looks up a key in the table
// returns either that value or null
var_t tbl_lookup(tbl_t *tbl, var_t key) {
    if (var_isnull(key))
        return vnull;

    hash_t i, hash = var_hash(key);

    for (; tbl; tbl = tbl->tail) {
        if (tbl->mask == -1)
            continue;

        for (i = hash;; i = (i<<2) + i + 1) {
            var_t *k = &tbl->keys[i & tbl->mask];
            var_t *v = &tbl->vals[i & tbl->mask];

            if (var_isnull(*k))
                break;

            if (!var_equals(key, *k))
                continue;

            if (var_isnull(*v))
                break;

            return *v;
        }
    }

    return vnull;
}


// resizes a table
static void tbl_resize(tbl_t *tbl, uint16_t size) {
    int32_t cap = tbl_npw2(tbl_ncap(size));
    int32_t mask = cap - 1;
    uint16_t count = 0;
    int j;

    var_t *keys = valloc(cap * sizeof(var_t));
    var_t *vals = valloc(cap * sizeof(var_t));

    memset(keys, 0, cap * sizeof(var_t));

    for (j=0; j <= tbl->mask; j++) {
        if (var_isnull(tbl->keys[j]) || var_isnull(tbl->vals[j]))
            continue;

        hash_t i = var_hash(tbl->keys[j]);

        for (;; i = (i<<2) + i + 1) {
            var_t *k = &tbl->keys[i & tbl->mask];
            var_t *v = &tbl->vals[i & tbl->mask];

            if (var_isnull(*k)) {
                keys[i] = *k;
                vals[i] = *v;
                count++;
                break;
            }
        }
    }

    vdealloc(tbl->keys);
    vdealloc(tbl->vals);

    tbl->mask = mask;
    tbl->nulls = 0;
    tbl->count = count;
    tbl->keys = keys;
    tbl->vals = vals;
}


// sets a value in the table with the given key
// decends down the tail chain until its found
void tbl_set(tbl_t *tbl, var_t key, var_t val) {
    if (var_isnull(key))
        return;

    hash_t i, hash = var_hash(key);

    for (; tbl; tbl = tbl->tail) {
        if (tbl->mask == -1)
            continue;

        for (i = hash;; i = (i<<2) + i + 1) {
            var_t *k = &tbl->keys[i & tbl->mask];
            var_t *v = &tbl->vals[i & tbl->mask];

            if (var_isnull(*k))
                break;

            if (!var_equals(key, *k))
                continue;

            if (var_isnull(*v))
                break;

            var_decref(*v);
            *v = val;

            if (var_isnull(val))
                tbl->nulls++;
            else
                var_incref(val);            

            return;
        }
    }

    tbl_assign(tbl, key, val);
}


// sets a value in the table with the given key
// without decending down the tail chain
void tbl_assign(tbl_t *tbl, var_t key, var_t val) {
    if (var_isnull(key))
        return;

    tbl->count++;

    if (tbl_ncap(tbl->count+1) > tbl->mask)
        tbl_resize(tbl, tbl->count+1 - tbl->nulls);


    hash_t i = var_hash(key);

    for (;; i = (i<<2) + i + 1) {
        var_t *k = &tbl->keys[i & tbl->mask];
        var_t *v = &tbl->vals[i & tbl->mask];

        if (var_isnull(*k)) {
            if (!var_isnull(val)) {
                *k = key;
                *v = val;
                var_incref(key);
                var_incref(val);
                tbl->count++;
            }

            return;
        }

        if (var_equals(key, *k)) {
            if (var_isnull(*v)) {
                *v = val;

                if (!var_isnull(val)) {
                    tbl->nulls--;
                    var_incref(val);
                }
            } else {
                var_decref(*v);
                *v = val;

                if (var_isnull(val))
                    tbl->nulls++;
                else
                    var_incref(val);
            }

            return;
        }
    }
}
        

