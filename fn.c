#include "fn.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "parse.h"
#include "vm.h"
#include <string.h>
#include <stdarg.h>


// Internally used conversion between mu_t and struct tbl
mu_inline mu_t mfn(struct fn *fn) {
    return (mu_t)((uint_t)fn + MU_FN);
}

mu_inline struct fn *fn_fn(mu_t m) {
    return (struct fn *)((uint_t)m - MU_FN);
}


// C Function creating functions and macros
mu_t mbfn(frame_t args, bfn_t *bfn) {
    struct fn *fn = ref_alloc(sizeof(struct fn));
    fn->flags.regs = 16;
    fn->flags.scope = 0;
    fn->flags.args = args;
    fn->flags.type = 1;
    fn->closure = 0;
    fn->bfn = bfn;
    return mfn(fn);
}

mu_t msbfn(frame_t args, sbfn_t *sbfn, mu_t closure) {
    struct fn *fn = ref_alloc(sizeof(struct fn));
    fn->flags.regs = 16;
    fn->flags.scope = 0;
    fn->flags.args = args;
    fn->flags.type = 2;
    fn->closure = closure;
    fn->sbfn = sbfn;
    return mfn(fn);
}

mu_t fn_create(struct code *code, mu_t closure) {
    struct fn *fn = ref_alloc(sizeof(struct fn));
    fn->flags = code->flags;
    fn->closure = closure;
    fn->code = code;
    return mfn(fn);
}

mu_t fn_parse_fn(mu_t code, mu_t closure) {
    return fn_create(mu_parse_fn(code), closure);
}

mu_t fn_parse_module(mu_t code, mu_t closure) {
    return fn_create(mu_parse_module(code), closure);
}


// Called by garbage collector to clean up
void fn_destroy(mu_t m) {
    struct fn *f = fn_fn(m);

    if (f->closure)
        tbl_dec(f->closure);

    if (f->flags.type == 0)
        code_dec(f->code);

    ref_dealloc(f, sizeof(struct fn));
}

void code_destroy(struct code *code) {
    for (uint_t i = 0; i < code->icount; i++)
        mu_dec(((mu_t*)code->data)[i]);

    for (uint_t i = 0; i < code->fcount; i++)
        code_dec((struct code *)code->data[code->icount + i]);

    ref_dealloc(code, sizeof(struct code) + code->bcount + 
                      code->icount + code->fcount);
}


// C interface for calling functions
static void bfn_fcall(struct fn *fn, frame_t c, mu_t *frame) {
    mu_fconvert(fn->flags.args, frame, c >> 4, frame);
    frame_t rets = fn->bfn(frame);
    mu_fconvert(c & 0xf, frame, rets, frame);
}

static void sbfn_fcall(struct fn *fn, frame_t c, mu_t *frame) {
    mu_fconvert(fn->flags.args, frame, c >> 4, frame);
    uint_t rets = fn->sbfn(fn->closure, frame);
    mu_fconvert(c & 0xf, frame, rets, frame);
}

void fn_fcall(mu_t m, frame_t c, mu_t *frame) {
    static void (*const fn_fcalls[3])(struct fn *, frame_t, mu_t *) = {
        mu_exec, bfn_fcall, sbfn_fcall
    };

    struct fn *f = fn_fn(m);
    return fn_fcalls[f->flags.type](f, c, frame);
}

mu_t fn_vcall(mu_t m, frame_t c, va_list args) {
    mu_t frame[MU_FRAME];

    mu_toframe(c >> 4, frame, args);
    fn_fcall(m, c, frame);
    return mu_fromframe(0xf & c, frame, args);
}

mu_t fn_call(mu_t m, frame_t c, ...) {
    va_list args;
    va_start(args, c);
    mu_t ret = mu_vcall(m, c, args);
    va_end(args);
    return ret;
}


// Returns a string representation of a function
mu_t fn_repr(mu_t f) {
    uint_t bits = (uint_t)fn_fn(f);

    struct str *m = mstr_create(5 + 2*sizeof(uint_t));
    byte_t *out = mstr_bytes(m);

    memcpy(out, "fn 0x", 5);
    out += 5;

    for (uint_t i = 0; i < 2*sizeof(uint_t); i++) {
        *out++ = num_ascii(0xf & (bits >> (4*(sizeof(uint_t)-i))));
    }

    return str_intern(m, mstr_len(m));
}
