#include "fn.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "parse.h"
#include "vm.h"

#include <string.h>


// Functions for managing functions
// Each function is preceded with a reference count
// which is used as its handle in a var

static fn_t *fn_realize(struct fnparse *fnparse, eh_t *eh) {
    // this is a bit tricky since fn and p->fn share memory
    fn_t *fn = (fn_t *)fnparse;
    tbl_t *vars = fnparse->vars;
    tbl_t *fns = fnparse->fns;
    
    fn->vcount = vars->len;
    fn->fcount = fns->len;

    fn->vars = mu_alloc(fn->vcount*sizeof(var_t) + 
                        fn->fcount*sizeof(fn_t *), eh);

    tbl_for_begin (k, v, vars) {
        fn->vars[getraw(v)] = k;
    } tbl_for_end;

    int i = 0;
    fn->fns = (fn_t**)&fn->vars[fn->vcount];

    tbl_for_begin (k, v, fns) {
        fn->fns[i++] = (fn_t*)getraw(v);
    } tbl_for_end;
    
    tbl_dec(vars);
    tbl_dec(fns);

    return fn;
}


fn_t *fn_create(tbl_t *args, var_t code, eh_t *eh) {
    parse_t *p = mu_alloc(sizeof(parse_t), eh);
    p->fn = ref_alloc(sizeof(fn_t), eh);
    p->fn->ins = 0;
    p->fn->len = 4;
    p->fn->bcode = mu_alloc(p->fn->len, eh);

    p->fn->vars = tbl_create(args ? tbl_len(args) : 0, eh);
    p->fn->fns = tbl_create(0, eh);

    mu_parse_init(p, code);
    mu_parse_args(p, args);
    mu_parse_top(p);
    p->fn->stack = 25; // TODO make this reasonable

    fn_t *fn = fn_realize(p->fn, eh);

    mu_dealloc(p, sizeof(parse_t));

    return fn;
}

fn_t *fn_create_nested(tbl_t *args, parse_t *p, eh_t *eh) {
    p->fn = ref_alloc(sizeof(fn_t), eh);
    p->fn->ins = 0;
    p->fn->len = 4;
    p->fn->bcode = mu_alloc(p->fn->len, eh);

    p->fn->vars = tbl_create(args ? tbl_len(args) : 0, eh);
    p->fn->fns = tbl_create(0, eh);

    mu_parse_args(p, args);
    mu_parse_nested(p);
    p->fn->stack = 25; // TODO make this reasonable

    return fn_realize(p->fn, eh);
}


// Called by garbage collector to clean up
void fn_destroy(void *v) {
    fn_t *fn = v;
    int i;

    for (i=0; i < fn->vcount; i++) {
        var_dec(fn->vars[i]);
    }

    for (i=0; i < fn->fcount; i++) {
        fn_dec(fn->fns[i]);
    }

    mu_dealloc((void *)fn->bcode, fn->bcount);
    mu_dealloc(fn->vars, fn->vcount*sizeof(var_t) + fn->fcount*sizeof(fn_t *));
    ref_dealloc(v, sizeof(fn_t));
}


// Call a function. Each function takes a table
// of arguments, and returns a single variable.
var_t fn_call(fn_t *fn, tbl_t *args, tbl_t *closure, eh_t *eh) {
    tbl_t *scope = tbl_create(1, eh);
    tbl_insert(scope, vcstr("args"), vtbl(args), eh);
    scope->tail = closure;

    return mu_exec(fn, args, scope, eh);
}


// Returns a string representation of a function
var_t fn_repr(var_t v, eh_t *eh) {
    uint32_t bits = (uint32_t)getfn(v);
    mstr_t *out = str_create(13, eh);
    mstr_t *res = out;
    int i;

    memcpy(res, "fn 0x", 5);
    res += 5;

    for (i = 0; i < 8; i++) {
        *res++ = num_ascii(0xf & (bits >> (8*(4-i))));
    }

    return vstr(out, 0, 13);
}
