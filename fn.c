#include "fn.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "parse.h"
#include "vm.h"

#include <string.h>


// C Function creating functions and macros
fn_t *fn_bfn(bfn_t *f, eh_t *eh) {
    fn_t *m = ref_alloc(sizeof(fn_t), eh);
    m->stack = 25; // TODO make this reasonable
    m->type = 0;
    m->closure = 0;
    m->imms = 0;
    m->bfn = f;
    return m;
}

fn_t *fn_sbfn(sbfn_t *f, tbl_t *scope, eh_t *eh) {
    fn_t *m = ref_alloc(sizeof(fn_t), eh);
    m->stack = 25; // TODO make this reasonable
    m->type = 1;
    m->closure = scope;
    m->imms = 0;
    m->sbfn = f;
    return m;
}

fn_t *fn_closure(fn_t *f, tbl_t *scope, eh_t *eh) {
    fn_t *m = ref_alloc(sizeof(fn_t), eh);
    m->stack = f->stack;
    m->type = f->type;
    m->closure = scope;
    m->imms = f->imms;
    m->bcode = f->bcode;
    return m;
}

static fn_t *fn_realize(struct fnparse *f, eh_t *eh) {
    f->stack = 25; // TODO make this reasonable
    f->type = 2;
    f->closure = 0;

    // this is a bit tricky since the fn memory is reused
    // and we want to keep the resulting imms as a list if possible
    len_t len = tbl_getlen(f->imms);

    mu_t *imms = mu_alloc(len * sizeof(mu_t), eh);
    tbl_for_begin (k, v, f->imms) {
        imms[getuint(v)] = k;
    } tbl_for_end;

    tbl_dec(f->imms);
    f->imms = tbl_create(len, eh);
    len_t i;

    for (i = 0; i < len; i++) {
        tbl_append(f->imms, imms[i], eh);
    }

    mu_dealloc(imms, len);

    return (fn_t *)f;
}


fn_t *fn_create(tbl_t *args, mu_t code, eh_t *eh) {
    parse_t *p = mu_parse_create(code, eh);

    p->f = ref_alloc(sizeof(fn_t), eh);
    p->f->ins = 0;
    p->f->imms = tbl_create(0, eh);
    p->f->bcode = mstr_create(MU_MINALLOC, eh);

    mu_parse_args(p, args);
    mu_parse_stmts(p);
    mu_parse_end(p);

    fn_t *f = fn_realize(p->f, eh);
    mu_parse_destroy(p);
    return f;
}

fn_t *fn_create_expr(tbl_t *args, mu_t code, eh_t *eh) {
    parse_t *p = mu_parse_create(code, eh);

    p->f = ref_alloc(sizeof(fn_t), eh);
    p->f->ins = 0;
    p->f->imms = tbl_create(0, eh);
    p->f->bcode = mstr_create(MU_MINALLOC, eh);

    mu_parse_args(p, args);
    mu_parse_expr(p);
    mu_parse_end(p);

    fn_t *f = fn_realize(p->f, eh);
    mu_parse_destroy(p);
    return f;
}

fn_t *fn_create_nested(tbl_t *args, parse_t *p, eh_t *eh) {
    p->f = ref_alloc(sizeof(fn_t), eh);
    p->f->ins = 0;
    p->f->imms = tbl_create(0, eh);
    p->f->bcode = mstr_create(MU_MINALLOC, eh);

    mu_parse_args(p, args);
    mu_parse_stmt(p);

    return fn_realize(p->f, eh);
}

// Called by garbage collector to clean up
void fn_destroy(fn_t *f) {
    if (f->closure)
        tbl_dec(f->closure);

    if (f->type == 2) {
        tbl_dec(f->imms);
        str_dec(f->bcode);
    }

    ref_dealloc((ref_t *)f, sizeof(fn_t));
}


// Call a function. Each function takes a table
// of arguments, and returns a single variable.
static mu_t bfn_call(fn_t *f, tbl_t *args, eh_t *eh) { 
    return f->bfn(args, eh); 
}

static mu_t sbfn_call(fn_t *f, tbl_t *args, eh_t *eh) { 
    return f->sbfn(args, f->closure, eh); 
}

static mu_t mufn_call(fn_t *f, tbl_t *args, eh_t *eh) {
    tbl_t *scope = tbl_create(1, eh);
    tbl_insert(scope, mcstr("args", eh), mtbl(args), eh);
    scope->tail = f->closure;

    return mu_exec(f, args, scope, eh);
}

mu_t fn_call(fn_t *f, tbl_t *args, eh_t *eh) {
    static mu_t (* const fn_calls[3])(fn_t *, tbl_t *, eh_t *) = {
        bfn_call, sbfn_call, mufn_call
    };

    return fn_calls[f->type](f, args, eh);
}


static mu_t bfn_call_in(fn_t *f, tbl_t *args, tbl_t *scope, eh_t *eh) { 
    return f->bfn(args, eh); 
}

static mu_t sbfn_call_in(fn_t *f, tbl_t *args, tbl_t *scope, eh_t *eh) { 
    return f->sbfn(args, scope, eh); 
}

static mu_t mufn_call_in(fn_t *f, tbl_t *args, tbl_t *scope, eh_t *eh) {
    return mu_exec(f, args, scope, eh);
}

mu_t fn_call_in(fn_t *f, tbl_t *args, tbl_t *scope, eh_t *eh) {
    static mu_t (* const fn_call_ins[3])(fn_t *, tbl_t *, tbl_t *, eh_t *) = {
        bfn_call_in, sbfn_call_in, mufn_call_in
    };

    return fn_call_ins[f->type](f, args, scope, eh);
}


// Returns a string representation of a function
str_t *fn_repr(fn_t *f, eh_t *eh) {
    uint_t bits = (uint_t)f->bcode;

    mstr_t *m = mstr_create(5 + 2*sizeof(uint_t), eh);
    data_t *out = m->data;

    memcpy(out, "fn 0x", 5);
    out += 5;

    uint_t i;
    for (i = 0; i < 2*sizeof(uint_t); i++) {
        *out++ = num_ascii(0xf & (bits >> (4*(sizeof(uint_t)-i))));
    }

    return str_intern(m, eh);
}
