#include "tbl.h"

#include "num.h"
#include "str.h"
#include "fn.h"
#include "parse.h"


// Table access
mu_inline struct mtbl *mtbl(mu_t t) {
    return (struct mtbl *)((muint_t)t - MTTBL);
}


// General purpose hash for mu types
mu_inline muint_t mu_tbl_hash(mu_t m) {
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
mu_inline muint_t mu_tbl_nsize(muint_t s) {
    return s + (s >> 1);
}

// Finds next power of 2 after checks for minimum bounds
// and growing capacity based on load factor
mu_inline muintq_t mu_tbl_npw2(bool linear, muint_t s) {
    if (!linear) {
        s = mu_tbl_nsize(s);
    }

    if (linear && s < (MU_MINALLOC/sizeof(mu_t))) {
        s = MU_MINALLOC/sizeof(mu_t);
    } else if (!linear && s < (MU_MINALLOC/sizeof(mu_t[2]))) {
        s = MU_MINALLOC/sizeof(mu_t[2]);
    }

    return mu_npw2(s);
}


// Functions for managing tables
mu_t mu_tbl_create(muint_t len) {
    struct mtbl *t = mu_ref_alloc(sizeof(struct mtbl));

    t->npw2 = mu_tbl_npw2(true, len);
    t->flags = MTBL_LINEAR;
    t->len = 0;
    t->tail = 0;

    muint_t size = 1 << t->npw2;
    t->array = mu_alloc(size * sizeof(mu_t));
    memset(t->array, 0, size * sizeof(mu_t));

    return (mu_t)((muint_t)t + MTTBL);
}

mu_t mu_tbl_extend(muint_t len, mu_t tail) {
    mu_assert(!tail || mu_istbl(tail));
    mu_t t = mu_tbl_create(len);
    mtbl(t)->tail = tail;
    return t;
}

void mu_tbl_settail(mu_t t, mu_t tail) {
    mu_assert(!tail || mu_istbl(tail));
    mtbl(t)->tail = tail;
}

void mu_tbl_destroy(mu_t t) {
    muint_t size = 1 << (mtbl(t)->npw2 + !(mtbl(t)->flags & MTBL_LINEAR));

    for (muint_t i = 0; i < size; i++) {
        mu_dec(mtbl(t)->array[i]);
    }

    mu_dealloc(mtbl(t)->array, size * sizeof(mu_t));
    mu_dec(mtbl(t)->tail);
    mu_ref_dealloc(t, sizeof(struct mtbl));
}


// Recursively looks up a key in the table
// returns either that value or nil
mu_t mu_tbl_lookup(mu_t t, mu_t k) {
    mu_assert(mu_istbl(t));
    if (!k) {
        return 0;
    }

    for (; t; t = mtbl(t)->tail) {
        muint_t mask = (1 << mtbl(t)->npw2) - 1;

        if (mtbl(t)->flags & MTBL_LINEAR) {
            muint_t i = mu_num_getuint(k) & mask;

            if (k == mu_num_fromuint(i) && mtbl(t)->array[i]) {
                return mu_inc(mtbl(t)->array[i]);
            }
        } else {
            for (muint_t i = mu_tbl_hash(k);; i++) {
                mu_t *p = &mtbl(t)->array[2*(i & mask)];

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
static void mu_tbl_place(mu_t t, mu_t *dp, mu_t v) {
    if ((muint_t)mtbl(t)->len + 1 > (mlen_t)-1) {
        mu_errorf("exceeded max length in table");
    }

    mtbl(t)->len += 1;
    *dp = v;
}

static void mu_tbl_replace(mu_t t, mu_t *dp, mu_t v) {
    mu_t d = *dp;

    if (v && !d) {
        if ((muint_t)mtbl(t)->len + 1 > (mlen_t)-1) {
            mu_errorf("exceeded max length in table");
        }

        mtbl(t)->len += 1;
        mtbl(t)->nils -= 1;
    } else if (!v && d) {
        mtbl(t)->len -= 1;
        mtbl(t)->nils += 1;
    }

    // We must replace before decrementing in case destructors run
    *dp = v;
    mu_dec(d);
}

// Converts from array to full table
static void mu_tbl_realize(mu_t t) {
    muint_t size = 1 << mtbl(t)->npw2;
    muintq_t npw2 = mu_tbl_npw2(false, mtbl(t)->len + 1);
    muint_t nsize = 1 << npw2;
    muint_t mask = nsize - 1;
    mu_t *array = mu_alloc(2*nsize * sizeof(mu_t));
    memset(array, 0, 2*nsize * sizeof(mu_t));

    for (muint_t j = 0; j < size; j++) {
        mu_t k = mu_num_fromuint(j);
        mu_t v = mtbl(t)->array[j];

        if (!v) {
            continue;
        }

        for (muint_t i = mu_tbl_hash(k);; i++) {
            mu_t *p = &array[2*(i & mask)];

            if (!p[0]) {
                p[0] = k;
                p[1] = v;
                break;
            }
        }
    }

    mu_dealloc(mtbl(t)->array, size * sizeof(mu_t));
    mtbl(t)->array = array;
    mtbl(t)->npw2 = npw2;
    mtbl(t)->nils = 0;
    mtbl(t)->flags &= ~MTBL_LINEAR;
}

// Make room for an additional element
static void mu_tbl_expand(mu_t t) {
    muint_t size = 1 << mtbl(t)->npw2;

    if (mtbl(t)->flags & MTBL_LINEAR) {
        if (mtbl(t)->len + 1 > size) {
            muintq_t npw2 = mu_tbl_npw2(true, mtbl(t)->len + 1);
            muint_t nsize = 1 << npw2;
            mu_t *array = mu_alloc(nsize * sizeof(mu_t));
            memcpy(array, mtbl(t)->array, size * sizeof(mu_t));
            memset(array+size, 0, (nsize-size) * sizeof(mu_t));

            mu_dealloc(mtbl(t)->array, size * sizeof(mu_t));
            mtbl(t)->array = array;
            mtbl(t)->npw2 = npw2;
        }
    } else {
        if (mu_tbl_nsize(mtbl(t)->len + mtbl(t)->nils + 1) > size) {
            muintq_t npw2 = mu_tbl_npw2(false, mtbl(t)->len + 1);
            muint_t nsize = 1 << npw2;
            muint_t mask = nsize - 1;
            mu_t *array = mu_alloc(2*nsize * sizeof(mu_t));
            memset(array, 0, 2*nsize * sizeof(mu_t));

            for (muint_t j = 0; j < size; j++) {
                mu_t k = mtbl(t)->array[2*j+0];
                mu_t v = mtbl(t)->array[2*j+1];

                if (!k || !v) {
                    continue;
                }

                for (muint_t i = mu_tbl_hash(k);; i++) {
                    mu_t *p = &array[2*(i & mask)];

                    if (!p[0]) {
                        p[0] = k;
                        p[1] = v;
                        break;
                    }
                }
            }

            mu_dealloc(mtbl(t)->array, 2*size * sizeof(mu_t));
            mtbl(t)->array = array;
            mtbl(t)->npw2 = npw2;
            mtbl(t)->nils = 0;
        }
    }
}


// Inserts a value in the table with the given key
// without decending down the tail chain
void mu_tbl_insert(mu_t t, mu_t k, mu_t v) {
    mu_assert(mu_istbl(t));
    if (!k) {
        mu_dec(v);
        return;
    }

    if (v) {
        mu_tbl_expand(t);
    }

    muint_t mask = (1 << mtbl(t)->npw2) - 1;

    if (mtbl(t)->flags & MTBL_LINEAR) {
        muint_t i = mu_num_getuint(k) & mask;

        if (k == mu_num_fromuint(i)) {
            mu_tbl_replace(t, &mtbl(t)->array[i], v);
        } else if (v) {
            // Index is out of range, convert to full table
            mu_tbl_realize(t);
            mu_tbl_insert(t, k, v);
        }
    } else {
        for (muint_t i = mu_tbl_hash(k);; i++) {
            mu_t *p = &mtbl(t)->array[2*(i & mask)];

            if (!p[0] && v) {
                p[0] = k;
                mu_tbl_place(t, &p[1], v);
                return;
            } else if (!p[0]) {
                mu_dec(k);
                return;
            } else if (k == p[0]) {
                mu_dec(k);
                mu_tbl_replace(t, &p[1], v);
                return;
            }
        }
    }
}

// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
void mu_tbl_assign(mu_t head, mu_t k, mu_t v) {
    mu_assert(mu_istbl(head));
    if (!k) {
        mu_dec(v);
        return;
    }

    for (mu_t t = head; t; t = mtbl(t)->tail) {
        muint_t mask = (1 << mtbl(t)->npw2) - 1;

        if (mtbl(t)->flags & MTBL_LINEAR) {
            muint_t i = mu_num_getuint(k) & mask;

            if (k == mu_num_fromuint(i) && mtbl(t)->array[i]) {
                mu_tbl_replace(t, &mtbl(t)->array[i], v);
                return;
            }
        } else {
            for (muint_t i = mu_tbl_hash(k);; i++) {
                mu_t *p = &mtbl(t)->array[2*(i & mask)];

                if (!p[0] || (k == p[0] && !p[1])) {
                    break;
                } else if (k == p[0]) {
                    mu_dec(k);
                    mu_tbl_replace(t, &p[1], v);
                    return;
                }
            }
        }
    }

    if (v) {
        mu_tbl_insert(head, k, v);
    } else {
        mu_dec(k);
    }
}


// Performs iteration on a table
bool mu_tbl_next(mu_t t, muint_t *ip, mu_t *kp, mu_t *vp) {
    mu_assert(mu_istbl(t));
    muint_t i = *ip;
    mu_t k, v;

    do {
        if (i >= (1 << mtbl(t)->npw2)) {
            return false;
        }

        if (mtbl(t)->flags & MTBL_LINEAR) {
            k = mu_num_fromuint(i);
            v = mtbl(t)->array[i];
        } else {
            k = mtbl(t)->array[2*i+0];
            v = mtbl(t)->array[2*i+1];
        }

        i++;
    } while (!k || !v);

    if (kp) *kp = mu_inc(k);
    if (vp) *vp = mu_inc(v);
    *ip = i;
    return true;
}

static mcnt_t mu_tbl_iter_step(mu_t scope, mu_t *frame) {
    mu_t t = mu_tbl_lookup(scope, mu_num_fromuint(0));
    muint_t i = mu_num_getuint(mu_tbl_lookup(scope, mu_num_fromuint(1)));

    bool next = mu_tbl_next(t, &i, 0, &frame[0]);
    mu_tbl_dec(t);
    mu_tbl_insert(scope, mu_num_fromuint(1), mu_num_fromuint(i));
    return next ? 1 : 0;
}

mu_t mu_tbl_iter(mu_t t) {
    mu_assert(mu_istbl(t));
    return mu_fn_fromsbfn(0x0, mu_tbl_iter_step,
            mu_tbl_fromlist((mu_t[]){t, mu_num_fromuint(0)}, 2));
}

static mcnt_t mu_tbl_pairs_step(mu_t scope, mu_t *frame) {
    mu_t t = mu_tbl_lookup(scope, mu_num_fromuint(0));
    muint_t i = mu_num_getuint(mu_tbl_lookup(scope, mu_num_fromuint(1)));

    bool next = mu_tbl_next(t, &i, &frame[0], &frame[1]);
    mu_tbl_dec(t);
    mu_tbl_insert(scope, mu_num_fromuint(1), mu_num_fromuint(i));
    return next ? 2 : 0;
}

mu_t mu_tbl_pairs(mu_t t) {
    mu_assert(mu_istbl(t));
    return mu_fn_fromsbfn(0x0, mu_tbl_pairs_step,
        mu_tbl_fromlist((mu_t[]){t, mu_num_fromuint(0)}, 2));
}


// Table creating functions
mu_t mu_tbl_initlist(struct mtbl *t, mu_t (*const *gen)(void), muint_t n) {
    memset(t, 0, sizeof(struct mtbl));
    t->npw2 = mu_tbl_npw2(true, n);
    t->flags = MTBL_LINEAR;

    muint_t size = 1 << t->npw2;
    t->array = mu_alloc(size * sizeof(mu_t));
    memset(t->array, 0, size * sizeof(mu_t));

    mu_t m = (mu_t)((muint_t)t + MTTBL);

    for (muint_t i = 0; i < n; i++) {
        mu_tbl_insert(m, mu_num_fromuint(i), gen[i]());
    }

    return m;
}

mu_t mu_tbl_initpairs(struct mtbl *t, mu_t (*const (*gen)[2])(void), muint_t n) {
    memset(t, 0, sizeof(struct mtbl));
    t->npw2 = mu_tbl_npw2(true, n);
    t->flags = MTBL_LINEAR;

    muint_t size = 1 << t->npw2;
    t->array = mu_alloc(size * sizeof(mu_t));
    memset(t->array, 0, size * sizeof(mu_t));

    mu_t m = (mu_t)((muint_t)t + MTTBL);

    for (muint_t i = 0; i < n; i++) {
        mu_tbl_insert(m, gen[i][0](), gen[i][1]());
    }

    return m;
}

mu_t mu_tbl_fromlist(mu_t *list, muint_t n) {
    mu_t t = mu_tbl_create(n);

    for (muint_t i = 0; i < n; i++) {
        mu_tbl_insert(t, mu_num_fromuint(i), list[i]);
    }

    return t;
}

mu_t mu_tbl_frompairs(mu_t (*pairs)[2], muint_t n) {
    mu_t t = mu_tbl_create(n);

    for (muint_t i = 0; i < n; i++) {
        mu_tbl_insert(t, pairs[i][0], pairs[i][1]);
    }

    return t;
}

mu_t mu_tbl_fromiter(mu_t i) {
    mu_t frame[MU_FRAME];
    mu_t t = mu_tbl_create(0);
    muint_t index = 0;

    while (mu_fn_next(i, 0x2, frame)) {
        if (frame[1]) {
            mu_tbl_insert(t, frame[0], frame[1]);
        } else {
            mu_tbl_insert(t, mu_num_fromuint(index++), frame[0]);
        }
    }

    mu_fn_dec(i);
    return t;
}

// Table operations
void mu_tbl_push(mu_t t, mu_t p, mint_t i) {
    mu_assert(mu_istbl(t));
    i = (i >= 0) ? i : i + mtbl(t)->len;

    mu_tbl_expand(t);
    mint_t size = 1 << mtbl(t)->npw2;

    if ((mtbl(t)->flags & MTBL_LINEAR) && i >= 0 && i < size) {
        memmove(&mtbl(t)->array[i+1], &mtbl(t)->array[i],
                    (size-i-1)*sizeof(mu_t));
        mu_tbl_place(t, &mtbl(t)->array[i], p);
    } else {
        mu_t d = mu_tbl_create(mtbl(t)->len);
        mu_t k, v;

        for (muint_t j = 0; mu_tbl_next(t, &j, &k, &v);) {
            if (mu_isnum(k) && mu_num_cmp(k, mu_num_fromint(i)) >= 0) {
                mu_tbl_insert(d, mu_num_add(k, mu_num_fromuint(1)), v);
                mu_tbl_insert(t, k, 0);
            } else {
                mu_dec(k);
                mu_dec(v);
            }
        }

        for (muint_t j = 0; mu_tbl_next(d, &j, &k, &v);) {
            mu_tbl_insert(t, k, v);
        }

        mu_tbl_dec(d);
        mu_tbl_insert(t, mu_num_fromint(i), p);
    }
}

mu_t mu_tbl_pop(mu_t t, mint_t i) {
    mu_assert(mu_istbl(t));
    i = (i >= 0) ? i : i + mtbl(t)->len;

    mint_t size = 1 << mtbl(t)->npw2;

    if ((mtbl(t)->flags & MTBL_LINEAR) && i >= 0 && i < size) {
        mu_t p = mtbl(t)->array[i];
        mu_tbl_replace(t, &mtbl(t)->array[i], 0);
        memmove(&mtbl(t)->array[i], &mtbl(t)->array[i+1],
                (size-(i+1))*sizeof(mu_t));
        mtbl(t)->array[size-1] = 0;
        return p;
    } else {
        mu_t p = mu_tbl_lookup(t, mu_num_fromint(i));
        mu_tbl_insert(t, mu_num_fromint(i), 0);

        mu_t d = mu_tbl_create(mtbl(t)->len);
        mu_t k, v;

        for (muint_t j = 0; mu_tbl_next(t, &j, &k, &v);) {
            if (mu_isnum(k) && mu_num_cmp(k, mu_num_fromint(i)) > 0) {
                mu_tbl_insert(d, mu_num_sub(k, mu_num_fromuint(1)), v);
                mu_tbl_insert(t, k, 0);
            } else {
                mu_dec(k);
                mu_dec(v);
            }
        }

        for (muint_t j = 0; mu_tbl_next(d, &j, &k, &v);) {
            mu_tbl_insert(t, k, v);
        }

        mu_tbl_dec(d);
        return p;
    }
}

mu_t mu_tbl_concat(mu_t a, mu_t b, mu_t offset) {
    mu_assert(mu_istbl(a) && mu_istbl(b)
              && (!offset || mu_isnum(offset)));

    if (!offset) {
        offset = mu_num_fromuint(mu_tbl_getlen(a));
    } else if (mu_num_cmp(offset, mu_num_fromuint(0)) < 0) {
        offset = mu_num_add(offset, mu_num_fromuint(mu_tbl_getlen(a)));
    }

    mu_t d = mu_tbl_create(mu_tbl_getlen(a) + mu_tbl_getlen(b));
    mu_t k, v;

    for (muint_t i = 0; mu_tbl_next(a, &i, &k, &v);) {
        mu_tbl_insert(d, k, v);
    }

    for (muint_t i = 0; mu_tbl_next(b, &i, &k, &v);) {
        if (mu_isnum(k)) {
            mu_tbl_insert(d, mu_num_add(k, offset), v);
        } else {
            mu_tbl_insert(d, k, v);
        }
    }

    mu_tbl_dec(a);
    mu_tbl_dec(b);
    return d;
}

mu_t mu_tbl_subset(mu_t t, mint_t lower, mint_t upper) {
    mu_assert(mu_istbl(t));
    lower = (lower >= 0) ? lower : lower + mu_tbl_getlen(t);
    upper = (upper >= 0) ? upper : upper + mu_tbl_getlen(t);

    if (lower < 0) {
        lower = 0;
    }

    if (lower > upper) {
        mu_dec(t);
        return mu_tbl_create(0);
    }

    muint_t len = (upper-lower < mu_tbl_getlen(t)) ?
            upper-lower : mu_tbl_getlen(t);
    mu_t d = mu_tbl_create(len);
    mu_t k, v;

    for (muint_t i = 0; mu_tbl_next(t, &i, &k, &v);) {
        if (mu_isnum(k) && mu_num_cmp(k, mu_num_fromint(lower)) >= 0
                        && mu_num_cmp(k, mu_num_fromint(upper)) < 0) {
            mu_tbl_insert(d, mu_num_sub(k, mu_num_fromint(lower)), v);
        } else {
            mu_dec(k);
            mu_dec(v);
        }
    }

    mu_dec(t);
    return d;
}

// Set operations
mu_t mu_tbl_and(mu_t a, mu_t b) {
    mu_assert(mu_istbl(a) && mu_istbl(b));
    mlen_t alen = mu_tbl_getlen(a);
    mlen_t blen = mu_tbl_getlen(b);
    mu_t d = mu_tbl_create(alen < blen ? alen : blen);
    mu_t k, v;

    for (muint_t i = 0; mu_tbl_next(a, &i, &k, &v);) {
        mu_t w = mu_tbl_lookup(b, mu_inc(k));
        if (w) {
            mu_dec(w);
            mu_tbl_insert(d, k, v);
        } else {
            mu_dec(k);
            mu_dec(v);
        }
    }

    mu_tbl_dec(a);
    mu_tbl_dec(b);
    return d;
}

mu_t mu_tbl_or(mu_t a, mu_t b) {
    mu_assert(mu_istbl(a) && mu_istbl(b));
    mu_t d = mu_tbl_create(mu_tbl_getlen(a) + mu_tbl_getlen(b));
    mu_t k, v;

    for (muint_t i = 0; mu_tbl_next(b, &i, &k, &v);) {
        mu_tbl_insert(d, k, v);
    }

    for (muint_t i = 0; mu_tbl_next(a, &i, &k, &v);) {
        mu_tbl_insert(d, k, v);
    }

    mu_tbl_dec(a);
    mu_tbl_dec(b);
    return d;
}

mu_t mu_tbl_xor(mu_t a, mu_t b) {
    mu_assert(mu_istbl(a) && mu_istbl(b));
    mlen_t alen = mu_tbl_getlen(a);
    mlen_t blen = mu_tbl_getlen(b);
    mu_t d = mu_tbl_create(alen > blen ? alen : blen);
    mu_t k, v;

    for (muint_t i = 0; mu_tbl_next(a, &i, &k, &v);) {
        mu_t w = mu_tbl_lookup(b, mu_inc(k));
        if (!w) {
            mu_tbl_insert(d, k, v);
        } else {
            mu_dec(k);
            mu_dec(v);
            mu_dec(w);
        }
    }

    for (muint_t i = 0; mu_tbl_next(b, &i, &k, &v);) {
        mu_t w = mu_tbl_lookup(a, mu_inc(k));
        if (!w) {
            mu_tbl_insert(d, k, v);
        } else {
            mu_dec(k);
            mu_dec(v);
            mu_dec(w);
        }
    }

    mu_tbl_dec(a);
    mu_tbl_dec(b);
    return d;
}

mu_t mu_tbl_diff(mu_t a, mu_t b) {
    mu_assert(mu_istbl(a) && mu_istbl(b));
    mu_t d = mu_tbl_create(mu_tbl_getlen(a));
    mu_t k, v;

    for (muint_t i = 0; mu_tbl_next(a, &i, &k, &v);) {
        mu_t w = mu_tbl_lookup(b, mu_inc(k));
        if (!w) {
            mu_tbl_insert(d, k, v);
        } else {
            mu_dec(k);
            mu_dec(v);
            mu_dec(w);
        }
    }

    mu_tbl_dec(a);
    mu_tbl_dec(b);
    return d;
}


// String representation
mu_t mu_tbl_parse(const mbyte_t **ppos, const mbyte_t *end) {
    const mbyte_t *pos = *ppos;
    mu_t t = mu_tbl_create(0);
    mu_t i = mu_num_fromuint(0);

    while (pos < end && *pos != ']') {
        mu_t k = mu_nparse(&pos, end);

        if (pos < end && *pos == ':') {
            pos++;
            mu_t v = mu_nparse(&pos, end);
            mu_tbl_insert(t, k, v);
        } else {
            mu_tbl_insert(t, i, k);
            i = mu_num_add(i, mu_num_fromuint(1));
        }

        if (pos == end || *pos != ',') {
            break;
        }

        pos++;
    }

    if (pos == end || *pos++ != ']') {
        mu_errorf("unterminated table literal");
    }

    *ppos = pos;
    return t;
}

static void mu_tbl_dump_nested(mu_t t, mu_t *s, muint_t *n, mu_t depth) {
    if (mu_num_cmp(depth, mu_num_fromuint(0)) <= 0) {
        mu_buf_format(s, n, "%nr", t, 0);
        return;
    }

    bool linear = mtbl(t)->flags & MTBL_LINEAR;
    for (muint_t i = 0; linear && i < mu_tbl_getlen(t); i++) {
        if (!mtbl(t)->array[i]) {
            linear = false;
        }
    }

    mu_buf_push(s, n, '[');

    mu_t k, v;
    for (muint_t i = 0; mu_tbl_next(t, &i, &k, &v);) {
        if (!linear) {
            mu_buf_format(s, n, "%nr: ", k, 0);
        } else {
            mu_dec(k);
        }

        if (mu_istbl(v)) {
            mu_tbl_dump_nested(v, s, n, mu_num_sub(depth, mu_num_fromuint(1)));
            mu_buf_format(s, n, ", ");
        } else {
            mu_buf_format(s, n, "%r, ", v);
        }
    }

    if (mu_tbl_getlen(t) > 0) {
        *n -= 2;
    }

    mu_buf_push(s, n, ']');
    mu_tbl_dec(t);
}

mu_t mu_tbl_dump(mu_t t, mu_t depth) {
    mu_assert(mu_istbl(t) && (!depth || mu_isnum(depth)));

    if (!depth) {
        depth = mu_num_fromuint(1);
    }

    mu_t s = mu_buf_create(0);
    muint_t n = 0;

    mu_tbl_dump_nested(t, &s, &n, depth);

    return mu_str_intern(s, n);
}


// Table related Mu functions
static mcnt_t mu_bfn_tbl(mu_t *frame) {
    mu_t m    = frame[0];
    mu_t tail = frame[1];
    if (tail && !mu_istbl(tail)) {
        mu_error_arg(MU_KEY_TBL, 0x2, frame);
    }

    switch (mu_gettype(m)) {
        case MTNIL:
            frame[0] = mu_tbl_create(0);
            break;

        case MTNUM:
            frame[0] = mu_tbl_create(mu_num_getuint(m));
            break;

        case MTSTR:
            frame[0] = m;
            mu_fn_fcall(MU_ITER, 0x11, frame);
            frame[0] = mu_tbl_fromiter(frame[0]);
            return 1;

        case MTTBL:
            frame[0] = m;
            mu_fn_fcall(MU_PAIRS, 0x11, frame);
            frame[0] = mu_tbl_fromiter(frame[0]);
            return 1;

        case MTFN:
            frame[0] = mu_tbl_fromiter(m);
            break;

        default:
            mu_error_cast(MU_KEY_TBL, m);
    }

    mu_tbl_settail(frame[0], tail);
    return 1;
}

MU_GEN_STR(mu_gen_key_tbl, "tbl")
MU_GEN_BFN(mu_gen_tbl, 0x2, mu_bfn_tbl)

static mcnt_t mu_bfn_tail(mu_t *frame) {
    if (!mu_istbl(frame[0])) {
        mu_error_arg(MU_KEY_TAIL, 0x1, frame);
    }

    frame[0] = mu_tbl_gettail(frame[0]);
    return 1;
}

MU_GEN_STR(mu_gen_key_tail, "tail")
MU_GEN_BFN(mu_gen_tail, 0x1, mu_bfn_tail)
