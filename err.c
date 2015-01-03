#include "err.h"

#include "str.h"
#include "tbl.h"


void mu_handle(tbl_t *err, eh_t *eh) {
    if (eh->handles) {
        if (setjmp(eh->env) == 0) {
            mu_t type = tbl_lookup(err, mcstr("type", eh));
            mu_t handle = tbl_lookup(eh->handles, type);

            if (isfn(handle)) {
                // This is just for returning from an error
                // were just going to ignore it at this point
                if (mu_likely(setjmp(eh->env) == 0)) {
                    tbl_t *args = tbl_create(1, eh);
                    tbl_append(args, mtbl(err), eh);

                    mu_call(handle, args, eh);
                }
            }
        }
    }
}


mu_noreturn void mu_err(tbl_t *err, eh_t *eh) {
    // Just jump to the seteh call. We'll let it take care 
    // of handling things since it has more stack space
    longjmp(eh->env, (int)err);
}

mu_noreturn void mu_cerr(mu_t type, mu_t reason, eh_t *eh) {
    tbl_t *err = tbl_create(2, eh);
    tbl_insert(err, mcstr("type", eh), type, eh);
    tbl_insert(err, mcstr("reason", eh), reason, eh);

    mu_err(err, eh);
}


mu_noreturn void err_nomem(eh_t *eh) {
    mu_assert(false); // Figure out how to recover later

    // NOTE: BAD, we're already out of memory
    // This is idiotic but will just fix it later
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, mcstr("type", eh), mcstr("memory", eh), eh);
    tbl_insert(err, mcstr("reason", eh), mcstr("out of memory", eh), eh);

    mu_err(err, eh);
}

mu_noreturn void err_len(eh_t *eh) {
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, mcstr("type", eh), mcstr("length", eh), eh);
    tbl_insert(err, mcstr("reason", eh), mcstr("exceeded max length", eh), eh);

    mu_err(err, eh);
}

mu_noreturn void err_readonly(eh_t *eh) {
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, mcstr("type", eh), mcstr("readonly", eh), eh);
    tbl_insert(err, mcstr("reason", eh), mcstr("assigning to readonly table", eh), eh);

    mu_err(err, eh);
}

mu_noreturn void err_parse(eh_t *eh) {
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, mcstr("type", eh), mcstr("parse", eh), eh);
    tbl_insert(err, mcstr("reason", eh), mcstr("expression could not be parsed", eh), eh);

    mu_err(err, eh);
}

mu_noreturn void err_undefined(eh_t *eh) {
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, mcstr("type", eh), mcstr("undefined", eh), eh);
    tbl_insert(err, mcstr("reason", eh), mcstr("operation is undefined for type", eh), eh);

    mu_err(err, eh);
}
