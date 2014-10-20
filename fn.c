#include "fn.h"

#include "mem.h"
#include "str.h"
#include "vlex.h"
#include "vparse.h"
#include "vm.h"


// Functions for managing functions
// Each function is preceded with a reference count
// which is used as its handle in a var

static fn_t *fn_realize(struct vfnstate *fnstate, veh_t *eh) {
    // this is a bit tricky since fn and vs->fn share memory
    fn_t *fn = (fn_t *)fnstate;
    tbl_t *vars = fnstate->vars;
    tbl_t *fns = fnstate->fns;
    
    fn->vcount = vars->len;
    fn->fcount = fns->len;

    fn->vars = v_alloc(fn->vcount*sizeof(var_t) + 
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


fn_t *fn_create(tbl_t *args, var_t code, veh_t *eh) {
    args = tbl_readp(args);

    vstate_t *vs = v_alloc(sizeof(vstate_t), eh);
    vs->fn = vref_alloc(sizeof(fn_t), eh);
    vs->fn->ins = 0;
    vs->fn->len = 4;
    vs->fn->bcode = v_alloc(vs->fn->len, eh);

    vs->fn->vars = tbl_create(args ? args->len : 0, eh);
    vs->fn->fns = tbl_create(0, eh);

    vparse_init(vs, code);
    vparse_args(vs, args);
    vparse_top(vs);
    vs->fn->stack = 25; // TODO make this reasonable

    fn_t *fn = fn_realize(vs->fn, eh);

    v_dealloc(vs, sizeof(vstate_t));

    return fn;
}

fn_t *fn_create_nested(tbl_t *args, void *vsmem, veh_t *eh) {
    args = tbl_readp(args);

    vstate_t *vs = vsmem;
    vs->fn = vref_alloc(sizeof(fn_t), eh);
    vs->fn->ins = 0;
    vs->fn->len = 4;
    vs->fn->bcode = v_alloc(vs->fn->len, eh);

    vs->fn->vars = tbl_create(args ? args->len : 0, eh);
    vs->fn->fns = tbl_create(0, eh);

    vparse_args(vs, args);
    vparse_nested(vs);
    vs->fn->stack = 25; // TODO make this reasonable

    return fn_realize(vs->fn, eh);
}


// Called by garbage collector to clean up
void fn_destroy(void *m) {
    fn_t *f = m;
    int i;

    for (i=0; i < f->vcount; i++) {
        var_dec(f->vars[i]);
    }

    for (i=0; i < f->fcount; i++) {
        fn_dec(f->fns[i]);
    }

    v_dealloc((void *)f->bcode, f->bcount);
    v_dealloc(f->vars, f->vcount*sizeof(var_t) + f->fcount*sizeof(fn_t *));
    vref_dealloc(m, sizeof(fn_t));
}


// Call a function. Each function takes a table
// of arguments, and returns a single variable.
var_t fn_call(fn_t *f, tbl_t *args, tbl_t *closure, veh_t *eh) {
    tbl_t *scope = tbl_create(1, eh);
    tbl_insert(scope, vcstr("args"), vtbl(args), eh);
    scope->tail = closure;

    return vexec(f, args, scope, eh);
}


// Returns a string representation of a function
var_t fn_repr(var_t v, veh_t *eh) {
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
