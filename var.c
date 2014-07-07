#include "var.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


// Returns true if both variables are the
// same type and equivalent.
// all nils are equal
static bool nil_equals(var_t a, var_t b) { return true; }
// compare raw bits by default
static bool bit_equals(var_t a, var_t b) { return a.bits == b.bits; }

static bool (* const var_equals_a[8])(var_t, var_t) = {
    nil_equals, num_equals, bit_equals, bit_equals,
    str_equals, bit_equals, bit_equals, bit_equals
};

bool var_equals(var_t a, var_t b) {
    if (a.type != b.type)
        return false;

    return var_equals_a[a.type](a, b);
}


// Returns a hash value of the given variable. 
// nils should never be hashed
// however hash could be called directly
static hash_t nil_hash(var_t v) { return 0; }
// use raw bits by default
static hash_t bit_hash(var_t v) { return v.meta ^ v.data; }

hash_t var_hash(var_t v) {
    static hash_t (* const var_hash_a[8])(var_t) = {
        nil_hash, num_hash, bit_hash, bit_hash,
        str_hash, bit_hash, bit_hash, bit_hash
    };

    return var_hash_a[v.type](v);
}


// Returns a string representation of the variable
static var_t nil_repr(var_t v) { return vcstr("nil"); }
static var_t bfn_repr(var_t v) { return vcstr("fn() <builtin>"); }
static var_t bad_repr(var_t v) { return vcstr("<bad var>"); }

var_t var_repr(var_t v) {
    static var_t (* const var_repr_a[8])(var_t) = {
        nil_repr, num_repr, bfn_repr, bfn_repr,
        str_repr, fn_repr,  tbl_repr, bad_repr
    };

    return var_repr_a[v.type](v);
}

// Prints variable to stdout for debugging
void var_print(var_t v) {
    var_t repr = var_repr(v);
    printf("%.*s", repr.len, var_str(repr));
}


// Table related functions performed on variables
static var_t nil_lookup(var_t t, var_t k) { assert(false); } // TODO error
static var_t vtbl_lookup(var_t t, var_t k) { return tbl_lookup(t.tbl, k); }

var_t var_lookup(var_t v, var_t key) {
    static var_t (* const var_lookup_a[8])(var_t, var_t) = {
        nil_lookup, nil_lookup, nil_lookup, nil_lookup,
        nil_lookup, nil_lookup, vtbl_lookup, nil_lookup
    };

    return var_lookup_a[v.type](v, key);
}


static void nil_assign(var_t t, var_t k, var_t v) { assert(false); } // TODO error
static void vtbl_assign(var_t t, var_t k, var_t v) { tbl_assign(t.tbl, k, v); }

void var_assign(var_t v, var_t key, var_t val) {
    static void (* const var_assign_a[8])(var_t, var_t, var_t) = {
        nil_assign, nil_assign, nil_assign, nil_assign,
        nil_assign, nil_assign, vtbl_assign, nil_assign
    };

    var_assign_a[v.type](v, key, val);
}


static void nil_insert(var_t t, var_t k, var_t v) { assert(false); } // TODO error
static void vtbl_insert(var_t t, var_t k, var_t v) { tbl_insert(t.tbl, k, v); }

void var_insert(var_t v, var_t key, var_t val) {
    static void (* const var_insert_a[8])(var_t, var_t, var_t) = {
        nil_insert, nil_insert, nil_insert, nil_insert,
        nil_insert, nil_insert, vtbl_insert, nil_insert
    };

    var_insert_a[v.type](v, key, val);
}


static void nil_add(var_t t, var_t v) { assert(false); } // TODO error
static void vtbl_add(var_t t, var_t v) { tbl_add(t.tbl, v); }

void var_add(var_t v, var_t val) {
    static void (* const var_add_a[8])(var_t, var_t) = {
        nil_add, nil_add, nil_add, nil_add,
        nil_add, nil_add, vtbl_add, nil_add
    };

    var_add_a[v.type](v, val);
}


// Function calls performed on variables
static var_t nil_call(var_t f, tbl_t *a) { assert(false); } // TODO error
static var_t vfn_call(var_t f, tbl_t *a) { return fn_call(var_fn(f), a, f.tbl); }
static var_t vbfn_call(var_t f, tbl_t *a) { return f.bfn(a); }
static var_t vsfn_call(var_t f, tbl_t *a) { f.type = 4; return f.sfn(a, f.stbl); }

var_t var_call(var_t v, tbl_t *args) {
    static var_t (* const var_call_a[8])(var_t, tbl_t *) = {
        nil_call, nil_call, vbfn_call, vsfn_call,
        nil_call, vfn_call, nil_call, nil_call
    };

    return var_call_a[v.type](v, args);
}


// Cleans up memory of a variable
// Only takes care of none table memory
static void str_destroy(void *m) {}

void (* const vdtor_a[8])(void *) = {
    0, 0, 0, tbl_destroy,
    str_destroy, fn_destroy, 0, 0
};
