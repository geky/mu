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

    uint8_t indirect;
    uint8_t paren;
    uint8_t nprec;

    uint8_t tok;
    var_t val;

    union {
        struct {
            uint8_t prec   : 8;
            uint32_t opins : 24;
        };

        uint32_t opstate;
    };

    union {
        struct {
            int lins;
            tbl_t *ltbl;
        };

        uint64_t lstate;
    };

    tbl_t *vars;
    tbl_t *keys;
    tbl_t *ops;

    int ins;
    uint8_t *bcode;
    int (*encode)(uint8_t *, enum vop, uint16_t);
};


// Parses V source code and evaluates the result
int vparse(struct vstate *);


#endif
