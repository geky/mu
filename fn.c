#include "fn.h"

#include "mem.h"
#include "vparse.h"
#include "vm.h"

#include <assert.h>


// Functions for managing functions
// Each function is preceded with a reference count
// which is used as its handle in a var
var_t fn_create(var_t argv, var_t code, var_t scope) {
    fn_t *f;
    tbl_t *args;
    tbl_t *vars;
    int i = 0;

    assert(var_istbl(argv) && 
           var_isstr(code) &&
           (var_istbl(scope) || 
            var_isnull(scope))); // TODO errors

    args = tblp_readp(argv.tbl);
    vars = tbl_create(args->len + 1).tbl;
    f = vref_alloc(sizeof(fn_t) + args->len);


    tbl_assign(vars, code, vnum(i++));

    tbl_for(k, v, args, {
        tbl_assign(vars, v, vnum(i++));
    })
        

    f->acount = args->len;
    f->stack = 25; // TODO make this reasonable
    f->scope = scope.tbl;

    struct vstate *vs = valloc(sizeof(struct vstate));
    vs->off = var_str(code);
    vs->end = vs->off + code.len;
    vs->ref = var_ref(code);

    vs->bcode = 0;
    vs->vars = vars;
    vs->encode = vcount;

    int ins = vparse(vs);

    vs->off = var_str(code);
    vs->end = vs->off + code.len;
    vs->ref = var_ref(code);
    vs->bcode = valloc(ins);
    vs->encode = vencode;

    vparse(vs);

    f->bcode = vs->bcode;
    f->vcount = vars->len;
    f->vars = valloc(sizeof(var_t) * f->vcount);

    tbl_for(k, v, vars, {
        f->vars[(uint16_t)var_num(v)] = k;
        var_incref(k);
    });
    
    vref_dec(vars);
    vdealloc(vs);

    return vfn(f);
}



// Called by garbage collector to clean up
void fn_destroy(void *m) {
    fn_t *f = m;
    int i;

    if (f->scope)
        vref_dec(f->scope);

    vref_dec((ref_t *)f->bcode);

    for (i=0; i < f->vcount; i++) {
        var_decref(f->vars[i]);
    }
}


// Call a function. Each function takes a table
// of arguments, and returns a single variable.
var_t fn_call(fn_t *f, tbl_t *args) {
    tbl_t *scope = tbl_create(f->acount + 2).tbl;
    int i;

    tbl_assign(scope, vcstr("args"), vtbl(args));
    tbl_assign(scope, vcstr("this"), tbl_lookup(args, vcstr("this")));

    for (i=1; i <= f->acount; i++) {
        var_t param = tbl_lookup(args, vnum(i));

        if (var_isnull(param))
            param = tbl_lookup(args, f->vars[i]);

        tbl_assign(scope, f->vars[i], param);
    }

    scope->tail = f->scope;


    return vexec(f, vtbl(scope));
}


// Returns a string representation of a function
var_t bfn_repr(var_t v) {
    return vcstr("fn() <builtin>");
}

var_t fn_repr(var_t v) {
    fn_t *f = v.fn;
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
