/*
 * Mu parsing and compilation
 */

#ifndef MU_PARSE_H
#define MU_PARSE_H
#include "mu.h"


// Print parsable representation of literals
mu_t mu_repr(mu_t m, mu_t depth);

// Parse literals without side-effects
mu_t mu_parsen(const mbyte_t **s, const mbyte_t *end);
mu_t mu_parse(const char *s, muint_t n);

// Compile Mu source code into code objects
mu_t mu_compilen(const mbyte_t **s, const mbyte_t *end);
mu_t mu_compile(const char *s, muint_t n);


// Language keywords
#define MU_KEYWORDS     mu_keywords_def()

#define MU_KW_LET       mu_let_key_def()
#define MU_KW_ELSE      mu_else_key_def()
#define MU_KW_AND       mu_and_key_def()
#define MU_KW_OR        mu_or_key_def()
#define MU_KW_CONT      mu_continue_key_def()
#define MU_KW_BREAK     mu_break_key_def()
#define MU_KW_RETURN    mu_return_key_def()
#define MU_KW_FN        mu_fn_key_def()
#define MU_KW_TYPE      mu_type_key_def()
#define MU_KW_IF        mu_if_key_def()
#define MU_KW_WHILE     mu_while_key_def()
#define MU_KW_FOR       mu_for_key_def()
#define MU_KW_NIL       mu_nil_key_def()
#define MU_KW_NIL2      mu_def_key_nil2()
#define MU_KW_ASSIGN    mu_assign_key_def()
#define MU_KW_PAIR      mu_pair_key_def()
#define MU_KW_DOT       mu_dot_key_def()
#define MU_KW_ARROW     mu_arrow_key_def()
#define MU_KW_EXPAND    mu_expand_key_def()

mu_t mu_keywords_def(void);

mu_t mu_let_key_def(void);
mu_t mu_else_key_def(void);
mu_t mu_and_key_def(void);
mu_t mu_or_key_def(void);
mu_t mu_continue_key_def(void);
mu_t mu_break_key_def(void);
mu_t mu_return_key_def(void);
mu_t mu_fn_key_def(void);
mu_t mu_type_key_def(void);
mu_t mu_if_key_def(void);
mu_t mu_while_key_def(void);
mu_t mu_for_key_def(void);
mu_t mu_nil_key_def(void);
mu_t mu_def_key_nil2(void);
mu_t mu_assign_key_def(void);
mu_t mu_pair_key_def(void);
mu_t mu_dot_key_def(void);
mu_t mu_arrow_key_def(void);
mu_t mu_expand_key_def(void);


#endif
