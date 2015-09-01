#include "fn.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "parse.h"
#include "vm.h"
#include <string.h>
#include <stdarg.h>


// Internally used conversion between mu_t and struct tbl
mu_inline struct fn *fromfn(mu_t m) {
    return (struct fn *)(~7 & (muint_t)m);
}


// C Function creating functions and macros
mu_t mbfn(mc_t args, mbfn_t *bfn) {
    struct fn *f = ref_alloc(sizeof(struct fn));
    f->args = args;
    f->type = MU_BFN - MU_FN;
    f->closure = mnil;
    f->bfn = bfn;
    return (mu_t)((muint_t)f + MU_BFN);
}

mu_t msbfn(mc_t args, msbfn_t *sbfn, mu_t closure) {
    struct fn *f = ref_alloc(sizeof(struct fn));
    f->args = args;
    f->type = MU_SBFN - MU_FN;
    f->closure = closure;
    f->sbfn = sbfn;
    return (mu_t)((muint_t)f + MU_SBFN);
}

mu_t mfn(struct code *c, mu_t closure) {
    struct fn *f = ref_alloc(sizeof(struct fn));
    f->args = c->args;
    f->type = MU_FN - MU_FN;
    f->closure = closure;
    f->code = c;
    return (mu_t)((muint_t)f + MU_FN);
}


// Called by garbage collector to clean up
void bfn_destroy(mu_t m) {
    struct fn *f = fromfn(m);
    ref_dealloc(f, sizeof(struct fn));
}

void sbfn_destroy(mu_t m) {
    struct fn *f = fromfn(m);
    mu_dec(f->closure);
    ref_dealloc(f, sizeof(struct fn));
}

void fn_destroy(mu_t m) {
    struct fn *f = fromfn(m);
    mu_dec(f->closure);
    code_dec(f->code);
    ref_dealloc(f, sizeof(struct fn));
}

void code_destroy(struct code *c) {
    for (muint_t i = 0; i < c->icount; i++)
        mu_dec(c->data[i].imms);

    for (muint_t i = 0; i < c->fcount; i++)
        code_dec(c->data[c->icount+i].fns);

    ref_dealloc(c, mu_offset(struct code, data) +
                   c->icount*sizeof(mu_t) +
                   c->fcount*sizeof(struct code *) +
                   c->bcount);
}


// C interface for calling functions
mc_t bfn_tcall(mu_t m, mc_t fc, mu_t *frame) {
    mbfn_t *bfn = fromfn(m)->bfn;
    mu_fto(fromfn(m)->args, fc, frame);
    fn_dec(m);

    return bfn(frame);
}

mc_t sbfn_tcall(mu_t m, mc_t fc, mu_t *frame) {
    msbfn_t *sbfn = fromfn(m)->sbfn;
    mu_fto(fromfn(m)->args, fc, frame);

    mc_t rc = sbfn(fromfn(m)->closure, frame);
    fn_dec(m);
    return rc;
}

mc_t fn_tcall(mu_t m, mc_t fc, mu_t *frame) {
    struct code *c = fn_code(m);
    mu_t scope = tbl_create(c->scope, fn_closure(m));
    mu_fto(c->args, fc, frame);
    fn_dec(m);

    return mu_exec(c, scope, frame);
}


void bfn_fcall(mu_t m, mc_t fc, mu_t *frame) {
    mc_t rets = bfn_tcall(m, fc >> 4, frame);
    mu_fto(fc & 0xf, rets, frame);
}

void sbfn_fcall(mu_t m, mc_t fc, mu_t *frame) {
    mc_t rets = sbfn_tcall(m, fc >> 4, frame);
    mu_fto(fc & 0xf, rets, frame);
}

void fn_fcall(mu_t m, mc_t fc, mu_t *frame) {
    mc_t rets = fn_tcall(m, fc >> 4, frame);
    mu_fto(fc & 0xf, rets, frame);
}

// Default function
static mc_t fn_identity(mu_t *frame) { return 0xf; }

mu_const mu_t fn_id(void) {
    return mcfn(0xf, fn_identity);
}

// Binds arguments to function
static mc_t fn_bound(mu_t scope, mu_t *frame) {
    mu_t f = tbl_lookup(scope, muint(0));
    mu_t args = tbl_lookup(scope, muint(1));

    frame[0] = tbl_concat(args, frame[0], muint(mu_len(args)));
    return mu_tcall(f, 0xf, frame);
}

