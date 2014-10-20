#include "tbl.h"

#include "mem.h"
#include "str.h"

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
tbl_t *tbl_create(len_t size, veh_t *eh) {
    tbl_t *tbl = vref_alloc(sizeof(tbl_t), eh);

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

        v_dealloc(tbl->array, cap * sizeof(var_t));
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


// Recursively looks up either a key or index
// if key is not found
var_t tbl_lookdn(tbl_t *tbl, var_t key, len_t i) {
    tbl = tbl_readp(tbl);

    if (!tbl->tail && tbl->stride < 2) {
        if (i < tbl->len) {
            if (tbl->stride == 0)
                return vnum(i + tbl->offset);
            else
                return tbl->array[i];
        }

        return vnil;
    } else {
        var_t val = tbl_lookup(tbl, key);

        if (!var_isnil(val))
            return val;

        return tbl_lookup(tbl, vnum(i));
    }
}


// converts implicit range to actual array of nums on heap
static void tbl_realizevars(tbl_t *tbl, veh_t *eh) {
    hash_t cap = tbl->mask + 1;
    var_t *w = v_alloc(cap * sizeof(var_t), eh);
    int i;

    for (i=0; i < tbl->len; i++) {
        w[i] = vnum(i + tbl->offset);
    }

    tbl->array = w;
    tbl->stride = 1;
}

static void tbl_realizekeys(tbl_t *tbl, veh_t *eh) {
    hash_t cap = tbl->mask + 1;
    var_t *w = v_alloc(2*cap * sizeof(var_t), eh);
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

        v_dealloc(tbl->array, cap * sizeof(var_t));
    }

    memset(w + 2*tbl->len, 0, 2*(cap - tbl->len) * sizeof(var_t));
    tbl->array = w;
    tbl->stride = 2;
}


// reallocates and rehashes a table
static inline void tbl_resize(tbl_t * tbl, len_t size, veh_t *eh) {
    hash_t cap = tbl_npw2(tbl_ncap(size));
    hash_t mask = cap - 1;

    if (tbl->stride < 2) {
        if (tbl->stride == 0) {
            tbl->mask = mask;
            return;
        }

        var_t *w = v_alloc(cap * sizeof(var_t), eh);
        memcpy(w, tbl->array, tbl->len * sizeof(var_t));

        v_dealloc(tbl->array, (tbl->mask+1) * sizeof(var_t));
        tbl->array = w;
        tbl->mask = mask;
    } else {
        var_t *w = v_alloc(2*cap * sizeof(var_t), eh);
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

        v_dealloc(tbl->array, 2*(tbl->mask+1) * sizeof(var_t));
        tbl->array = w;
        tbl->nils = 0;
        tbl->mask = mask;
    }
}
    

// Inserts a value in the table with the given key
// without decending down the tail chain
static void tbl_insertnil(tbl_t *tbl, var_t key, var_t val, veh_t *eh) {
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

        tbl_realizekeys(tbl, eh);
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


static void tbl_insertval(tbl_t *tbl, var_t key, var_t val, veh_t *eh) {
    hash_t i, hash = var_hash(key);

    if (tbl_ncap(tbl->nils+tbl->len + 1) > tbl->mask + 1)
        tbl_resize(tbl, tbl->len + 1, eh);

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

                    tbl_realizevars(tbl, eh);
                }

                tbl->array[hash] = val;
                tbl->len++;
                return;
            } else if (hash < tbl->len) {
                if (tbl->stride == 0) {
                    if (num_ishash(val, hash + tbl->offset))
                        return;

                    tbl_realizevars(tbl, eh);
                }

                var_dec(tbl->array[hash]);
                tbl->array[hash] = val;
                return;
            }
        }

        tbl_realizekeys(tbl, eh);
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
    

void tbl_insert(tbl_t *tbl, var_t key, var_t val, veh_t *eh) {
    tbl = tbl_writep(tbl, eh);

    if (var_isnil(key))
        return;

    if (var_isnil(val))
        tbl_insertnil(tbl, key, val, eh);
    else
        tbl_insertval(tbl, key, val, eh);
}


// Sets the next index in the table with the value
void tbl_add(tbl_t *tbl, var_t val, veh_t *eh) {
    tbl_insert(tbl, vnum(tbl->len), val, eh);
}


// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
static void tbl_assignnil(tbl_t *tbl, var_t key, var_t val, veh_t *eh) {
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

            tbl_realizekeys(tbl, eh);
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


static void tbl_assignval(tbl_t *head, var_t key, var_t val, veh_t *eh) {
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

                tbl_realizevars(tbl, eh);
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


    tbl = tbl_writep(head, eh);

    if (tbl_ncap(tbl->len+tbl->nils + 1) > tbl->mask + 1)
        tbl_resize(tbl, tbl->len+1, eh);

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

                tbl_realizevars(tbl, eh);
            }

            tbl->array[hash] = val;
            tbl->len++;
            return;
        }

        tbl_realizekeys(tbl, eh);
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


