#include "tbl.h"

#include "num.h"
#include "str.h"
#include "fn.h"
#include "parse.h"


// Table access
mu_inline struct tbl *tbl(mu_t t) {
    return (struct tbl *)((muint_t)t - MTTBL);
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

    return (mu_t)((muint_t)t + MTTBL);
}

mu_t tbl_extend(muint_t len, mu_t tail) {
    mu_assert(!tail || mu_istbl(tail));
    mu_t t = tbl_create(len);
    tbl(t)->tail = tail;
    return t;
}

void tbl_inherit(mu_t t, mu_t tail) {
    mu_assert(!tail || mu_istbl(tail));
    tbl(t)->tail = tail;
}

void tbl_destroy(mu_t t) {
    muint_t size = (tbl(t)->linear ? 1 : 2) * (1 << tbl(t)->npw2);

    for (muint_t i = 0; i < size; i++)
        mu_dec(tbl(t)->array[i]);

    mu_dealloc(tbl(t)->array, size * sizeof(mu_t));
    mu_dec(tbl(t)->tail);
    ref_dealloc(t, sizeof(struct tbl));
}


// Recursively looks up a key in the table
// returns either that value or nil
mu_t tbl_lookup(mu_t t, mu_t k) {
    mu_assert(mu_istbl(t));
    if (!k) {
        return 0;
    }

    for (; t; t = tbl(t)->tail) {
        muint_t mask = (1 << tbl(t)->npw2) - 1;

        if (tbl(t)->linear) {
            muint_t i = num_uint(k) & mask;

            if (k == muint(i) && tbl(t)->array[i])
                return mu_inc(tbl(t)->array[i]);

        } else {
            for (muint_t i = tbl_hash(k);; i++) {
                mu_t *p = &tbl(t)->array[2*(i & mask)];

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
    return 0;
}


// Modify table info according to placement/replacement
static void tbl_place(mu_t t, mu_t *dp, mu_t v) {
    if ((muint_t)tbl(t)->len + 1 > (mlen_t)-1)
        mu_errorf("exceeded max length in table");

    tbl(t)->len += 1;
    *dp = v;
}

static void tbl_replace(mu_t t, mu_t *dp, mu_t v) {
    mu_t d = *dp;

    if (v && !d) {
        if ((muint_t)tbl(t)->len + 1 > (mlen_t)-1)
            mu_errorf("exceeded max length in table");

        tbl(t)->len += 1;
        tbl(t)->nils -= 1;
    } else if (!v && d) {
        tbl(t)->len -= 1;
        tbl(t)->nils += 1;
    }

    // We must replace before decrementing in case destructors run
    *dp = v;
    mu_dec(d);
}

// Converts from array to full table
static void tbl_realize(mu_t t) {
    muint_t size = 1 << tbl(t)->npw2;
    muintq_t npw2 = tbl_npw2(false, tbl(t)->len + 1);
    muint_t nsize = 1 << npw2;
    muint_t mask = nsize - 1;
    mu_t *array = mu_alloc(2*nsize * sizeof(mu_t));
    memset(array, 0, 2*nsize * sizeof(mu_t));

    for (muint_t j = 0; j < size; j++) {
        mu_t k = muint(j);
        mu_t v = tbl(t)->array[j];

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

    mu_dealloc(tbl(t)->array, size * sizeof(mu_t));
    tbl(t)->array = array;
    tbl(t)->npw2 = npw2;
    tbl(t)->nils = 0;
    tbl(t)->linear = false;
}

// Make room for an additional element
static void tbl_expand(mu_t t) {
    muint_t size = 1 << tbl(t)->npw2;

    if (tbl(t)->linear) {
        if (tbl(t)->len + 1 > size) {
            muintq_t npw2 = tbl_npw2(true, tbl(t)->len + 1);
            muint_t nsize = 1 << npw2;
            mu_t *array = mu_alloc(nsize * sizeof(mu_t));
            memcpy(array, tbl(t)->array, size * sizeof(mu_t));
            memset(array+size, 0, (nsize-size) * sizeof(mu_t));

            mu_dealloc(tbl(t)->array, size * sizeof(mu_t));
            tbl(t)->array = array;
            tbl(t)->npw2 = npw2;
        }
    } else {
        if (tbl_nsize(tbl(t)->len + tbl(t)->nils + 1) > size) {
            muintq_t npw2 = tbl_npw2(false, tbl(t)->len + 1);
            muint_t nsize = 1 << npw2;
            muint_t mask = nsize - 1;
            mu_t *array = mu_alloc(2*nsize * sizeof(mu_t));
            memset(array, 0, 2*nsize * sizeof(mu_t));

            for (muint_t j = 0; j < size; j++) {
                mu_t k = tbl(t)->array[2*j+0];
                mu_t v = tbl(t)->array[2*j+1];

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

            mu_dealloc(tbl(t)->array, 2*size * sizeof(mu_t));
            tbl(t)->array = array;
            tbl(t)->npw2 = npw2;
            tbl(t)->nils = 0;
        }
    }
}


// Inserts a value in the table with the given key
// without decending down the tail chain
void tbl_insert(mu_t t, mu_t k, mu_t v) {
    mu_assert(mu_istbl(t));
    if (!k) {
        mu_dec(v);
        return;
    }

    if (v) {
        tbl_expand(t);
    }

    muint_t mask = (1 << tbl(t)->npw2) - 1;

    if (tbl(t)->linear) {
        muint_t i = num_uint(k) & mask;

        if (k == muint(i)) {
            tbl_replace(t, &tbl(t)->array[i], v);
        } else if (v) {
            // Index is out of range, convert to full table
            tbl_realize(t);
            return tbl_insert(t, k, v);
        }
    } else {
        for (muint_t i = tbl_hash(k);; i++) {
            mu_t *p = &tbl(t)->array[2*(i & mask)];

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
void tbl_assign(mu_t head, mu_t k, mu_t v) {
    mu_assert(mu_istbl(head));
    if (!k) {
        mu_dec(v);
        return;
    }

    for (mu_t t = head; t; t = tbl(t)->tail) {
        muint_t mask = (1 << tbl(t)->npw2) - 1;

        if (tbl(t)->linear) {
            muint_t i = num_uint(k) & mask;

            if (k == muint(i) && tbl(t)->array[i]) {
                tbl_replace(t, &tbl(t)->array[i], v);
                return;
            }
        } else {
            for (muint_t i = tbl_hash(k);; i++) {
                mu_t *p = &tbl(t)->array[2*(i & mask)];

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

    if (v)
        tbl_insert(head, k, v);
    else
        mu_dec(k);
}


// Performs iteration on a table
bool tbl_next(mu_t t, muint_t *ip, mu_t *kp, mu_t *vp) {
    mu_assert(mu_istbl(t));
    muint_t i = *ip;
    mu_t k, v;

    do {
        if (i >= (1 << tbl(t)->npw2))
            return false;

        k = tbl(t)->linear ? muint(i)         : tbl(t)->array[2*i+0];
        v = tbl(t)->linear ? tbl(t)->array[i] : tbl(t)->array[2*i+1];
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
    tbl_dec(t);
    tbl_insert(scope, muint(1), muint(i));
    return next ? 1 : 0;
}

mu_t tbl_iter(mu_t t) {
    mu_assert(mu_istbl(t));
    return msbfn(0x0, tbl_iter_step, mlist({t, muint(0)}));
}

static mc_t tbl_pairs_step(mu_t scope, mu_t *frame) {
    mu_t t = tbl_lookup(scope, muint(0));
    muint_t i = num_uint(tbl_lookup(scope, muint(1)));

    bool next = tbl_next(t, &i, &frame[0], &frame[1]);
    tbl_dec(t);
    tbl_insert(scope, muint(1), muint(i));
    return next ? 2 : 0;
}

mu_t tbl_pairs(mu_t t) {
    mu_assert(mu_istbl(t));
    return msbfn(0x0, tbl_pairs_step, mlist({t, muint(0)}));
}


// Table creating functions
mu_t tbl_init_list(struct tbl *t, mu_t (*const *gen)(void), muint_t n) {
    memset(t, 0, sizeof(struct tbl));
    t->npw2 = tbl_npw2(true, n);
    t->linear = true;

    muint_t size = 1 << t->npw2;
    t->array = mu_alloc(size * sizeof(mu_t));
    memset(t->array, 0, size * sizeof(mu_t));

    mu_t m = (mu_t)((muint_t)t + MTTBL);

    for (muint_t i = 0; i < n; i++)
        tbl_insert(m, muint(i), gen[i]());

    return m;
}

mu_t tbl_init_tbl(struct tbl *t, mu_t (*const (*gen)[2])(void), muint_t n) {
    memset(t, 0, sizeof(struct tbl));
    t->npw2 = tbl_npw2(true, n);
    t->linear = true;

    muint_t size = 1 << t->npw2;
    t->array = mu_alloc(size * sizeof(mu_t));
    memset(t->array, 0, size * sizeof(mu_t));

    mu_t m = (mu_t)((muint_t)t + MTTBL);

    for (muint_t i = 0; i < n; i++)
        tbl_insert(m, gen[i][0](), gen[i][1]());

    return m;
}

mu_t tbl_fromntbl(mu_t (*pairs)[2], muint_t n) {
    mu_t t = tbl_create(n);

    for (muint_t i = 0; i < n; i++)
        tbl_insert(t, pairs[i][0], pairs[i][1]);

    return t;
}

mu_t tbl_fromnlist(mu_t *list, muint_t n) {
    mu_t t = tbl_create(n);

    for (muint_t i = 0; i < n; i++)
        tbl_insert(t, muint(i), list[i]);

    return t;
}

mu_t tbl_fromnum(mu_t n) {
    mu_assert(mu_isnum(n));
    return tbl_create(num_uint(n));
}

mu_t tbl_fromiter(mu_t i) {
    mu_t frame[MU_FRAME];
    mu_t t = tbl_create(0);
    muint_t index = 0;

    while (fn_next(i, 0x2, frame)) {
        if (frame[1])
            tbl_insert(t, frame[0], frame[1]);
        else 
            tbl_insert(t, muint(index++), frame[0]);
    }

    fn_dec(i);
    return t;
}


// Data structure operations
void tbl_push(mu_t t, mu_t v, mu_t i) {
    mu_assert(mu_istbl(t) && (!i || mu_isnum(i)));

    if (!i)
        i = muint(tbl_len(t));
    else if (num_cmp(i, muint(0)) < 0)
        i = num_add(i, muint(tbl_len(t)));

    if (tbl(t)->linear) {
        tbl_expand(t);
        muint_t size = 1 << tbl(t)->npw2;

        if (num_cmp(i, muint(size)) <= 0) {
            muint_t j = num_uint(i);

            memmove(&tbl(t)->array[j+1], &tbl(t)->array[j], 
                    (size-j)*sizeof(mu_t));
            tbl(t)->array[j] = 0;
        }
    } else {
        mu_t d = tbl_create(tbl(t)->len);
        mu_t k, v;

        for (muint_t j = 0; tbl_next(t, &j, &k, &v);) {
            if (mu_isnum(k) && num_cmp(k, i) >= 0) {
                tbl_insert(d, num_add(k, muint(1)), v);
                tbl_insert(t, k, 0);
            } else {
                mu_dec(k);
                mu_dec(v);
            }
        }

        for (muint_t j = 0; tbl_next(d, &j, &k, &v);) {
            tbl_insert(t, k, v);
        }

        tbl_dec(d);
    }

    tbl_insert(t, i, v);
}

mu_t tbl_pop(mu_t t, mu_t i) {
    mu_assert(mu_istbl(t) && (!i || mu_isnum(i)));

    if (!i)
        i = muint(tbl_len(t)-1);
    else if (num_cmp(i, muint(0)) < 0)
        i = num_add(i, muint(tbl_len(t)));

    mu_t ret = tbl_lookup(t, mu_inc(i));
    tbl_insert(t, i, 0);

    if (tbl(t)->linear) {
        muint_t size = 1 << tbl(t)->npw2;

        if (num_cmp(i, muint(size)) < 0) {
            muint_t j = num_uint(i);

            memmove(&tbl(t)->array[j], &tbl(t)->array[j+1], 
                    (size-(j+1))*sizeof(mu_t));
            tbl(t)->array[size-1] = 0;
        }
    } else {
        mu_t d = tbl_create(tbl(t)->len);
        mu_t k, v;

        for (muint_t j = 0; tbl_next(t, &j, &k, &v);) {
            if (mu_isnum(k) && num_cmp(k, i) > 0) {
                tbl_insert(d, num_sub(k, muint(1)), v);
                tbl_insert(t, k, 0);
            } else {
                mu_dec(k);
                mu_dec(v);
            }
        }

        for (muint_t j = 0; tbl_next(d, &j, &k, &v);) {
            tbl_insert(t, k, v);
        }

        tbl_dec(d);
    }

    return ret;
}

mu_t tbl_concat(mu_t a, mu_t b, mu_t offset) {
    mu_assert(mu_istbl(a) && mu_istbl(b) 
              && (!offset || mu_isnum(offset)));

    if (!offset)
        offset = muint(tbl_len(a));
    else if (num_cmp(offset, muint(0)) < 0)
        offset = num_add(offset, muint(tbl_len(a)));

    mu_t d = tbl_create(tbl_len(a) + tbl_len(b));
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
    mu_assert(mu_istbl(t) && mu_isnum(lower)
              && (!upper || mu_isnum(upper)));

    if (num_cmp(lower, muint(0)) < 0)
        lower = num_add(lower, muint(tbl_len(t)));

    if (!upper)
        upper = num_add(lower, muint(1));
    else if (num_cmp(upper, muint(0)) < 0)
        upper = num_add(upper, muint(tbl_len(t)));

    mu_t d = tbl_create(tbl_len(t));
    mu_t k, v;

    for (muint_t i = 0; tbl_next(t, &i, &k, &v);) {
        if (mu_isnum(k) && num_cmp(k, lower) >= 0
                        && num_cmp(k, upper) < 0) {
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
    mu_assert(mu_istbl(a) && mu_istbl(b));
    mlen_t alen = tbl_len(a);
    mlen_t blen = tbl_len(b);
    mu_t d = tbl_create(alen < blen ? alen : blen);
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
    mu_assert(mu_istbl(a) && mu_istbl(b));
    mu_t d = tbl_create(tbl_len(a) + tbl_len(b));
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
    mu_assert(mu_istbl(a) && mu_istbl(b));
    mlen_t alen = tbl_len(a);
    mlen_t blen = tbl_len(b);
    mu_t d = tbl_create(alen > blen ? alen : blen);
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
    mu_assert(mu_istbl(a) && mu_istbl(b));
    mu_t d = tbl_create(tbl_len(a));
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
    mu_t t = tbl_create(0);
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
        mu_errorf("unterminated table literal");

    *ppos = pos;
    return t;    
}

static void tbl_dump_nested(mu_t t, mu_t *s, muint_t *n, 
                            mu_t depth, mu_t indent, muint_t nest) {
    bool linear = tbl(t)->linear;
    for (muint_t i = 0; linear && i < tbl_len(t); i++) {
        if (!tbl(t)->array[i])
            linear = false;
    }

    buf_push(s, n, '[');

    mu_t k, v;
    for (muint_t i = 0; tbl_next(t, &i, &k, &v);) {
        if (indent) {
            muint_t nest_indent = nest * num_uint(indent);
            buf_push(s, n, '\n');
            for (muint_t j = 0; j < nest_indent; j++)
                buf_push(s, n, ' ');
        }

        if (!linear) {
            buf_concat(s, n, mu_repr(k));
            buf_format(s, n, ": ");
        } else {
            mu_dec(k);
        }

        if (mu_istbl(v) && num_cmp(depth, muint(0)) > 0) {
            tbl_dump_nested(v, s, n,
                            num_sub(depth, muint(1)),
                            indent, nest + 1);
        } else {
            buf_concat(s, n, mu_repr(v));
        }

        buf_format(s, n, indent ? "," : ", ");
    }

    if (tbl_len(t) > 0) {
        *n -= indent ? 1 : 2;

        if (indent) {
            muint_t nest_indent = (nest-1) * num_uint(indent);
            buf_push(s, n, '\n');
            for (muint_t j = 0; j < nest_indent; j++)
                buf_push(s, n, ' ');
        }
    }

    buf_push(s, n, ']');
    tbl_dec(t);
}

mu_t tbl_dump(mu_t t, mu_t depth, mu_t indent) {
    mu_assert(mu_istbl(t) && (!depth || mu_isnum(depth))
                          && (!indent || mu_isnum(indent)));

    if (!depth || num_cmp(depth, muint(0)) <= 0)
        return mu_addr(t);

    mu_t s = buf_create(0);
    muint_t n = 0;

    tbl_dump_nested(t, &s, &n, 
                    num_sub(depth, muint(1)), 
                    indent, 1);

    return str_intern(s, n);
}
