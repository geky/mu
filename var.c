#include "var.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>



// Returns true if both variables are the
// same type and equivalent.

// all nulls are equal
static bool null_equals(var_t a, var_t b) { return true; }
// compare raw data field by default
static bool data_equals(var_t a, var_t b) { return a.data == b.data; }

static bool (* const var_equals_a[8])(var_t, var_t) = {
    null_equals, data_equals, 0, num_equals,
    str_equals, data_equals, data_equals, data_equals
};

bool var_equals(var_t a, var_t b) {
    if (a.type != b.type)
        return false;

    return var_equals_a[a.type](a, b);
}


// Returns a hash value of the given variable. 

// nulls should never be hashed
// however hash could be called directly
static hash_t null_hash(var_t v) { return 0; }
// use raw data field by default
static hash_t data_hash(var_t v) { return v.data; }

static hash_t (* const var_hash_a[8])(var_t) = {
    null_hash, data_hash, 0, num_hash,
    str_hash, data_hash, data_hash, data_hash
};

hash_t var_hash(var_t v) {
    return var_hash_a[v.type](v);
}


// Returns a string representation of the variable

static var_t null_repr(var_t v) { return vcstr("null"); }
static var_t def_repr(var_t v) { return vcstr("bad type"); }

static var_t (* const var_repr_a[8])(var_t) = {
    null_repr, bfn_repr, def_repr, num_repr,
    str_repr, fn_repr, tbl_repr, def_repr
};

var_t var_repr(var_t v) {
    return var_repr_a[v.type](v);
}

// Prints variable to stdout for debugging
void var_print(var_t v) {
    var_t repr = var_repr(v);
    printf("%.*s", repr.len, var_str(repr));
}


// Cleans up memory of a variable

static void null_destroy(void *m) {}

static void (* const vdestroy_a[4])(void *) = {
    null_destroy, fn_destroy, tbl_destroy, tbl_destroy
};

void vdestroy(void *m) {
    vdestroy_a[0x3 & (uint32_t)m](m);
}
