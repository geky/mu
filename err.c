#include "err.h"

#include "str.h"
#include "tbl.h"
#include <stdlib.h>


static mu_thread struct eh *global_eh = 0;
static mu_thread mu_t global_err = 0;

struct eh *eh_get(void) { return global_eh; }
void eh_set(struct eh *eh) { global_eh = eh; }
mu_t err_get(void) { return global_err; }


void eh_handle(struct eh *eh, mu_t err) {
    if (!eh->handles)
        return;

    // If we encounter an error while handling another
    // error, were just going to ignore it.
    if (setjmp(eh->env) == 0) {
        mu_t type = tbl_lookup(err, mcstr("type"));
        mu_t handle = tbl_lookup(eh->handles, type);

        if (handle)
            mu_call(handle, 0x10, err);
    }
}


mu_noreturn mu_err(mu_t err) {
    struct eh *eh = eh_get();
    global_err = err;

    // If no error handler has been registered, we
    // just abort here
    if (!eh)
        abort();

    // Just jump to the seteh call. We'll let it take care
    // of handling things there since it has more stack space
    longjmp(eh->env, 1);
}

mu_noreturn mu_cerr(mu_t type, mu_t reason) {
    mu_t err = tbl_create(2, 0);
    tbl_insert(err, mcstr("type"), type);
    tbl_insert(err, mcstr("reason"), reason);
    mu_err(err);
}


mu_noreturn mu_err_nomem(void) {
    mu_assert(false); // Figure out how to recover later

    mu_cerr(mcstr("memory"),
            mcstr("out of memory"));
}

mu_noreturn mu_err_len(void) {
    mu_cerr(mcstr("length"),
            mcstr("exceeded max length"));
}

mu_noreturn mu_err_readonly(void) {
    mu_cerr(mcstr("readonly"),
            mcstr("assigning to readonly table"));
}

mu_noreturn mu_err_parse(void) {
    mu_cerr(mcstr("parse"),
            mcstr("expression could not be parsed"));
}

mu_noreturn mu_err_undefined(void) {
    mu_cerr(mcstr("undefined"),
            mcstr("operation is undefined for type"));
}
