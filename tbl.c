#include "tbl.h"

#include "num.h"
#include "str.h"
#include "fn.h"
#include "err.h"
#include "parse.h"
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
    mu_t t = tbl_create(n, 0);

    for (muint_t i = 0; i < n; i++) {
        tbl_insert(t, pairs[i][0], pairs[i][1]);
    }

    return t;
}


// Functions for managing tables
mu_t tbl_create(muint_t len, mu_t tail) {
    struct tbl *t = ref_alloc(sizeof(struct tbl));

    t->npw2 = tbl_npw2(true, len);
    t->linear = true;
    t->len = 0;
    t->tail = tail;

    muint_t size = 1 << t->npw2;
    t->array = mu_alloc(size * sizeof(mu_t));
    memset(t->array, 0, size * sizeof(mu_t));

    return totbl(t);
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
        if (i >= (1 << t->npw2))
            return false;

        k = t->linear ? muint(i)    : t->array[2*i+0];
        v = t->linear ? t->array[i] : t->array[2*i+1];
        i++;
    } while (!k || !v);

    if (kp) *kp = mu_inc(k);
    if (vp) *vp = mu_inc(v);
    *ip = i;
    return true;
}

static mc_t tbl_iter_step(mu_t scope, mu_t *frame) {
    mu_t t = tbl_lookup(scope, muint(0));
    muint_t i = num_uint(tbl_lookup(scope, muint(1)));

    bool next = tbl_next(t, &i, 0, &frame[0]);
    mu_dec(t);
    tbl_insert(scope, muint(1), muint(i));
    return next ? 1 : 0;
}

mu_t tbl_iter(mu_t t) {
    return msbfn(0x0, tbl_iter_step, mtbl({
        { muint(0), t },
        { muint(1), muint(0) }
    }));
}

static mc_t tbl_pairs_step(mu_t scope, mu_t *frame) {
    mu_t t = tbl_lookup(scope, muint(0));
    muint_t i = num_uint(tbl_lookup(scope, muint(1)));

    bool next = tbl_next(t, &i, &frame[0], &frame[1]);
    mu_dec(t);
    tbl_insert(scope, muint(1), muint(i));
    return next ? 2 : 0;
}

mu_t tbl_pairs(mu_t t) {
    return msbfn(0x0, tbl_pairs_step, mtbl({
        { muint(0), t },
        { muint(1), muint(0) }
    }));
}

// Creates a table from an iterator
mu_t tbl_fromiter(mu_t i, mu_t tail) {
    mu_t frame[MU_FRAME];
    mu_t t = tbl_create(0, tail);
    muint_t index = 0;

    while (true) {
        mu_fcall(mu_inc(i), 0x02, frame);

        if (!frame[0]) {
            fn_dec(i);
            return t;
        }

        if (frame[1])
            tbl_insert(t, frame[0], frame[1]);
        else 
            tbl_insert(t, muint(index++), frame[0]);
    }
}


// Data structure operations
void tbl_push(mu_t m, mu_t v, mu_t i) {
    struct tbl *t = fromwtbl(m); // TODO better error handling?
    if (t->linear) {
        tbl_expand(t);
        muint_t size = 1 << t->npw2;

        if (num_cmp(i, muint(size)) <= 0) {
            muint_t j = num_uint(i);

            memmove(&t->array[j+1], &t->array[j], (size-j)*sizeof(mu_t));
            t->array[j] = mnil;
        }
    } else {
        mu_t d = tbl_create(tbl_len(m), 0);
        mu_t k, v;

        for (muint_t j = 0; tbl_next(m, &j, &k, &v);) {
            if (mu_isnum(k) && num_cmp(k, i) >= 0) {
                tbl_insert(d, num_add(k, muint(1)), v);
                tbl_insert(m, k, mnil);
            } else {
                mu_dec(k);
                mu_dec(v);
            }
        }

        for (muint_t j = 0; tbl_next(d, &j, &k, &v);) {
            tbl_insert(m, k, v);
        }

        tbl_dec(d);
    }

    tbl_insert(m, i, v);
}

