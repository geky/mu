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


tbl_t *mu_eh(eh_t *eh);

mu_noreturn void mu_err(tbl_t *err, eh_t *eh);


mu_noreturn void err_nomem(eh_t *eh);
mu_noreturn void err_len(eh_t *eh);
mu_noreturn void err_ro(eh_t *eh);
mu_noreturn void err_parse(eh_t *eh);
mu_noreturn void err_undefined(eh_t *eh);


#endif
#endif
