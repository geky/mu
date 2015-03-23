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
/*
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
//
    tbl_t *lhs;
    tbl_t *rhs;
    struct {
        mstr_t *code;
        data_t *pos;
        data_t *end;
    } b;

    tbl_t *imms;
//
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
*/

// TODO use flexible array member with offsetof 
// to handle alignment issue?
struct chunk {
    ref_t ref;
    len_t size;

    len_t len;
    uintq_t indirect;

    data_t data[];
};

mu_inline struct chunk *getchunk(mu_t m) {
    return (struct chunk *)getstr(m);
}

mu_inline mu_t mchunk(struct chunk *ch) {
    return mstr((mstr_t *)ch);
}

typedef struct parse {
    tbl_t *keys;
    tbl_t *vals;
    struct chunk *ch;

    tbl_t *imms;
    tbl_t *fns;

    tok_t tok;
    mu_t val;

    const data_t *pos;
    const data_t *end;

    uintq_t paren;
    struct opparse op;
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
