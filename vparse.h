/*
 *  Parser for V
 */

#ifndef V_PARSE
#define V_PARSE

#include "var.h"
#include "vm.h"


// Parsing type definitions
typedef struct vstate vstate_t;
typedef uint8_t vtok_t;


// Representation of parsing product
enum vproduct {
    VP_NONE = 0,
    VP_REF  = 1,
    VP_VAL  = 2
};

// State of a parse
struct vstate {
    const str_t *pos;
    const str_t *end;
    ref_t *ref;

    uint8_t indirect;
    uint8_t paren;
    uint8_t nprec;

    vtok_t tok;
    var_t val;

    union {
        struct {
            uint8_t prec   : 8;
            vins_t opins : 24;
        };

        uint32_t opstate;
    };

    union {
        struct {
            vins_t lins;
            tbl_t *ltbl;
        };

        uint64_t lstate;
    };

    tbl_t *vars;
    tbl_t *keys;
    tbl_t *ops;

    vins_t ins;
    str_t *bcode;
    vins_t (*encode)(str_t *, vop_t, varg_t);
};


// Parses V source code and evaluates the result
vins_t vparse(struct vstate *);


#endif