void tbl_assign(tbl_t *tbl, var_t key, var_t val, veh_t *eh) {
    if (var_isnil(key))
        return;

    if (var_isnil(val))
        tbl_assignnil(tbl, key, val, eh);
    else
        tbl_assignval(tbl, key, val, eh);
}



// Performs iteration on a table
v_fn var_t tbl_0_iteration(tbl_t *args, tbl_t *scope, veh_t *eh) {
    tbl_t *tbl = tbl_lookup(scope, vnum(0)).tbl;
    tbl_t *ret = tbl_lookup(scope, vnum(1)).tbl;
    int i = tbl_lookup(scope, vnum(2)).data;

    if (i >= tbl->len)
        return vnil;

    tbl_insert(ret, vnum(0), vnum(tbl->offset + i), eh);
    tbl_insert(ret, vnum(1), vnum(i), eh);
    tbl_insert(ret, vnum(2), vnum(i), eh);

    i += 1;
    tbl_insert(scope, vnum(2), vraw(i), eh);

    return vtbl(ret);
}

v_fn var_t tbl_1_iteration(tbl_t *args, tbl_t *scope, veh_t *eh) {
    tbl_t *tbl = tbl_lookup(scope, vnum(0)).tbl;
    tbl_t *ret = tbl_lookup(scope, vnum(1)).tbl;
    int i = tbl_lookup(scope, vnum(2)).data;

    if (i >= tbl->len)
        return vnil;

    tbl_insert(ret, vnum(0), tbl->array[i], eh);
    tbl_insert(ret, vnum(1), vnum(i), eh);
    tbl_insert(ret, vnum(2), vnum(i), eh);

    i += 1;
    tbl_insert(scope, vnum(2), vraw(i), eh);

    return vtbl(ret);
}

v_fn var_t tbl_2_iteration(tbl_t *args, tbl_t *scope, veh_t *eh) {
    tbl_t *tbl = tbl_lookup(scope, vnum(0)).tbl;
    tbl_t *ret = tbl_lookup(scope, vnum(1)).tbl;
    int i = tbl_lookup(scope, vnum(2)).data;
    int j = tbl_lookup(scope, vnum(3)).data;
    var_t k, v;

    if (i >= tbl->len)
        return vnil;

    do {
        k = tbl->array[2*j  ];
        v = tbl->array[2*j+1];

        j += 1;
        tbl_insert(scope, vnum(3), vraw(j), eh);
    } while (var_isnil(k) || var_isnil(v));

    tbl_insert(ret, vnum(0), v, eh);
    tbl_insert(ret, vnum(1), k, eh);
    tbl_insert(ret, vnum(2), vnum(i), eh);

    i += 1;
    tbl_insert(scope, vnum(2), vraw(i), eh);

    return vtbl(ret);
}

var_t tbl_iter(var_t v, veh_t *eh) {
    static sfn_t * const tbl_iters[3] = {
        tbl_0_iteration,
        tbl_1_iteration,
        tbl_2_iteration
    };

    tbl_t *tbl = tbl_readp(v.tbl);
    tbl_t *scope = tbl_create(3, eh);

    v_on_err_begin (eh) {
        tbl_insert(scope, vnum(0), vtbl(tbl), eh);
        tbl_insert(scope, vnum(1), vtbl(tbl_create(3, eh)), eh);
        tbl_insert(scope, vnum(2), vraw(0), eh);
    } v_on_err_do {
        tbl_dec(scope);
        // TODO clean up everything
    } v_on_err_end;

    return vsfn(tbl_iters[tbl->stride], scope);
}


// Returns a string representation of the table
var_t tbl_repr(var_t v, veh_t *eh) {
    tbl_t *tbl = tbl_readp(v.tbl);

    var_t *reprs = v_alloc(2*tbl->len * sizeof(var_t), eh);

    v_on_err_begin (eh) {
        int size = 2;
        int i = 0;

        tbl_for_begin (k, v, tbl) {
            reprs[2*i  ] = var_repr(k, eh);
            reprs[2*i+1] = var_repr(v, eh);
            size += reprs[2*i  ].len;
            size += reprs[2*i+1].len;
            size += (i == tbl->len-1) ? 2 : 4;

            i++;
        } tbl_for_end;

        if (size > VMAXLEN)
            err_len(eh);

        str_t *out = str_create(size, eh);
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

        v_dealloc(reprs, 2*tbl->len * sizeof(var_t));

        return vstr(out, 0, size);
    } v_on_err_do {
        v_dealloc(reprs, 2*tbl->len * sizeof(var_t));
    } v_on_err_end;
}