mu_t tbl_pop(mu_t m, mu_t i) {
    mu_t ret = tbl_lookup(m, mu_inc(i));
    tbl_insert(m, i, mnil);

    struct tbl *t = fromwtbl(m); // TODO better error handling?
    if (t->linear) {
        muint_t size = 1 << t->npw2;

        if (num_cmp(i, muint(size)) < 0) {
            muint_t j = num_uint(i);

            memmove(&t->array[j], &t->array[j+1], (size-(j+1))*sizeof(mu_t));
            t->array[size-1] = mnil;
        }
    } else {
        mu_t d = tbl_create(tbl_len(m), 0);
        mu_t k, v;

        for (muint_t j = 0; tbl_next(m, &j, &k, &v);) {
            if (mu_isnum(k) && num_cmp(k, i) > 0) {
                tbl_insert(d, num_sub(k, muint(1)), v);
                tbl_insert(m, k, mnil);
            } else {
                mu_dec(k);
                mu_dec(v);
            }
        }

        for (muint_t j = 0; tbl_next(d, &j, &k, &v);) {
            tbl_insert(m, k, v);
        }

        tbl_dec(d);
    }

    return ret;
}

mu_t tbl_concat(mu_t a, mu_t b, mu_t offset) {
    mu_t d = tbl_create(tbl_len(a) + tbl_len(b), 0);
    mu_t k, v;

    for (muint_t i = 0; tbl_next(a, &i, &k, &v);) {
        tbl_insert(d, k, v);
    }

    for (muint_t i = 0; tbl_next(b, &i, &k, &v);) {
        if (mu_isnum(k))
            tbl_insert(d, num_add(k, offset), v);
        else
            tbl_insert(d, k, v);
    }

    tbl_dec(a);
    tbl_dec(b);
    return d;
}

mu_t tbl_subset(mu_t t, mu_t lower, mu_t upper) {
    mu_t d = tbl_create(tbl_len(t), 0);
    mu_t k, v;

    for (muint_t i = 0; tbl_next(t, &i, &k, &v);) {
        if (mu_isnum(k) && num_cmp(k, lower) >= 0 &&
                           num_cmp(k, upper) < 0) {
            tbl_insert(d, num_sub(k, lower), v);
        } else {
            mu_dec(k);
            mu_dec(v);
        }
    }

    mu_dec(t);
    return d;
}

// Set operations
mu_t tbl_and(mu_t a, mu_t b) {
    mlen_t alen = tbl_len(a);
    mlen_t blen = tbl_len(b);
    mu_t d = tbl_create(alen < blen ? alen : blen, 0);
    mu_t k, v;

    for (muint_t i = 0; tbl_next(a, &i, &k, &v);) {
        mu_t w = tbl_lookup(b, mu_inc(k));
        if (w) {
            mu_dec(w);
            tbl_insert(d, k, v);
        } else {
            mu_dec(k);
            mu_dec(v);
        }
    }

    tbl_dec(a);
    tbl_dec(b);
    return d;
}

mu_t tbl_or(mu_t a, mu_t b) {
    mu_t d = tbl_create(tbl_len(a) + tbl_len(b), 0);
    mu_t k, v;

    for (muint_t i = 0; tbl_next(b, &i, &k, &v);) {
        tbl_insert(d, k, v);
    }

    for (muint_t i = 0; tbl_next(a, &i, &k, &v);) {
        tbl_insert(d, k, v);
    }

    tbl_dec(a);
    tbl_dec(b);
    return d;
}

mu_t tbl_xor(mu_t a, mu_t b) {
    mlen_t alen = tbl_len(a);
    mlen_t blen = tbl_len(b);
    mu_t d = tbl_create(alen > blen ? alen : blen, 0);
    mu_t k, v;

    for (muint_t i = 0; tbl_next(a, &i, &k, &v);) {
        mu_t w = tbl_lookup(b, mu_inc(k));
        if (!w) {
            tbl_insert(d, k, v);
        } else {
            mu_dec(k);
            mu_dec(v);
            mu_dec(w);
        }
    }

    for (muint_t i = 0; tbl_next(b, &i, &k, &v);) {
        mu_t w = tbl_lookup(a, mu_inc(k));
        if (!w) {
            tbl_insert(d, k, v);
        } else {
            mu_dec(k);
            mu_dec(v);
            mu_dec(w);
        }
    }

    tbl_dec(a);
    tbl_dec(b);
    return d;
}

