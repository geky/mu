#include "types.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"
#include "err.h"


// Destructor table
extern void str_destroy(mu_t);
extern void tbl_destroy(mu_t);
extern void fn_destroy(mu_t);
extern void bfn_destroy(mu_t);
extern void sbfn_destroy(mu_t);

void (*const mu_destroy_table[6])(mu_t) = {
    tbl_destroy, tbl_destroy,
    str_destroy, fn_destroy,
    bfn_destroy, sbfn_destroy,
};


// Returns a string representation of the variable
static mu_t nil_repr(mu_t m) { return mcstr("nil"); }
mu_t mu_repr(mu_t m) {
    mu_t (*const mu_repr_table[8])(mu_t) = {
        nil_repr, num_repr,
        tbl_repr, tbl_repr,
        str_repr, fn_repr,
        fn_repr,  fn_repr,
    };

    return mu_repr_table[mu_type(m)](m);
}

// Performs iteration on variables
static mu_t no_iter(mu_t m) { mu_err_undefined(); }
static mu_t fn_iter(mu_t m) { return m; }
mu_t mu_iter(mu_t m) {
    mu_t (*const mu_iter_table[8])(mu_t) = {
        no_iter,  no_iter,
        tbl_iter, tbl_iter,
        str_iter, fn_iter,
        fn_iter,  fn_iter,
    };

    return mu_iter_table[mu_type(m)](m);
}

// Table related functions performed on variables
static mu_t no_lookup(mu_t m, mu_t k) { mu_err_undefined(); }
mu_t mu_lookup(mu_t m, mu_t k) {
    mu_t (*const mu_lookup_table[8])(mu_t, mu_t) = {
        no_lookup,  no_lookup,
        tbl_lookup, tbl_lookup,
        no_lookup,  no_lookup,
        no_lookup,  no_lookup,
    };

    return mu_lookup_table[mu_type(m)](m, k);
}

static void no_insert(mu_t m, mu_t k, mu_t v) { mu_err_undefined(); }
static void ro_insert(mu_t m, mu_t k, mu_t v) { mu_err_readonly(); }
void mu_insert(mu_t m, mu_t k, mu_t v) {
    void (*const mu_insert_table[8])(mu_t, mu_t, mu_t) = {
        no_insert,  no_insert,
        tbl_insert, ro_insert,
        no_insert,  no_insert,
        no_insert,  no_insert,
    };

    return mu_insert_table[mu_type(m)](m, k, v);
}

static void no_assign(mu_t m, mu_t k, mu_t v) { mu_err_undefined(); }
static void ro_assign(mu_t m, mu_t k, mu_t v) { mu_err_readonly(); }
void mu_assign(mu_t m, mu_t k, mu_t v) {
    void (*const mu_assign_table[8])(mu_t, mu_t, mu_t) = {
        no_assign,  no_assign,
        tbl_assign, ro_assign,
        no_assign,  no_assign,
        no_assign,  no_assign,
    };

    return mu_assign_table[mu_type(m)](m, k, v);
}

// Function calls performed on variables
static frame_t no_tcall(mu_t m, frame_t fc, mu_t *f) { mu_err_undefined(); }
static frame_t tbl_tcall(mu_t m, frame_t fc, mu_t *frame) {
    mu_t call_hook = tbl_lookup(m, mcstr("call"));

    if (call_hook)
        return mu_tcall(call_hook, fc, frame);
    else
        return no_tcall(m, fc, frame);
}
frame_t mu_tcall(mu_t m, frame_t fc, mu_t *frame) {
    frame_t (*const mu_tcall_table[8])(mu_t, frame_t, mu_t *) = {
        no_tcall,  no_tcall,
        tbl_tcall, tbl_tcall,
        no_tcall,  fn_tcall,
        bfn_tcall, sbfn_tcall,
    };

    return mu_tcall_table[mu_type(m)](m, fc, frame);
}

void mu_fcall(mu_t m, frame_t fc, mu_t *frame) {
    mu_fto(0xf & fc, mu_tcall(m, fc >> 4, frame), frame);
}

mu_t mu_vcall(mu_t m, frame_t fc, va_list args) {
    mu_t frame[MU_FRAME];

    for (uint_t i = 0; i < mu_fcount(0xf & fc); i++)
        frame[i] = va_arg(args, mu_t);

    mu_fcall(m, fc, frame);

    for (uint_t i = 1; i < mu_fcount(fc >> 4); i++)
        *va_arg(args, mu_t *) = frame[i];

    return (fc >> 4) ? *frame : mnil;
}

mu_t mu_call(mu_t m, frame_t fc, ...) {
    va_list args;
    va_start(args, fc);
    mu_t ret = mu_vcall(m, fc, args);
    va_end(args);
    return ret;
}
