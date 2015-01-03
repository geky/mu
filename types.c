#include "types.h"

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

bool mu_equals(mu_t a, mu_t b) {
    static bool (* const mu_equalss[8])(void *, void *) = {
        nil_equals, (void *)num_equals, (void *)mstr_equals, bit_equals,
        bit_equals, bit_equals, 0, 0
    };

    if (gettype(a) != gettype(b))
        return false;

    return mu_equalss[gettype(a)](getptr(a), getptr(b));
}


// Returns a hash value of the given variable. 
// nils should never be hashed
// use raw bits by default
static hash_t bit_hash(void *v) { return (hash_t)v; }

hash_t mu_hash(mu_t v) {
    static hash_t (* const mu_hashs[8])(void *) = {
        bit_hash, (void *)num_hash, (void *)mstr_hash, bit_hash,
        bit_hash, bit_hash, 0, 0
    };

    return mu_hashs[gettype(v)](getptr(v));
}


// Performs iteration on variables
static fn_t *nil_iter(void *v, eh_t *eh) { err_undefined(eh); }

fn_t *mu_iter(mu_t v, eh_t *eh) {
    static fn_t *(* const mu_iters[8])(void *, eh_t *) = {
        nil_iter, nil_iter, nil_iter, nil_iter,
        (void *)tbl_iter, (void *)tbl_iter, 0, 0
    };

    return mu_iters[gettype(v)](getptr(v), eh);
}
    

// Returns a string representation of the variable
static str_t *nil_repr(void *v, eh_t *eh) { return str_cstr("nil", eh); }

str_t *mu_repr(mu_t v, eh_t *eh) {
    static str_t *(* const mu_reprs[8])(void *, eh_t *) = {
        nil_repr, (void *)num_repr, (void *)str_repr, (void *)fn_repr,
        (void *)tbl_repr, (void *)tbl_repr, 0, 0
    };

    return mu_reprs[gettype(v)](getptr(v), eh);
}


// Table related functions performed on variables
static mu_t nil_lookup(void *t, mu_t k, eh_t *eh)  { err_undefined(eh); }

mu_t mu_lookup(mu_t v, mu_t key, eh_t *eh) {
    static mu_t (* const mu_lookups[8])(void *, mu_t, eh_t *) = {
        nil_lookup, nil_lookup, nil_lookup, nil_lookup,
        (void *)tbl_lookup, (void *)tbl_lookup, 0, 0
    };

    return mu_lookups[gettype(v)](getptr(v), key, eh);
}

static mu_t nil_lookdn(void *t, mu_t k, hash_t i, eh_t *eh) { err_undefined(eh); }

mu_t mu_lookdn(mu_t v, mu_t key, hash_t i, eh_t *eh) {
    static mu_t (* const mu_lookdns[8])(void *, mu_t, hash_t, eh_t *) = {
        nil_lookdn, nil_lookdn, nil_lookdn, nil_lookdn,
        (void *)tbl_lookdn, (void *)tbl_lookdn, 0, 0
    };

    return mu_lookdns[gettype(v)](getptr(v), key, i, eh);
}


static void nil_insert(void *t, mu_t k, mu_t v, eh_t *eh) { err_undefined(eh); }
static void ro_insert(void *t, mu_t k, mu_t v, eh_t *eh) { err_readonly(eh); }

void mu_insert(mu_t v, mu_t key, mu_t val, eh_t *eh) {
    static void (* const mu_inserts[8])(void *, mu_t, mu_t, eh_t *) = {
        nil_insert, nil_insert, nil_insert, nil_insert,
        (void *)tbl_insert, ro_insert, 0, 0
    };

    mu_inserts[gettype(v)](getptr(v), key, val, eh);
}


static void nil_assign(void *t, mu_t k, mu_t v, eh_t *eh) { err_undefined(eh); }
static void ro_assign(void *t, mu_t k, mu_t v, eh_t *eh) { err_readonly(eh); }

void mu_assign(mu_t v, mu_t key, mu_t val, eh_t *eh) {
    static void (* const mu_assigns[8])(void *, mu_t, mu_t, eh_t *) = {
        nil_assign, nil_assign, nil_assign, nil_assign,
        (void *)tbl_assign, ro_assign, 0, 0
    };

    mu_assigns[gettype(v)](getptr(v), key, val, eh);
}


static void nil_append(void *t, mu_t v, eh_t *eh) { err_undefined(eh); }
static void ro_append(void *t, mu_t v, eh_t *eh) { err_readonly(eh); }

void mu_append(mu_t v, mu_t val, eh_t *eh) {
    static void (* const mu_appends[8])(void *, mu_t, eh_t *) = {
        nil_append, nil_append, nil_append, nil_append,
        (void *)tbl_append, ro_append, 0, 0
    };

    mu_appends[gettype(v)](getptr(v), val, eh);
}


// Function calls performed on variables
static mu_t nil_call(void *f, tbl_t *a, eh_t *eh)  { err_undefined(eh); }

mu_t mu_call(mu_t v, tbl_t *args, eh_t *eh) {
    static mu_t (* const mu_calls[8])(void *, tbl_t *, eh_t *) = {
        nil_call, nil_call, nil_call, (void *)fn_call,
        nil_call, nil_call, nil_call, nil_call
    };

    return mu_calls[gettype(v)](getptr(v), args, eh);
}

