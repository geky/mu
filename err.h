/*
 * Error Handling
 */

#ifdef MU_DEF
#ifndef MU_ERR_DEF
#define MU_ERR_DEF
#include "mu.h"


// Definition of overhead for error handler in Mu
typedef struct eh eh_t;


#endif
#else
#ifndef MU_ERR_H
#define MU_ERR_H
#define MU_DEF
#include "err.h"
#include "types.h"
#undef MU_DEF
#include <setjmp.h>


struct eh {
    struct eh *prev;
    tbl_t *handles;
    jmp_buf env;
};


eh_t *eh_get(void);
void eh_set(eh_t *eh);

void eh_handle(eh_t *eh, tbl_t *err);


mu_noreturn void mu_err(tbl_t *err);
mu_noreturn void mu_cerr(str_t *type, str_t *reason);

mu_noreturn void mu_err_nomem(void);
mu_noreturn void mu_err_len(void);
mu_noreturn void mu_err_readonly(void);
mu_noreturn void mu_err_parse(void);
mu_noreturn void mu_err_undefined(void);


#define mu_try_begin {                      \
    eh_t _eh = {eh_get(), 0};               \
    tbl_t *_err = (tbl_t *)setjmp(_eh.env); \
                                            \
    if (mu_unlikely(_err != 0))             \
        eh_handle(&_eh, _err);              \
                                            \
    if (mu_likely(_err == 0)) {             \
        eh_set(&_eh);                       \
{
#define mu_on_err(err)                      \
}                                           \
    } else {                                \
        mu_unused tbl_t *err = _err;        \
        eh_set(_eh.prev);                   \
{
#define mu_try_end                          \
}                                           \
    }                                       \
                                            \
    eh_set(_eh.prev);                       \
}


#endif
#endif
