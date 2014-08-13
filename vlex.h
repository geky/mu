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
    VT_FOR      = 15,
    VT_CONT     = 16,
    VT_BREAK    = 17,
    VT_ELSE     = 18
};


// Creates internal tables for keywords or uses prexisting.
// Use this to initialize an op table if nescessary.
__attribute__((const)) tbl_t *vkeys(void);
__attribute__((const)) tbl_t *vops(void);


// Performs lexical analysis on current location in string
// Updates position, stores token type in tok, and value in val
void vlex(vstate_t *);


#endif
