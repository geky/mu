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
    VT_LIT      = 2,
    VT_SET      = 3,
    VT_KEY      = 4,
    VT_OP       = 5,
    VT_OPSET    = 6,
    VT_IDENT    = 7,
    VT_IDSET    = 8,
    VT_FN       = 9,
    VT_FNSET    = 10,
    VT_NIL      = 11,
    VT_LET      = 12,
    VT_RETURN   = 13,
    VT_IF       = 14,
    VT_WHILE    = 15,
    VT_FOR      = 16,
    VT_CONT     = 17,
    VT_BREAK    = 18,
    VT_ELSE     = 19,
    VT_AND      = 20,
    VT_OR       = 21
};


// Creates internal tables for keywords or uses prexisting.
__attribute__((const))
tbl_t *vkeys(void);

// Performs lexical analysis on current location in string
// Updates position, stores token type in tok, and value in val
void vlex(vstate_t *);


#endif
