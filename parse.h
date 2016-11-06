/*
 * Mu parsing and compilation
 */

#ifndef MU_PARSE_H
#define MU_PARSE_H
#include "mu.h"


// Parse literals without side-effects
mu_t mu_parse(const char *s, muint_t n);
mu_t mu_nparse(const mbyte_t **pos, const mbyte_t *end);

// Compile Mu source code into code objects
mu_t mu_compile(const char *s, muint_t n);
mu_t mu_ncompile(const mbyte_t **pos, const mbyte_t *end);


// Conversion to/from ascii
mu_inline muint_t mu_fromascii(mbyte_t c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else {
        return -1;
    }
}

mu_inline mbyte_t mu_toascii(muint_t c) {
    if (c < 10) {
        return '0' + c;
    } else {
        return 'a' + (c-10);
    }
}


// Language keywords
#define MU_KEYWORDS     mu_gen_keywords()

#define MU_KW_LET       mu_gen_key_let()
#define MU_KW_ELSE      mu_gen_key_else()
#define MU_KW_AND       mu_gen_key_and()
#define MU_KW_OR        mu_gen_key_or()
#define MU_KW_CONT      mu_gen_key_continue()
#define MU_KW_BREAK     mu_gen_key_break()
#define MU_KW_RETURN    mu_gen_key_return()
#define MU_KW_FN        mu_gen_key_fn()
#define MU_KW_TYPE      mu_gen_key_type()
#define MU_KW_IF        mu_gen_key_if()
#define MU_KW_WHILE     mu_gen_key_while()
#define MU_KW_FOR       mu_gen_key_for()
#define MU_KW_NIL       mu_gen_key_nil()
#define MU_KW_NIL2      mu_gen_key_nil2()
#define MU_KW_ASSIGN    mu_gen_key_assign()
#define MU_KW_PAIR      mu_gen_key_pair()
#define MU_KW_DOT       mu_gen_key_dot()
#define MU_KW_ARROW     mu_gen_key_arrow()
#define MU_KW_EXPAND    mu_gen_key_expand()

mu_t mu_gen_keywords(void);

mu_t mu_gen_key_let(void);
mu_t mu_gen_key_else(void);
mu_t mu_gen_key_and(void);
mu_t mu_gen_key_or(void);
mu_t mu_gen_key_continue(void);
mu_t mu_gen_key_break(void);
mu_t mu_gen_key_return(void);
mu_t mu_gen_key_fn(void);
mu_t mu_gen_key_type(void);
mu_t mu_gen_key_if(void);
mu_t mu_gen_key_while(void);
mu_t mu_gen_key_for(void);
mu_t mu_gen_key_nil(void);
mu_t mu_gen_key_nil2(void);
mu_t mu_gen_key_assign(void);
mu_t mu_gen_key_pair(void);
mu_t mu_gen_key_dot(void);
mu_t mu_gen_key_arrow(void);
mu_t mu_gen_key_expand(void);


#endif
