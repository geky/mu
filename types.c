#include "types.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"
#include "err.h"


// Destructor table
void (*const mu_destroy_table[4])(mu_t) = {
    str_destroy, fn_destroy, tbl_destroy, tbl_destroy
};
    
// Returns a string representation of the variable
mu_t nil_repr(mu_t m) { return mcstr("nil"); }

mu_t (*const mu_repr_table[6])(mu_t) = {
    nil_repr, num_repr, str_repr,
    fn_repr, tbl_repr, tbl_repr,
};

// Performs iteration on variables
mu_t no_iter(mu_t m) { mu_err_undefined(); }
mu_t fn_iter(mu_t m) { return m; }

mu_t (*const mu_iter_table[6])(mu_t) = {
    no_iter, no_iter, str_iter,
    fn_iter, tbl_iter, tbl_iter,
};

// Table related functions performed on variables
mu_t no_lookup(mu_t m, mu_t k) { mu_err_undefined(); }

mu_t (*const mu_lookup_table[6])(mu_t, mu_t) = {
    no_lookup, no_lookup, no_lookup,
    no_lookup, tbl_lookup, tbl_lookup,
};

void no_insert(mu_t m, mu_t k, mu_t v) { mu_err_undefined(); }
void ro_insert(mu_t m, mu_t k, mu_t v) { mu_err_readonly(); }

void (*const mu_insert_table[6])(mu_t, mu_t, mu_t) = {
    no_insert, no_insert, no_insert,
    no_insert, tbl_insert, ro_insert,
};

void no_assign(mu_t m, mu_t k, mu_t v) { mu_err_undefined(); }
void ro_assign(mu_t m, mu_t k, mu_t v) { mu_err_readonly(); }

void (*const mu_assign_table[6])(mu_t, mu_t, mu_t) = {
    no_assign, no_assign, no_assign,
    no_assign, tbl_assign, ro_assign,
};

// Function calls performed on variables
void no_fcall(mu_t m, frame_t c, mu_t *f) { mu_err_undefined(); }

void tbl_fcall(mu_t m, frame_t c, mu_t *frame) {
    mu_t call_hook = tbl_lookup(m, mcstr("call"));

    if (call_hook)
        mu_fcall(call_hook, c, frame);
    else
        no_fcall(m, c, frame);
}

void (*const mu_fcall_table[6])(mu_t, frame_t, mu_t *) = {
    no_fcall, no_fcall, no_fcall,
    fn_fcall, tbl_fcall, tbl_fcall,
};

// Function calls with varargs handling
mu_t mu_vcall(mu_t m, frame_t c, va_list args) {
    mu_t frame[MU_FRAME];

    mu_toframe(c >> 4, frame, args);
    mu_fcall(m, c, frame);
    return mu_fromframe(0xf & c, frame, args);
}

mu_t mu_call(mu_t m, frame_t c, ...) {
    va_list args;
    va_start(args, c);
    mu_t ret = mu_vcall(m, c, args);
    va_end(args);
    return ret;
}
