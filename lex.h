/*
 *  Lexer for Mu
 *  Does not use Flex to take full advantage 
 *  of Mu's string type 
 */

#ifndef MU_LEX
#define MU_LEX

#include "var.h"
#include "parse.h"


// Token representation in Mu
// single character tokens are also used
// so enum spans lower range of ascii
enum mutok {
    MT_END      = 0,
    MT_SEP      = 1,
    MT_LIT      = 2,
    MT_SET      = 3,
    MT_KEY      = 4,
    MT_OP       = 5,
    MT_OPSET    = 6,
    MT_IDENT    = 7,
    MT_IDSET    = 8,
    MT_FN       = 9,
    MT_FNSET    = 10,
    MT_NIL      = 11,
    MT_LET      = 12,
    MT_RETURN   = 13,
    MT_IF       = 14,
    MT_WHILE    = 15,
    MT_FOR      = 16,
    MT_CONT     = 17,
    MT_BREAK    = 18,
    MT_ELSE     = 19,
    MT_AND      = 20,
    MT_OR       = 21
};


// Creates internal tables for keywords or uses prexisting.
__attribute__((const))
tbl_t *mu_keys(void);

// Performs lexical analysis on current location in string
// Updates position, stores token type in tok, and value in val
void mu_lex(mstate_t *);


#endif
