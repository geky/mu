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
    int i = 0;

    assert(var_istbl(argv) && 
           var_isstr(code) &&
           (var_istbl(scope) || var_isnull(scope))); // TODO errors

    args = tblp_readp(argv.tbl);
    f = vref_alloc(sizeof(fn_t) + args->len);

    tbl_for(k, v, args, {
        f->args[i++] = v;
        var_incref(v);
    })


    f->acount = args->len;
    f->stack = 25; // TODO make this reasonable
    f->scope = scope.tbl;
    f->code = code;

    struct vstate *vs = valloc(sizeof(struct vstate));
    vs->off = var_str(code);
    vs->end = vs->off + code.len;
    vs->ref = var_ref(code);

    vs->bcode = 0;
    vs->encode = vcount;

    int ins = vparse(vs);

    vs->off = var_str(code);
    vs->end = vs->off + code.len;
    vs->ref = var_ref(code);
    vs->bcode = valloc(ins);
    vs->encode = vencode;

    vparse(vs);

    f->bcode = vs->bcode;
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

    for (i=-1; i < f->acount; i++) {
        var_decref(f->args[i]);
    }
}


// Call a function. Each function takes a table
// of arguments, and returns a single variable.
var_t fn_call(fn_t *f, tbl_t *args) {
    tbl_t *scope = tbl_create(f->acount + 2).tbl;
    int i;

    tbl_assign(scope, vcstr("args"), vtbl(args));
    tbl_assign(scope, vcstr("this"), tbl_lookup(args, vcstr("this")));

    for (i=0; i < f->acount; i++) {
        var_t param = tbl_lookup(args, vnum(i));

        if (var_isnull(param))
            param = tbl_lookup(args, f->args[i]);

        tbl_assign(scope, f->args[i], param);
    }

    scope->tail = f->scope;

    
    return vexec(f->bcode, f->stack, scope);
}


// Returns a string representation of a function
var_t bfn_repr(var_t v) {
    return vcstr("fn() <builtin>");
}

var_t fn_repr(var_t v) {
    fn_t *f = v.fn;
    unsigned int size = 7 + f->code.len;
    int i;

    uint8_t *out, *s;

    for (i=0; i < f->acount; i++) {
        size += f->args[i].len;

        if (i != f->acount-1)
            size += 2;
    }


    out = vref_alloc(size);
    s = out;

    *s++ = 'f';
    *s++ = 'n';
    *s++ = '(';

    for (i=0; i < f->acount; i++) {
        memcpy(s, var_str(f->args[i]), f->args[i].len);
        s += f->args[i].len;

        if (i++ != f->acount-1) {
            *s++ = ',';
            *s++ = ' ';
        }
    }

    *s++ = ')';
    *s++ = ' ';
    *s++ = '{';

    memcpy(s, var_str(f->code), f->code.len);
    s += f->code.len;

    *s++ = '}';


    return vstr(out, 0, size);
}
