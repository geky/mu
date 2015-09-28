#include "fn.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "parse.h"
#include "vm.h"


// Function access
mu_inline struct fn *fn(mu_t f) {
    return (struct fn *)(~7 & (muint_t)f);
}


// Conversion functions
mu_t fn_frombfn(mc_t args, mbfn_t *bfn) {
    struct fn *f = ref_alloc(sizeof(struct fn));
    f->args = args;
    f->type = MU_BFN - MU_FN;
    f->closure = mnil;
    f->bfn = bfn;
    return (mu_t)((muint_t)f + MU_BFN);
}

mu_t fn_fromsfn(mc_t args, msfn_t *sfn, mu_t closure) {
    struct fn *f = ref_alloc(sizeof(struct fn));
    f->args = args;
    f->type = MU_SFN - MU_FN;
    f->closure = closure;
    f->sfn = sfn;
    return (mu_t)((muint_t)f + MU_SFN);
}

mu_t fn_fromcode(struct code *c, mu_t closure) {
    struct fn *f = ref_alloc(sizeof(struct fn));
    f->args = c->args;
    f->type = MU_FN - MU_FN;
    f->closure = closure;
    f->code = c;
    return (mu_t)((muint_t)f + MU_FN);
}


// Called by garbage collector to clean up
void fn_destroy(mu_t f) {
    if (mu_type(f) == MU_FN)
        code_dec(fn(f)->code);

    if (fn(f)->closure)
        mu_dec(fn(f)->closure);

    ref_dealloc(fn(f), sizeof(struct fn));
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
mc_t bfn_tcall(mu_t f, mc_t fc, mu_t *frame) {
    mu_fconvert(fn(f)->args, fc, frame);
    mbfn_t *bfn = fn(f)->bfn;
    fn_dec(f);
    return bfn(frame);
}

mc_t sfn_tcall(mu_t f, mc_t fc, mu_t *frame) {
    mu_fconvert(fn(f)->args, fc, frame);
    mc_t rc = fn(f)->sfn(fn(f)->closure, frame);
    fn_dec(f);
    return rc;
}

mc_t mfn_tcall(mu_t f, mc_t fc, mu_t *frame) {
    struct code *c = fn_code(f);
    mu_fconvert(c->args, fc, frame);
    mu_t scope = tbl_extend(c->scope, fn_closure(f));
    fn_dec(f);
    return mu_exec(c, scope, frame);
}

mc_t fn_tcall(mu_t f, mc_t fc, mu_t *frame) {
    mu_assert(mu_isfn(f));

    switch (mu_type(f)) {
        case MU_FN:  return mfn_tcall(f, fc, frame);
        case MU_BFN: return bfn_tcall(f, fc, frame);
        case MU_SFN: return sfn_tcall(f, fc, frame);
        default:     mu_unreachable;
    }
}

void fn_fcall(mu_t f, mc_t fc, mu_t *frame) {
    mc_t rets = fn_tcall(mu_inc(f), fc >> 4, frame);
    mu_fconvert(fc & 0xf, rets, frame);
}


// Iteration
bool fn_next(mu_t f, mc_t fc, mu_t *frame) {
    mu_assert(mu_isfn(f));
    fn_fcall(f, fc ? fc : 1, frame);

    if (fc != 0xf) {
        if (frame[0]) {
            if (fc == 0)
                mu_dec(frame[0]);
            return true;
        } else {
            mu_fconvert(0, fc, frame);
            return false;
        }
    } else {
        mu_t m = tbl_lookup(frame[0], muint(0));

        if (m) {
            mu_dec(m);
            return true;
        } else {
            tbl_dec(frame[0]);
            return false;
        }
    }           
}


// Default function
static mc_t fn_identity(mu_t *frame) { return 0xf; }

mu_pure mu_t fn_id(void) {
    return mcfn(0xf, fn_identity);
}

// Binds arguments to function
static mc_t fn_bound(mu_t scope, mu_t *frame) {
    mu_t f = tbl_lookup(scope, muint(0));
    mu_t args = tbl_lookup(scope, muint(1));

    frame[0] = tbl_concat(args, frame[0], mnil);
    return fn_tcall(f, 0xf, frame);
}

mu_t fn_bind(mu_t f, mu_t args) {
    mu_assert(mu_isfn(f) && mu_istbl(args));
    return msfn(0xf, fn_bound, mmlist({f, args}));
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
    mu_assert(mu_istbl(fs));
    return msfn(0xf, fn_composed, fs);
}
    

// Functions over iterators
static mc_t fn_map_step(mu_t scope, mu_t *frame) {
    mu_t f = tbl_lookup(scope, muint(0));
    mu_t i = tbl_lookup(scope, muint(1));

    while (fn_next(i, 0xf, frame)) {
        fn_fcall(f, 0xff, frame);
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (m) {
            mu_dec(m);
            mu_dec(f);
            mu_dec(i);
            return 0xf;
        }
        tbl_dec(frame[0]);
    }

    fn_dec(f);
    fn_dec(i);
    return 0;
}

mu_t fn_map(mu_t f, mu_t iter) {
    mu_assert(mu_isfn(f) && mu_isfn(iter));
    return msfn(0, fn_map_step, mmlist({f, iter}));
}

static mc_t fn_filter_step(mu_t scope, mu_t *frame) {
    mu_t f = tbl_lookup(scope, muint(0));
    mu_t i = tbl_lookup(scope, muint(1));

    while (fn_next(i, 0xf, frame)) {
        mu_t m = tbl_inc(frame[0]);
        fn_fcall(f, 0xf1, frame);
        if (frame[0]) {
            mu_dec(frame[0]);
            frame[0] = m;
            mu_dec(f);
            mu_dec(i);
            return 0xf;
        }
        tbl_dec(m);
    }

    fn_dec(f);
    fn_dec(i);
    return 0;
}

mu_t fn_filter(mu_t f, mu_t iter) {
    mu_assert(mu_isfn(f) && mu_isfn(iter));
    return msfn(0, fn_filter_step, mmlist({f, iter}));
}

mu_t fn_reduce(mu_t f, mu_t iter, mu_t acc) {
    mu_assert(mu_isfn(f) && mu_isfn(iter) && 
              (!acc || mu_istbl(acc)));
    mu_t frame[MU_FRAME];

    if (!acc || tbl_len(acc) == 0) {
        mu_dec(acc);
        fn_fcall(iter, 0x0f, frame);
        acc = frame[0];
    }

    while (fn_next(iter, 0xf, frame)) {
        frame[0] = tbl_concat(acc, frame[0], mnil);
        fn_fcall(f, 0xff, frame);
        acc = frame[0];
    }

    fn_dec(f);
    fn_dec(iter);
    return acc;
}

bool fn_any(mu_t f, mu_t iter) {
    mu_assert(mu_isfn(f) && mu_isfn(iter));
    mu_t frame[MU_FRAME];

    while (fn_next(iter, 0xf, frame)) {
        fn_fcall(f, 0xf1, frame);
        if (frame[0]) {
            mu_dec(frame[0]);
            return true;
        }
    }

    fn_dec(f);
    fn_dec(iter);
    return false;
}

bool fn_all(mu_t f, mu_t iter) {
    mu_assert(mu_isfn(f) && mu_isfn(iter));
    mu_t frame[MU_FRAME];

    while (fn_next(iter, 0xf, frame)) {
        fn_fcall(f, 0xf1, frame);
        if (frame[0])
            mu_dec(frame[0]);
        else
            return false;
    }

    fn_dec(f);
    fn_dec(iter);
    return true;
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
    mu_assert((!start || mu_isnum(start)) && 
              (!stop || mu_isnum(stop)) && 
              (!step || mu_isnum(step)));

    return msfn(0x0, fn_range_step, mmlist({
        start ? start : muint(0),
        stop ? stop : MU_INF,
        step ? step : muint(1)
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
    mu_assert(mu_isnum(times) && (!times || mu_isnum(times)));

    return msfn(0x0, fn_repeat_step, mmlist({
        m, 
        times ? times : MU_INF
    }));
}

// Iterator manipulation
static mc_t fn_zip_step(mu_t scope, mu_t *frame) {
    mu_t iters = tbl_lookup(scope, muint(1));

    if (!iters) {
        mu_t iteriter = tbl_lookup(scope, muint(0));
        iters = tbl_create(0);

        while (fn_next(iteriter, 0x1, frame)) {
            tbl_insert(iters, muint(tbl_len(iters)), mu_iter(frame[0]));
        }

        fn_dec(iteriter);
        tbl_insert(scope, muint(1), mu_inc(iters));
    }

    mu_t acc = tbl_create(tbl_len(iters));
    mu_t iter;

    for (muint_t i = 0; tbl_next(iters, &i, 0, &iter);) {
        fn_fcall(iter, 0x0f, frame);
        fn_dec(iter);

        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            mu_dec(acc);
            mu_dec(frame[0]);
            tbl_dec(iters);
            return 0;
        }
        mu_dec(m);

        acc = tbl_concat(acc, frame[0], mnil);
    }

    tbl_dec(iters);
    frame[0] = acc;
    return 0xf;
}

mu_t fn_zip(mu_t iters) {
    mu_assert(mu_isfn(iters));
    return msfn(0x0, fn_zip_step, mmlist({iters}));
}

static mc_t fn_chain_step(mu_t scope, mu_t *frame) {
    mu_t iter = tbl_lookup(scope, muint(1));

    if (iter) {
        fn_fcall(iter, 0x0f, frame);
        fn_dec(iter);

        mu_t m = tbl_lookup(frame[0], muint(0));
        if (m) {
            mu_dec(m);
            return 0xf;
        }
        mu_dec(frame[0]);
    }

    mu_t iters = tbl_lookup(scope, muint(0));
    fn_fcall(iters, 0x01, frame);
    fn_dec(iters);
    if (frame[0]) {
        tbl_insert(scope, muint(1), mu_iter(frame[0]));
        return fn_chain_step(scope, frame);
    }

    return 0;
}

mu_t fn_chain(mu_t iters) {
    mu_assert(mu_isfn(iters));
    return msfn(0x0, fn_chain_step, mmlist({iters}));
}

static mc_t fn_take_count_step(mu_t scope, mu_t *frame) {
    mu_t i = tbl_lookup(scope, muint(1));
    if (num_cmp(i, muint(0)) <= 0)
        return 0;

    tbl_insert(scope, muint(1), num_sub(i, muint(1)));
    mu_t iter = tbl_lookup(scope, muint(0));
    return fn_tcall(iter, 0x0, frame);
}

static mc_t fn_take_while_step(mu_t scope, mu_t *frame) {
    mu_t iter = tbl_lookup(scope, muint(0));
    fn_fcall(iter, 0x0f, frame);
    fn_dec(iter);

    mu_t m = tbl_lookup(frame[0], muint(0));
    if (!m) {
        mu_dec(frame[0]);
        return 0;
    }
    mu_dec(m);

    m = mu_inc(frame[0]);
    mu_t cond = tbl_lookup(scope, muint(1));
    fn_fcall(cond, 0xf1, frame);
    fn_dec(cond);
    if (!frame[0]) {
        tbl_dec(m);
        return 0;
    }
    mu_dec(frame[0]);
    frame[0] = m;
    return 0xf;
}

mu_t fn_take(mu_t cond, mu_t iter) {
    mu_assert((mu_isnum(cond) || mu_isfn(cond)) && mu_isfn(iter));

    if (mu_isnum(cond)) {
        return msfn(0x0, fn_take_count_step, mmlist({iter, cond}));
    } else {
        return msfn(0x0, fn_take_while_step, mmlist({iter, cond}));
    }
}

static mc_t fn_drop_count_step(mu_t scope, mu_t *frame) {
    mu_t iter = tbl_lookup(scope, muint(0));
    mu_t i = tbl_lookup(scope, muint(1));

    if (i) {
        while (num_cmp(i, muint(0)) > 0) {
            fn_fcall(iter, 0x01, frame);
            if (!frame[0]) {
                mu_dec(iter);
                return 0;
            }
            mu_dec(frame[0]);
            i = num_sub(i, muint(1));
        }

        tbl_insert(scope, muint(1), mnil);
    }

    return fn_tcall(iter, 0x0, frame);
}

static mc_t fn_drop_while_step(mu_t scope, mu_t *frame) {
    mu_t iter = tbl_lookup(scope, muint(0));
    mu_t cond = tbl_lookup(scope, muint(1));

    if (cond) {
        while (fn_next(iter, 0xf, frame)) {
            mu_t m = tbl_inc(frame[0]);
            fn_fcall(cond, 0xf1, frame);
            if (!frame[0]) {
                fn_dec(iter);
                fn_dec(cond);
                frame[0] = m;
                tbl_insert(scope, muint(1), mnil);
                return 0xf;
            }
            mu_dec(m);
        }

        fn_dec(iter);
        fn_dec(cond);
        return 0;
    }

    return fn_tcall(iter, 0x0, frame);
}

mu_t fn_drop(mu_t cond, mu_t iter) {
    mu_assert((mu_isnum(cond) || mu_isfn(cond)) && mu_isfn(iter));

    if (mu_isnum(cond)) {
        return msfn(0x0, fn_drop_count_step, mmlist({iter, cond}));
    } else {
        return msfn(0x0, fn_drop_while_step, mmlist({iter, cond}));
    }
}

// Iterator ordering
mu_t fn_min(mu_t iter) {
    mu_assert(mu_isfn(iter));
    mu_t frame[MU_FRAME];

    fn_fcall(iter, 0x0f, frame);
    mu_t min_frame = frame[0];
    mu_t min = tbl_lookup(min_frame, muint(0));
    if (!min)
        mu_error(mcstr("no elements passed to min"));

    while (fn_next(iter, 0xf, frame)) {
        mu_t m = tbl_lookup(frame[0], muint(0));

        if (mu_cmp(m, min) < 0) {
            mu_dec(min);
            tbl_dec(min_frame);
            min = m;
            min_frame = frame[0];
        } else {
            mu_dec(m);
            tbl_dec(frame[0]);
        }
    }

    fn_dec(iter);
    mu_dec(min);
    return min_frame;
}

mu_t fn_max(mu_t iter) {
    mu_assert(mu_isfn(iter));
    mu_t frame[MU_FRAME];

    fn_fcall(iter, 0x0f, frame);
    mu_t max_frame = frame[0];
    mu_t max = tbl_lookup(max_frame, muint(0));
    if (!max)
        mu_error(mcstr("no elements passed to max"));

    while (fn_next(iter, 0xf, frame)) {
        mu_t m = tbl_lookup(frame[0], muint(0));
        
        if (mu_cmp(m, max) > 0) {
            mu_dec(max);
            tbl_dec(max_frame);
            max = m;
            max_frame = frame[0];
        } else {
            mu_dec(m);
            tbl_dec(frame[0]);
        }
    }

    fn_dec(iter);
    mu_dec(max);
    return max_frame;
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
    mu_assert(mu_isfn(iter));
    mu_t frame[MU_FRAME];
    mu_t store = tbl_create(0);

    while (fn_next(iter, 0xf, frame)) {
        tbl_insert(store, muint(tbl_len(store)), frame[0]);
    }

    fn_dec(iter);

    return msfn(0x0, fn_reverse_step, mmlist({
        store, muint(tbl_len(store)-1)
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
    mu_assert(mu_isfn(iter));
    mu_t frame[MU_FRAME];
    mu_t store = tbl_create(0);

    while (fn_next(iter, 0xf, frame)) {
        tbl_insert(store, muint(tbl_len(store)), frame[0]);
    }

    mu_dec(iter);

    fn_merge_sort(store);

    return msfn(0x0, fn_sort_step, mmlist({store, muint(0)}));
}

