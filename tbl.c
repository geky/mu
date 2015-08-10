#include "tbl.h"

#include "num.h"
#include "str.h"
#include "fn.h"
#include "err.h"
#include <string.h>


// Internally used conversion between mu_t and struct tbl
mu_inline mu_t totbl(struct tbl *t) {
    return (mu_t)((muint_t)t + MU_TBL);
}

mu_inline struct tbl *fromrtbl(mu_t m) {
    return (struct tbl *)(~7 & (muint_t)m);
}

mu_inline struct tbl *fromwtbl(mu_t m) {
    if (mu_unlikely(tbl_isro(m)))
        mu_err_readonly();

    return (struct tbl *)((muint_t)m - MU_TBL);
}


// General purpose hash for mu types
mu_inline muint_t tbl_hash(mu_t m) {
    // Mu garuntees bitwise equality for Mu types, which has the very
    // nice property of being a free hash function.
    //
    // We remove the lower 3 bits, since they store the type, which
    // generally doesn't vary in tables. And we xor the upper and lower
    // halves so all bits can affect the range of the length type.
    // This is the boundary on the table size so it's really the only
    // part that matters.
    return ((muint_t)m >> (8*sizeof(mlen_t))) ^ ((muint_t)m >> 3);
}

// Finds capacity based on load factor of 2/3
mu_inline muint_t tbl_nsize(muint_t s) {
    return s + (s >> 1);
}

// Finds next power of 2 after checks for minimum bounds
// and growing capacity based on load factor
mu_inline muintq_t tbl_npw2(bool linear, muint_t s) {
    if (!linear)
        s = tbl_nsize(s);

    if (linear && s < (MU_MINALLOC/sizeof(mu_t)))
        s = MU_MINALLOC/sizeof(mu_t);
    else if (!linear && s < (MU_MINALLOC/sizeof(mu_t[2])))
        s = MU_MINALLOC/sizeof(mu_t[2]);

    return mu_npw2(s);
}


// Table creating functions
mu_t mntbl(muint_t n, mu_t (*pairs)[2]) {
    mu_t t = tbl_create(n);

    for (muint_t i = 0; i < n; i++) {
        tbl_insert(t, pairs[i][0], pairs[i][1]);
    }

    return t;
}


// Functions for managing tables
mu_t tbl_create(muint_t len) {
    struct tbl *t = ref_alloc(sizeof(struct tbl));

    t->npw2 = tbl_npw2(true, len);
    t->linear = true;
    t->len = 0;
    t->tail = 0;

    muint_t size = 1 << t->npw2;
    t->array = mu_alloc(size * sizeof(mu_t));
    memset(t->array, 0, size * sizeof(mu_t));

    return totbl(t);
}

mu_t tbl_extend(muint_t len, mu_t tail) {
    mu_t t = tbl_create(len);
    fromrtbl(t)->tail = tail;
    return t;
}

void tbl_destroy(mu_t m) {
    struct tbl *t = fromrtbl(m);
    muint_t size = (t->linear ? 1 : 2) * (1 << t->npw2);

    for (muint_t i = 0; i < size; i++)
        mu_dec(t->array[i]);

    mu_dealloc(t->array, size * sizeof(mu_t));
    mu_dec(t->tail);
    ref_dealloc(t, sizeof(struct tbl));
}


// Recursively looks up a key in the table
// returns either that value or nil
mu_t tbl_lookup(mu_t m, mu_t k) {
    if (!k) return mnil;

    for (struct tbl *t = fromrtbl(m); t; t = fromrtbl(t->tail)) {
        muint_t mask = (1 << t->npw2) - 1;

        if (t->linear) {
            muint_t i = num_uint(k) & mask;

            if (k == muint(i))
                return mu_inc(t->array[i]);

        } else {
            for (muint_t i = tbl_hash(k);; i++) {
                mu_t *p = &t->array[2*(i & mask)];

                if (!p[0] || (k == p[0] && !p[1])) {
                    break;
                } else if (k == p[0]) {
                    mu_dec(k);
                    return mu_inc(p[1]);
                }
            }
        }
    }

    mu_dec(k);
    return mnil;
}


// Modify table info according to placement/replacement
static void tbl_place(struct tbl *t, mu_t *dp, mu_t v) {
    mu_check_len(1 + (muint_t)t->len);
    t->len++;
    *dp = v;
}

static void tbl_replace(struct tbl *t, mu_t *dp, mu_t v) {
    mu_t d = *dp;

    if (v && !d) {
        mu_check_len(1 + (muint_t)t->len);
        t->len++;
        t->nils--;
    } else if (!v && d) {
        t->len--;
        t->nils++;
    }

    // We must replace before decrementing in case destructors run
    *dp = v;
    mu_dec(d);
}

