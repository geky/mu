#include "tbl.h"

#include "num.h"
#include "str.h"
#include "fn.h"
#include "err.h"
#include <string.h>


// Internally used conversion between mu_t and struct tbl
mu_inline mu_t mtbl(struct tbl *t) {
    return (mu_t)((uint_t)t + MU_TBL);
}

mu_inline struct tbl *tbl_rtbl(mu_t m) {
    return (struct tbl *)(~7 & (uint_t)m);
}

mu_inline struct tbl *tbl_wtbl(mu_t m) {
    if (mu_unlikely(tbl_isro(m)))
        mu_err_readonly();
    else
        return tbl_rtbl(m);
}


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
mu_t tbl_create(len_t size) {
    struct tbl *t = ref_alloc(sizeof(struct tbl));

    t->npw2 = tbl_npw2(tbl_ncap(size));

    t->len = 0;
    t->nils = 0;
    t->stride = 0;
    t->tail = 0;
    t->offset = 0;

    return mtbl(t);
}

mu_t tbl_extend(len_t size, mu_t parent) {
    mu_t t = tbl_create(size);
    tbl_rtbl(t)->tail = parent;

    return t;
}

void tbl_destroy(mu_t m) {
    struct tbl *t = tbl_rtbl(m);

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

    ref_dealloc(t, sizeof(struct tbl));
}


// Recursively looks up a key in the table
// returns either that value or nil
mu_t tbl_lookup(mu_t m, mu_t k) {
    if (!k)
        return mnil;

    hash_t i, hash = mu_hash(k);

    for (struct tbl *t = tbl_rtbl(m); t; t = tbl_rtbl(t->tail)) {
        if (t->stride < 2) {
            if (mu_ishash(k) && hash < t->len) {
                if (t->stride == 0)
                    return muint(hash + t->offset);
                else
                    return t->array[hash];
            }
        } else {
            for (i = hash;; i = tbl_next(i)) {
                hash_t mi = i & ((1 << t->npw2) - 1);
                mu_t *v = &t->array[2*mi];

                if (!v[0])
                    break;

                if (mu_equals(k, v[0]) && v[1])
                    return v[1];
            }
        }
    }

    return mnil;
}

