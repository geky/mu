#include "fn.h"

#include "mem.h"
#include "vlex.h"
#include "vparse.h"
#include "vm.h"

#include <assert.h>


// Functions for managing functions
// Each function is preceded with a reference count
// which is used as its handle in a var
fn_t *fn_create(tbl_t *args, var_t code, tbl_t *ops, tbl_t *keys) {
    fn_t *f;
    tbl_t *vars;
    int i = 0;

    args = tbl_readp(args);
    f = vref_alloc(sizeof(fn_t));
    vars = tbl_create(args->len + 1);

    tbl_insert(vars, code, vnum(i++));

    tbl_for(k, v, args, {
        tbl_insert(vars, v, vnum(i++));
    })

        
    struct vstate *vs = valloc(sizeof(struct vstate));
    vs->ref = var_ref(code);
    vs->pos = var_str(code);
    vs->end = vs->pos + code.len;
    vs->bcode = 0;
    vs->vars = vars;
    vs->encode = vcount;

    vs->ops = ops ? tbl_readp(ops) : vops();
    vs->keys = keys ? tbl_readp(keys) : vkeys();


    int ins = vparse(vs);

    vs->pos = var_str(code);
    vs->bcode = valloc(ins);
    vs->encode = vencode;

    vparse(vs);

    f->bcode = vs->bcode;
    f->acount = args->len;
    f->vcount = vars->len;
    f->stack = 25; // TODO make this reasonable

    f->vars = valloc(sizeof(var_t) * f->vcount);

    tbl_for(k, v, vars, {
        f->vars[(uint16_t)var_num(v)] = k;
    });
    
    tbl_dec(vars);
    vdealloc(vs);

    return f;
}



// Called by garbage collector to clean up
void fn_destroy(void *m) {
    fn_t *f = m;
    int i;

    for (i=0; i < f->vcount; i++) {
        var_dec(f->vars[i]);
    }

    vdealloc((void *)f->bcode);
    vdealloc(f->vars);
}


// Call a function. Each function takes a table
// of arguments, and returns a single variable.
var_t fn_call(fn_t *f, tbl_t *args, tbl_t *closure) {
    tbl_t *scope = tbl_create(f->acount + 1);
    int i;

    tbl_insert(scope, vcstr("args"), vtbl(args));

    for (i=0; i < f->acount; i++) {
        var_t param = tbl_lookup(args, vnum(i));

        if (var_isnil(param))
            param = tbl_lookup(args, f->vars[i-1]);

        tbl_insert(scope, f->vars[i-1], param);
    }

    scope->tail = closure;

    return vexec(f, scope);
}


// Returns a string representation of a function
var_t fn_repr(var_t v) {
    fn_t *f = var_fn(v);
    unsigned int size = 7 + f->vars[0].len;
    int i;

    uint8_t *out, *s;

    for (i=1; i <= f->acount; i++) {
        size += f->vars[i].len;

        if (i != f->acount-1)
            size += 2;
    }


    out = vref_alloc(size);
    s = out;

    *s++ = 'f';
    *s++ = 'n';
    *s++ = '(';

    for (i=1; i <= f->acount; i++) {
        memcpy(s, var_str(f->vars[i]), f->vars[i].len);
        s += f->vars[i].len;

        if (i++ != f->acount-1) {
            *s++ = ',';
            *s++ = ' ';
        }
    }

    *s++ = ')';
    *s++ = ' ';
    *s++ = '{';

    memcpy(s, var_str(f->vars[0]), f->vars[0].len);
    s += f->vars[0].len;

    *s++ = '}';


    return vstr(out, 0, size);
}