// Converts from array to full table
static void tbl_realize(struct tbl *t) {
    muint_t size = 1 << t->npw2;
    muintq_t npw2 = tbl_npw2(false, t->len + 1);
    muint_t nsize = 1 << npw2;
    muint_t mask = nsize - 1;
    mu_t *array = mu_alloc(2*nsize * sizeof(mu_t));
    memset(array, 0, 2*nsize * sizeof(mu_t));

    for (muint_t j = 0; j < size; j++) {
        mu_t k = muint(j);
        mu_t v = t->array[j];

        if (!v) continue;

        for (muint_t i = tbl_hash(k);; i++) {
            mu_t *p = &array[2*(i & mask)];

            if (!p[0]) {
                p[0] = k;
                p[1] = v;
                break;
            }
        }
    }

    mu_dealloc(t->array, size * sizeof(mu_t));
    t->array = array;
    t->npw2 = npw2;
    t->nils = 0;
    t->linear = false;
}

// Make room for an additional element
static void tbl_expand(struct tbl *t) {
    muint_t size = 1 << t->npw2;

    if (t->linear) {
        if (t->len + 1 > size) {
            muintq_t npw2 = tbl_npw2(true, t->len + 1);
            muint_t nsize = 1 << npw2;
            mu_t *array = mu_alloc(nsize * sizeof(mu_t));
            memcpy(array, t->array, size * sizeof(mu_t));
            memset(array+size, 0, (nsize-size) * sizeof(mu_t));

            mu_dealloc(t->array, size * sizeof(mu_t));
            t->array = array;
            t->npw2 = npw2;
        }
    } else {
        if (tbl_nsize(t->len + t->nils + 1) > size) {
            muintq_t npw2 = tbl_npw2(false, t->len + 1);
            muint_t nsize = 1 << npw2;
            muint_t mask = nsize - 1;
            mu_t *array = mu_alloc(2*nsize * sizeof(mu_t));
            memset(array, 0, 2*nsize * sizeof(mu_t));

            for (muint_t j = 0; j < size; j++) {
                mu_t k = t->array[2*j+0];
                mu_t v = t->array[2*j+1];

                if (!k || !v)
                    continue;

                for (muint_t i = tbl_hash(k);; i++) {
                    mu_t *p = &array[2*(i & mask)];

                    if (!p[0]) {
                        p[0] = k;
                        p[1] = v;
                        break;
                    }
                }
            }

            mu_dealloc(t->array, 2*size * sizeof(mu_t));
            t->array = array;
            t->npw2 = npw2;
            t->nils = 0;
        }
    }
}


// Inserts a value in the table with the given key
// without decending down the tail chain
void tbl_insert(mu_t m, mu_t k, mu_t v) {
    if (!k) {
        mu_dec(v);
        return;
    }

    struct tbl *t = fromwtbl(m);
    if (v)
        tbl_expand(t);

    muint_t mask = (1 << t->npw2) - 1;

    if (t->linear) {
        muint_t i = num_uint(k) & mask;

        if (k == muint(i)) {
            tbl_replace(t, &t->array[i], v);
        } else if (v) {
            // Index is out of range, convert to full table
            tbl_realize(t);
            return tbl_insert(m, k, v);
        }
    } else {
        for (muint_t i = tbl_hash(k);; i++) {
            mu_t *p = &t->array[2*(i & mask)];

            if (!p[0] && v) {
                p[0] = k;
                tbl_place(t, &p[1], v);
                return;
            } else if (!p[0]) {
                mu_dec(k);
                return;
            } else if (k == p[0]) {
                mu_dec(k);
                tbl_replace(t, &p[1], v);
                return;
            }
        }
    }
}

// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
void tbl_assign(mu_t m, mu_t k, mu_t v) {
    if (!k) {
        mu_dec(v);
        return;
    }

    mu_t head = m;

    for (struct tbl *t; mu_type(m) == MU_TBL; m = t->tail) {
        t = fromrtbl(m);
        muint_t mask = (1 << t->npw2) - 1;

        if (t->linear) {
            muint_t i = num_uint(k) & mask;

            if (k == muint(i) && t->array[i]) {
                tbl_replace(t, &t->array[i], v);
                return;
            }
        } else {
            for (muint_t i = tbl_hash(k);; i++) {
                mu_t *p = &t->array[2*(i & mask)];

                if (!p[0] || (k == p[0] && !p[1])) {
                    break;
                } else if (k == p[0]) {
                    mu_dec(k);
                    tbl_replace(t, &p[1], v);
                    return;
                }
            }
        }
    }

    if (!v)
        mu_dec(k);
    else
        tbl_insert(head, k, v);
}


// Performs iteration on a table
bool tbl_next(mu_t m, muint_t *ip, mu_t *kp, mu_t *vp) {
    struct tbl *t = fromrtbl(m);
    muint_t i = *ip;
    mu_t k, v;

    do {
        if (i >= (1 << t->npw2)) {
            *kp = mnil;
            *vp = mnil;
            return false;
        }

        k = t->linear ? muint(i)    : t->array[2*i+0];
        v = t->linear ? t->array[i] : t->array[2*i+1];
        i++;
    } while (!k || !v);

    *ip = i;
    *kp = mu_inc(k);
    *vp = mu_inc(v);
    return true;
}

