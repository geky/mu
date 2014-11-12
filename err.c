#include "err.h"

#include "var.h"
#include "tbl.h"


tbl_t *mu_eh(eh_t *eh) {
    eh->handles = 0;

    tbl_t *err = (tbl_t *)setjmp(eh->env);

    if (mu_unlikely(err != 0)) {
        // Take care of error handlers here to make sure
        // we have the stack space for them
        // TODO dont do this when out of memory
        if (eh->handles) {
            var_t type = tbl_lookup(err, vcstr("type"));
            var_t handle = tbl_lookup(eh->handles, type);

            if (isfn(handle)) {
                // This is just for returning from an error
                // were just going to ignore it at this point
                if (mu_likely(setjmp(eh->env) == 0)) {
                    tbl_t *args = tbl_create(1, eh);
                    tbl_append(args, vtbl(err), eh);

                    var_call(handle, args, eh);
                }
            }
        }
    }

    return err;
}


mu_noreturn void mu_err(tbl_t *err, eh_t *eh) {
    // Just jump to the seteh call. We'll let it take care 
    // of handling things since it has more stack space
    longjmp(eh->env, (int)err);
}

mu_noreturn void err_nomem(eh_t *eh) {
    mu_assert(false); // Figure out how to recover later

    // NOTE: BAD, we're already out of memory
    // This is idiotic but will just fix it later
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, vcstr("type"), vcstr("memory error"), eh);
    tbl_insert(err, vcstr("reason"), vcstr("out of memory"), eh);

    mu_err(err, eh);
}

mu_noreturn void err_len(eh_t *eh) {
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, vcstr("type"), vcstr("length error"), eh);
    tbl_insert(err, vcstr("reason"), vcstr("exceeded max length"), eh);

    mu_err(err, eh);
}

mu_noreturn void err_ro(eh_t *eh) {
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, vcstr("type"), vcstr("readonly error"), eh);
    tbl_insert(err, vcstr("reason"), vcstr("assigning to readonly table"), eh);

    mu_err(err, eh);
}

mu_noreturn void err_parse(eh_t *eh) {
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, vcstr("type"), vcstr("parse error"), eh);
    tbl_insert(err, vcstr("reason"), vcstr("expression could not be parsed"), eh);

    mu_err(err, eh);
}

mu_noreturn void err_undefined(eh_t *eh) {
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, vcstr("type"), vcstr("undefined error"), eh);
    tbl_insert(err, vcstr("reason"), vcstr("operation is undefined for type"), eh);

    mu_err(err, eh);
}
