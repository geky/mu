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
    f->closure = 0;
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
    f->type = c->type;
    f->code = c;
    f->closure = closure;
    return (mu_t)((uint_t)f + MU_FN);
}


// Called by garbage collector to clean up
void bfn_destroy(mu_t f) {
    ref_dealloc(f, sizeof(struct fn));
}

void sbfn_destroy(mu_t f) {
    mu_dec(fn_closure(f));
    ref_dealloc(f, sizeof(struct fn));
}

void fn_destroy(mu_t f) {
    mu_dec(fn_closure(f));
    code_dec(fn_code(f));
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
}

void sbfn_fcall(mu_t m, frame_t fc, mu_t *frame) {
    struct fn *f = fn_fn(m);
    mu_fconvert(f->args, frame, fc >> 4, frame);
    frame_t rets = f->sbfn(f->closure, frame);
    mu_fconvert(fc & 0xf, frame, rets, frame);
}

void fn_fcall(mu_t m, frame_t fc, mu_t *frame) {
    struct fn *f = fn_fn(m);
    mu_t scope = tbl_extend(f->code->scope, f->closure);
    mu_exec(f->code, scope, fc, frame);
}


// Returns a string representation of a function
mu_t fn_repr(mu_t f) {
    uint_t bits = (uint_t)fn_fn(f);

    byte_t *s = mstr_create(5 + 2*sizeof(uint_t));
    memcpy(s, "fn 0x", 5);

    for (uint_t i = 0; i < 2*sizeof(uint_t); i++) {
        s[i+5] = num_ascii(0xf & (bits >> (4*(sizeof(uint_t)-i))));
    }

    return mstr_intern(s, 5 + 2*sizeof(uint_t));
}
