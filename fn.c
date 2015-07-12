#include "fn.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "parse.h"
#include "vm.h"
#include <string.h>
#include <stdarg.h>


// Conversion between different frame types
// Supports inplace conversion
void mu_fconvert(frame_t dcount, mu_t *dframe,
                 frame_t scount, mu_t *sframe) {
    if (dcount > MU_FRAME) {
        if (scount > MU_FRAME) {
            *dframe = *sframe;
        } else {
            tbl_t *tbl = tbl_create(scount);

            for (uint_t i = 0; i < scount; i++)
                tbl_insert(tbl, muint(i), sframe[i]);

            *dframe = mtbl(tbl);
        }
    } else {
        if (scount > MU_FRAME) {
            tbl_t *tbl = gettbl(*sframe);

            for (uint_t i = 0; i < dcount; i++)
                dframe[i] = tbl_lookup(tbl, muint(i));

            tbl_dec(tbl);
        } else {
            for (uint_t i = 0; i < scount && i < dcount; i++)
                dframe[i] = sframe[i];

            for (uint_t i = dcount; i < scount; i++)
                mu_dec(sframe[i]);

            for (uint_t i = scount; i < dcount; i++)
                dframe[i] = mnil;
        }
    }
}


// C Function creating functions and macros
fn_t *fn_bfn(frame_t args, bfn_t *bfn) {
    fn_t *fn = ref_alloc(sizeof(fn_t));
    fn->flags.regs = 16;
    fn->flags.scope = 0;
    fn->flags.args = args;
    fn->flags.type = 1;
    fn->closure = 0;
    fn->bfn = bfn;
    return fn;
}

fn_t *fn_sbfn(frame_t args, sbfn_t *sbfn, tbl_t *closure) {
    fn_t *fn = ref_alloc(sizeof(fn_t));
    fn->flags.regs = 16;
    fn->flags.scope = 0;
    fn->flags.args = args;
    fn->flags.type = 2;
    fn->closure = closure;
    fn->sbfn = sbfn;
    return fn;
}

fn_t *fn_create(code_t *code, tbl_t *closure) {
    fn_t *fn = ref_alloc(sizeof(fn_t));
    fn->flags = code->flags;
    fn->closure = closure;
    fn->code = code;
    return fn;
}

fn_t *fn_parse_fn(str_t *code, tbl_t *closure) {
    return fn_create(mu_parse_fn(code), closure);
}

fn_t *fn_parse_module(str_t *code, tbl_t *closure) {
    return fn_create(mu_parse_module(code), closure);
}


// Called by garbage collector to clean up
void fn_destroy(fn_t *fn) {
    if (fn->closure)
        tbl_dec(fn->closure);

    if (fn->flags.type == 0)
        code_dec(fn->code);

    ref_dealloc(fn, sizeof(fn_t));
}

void code_destroy(code_t *code) {
    for (uint_t i = 0; i < code->icount; i++)
        mu_dec(((mu_t*)code->data)[i]);

    for (uint_t i = 0; i < code->fcount; i++)
        code_dec((code_t *)code->data[code->icount + i]);

    ref_dealloc(code, sizeof(code_t) + code->bcount + 
                      code->icount + code->fcount);
}


// C interface for calling functions
static void bfn_fcall(fn_t *fn, frame_t c, mu_t *frame) {
    mu_fconvert(fn->flags.args, frame, c >> 4, frame);
    frame_t rets = fn->bfn(frame);
    mu_fconvert(c & 0xf, frame, rets, frame);
}

static void sbfn_fcall(fn_t *fn, frame_t c, mu_t *frame) {
    mu_fconvert(fn->flags.args, frame, c >> 4, frame);
    uint_t rets = fn->sbfn(fn->closure, frame);
    mu_fconvert(c & 0xf, frame, rets, frame);
}

void fn_fcall(fn_t *fn, frame_t c, mu_t *frame) {
    static void (*const fn_fcalls[3])(fn_t *, frame_t, mu_t *) = {
        mu_exec, bfn_fcall, sbfn_fcall
    };

    return fn_fcalls[fn->flags.type](fn, c, frame);
}

mu_t fn_call(fn_t *fn, frame_t c, ...) {
    va_list args;
    mu_t frame[MU_FRAME];

    va_start(args, c);

    if ((c >> 4) == 0xf) {
        frame[0] = va_arg(args, mu_t);
    } else if ((c >> 4) > MU_FRAME) {
        tbl_t *tbl = tbl_create(c >> 4);

        for (uint_t i = 0; i < (c >> 4); i++)
            tbl_insert(tbl, muint(i), va_arg(args, mu_t));

        frame[0] = mtbl(tbl);
    } else {
        for (uint_t i = 0; i < (c >> 4); i++)
            frame[i] = va_arg(args, mu_t);
    }

    fn_fcall(fn, c, frame);

    if ((c & 0xf) != 0xf) {
        if ((c & 0xf) > MU_FRAME) {
            tbl_t *tbl = gettbl(frame[0]);
            frame[0] = tbl_lookup(tbl, muint(0));

            for (uint_t i = 1; i < (c & 0xf); i++)
                *va_arg(args, mu_t *) = tbl_lookup(tbl, muint(i));

            tbl_dec(tbl);
        } else {
            for (uint_t i = 1; i < (c & 0xf); i++) {
                *va_arg(args, mu_t *) = frame[i];
            }
        }
    }

    va_end(args);

    return (c & 0xf) ? frame[0] : mnil;
}


// Returns a string representation of a function
str_t *fn_repr(fn_t *fn) {
    uint_t bits = (uint_t)fn;

    mstr_t *m = mstr_create(5 + 2*sizeof(uint_t));
    data_t *out = m->data;

    memcpy(out, "fn 0x", 5);
    out += 5;

    for (uint_t i = 0; i < 2*sizeof(uint_t); i++) {
        *out++ = num_ascii(0xf & (bits >> (4*(sizeof(uint_t)-i))));
    }

    return str_intern(m, m->len);
}
