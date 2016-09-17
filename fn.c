#include "fn.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "parse.h"
#include "vm.h"


// Function access
mu_inline struct fn *fn(mu_t f) {
    return (struct fn *)((muint_t)f - MTFN);
}


// Creation functions
mu_t fn_create(struct code *c, mu_t closure) {
    if (c->flags & FN_WEAK) {
        mu_assert(mu_ref(closure) > 1);
        mu_dec(closure);
    }

    struct fn *f = ref_alloc(sizeof(struct fn));
    f->args = c->args;
    f->flags = c->flags;
    f->closure = closure;
    f->fn.code = c;
    return (mu_t)((muint_t)f + MTFN);
}

mu_t fn_frombfn(mcnt_t args, mbfn_t *bfn) {
    struct fn *f = ref_alloc(sizeof(struct fn));
    f->args = args;
    f->flags = FN_BUILTIN;
    f->closure = 0;
    f->fn.bfn = bfn;
    return (mu_t)((muint_t)f + MTFN);
}

mu_t fn_fromsbfn(mcnt_t args, msbfn_t *sbfn, mu_t closure) {
    struct fn *f = ref_alloc(sizeof(struct fn));
    f->args = args;
    f->flags = FN_BUILTIN | FN_SCOPED;
    f->closure = closure;
    f->fn.sbfn = sbfn;
    return (mu_t)((muint_t)f + MTFN);
}


// Called by garbage collector to clean up
void fn_destroy(mu_t f) {
    if (!(fn(f)->flags & FN_BUILTIN)) {
        code_dec(fn(f)->fn.code);
    }

    if (!(fn(f)->flags & FN_WEAK)) {
        mu_dec(fn(f)->closure);
    }

    ref_dealloc(fn(f), sizeof(struct fn));
}

void code_destroy(struct code *c) {
    for (muint_t i = 0; i < c->icount; i++) {
        mu_dec(code_imms(c)[i]);
    }

    for (muint_t i = 0; i < c->fcount; i++) {
        code_dec(code_fns(c)[i]);
    }

    ref_dealloc(c, sizeof(struct code) +
                   c->icount*sizeof(mu_t) +
                   c->fcount*sizeof(struct code *) +
                   c->bcount);
}


// C interface for calling functions
mcnt_t fn_tcall(mu_t f, mcnt_t fc, mu_t *frame) {
    mu_frame_convert(fc, fn(f)->args, frame);

    switch (fn(f)->flags & (FN_BUILTIN | FN_SCOPED)) {
        case FN_BUILTIN: {
            mbfn_t *bfn = fn(f)->fn.bfn;
            fn_dec(f);
            return bfn(frame);
        }

        case FN_BUILTIN | FN_SCOPED: {
            mcnt_t rc = fn(f)->fn.sbfn(fn(f)->closure, frame);
            fn_dec(f);
            return rc;
        }

        case FN_SCOPED: {
            struct code *c = fn_code(f);
            mu_t scope = tbl_extend(c->scope, fn_closure(f));
            fn_dec(f);
            return mu_exec(c, scope, frame);
        }
    }

    mu_unreachable;
}

void fn_fcall(mu_t f, mcnt_t fc, mu_t *frame) {
    mcnt_t rets = fn_tcall(mu_inc(f), fc >> 4, frame);
    mu_frame_convert(rets, fc & 0xf, frame);
}

mu_t fn_vcall(mu_t f, mcnt_t fc, va_list args) {
    mu_t frame[MU_FRAME];

    for (muint_t i = 0; i < mu_frame_len(fc >> 4); i++) {
        frame[i] = va_arg(args, mu_t);
    }

    fn_fcall(f, fc, frame);

    for (muint_t i = 1; i < mu_frame_len(0xf & fc); i++) {
        *va_arg(args, mu_t *) = frame[i];
    }

    return (0xf & fc) ? *frame : 0;
}

mu_t fn_call(mu_t f, mcnt_t fc, ...) {
    va_list args;
    va_start(args, fc);
    mu_t ret = fn_vcall(f, fc, args);
    va_end(args);
    return ret;
}


// Iteration
bool fn_next(mu_t f, mcnt_t fc, mu_t *frame) {
    mu_assert(mu_isfn(f));
    fn_fcall(f, (fc == 0) ? 1 : fc, frame);

    if (fc != 0xf) {
        if (frame[0]) {
            if (fc == 0) {
                mu_dec(frame[0]);
            }
            return true;
        } else {
            mu_frame_convert(fc, 0, frame);
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


// Function related Mu functions
static mcnt_t mu_bfn_fn(mu_t *frame) {
    mu_t m = frame[0];

    switch (mu_type(m)) {
        case MTNIL:
            mu_dec(m);
            frame[0] = MU_ID;
            return 1;

        case MTFN:
            frame[0] = m;
            return 1;

        default:
            break;
    }

    mu_error_cast(MU_KEY_FN2, m);
}

MSTR(mu_gen_key_fn2, "fn_")
MBFN(mu_gen_fn, 0x1, mu_bfn_fn)

// Default function
static mcnt_t mu_bfn_id(mu_t *frame) {
    return 0xf;
}

MSTR(mu_gen_key_id, "id")
MBFN(mu_gen_id, 0xf, mu_bfn_id)

// Binds arguments to function
static mcnt_t fn_bound(mu_t scope, mu_t *frame) {
    mu_t f = tbl_lookup(scope, muint(0));
    mu_t args = tbl_lookup(scope, muint(1));

    frame[0] = tbl_concat(args, frame[0], 0);
    return fn_tcall(f, 0xf, frame);
}

static mcnt_t mu_bfn_bind(mu_t *frame) {
    mu_t f = tbl_pop(frame[0], 0);
    if (!mu_isfn(f)) {
        mu_error_arg(MU_KEY_BIND, 0x2, (mu_t[]){f, frame[0]});
    }

    frame[0] = msbfn(0xf, fn_bound, mlist({f, frame[0]}));
    return 1;
}

MSTR(mu_gen_key_bind, "bind")
MBFN(mu_gen_bind, 0xf, mu_bfn_bind)

static mcnt_t fn_composed(mu_t fs, mu_t *frame) {
    mcnt_t c = 0xf;
    for (muint_t i = tbl_len(fs)-1; i+1 > 0; i--) {
        mu_t f = tbl_lookup(fs, muint(i));
        c = fn_tcall(f, c, frame);
    }

    return c;
}

static mcnt_t mu_bfn_comp(mu_t *frame) {
    mu_t f;
    for (muint_t i = 0; tbl_next(frame[0], &i, 0, &f);) {
        if (!mu_isfn(f)) {
            mu_error_arg(MU_KEY_COMP, 0xf, frame);
        }
    }

    frame[0] = msbfn(0xf, fn_composed, frame[0]);
    return 1;
}

MSTR(mu_gen_key_comp, "comp")
MBFN(mu_gen_comp, 0xf, mu_bfn_comp)

