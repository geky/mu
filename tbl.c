#include "tbl.h"

#include "mem.h"

#include <assert.h>
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


// Functions for managing tables
// Each table is preceeded with a reference count
// which is used as its handle in a var
var_t tbl_create(uint16_t size) {
    return vtbl(tblp_create(size));
}

tbl_t *tblp_create(uint16_t size) {
    tbl_t *tbl = vref_alloc(sizeof(tbl_t));
    int32_t cap = tbl_npw2(tbl_ncap(size));

    tbl->tail = 0;
    tbl->nulls = 0;
    tbl->len = 0;
    tbl->mask = cap - 1;
    tbl->keys = (union tbl_array){0x1};
    tbl->vals = (union tbl_array){0x1};

    return tbl;
}

// Called by garbage collector to clean up
void tbl_destroy(void *m) {
    tbl_t *tbl = m;
    int i;

    for (i=0; i <= tbl->mask; i++) {
        var_t k = tbl_getkey(tbl, i);
        var_t v = tbl_getval(tbl, i);

        if (!var_isnull(k)) {
            var_decref(k);
            var_decref(v);
        }
    }

    if (!tbl->keys.range) vdealloc(tbl->keys.array);
    if (!tbl->vals.range) vdealloc(tbl->vals.array);
}


// Recursively looks up a key in the table
// returns either that value or null
var_t tbl_lookup(var_t v, var_t key) {
    assert(var_istbl(v)); // TODO error on non-tables

    return tblp_lookup(v.tbl, key);
}

var_t tblp_lookup(tbl_t *tbl, var_t key) {
    tbl = tblp_readp(tbl);

    if (var_isnull(key))
        return vnull;

    hash_t i, hash = var_hash(key);

    for (; tbl; tbl = tbl->tail) {
        if (tbl->mask == -1)
            continue;

        for (i = hash;; i = tbl_next(i)) {
            hash_t mi = i & tbl->mask;
            var_t k = tbl_getkey(tbl, mi);

            if (var_isnull(k))
                break;

            if (var_equals(key, k)) {
                var_t v = tbl_getval(tbl, mi);

                if (var_isnull(v))
                    break;

                return v;
            }
        }
    }

    return vnull;
}


