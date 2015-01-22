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
struct opparse {
    len_t ins;
    uintq_t lprec;
    uintq_t rprec;
};

struct jparse {
    tbl_t *ctbl;
    tbl_t *btbl;
};

struct stparse {
    uintq_t max;
    uintq_t off;
    uintq_t scope;
};

struct fnparse {
    union {
        fn_t fn;

        struct {
            ref_t ref;
            len_t ins;
            struct stparse stack;

            tbl_t *imms;
            mstr_t *bcode;
        };
    };
};

// State of a parse
typedef struct parse {
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
    mu_t val;

    ref_t *ref;
    const data_t *str;
    const data_t *pos;
    const data_t *end;
} parse_t;


// Entry points into parsing Mu source into bytecode
parse_t *mu_parse_create(mu_t code);
void mu_parse_destroy(parse_t *p);

void mu_parse_args(parse_t *p, tbl_t *args);
void mu_parse_stmts(parse_t *p);
void mu_parse_stmt(parse_t *p);
void mu_parse_expr(parse_t *p);
void mu_parse_end(parse_t *p);


#endif
#endif
