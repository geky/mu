#include "tbl.h"

#include "num.h"
#include "str.h"
#include "fn.h"
#include <string.h>


// Finds capacity based on load factor of 2/3
mu_inline hash_t tbl_ncap(hash_t s) {
    return s + (s >> 1);
}

// Iterates through hash entries using i = i*5 + 1
// This uses the recurrence equation used in Python's dictionary 
// implementation, which allows open hashing with the benefit
// of reducing collisions with very regular sets of data.
mu_inline hash_t tbl_next(hash_t i) {
    return (i << 2) + i + 1;
}

// Finds next power of 2 and checks for minimum bound
mu_inline uintq_t tbl_npw2(hash_t i) {
    if (i == 0)
        return mu_npw2(MU_MINALLOC);
    else
        return mu_npw2(i);
}


// Functions for managing tables
tbl_t *tbl_create(len_t size) {
    tbl_t *t = ref_alloc(sizeof(tbl_t));

    t->npw2 = tbl_npw2(tbl_ncap(size));

    t->len = 0;
    t->nils = 0;
    t->stride = 0;
    t->tail = 0;
    t->offset = 0;

    return t;
}

tbl_t *tbl_extend(len_t size, tbl_t *parent) {
    tbl_t *t = tbl_create(size);

    t->tail = parent;

    return t;
}

void tbl_destroy(tbl_t *t) {
    if (t->stride > 0) {
        uint_t i, cap, len;

        if (t->stride < 2) {
            cap = 1 << t->npw2;
            len = t->len;
        } else {
            cap = 2 * (1 << t->npw2);
            len = cap;
        }

        for (i = 0; i < len; i++) {
            mu_dec(t->array[i]);
        }

        mu_dealloc(t->array, cap * sizeof(mu_t));
    }

    if (t->tail)
        tbl_dec(t->tail);

    ref_dealloc(t, sizeof(tbl_t));
}


// Recursively looks up a key in the table
// returns either that value or nil
mu_t tbl_lookup(tbl_t *t, mu_t key) {
    if (isnil(key))
        return mnil;

    hash_t i, hash = mu_hash(key);

    for (t = tbl_read(t); t; t = tbl_read(t->tail)) {
        if (t->stride < 2) {
            if (ishash(key) && hash < t->len) {
                if (t->stride == 0)
                    return muint(hash + t->offset);
                else
                    return t->array[hash];
            }
        } else {
            for (i = hash;; i = tbl_next(i)) {
                hash_t mi = i & ((1 << t->npw2) - 1);
                mu_t *v = &t->array[2*mi];

                if (isnil(v[0]))
                    break;

                if (mu_equals(key, v[0]) && !isnil(v[1]))
                    return v[1];
            }
        }
    }

    return mnil;
}


// Recursively looks up either a key or index
// if key is not found
mu_t tbl_lookdn(tbl_t *tbl, mu_t key, hash_t i) {
    tbl = tbl_read(tbl);

    if (!tbl->tail && tbl->stride < 2) {
        if (i < tbl->len) {
            if (tbl->stride == 0)
                return muint(i + tbl->offset);
            else
                return tbl->array[i];
        }

        return mnil;
    } else {
        mu_t val = tbl_lookup(tbl, key);

        if (!isnil(val))
            return val;

        return tbl_lookup(tbl, muint(i));
    }
}


// converts implicit range to actual array of nums on heap
static void tbl_realizevals(tbl_t *t) {
    hash_t cap = 1 << t->npw2;
    mu_t *array = mu_alloc(cap * sizeof(mu_t));
    uint_t i;

    for (i = 0; i < t->len; i++) {
        array[i] = muint(i + t->offset);
    }

    t->array = array;
    t->stride = 1;
}

// converts either a range or list to a full hash table
static void tbl_realizekeys(tbl_t *t) {
    hash_t cap = 1 << t->npw2;
    mu_t *array = mu_alloc(2*cap * sizeof(mu_t));
    uint_t i;

    if (t->stride == 0) {
        for (i = 0; i < t->len; i++) {
            array[2*i  ] = muint(i);
            array[2*i+1] = muint(i + t->offset);
        }
    } else {
        for (i = 0; i < t->len; i++) {
            array[2*i  ] = muint(i);
            array[2*i+1] = t->array[i];
        }

        mu_dealloc(t->array, cap * sizeof(mu_t));
    }

    memset(&array[2*t->len], 0, 2*(cap - t->len) * sizeof(mu_t));
    t->array = array;
    t->stride = 2;
}


