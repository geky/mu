/*
 *  Parser for Mu
 */

#ifdef MU_DEF
#ifndef MU_PARSE_DEF
#define MU_PARSE_DEF
#include "mu.h"
#include "var.h"
#include "tbl.h"
#include "err.h"
#include "lex.h"
#include "mem.h"


typedef struct parse parse_t;


#endif
#else
#ifndef MU_PARSE_H
#define MU_PARSE_H
#define MU_DEF
#include "parse.h"
#undef MU_DEF


// Specific state structures
struct opparse {
    len_t ins;
    uintq_t lprec;
    uintq_t rprec;
};

struct jparse {
    tbl_t *ctbl;
    tbl_t *btbl;
};

struct fnparse {
    ref_t ref;

    uintq_t stack;
    uintq_t type;

    union {
        tbl_t *closure;
        struct {
            len_t ins;
        };
    };

    tbl_t *imms;
    mstr_t *bcode;
};

// State of a parse
struct parse {
    struct fnparse *f;
    struct jparse j;
    struct opparse op;
    tbl_t *args;
    tbl_t *keys;

    uintq_t indirect;
    uintq_t stmt;
    uintq_t left;
    uintq_t key;
    uintq_t paren;

    uintq_t jsize;
    uintq_t jtsize;
    uintq_t jfsize;

    tok_t tok;
    var_t val;

    ref_t *ref;
    const data_t *str;
    const data_t *pos;
    const data_t *end;

    eh_t *eh;
};


// Entry points into parsing Mu source into bytecode
parse_t *parse_create(var_t code, eh_t *eh);
void parse_destroy(parse_t *p);

void parse_args(parse_t *p, tbl_t *args);
void parse_stmts(parse_t *p);
void parse_stmt(parse_t *p);
void parse_expr(parse_t *p);
void parse_end(parse_t *p);

#endif
#endif