static mc_t tbl_step(mu_t scope, mu_t *frame) {
    mu_t t = tbl_lookup(scope, muint(0));
    muint_t i = num_uint(tbl_lookup(scope, muint(1)));

    tbl_next(t, &i, &frame[0], &frame[1]);
    tbl_insert(scope, muint(1), muint(i));
    return 2;
}

mu_t tbl_iter(mu_t t) {
    return msbfn(0x0, tbl_step, mtbl({
        { muint(0), t },
        { muint(1), muint(0) }
    }));
}


// Returns a string representation of the table
mu_t tbl_repr(mu_t t) {
    mbyte_t *s = mstr_create(0);
    muint_t si = 0;
    muint_t ti = 0;
    mu_t k, v;

    bool linear = fromrtbl(t)->linear;
    for (muint_t i = 0; linear && i < tbl_len(t); i++) {
        if (!fromrtbl(t)->array[i])
            linear = false;
    }

    mstr_insert(&s, &si, '[');

    while (tbl_next(t, &ti, &k, &v)) {
        if (!linear) {
            mstr_concat(&s, &si, mu_repr(k));
            mstr_zcat(&s, &si, ": ");
        } else {
            mu_dec(k);
        }

        mstr_concat(&s, &si, mu_repr(v));
        mstr_zcat(&s, &si, ", ");
    }

    if (tbl_len(t) > 0)
        si -= 2;

    tbl_dec(t);
    mstr_insert(&s, &si, ']');
    return mstr_intern(s, si);
}


// Array-like manipulation
mu_t tbl_concat(mu_t a, mu_t b, mu_t moffset) {
    muint_t offset = num_uint(moffset);
    if (moffset != muint(offset)) {
        offset = 0;

        mu_t k, v;
        for (muint_t i = 0; tbl_next(a, &i, &k, &v);) {
            muint_t n = num_uint(k);
            if (k == muint(n) && n+1 > offset)
                offset = n+1;
        }
    }

    mu_t res = tbl_create(tbl_len(a) + tbl_len(b));
    mu_t k, v;

    for (muint_t i = 0; tbl_next(a, &i, &k, &v);) {
        tbl_insert(res, k, v);
    }

    for (muint_t i = 0; tbl_next(b, &i, &k, &v);) {
        muint_t n = num_uint(k);
        if (k == muint(n))
            tbl_insert(res, muint(n+offset), v);
        else
            tbl_insert(res, k, v);
    }

    tbl_dec(a);
    tbl_dec(b);
    return res;
}

mu_t tbl_pop(mu_t t, mu_t k) {
    if (!k)
        k = muint(tbl_len(t)-1);

    mu_t ret = tbl_lookup(t, mu_inc(k));
    tbl_insert(t, k, mnil);

    muint_t i = num_uint(k);
    if (k == muint(i)) {
        if (fromrtbl(t)->linear) {
            muint_t size = 1 << fromrtbl(t)->npw2;
            mu_t *array = fromrtbl(t)->array;

            memmove(&array[i], &array[i+1], (size - (i+1)) * sizeof(mu_t));
            array[size-1] = mnil;
        } else {
            mu_t temp = tbl_create(tbl_len(t));
            mu_t k, v;

            for (muint_t j = 0; tbl_next(t, &j, &k, &v);) {
                muint_t n = num_uint(k);
                if (k == muint(n) && n > i) {
                    tbl_insert(temp, muint(n-1), v);
                    tbl_insert(t, k, mnil);
                }
            }

            for (muint_t j = 0; tbl_next(temp, &j, &k, &v);) {
                tbl_insert(t, k, v);
            }

            tbl_dec(temp);
        }
    }

    return ret;
}

void tbl_push(mu_t t, mu_t v, mu_t k) {
    if (!k)
        k = muint(tbl_len(t));

    muint_t i = num_uint(k);
    if (k == muint(i)) {
        if (fromrtbl(t)->linear) {
            tbl_expand(fromrtbl(t));
            muint_t size = 1 << fromrtbl(t)->npw2;
            mu_t *array = fromrtbl(t)->array;

            memmove(&array[i+1], &array[i], (size - i) * sizeof(mu_t));
        } else {
            mu_t temp = tbl_create(tbl_len(t));
            mu_t k, v;

            for (muint_t j = 0; tbl_next(t, &j, &k, &v);) {
                muint_t n = num_uint(k);
                if (k == muint(n) && n >= i) {
                    tbl_insert(temp, muint(n+1), v);
                    tbl_insert(t, k, mnil);
                }
            }

            for (muint_t j = 0; tbl_next(temp, &j, &k, &v);) {
                tbl_insert(t, k, v);
            }

            tbl_dec(temp);
        }
    }

    tbl_insert(t, k, v);
}

