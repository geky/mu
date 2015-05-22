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

// TODO move this lex.c?
struct l_parse {
    const data_t *pos;
    const data_t *end;

    mu_t val;
    tok_t tok;

    uintq_t paren;
    uintq_t indent;
    uintq_t lookahead : 1;
};

struct f_parse {
    len_t lcount;
    len_t rcount;
    uintq_t tabled  : 1;
    uintq_t call    : 1;
    uintq_t unpack  : 1;
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

    struct op_parse op;
    struct l_parse l;
    struct f_parse f;
    struct fn_flags flags;

    enum {
        P_DIRECT,
        P_INDIRECT,
        P_SCOPED,
        P_CALLED
    } state;

    uintq_t sp;
    uintq_t args;

    bool insert   : 1;
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
