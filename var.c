#include "var.h"

#include "num.h"
#include "str.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

// valloc and vdealloc just map to malloc and free
#define valloc malloc
#define vdealloc free


// Memory management and garbage collection
// Each block of memory prefixed with ref_t reference
// count. Deallocated immediately when refs hit zero.
// It is up to the user to avoid cyclic dependencies

ref_t *var_alloc(size_t size) {
    ref_t *m = (ref_t *)valloc(sizeof(ref_t) + size);

    assert(m != 0);                     // out of memory
    assert(sizeof m == 4);              // address width
    assert((((uint32_t)m) & 0x7) == 0); // alignment

    // start with count of 1
    *m = 1;

    return m;
}

void var_incref(var_t var) {
    if (var_type(var) & 0x4)
        (*var_ref(var))++;
}

void var_decref(var_t var) {
    if (var_type(var) & 0x4) {
        ref_t *ref = var_ref(var);
        assert(*ref > 0);

        if (--(*ref) == 0) {
            // TODO add special cases for vars with more cleanup
            
            vdealloc(ref);
        }
    }
}



// Returns true if both variables are the
// same type and equivalent.

// all nulls are equal
static bool null_equals(var_t a, var_t b) { return true; }
// compare raw data field by default
static bool data_equals(var_t a, var_t b) { return a.data == b.data; }

static bool (* const var_equals_a[8])(var_t, var_t) = {
    null_equals, data_equals, data_equals, num_equals,
    str_equals, data_equals, data_equals, data_equals
};

bool var_equals(var_t a, var_t b) {
    if (var_type(a) != var_type(b))
        return false;

    return var_equals_a[var_type(a)](a, b);
}


// Returns a hash value of the given variable. 

// nulls should never be hashed
// however hash could be called directly
static hash_t null_hash(var_t v) { return 0; }
// use raw data field by default
static hash_t data_hash(var_t v) { return v.data; }

static hash_t (* const var_hash_a[8])(var_t) = {
    null_hash, data_hash, data_hash, num_hash,
    str_hash, data_hash, data_hash, data_hash
};

hash_t var_hash(var_t v) {
    return var_hash_a[var_type(v)](v);
}


// Returns a string representation of the variable

static var_t null_repr(var_t v) { return vstr("null"); }

static var_t (* const var_repr_a[8])(var_t) = {
    null_repr, 0, 0, num_repr,
    str_repr, 0, 0, 0
};

var_t var_repr(var_t v) {
    return var_repr_a[var_type(v)](v);
}

// Prints variable to stdout for debugging
void var_print(var_t v) {
    var_t repr = var_repr(v);
    printf("%.*s", repr.len, var_str(repr));
}

