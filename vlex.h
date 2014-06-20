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
    VT_FN       = 1,
    VT_RETURN   = 2,
    VT_IDENT    = 3,
    VT_NUM      = 4,
    VT_STR      = 5,
    VT_OP       = 6,
    VT_DOT      = 7,
    VT_SET      = 8,
    VT_AND      = 9,
    VT_OR       = 10,
    VT_LET      = 11,
    VT_SEP      = 12
};

// Performs lexical analysis on the passed string
// Value is stored in val and a token's type is returned
int vlex(struct vstate *);


// Lookup table of lex functions based 
// only on first character of token
extern int (* const vlex_a[256])(struct vstate *);


#endif
