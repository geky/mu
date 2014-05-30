/*
 *  Recursive descent parser for V
 */

#ifndef V_PARSE
#define V_PARSE

#include "var.h"


// Token representation in V
// single character tokens are also used
// so enum starts after valid ascii range
typedef int vtok_t;

enum vtok {
    VT_END = 0,
    VT_FN = 0x80,
    VT_RETURN,
    VT_IDENT,
    VT_NUM,
    VT_STR,
    VT_OP,
    VT_DOT,
    VT_ASSIGN,
    VT_SET,
    VT_AND,
    VT_OR,
    VT_SPACE,
    VT_TERM
};

// State of a parse
struct vstate {
    str_t *off;
    str_t *end;
    ref_t *ref;

    tbl_t *scope;
    var_t val;

    uint8_t paren;
    vtok_t tok;
};


// Parses V source code and evaluates the result
var_t vparse(struct vstate *);


#endif