// converts implicit range to actual array of nums on heap
var_t *tbl_realizerange(uint16_t off, uint16_t len, int32_t cap) {
    var_t *m = valloc(cap * sizeof(var_t));
    int i;

    for (i=0; i < len; i++) {
        m[i] = vnum(i + off);
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

    if (!tbl->keys.range) {
        keys = valloc(cap * sizeof(var_t));
        memset(keys, 0, cap * sizeof(var_t));
    }

    if (!tbl->vals.range) {
        vals = valloc(cap * sizeof(var_t));
    }


    for (j=0; j <= tbl->mask; j++) {
        var_t k = tbl_getkey(tbl, j);
        var_t v = tbl_getval(tbl, j);

        if (var_isnull(k) || var_isnull(v))
            continue;

        hash_t i = var_hash(k);

        for (;; i = tbl_next(i)) {
            hash_t mi = i & mask;

            if ((keys && var_isnull(keys[mi])) || mi <= len) {
                if (keys) keys[mi] = tbl->keys.array[j];
                if (vals) vals[mi] = tbl->vals.array[j];

                len++;
                break;
            }
        }
    }


    if (keys) {
        vdealloc(tbl->keys.array);
        tbl->keys.array = keys;
    }

    if (vals) {
        vdealloc(tbl->vals.array);
        tbl->vals.array = vals;
    }

    tbl->mask = mask;
    tbl->nulls = 0;
    tbl->len = len;
}


// sets a value in the table with the given key
// without decending down the tail chain
void tbl_assign(var_t v, var_t key, var_t val) {
    assert(var_istbl(v)); // TODO error on non-tables

    tblp_assign(v.tbl, key, val);
}

void tblp_assign(tbl_t *tbl, var_t key, var_t val) {
    tbl = tblp_writep(tbl);

    if (var_isnull(key))
        return;
   
    if (tbl_ncap(tbl->len + tbl->nulls + 1) > tbl->mask + 1)
        tbl_resize(tbl, tbl->len+1);
     
    hash_t i = var_hash(key);

    for (;; i = tbl_next(i)) {
        hash_t mi = i & tbl->mask;
        var_t k = tbl_getkey(tbl, mi);

        if (var_isnull(k)) {
            if (!var_isnull(val)) {
                tbl_setkey(tbl, mi, key);
                tbl_setval(tbl, mi, val);

                assert(tbl->len < tbl_maxlen); // TODO add errors
                tbl->len++;
            }

            return;
        }

        if (var_equals(key, k)) {
            var_t v = tbl_getval(tbl, mi);

            if (var_isnull(v)) {
                tbl_setval(tbl, mi, vnull);
                tbl->nulls--;
                tbl->len++;
            } else {
                var_decref(v);
                tbl_setval(tbl, mi, val);

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
void tbl_set(var_t v, var_t key, var_t val) {
    assert(var_istbl(v)); // TODO error on non-tables

    tblp_set(v.tbl, key, val);
}

void tblp_set(tbl_t *tbl, var_t key, var_t val) {
    if (var_isnull(key))
        return;

    tbl_t *head = tbl;
    hash_t i, hash = var_hash(key);

    for (; tbl; tbl = tbl->tail) {
        if (tblp_isro(tbl))
            break;

        if (tbl->mask == -1)
            continue;

        for (i = hash;; i = tbl_next(i)) {
            hash_t mi = i & tbl->mask;
            var_t k = tbl_getkey(tbl, mi);

            if (var_isnull(k)) 
                break;

            if (var_equals(key, k)) {
                var_t v = tbl_getval(tbl, mi);

                if (var_isnull(v))
                    break;

                var_decref(v);
                tbl_setval(tbl, mi, val);

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


    tbl = tblp_writep(head);

    if (tbl_ncap(tbl->len + tbl->nulls + 1) > tbl->mask + 1)
        tbl_resize(tbl, tbl->len+1);

    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & tbl->mask;
        var_t k = tbl_getkey(tbl, mi);

        if (var_isnull(k)) {
            tbl_setkey(tbl, mi, key);
            tbl_setval(tbl, mi, val);

            assert(tbl->len < tbl_maxlen); // TODO add errors
            tbl->len++;
            return;
        }
    }
}



// Returns a string representation of the table
var_t tbl_repr(var_t v) {
    tbl_t *tbl = tblp_readp(v.tbl);
    unsigned int size = 2;
    int i, j;

    uint8_t *out, *s;

    var_t *key_repr = valloc(tbl->len * sizeof(var_t));
    var_t *val_repr = valloc(tbl->len * sizeof(var_t));

    for (i=0, j=0; i <= tbl->mask; i++) {
        var_t k = tbl_getkey(tbl, i);
        var_t v = tbl_getval(tbl, i);

        if (var_isnull(k) || var_isnull(v))
            continue;

        if (!tbl->keys.range) {
            key_repr[j] = var_repr(k);
            size += key_repr[j].len + 2;
        }

        val_repr[j] = var_repr(v);
        size += val_repr[j].len;

        if (j != tbl->len-1)
            size += 2;

        j++;
    }


    out = vref_alloc(size);
    s = out;

    *s++ = '[';

    for (i=0; i < tbl->len; i++) {
        if (!tbl->keys.range) {
            memcpy(s, var_str(key_repr[i]), key_repr[i].len);
            s += key_repr[i].len;
            var_decref(key_repr[i]);

            *s++ = ':';
            *s++ = ' ';
        }

        memcpy(s, var_str(val_repr[i]), val_repr[i].len);
        s += val_repr[i].len;
        var_decref(val_repr[i]);

        if (i != tbl->len-1) {
            *s++ = ',';
            *s++ = ' ';
        }
    }

    *s++ = ']';

    vdealloc(key_repr);
    vdealloc(val_repr);


    return vstr(out, 0, size);
}