mu_t fn_bind(mu_t f, mu_t args) {
    return msbfn(0xf, fn_bound, mtbl({
        { muint(0), f },
        { muint(1), args }
    }));
}

static mc_t fn_composed(mu_t fs, mu_t *frame) {
    mc_t c = 0xf;

    for (muint_t i = tbl_len(fs)-1; i+1 > 0; i--) {
        mu_t f = tbl_lookup(fs, muint(i));
        c = mu_tcall(f, c, frame);
    }

    return c;
}

mu_t fn_comp(mu_t fs) {
    return msbfn(0xf, fn_composed, fs);
}
    

// Functions over iterators
static mc_t fn_map_step(mu_t scope, mu_t *frame) {
    mu_t f = tbl_lookup(scope, muint(0));
    mu_t i = tbl_lookup(scope, muint(1));

    while (true) {
        mu_fcall(mu_inc(i), 0x0f, frame);
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            tbl_dec(frame[0]);
            mu_dec(f);
            mu_dec(i);
            return 0;
        }
        mu_dec(m);

        mu_fcall(mu_inc(f), 0xff, frame);
        m = tbl_lookup(frame[0], muint(0));
        if (m) {
            mu_dec(m);
            mu_dec(f);
            mu_dec(i);
            return 0xf;
        }
        tbl_dec(frame[0]);
    }
}

mu_t fn_map(mu_t f, mu_t iter) {
    return msbfn(0, fn_map_step, mtbl({
        { muint(0), f },
        { muint(1), iter }
    }));
}

static mc_t fn_filter_step(mu_t scope, mu_t *frame) {
    mu_t f = tbl_lookup(scope, muint(0));
    mu_t i = tbl_lookup(scope, muint(1));

    while (true) {
        mu_fcall(mu_inc(i), 0x0f, frame);
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            tbl_dec(frame[0]);
            mu_dec(f);
            mu_dec(i);
            return 0;
        }
        mu_dec(m);

        m = mu_inc(frame[0]);
        mu_fcall(mu_inc(f), 0xf1, frame);
        if (frame[0]) {
            mu_dec(frame[0]);
            frame[0] = m;
            mu_dec(f);
            mu_dec(i);
            return 0xf;
        }
        tbl_dec(m);
    }
}

mu_t fn_filter(mu_t f, mu_t iter) {
    return msbfn(0, fn_filter_step, mtbl({
        { muint(0), f },
        { muint(1), iter }
    }));
}

mu_t fn_reduce(mu_t f, mu_t iter, mu_t inits) {
    mu_t frame[MU_FRAME];

    if (!inits || tbl_len(inits) == 0) {
        mu_dec(inits);
        mu_fcall(mu_inc(iter), 0x0f, frame);
        inits = frame[0];
    }

    mu_t results = inits;

    while (true) {
        mu_fcall(mu_inc(iter), 0x0f, frame);
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            mu_dec(iter);
            mu_dec(f);
            tbl_dec(frame[0]);
            return results;
        }
        mu_dec(m);

        frame[0] = tbl_concat(results, frame[0], muint(mu_len(results)));
        mu_fcall(mu_inc(f), 0xff, frame);
        results = frame[0];
    }
}

bool fn_any(mu_t f, mu_t iter) {
    mu_t frame[MU_FRAME];

    while (true) {
        mu_fcall(mu_inc(iter), 0x0f, frame);
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            mu_dec(iter);
            mu_dec(f);
            tbl_dec(frame[0]);
            return false;
        }
        mu_dec(m);

        mu_fcall(mu_inc(f), 0xf1, frame);
        if (frame[0]) {
            mu_dec(frame[0]);
            return true;
        }
    }
}

bool fn_all(mu_t f, mu_t iter) {
    mu_t frame[MU_FRAME];

    while (true) {
        mu_fcall(mu_inc(iter), 0x0f, frame);
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            mu_dec(iter);
            mu_dec(f);
            tbl_dec(frame[0]);
            return true;
        }
        mu_dec(m);

        mu_fcall(mu_inc(f), 0xf1, frame);
        if (!frame[0])
            return false;
        else
            mu_dec(frame[0]);
    }
}

