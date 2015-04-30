/*
 *  Parser for Mu
 */

#ifdef MU_DEF
#ifndef MU_PARSE_DEF
#define MU_PARSE_DEF
#include "mu.h"


typedef struct parse parse_t;


#endif
#else
#ifndef MU_PARSE_H
#define MU_PARSE_H
#define MU_DEF
#include "parse.h"
#include "lex.h"
#include "mem.h"
#include "types.h"
#undef MU_DEF
#include "fn.h"


// Specific state structures

struct l_parse {
    const data_t *pos;
    const data_t *end;

    uintq_t pass : 1;
    tok_t tok;
    mu_t val;
};

struct f_parse {
    uintq_t tabled : 1;
    len_t lcount;
    len_t rcount;
};

struct op_parse {
    uintq_t lprec;
    uintq_t rprec;
};

typedef struct parse {
    mstr_t *bcode;
    len_t bcount;

    tbl_t *imms;
    tbl_t *fns;
    len_t sp;
    len_t smax;
    uintq_t paren;

    struct op_parse op;
    struct l_parse l;
    struct f_parse f;

    bool indirect     : 1;
    bool scoped       : 1;
    bool rested       : 1;
    bool insert       : 1;
    bool unpack       : 1;
} parse_t;


// Entry points into parsing Mu source into bytecode
// The only difference between parsing functions and
// modules is that modules return their scope for use 
// in type and module definitions
code_t *mu_parse_expr(str_t *code);
code_t *mu_parse_fn(str_t *code);
code_t *mu_parse_module(str_t *code);

#endif
#endif
