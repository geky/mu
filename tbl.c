#include "tbl.h"

#include "mem.h"
#include "str.h"

#include <assert.h>
#include <string.h>

// TODO check lengths appropriately

// finds capactiy based on load factor of 1.5
static inline hash_t tbl_ncap(hash_t s) {
    return s + (s >> 1);
}

// finds next power of two
static inline hash_t tbl_npw2(hash_t s) {
    return s ? 1 << (32-__builtin_clz(s - 1)) : 0;
}

// iterates through hash entries using
// i = i*5 + 1 to avoid degenerative cases
static inline hash_t tbl_next(hash_t i) {
    return (i<<2) + i + 1;
}


// Functions for managing tables
// Each table is preceeded with a reference count
// which is used as its handle in a var
tbl_t *tbl_create(len_t size) {
    tbl_t *tbl = vref_alloc(sizeof(tbl_t));

    tbl->mask = tbl_npw2(tbl_ncap(size)) - 1;

    tbl->tail = 0;
    tbl->nils = 0;
    tbl->len = 0;

    tbl->offset = 0;
    tbl->stride = 0;

    return tbl;
}


// Called by garbage collector to clean up
void tbl_destroy(void *m) {
    tbl_t *tbl = m;

    if (tbl->stride > 0) {
        int i, cap, entries;

        if (tbl->stride < 2) {
            cap = tbl->mask + 1;
            entries = tbl->len;
        } else {
            cap = 2 * (tbl->mask + 1);
            entries = cap;
        }

        for (i=0; i < entries; i++)
            var_dec(tbl->array[i]);

        vdealloc(tbl->array, cap * sizeof(var_t));
    }

    if (tbl->tail)
        tbl_dec(tbl->tail);

    vref_dealloc(m, sizeof(tbl_t));
}


// Recursively looks up a key in the table
// returns either that value or nil
var_t tbl_lookup(tbl_t *tbl, var_t key) {
    if (var_isnil(key))
        return vnil;

    hash_t i, hash = var_hash(key);

    for (tbl = tbl_readp(tbl); tbl; tbl = tbl_readp(tbl->tail)) {
        if (tbl->stride < 2) {
            if (num_ishash(key, hash) && hash < tbl->len) {
                if (tbl->stride == 0)
                    return vnum(hash + tbl->offset);
                else
                    return tbl->array[hash];
            }
        } else {
            for (i = hash;; i = tbl_next(i)) {
                hash_t mi = i & tbl->mask;
                var_t *v = &tbl->array[2*mi];

                if (var_isnil(v[0]))
                    break;

                if (var_equals(key, v[0]) && !var_isnil(v[1]))
                    return v[1];
            }
        }
    }

    return vnil;
}


// converts implicit range to actual array of nums on heap
static void tbl_realizevars(tbl_t *tbl) {
    hash_t cap = tbl->mask + 1;
    var_t *w = valloc(cap * sizeof(var_t));
    int i;

    for (i=0; i < tbl->len; i++) {
        w[i] = vnum(i + tbl->offset);
    }

    tbl->array = w;
    tbl->stride = 1;
}

static void tbl_realizekeys(tbl_t *tbl) {
    hash_t cap = tbl->mask + 1;
    var_t *w = valloc(2*cap * sizeof(var_t));
    int i;

    if (tbl->stride == 0) {
        for (i=0; i < tbl->len; i++) {
            w[2*i  ] = vnum(i);
            w[2*i+1] = vnum(i + tbl->offset);
        }
    } else {
        for (i=0; i < tbl->len; i++) {
            w[2*i  ] = vnum(i);
            w[2*i+1] = tbl->array[i];
        }

        vdealloc(tbl->array, cap * sizeof(var_t));
    }

    memset(w + 2*tbl->len, 0, 2*(cap - tbl->len) * sizeof(var_t));
    tbl->array = w;
    tbl->stride = 2;
}


