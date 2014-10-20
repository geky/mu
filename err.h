/*
 * Error Handling
 */

#ifndef V_ERR
#define V_ERR

#include <setjmp.h>


// Definition of overhead for error handler in V
extern jmp_buf _jmp_buf;
typedef typeof(_jmp_buf[0]) veh_t;


// Try a statement returning a table pointer if exception 
// is thrown, null marks successful execution
#define v_try_begin(err, eh) {              \
    veh_t eh[1];                            \
                                            \
    err = (tbl_t *)setjmp(eh);              \
                                            \
    if (__builtin_expect(err == 0, 1)) {    \
{
#define v_try_end                           \
}                                           \
    }                                       \
}


#define v_on_err_begin(eh) {                \
    veh_t **_eh = &(eh);                    \
    veh_t _new_eh[1];                       \
    veh_t *_old_eh = *_eh;                  \
                                            \
    tbl_t *_err = (tbl_t *)setjmp(_new_eh); \
                                            \
    if (__builtin_expect(_err == 0, 1)) {   \
        *_eh = _new_eh;                     \
{
#define v_on_err_do                         \
}                                           \
        *_eh = _old_eh;                     \
    } else {                                \
        *_eh = _old_eh;                     \
{
#define v_on_err_end                        \
}                                           \
        v_err(_err, _old_eh);               \
    }                                       \
}


// Redundant definition of table struct to avoid 
// dragging in all of var.h
typedef struct tbl tbl_t;

__attribute__((noreturn)) void v_err(tbl_t *err, veh_t *eh);


__attribute__((noreturn)) void err_nomem(veh_t *eh);
__attribute__((noreturn)) void err_len(veh_t *eh);
__attribute__((noreturn)) void err_ro(veh_t *eh);
__attribute__((noreturn)) void err_parse(veh_t *eh);
__attribute__((noreturn)) void err_undefined(veh_t *eh);


#endif