// reallocates and rehashes a table
mu_inline void tbl_resize(tbl_t *t, len_t len) {
    data_t npw2 = tbl_npw2(tbl_ncap(len));
    hash_t cap = 1 << npw2;
    hash_t mask = cap - 1;

    if (t->stride < 2) {
        if (t->stride == 0) {
            t->npw2 = npw2;
            return;
        }

        mu_t *array = mu_alloc(cap * sizeof(mu_t));
        memcpy(array, t->array, t->len * sizeof(mu_t));

        mu_dealloc(t->array, (1 << t->npw2) * sizeof(mu_t));
        t->array = array;
        t->npw2 = npw2;
    } else {
        mu_t *array = mu_alloc(2*cap * sizeof(mu_t));
        memset(array, 0, 2*cap * sizeof(mu_t));
        hash_t i, j;

        for (j = 0; j < (1 << t->npw2); j++) {
            mu_t *u = &t->array[2*j];

            if (isnil(u[0]) || isnil(u[1]))
                continue;

            for (i = mu_hash(u[0]);; i = tbl_next(i)) {
                hash_t mi = i & mask;
                mu_t *v = &array[2*mi];

                if (isnil(v[0])) {
                    v[0] = u[0];
                    v[1] = u[1];
                    break;
                }
            }
        }

        mu_dealloc(t->array, 2*(1 << t->npw2) * sizeof(mu_t));
        t->array = array;
        t->nils = 0;
        t->npw2 = npw2;
    }
}


// Inserts a value in the table with the given key
// without decending down the tail chain
static void tbl_insertnil(tbl_t *t, mu_t key, mu_t val) {
    hash_t i, hash = mu_hash(key);

    if (t->stride < 2) {
        if (!ishash(key) || hash >= t->len)
            return;

        if (hash == t->len - 1) {
            if (t->stride != 0)
                mu_dec(t->array[hash]);

            t->len--;
            return;
        }

        tbl_realizekeys(t);
    }

    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & ((1 << t->npw2) - 1);
        mu_t *v = &t->array[2*mi];

        if (isnil(v[0]))
            return;

        if (mu_equals(key, v[0])) {
            if (!isnil(v[1])) {
                mu_dec(v[1]);
                v[1] = mnil;
                t->nils++;
                t->len--;
            }

            return;
        }
    }
}


static void tbl_insertval(tbl_t *t, mu_t key, mu_t val) {
    hash_t i, hash = mu_hash(key);

    if (tbl_ncap(t->nils+t->len + 1) > (1 << t->npw2))
        tbl_resize(t, t->len+1); // TODO check len

    if (t->stride < 2) {
        if (ishash(key)) {
            if (hash == t->len) {
                if (t->stride == 0) {
                    if (ishash(val)) {
                        if (t->len == 0)
                            t->offset = getuint(val);

                        if (getuint(val) == hash + t->offset) {
                            t->len++;
                            return;
                        }
                    }

                    tbl_realizevals(t);
                }

                t->array[hash] = val;
                t->len++;
                return;
            } else if (hash < t->len) {
                if (t->stride == 0) {
                    if (ishash(val) && getuint(val) == hash + t->offset)
                        return;

                    tbl_realizevals(t);
                }

                mu_dec(t->array[hash]);
                t->array[hash] = val;
                return;
            }
        }

        tbl_realizekeys(t);
    }

    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & ((1 << t->npw2) - 1);
        mu_t *v = &t->array[2*mi];

        if (isnil(v[0])) {
            v[0] = key;
            v[1] = val;
            t->len++;
            return;
        }

        if (mu_equals(key, v[0])) {
            if (isnil(v[1])) {
                v[1] = val;
                t->nils--;
                t->len++;
            } else {
                mu_dec(v[1]);
                v[1] = val;
            }

            return;
        }
    }
}
    

void tbl_insert(tbl_t *t, mu_t key, mu_t val) {
    t = tbl_write(t);

    if (isnil(key))
        return;

    if (isnil(val))
        tbl_insertnil(t, key, val);
    else
        tbl_insertval(t, key, val);
}


// Sets the next index in the table with the value
void tbl_append(tbl_t *t, mu_t val) {
    tbl_insert(t, muint(t->len), val);
}


// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
static void tbl_assignnil(tbl_t *t, mu_t key, mu_t val) {
    hash_t i, hash = mu_hash(key);

    for (; t && !tbl_isro(t); t = t->tail) {
        if (t->stride < 2) {
            if (!ishash(key) || hash >= t->len)
                continue;

            if (hash == t->len - 1) {
                if (t->stride != 0)
                    mu_dec(t->array[hash]);

                t->len--;
                return;
            }

            tbl_realizekeys(t);
        }

        for (i = hash;; i = tbl_next(i)) {
            hash_t mi = i & ((1 << t->npw2) - 1);
            mu_t *v = &t->array[2*mi];

            if (isnil(v[0])) 
                break;

            if (mu_equals(key, v[0])) {
                if (isnil(v[1]))
                    break;

                mu_dec(v[1]);
                v[1] = mnil;
                t->nils++;
                t->len--;
                return;
            }
        }
    }
}


