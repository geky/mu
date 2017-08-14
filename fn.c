#include "fn.h"
#include "mu.h"


// Function access
mu_inline struct mfn *mfn(mu_t f) {
    return (struct mfn *)((muint_t)f - MTFN);
}


// Creation functions
mu_t mu_fn_fromcode(mu_t c, mu_t closure) {
    if (mu_code_getflags(c) & MFN_WEAK) {
        mu_assert(mu_getref(closure) > 1);
        mu_dec(closure);
    }

    struct mfn *f = mu_refalloc(sizeof(struct mfn));
    f->args = mu_code_getargs(c);
    f->flags = mu_code_getflags(c);
    f->closure = closure;
    f->fn.code = c;
    return (mu_t)((muint_t)f + MTFN);
}

mu_t mu_fn_frombfn(mcnt_t args, mbfn_t *bfn) {
    struct mfn *f = mu_refalloc(sizeof(struct mfn));
    f->args = args;
    f->flags = MFN_BUILTIN;
    f->closure = 0;
    f->fn.bfn = bfn;
    return (mu_t)((muint_t)f + MTFN);
}

mu_t mu_fn_fromsbfn(mcnt_t args, msbfn_t *sbfn, mu_t closure) {
    struct mfn *f = mu_refalloc(sizeof(struct mfn));
    f->args = args;
    f->flags = MFN_BUILTIN | MFN_SCOPED;
    f->closure = closure;
    f->fn.sbfn = sbfn;
    return (mu_t)((muint_t)f + MTFN);
}

static mcnt_t mu_id_bfn(mu_t *frame) {
    return 0xf;
}
MU_DEF_BFN(mu_id_def, 0xf, mu_id_bfn)

mu_t mu_fn_frommu(mu_t m) {
    switch (mu_gettype(m)) {
        case MTNIL:
            return mu_id_def();

        case MTFN:
            return m;

        default:
            mu_dec(m);
            return 0;
    }
}

// Direct initialization
mu_t mu_fn_initsbfn(struct mfn *f, mcnt_t args,
            msbfn_t *sbfn, mu_t (*closure)(void)) {
    f->args = args;
    f->flags = MFN_BUILTIN | MFN_SCOPED;
    f->closure = closure();
    f->fn.sbfn = sbfn;
    return (mu_t)((muint_t)f + MTFN);
}


// Called by garbage collector to clean up
void mu_fn_destroy(mu_t f) {
    if (!(mfn(f)->flags & MFN_BUILTIN)) {
        mu_code_dec(mfn(f)->fn.code);
    }

    if (!(mfn(f)->flags & MFN_WEAK)) {
        mu_dec(mfn(f)->closure);
    }

    mu_refdealloc(mfn(f), sizeof(struct mfn));
}

void mu_code_destroy(mu_t c) {
    for (muint_t i = 0; i < mu_code_getimmslen(c); i++) {
        mu_dec(mu_code_getimms(c)[i]);
    }
}


// C interface for calling functions
mcnt_t mu_fn_tcall(mu_t f, mcnt_t fc, mu_t *frame) {
    mu_assert(mu_isfn(f));
    mu_frameconvert(fc, mfn(f)->args, frame);

    switch (mfn(f)->flags & (MFN_BUILTIN | MFN_SCOPED)) {
        case MFN_BUILTIN: {
            mbfn_t *bfn = mfn(f)->fn.bfn;
            mu_fn_dec(f);
            return bfn(frame);
        }

        case MFN_BUILTIN | MFN_SCOPED: {
            mcnt_t rc = mfn(f)->fn.sbfn(mfn(f)->closure, frame);
            mu_fn_dec(f);
            return rc;
        }

        case MFN_SCOPED: {
            mu_t c = mu_code_inc(mfn(f)->fn.code);
            mu_t scope = mu_tbl_create(mu_code_getscope(c));
            mu_tbl_settail(scope, mu_fn_getclosure(f));
            mu_fn_dec(f);
            return mu_exec(c, scope, frame);
        }
    }

    mu_unreachable;
}

void mu_fn_fcall(mu_t f, mcnt_t fc, mu_t *frame) {
    mu_assert(mu_isfn(f));
    mcnt_t rets = mu_fn_tcall(mu_inc(f), fc >> 4, frame);
    mu_frameconvert(rets, fc & 0xf, frame);
}

