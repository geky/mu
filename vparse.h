/*
 *  Parser for V
 */

#ifndef V_PARSE
#define V_PARSE

#include "var.h"
#include "vm.h"



// State of a parse
struct vstate {
    str_t *off;
    str_t *end;
    ref_t *ref;

    int tok;
    var_t val;

    int ins;
    uint8_t *bcode;
    tbl_t *vars;
    int (*encode)(uint8_t *, enum vop, uint16_t);

    union {
        struct {
            uint8_t op     : 8;
            uint8_t paren  : 8;
        };

        uint32_t state;
    };
};


// Parses V source code and evaluates the result
int vparse(struct vstate *);


#endif
