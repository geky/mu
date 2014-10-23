/*
 *  Parser for Mu
 */

#ifndef MU_PARSE_H
#define MU_PARSE_H

#include "var.h"
#include "vm.h"


// Parsing type definitions
typedef uint8_t mtok_t;
typedef struct mstate mstate_t;


// Specific state structures
struct opstate {
    len_t ins;
    uint8_t lprec;
    uint8_t rprec;
};

struct jstate {
    tbl_t *ctbl;
    tbl_t *btbl;
};

struct fnstate {
    str_t *bcode;

    len_t stack;
    len_t len;
    len_t ins;

    tbl_t *fns;
    tbl_t *vars;
};

// State of a parse
struct mstate {
    struct fnstate *fn;
    struct jstate j;
    struct opstate op;
    tbl_t *args;

    tbl_t *keys;

    uint8_t indirect;
    uint8_t stmt;
    uint8_t left;
    uint8_t key;
    uint8_t paren;

    uint8_t jsize;
    uint8_t jtsize;
    uint8_t jfsize;

    mtok_t tok;
    var_t val;

    ref_t *ref;
    const str_t *str;
    const str_t *pos;
    const str_t *end;

    eh_t *eh;
};


// Parses Mu source into bytecode
void mu_parse_init(mstate_t *vs, var_t code);
void mu_parse_args(mstate_t *vs, tbl_t *args);
void mu_parse_top(mstate_t *vs);
void mu_parse_nested(mstate_t *vs);

#endif
