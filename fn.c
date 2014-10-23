#include "fn.h"

#include "mem.h"
#include "str.h"
#include "lex.h"
#include "parse.h"
#include "vm.h"


// Functions for managing functions
// Each function is preceded with a reference count
// which is used as its handle in a var

static fn_t *fn_realize(struct fnstate *fnstate, eh_t *eh) {
    // this is a bit tricky since fn and m->fn share memory
    fn_t *fn = (fn_t *)fnstate;
    tbl_t *vars = fnstate->vars;
    tbl_t *fns = fnstate->fns;
    
    fn->vcount = vars->len;
    fn->fcount = fns->len;

    fn->vars = mu_alloc(fn->vcount*sizeof(var_t) + 
                        fn->fcount*sizeof(fn_t *), eh);
    tbl_for_begin (k, v, vars) {
        fn->vars[v.data] = k;
    } tbl_for_end;

    fn->fns = (fn_t**)&fn->vars[fn->vcount];
    int i = 0;
    tbl_for_begin (k, v, fns) {
        fn->fns[i++] = (fn_t*)v.data;
    } tbl_for_end;
    
    tbl_dec(vars);
    tbl_dec(fns);

    return fn;
}


fn_t *fn_create(tbl_t *args, var_t code, eh_t *eh) {
    args = tbl_readp(args);

    mstate_t *m = mu_alloc(sizeof(mstate_t), eh);
    m->fn = ref_alloc(sizeof(fn_t), eh);
    m->fn->ins = 0;
    m->fn->len = 4;
    m->fn->bcode = mu_alloc(m->fn->len, eh);

    m->fn->vars = tbl_create(args ? args->len : 0, eh);
    m->fn->fns = tbl_create(0, eh);

    mu_parse_init(m, code);
    mu_parse_args(m, args);
    mu_parse_top(m);
    m->fn->stack = 25; // TODO make this reasonable

    fn_t *fn = fn_realize(m->fn, eh);

    mu_dealloc(m, sizeof(mstate_t));

    return fn;
}

fn_t *fn_create_nested(tbl_t *args, void *mmem, eh_t *eh) {
    args = tbl_readp(args);

    mstate_t *m = mmem;
    m->fn = ref_alloc(sizeof(fn_t), eh);
    m->fn->ins = 0;
    m->fn->len = 4;
    m->fn->bcode = mu_alloc(m->fn->len, eh);

    m->fn->vars = tbl_create(args ? args->len : 0, eh);
    m->fn->fns = tbl_create(0, eh);

    mu_parse_args(m, args);
    mu_parse_nested(m);
    m->fn->stack = 25; // TODO make this reasonable

    return fn_realize(m->fn, eh);
}


// Called by garbage collector to clean up
void fn_destroy(void *m) {
    fn_t *fn = m;
    int i;

    for (i=0; i < fn->vcount; i++) {
        var_dec(fn->vars[i]);
    }

    for (i=0; i < fn->fcount; i++) {
        fn_dec(fn->fns[i]);
    }

    mu_dealloc((void *)fn->bcode, fn->bcount);
    mu_dealloc(fn->vars, fn->vcount*sizeof(var_t) + fn->fcount*sizeof(fn_t *));
    ref_dealloc(m, sizeof(fn_t));
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
    v.type &= ~3;

    str_t *out = str_create(13, eh);
    str_t *res = out;
    int i;

    *res++ = 'f';
    *res++ = 'n';
    *res++ = ' ';
    *res++ = '0';
    *res++ = 'x';

    for (i = 28; i >= 0; i -= 4) {
        *res++ = num_ascii(0xf & (v.meta >> i));
    }

    return vstr(out, 0, 13);
}
