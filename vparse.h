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
    VTOK_END = 0,
    VTOK_FN = 0x80,
    VTOK_RETURN,
    VTOK_IDENT,
    VTOK_NUM,
    VTOK_STR,
    VTOK_OP,
    VTOK_DOT,
    VTOK_ASSIGN,
    VTOK_SET,
    VTOK_AND,
    VTOK_OR,
    VTOK_SPACE,
    VTOK_TERM
};

// State of a parse
struct vstate {
    str_t *off;
    str_t *end;
    ref_t *ref;

    tbl_t *scope;
    vtok_t tok;
    var_t val;
};


// Parses V source code and evaluates the result
var_t vparse(struct vstate *);


#endif
