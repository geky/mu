#include "fn.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "vm.h"
#include "parse.h"
#include <string.h>
#include <stdarg.h>


// C Function creating functions and macros
fn_t *fn_bfn(c_t args, bfn_t *bfn) {
    fn_t *fn = ref_alloc(sizeof(fn_t));
    fn->flags.stack = 25;
    fn->flags.scope = 0;
    fn->flags.args = mu_args(args);
    fn->flags.type = 1;
    fn->closure = 0;
    fn->bfn = bfn;
    return fn;
}

fn_t *fn_sbfn(c_t args, sbfn_t *sbfn, tbl_t *closure) {
    fn_t *fn = ref_alloc(sizeof(fn_t));
    fn->flags.stack = 25;
    fn->flags.scope = 0;
    fn->flags.args = mu_args(args);
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
static void bfn_fcall(fn_t *fn, c_t c, mu_t *frame) {
    mu_fconvert(mu_args(c), frame, fn->flags.args, frame);
    c_t rets = fn->bfn(frame);
    mu_fconvert(rets, frame, mu_rets(c), frame);
}

static void sbfn_fcall(fn_t *fn, c_t c, mu_t *frame) { 
    mu_fconvert(mu_args(c), frame, fn->flags.args, frame);
    uint_t rets = fn->sbfn(fn->closure, frame);
    mu_fconvert(rets, frame, mu_rets(c), frame);
}

void fn_fcall(fn_t *fn, c_t c, mu_t *frame) {
    static void (*const fn_fcalls[3])(fn_t *, c_t, mu_t *) = {
        mu_exec, bfn_fcall, sbfn_fcall
    };

    return fn_fcalls[fn->flags.type](fn, c, frame);
}

mu_t fn_call(fn_t *fn, c_t c, ...) {
    va_list args;
    mu_t frame[MU_FRAME];

    va_start(args, c);

    if (mu_args(c) == 0xf) {
        frame[0] = va_arg(args, mu_t);
    } else if (mu_args(c) > MU_FRAME) {
        tbl_t *tbl = tbl_create(mu_args(c));
        for (uint_t i = 0; i < mu_args(c); i++)
            tbl_insert(tbl, muint(i), va_arg(args, mu_t));
        frame[0] = mtbl(tbl);
    } else {
        for (uint_t i = 0; i < mu_args(c); i++) {
            frame[i] = va_arg(args, mu_t);
        }
    }

    fn_fcall(fn, c, frame);

    if (mu_rets(c) != 0xf) {
        if (mu_rets(c) > MU_FRAME) {
            tbl_t *tbl = gettbl(frame[0]);
            frame[0] = tbl_lookup(tbl, muint(0));
            for (uint_t i = 1; i < mu_rets(c); i++) {
                *va_arg(args, mu_t *) = tbl_lookup(tbl, muint(i));
            }
            tbl_dec(tbl);
        } else {
            for (uint_t i = 1; i < mu_rets(c); i++) {
                *va_arg(args, mu_t *) = frame[i];
            }
        }
    }

    va_end(args);

    return mu_rets(c) ? frame[0] : mnil;
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
