#include "tbl.h"

#include "var.h"
#include "num.h"
#include "str.h"
#include "mem.h"

#include <string.h>


// TODO check lengths appropriately

// finds capactiy based on load factor of 1.5
mu_inline hash_t tbl_ncap(hash_t s) {
    return s + (s >> 1);
}

// Iterates through hash entries using i = i*5 + 1
// This uses the recurrence equation used in Python's dictionary 
// implementation, which allows open hashing with the benifit
// of reducing collisions with very regular sets of data.
mu_inline hash_t tbl_next(hash_t i) {
    return (i<<2) + i + 1;
}


// Functions for managing tables
// Each table is preceeded with a reference count
// which is used as its handle in a var
tbl_t *tbl_create(len_t size, eh_t *eh) {
    tbl_t *tbl = ref_alloc(sizeof(tbl_t), eh);

    tbl->mask = mu_npw2(tbl_ncap(size)) - 1;

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

        mu_dealloc(tbl->array, cap * sizeof(var_t));
    }

    if (tbl->tail)
        tbl_dec(tbl->tail);

    ref_dealloc(m, sizeof(tbl_t));
}


// Recursively looks up a key in the table
// returns either that value or nil
var_t tbl_lookup(tbl_t *tbl, var_t key) {
    if (isnil(key))
        return vnil;

    hash_t i, hash = var_hash(key);

    for (tbl = tbl_read(tbl); tbl; tbl = tbl_read(tbl->tail)) {
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

                if (isnil(v[0]))
                    break;

                if (var_equals(key, v[0]) && !isnil(v[1]))
                    return v[1];
            }
        }
    }

    return vnil;
}


// Recursively looks up either a key or index
// if key is not found
var_t tbl_lookdn(tbl_t *tbl, var_t key, len_t i) {
    tbl = tbl_read(tbl);

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

        if (!isnil(val))
            return val;

        return tbl_lookup(tbl, vnum(i));
    }
}


// converts implicit range to actual array of nums on heap
static void tbl_realizevars(tbl_t *tbl, eh_t *eh) {
    hash_t cap = tbl->mask + 1;
    var_t *w = mu_alloc(cap * sizeof(var_t), eh);
    int i;

    for (i=0; i < tbl->len; i++) {
        w[i] = vnum(i + tbl->offset);
    }

    tbl->array = w;
    tbl->stride = 1;
}

static void tbl_realizekeys(tbl_t *tbl, eh_t *eh) {
    hash_t cap = tbl->mask + 1;
    var_t *w = mu_alloc(2*cap * sizeof(var_t), eh);
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

        mu_dealloc(tbl->array, cap * sizeof(var_t));
    }

    memset(w + 2*tbl->len, 0, 2*(cap - tbl->len) * sizeof(var_t));
    tbl->array = w;
    tbl->stride = 2;
}


// reallocates and rehashes a table
mu_inline void tbl_resize(tbl_t * tbl, len_t size, eh_t *eh) {
    hash_t cap = mu_npw2(tbl_ncap(size));
    hash_t mask = cap - 1;

    if (tbl->stride < 2) {
        if (tbl->stride == 0) {
            tbl->mask = mask;
            return;
        }

        var_t *w = mu_alloc(cap * sizeof(var_t), eh);
        memcpy(w, tbl->array, tbl->len * sizeof(var_t));

        mu_dealloc(tbl->array, (tbl->mask+1) * sizeof(var_t));
        tbl->array = w;
        tbl->mask = mask;
    } else {
        var_t *w = mu_alloc(2*cap * sizeof(var_t), eh);
        memset(w, 0, 2*cap * sizeof(var_t));

        hash_t i, j;

        for (j=0; j <= tbl->mask; j++) {
            var_t *u = &tbl->array[2*j];

            if (isnil(u[0]) || isnil(u[1]))
                continue;

            for (i = var_hash(u[0]);; i = tbl_next(i)) {
                hash_t mi = i & mask;
                var_t *v = &w[2*mi];

                if (isnil(v[0])) {
                    v[0] = u[0];
                    v[1] = u[1];
                    break;
                }
            }
        }

        mu_dealloc(tbl->array, 2*(tbl->mask+1) * sizeof(var_t));
        tbl->array = w;
        tbl->nils = 0;
        tbl->mask = mask;
    }
}
    