// converts implicit range to actual array of nums on heap
static void tbl_realizevals(struct tbl *t) {
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
static void tbl_realizekeys(struct tbl *t) {
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
mu_inline void tbl_resize(struct tbl *t, len_t len) {
    byte_t npw2 = tbl_npw2(tbl_ncap(len));
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

            if (!u[0] || !u[1])
                continue;

            for (i = mu_hash(u[0]);; i = tbl_next(i)) {
                hash_t mi = i & mask;
                mu_t *v = &array[2*mi];

                if (!v[0]) {
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
static void tbl_insertnil(struct tbl *t, mu_t k, mu_t v) {
    hash_t i, hash = mu_hash(k);

    if (t->stride < 2) {
        if (!mu_ishash(k) || hash >= t->len)
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
        mu_t *p = &t->array[2*mi];

        if (!p[0])
            return;

        if (mu_equals(k, p[0])) {
            if (p[1]) {
                mu_dec(p[1]);
                p[1] = mnil;
                t->nils++;
                t->len--;
            }

            return;
        }
    }
}


static void tbl_insertval(struct tbl *t, mu_t k, mu_t v) {
    hash_t i, hash = mu_hash(k);

    if (tbl_ncap(t->nils+t->len + 1) > (1 << t->npw2))
        tbl_resize(t, t->len+1); // TODO check len

    if (t->stride < 2) {
        if (mu_ishash(k)) {
            if (hash == t->len) {
                if (t->stride == 0) {
                    if (mu_ishash(v)) {
                        if (t->len == 0)
                            t->offset = num_uint(v);

                        if (num_uint(v) == hash + t->offset) {
                            t->len++;
                            return;
                        }
                    }

                    tbl_realizevals(t);
                }

                t->array[hash] = v;
                t->len++;
                return;
            } else if (hash < t->len) {
                if (t->stride == 0) {
                    if (mu_ishash(v) && num_uint(v) == hash + t->offset)
                        return;

                    tbl_realizevals(t);
                }

                mu_dec(t->array[hash]);
                t->array[hash] = v;
                return;
            }
        }

        tbl_realizekeys(t);
    }

    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & ((1 << t->npw2) - 1);
        mu_t *p = &t->array[2*mi];

        if (!p[0]) {
            p[0] = k;
            p[1] = v;
            t->len++;
            return;
        }

        if (mu_equals(k, p[0])) {
            if (!p[1]) {
                p[1] = v;
                t->nils--;
                t->len++;
            } else {
                mu_dec(p[1]);
                p[1] = v;
            }

            return;
        }
    }
}


void tbl_insert(mu_t m, mu_t k, mu_t v) {
    struct tbl *t = tbl_wtbl(m);

    if (!k)
        return;

    if (!v)
        return tbl_insertnil(t, k, v);
    else
        return tbl_insertval(t, k, v);
}


// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
static void tbl_assignnil(mu_t m, mu_t k, mu_t v) {
    hash_t i, hash = mu_hash(k);

    for (; m && !tbl_isro(m); m = tbl_rtbl(m)->tail) {
        struct tbl *t = tbl_rtbl(m);

        if (t->stride < 2) {
            if (!mu_ishash(k) || hash >= t->len)
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
            mu_t *p = &t->array[2*mi];

            if (!p[0]) 
                break;

            if (mu_equals(k, p[0])) {
                if (!p[1])
                    break;

                mu_dec(p[1]);
                p[1] = mnil;
                t->nils++;
                t->len--;
                return;
            }
        }
    }
}


static void tbl_assignval(mu_t m, mu_t k, mu_t v) {
    mu_t head = m;
    hash_t i, hash = mu_hash(k);

    for (; m && tbl_isro(m); m = tbl_rtbl(m)->tail) {
        struct tbl *t = tbl_rtbl(m);

        if (t->stride < 2) {
            if (!mu_ishash(k) || hash >= t->len)
                continue;

            if (t->stride == 0) {
                if (mu_ishash(v) && num_uint(v) == hash + t->offset)
                    return;

                tbl_realizevals(t);
            }

            mu_dec(t->array[hash]);
            t->array[hash] = v;
            return;
        }

        for (i = hash;; i = tbl_next(i)) {
            hash_t mi = i & ((1 << t->npw2) - 1);
            mu_t *p = &t->array[2*mi];

            if (!p[0])
                break;

            if (mu_equals(k, p[0])) {
                if (!p[1])
                    break;

                mu_dec(p[1]);
                p[1] = v;
                return;
            }
        }
    }


    struct tbl *t = tbl_wtbl(head);

    if (tbl_ncap(t->len+t->nils + 1) > (1 << t->npw2))
        tbl_resize(t, t->len+1); // TODO check size

    if (t->stride < 2) {
        if (mu_ishash(k) && hash == t->len) {
            if (t->stride == 0) {
                if (mu_ishash(v)) {
                    if (t->len == 0)
                        t->offset = num_uint(v);

                    if (num_uint(v) == hash + t->offset) {
                        t->len++;
                        return;
                    }
                }

                tbl_realizevals(t);
            }

            t->array[hash] = v;
            t->len++;
            return;
        }

        tbl_realizekeys(t);
    }

    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & ((1 << t->npw2) - 1);
        mu_t *p = &t->array[2*mi];

        if (!p[0]) {
            p[0] = k;
            p[1] = v;
            t->len++;
            return;
        }

        if (mu_equals(k, p[0]) && !p[1]) {
            p[1] = v;
            t->nils--;
            t->len++;
            return;
        }
    }
}


void tbl_assign(mu_t m, mu_t k, mu_t v) {
    if (!k)
        return;

    if (!v)
        return tbl_assignnil(m, k, v);
    else
        return tbl_assignval(m, k, v);
}



// Performs iteration on a table
frame_t tbl_0_iteration(mu_t scope, mu_t *frame) {
    struct tbl *t = tbl_rtbl(tbl_lookup(scope, muint(0)));
    uint_t i = num_uint(tbl_lookup(scope, muint(1)));

    if (i >= t->len)
        return 0;

    tbl_insert(scope, muint(1), muint(i+1));

    frame[0] = muint(i);
    frame[1] = muint(t->offset + i);
    return 2;
}

frame_t tbl_1_iteration(mu_t scope, mu_t *frame) {
    struct tbl *t = tbl_rtbl(tbl_lookup(scope, muint(0)));
    uint_t i = num_uint(tbl_lookup(scope, muint(1)));

    if (i >= t->len)
        return 0;

    tbl_insert(scope, muint(1), muint(i+1));

    frame[0] = muint(i);
    frame[1] = mu_inc(t->array[i]);
    return 2;
}

frame_t tbl_2_iteration(mu_t scope, mu_t *frame) {
    struct tbl *t = tbl_rtbl(tbl_lookup(scope, muint(0)));
    uint_t i = num_uint(tbl_lookup(scope, muint(1)));
    mu_t k, v;

    if (i >= t->len)
        return 0;

    do {
        k = t->array[2*i  ];
        v = t->array[2*i+1];
        i += 1;
    } while (!k || !v);

    tbl_insert(scope, muint(1), muint(i));

    frame[0] = mu_inc(k);
    frame[1] = mu_inc(v);
    return 2;
}

mu_t tbl_iter(mu_t tbl) {
    static sbfn_t *const tbl_iters[3] = {
        tbl_0_iteration,
        tbl_1_iteration,
        tbl_2_iteration
    };

    mu_t scope = tbl_create(2);
    tbl_insert(scope, muint(0), tbl);
    tbl_insert(scope, muint(1), muint(0));

    return msbfn(0x00, tbl_iters[tbl_rtbl(tbl)->stride], scope);
}


// Returns a string representation of the table
mu_t tbl_repr(mu_t t) {
    mu_t *reprs = mu_alloc(2*tbl_len(t) * sizeof(mu_t));
    uint_t size = 2;
    uint_t i = 0;

    tbl_for_begin (k, v, t) {
        reprs[2*i  ] = mu_repr(k);
        reprs[2*i+1] = mu_repr(v);
        size += str_len(reprs[2*i  ]);
        size += str_len(reprs[2*i+1]);
        size += (i == tbl_len(t)-1) ? 2 : 4;

        i++;
    } tbl_for_end;

    if (size > MU_MAXLEN)
        mu_err_len();

    byte_t *s = mstr_create(size);
    byte_t *out = s;

    *out++ = '[';

    for (i=0; i < tbl_len(t); i++) {
        memcpy(out, str_bytes(reprs[2*i]), str_len(reprs[2*i]));
        out += str_len(reprs[2*i]);
        str_dec(reprs[2*i]);

        *out++ = ':';
        *out++ = ' ';

        memcpy(out, str_bytes(reprs[2*i+1]), str_len(reprs[2*i+1]));
        out += str_len(reprs[2*i+1]);
        str_dec(reprs[2*i+1]);

        if (i != tbl_len(t)-1) {
            *out++ = ',';
            *out++ = ' ';
        }
    }

    *out++ = ']';

    mu_dealloc(reprs, 2*tbl_len(t) * sizeof(mu_t));
    return mstr_intern(s, size);
}

mu_t tbl_concat(mu_t a, mu_t b, mu_t offset) {
    uint_t max = 0;
    if (mu_isnum(offset)) {
        max = num_uint(offset);
    } else {
        tbl_for_begin(k, v, a) {
            if (mu_isnum(k) && num_uint(k)+1 > max)
                max = num_uint(k)+1;
        } tbl_for_end
    }

    mu_t res;
    if (mu_ref(a) == 1) {
        res = a;
    } else {
        res = tbl_create(tbl_len(a) + tbl_len(b));

        tbl_for_begin(k, v, a) {
            tbl_insert(res, k, v);
        } tbl_for_end

        tbl_dec(a);
    }

    tbl_for_begin(k, v, b) {
        if (mu_isnum(k))
            tbl_insert(res, muint(num_uint(k) + max), v);
        else
            tbl_insert(res, k, v);
    } tbl_for_end

    tbl_dec(b);

    return res;
}

// TODO optimize for arrays
// TODO can the heap allocation be removed in the general case?
mu_t tbl_pop(mu_t t, mu_t i) {
    mu_t ret = tbl_lookup(t, i);
    tbl_insert(t, i, mnil);

    if (mu_isnum(i)) {
        mu_t temp = tbl_create(tbl_len(t));

        tbl_for_begin(k, v, t) {
            if (mu_isnum(k) && num_uint(k) > num_uint(i)) {
                tbl_insert(temp, k, v);
                tbl_insert(t, k, mnil);
            }
        } tbl_for_end

        tbl_for_begin(k, v, temp) {
            tbl_insert(t, muint(num_uint(k)-1), v);
        } tbl_for_end
    }

    return ret;
}

// TODO optimize for arrays
// TODO can the heap allocation be removed in the general case?
void tbl_push(mu_t t, mu_t v, mu_t i) {
    if (mu_isnum(i)) {
        mu_t temp = tbl_create(tbl_len(t));

        tbl_for_begin(k, v, t) {
            if (mu_isnum(k) && num_uint(k) >= num_uint(i)) {
                tbl_insert(temp, k, v);
                tbl_insert(t, k, mnil);
            }
        } tbl_for_end

        tbl_for_begin(k, v, temp) {
            tbl_insert(t, muint(num_uint(k)+1), v);
        } tbl_for_end
    }

    tbl_insert(t, i, v);
}

