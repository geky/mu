#include "tbl.h"

#include "mem.h"

#include <string.h>


// finds capactiy based on load factor of 1.5
static inline int32_t tbl_ncap(int32_t s) {
    return s + (s >> 1);
}

// finds next power of two
static inline int32_t tbl_npw2(int32_t s) {
    if (s)
        return 1 << (32-__builtin_clz(s - 1));
    else
        return 0;
}

// iterates through hash entries using
// i = i*5 + 1 to avoid degenerative cases
static inline hash_t tbl_next(hash_t i) {
    return (i<<2) + i + 1;
}


static var_t *tbl_realize_range(uint16_t len, int32_t cap);

// looks up a var in a table array
static inline var_t tbl_at(tbl_t *tbl, var_t *a, uint32_t i) {
    if (a)
        return a[i];
    else if (i < tbl->len + tbl->nulls)
        return vnum(i);
    else
        return vnull;
}   

// places a var into a table array
static inline void tbl_put(tbl_t *tbl, var_t **a, uint32_t i, var_t v) {
    if (!*a) {
        if (var_equals(v, vnum(i)) && i <= tbl->len + tbl->nulls)
            return;
        else
            *a = tbl_realize_range(tbl->len + tbl->nulls, tbl->mask+1);
    }

    (*a)[i] = v;
    var_incref(v);
}


// Functions for managing tables
// Each table is preceeded with a reference count
// which is used as its handle in a var
tbl_t *tbl_create(void) {
    tbl_t *tbl = vref_alloc(sizeof(tbl_t));

    tbl->tail = 0;
    tbl->nulls = 0;
    tbl->len = 0;
    tbl->mask = -1;
    tbl->keys = 0;
    tbl->vals = 0;

    return tbl;
}

void tbl_destroy(tbl_t *tbl) {
    int i;

    for (i=0; i <= tbl->mask; i++) {
        var_t k = tbl_at(tbl, tbl->keys, i);
        var_t v = tbl_at(tbl, tbl->vals, i);

        if (!var_isnull(k)) {
            var_decref(k);
            var_decref(v);
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
    tbl->len = 0;

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
    tbl->len = 0;

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

        for (i = hash;; i = tbl_next(i)) {
            hash_t mi = i & tbl->mask;
            var_t k = tbl_at(tbl, tbl->keys, mi);

            if (var_isnull(k))
                break;

            if (var_equals(key, k)) {
                var_t v = tbl_at(tbl, tbl->vals, mi);

                if (var_isnull(v))
                    break;

                return v;
            }
        }
    }

    return vnull;
}

// converts implicit range to actual array of nums on heap
static var_t *tbl_realize_range(uint16_t len, int32_t cap) {
    var_t *m = valloc(cap * sizeof(var_t));
    int i;

    for (i=0; i < len; i++) {
        m[i] = vnum(i);
    }

    memset(m+len, 0, (cap-len) * sizeof(var_t));

    return m;
}   

// reallocates and rehashes a table, ignoring any ranges
static void tbl_resize(tbl_t *tbl, uint16_t size) {
    int32_t cap = tbl_npw2(tbl_ncap(size));
    int32_t mask = cap - 1;
    uint16_t len = 0;
    int j;

    var_t *keys = 0;
    var_t *vals = 0;

    if (tbl->keys) {
        keys = valloc(cap * sizeof(var_t));
        memset(keys, 0, cap * sizeof(var_t));
    }

    if (tbl->vals) {
        vals = valloc(cap * sizeof(var_t));
    }


    for (j=0; j <= tbl->mask; j++) {
        var_t k = tbl_at(tbl, tbl->keys, j);
        var_t v = tbl_at(tbl, tbl->vals, j);

        if (var_isnull(k) || var_isnull(v))
            continue;

        hash_t i = var_hash(k);

        for (;; i = tbl_next(i)) {
            hash_t mi = i & mask;

            if ((keys && var_isnull(keys[mi])) || mi <= len) {
                if (keys) keys[mi] = tbl->keys[j];
                if (vals) vals[mi] = tbl->vals[j];

                len++;
                break;
            }
        }
    }

    vdealloc(tbl->keys);
    vdealloc(tbl->vals);

    tbl->mask = mask;
    tbl->nulls = 0;
    tbl->len = len;
    tbl->keys = keys;
    tbl->vals = vals;
}


// sets a value in the table with the given key
// without decending down the tail chain
void tbl_assign(tbl_t *tbl, var_t key, var_t val) {
    if (var_isnull(key))
        return;
   
    if (tbl_ncap(tbl->len + tbl->nulls + 1) > tbl->mask + 1)
        tbl_resize(tbl, tbl->len+1);
     
    hash_t i = var_hash(val);

    for (;; i = tbl_next(i)) {
        hash_t mi = i & tbl->mask;
        var_t k = tbl_at(tbl, tbl->keys, mi);

        if (var_isnull(k)) {
            if (!var_isnull(val)) {
                tbl_put(tbl, &tbl->keys, mi, key);
                tbl_put(tbl, &tbl->vals, mi, val);
                tbl->len++;
            }

            return;
        }

        if (var_equals(key, k)) {
            var_t v = tbl_at(tbl, tbl->vals, mi);

            if (var_isnull(v)) {
                tbl_put(tbl, &tbl->vals, mi, vnull);
                tbl->nulls--;
                tbl->len++;
            } else {
                var_decref(v);
                tbl_put(tbl, &tbl->vals, mi, val);

                if (var_isnull(val)) {
                    tbl->nulls++;
                    tbl->len--;
                }
            }

            return;
        }
    }
}

// sets a value in the table with the given key
// decends down the tail chain until its found
void tbl_set(tbl_t *tbl, var_t key, var_t val) {
    if (var_isnull(key))
        return;

    tbl_t *head = tbl;
    hash_t i, hash = var_hash(key);

    for (; tbl; tbl = tbl->tail) {
        if (tbl->mask == -1)
            continue;

        for (i = hash;; i = tbl_next(i)) {
            hash_t mi = i & tbl->mask;
            var_t k = tbl_at(tbl, tbl->keys, mi);

            if (var_isnull(k)) 
                break;

            if (var_equals(key, k)) {
                var_t v = tbl_at(tbl, tbl->vals, mi);

                if (var_isnull(v))
                    break;

                var_decref(v);
                tbl_put(tbl, &tbl->vals, mi, val);

                if (var_isnull(val)) {
                    tbl->nulls++;
                    tbl->len--;
                }

                return;
            }
        }
    }

    if (var_isnull(val))
        return;


    tbl = head;

    if (tbl_ncap(tbl->len + tbl->nulls + 1) > tbl->mask + 1)
        tbl_resize(tbl, tbl->len+1);

    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & tbl->mask;
        var_t k = tbl_at(tbl, tbl->keys, mi);

        if (var_isnull(k)) {
            tbl_put(tbl, &tbl->keys, mi, key);
            tbl_put(tbl, &tbl->vals, mi, val);
            tbl->len++;
            return;
        }
    }
}
