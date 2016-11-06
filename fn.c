#include "fn.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "parse.h"
#include "vm.h"


// Function access
mu_inline struct mfn *mfn(mu_t f) {
    return (struct mfn *)((muint_t)f - MTFN);
}


// Creation functions
mu_t mu_fn_fromcode(mu_t c, mu_t closure) {
    if (mu_code_getheader(c)->flags & MFN_WEAK) {
        mu_assert(mu_getref(closure) > 1);
        mu_dec(closure);
    }

    struct mfn *f = mu_ref_alloc(sizeof(struct mfn));
    f->args = mu_code_getheader(c)->args;
    f->flags = mu_code_getheader(c)->flags;
    f->closure = closure;
    f->fn.code = c;
    return (mu_t)((muint_t)f + MTFN);
}

mu_t mu_fn_frombfn(mcnt_t args, mbfn_t *bfn) {
    struct mfn *f = mu_ref_alloc(sizeof(struct mfn));
    f->args = args;
    f->flags = MFN_BUILTIN;
    f->closure = 0;
    f->fn.bfn = bfn;
    return (mu_t)((muint_t)f + MTFN);
}

mu_t mu_fn_fromsbfn(mcnt_t args, msbfn_t *sbfn, mu_t closure) {
    struct mfn *f = mu_ref_alloc(sizeof(struct mfn));
    f->args = args;
    f->flags = MFN_BUILTIN | MFN_SCOPED;
    f->closure = closure;
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

    mu_ref_dealloc(mfn(f), sizeof(struct mfn));
}

void mu_code_destroy(mu_t c) {
    for (muint_t i = 0; i < mu_code_getimmslen(c); i++) {
        mu_dec(mu_code_getimms(c)[i]);
    }
}


// C interface for calling functions
mcnt_t mu_fn_tcall(mu_t f, mcnt_t fc, mu_t *frame) {
    mu_frame_convert(fc, mfn(f)->args, frame);

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
            mu_t scope = mu_tbl_extend(
                    mu_code_getheader(c)->scope, mu_fn_getclosure(f));
            mu_fn_dec(f);
            return mu_exec(c, scope, frame);
        }
    }

    mu_unreachable;
}

void mu_fn_fcall(mu_t f, mcnt_t fc, mu_t *frame) {
    mcnt_t rets = mu_fn_tcall(mu_inc(f), fc >> 4, frame);
    mu_frame_convert(rets, fc & 0xf, frame);
}

mu_t mu_fn_vcall(mu_t f, mcnt_t fc, va_list args) {
    mu_t frame[MU_FRAME];

    for (muint_t i = 0; i < mu_frame_len(fc >> 4); i++) {
        frame[i] = va_arg(args, mu_t);
    }

    mu_fn_fcall(f, fc, frame);

    for (muint_t i = 1; i < mu_frame_len(0xf & fc); i++) {
        *va_arg(args, mu_t *) = frame[i];
    }

    return (0xf & fc) ? *frame : 0;
}

mu_t mu_fn_call(mu_t f, mcnt_t fc, ...) {
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
            mu_frame_convert(fc, 0, frame);
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
static mcnt_t mu_bfn_fn(mu_t *frame) {
    mu_t m = frame[0];

    switch (mu_gettype(m)) {
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

MU_GEN_STR(mu_gen_key_fn2, "fn_")
MU_GEN_BFN(mu_gen_fn, 0x1, mu_bfn_fn)

// Default function
static mcnt_t mu_bfn_id(mu_t *frame) {
    return 0xf;
}

MU_GEN_STR(mu_gen_key_id, "id")
MU_GEN_BFN(mu_gen_id, 0xf, mu_bfn_id)

// Binds arguments to function
static mcnt_t mu_fn_bound(mu_t scope, mu_t *frame) {
    mu_t f = mu_tbl_lookup(scope, mu_num_fromuint(0));
    mu_t args = mu_tbl_lookup(scope, mu_num_fromuint(1));

    frame[0] = mu_tbl_concat(args, frame[0], 0);
    return mu_fn_tcall(f, 0xf, frame);
}

static mcnt_t mu_bfn_bind(mu_t *frame) {
    mu_t f = mu_tbl_pop(frame[0], 0);
    if (!mu_isfn(f)) {
        mu_error_arg(MU_KEY_BIND, 0x2, (mu_t[]){f, frame[0]});
    }

    frame[0] = mu_fn_fromsbfn(0xf, mu_fn_bound,
            mu_tbl_fromlist((mu_t[]){f, frame[0]}, 2));
    return 1;
}

MU_GEN_STR(mu_gen_key_bind, "bind")
MU_GEN_BFN(mu_gen_bind, 0xf, mu_bfn_bind)

static mcnt_t mu_fn_composed(mu_t fs, mu_t *frame) {
    mcnt_t c = 0xf;
    for (muint_t i = mu_tbl_getlen(fs)-1; i+1 > 0; i--) {
        mu_t f = mu_tbl_lookup(fs, mu_num_fromuint(i));
        c = mu_fn_tcall(f, c, frame);
    }

    return c;
}

static mcnt_t mu_bfn_comp(mu_t *frame) {
    mu_t f;
    for (muint_t i = 0; mu_tbl_next(frame[0], &i, 0, &f);) {
        if (!mu_isfn(f)) {
            mu_error_arg(MU_KEY_COMP, 0xf, frame);
        }
    }

    frame[0] = mu_fn_fromsbfn(0xf, mu_fn_composed, frame[0]);
    return 1;
}

MU_GEN_STR(mu_gen_key_comp, "comp")
MU_GEN_BFN(mu_gen_comp, 0xf, mu_bfn_comp)

