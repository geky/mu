/*
 *  Parser for V
 */

#ifndef V_PARSE
#define V_PARSE

#include "var.h"
#include "vm.h"


// Parsing type definitions
typedef uint8_t vtok_t;
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
    uint8_t lprec;
    uint8_t rprec;
};

struct vjstate {
    tbl_t *ctbl;
    tbl_t *btbl;
};

struct vfnstate {
    str_t *bcode;

    len_t stack;
    len_t len;
    len_t ins;

    tbl_t *fns;
    tbl_t *vars;
};

// State of a parse
struct vstate {
    struct vfnstate *fn;
    struct vjstate j;
    struct vopstate op;
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

    vtok_t tok;
    var_t val;

    ref_t *ref;
    const str_t *str;
    const str_t *pos;
    const str_t *end;
};


// Parses V source into bytecode
void vparse_init(vstate_t *vs, var_t code);
void vparse_args(vstate_t *vs, tbl_t *args);
void vparse_top(vstate_t *vs);
void vparse_nested(vstate_t *vs);

#endif
