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


// General purpose hash for mu types
mu_inline uint_t tbl_hash(mu_t m) {
    // Mu garuntees bitwise equality for Mu types, which has the very
    // nice property of being a free hash function.
    //
    // We remove the lower 3 bits, since they store the type, which 
    // generally doesn't vary in tables. And we xor the upper and lower 
    // halves so all bits can affect the range of the length type. 
    // This is the boundary on the table size so it's really the only 
    // part that matters.
    return ((uint_t)m >> (8*sizeof(len_t))) ^ ((uint_t)m >> 3);
}

// Finds next power of 2 and check for minimum bound
mu_inline uintq_t tbl_npw2(uint_t s) {
    if (s < (MU_MINALLOC/sizeof(mu_t)))
        s = (MU_MINALLOC/sizeof(mu_t));

    return mu_npw2(s);
}

// Finds capacity based on load factor of 2/3
mu_inline uint_t tbl_nsize(uint_t s) {
    return s + (s >> 1);
}


// Functions for managing tables
mu_t tbl_create(uint_t len) {
    struct tbl *t = ref_alloc(sizeof(struct tbl));

    t->npw2 = tbl_npw2(len);
    t->linear = true;
    t->len = 0;
    t->nils = 0;
    t->tail = 0;

    int size = 1 << t->npw2;
    t->array = mu_alloc(size * sizeof(mu_t));
    memset(t->array, 0, size * sizeof(mu_t));

    return mtbl(t);
}

mu_t tbl_extend(uint_t len, mu_t tail) {
    mu_t t = tbl_create(len);
    tbl_rtbl(t)->tail = tail;
    return t;
}

void tbl_destroy(mu_t m) {
    struct tbl *t = tbl_rtbl(m);
    int size = (t->linear ? 1 : 2) * (1 << t->npw2);

    for (int i = 0; i < size; i++)
        mu_dec(t->array[i]);

    mu_dealloc(t->array, size * sizeof(mu_t));
    ref_dealloc(t, sizeof(struct tbl));
}


// Recursively looks up a key in the table
// returns either that value or nil
mu_t tbl_lookup(mu_t m, mu_t k) {
    if (!k)
        return mnil;

    for (struct tbl *t = tbl_rtbl(m); t; t = tbl_rtbl(t->tail)) {
        uint_t mask = (1 << t->npw2) - 1;

        if (t->linear) {
            uint_t i = num_uint(k) & mask;

            if (k == muint(i))
                return t->array[i];

        } else {
            for (uint_t i = tbl_hash(k);; i++) {
                mu_t *p = &t->array[2*(i & mask)];

                if (!p[0] || (k == p[0] && !p[1]))
                    break;
                else if (k == p[0])
                    return p[1];
            }
        }
    }

    return mnil;
}


// Helper methods for modifying tables
mu_inline void tbl_inc_len(struct tbl *t) {
    if (mu_unlikely(t->len == MU_MAXLEN))
        mu_err_len();

    t->len++;
}

mu_inline void tbl_dec_len(struct tbl *t) {
    t->len--;
}

mu_inline void tbl_inc_nils(struct tbl *t) {
    tbl_dec_len(t);
    t->nils++;
}

mu_inline void tbl_dec_nils(struct tbl *t) {
    tbl_inc_len(t);
    t->nils--;
}


