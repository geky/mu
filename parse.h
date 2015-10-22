/*
 *  Parser for Mu
 */

#ifndef MU_PARSE_H
#define MU_PARSE_H
#include "mu.h"


// Parse literals without side-effects
mu_t mu_parse(mu_t m);
mu_t mu_nparse(const mbyte_t **pos, const mbyte_t *end);

// Compile Mu source code into code objects
struct code *mu_compile(mu_t s);
struct code *mu_ncompile(const mbyte_t *pos, const mbyte_t *end);


// Conversion to/from ascii
mu_inline muint_t mu_fromascii(mbyte_t c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else
        return -1;
}

mu_inline mbyte_t mu_toascii(muint_t c) {
    if (c < 10)
        return '0' + c;
    else
        return 'a' + (c-10);
}


// Keywords
#define MU_KEYWORDS mu_keywords()
mu_pure mu_t mu_keywords(void);

#define MU_KW_LET mu_kw_let()
mu_pure mu_t mu_kw_let(void);
#define MU_KW_ELSE mu_kw_else()
mu_pure mu_t mu_kw_else(void);
#define MU_KW_AND mu_kw_and()
mu_pure mu_t mu_kw_and(void);
#define MU_KW_OR mu_kw_or()
mu_pure mu_t mu_kw_or(void);
#define MU_KW_CONT mu_kw_continue()
mu_pure mu_t mu_kw_continue(void);
#define MU_KW_BREAK mu_kw_break()
mu_pure mu_t mu_kw_break(void);
#define MU_KW_RETURN mu_kw_return()
mu_pure mu_t mu_kw_return(void);
#define MU_KW_FN mu_kw_fn()
mu_pure mu_t mu_kw_fn(void);
#define MU_KW_TYPE mu_kw_type()
mu_pure mu_t mu_kw_type(void);
#define MU_KW_IF mu_kw_if()
mu_pure mu_t mu_kw_if(void);
#define MU_KW_WHILE mu_kw_while()
mu_pure mu_t mu_kw_while(void);
#define MU_KW_FOR mu_kw_for()
mu_pure mu_t mu_kw_for(void);
#define MU_KW_NIL mu_kw_nil()
mu_pure mu_t mu_kw_nil(void);
#define MU_KW_NIL2 mu_kw_nil2()
mu_pure mu_t mu_kw_nil2(void);
#define MU_KW_ASSIGN mu_kw_assign()
mu_pure mu_t mu_kw_assign(void);
#define MU_KW_PAIR mu_kw_pair()
mu_pure mu_t mu_kw_pair(void);
#define MU_KW_DOT mu_kw_dot()
mu_pure mu_t mu_kw_dot(void);
#define MU_KW_ARROW mu_kw_arrow()
mu_pure mu_t mu_kw_arrow(void);
#define MU_KW_EXPAND mu_kw_expand()
mu_pure mu_t mu_kw_expand(void);


#endif
