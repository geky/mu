#include "err.h"

#include "var.h"
#include "tbl.h"

__attribute__((noreturn)) void v_err(tbl_t *err, veh_t *eh) {
    longjmp(eh, (uint32_t)err);
}

__attribute__((noreturn)) void err_nomem(veh_t *eh) {
    assert(false); // Figure out how to recover later

    // NOTE: BAD, we're already out of memory
    // This is idiotic but will just fix it later
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, vcstr("type"), vcstr("memory error"), eh);
    tbl_insert(err, vcstr("reason"), vcstr("out of memory"), eh);

    v_err(err, eh);
}

__attribute__((noreturn)) void err_len(veh_t *eh) {
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, vcstr("type"), vcstr("length error"), eh);
    tbl_insert(err, vcstr("reason"), vcstr("exceeded max length"), eh);

    v_err(err, eh);
}

__attribute__((noreturn)) void err_ro(veh_t *eh) {
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, vcstr("type"), vcstr("readonly error"), eh);
    tbl_insert(err, vcstr("reason"), vcstr("assigning to readonly table"), eh);

    v_err(err, eh);
}

__attribute__((noreturn)) void err_parse(veh_t *eh) {
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, vcstr("type"), vcstr("parse error"), eh);
    tbl_insert(err, vcstr("reason"), vcstr("expression could not be parsed"), eh);

    v_err(err, eh);
}

__attribute__((noreturn)) void err_undefined(veh_t *eh) {
    tbl_t *err = tbl_create(0, eh);
    tbl_insert(err, vcstr("type"), vcstr("undefined error"), eh);
    tbl_insert(err, vcstr("reason"), vcstr("operation is undefined for type"), eh);

    v_err(err, eh);
}
