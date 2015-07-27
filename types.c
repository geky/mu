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
mu_t nil_repr(mu_t m) { return mcstr("nil"); }

mu_t (*const mu_repr_table[8])(mu_t) = {
    nil_repr, num_repr, 
    tbl_repr, tbl_repr,
    str_repr, fn_repr,
    fn_repr,  fn_repr,
};

// Performs iteration on variables
mu_t no_iter(mu_t m) { mu_err_undefined(); }
mu_t fn_iter(mu_t m) { return m; }

mu_t (*const mu_iter_table[8])(mu_t) = {
    no_iter,  no_iter, 
    tbl_iter, tbl_iter,
    str_iter, fn_iter, 
    fn_iter,  fn_iter,
};

// Table related functions performed on variables
mu_t no_lookup(mu_t m, mu_t k) { mu_err_undefined(); }

mu_t (*const mu_lookup_table[8])(mu_t, mu_t) = {
    no_lookup,  no_lookup, 
    tbl_lookup, tbl_lookup,
    no_lookup,  no_lookup,
    no_lookup,  no_lookup,
};

void no_insert(mu_t m, mu_t k, mu_t v) { mu_err_undefined(); }
void ro_insert(mu_t m, mu_t k, mu_t v) { mu_err_readonly(); }

void (*const mu_insert_table[8])(mu_t, mu_t, mu_t) = {
    no_insert,  no_insert, 
    tbl_insert, ro_insert,
    no_insert,  no_insert,
    no_insert,  no_insert,
};

void no_assign(mu_t m, mu_t k, mu_t v) { mu_err_undefined(); }
void ro_assign(mu_t m, mu_t k, mu_t v) { mu_err_readonly(); }

void (*const mu_assign_table[8])(mu_t, mu_t, mu_t) = {
    no_assign,  no_assign,
    tbl_assign, ro_assign,
    no_assign,  no_assign,
    no_assign,  no_assign,
};

// Function calls performed on variables
void no_fcall(mu_t m, frame_t fc, mu_t *f) { mu_err_undefined(); }

void tbl_fcall(mu_t m, frame_t fc, mu_t *frame) {
    mu_t call_hook = tbl_lookup(m, mcstr("call"));

    if (call_hook)
        mu_fcall(call_hook, fc, frame);
    else
        no_fcall(m, fc, frame);
}

void (*const mu_fcall_table[8])(mu_t, frame_t, mu_t *) = {
    no_fcall,  no_fcall, 
    tbl_fcall, tbl_fcall,
    no_fcall,  fn_fcall,
    bfn_fcall, sbfn_fcall,
};

// Function calls with varargs handling
mu_t mu_vcall(mu_t m, frame_t fc, va_list args) {
    mu_t frame[MU_FRAME];

    mu_toframe(fc >> 4, frame, args);
    mu_fcall(m, fc, frame);
    return mu_fromframe(0xf & fc, frame, args);
}

mu_t mu_call(mu_t m, frame_t fc, ...) {
    va_list args;
    va_start(args, fc);
    mu_t ret = mu_vcall(m, fc, args);
    va_end(args);
    return ret;
}
