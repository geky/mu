#include "err.h"

#include "str.h"
#include "tbl.h"
#include <stdlib.h>


static mu_thread eh_t *global_eh = 0;

eh_t *eh_get(void) { return global_eh; }
void eh_set(eh_t *eh) { global_eh = eh; }


void eh_handle(eh_t *eh, tbl_t *err) {
    if (!eh->handles)
        return;

    // If we encounter an error while handling another
    // error, were just going to ignore it.
    if (setjmp(eh->env) == 0) {
        mu_t type = tbl_lookup(err, mcstr("type"));
        mu_t handle = tbl_lookup(eh->handles, type);

        if (!isnil(handle))
            mu_call(handle, 0x10, (mu_t []){ mtbl(err) }, 0);
    }
}


mu_noreturn void mu_err(tbl_t *err) {
    eh_t *eh = eh_get();

    // If no error handler has been registered, we 
    // just abort here
    if (!eh)
        abort();

    // Just jump to the seteh call. We'll let it take care 
    // of handling things there since it has more stack space
    longjmp(eh->env, (int)err);
}

mu_noreturn void mu_cerr(str_t *type, str_t *reason) {
    tbl_t *err = tbl_create(2);
    tbl_insert(err, mcstr("type"), mstr(type));
    tbl_insert(err, mcstr("reason"), mstr(reason));
    mu_err(err);
}


mu_noreturn void mu_err_nomem() {
    mu_assert(false); // Figure out how to recover later

    mu_cerr(str_cstr("memory"), 
            str_cstr("out of memory"));
}

mu_noreturn void mu_err_len() {
    mu_cerr(str_cstr("length"),
            str_cstr("exceeded max length"));
}

mu_noreturn void mu_err_readonly() {
    mu_cerr(str_cstr("readonly"),
            str_cstr("assigning to readonly table"));
}

mu_noreturn void mu_err_parse() {
    mu_cerr(str_cstr("parse"),
            str_cstr("expression could not be parsed"));
}

mu_noreturn void mu_err_undefined() {
    mu_cerr(str_cstr("undefined"),
            str_cstr("operation is undefined for type"));
}
