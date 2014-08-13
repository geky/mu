/*
 *  Parser for V
 */

#ifndef V_PARSE
#define V_PARSE

#include "var.h"
#include "vm.h"


// Parsing type definitions
typedef uint8_t vtok_t;
typedef int vencode_t(str_t *, vop_t, varg_t);
typedef struct vstate vstate_t;


// Representation of parsing product
enum vproduct {
    VP_NONE = 0,
    VP_REF  = 1,
    VP_VAL  = 2
};


// Specific state structures
struct vopstate {
    len_t ins;
    uint8_t prec;
};

struct vjstate {
    tbl_t *ctbl;
    tbl_t *btbl;
};


// State of a parse
struct vstate {
    ref_t *ref;
    const str_t *str;
    const str_t *pos;
    const str_t *end;

    tbl_t *vars;
    tbl_t *keys;
    tbl_t *ops;

    tbl_t *args;

    struct vopstate op;
    struct vjstate j;

    uint8_t indirect;
    uint8_t paren;
    uint8_t nprec;

    uint8_t jsize;
    uint8_t jtsize;
    uint8_t jfsize;

    vtok_t tok;
    var_t val;

    int ins;
    str_t *bcode;
    vencode_t *encode;
};


// Parses V source code and evaluates the result
void vparse(vstate_t *);


#endif
