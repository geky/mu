#include "fn.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "parse.h"
#include "vm.h"
#include <string.h>


// C Function creating functions and macros
fn_t *fn_bfn(bfn_t *f) {
    fn_t *m = ref_alloc(sizeof(fn_t));
    m->stack = 25; // TODO make this reasonable
    m->type = 0;
    m->closure = 0;
    m->imms = 0;
    m->bfn = f;
    return m;
}

fn_t *fn_sbfn(sbfn_t *f, tbl_t *scope) {
    fn_t *m = ref_alloc(sizeof(fn_t));
    m->stack = 25; // TODO make this reasonable
    m->type = 1;
    m->closure = scope;
    m->imms = 0;
    m->sbfn = f;
    return m;
}

fn_t *fn_closure(fn_t *f, tbl_t *scope) {
    fn_t *m = ref_alloc(sizeof(fn_t));
    m->stack = f->stack;
    m->type = f->type;
    m->closure = scope;
    m->imms = f->imms;
    m->bcode = f->bcode;
    return m;
}

static fn_t *fn_realize(struct fnparse *f) {
    f->stack = 25; // TODO make this reasonable
    f->type = 2;
    f->closure = 0;

    // this is a bit tricky since the fn memory is reused
    // and we want to keep the resulting imms as a list if possible
    len_t len = tbl_getlen(f->imms);

    mu_t *imms = mu_alloc(len * sizeof(mu_t));
    tbl_for_begin (k, v, f->imms) {
        imms[getuint(v)] = k;
    } tbl_for_end;

    tbl_dec(f->imms);
    f->imms = tbl_create(len);
    len_t i;

    for (i = 0; i < len; i++) {
        tbl_append(f->imms, imms[i]);
    }

    mu_dealloc(imms, len);

    return (fn_t *)f;
}


fn_t *fn_create(tbl_t *args, mu_t code) {
    parse_t *p = mu_parse_create(code);

    p->f = ref_alloc(sizeof(fn_t));
    p->f->ins = 0;
    p->f->imms = tbl_create(0);
    p->f->bcode = mstr_create(MU_MINALLOC);

    mu_parse_args(p, args);
    mu_parse_stmts(p);
    mu_parse_end(p);

    fn_t *f = fn_realize(p->f);
    mu_parse_destroy(p);
    return f;
}

fn_t *fn_create_expr(tbl_t *args, mu_t code) {
    parse_t *p = mu_parse_create(code);

    p->f = ref_alloc(sizeof(fn_t));
    p->f->ins = 0;
    p->f->imms = tbl_create(0);
    p->f->bcode = mstr_create(MU_MINALLOC);

    mu_parse_args(p, args);
    mu_parse_expr(p);
    mu_parse_end(p);

    fn_t *f = fn_realize(p->f);
    mu_parse_destroy(p);
    return f;
}

fn_t *fn_create_nested(tbl_t *args, parse_t *p) {
    p->f = ref_alloc(sizeof(fn_t));
    p->f->ins = 0;
    p->f->imms = tbl_create(0);
    p->f->bcode = mstr_create(MU_MINALLOC);

    mu_parse_args(p, args);
    mu_parse_stmt(p);

    return fn_realize(p->f);
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
static mu_t bfn_call(fn_t *f, tbl_t *args) { 
    return f->bfn(args); 
}

static mu_t sbfn_call(fn_t *f, tbl_t *args) { 
    return f->sbfn(args, f->closure); 
}

static mu_t mufn_call(fn_t *f, tbl_t *args) {
    tbl_t *scope = tbl_create(1);
    tbl_insert(scope, mcstr("args"), mtbl(args));
    scope->tail = f->closure;

    return mu_exec(f, args, scope);
}

mu_t fn_call(fn_t *f, tbl_t *args) {
    static mu_t (* const fn_calls[3])(fn_t *, tbl_t *) = {
        bfn_call, sbfn_call, mufn_call
    };

    return fn_calls[f->type](f, args);
}


static mu_t bfn_call_in(fn_t *f, tbl_t *args, tbl_t *scope) { 
    return f->bfn(args); 
}

static mu_t sbfn_call_in(fn_t *f, tbl_t *args, tbl_t *scope) { 
    return f->sbfn(args, scope); 
}

static mu_t mufn_call_in(fn_t *f, tbl_t *args, tbl_t *scope) {
    return mu_exec(f, args, scope);
}

mu_t fn_call_in(fn_t *f, tbl_t *args, tbl_t *scope) {
    static mu_t (* const fn_call_ins[3])(fn_t *, tbl_t *, tbl_t *) = {
        bfn_call_in, sbfn_call_in, mufn_call_in
    };

    return fn_call_ins[f->type](f, args, scope);
}


// Returns a string representation of a function
str_t *fn_repr(fn_t *f) {
    uint_t bits = (uint_t)f->bcode;

    mstr_t *m = mstr_create(5 + 2*sizeof(uint_t));
    data_t *out = m->data;

    memcpy(out, "fn 0x", 5);
    out += 5;

    uint_t i;
    for (i = 0; i < 2*sizeof(uint_t); i++) {
        *out++ = num_ascii(0xf & (bits >> (4*(sizeof(uint_t)-i))));
    }

    return str_intern(m);
}
