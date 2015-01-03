#include "var.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"

#include <string.h>
#include <stdlib.h>


// Returns true if both variables are the
// same type and equivalent.

// all nils are equal
static bool nil_equals(void *a, void *b) { return true; }
// compare raw bits by default
static bool bit_equals(void *a, void *b) { return (uint_t)a == (uint_t)b; }

bool var_equals(var_t a, var_t b) {
    static bool (* const var_equalss[8])(void *, void *) = {
        nil_equals, (void *)num_equals, (void *)mstr_equals, bit_equals,
        bit_equals, bit_equals, 0, 0
    };

    if (gettype(a) != gettype(b))
        return false;

    return var_equalss[gettype(a)](getptr(a), getptr(b));
}


// Returns a hash value of the given variable. 
// nils should never be hashed
// use raw bits by default
static hash_t bit_hash(void *v) { return (hash_t)v; }

hash_t var_hash(var_t v) {
    static hash_t (* const var_hashs[8])(void *) = {
        bit_hash, (void *)num_hash, (void *)mstr_hash, bit_hash,
        bit_hash, bit_hash, 0, 0
    };

    return var_hashs[gettype(v)](getptr(v));
}


// Performs iteration on variables
static fn_t *nil_iter(void *v, eh_t *eh) { err_undefined(eh); }

fn_t *var_iter(var_t v, eh_t *eh) {
    static fn_t *(* const var_iters[8])(void *, eh_t *) = {
        nil_iter, nil_iter, nil_iter, nil_iter,
        (void *)tbl_iter, (void *)tbl_iter, 0, 0
    };

    return var_iters[gettype(v)](getptr(v), eh);
}
    

// Returns a string representation of the variable
static str_t *nil_repr(void *v, eh_t *eh) { return str_cstr("nil", eh); }

str_t *var_repr(var_t v, eh_t *eh) {
    static str_t *(* const var_reprs[8])(void *, eh_t *) = {
        nil_repr, (void *)num_repr, (void *)str_repr, (void *)fn_repr,
        (void *)tbl_repr, (void *)tbl_repr, 0, 0
    };

    return var_reprs[gettype(v)](getptr(v), eh);
}


// Table related functions performed on variables
static var_t nil_lookup(void *t, var_t k, eh_t *eh)  { err_undefined(eh); }

var_t var_lookup(var_t v, var_t key, eh_t *eh) {
    static var_t (* const var_lookups[8])(void *, var_t, eh_t *) = {
        nil_lookup, nil_lookup, nil_lookup, nil_lookup,
        (void *)tbl_lookup, (void *)tbl_lookup, 0, 0
    };

    return var_lookups[gettype(v)](getptr(v), key, eh);
}

static var_t nil_lookdn(void *t, var_t k, hash_t i, eh_t *eh) { err_undefined(eh); }

var_t var_lookdn(var_t v, var_t key, hash_t i, eh_t *eh) {
    static var_t (* const var_lookdns[8])(void *, var_t, hash_t, eh_t *) = {
        nil_lookdn, nil_lookdn, nil_lookdn, nil_lookdn,
        (void *)tbl_lookdn, (void *)tbl_lookdn, 0, 0
    };

    return var_lookdns[gettype(v)](getptr(v), key, i, eh);
}


static void nil_insert(void *t, var_t k, var_t v, eh_t *eh) { err_undefined(eh); }
static void ro_insert(void *t, var_t k, var_t v, eh_t *eh) { err_readonly(eh); }

void var_insert(var_t v, var_t key, var_t val, eh_t *eh) {
    static void (* const var_inserts[8])(void *, var_t, var_t, eh_t *) = {
        nil_insert, nil_insert, nil_insert, nil_insert,
        (void *)tbl_insert, ro_insert, 0, 0
    };

    var_inserts[gettype(v)](getptr(v), key, val, eh);
}


static void nil_assign(void *t, var_t k, var_t v, eh_t *eh) { err_undefined(eh); }
static void ro_assign(void *t, var_t k, var_t v, eh_t *eh) { err_readonly(eh); }

void var_assign(var_t v, var_t key, var_t val, eh_t *eh) {
    static void (* const var_assigns[8])(void *, var_t, var_t, eh_t *) = {
        nil_assign, nil_assign, nil_assign, nil_assign,
        (void *)tbl_assign, ro_assign, 0, 0
    };

    var_assigns[gettype(v)](getptr(v), key, val, eh);
}


static void nil_append(void *t, var_t v, eh_t *eh) { err_undefined(eh); }
static void ro_append(void *t, var_t v, eh_t *eh) { err_readonly(eh); }

void var_append(var_t v, var_t val, eh_t *eh) {
    static void (* const var_appends[8])(void *, var_t, eh_t *) = {
        nil_append, nil_append, nil_append, nil_append,
        (void *)tbl_append, ro_append, 0, 0
    };

    var_appends[gettype(v)](getptr(v), val, eh);
}


// Function calls performed on variables
static var_t nil_call(void *f, tbl_t *a, eh_t *eh)  { err_undefined(eh); }

var_t var_call(var_t v, tbl_t *args, eh_t *eh) {
    static var_t (* const var_calls[8])(void *, tbl_t *, eh_t *) = {
        nil_call, nil_call, nil_call, (void *)fn_call,
        nil_call, nil_call, nil_call, nil_call
    };

    return var_calls[gettype(v)](getptr(v), args, eh);
}