// Iterators and generators
static mc_t fn_range_step(mu_t scope, mu_t *frame) {
    mu_t i = tbl_lookup(scope, muint(0));
    mu_t stop = tbl_lookup(scope, muint(1));
    mu_t step = tbl_lookup(scope, muint(2));

    if ((num_cmp(step, muint(0)) > 0 && num_cmp(i, stop) >= 0) ||
        (num_cmp(step, muint(0)) < 0 && num_cmp(i, stop) <= 0))
        return 0;

    frame[0] = i;
    tbl_insert(scope, muint(0), num_add(i, step));
    return 1;
}

mu_t fn_range(mu_t start, mu_t stop, mu_t step) {
    return msbfn(0x0, fn_range_step, mtbl({
        { muint(0), start },
        { muint(1), stop },
        { muint(2), step }
    }));
}

static mc_t fn_repeat_step(mu_t scope, mu_t *frame) {
    mu_t i = tbl_lookup(scope, muint(1));
    if (num_cmp(i, muint(0)) <= 0)
        return 0;

    frame[0] = tbl_lookup(scope, muint(0));
    tbl_insert(scope, muint(1), num_sub(i, muint(1)));
    return 1;
}

mu_t fn_repeat(mu_t m, mu_t times) {
    return msbfn(0x0, fn_repeat_step, mtbl({
        { muint(0), m },
        { muint(1), times }
    }));
}

// Iterator manipulation
static mc_t fn_zip_step(mu_t scope, mu_t *frame) {
    mu_t iters = tbl_lookup(scope, muint(1));

    if (!iters) {
        mu_t iteriter = tbl_lookup(scope, muint(0));
        iters = tbl_create(0, 0);

        while (true) {
            mu_fcall(mu_inc(iteriter), 0x01, frame);
            if (!frame[0])
                break;

            tbl_insert(iters, muint(tbl_len(iters)), mu_iter(frame[0]));
        }

        mu_dec(iteriter);
        tbl_insert(scope, muint(1), mu_inc(iters));
    }

    mu_t acc = tbl_create(tbl_len(iters), 0);
    mu_t iter;

    for (muint_t i = 0; tbl_next(iters, &i, 0, &iter);) {
        mu_fcall(iter, 0x0f, frame);

        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            mu_dec(acc);
            mu_dec(frame[0]);
            return 0;
        }
        mu_dec(m);

        acc = tbl_concat(acc, frame[0], muint(tbl_len(acc)));
    }

    frame[0] = acc;
    return 0xf;
}

mu_t fn_zip(mu_t iters) {
    return msbfn(0x0, fn_zip_step, mtbl({
        { muint(0), iters }
    }));
}

static mc_t fn_chain_step(mu_t scope, mu_t *frame) {
    mu_t iter = tbl_lookup(scope, muint(1));

    if (iter) {
        mu_fcall(iter, 0x0f, frame);

        mu_t m = tbl_lookup(frame[0], muint(0));
        if (m) {
            mu_dec(m);
            return 0xf;
        }
        mu_dec(frame[0]);
    }

    mu_t iters = tbl_lookup(scope, muint(0));
    mu_fcall(iters, 0x01, frame);
    if (frame[0]) {
        tbl_insert(scope, muint(1), mu_iter(frame[0]));
        return fn_chain_step(scope, frame);
    }

    return 0;
}

mu_t fn_chain(mu_t iters) {
    return msbfn(0x0, fn_chain_step, mtbl({
        { muint(0), iters }
    }));
}

static mc_t fn_take_count_step(mu_t scope, mu_t *frame) {
    mu_t i = tbl_lookup(scope, muint(1));
    if (num_cmp(i, muint(0)) <= 0)
        return 0;

    tbl_insert(scope, muint(1), num_sub(i, muint(1)));
    mu_t iter = tbl_lookup(scope, muint(0));
    return mu_tcall(iter, 0x0, frame);
}

