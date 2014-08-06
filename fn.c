#include "fn.h"

#include "mem.h"
#include "str.h"
#include "vlex.h"
#include "vparse.h"
#include "vm.h"

#include <assert.h>


// Functions for managing functions
// Each function is preceded with a reference count
// which is used as its handle in a var
fn_t *fn_create(tbl_t *args, var_t code, tbl_t *ops, tbl_t *keys) {
    args = tbl_readp(args);

    fn_t *f = vref_alloc(sizeof(fn_t));
    tbl_t *vars = tbl_create(args->len + 1);
    int i = 0;

    tbl_insert(vars, code, vnum(i++));

    tbl_for(k, v, args, {
        tbl_insert(vars, v, vnum(i++));
    })

    vstate_t *vs = valloc(sizeof(vstate_t));
    vs->ref = var_ref(code);
    vs->pos = var_str(code);
    vs->end = vs->pos + code.len;
    vs->bcode = 0;
    vs->vars = vars;
    vs->encode = vcount;

    vs->ops = ops ? tbl_readp(ops) : vops();
    vs->keys = keys ? tbl_readp(keys) : vkeys();


    f->bcount = vparse(vs);

    vs->pos = var_str(code);
    vs->bcode = valloc(f->bcount);
    vs->encode = vencode;

    vparse(vs);

    f->bcode = vs->bcode;
    f->acount = args->len;
    f->vcount = vars->len;
    f->stack = 25; // TODO make this reasonable

    f->vars = valloc(f->vcount * sizeof(var_t));
    tbl_for(k, v, vars, {
        f->vars[(len_t)var_num(v)] = k;
    });
    
    tbl_dec(vars);
    vdealloc(vs, sizeof(vstate_t));

    return f;
}



// Called by garbage collector to clean up
void fn_destroy(void *m) {
    fn_t *f = m;
    int i;

    f->vars -= 1;
    for (i=0; i < f->vcount; i++) {
        var_dec(f->vars[i]);
    }

    vdealloc((void *)f->bcode, f->bcount);
    vdealloc(f->vars, f->vcount * sizeof(var_t));
    vref_dealloc(m, sizeof(fn_t));
}


// Call a function. Each function takes a table
// of arguments, and returns a single variable.
var_t fn_call(fn_t *f, tbl_t *args, tbl_t *closure) {
    tbl_t *scope = tbl_create(f->acount + 1);
    int i;

    tbl_insert(scope, vcstr("args"), vtbl(args));

    for (i=0; i < f->acount; i++) {
        var_t param = tbl_lookup(args, vnum(0));

        if (var_isnil(param))
            param = tbl_lookup(args, f->vars[i+1]);

        tbl_insert(scope, f->vars[i+1], param);
    }

    scope->tail = closure;

    return vexec(f, scope);
}


// Returns a string representation of a function
var_t fn_repr(var_t v) {
    fn_t *f = var_fn(v);
    int size = 7 + f->vars[0].len;
    int i;

    for (i=0; i < f->acount; i++) {
        size += f->vars[i+1].len;
        size += (i == f->acount-1) ? 0 : 2;
    }

    assert(size <= VMAXLEN); // TODO error

    str_t *out = str_create(size);
    str_t *res = out;

    *res++ = 'f';
    *res++ = 'n';
    *res++ = '(';

    for (i=0; i < f->acount; i++) {
        memcpy(res, var_str(f->vars[i+1]), f->vars[i+1].len);
        res += f->vars[i+1].len;

        if (i != f->acount-1) {
            *res++ = ',';
            *res++ = ' ';
        }
    }

    *res++ = ')';
    *res++ = ' ';
    *res++ = '{';

    memcpy(res, var_str(f->vars[0]), f->vars[0].len);
    res += f->vars[0].len;

    *res++ = '}';

    return vstr(out, 0, size);
}