// reallocates and rehashes a table
static inline void tbl_resize(tbl_t * tbl, len_t size) {
    hash_t cap = tbl_npw2(tbl_ncap(size));
    hash_t mask = cap - 1;

    if (tbl->stride < 2) {
        if (tbl->stride == 0) {
            tbl->mask = mask;
            return;
        }

        var_t *w = valloc(cap * sizeof(var_t));
        memcpy(w, tbl->array, tbl->len * sizeof(var_t));

        vdealloc(tbl->array, (tbl->mask+1) * sizeof(var_t));
        tbl->array = w;
        tbl->mask = mask;
    } else {
        var_t *w = valloc(2*cap * sizeof(var_t));
        memset(w, 0, 2*cap * sizeof(var_t));

        hash_t i, j;

        for (j=0; j <= tbl->mask; j++) {
            var_t *u = &tbl->array[2*j];

            if (var_isnil(u[0]) || var_isnil(u[1]))
                continue;

            for (i = var_hash(u[0]);; i = tbl_next(i)) {
                hash_t mi = i & mask;
                var_t *v = &w[2*mi];

                if (var_isnil(v[0])) {
                    v[0] = u[0];
                    v[1] = u[1];
                    break;
                }
            }
        }

        vdealloc(tbl->array, 2*(tbl->mask+1) * sizeof(var_t));
        tbl->array = w;
        tbl->nils = 0;
        tbl->mask = mask;
    }
}
    

// Inserts a value in the table with the given key
// without decending down the tail chain
static void tbl_insertnil(tbl_t *tbl, var_t key, var_t val) {
    hash_t i, hash = var_hash(key);

    if (tbl->stride < 2) {
        if (!num_ishash(key, hash) || hash >= tbl->len)
            return;

        if (hash == tbl->len - 1) {
            if (tbl->stride != 0)
                var_dec(tbl->array[hash]);

            tbl->len--;
            return;
        }

        tbl_realizekeys(tbl);
    }

    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & tbl->mask;
        var_t *v = &tbl->array[2*mi];

        if (var_isnil(v[0]))
            return;

        if (var_equals(key, v[0])) {
            if (!var_isnil(v[1])) {
                var_dec(v[1]);
                v[1] = vnil;
                tbl->nils++;
                tbl->len--;
            }

            return;
        }
    }
}


static void tbl_insertval(tbl_t *tbl, var_t key, var_t val) {
    hash_t i, hash = var_hash(key);

    if (tbl_ncap(tbl->nils+tbl->len + 1) > tbl->mask + 1)
        tbl_resize(tbl, tbl->len + 1);

    if (tbl->stride < 2) {
        if (num_ishash(key, hash)) {
            if (hash == tbl->len) {
                if (tbl->stride == 0) {
                    if (var_isnum(val)) {
                        if (tbl->len == 0)
                            tbl->offset = num_hash(val);

                        if (num_ishash(val, hash + tbl->offset)) {
                            tbl->len++;
                            return;
                        }
                    }

                    tbl_realizevars(tbl);
                }

                tbl->array[hash] = val;
                tbl->len++;
                return;
            } else if (hash < tbl->len) {
                if (tbl->stride == 0) {
                    if (num_ishash(val, hash + tbl->offset))
                        return;

                    tbl_realizevars(tbl);
                }

                var_dec(tbl->array[hash]);
                tbl->array[hash] = val;
                return;
            }
        }

        tbl_realizekeys(tbl);
    }

    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & tbl->mask;
        var_t *v = &tbl->array[2*mi];

        if (var_isnil(v[0])) {
            v[0] = key;
            v[1] = val;
            tbl->len++;
            return;
        }

        if (var_equals(key, v[0])) {
            if (var_isnil(v[1])) {
                v[1] = val;
                tbl->nils--;
                tbl->len++;
            } else {
                var_dec(v[1]);
                v[1] = val;
            }

            return;
        }
    }
}
    

void tbl_insert(tbl_t *tbl, var_t key, var_t val) {
    tbl = tbl_writep(tbl);

    if (var_isnil(key))
        return;

    if (var_isnil(val))
        tbl_insertnil(tbl, key, val);
    else
        tbl_insertval(tbl, key, val);
}


// Sets the next index in the table with the value
void tbl_add(tbl_t *tbl, var_t val) {
    tbl_insert(tbl, vnum(tbl->len), val);
}


// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
static void tbl_assignnil(tbl_t *tbl, var_t key, var_t val) {
    hash_t i, hash = var_hash(key);

    for (; tbl; tbl = tbl->tail) {
        if (tbl_isro(tbl))
            break;

        if (tbl->stride < 2) {
            if (!num_ishash(key, hash) || hash >= tbl->len)
                continue;

            if (hash == tbl->len - 1) {
                if (tbl->stride != 0)
                    var_dec(tbl->array[hash]);

                tbl->len--;
                return;
            }

            tbl_realizekeys(tbl);
        }

        for (i = hash;; i = tbl_next(i)) {
            hash_t mi = i & tbl->mask;
            var_t *v = &tbl->array[2*mi];

            if (var_isnil(v[0])) 
                break;

            if (var_equals(key, v[0])) {
                if (var_isnil(v[1]))
                    break;

                var_dec(v[1]);
                v[1] = vnil;
                tbl->nils++;
                tbl->len--;
                return;
            }
        }
    }
}


static void tbl_assignval(tbl_t *head, var_t key, var_t val) {
    hash_t i, hash = var_hash(key);
    tbl_t *tbl = head;

    for (; tbl; tbl = tbl->tail) {
        if (tbl_isro(tbl))
            break;

        if (tbl->stride < 2) {
            if (!num_ishash(key, hash) || hash >= tbl->len)
                continue;

            if (tbl->stride == 0) {
                if (num_ishash(val, hash + tbl->offset))
                    return;

                tbl_realizevars(tbl);
            }

            var_dec(tbl->array[hash]);
            tbl->array[hash] = val;
            return;
        }

        for (i = hash;; i = tbl_next(i)) {
            hash_t mi = i & tbl->mask;
            var_t *v = &tbl->array[2*mi];

            if (var_isnil(v[0]))
                break;

            if (var_equals(key, v[0])) {
                if (var_isnil(v[1]))
                    break;

                var_dec(v[1]);
                v[1] = val;
                return;
            }
        }
    }


    tbl = tbl_writep(head);

    if (tbl_ncap(tbl->len+tbl->nils + 1) > tbl->mask + 1)
        tbl_resize(tbl, tbl->len+1);

    if (tbl->stride < 2) {
        if (num_ishash(key, hash) && hash == tbl->len) {
            if (tbl->stride == 0) {
                if (var_isnum(val)) {
                    if (tbl->len == 0)
                        tbl->offset = num_hash(val);

                    if (num_ishash(val, hash + tbl->offset)) {
                        tbl->len++;
                        return;
                    }
                }

                tbl_realizevars(tbl);
            }

            tbl->array[hash] = val;
            tbl->len++;
            return;
        }

        tbl_realizekeys(tbl);
    }

    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & tbl->mask;
        var_t *v = &tbl->array[2*mi];

        if (var_isnil(v[0])) {
            v[0] = key;
            v[1] = val;
            tbl->len++;
            return;
        }

        if (var_equals(key, v[0]) && var_isnil(v[1])) {
            v[1] = val;
            tbl->nils--;
            tbl->len++;
            return;
        }
    }
}


void tbl_assign(tbl_t *tbl, var_t key, var_t val) {
    if (var_isnil(key))
        return;

    if (var_isnil(val))
        tbl_assignnil(tbl, key, val);
    else
        tbl_assignval(tbl, key, val);
}



// Returns a string representation of the table
var_t tbl_repr(var_t v) {
    tbl_t *tbl = tbl_readp(v.tbl);

    var_t *reprs = valloc(2*tbl->len * sizeof(var_t));
    int size = 2;
    int i = 0;

    tbl_for (k, v, tbl, {
        reprs[2*i  ] = var_repr(k);
        reprs[2*i+1] = var_repr(v);
        size += reprs[2*i  ].len;
        size += reprs[2*i+1].len;
        size += (i == tbl->len-1) ? 2 : 4;

        i++;
    })

    assert(size <= VMAXLEN); // TODO error

    str_t *out = str_create(size);
    str_t *res = out;

    *res++ = '[';

    for (i=0; i < tbl->len; i++) {
        memcpy(res, var_str(reprs[2*i]), reprs[2*i].len);
        res += reprs[2*i].len;
        var_dec(reprs[2*i]);

        *res++ = ':';
        *res++ = ' ';

        memcpy(res, var_str(reprs[2*i+1]), reprs[2*i+1].len);
        res += reprs[2*i+1].len;
        var_dec(reprs[2*i+1]);

        if (i != tbl->len-1) {
            *res++ = ',';
            *res++ = ' ';
        }
    }

    *res++ = ']';

    vdealloc(reprs, 2*tbl->len * sizeof(var_t));

    return vstr(out, 0, size);
}