// Converts from array to full table
static void tbl_realize(struct tbl *t) {
    uint_t size = 1 << t->npw2;
    uintq_t npw2 = tbl_npw2(tbl_nsize(t->len + 1));
    uint_t nsize = 1 << npw2;
    uint_t mask = nsize - 1;
    mu_t *array = mu_alloc(2*nsize * sizeof(mu_t));
    memset(array, 0, 2*nsize * sizeof(mu_t));

    for (uint_t j = 0; j < size; j++) {
        mu_t k = muint(j);
        mu_t v = t->array[j];

        if (!v)
            continue;

        for (uint_t i = tbl_hash(k);; i++) {
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
    uint_t size = 1 << t->npw2;

    if (t->linear) {
        if (t->len + 1 > size) {
            uintq_t npw2 = tbl_npw2(t->len + 1);
            uint_t nsize = 1 << npw2;
            mu_t *array = mu_alloc(nsize * sizeof(mu_t));
            memcpy(array, t->array, size * sizeof(mu_t));
            memset(array+size, 0, (nsize-size) * sizeof(mu_t));

            mu_dealloc(t->array, size * sizeof(mu_t));
            t->array = array;
            t->npw2 = npw2;
        }
    } else {
        if (tbl_nsize(t->len + t->nils + 1) > size) {
            uintq_t npw2 = tbl_npw2(tbl_nsize(t->len + 1));
            uint_t nsize = 1 << npw2;
            uint_t mask = nsize - 1;
            mu_t *array = mu_alloc(2*nsize * sizeof(mu_t));
            memset(array, 0, 2*nsize * sizeof(mu_t));

            for (uint_t j = 0; j < size; j++) {
                mu_t k = t->array[2*j+0];
                mu_t v = t->array[2*j+1];

                if (!k || !v)
                    continue;

                for (uint_t i = tbl_hash(k);; i++) {
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
static void tbl_insert_nil(struct tbl *t, mu_t k) {
    uint_t mask = (1 << t->npw2) - 1;

    if (t->linear) {
        uint_t i = num_uint(k) & mask;

        if (k == muint(i) && t->array[i]) {
            tbl_dec_len(t);
            mu_dec(t->array[i]);
            t->array[i] = mnil;
        }
    } else {
        for (uint_t i = tbl_hash(k);; i++) {
            mu_t *p = &t->array[2*(i & mask)];

            if (!p[0]) {
                return;
            } else if (k == p[0]) {
                if (p[1]) tbl_inc_nils(t);
                mu_dec(p[1]);
                p[1] = mnil;
            }
        }
    }
}

static void tbl_insert_val(struct tbl *t, mu_t k, mu_t v) {
    // We expand just so we always have space to insert
    // Worst case we have some extra space a bit early, 
    // but at least we won't need to rehash what we're adding.
    tbl_expand(t);
    uint_t mask = (1 << t->npw2) - 1;

    if (t->linear) {
        uint_t i = num_uint(k) & mask;

        if (k == muint(i)) {
            if (!t->array[i]) tbl_inc_len(t);
            mu_dec(t->array[i]);
            t->array[i] = v;
            return;
        }

        // Index is out of range, convert to full table
        tbl_realize(t);
    }

    for (uint_t i = tbl_hash(k);; i++) {
        mu_t *p = &t->array[2*(i & mask)];

        if (!p[0]) {
            tbl_inc_len(t);
            p[0] = k;
            p[1] = v;
            return;
        } else if (k == p[0]) {
            if (!p[1]) tbl_dec_nils(t);
            mu_dec(p[1]);
            p[1] = v;
            return;
        }
    }
}

void tbl_insert(mu_t m, mu_t k, mu_t v) {
    if (k && v)
        return tbl_insert_val(tbl_wtbl(m), k, v);
    else if (k)
        return tbl_insert_nil(tbl_wtbl(m), k);
}


// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
static void tbl_assign_nil(struct tbl *t, mu_t k) {
    while (t->tail && !tbl_isro(t->tail)) {
        t = tbl_rtbl(t->tail);
        uint_t mask = (1 << t->npw2) - 1;

        if (t->linear) {
            uint_t i = num_uint(k) & mask;

            if (k == muint(i) && t->array[i]) {
                tbl_dec_len(t);
                mu_dec(t->array[i]);
                t->array[i] = mnil;
                return;
            }
        } else {
            for (uint_t i = tbl_hash(k);; i++) {
                mu_t *p = &t->array[2*(i & mask)];

                if (!p[0] && (k == p[0] && !p[1])) {
                    break;
                } else if (k == p[0]) {
                    tbl_inc_nils(t);
                    mu_dec(p[1]);
                    p[1] = mnil;
                }
            }
        }
    }
}

static void tbl_assign_val(struct tbl *t, mu_t k, mu_t v) {
    struct tbl *head = t;

    while (t->tail && !tbl_isro(t->tail)) {
        t = tbl_rtbl(t->tail);
        uint_t mask = (1 << t->npw2) - 1;

        if (t->linear) {
            uint_t i = num_uint(k) & mask;

            if (k == muint(i) && t->array[i]) {
                mu_dec(t->array[i]);
                t->array[i] = v;
                return;
            }
        } else {
            for (uint_t i = tbl_hash(k);; i++) {
                mu_t *p = &t->array[2*(i & mask)];

                if (!p[0] || (k == p[0] && !p[1])) {
                    break;
                } else if (k == p[0]) {
                    mu_dec(p[1]);
                    p[1] = v;
                    return;
                }
            }
        }
    }

    return tbl_insert_val(head, k, v);
}

void tbl_assign(mu_t m, mu_t k, mu_t v) {
    if (k && v)
        return tbl_assign_val(tbl_wtbl(m), k, v);
    else if (k)
        return tbl_assign_nil(tbl_wtbl(m), k);
}



// Performs iteration on a table
bool tbl_next(mu_t m, uint_t *ip, mu_t *kp, mu_t *vp) {
    struct tbl *t = tbl_rtbl(m);
    uint_t i = *ip;
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
    *kp = k;
    *vp = v;
    return true;
}           

static frame_t tbl_step(mu_t scope, mu_t *frame) {
    mu_t t = tbl_lookup(scope, muint(0));
    uint_t i = num_uint(tbl_lookup(scope, muint(1)));

    tbl_next(t, &i, &frame[0], &frame[1]);
    tbl_insert(scope, muint(0), muint(i));
    return 2;
}

mu_t tbl_iter(mu_t t) {
    mu_t scope = tbl_create(2);
    tbl_insert(scope, muint(0), t);
    tbl_insert(scope, muint(1), muint(0));

    return msbfn(0x00, tbl_step, scope);
}


// Returns a string representation of the table
mu_t tbl_repr(mu_t t) {
    byte_t *s = mstr_create(0);
    uint_t si = 0;
    uint_t ti = 0;
    mu_t k, v, r;

    bool linear = tbl_rtbl(t)->linear;
    for (int i = 0; linear && i < tbl_len(t); i++) {
        if (!tbl_rtbl(t)->array[i])
            linear = false;
    }

    mstr_insert(&s, &si, '[');

    while (tbl_next(t, &ti, &k, &v)) {
        if (!linear) {
            r = mu_repr(k);
            mstr_concat(&s, &si, r);
            mstr_cconcat(&s, &si, ": ");
            mu_dec(r);
        }

        r = mu_repr(v);
        mstr_concat(&s, &si, r);
        mstr_cconcat(&s, &si, ", ");
        mu_dec(r);
    }

    if (tbl_len(t) > 0)
        si -= 2;

    mstr_insert(&s, &si, ']');
    return mstr_intern(s, si);
}


// Array-like manipulation
mu_t tbl_concat(mu_t a, mu_t b, mu_t moffset) {
    uint_t offset = num_uint(moffset);
    if (moffset != muint(offset)) {
        offset = 0;

        mu_t k, v;
        for (uint_t i = 0; tbl_next(a, &i, &k, &v);) {
            uint_t n = num_uint(k);
            if (k == muint(n) && n+1 > offset)
                offset = n+1;
        }
    }

    mu_t res;
    if (mu_ref(a) == 1) {
        res = a;
    } else {
        res = tbl_create(tbl_len(a) + tbl_len(b));

        mu_t k, v;
        for (uint_t i = 0; tbl_next(a, &i, &k, &v);) {
            tbl_insert(res, k, v);
        }

        tbl_dec(a);
    }

    mu_t k, v;
    for (uint_t i = 0; tbl_next(b, &i, &k, &v);) {
        uint_t n = num_uint(k);
        if (k == muint(n))
            tbl_insert(res, muint(n+offset), v);
        else
            tbl_insert(res, k, v);
    }

    tbl_dec(b);

    return res;
}

mu_t tbl_pop(mu_t t, mu_t k) {
    if (!k)
        k = muint(tbl_len(t)-1);

    mu_t ret = tbl_lookup(t, k);
    tbl_insert(t, k, mnil);

    uint_t i = num_uint(k);
    if (k == muint(i)) {
        if (tbl_rtbl(t)->linear) {
            uint_t size = 1 << tbl_rtbl(t)->npw2;
            mu_t *array = tbl_rtbl(t)->array;

            memmove(&array[i], &array[i+1], (size - (i+1)) * sizeof(mu_t));
            array[size-1] = mnil;
        } else {
            mu_t temp = tbl_create(tbl_len(t));
            mu_t k, v;

            for (uint_t j = 0; tbl_next(t, &j, &k, &v);) {
                uint_t n = num_uint(k);
                if (k == muint(n) && n > i) {
                    tbl_insert(temp, muint(n-1), v);
                    tbl_insert(t, k, mnil);
                }
            }

            for (uint_t j = 0; tbl_next(temp, &j, &k, &v);) {
                tbl_insert(t, k, v);
            }
        }
    }

    return ret;
}            

void tbl_push(mu_t t, mu_t v, mu_t k) {
    if (!k)
        k = muint(tbl_len(t));

    uint_t i = num_uint(k);
    if (k == muint(i)) {
        if (tbl_rtbl(t)->linear) {
            tbl_expand(tbl_rtbl(t));
            uint_t size = 1 << tbl_rtbl(t)->npw2;
            mu_t *array = tbl_rtbl(t)->array;

            memmove(&array[i+1], &array[i], (size - i) * sizeof(mu_t));
        } else {
            mu_t temp = tbl_create(tbl_len(t));
            mu_t k, v;

            for (uint_t j = 0; tbl_next(t, &j, &k, &v);) {
                uint_t n = num_uint(k);
                if (k == muint(n) && n >= i) {
                    tbl_insert(temp, muint(n+1), v);
                    tbl_insert(t, k, mnil);
                }
            }

            for (uint_t j = 0; tbl_next(temp, &j, &k, &v);) {
                tbl_insert(t, k, v);
            }
        }
    }

    tbl_insert(t, k, v);
}