static void tbl_assignval(tbl_t *t, mu_t key, mu_t val) {
    tbl_t *head = t;
    hash_t i, hash = mu_hash(key);

    for (; t && tbl_isro(t); t = t->tail) {
        if (t->stride < 2) {
            if (!ishash(key) || hash >= t->len)
                continue;

            if (t->stride == 0) {
                if (ishash(val) && getuint(val) == hash + t->offset)
                    return;

                tbl_realizevals(t);
            }

            mu_dec(t->array[hash]);
            t->array[hash] = val;
            return;
        }

        for (i = hash;; i = tbl_next(i)) {
            hash_t mi = i & ((1 << t->npw2) - 1);
            mu_t *v = &t->array[2*mi];

            if (isnil(v[0]))
                break;

            if (mu_equals(key, v[0])) {
                if (isnil(v[1]))
                    break;

                mu_dec(v[1]);
                v[1] = val;
                return;
            }
        }
    }


    t = tbl_write(head);

    if (tbl_ncap(t->len+t->nils + 1) > (1 << t->npw2))
        tbl_resize(t, t->len+1); // TODO check size

    if (t->stride < 2) {
        if (ishash(key) && hash == t->len) {
            if (t->stride == 0) {
                if (ishash(val)) {
                    if (t->len == 0)
                        t->offset = getuint(val);

                    if (getuint(val) == hash + t->offset) {
                        t->len++;
                        return;
                    }
                }

                tbl_realizevals(t);
            }

            t->array[hash] = val;
            t->len++;
            return;
        }

        tbl_realizekeys(t);
    }

    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & ((1 << t->npw2) - 1);
        mu_t *v = &t->array[2*mi];

        if (isnil(v[0])) {
            v[0] = key;
            v[1] = val;
            t->len++;
            return;
        }

        if (mu_equals(key, v[0]) && isnil(v[1])) {
            v[1] = val;
            t->nils--;
            t->len++;
            return;
        }
    }
}


void tbl_assign(tbl_t *t, mu_t key, mu_t val) {
    if (isnil(key))
        return;

    if (isnil(val))
        tbl_assignnil(t, key, val);
    else
        tbl_assignval(t, key, val);
}



// Performs iteration on a table
c_t tbl_0_iteration(tbl_t *scope, mu_t *frame) {
    tbl_t *tbl = gettbl(tbl_lookup(scope, muint(0)));
    uint_t i = getuint(tbl_lookup(scope, muint(1)));

    if (i >= tbl->len)
        return 0;

    tbl_insert(scope, muint(1), muint(i+1));

    frame[0] = muint(i);
    frame[1] = muint(tbl->offset + i);
    return 2;
}

c_t tbl_1_iteration(tbl_t *scope, mu_t *frame) {
    tbl_t *tbl = gettbl(tbl_lookup(scope, muint(0)));
    uint_t i = getuint(tbl_lookup(scope, muint(1)));

    if (i >= tbl->len)
        return 0;

    tbl_insert(scope, muint(1), muint(i+1));

    frame[0] = muint(i);
    frame[1] = mu_inc(tbl->array[i]);
    return 2;
}

c_t tbl_2_iteration(tbl_t *scope, mu_t *frame) {
    tbl_t *tbl = gettbl(tbl_lookup(scope, muint(0)));
    uint_t i = getuint(tbl_lookup(scope, muint(1)));
    mu_t k, v;

    if (i >= tbl->len)
        return 0;

    do {
        k = tbl->array[2*i  ];
        v = tbl->array[2*i+1];
        i += 1;
    } while (isnil(k) || isnil(v));

    tbl_insert(scope, muint(1), muint(i));

    frame[0] = mu_inc(k);
    frame[1] = mu_inc(v);
    return 2;
}

fn_t *tbl_iter(tbl_t *tbl) {
    static sbfn_t *const tbl_iters[3] = {
        tbl_0_iteration,
        tbl_1_iteration,
        tbl_2_iteration
    };

    tbl = tbl_read(tbl);
    tbl_t *scope = tbl_create(2);
    tbl_insert(scope, muint(0), mtbl(tbl));
    tbl_insert(scope, muint(1), muint(0));

    return fn_sbfn(0, tbl_iters[tbl->stride], scope);
}


// Returns a string representation of the table
str_t *tbl_repr(tbl_t *t) {
    str_t **reprs = mu_alloc(2*tbl_getlen(t) * sizeof(str_t *));

    uint_t size = 2;
    uint_t i = 0;

    tbl_for_begin (k, v, t) {
        reprs[2*i  ] = mu_repr(k);
        reprs[2*i+1] = mu_repr(v);
        size += str_getlen(reprs[2*i  ]);
        size += str_getlen(reprs[2*i+1]);
        size += (i == tbl_getlen(t)-1) ? 2 : 4;

        i++;
    } tbl_for_end;

    if (size > MU_MAXLEN)
        mu_err_len();

    mstr_t *m = mstr_create(size);
    data_t *out = m->data;

    *out++ = '[';

    for (i=0; i < tbl_getlen(t); i++) {
        memcpy(out, str_getdata(reprs[2*i]), str_getlen(reprs[2*i]));
        out += str_getlen(reprs[2*i]);
        str_dec(reprs[2*i]);

        *out++ = ':';
        *out++ = ' ';

        memcpy(out, str_getdata(reprs[2*i+1]), str_getlen(reprs[2*i+1]));
        out += str_getlen(reprs[2*i+1]);
        str_dec(reprs[2*i+1]);

        if (i != tbl_getlen(t)-1) {
            *out++ = ',';
            *out++ = ' ';
        }
    }

    *out++ = ']';

    mu_dealloc(reprs, 2*tbl_getlen(t) * sizeof(mu_t));

    return str_intern(m, m->len);
}