static mc_t fn_take_while_step(mu_t scope, mu_t *frame) {
    mu_t iter = tbl_lookup(scope, muint(0));
    mu_fcall(iter, 0x0f, frame);

    mu_t m = tbl_lookup(frame[0], muint(0));
    if (!m) {
        mu_dec(frame[0]);
        return 0;
    }
    mu_dec(m);

    m = mu_inc(frame[0]);
    mu_t cond = tbl_lookup(scope, muint(1));
    mu_fcall(cond, 0xf1, frame);
    if (!frame[0]) {
        tbl_dec(m);
        return 0;
    }
    mu_dec(frame[0]);
    frame[0] = m;
    return 0xf;
}

mu_t fn_take(mu_t cond, mu_t iter) {
    if (mu_isnum(cond)) {
        return msbfn(0x0, fn_take_count_step, mtbl({
            { muint(0), iter },
            { muint(1), cond }
        }));
    } else {
        return msbfn(0x0, fn_take_while_step, mtbl({
            { muint(0), iter },
            { muint(1), cond }
        }));
    }
}

static mc_t fn_drop_count_step(mu_t scope, mu_t *frame) {
    mu_t iter = tbl_lookup(scope, muint(0));
    mu_t i = tbl_lookup(scope, muint(1));

    if (i) {
        while (num_cmp(i, muint(0)) > 0) {
            mu_fcall(mu_inc(iter), 0x01, frame);
            if (!frame[0]) {
                mu_dec(iter);
                return 0;
            }
            mu_dec(frame[0]);
            i = num_sub(i, muint(1));
        }

        tbl_insert(scope, muint(1), mnil);
    }

    return mu_tcall(iter, 0x0, frame);
}

static mc_t fn_drop_while_step(mu_t scope, mu_t *frame) {
    mu_t iter = tbl_lookup(scope, muint(0));
    mu_t cond = tbl_lookup(scope, muint(1));

    if (cond) {
        while (true) {
            mu_fcall(mu_inc(iter), 0x0f, frame);

            mu_t m = tbl_lookup(frame[0], muint(0));
            if (!m) {
                mu_dec(iter);
                mu_dec(cond);
                return 0;
            }
            mu_dec(m);

            m = mu_inc(frame[0]);
            mu_fcall(mu_inc(cond), 0xf1, frame);
            if (!frame[0]) {
                mu_dec(iter);
                mu_dec(cond);
                frame[0] = m;
                tbl_insert(scope, muint(1), mnil);
                return 0xf;
            }
            mu_dec(m);
        }
    }

    return mu_tcall(iter, 0x0, frame);
}

mu_t fn_drop(mu_t cond, mu_t iter) {
    if (mu_isnum(cond)) {
        return msbfn(0x0, fn_drop_count_step, mtbl({
            { muint(0), iter },
            { muint(1), cond }
        }));
    } else {
        return msbfn(0x0, fn_drop_while_step, mtbl({
            { muint(0), iter },
            { muint(1), cond }
        }));
    }
}

// Iterator ordering
mu_t fn_min(mu_t iter) {
    mu_t frame[MU_FRAME];

    mu_fcall(mu_inc(iter), 0x0f, frame);
    mu_t min_frame = frame[0];
    mu_t min = tbl_lookup(min_frame, muint(0));
    if (!min)
        mu_err_undefined();

    while (true) {
        mu_fcall(mu_inc(iter), 0x0f, frame);
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            mu_dec(frame[0]);
            mu_dec(min);
            return min_frame;
        }

        if (mu_cmp(m, min) < 0) {
            mu_dec(min);
            mu_dec(min_frame);
            min = m;
            min_frame = frame[0];
        } else {
            mu_dec(m);
            mu_dec(frame[0]);
        }
    }
}

mu_t fn_max(mu_t iter) {
    mu_t frame[MU_FRAME];

    mu_fcall(mu_inc(iter), 0x0f, frame);
    mu_t max_frame = frame[0];
    mu_t max = tbl_lookup(max_frame, muint(0));
    if (!max)
        mu_err_undefined();

    while (true) {
        mu_fcall(mu_inc(iter), 0x0f, frame);
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            mu_dec(frame[0]);
            mu_dec(max);
            return max_frame;
        }

        if (mu_cmp(m, max) > 0) {
            mu_dec(max);
            mu_dec(max_frame);
            max = m;
            max_frame = frame[0];
        } else {
            mu_dec(m);
            mu_dec(frame[0]);
        }
    }
}

