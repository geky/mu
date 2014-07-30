/*
 *  Lexer for V
 *  Does not use Flex to take full advantage 
 *  of V's string type 
 */

#ifndef V_LEX
#define V_LEX

#include "var.h"
#include "vparse.h"


// Token representation in V
// single character tokens are also used
// so enum spans lower range of ascii
enum vtok {
    VT_END      = 0,
    VT_SEP      = 1,
    VT_NUM      = 2,
    VT_STR      = 3,
    VT_OP       = 4,
    VT_OPSET    = 5,
    VT_SET      = 6,
    VT_DOT      = 7,
    VT_IDENT    = 8,
    VT_NIL      = 9,
    VT_FN       = 10,
    VT_LET      = 11,
    VT_RETURN   = 12,
    VT_IF       = 13,
    VT_WHILE    = 14,
    VT_CONT     = 15,
    VT_BREAK    = 16,
    VT_ELSE     = 17
};


// Creates internal tables for keywords or uses prexisting.
// Use this to initialize an op table if nescessary.
tbl_t *vkeys(void);
tbl_t *vops(void);


// Performs lexical analysis on the passed string
// Value is stored in val and a token's type is returned
vtok_t vlex(struct vstate *);


// Lookup table of lex functions based 
// only on a single character of token
extern vtok_t (* const vlex_a[256])(struct vstate *);


#endif
