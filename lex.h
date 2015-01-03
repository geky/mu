/*
 *  Lexer for Mu
 */

#ifdef MU_DEF
#ifndef MU_LEX_DEF
#define MU_LEX_DEF

#include "mu.h"


// Token representation in Mu
// single character tokens are also used
// so enum spans lower range of ascii
typedef enum tok {
    T_END      = 0,

    T_SEP      = 1,
    T_LIT      = 2,
    T_SET      = 3,
    T_KEY      = 4,
    T_OP       = 5,
    T_OPSET    = 6,
    T_IDENT    = 7,
    T_IDSET    = 8,
    T_FN       = 9,
    T_FNSET    = 10,

    T_NIL      = 11,
    T_LET      = 12,
    T_RETURN   = 13,
    T_IF       = 14,
    T_WHILE    = 15,
    T_FOR      = 16,
    T_CONT     = 17,
    T_BREAK    = 18,
    T_ELSE     = 19,
    T_AND      = 20,
    T_OR       = 21,

    T_LPAREN   = '(',
    T_RPAREN   = ')',
    T_LBRACE   = '{',
    T_RBRACE   = '}',
    T_LBRACKET = '[',
    T_RBRACKET = ']',
    T_SLASH    = '\\',
} tok_t;


#endif
#else
#ifndef MU_LEX_H
#define MU_LEX_H
#define MU_DEF
#include "lex.h"
#include "types.h"
#include "parse.h"
#undef MU_DEF


// Creates internal tables for keywords or uses prexisting.
mu_const tbl_t *mu_keys(void);

// Performs lexical analysis on current location in string
// Updates position, stores token type in tok, and value in val
void mu_lex(parse_t *);


#endif
#endif
