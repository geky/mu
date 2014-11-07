#include "err.h"

#include "var.h"
#include "tbl.h"


mu_noreturn void mu_err(tbl_t *err, eh_t *eh) {
    longjmp(eh, (uint32_t)err);
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