// Inserts a value in the table with the given key
// without decending down the tail chain
static void tbl_insertnil(tbl_t *tbl, var_t key, var_t val, eh_t *eh) {
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

        if (isnil(v[0]))
            return;

        if (var_equals(key, v[0])) {
            if (!isnil(v[1])) {
                var_dec(v[1]);
                v[1] = vnil;
                tbl->nils++;
                tbl->len--;
            }

            return;
        }
    }
}


static void tbl_insertval(tbl_t *tbl, var_t key, var_t val, eh_t *eh) {
    hash_t i, hash = var_hash(key);

    if (tbl_ncap(tbl->nils+tbl->len + 1) > tbl->mask + 1)
        tbl_resize(tbl, tbl->len + 1, eh);

    if (tbl->stride < 2) {
        if (num_ishash(key, hash)) {
            if (hash == tbl->len) {
                if (tbl->stride == 0) {
                    if (isnum(val)) {
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

        if (isnil(v[0])) {
            v[0] = key;
            v[1] = val;
            tbl->len++;
            return;
        }

        if (var_equals(key, v[0])) {
            if (isnil(v[1])) {
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
    

void tbl_insert(tbl_t *tbl, var_t key, var_t val, eh_t *eh) {
    tbl = tbl_write(tbl, eh);

    if (isnil(key))
        return;

    if (isnil(val))
        tbl_insertnil(tbl, key, val, eh);
    else
        tbl_insertval(tbl, key, val, eh);
}


// Sets the next index in the table with the value
void tbl_append(tbl_t *tbl, var_t val, eh_t *eh) {
    tbl_insert(tbl, vnum(tbl->len), val, eh);
}


// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
static void tbl_assignnil(tbl_t *tbl, var_t key, var_t val, eh_t *eh) {
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

            if (isnil(v[0])) 
                break;

            if (var_equals(key, v[0])) {
                if (isnil(v[1]))
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


static void tbl_assignval(tbl_t *head, var_t key, var_t val, eh_t *eh) {
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

            if (isnil(v[0]))
                break;

            if (var_equals(key, v[0])) {
                if (isnil(v[1]))
                    break;

                var_dec(v[1]);
                v[1] = val;
                return;
            }
        }
    }


    tbl = tbl_write(head, eh);

    if (tbl_ncap(tbl->len+tbl->nils + 1) > tbl->mask + 1)
        tbl_resize(tbl, tbl->len+1, eh);

    if (tbl->stride < 2) {
        if (num_ishash(key, hash) && hash == tbl->len) {
            if (tbl->stride == 0) {
                if (isnum(val)) {
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

        if (isnil(v[0])) {
            v[0] = key;
            v[1] = val;
            tbl->len++;
            return;
        }

        if (var_equals(key, v[0]) && isnil(v[1])) {
            v[1] = val;
            tbl->nils--;
            tbl->len++;
            return;
        }
    }
}


void tbl_assign(tbl_t *tbl, var_t key, var_t val, eh_t *eh) {
    if (isnil(key))
        return;

    if (isnil(val))
        tbl_assignnil(tbl, key, val, eh);
    else
        tbl_assignval(tbl, key, val, eh);
}



// Performs iteration on a table
mu_fn var_t tbl_0_iteration(tbl_t *args, tbl_t *scope, eh_t *eh) {
    tbl_t *tbl = gettbl(tbl_lookup(scope, vnum(0)));
    tbl_t *ret = gettbl(tbl_lookup(scope, vnum(1)));
    int i = getraw(tbl_lookup(scope, vnum(2)));

    if (i >= tbl->len)
        return vnil;

    tbl_insert(ret, vnum(0), vnum(tbl->offset + i), eh);
    tbl_insert(ret, vnum(1), vnum(i), eh);
    tbl_insert(ret, vnum(2), vnum(i), eh);

    i += 1;
    tbl_insert(scope, vnum(2), vraw(i), eh);

    return vtbl(ret);
}

mu_fn var_t tbl_1_iteration(tbl_t *args, tbl_t *scope, eh_t *eh) {
    tbl_t *tbl = gettbl(tbl_lookup(scope, vnum(0)));
    tbl_t *ret = gettbl(tbl_lookup(scope, vnum(1)));
    int i = getraw(tbl_lookup(scope, vnum(2)));

    if (i >= tbl->len)
        return vnil;

    tbl_insert(ret, vnum(0), tbl->array[i], eh);
    tbl_insert(ret, vnum(1), vnum(i), eh);
    tbl_insert(ret, vnum(2), vnum(i), eh);

    i += 1;
    tbl_insert(scope, vnum(2), vraw(i), eh);

    return vtbl(ret);
}

mu_fn var_t tbl_2_iteration(tbl_t *args, tbl_t *scope, eh_t *eh) {
    tbl_t *tbl = gettbl(tbl_lookup(scope, vnum(0)));
    tbl_t *ret = gettbl(tbl_lookup(scope, vnum(1)));
    int i = getraw(tbl_lookup(scope, vnum(2)));
    int j = getraw(tbl_lookup(scope, vnum(3)));
    var_t k, v;

    if (i >= tbl->len)
        return vnil;

    do {
        k = tbl->array[2*j  ];
        v = tbl->array[2*j+1];

        j += 1;
    } while (isnil(k) || isnil(v));

    tbl_insert(ret, vnum(0), v, eh);
    tbl_insert(ret, vnum(1), k, eh);
    tbl_insert(ret, vnum(2), vnum(i), eh);

    i += 1;
    tbl_insert(scope, vnum(2), vraw(i), eh);
    tbl_insert(scope, vnum(3), vraw(j), eh);

    return vtbl(ret);
}

var_t tbl_iter(var_t v, eh_t *eh) {
    static sfn_t * const tbl_iters[3] = {
        tbl_0_iteration,
        tbl_1_iteration,
        tbl_2_iteration
    };

    tbl_t *tbl = tbl_read(gettbl(v));
    tbl_t *scope = tbl_create(3, eh);
    tbl_insert(scope, vnum(0), vtbl(tbl), eh);
    tbl_insert(scope, vnum(1), vtbl(tbl_create(3, eh)), eh);
    tbl_insert(scope, vnum(2), vraw(0), eh);

    return vsfn(tbl_iters[tbl->stride], scope);
}


// Returns a string representation of the table
var_t tbl_repr(var_t v, eh_t *eh) {
    tbl_t *tbl = gettbl(v);
    var_t *reprs = mu_alloc(2*tbl_len(tbl) * sizeof(var_t), eh);

    int size = 2;
    int i = 0;

    tbl_for_begin (k, v, tbl) {
        reprs[2*i  ] = var_repr(k, eh);
        reprs[2*i+1] = var_repr(v, eh);
        size += getlen(reprs[2*i  ]);
        size += getlen(reprs[2*i+1]);
        size += (i == tbl_len(tbl)-1) ? 2 : 4;

        i++;
    } tbl_for_end;

    if (size > MU_MAXLEN)
        err_len(eh);

    mstr_t *out = str_create(size, eh);
    mstr_t *res = out;

    *res++ = '[';

    for (i=0; i < tbl_len(tbl); i++) {
        memcpy(res, getstr(reprs[2*i]), getlen(reprs[2*i]));
        res += getlen(reprs[2*i]);
        var_dec(reprs[2*i]);

        *res++ = ':';
        *res++ = ' ';

        memcpy(res, getstr(reprs[2*i+1]), getlen(reprs[2*i+1]));
        res += getlen(reprs[2*i+1]);
        var_dec(reprs[2*i+1]);

        if (i != tbl_len(tbl)-1) {
            *res++ = ',';
            *res++ = ' ';
        }
    }

    *res++ = ']';

    mu_dealloc(reprs, 2*tbl_len(tbl) * sizeof(var_t));

    return vstr(out, 0, size);
}
