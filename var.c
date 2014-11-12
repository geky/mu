#include "var.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


// Returns true if both variables are the
// same type and equivalent.
// all nils are equal
static bool nil_equals(var_t a, var_t b) { return true; }
// compare raw bits by default
static bool bit_equals(var_t a, var_t b) { return a.bits == b.bits; }

bool var_equals(var_t a, var_t b) {
    static bool (* const var_equalss[8])(var_t, var_t) = {
        nil_equals, num_equals, bit_equals, bit_equals,
        bit_equals, bit_equals, str_equals, bit_equals
    };

    if (type(a) != type(b))
        return false;

    return var_equalss[type(a)](a, b);
}


// Returns a hash value of the given variable. 
// nils should never be hashed
// use raw bits by default
static hash_t bit_hash(var_t v) { return v.meta ^ v.data; }

hash_t var_hash(var_t v) {
    static hash_t (* const var_hashs[8])(var_t) = {
        bit_hash, num_hash, bit_hash, bit_hash,
        bit_hash, bit_hash, str_hash, bit_hash
    };

    return var_hashs[type(v)](v);
}


// Performs iteration on variables
static var_t nil_iter(var_t v, eh_t *eh) { err_undefined(eh); }

var_t var_iter(var_t v, eh_t *eh) {
    static var_t (* const var_iters[8])(var_t, eh_t *) = {
        nil_iter, nil_iter, nil_iter, nil_iter,
        tbl_iter, nil_iter, nil_iter, nil_iter
    };

    return var_iters[type(v)](v, eh);
}
    

// Returns a string representation of the variable
static var_t bad_repr(var_t v, eh_t *eh) { err_undefined(eh); }
static var_t nil_repr(var_t v, eh_t *eh) { return vcstr("nil"); }

var_t var_repr(var_t v, eh_t *eh) {
    static var_t (* const var_reprs[8])(var_t, eh_t *) = {
        nil_repr, num_repr, fn_repr, fn_repr, 
        tbl_repr, bad_repr, str_repr, fn_repr
    };

    return var_reprs[type(v)](v, eh);
}

// Prints variable to stdout for debugging
void var_print(var_t v, eh_t *eh) {
    var_t repr = var_repr(v, eh);
    printf("%.*s", getlen(repr), getstr(repr));
}


// Table related functions performed on variables
static var_t nil_lookup(var_t t, var_t k, eh_t *eh)  { err_undefined(eh); }
static var_t vtbl_lookup(var_t t, var_t k, eh_t *eh) { return tbl_lookup(gettbl(t), k); }

var_t var_lookup(var_t v, var_t key, eh_t *eh) {
    static var_t (* const var_lookups[8])(var_t, var_t, eh_t *) = {
        nil_lookup, nil_lookup, nil_lookup, nil_lookup,
        vtbl_lookup, nil_lookup, nil_lookup, nil_lookup
    };

    return var_lookups[type(v)](v, key, eh);
}

static var_t nil_lookdn(var_t t, var_t k, len_t i, eh_t *eh)  { err_undefined(eh); }
static var_t vtbl_lookdn(var_t t, var_t k, len_t i, eh_t *eh) { return tbl_lookdn(gettbl(t), k, i); }

var_t var_lookdn(var_t v, var_t key, len_t i, eh_t *eh) {
    static var_t (* const var_lookdns[8])(var_t, var_t, len_t, eh_t *) = {
        nil_lookdn, nil_lookdn, nil_lookdn, nil_lookdn,
        vtbl_lookdn, nil_lookdn, nil_lookdn, nil_lookdn
    };

    return var_lookdns[type(v)](v, key, i, eh);
}


static void nil_insert(var_t t, var_t k, var_t v, eh_t *eh)  { err_undefined(eh); }
static void vtbl_insert(var_t t, var_t k, var_t v, eh_t *eh) { tbl_insert(gettbl(t), k, v, eh); }

void var_insert(var_t v, var_t key, var_t val, eh_t *eh) {
    static void (* const var_inserts[8])(var_t, var_t, var_t, eh_t *) = {
        nil_insert, nil_insert, nil_insert, nil_insert,
        vtbl_insert, nil_insert, nil_insert, nil_insert
    };

    var_inserts[type(v)](v, key, val, eh);
}


static void nil_assign(var_t t, var_t k, var_t v, eh_t *eh)  { err_undefined(eh); }
static void vtbl_assign(var_t t, var_t k, var_t v, eh_t *eh) { tbl_assign(gettbl(t), k, v, eh); }

void var_assign(var_t v, var_t key, var_t val, eh_t *eh) {
    static void (* const var_assigns[8])(var_t, var_t, var_t, eh_t *) = {
        nil_assign, nil_assign, nil_assign, nil_assign,
        vtbl_assign, nil_assign, nil_assign, nil_assign
    };

    var_assigns[type(v)](v, key, val, eh);
}


static void nil_append(var_t t, var_t v, eh_t *eh)  { err_undefined(eh); }
static void vtbl_append(var_t t, var_t v, eh_t *eh) { tbl_append(gettbl(t), v, eh); }

void var_append(var_t v, var_t val, eh_t *eh) {
    static void (* const var_appends[8])(var_t, var_t, eh_t *) = {
        nil_append, nil_append, nil_append, nil_append,
        vtbl_append, nil_append, nil_append, nil_append
    };

    var_appends[type(v)](v, val, eh);
}


// Function calls performed on variables
static var_t nil_call(var_t f, tbl_t *a, eh_t *eh)  { err_undefined(eh); }
static var_t vfn_call(var_t f, tbl_t *a, eh_t *eh)  { return fn_call(getfn(f), a, gettbl(f), eh); }
static var_t vbfn_call(var_t f, tbl_t *a, eh_t *eh) { return getbfn(f)(a, eh); }
static var_t vsfn_call(var_t f, tbl_t *a, eh_t *eh) { return getsfn(f)(a, gettbl(f), eh); }

var_t var_call(var_t v, tbl_t *args, eh_t *eh) {
    static var_t (* const var_calls[8])(var_t, tbl_t *, eh_t *) = {
        nil_call, nil_call, vbfn_call, vsfn_call, 
        nil_call, nil_call, nil_call, vfn_call
    };

    return var_calls[type(v)](v, args, eh);
}

var_t var_pcall(var_t v, tbl_t *args) {
    eh_t eh;
    tbl_t *err = mu_eh(&eh);

    if (mu_likely(err == 0)) {
        return var_call(v, args, &eh);
    } else {
        return verr(err);
    }
}