static mc_t fn_reverse_step(mu_t scope, mu_t *frame) {
    mu_t i = tbl_lookup(scope, muint(1));
    if (num_cmp(i, muint(0)) < 0)
        return 0;

    mu_t store = tbl_lookup(scope, muint(0));
    frame[0] = tbl_lookup(store, i);
    mu_dec(store);

    tbl_insert(scope, muint(1), num_sub(i, muint(1)));
    return 0xf;
}

mu_t fn_reverse(mu_t iter) {
    mu_t frame[MU_FRAME];
    mu_t store = tbl_create(0, 0);

    while (true) {
        mu_fcall(mu_inc(iter), 0x0f, frame);

        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            mu_dec(frame[0]);
            break;
        }
        mu_dec(m);

        tbl_insert(store, muint(tbl_len(store)), frame[0]);
    }

    mu_dec(iter);

    return msbfn(0x0, fn_reverse_step, mtbl({
        { muint(0), store },
        { muint(1), muint(tbl_len(store)-1) }
    }));
}

// Simple iterative merge sort
static void fn_merge_sort(mu_t elems) {
    // Uses arrays (first elem, frame) to keep from 
    // looking up the elem each time.
    // We use two arrays so we can just flip each merge.
    muint_t len = tbl_len(elems);
    mu_t (*a)[2] = mu_alloc(len*sizeof(mu_t[2]));
    mu_t (*b)[2] = mu_alloc(len*sizeof(mu_t[2]));

    for (muint_t i = 0, j = 0; tbl_next(elems, &i, 0, &a[j][1]); j++) {
        a[j][0] = tbl_lookup(a[j][1], muint(0));
    }

    for (muint_t slice = 1; slice < len; slice *= 2) {
        for (muint_t i = 0; i < len; i += 2*slice) {
            muint_t x = 0;
            muint_t y = 0;

            for (muint_t j = 0; j < 2*slice && i+j < len; j++) {
                if (y >= slice || i+slice+y >= len ||
                    (x < slice && mu_cmp(a[i+x][0], a[i+slice+y][0]) <= 0)) {
                    memcpy(b[i+j], a[i+x], sizeof(mu_t[2]));
                    x++;
                } else {
                    memcpy(b[i+j], a[i+slice+y], sizeof(mu_t[2]));
                    y++;
                }
            }
        }

        mu_t (*t)[2] = a;
        a = b;
        b = t;
    }

    // Reuse the table to store results
    for (muint_t i = 0; i < len; i++) {
        mu_dec(a[i][0]);
        tbl_insert(elems, muint(i), a[i][1]);
    }

    // Since both arrays identical, it doesn't matter if they've been swapped
    mu_dealloc(a, len*sizeof(mu_t[2]));
    mu_dealloc(b, len*sizeof(mu_t[2]));
}

static mc_t fn_sort_step(mu_t scope, mu_t *frame) {
    mu_t store = tbl_lookup(scope, muint(0));
    muint_t i = num_uint(tbl_lookup(scope, muint(1)));

    bool next = tbl_next(store, &i, 0, &frame[0]);
    mu_dec(store);
    tbl_insert(scope, muint(1), muint(i));
    return next ? 0xf : 0;
}

mu_t fn_sort(mu_t iter) {
    mu_t frame[MU_FRAME];
    mu_t store = tbl_create(0, 0);

    while (true) {
        mu_fcall(mu_inc(iter), 0x0f, frame);

        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            mu_dec(frame[0]);
            break;
        }
        mu_dec(m);

        tbl_insert(store, muint(tbl_len(store)), frame[0]);
    }

    mu_dec(iter);

    fn_merge_sort(store);

    return msbfn(0x0, fn_sort_step, mtbl({
        { muint(0), store },
        { muint(1), muint(0) },
    }));
}


// String representation
mu_t fn_repr(mu_t t) {
    mbyte_t *s = mstr_create(7 + 2*sizeof(muint_t));
    memcpy(s, "fn(0x", 6);

    for (muint_t i = 0; i < 2*sizeof(muint_t); i++)
        s[i+6] = mu_toascii(0xf & ((muint_t)t >> 4*(2*sizeof(muint_t)-1 - i)));

    s[6 + 2*sizeof(muint_t)] = ')';
    return mstr_intern(s, 7 + 2*sizeof(muint_t));
}
