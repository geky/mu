#include "fn.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "parse.h"
#include "vm.h"
#include <string.h>
#include <stdarg.h>


// Internally used conversion between mu_t and struct tbl
mu_inline struct fn *fn_fn(mu_t m) {
    return (struct fn *)(~7 & (uint_t)m);
}


// C Function creating functions and macros
mu_t mbfn(frame_t args, bfn_t *bfn) {
    struct fn *f = ref_alloc(sizeof(struct fn));
    f->args = args;
    f->type = MU_BFN - MU_FN;
    f->closure = mnil;
    f->bfn = bfn;
    return (mu_t)((uint_t)f + MU_BFN);
}

mu_t msbfn(frame_t args, sbfn_t *sbfn, mu_t closure) {
    struct fn *f = ref_alloc(sizeof(struct fn));
    f->args = args;
    f->type = MU_SBFN - MU_FN;
    f->closure = closure;
    f->sbfn = sbfn;
    return (mu_t)((uint_t)f + MU_SBFN);
}

mu_t mfn(struct code *c, mu_t closure) {
    struct fn *f = ref_alloc(sizeof(struct fn));
    f->args = c->args;
    f->type = MU_FN - MU_FN;
    f->closure = closure;
    f->code = c;
    return (mu_t)((uint_t)f + MU_FN);
}


// Called by garbage collector to clean up
void bfn_destroy(mu_t m) {
    struct fn *f = fn_fn(m);
    ref_dealloc(f, sizeof(struct fn));
}

void sbfn_destroy(mu_t m) {
    struct fn *f = fn_fn(m);
    mu_dec(f->closure);
    ref_dealloc(f, sizeof(struct fn));
}

void fn_destroy(mu_t m) {
    struct fn *f = fn_fn(m);
    mu_dec(f->closure);
    code_dec(f->code);
    ref_dealloc(f, sizeof(struct fn));
}

void code_destroy(struct code *c) {
    for (uint_t i = 0; i < c->icount; i++)
        mu_dec(c->data[i].imms);

    for (uint_t i = 0; i < c->fcount; i++)
        code_dec(c->data[c->icount+i].fns);

    ref_dealloc(c, mu_offset(struct code, data) + 
                   c->icount*sizeof(mu_t) +
                   c->fcount*sizeof(struct code *) +
                   c->bcount);
}


// C interface for calling functions
void bfn_fcall(mu_t m, frame_t fc, mu_t *frame) {
    struct fn *f = fn_fn(m);
    mu_fconvert(f->args, frame, fc >> 4, frame);
    frame_t rets = f->bfn(frame);
    mu_fconvert(fc & 0xf, frame, rets, frame);
    fn_dec(m);
}

void sbfn_fcall(mu_t m, frame_t fc, mu_t *frame) {
    struct fn *f = fn_fn(m);
    mu_fconvert(f->args, frame, fc >> 4, frame);
    frame_t rets = f->sbfn(f->closure, frame);
    mu_fconvert(fc & 0xf, frame, rets, frame);
    fn_dec(m);
}

void fn_fcall(mu_t m, frame_t fc, mu_t *frame) {
    struct fn *f = fn_fn(m);
    struct code *c = code_inc(f->code);
    mu_t scope = tbl_extend(c->scope, mu_inc(f->closure));
    fn_dec(m);
    mu_exec(c, scope, fc, frame);
}


// Binds arguments to function
static frame_t fn_bound(mu_t scope, mu_t *frame) {
    mu_t f = tbl_lookup(scope, muint(0));
    mu_t args = tbl_lookup(scope, muint(1));

    frame[0] = tbl_concat(args, frame[0], mnil);
    mu_fcall(f, 0xff, frame);
    return 0xf;
}

mu_t fn_bind(mu_t f, mu_t args) {
    mu_t scope = tbl_create(2);
    tbl_insert(scope, muint(0), f);
    tbl_insert(scope, muint(1), args);

    return msbfn(0xf, fn_bound, scope);
}


// Functions over iterators
static frame_t fn_map_step(mu_t scope, mu_t *frame) {
    mu_t f = tbl_lookup(scope, muint(0));
    mu_t i = tbl_lookup(scope, muint(1));

    while (true) {
        mu_fcall(i, 0x0f, frame);
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            tbl_dec(frame[0]);
            return 0;
        }
        mu_dec(m);

        mu_fcall(f, 0xff, frame);
        m = tbl_lookup(frame[0], muint(0));
        if (m) {
            mu_dec(m);
            return 0xf;
        }
        tbl_dec(frame[0]);
    }
}

mu_t fn_map(mu_t f, mu_t m) {
    mu_t i = mu_iter(m);

    mu_t scope = tbl_create(2);
    tbl_insert(scope, muint(0), f);
    tbl_insert(scope, muint(1), i);

    return msbfn(0, fn_map_step, scope);
}

static frame_t fn_filter_step(mu_t scope, mu_t *frame) {
    mu_t f = tbl_lookup(scope, muint(0));
    mu_t i = tbl_lookup(scope, muint(1));

    while (true) {
        mu_fcall(i, 0x0f, frame);
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            tbl_dec(frame[0]);
            return 0;
        }
        mu_dec(m);

        m = mu_inc(frame[0]);
        mu_fcall(f, 0xf1, frame);
        if (frame[0]) {
            mu_dec(frame[0]);
            frame[0] = m;
            return 0xf;
        }
        tbl_dec(m);
    }
}

mu_t fn_filter(mu_t f, mu_t m) {
    mu_t i = mu_iter(m);

    mu_t scope = tbl_create(2);
    tbl_insert(scope, muint(0), f);
    tbl_insert(scope, muint(1), i);

    return msbfn(0, fn_filter_step, scope);
}

mu_t fn_reduce(mu_t f, mu_t m, mu_t inits) {
    mu_t frame[MU_FRAME];
    mu_t i = mu_iter(m);

    if (!inits || tbl_len(inits) == 0) {
        mu_dec(inits);
        mu_fcall(i, 0x0f, frame);
        inits = frame[0];
    }

    mu_t results = inits;

    while (true) {
        mu_fcall(i, 0x0f, frame);
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            tbl_dec(frame[0]);
            return results;
        }
        mu_dec(m);

        frame[0] = tbl_concat(results, frame[0], mnil);
        mu_fcall(f, 0xff, frame);
        results = frame[0];
    }
}


// Returns a string representation of a function
mu_t fn_repr(mu_t f) {
    uint_t bits = (uint_t)fn_fn(f);

    byte_t *s = mstr_create(5 + 2*sizeof(uint_t));
    memcpy(s, "fn 0x", 5);

    for (uint_t i = 0; i < 2*sizeof(uint_t); i++) {
        s[i+5] = mu_toascii(0xf & (bits >> (4*(sizeof(uint_t)-i))));
    }

    return mstr_intern(s, 5 + 2*sizeof(uint_t));
}
