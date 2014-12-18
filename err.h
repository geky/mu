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
#include "tbl.h"
#undef MU_DEF
#include <setjmp.h>


struct eh {
    tbl_t *handles;
    jmp_buf env;
};


mu_noreturn void mu_err(tbl_t *err, eh_t *eh);
mu_noreturn void mu_cerr(var_t type, var_t reason, eh_t *eh);

void mu_handle(tbl_t *err, eh_t *eh);


mu_noreturn void err_nomem(eh_t *eh);
mu_noreturn void err_len(eh_t *eh);
mu_noreturn void err_readonly(eh_t *eh);
mu_noreturn void err_parse(eh_t *eh);
mu_noreturn void err_undefined(eh_t *eh);


#define mu_try_begin(eh) {                  \
    eh_t _eh;                               \
    _eh.handles = 0;                        \
                                            \
    tbl_t *_err = (tbl_t *)setjmp(_eh.env); \
                                            \
    if (mu_unlikely(_err != 0))             \
        mu_handle(_err, &_eh);              \
                                            \
    if (mu_likely(_err == 0)) {             \
        mu_unused eh_t *eh = &_eh;          \
{
#define mu_on_err(err)                      \
}                                           \
    } else {                                \
        mu_unused tbl_t *err = _err;        \
{
#define mu_try_end                          \
}                                           \
    }                                       \
}
    

#endif
#endif
