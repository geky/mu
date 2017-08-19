/*
 * Mu parsing and compilation
 *
 * Copyright (c) 2016 Christopher Haster
 * Distributed under the MIT license in mu.h
 */
#ifndef MU_PARSE_H
#define MU_PARSE_H
#include "config.h"
#include "types.h"


// Print parsable representation of literals
mu_t mu_repr(mu_t m, mu_t depth);

// Parse literals without side-effects
mu_t mu_parsen(const mbyte_t **s, const mbyte_t *end);
mu_t mu_parse(const char *s, muint_t n);

// Compile Mu source code into code objects
mu_t mu_compilen(const mbyte_t **s, const mbyte_t *end, mu_t scope);
mu_t mu_compile(const char *s, muint_t n, mu_t scope);


// Language keywords
#define MU_KEYWORDS     mu_keywords_def()

#define MU_KW_LET       mu_kw_let_def()
#define MU_KW_ELSE      mu_kw_else_def()
#define MU_KW_AND       mu_kw_and_def()
#define MU_KW_OR        mu_kw_or_def()
#define MU_KW_CONT      mu_kw_continue_def()
#define MU_KW_BREAK     mu_kw_break_def()
#define MU_KW_RETURN    mu_kw_return_def()
#define MU_KW_FN        mu_kw_fn_def()
#define MU_KW_TYPE      mu_kw_type_def()
#define MU_KW_IF        mu_kw_if_def()
#define MU_KW_WHILE     mu_kw_while_def()
#define MU_KW_FOR       mu_kw_for_def()
#define MU_KW_NIL       mu_kw_nil_def()
#define MU_KW_NIL2      mu_kw_nil2_def()
#define MU_KW_ASSIGN    mu_kw_assign_def()
#define MU_KW_PAIR      mu_kw_pair_def()
#define MU_KW_DOT       mu_kw_dot_def()
#define MU_KW_ARROW     mu_kw_arrow_def()
#define MU_KW_EXPAND    mu_kw_expand_def()

MU_DEF(mu_keywords_def)

MU_DEF(mu_kw_let_def)
MU_DEF(mu_kw_else_def)
MU_DEF(mu_kw_and_def)
MU_DEF(mu_kw_or_def)
MU_DEF(mu_kw_continue_def)
MU_DEF(mu_kw_break_def)
MU_DEF(mu_kw_return_def)
MU_DEF(mu_kw_fn_def)
MU_DEF(mu_kw_type_def)
MU_DEF(mu_kw_if_def)
MU_DEF(mu_kw_while_def)
MU_DEF(mu_kw_for_def)
MU_DEF(mu_kw_nil_def)
MU_DEF(mu_kw_nil2_def)
MU_DEF(mu_kw_assign_def)
MU_DEF(mu_kw_pair_def)
MU_DEF(mu_kw_dot_def)
MU_DEF(mu_kw_arrow_def)
MU_DEF(mu_kw_expand_def)


#endif
