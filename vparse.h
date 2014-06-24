/*
 *  Parser for V
 */

#ifndef V_PARSE
#define V_PARSE

#include "var.h"
#include "vm.h"

// Representation of parsing product
enum vproduct {
    VP_NONE = 0,
    VP_REF  = 1,
    VP_VAL  = 2
};

// State of a parse
struct vstate {
    str_t *pos;
    str_t *end;
    ref_t *ref;

    bool indirect : 1;
    uint8_t paren : 8;
    uint8_t prec  : 8;
    uint8_t nprec : 8;

    int tok;
    var_t val;

    tbl_t *vars;
    tbl_t *ops;

    int opins;
    int ins;
    uint8_t *bcode;
    int (*encode)(uint8_t *, enum vop, uint16_t);
};


// Parses V source code and evaluates the result
int vparse(struct vstate *);


#endif
