/*
 *  Parser for Mu
 */

#ifndef MU_PARSE_H
#define MU_PARSE_H
#include "mu.h"
#include "types.h"
#include "lex.h"
#include "fn.h"


// State of parsing
struct parse {
    mstr_t *bcode;
    len_t bcount;
    tbl_t *imms;
    tbl_t *fns;

    struct lex l;
    struct fn_flags flags;

    struct f_parse {
        len_t target;
        len_t count;
        len_t index;
        uintq_t paren;

        uintq_t single  : 1;
        uintq_t unpack  : 1;
        uintq_t insert  : 1;
        uintq_t tabled  : 1;
        uintq_t key     : 1;
        uintq_t call    : 1;
        uintq_t expand  : 1;
    } f;

    mu_packed enum {
        P_DIRECT,
        P_INDIRECT,
        P_SCOPED,
        P_CALLED
    } state;

    uintq_t sp;
    uintq_t args;
};


// Entry points into parsing Mu source into bytecode
// The only difference between parsing functions and
// modules is that modules return their scope for use 
// in type and module definitions
code_t *mu_parse_expr(str_t *code);
code_t *mu_parse_fn(str_t *code);
code_t *mu_parse_module(str_t *code);


#endif