mu_t tbl_diff(mu_t a, mu_t b) {
    mu_t d = tbl_create(tbl_len(a), 0);
    mu_t k, v;

    for (muint_t i = 0; tbl_next(a, &i, &k, &v);) {
        mu_t w = tbl_lookup(b, mu_inc(k));
        if (!w) {
            tbl_insert(d, k, v);
        } else {
            mu_dec(k);
            mu_dec(v);
            mu_dec(w);
        }
    }

    tbl_dec(a);
    tbl_dec(b);
    return d;
}


// String representation
mu_t tbl_parse(const mbyte_t **ppos, const mbyte_t *end) {
    const mbyte_t *pos = *ppos;

    if (pos == end || *pos++ != '[')
        mu_err_parse();

    mu_t t = tbl_create(0, 0);
    mu_t i = muint(0);

    while (pos < end && *pos != ']') {
        mu_t k = mu_nparse(&pos, end);

        if (pos < end && *pos == ':') {
            pos++;
            mu_t v = mu_nparse(&pos, end);
            tbl_insert(t, k, v);
        } else {
            tbl_insert(t, i, k);
            i = num_add(i, muint(1));
        }

        if (pos == end || *pos != ',')
            break;

        pos++;
    }

    if (pos == end || *pos++ != ']')
        mu_err_parse();

    *ppos = pos;
    return t;    
}

mu_t tbl_repr(mu_t t) {
    mbyte_t *s = mstr_create(7 + 2*sizeof(muint_t));
    memcpy(s, "tbl(0x", 6);

    for (muint_t i = 0; i < 2*sizeof(muint_t); i++)
        s[i+6] = mu_toascii(0xf & ((muint_t)t >> 4*(2*sizeof(muint_t)-1 - i)));

    s[6 + 2*sizeof(muint_t)] = ')';
    return mstr_intern(s, 7 + 2*sizeof(muint_t));
}

static void tbl_dump_nested(mu_t t, mbyte_t **s, muint_t *slen, 
                            mu_t depth, mu_t indent, muint_t nest) {
    bool linear = fromrtbl(t)->linear;
    for (muint_t i = 0; linear && i < tbl_len(t); i++) {
        if (!fromrtbl(t)->array[i])
            linear = false;
    }

    mstr_insert(s, slen, '[');

    mu_t k, v;
    for (muint_t i = 0; tbl_next(t, &i, &k, &v);) {
        if (indent) {
            muint_t nest_indent = nest * num_uint(indent);
            mstr_insert(s, slen, '\n');
            for (muint_t j = 0; j < nest_indent; j++)
                mstr_insert(s, slen, ' ');
        }

        if (!linear) {
            mstr_concat(s, slen, mu_repr(k));
            mstr_zcat(s, slen, ": ");
        } else {
            mu_dec(k);
        }

        if (mu_istbl(v) && num_cmp(depth, muint(0)) > 0) {
            tbl_dump_nested(v, s, slen, num_sub(depth, muint(1)), 
                            indent, nest + 1);
        } else {
            mstr_concat(s, slen, mu_repr(v));
        }

        mstr_zcat(s, slen, indent ? "," : ", ");
    }

    if (tbl_len(t) > 0) {
        *slen -= indent ? 1 : 2;

        if (indent) {
            muint_t nest_indent = (nest-1) * num_uint(indent);
            mstr_insert(s, slen, '\n');
            for (muint_t j = 0; j < nest_indent; j++)
                mstr_insert(s, slen, ' ');
        }
    }

    mstr_insert(s, slen, ']');
    tbl_dec(t);
}

mu_t tbl_dump(mu_t t, mu_t depth, mu_t indent) {
    mbyte_t *s = mstr_create(0);
    muint_t slen = 0;

    tbl_dump_nested(t, &s, &slen, 
                    num_sub(depth, muint(1)), indent, 1);

    return mstr_intern(s, slen);
}
