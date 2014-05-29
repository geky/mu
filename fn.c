#include "fn.h"

#include "mem.h"
#include "vlex.h"

#include <assert.h>


// Functions for managing functions
// Each function is preceded with a reference count
// which is used as its handle in a var

// Called by garbage collector to clean up
void fn_destroy(void *m) {
    fn_t *f = m;
    int i;

    if (f->scope)
        vref_dec(f->scope);

    var_decref(f->code);

    for (i=0; i < f->alen; f++) {
        var_decref(f->args[i]);
    }
}


// Call a function. Each function takes a table
// of arguments, and returns a single variable.
var_t fn_call(var_t v, tbl_t *args) {
    switch (v.type) {
        case TYPE_FN: return fnp_call(v.fn, args);
        case TYPE_BFN: return v.bfn(args);
        default: assert(false); // TODO error on not a function
    }
}

var_t fnp_call(fn_t *f, tbl_t *args) {
    tbl_t *scope = tblp_create(f->alen);
    var_t param;
    int i;

    tblp_assign(scope, vcstr("args"), vtbl(args));
    tblp_assign(scope, vcstr("this"), tblp_lookup(args, vcstr("this")));

    for (i=0; i < f->alen; i++) {
        param = tblp_lookup(args, vnum(i));

        if (var_isnull(param))
            param = tblp_lookup(args, f->args[i]);

        tblp_assign(scope, f->args[i], param);
    }

    scope->tail = f->scope;


    struct vstate *vs = valloc(sizeof(struct vstate));
    vs->off = var_str(f->code);
    vs->end = vs->off + f->code.len;
    vs->ref = var_ref(f->code);
    vs->scope = scope;

    return vparse(vs);
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

    for (i=0; i < f->alen; i++) {
        size += f->args[i].len;

        if (i != f->alen-1)
            size += 2;
    }


    out = vref_alloc(size);
    s = out;

    *s++ = 'f';
    *s++ = 'n';
    *s++ = '(';

    for (i=0; i < f->alen; i++) {
        memcpy(s, var_str(f->args[i]), f->args[i].len);
        s += f->args[i].len;

        if (i != f->alen-1) {
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
