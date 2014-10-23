/*
 * Error Handling
 */

#ifndef MU_ERR_H
#define MU_ERR_H

#include <setjmp.h>


// Definition of overhead for error handler in Mu
extern jmp_buf _jmp_buf;
typedef typeof(_jmp_buf[0]) eh_t;


// Try a statement returning a table pointer if exception 
// is thrown, null marks successful execution
#define mu_try_begin(err, eh) {             \
    eh_t eh[1];                             \
                                            \
    err = (tbl_t *)setjmp(eh);              \
                                            \
    if (__builtin_expect(err == 0, 1)) {    \
{
#define mu_try_end                          \
}                                           \
    }                                       \
}


#define mu_on_err_begin(eh) {               \
    eh_t **_eh = &(eh);                     \
    eh_t _new_eh[1];                        \
    eh_t *_old_eh = *_eh;                   \
                                            \
    tbl_t *_err = (tbl_t *)setjmp(_new_eh); \
                                            \
    if (__builtin_expect(_err == 0, 1)) {   \
        *_eh = _new_eh;                     \
{
#define mu_on_err_do                        \
}                                           \
        *_eh = _old_eh;                     \
    } else {                                \
        *_eh = _old_eh;                     \
{
#define mu_on_err_end                       \
}                                           \
        mu_err(_err, _old_eh);              \
    }                                       \
}


// Redundant definition of table struct to avoid 
// dragging in all of var.h
typedef struct tbl tbl_t;

__attribute__((noreturn)) void mu_err(tbl_t *err, eh_t *eh);


__attribute__((noreturn)) void err_nomem(eh_t *eh);
__attribute__((noreturn)) void err_len(eh_t *eh);
__attribute__((noreturn)) void err_ro(eh_t *eh);
__attribute__((noreturn)) void err_parse(eh_t *eh);
__attribute__((noreturn)) void err_undefined(eh_t *eh);


#endif
