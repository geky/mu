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
struct code *mu_compile(const char *s, muint_t n);
struct code *mu_ncompile(const mbyte_t **pos, const mbyte_t *end);


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


// Keywords
#define MU_KEYWORDS mu_keywords()
mgen_t mu_keywords;

#define MU_KW_LET mu_kw_let()
mgen_t mu_kw_let;
#define MU_KW_ELSE mu_kw_else()
mgen_t mu_kw_else;
#define MU_KW_AND mu_kw_and()
mgen_t mu_kw_and;
#define MU_KW_OR mu_kw_or()
mgen_t mu_kw_or;
#define MU_KW_CONT mu_kw_continue()
mgen_t mu_kw_continue;
#define MU_KW_BREAK mu_kw_break()
mgen_t mu_kw_break;
#define MU_KW_RETURN mu_kw_return()
mgen_t mu_kw_return;
#define MU_KW_FN mu_kw_fn()
mgen_t mu_kw_fn;
#define MU_KW_TYPE mu_kw_type()
mgen_t mu_kw_type;
#define MU_KW_IF mu_kw_if()
mgen_t mu_kw_if;
#define MU_KW_WHILE mu_kw_while()
mgen_t mu_kw_while;
#define MU_KW_FOR mu_kw_for()
mgen_t mu_kw_for;
#define MU_KW_NIL mu_kw_nil()
mgen_t mu_kw_nil;
#define MU_KW_NIL2 mu_kw_nil2()
mgen_t mu_kw_nil2;
#define MU_KW_ASSIGN mu_kw_assign()
mgen_t mu_kw_assign;
#define MU_KW_PAIR mu_kw_pair()
mgen_t mu_kw_pair;
#define MU_KW_DOT mu_kw_dot()
mgen_t mu_kw_dot;
#define MU_KW_ARROW mu_kw_arrow()
mgen_t mu_kw_arrow;
#define MU_KW_EXPAND mu_kw_expand()
mgen_t mu_kw_expand;


#endif