mu_t mu_fn_vcall(mu_t f, mcnt_t fc, va_list args) {
    mu_assert(mu_isfn(f));
    mu_t frame[MU_FRAME];

    for (muint_t i = 0; i < mu_framecount(fc >> 4); i++) {
        frame[i] = va_arg(args, mu_t);
    }

    mu_fn_fcall(f, fc, frame);

    for (muint_t i = 1; i < mu_framecount(0xf & fc); i++) {
        *va_arg(args, mu_t *) = frame[i];
    }

    return (0xf & fc) ? *frame : 0;
}

mu_t mu_fn_call(mu_t f, mcnt_t fc, ...) {
    mu_assert(mu_isfn(f));
    va_list args;
    va_start(args, fc);
    mu_t ret = mu_fn_vcall(f, fc, args);
    va_end(args);
    return ret;
}


// Iteration
bool mu_fn_next(mu_t f, mcnt_t fc, mu_t *frame) {
    mu_assert(mu_isfn(f));
    mu_fn_fcall(f, (fc == 0) ? 1 : fc, frame);

    if (fc != 0xf) {
        if (frame[0]) {
            if (fc == 0) {
                mu_dec(frame[0]);
            }
            return true;
        } else {
            mu_frameconvert(fc, 0, frame);
            return false;
        }
    } else {
        mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        if (m) {
            mu_dec(m);
            return true;
        } else {
            mu_tbl_dec(frame[0]);
            return false;
        }
    }
}


// Function related Mu functions

static mcnt_t mu_fn_bfn(mu_t *frame) {
    mu_t m = mu_fn_frommu(mu_inc(frame[0]));
    mu_checkargs(m, MU_FN_KEY, 0x1, frame);
    mu_dec(frame[0]);
    frame[0] = m;
    return 1;
}

MU_DEF_STR(mu_fn_key_def, "fn_")
MU_DEF_BFN(mu_fn_def, 0x1, mu_fn_bfn)

// Binds arguments to function
static mcnt_t mu_fn_bound(mu_t scope, mu_t *frame) {
    mu_t f = mu_tbl_lookup(scope, mu_num_fromuint(0));
    mu_t args = mu_tbl_lookup(scope, mu_num_fromuint(1));

    frame[0] = mu_tbl_concat(args, frame[0], 0);
    return mu_fn_tcall(f, 0xf, frame);
}

mu_t mu_fn_bind(mu_t f, mu_t g) {
    mu_assert(mu_isfn(f) && mu_istbl(g));
    return mu_fn_fromsbfn(0xf, mu_fn_bound,
            mu_tbl_fromlist((mu_t[]){mu_inc(f), g}, 2));
}

static mcnt_t mu_bind_bfn(mu_t *frame) {
    mu_t f = mu_tbl_pop(frame[0], 0);
    mu_checkargs(mu_isfn(f), MU_BIND_KEY, 0x2, (mu_t[]){f, frame[0]});

    frame[0] = mu_fn_bind(f, frame[0]);
    mu_dec(f);
    return 1;
}

MU_DEF_STR(mu_bind_key_def, "bind")
MU_DEF_BFN(mu_bind_def, 0xf, mu_bind_bfn)

static mcnt_t mu_fn_composed(mu_t fs, mu_t *frame) {
    mcnt_t c = 0xf;
    for (muint_t i = mu_tbl_getlen(fs)-1; i+1 > 0; i--) {
        mu_t f = mu_tbl_lookup(fs, mu_num_fromuint(i));
        c = mu_fn_tcall(f, c, frame);
    }

    return c;
}

mu_t mu_fn_comp(mu_t f, mu_t g) {
    mu_assert(mu_isfn(f) && mu_isfn(g));
    return mu_fn_fromsbfn(0xf, mu_fn_composed,
            mu_tbl_fromlist((mu_t[]){mu_inc(f), g}, 2));
}

static mcnt_t mu_comp_bfn(mu_t *frame) {
    mu_t f = frame[0];
    mu_t g = frame[1];
    mu_checkargs(mu_isfn(f) && mu_isfn(g), MU_COMP_KEY, 0xf, frame);

    frame[0] = mu_fn_comp(f, g);
    mu_dec(f);
    return 1;
}

MU_DEF_STR(mu_comp_key_def, "@")
MU_DEF_BFN(mu_comp_def, 0x2, mu_comp_bfn)

