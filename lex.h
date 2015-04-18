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

    T_IMM      = 1,
    T_KEY2     = 2,
    T_OP       = 3,
    T_OPSET    = 4,
    T_SYM      = 5,
    T_KEY      = 6,
    T_FN       = 7,
    T_FNSET    = 8,

    T_LET      = 9,
    T_IF       = 10,
    T_ELSE     = 11,
    T_WHILE    = 12,
    T_FOR      = 13,
    T_CONT     = 14,
    T_BREAK    = 15,
    T_RETURN   = 16,

    T_ARROW    = 17,
    T_AND      = 18,
    T_OR       = 19,

    T_NIL      = 20,
    T_ARGS     = 21,
    T_SCOPE    = 22,

    T_DOT      = '.',
    T_REST     = '*',
    T_TERM     = ';',
    T_COMMA    = ',',
    T_ASSIGN   = '=',
    T_COLON    = ':',

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
#include "parse.h"
#include "types.h"
#undef MU_DEF


// Creates internal tables for keywords or uses prexisting.
mu_const tbl_t *mu_keys(void);

// Performs lexical analysis on current location in string
// Updates position, stores token type in tok, and value in val
void mu_lex(parse_t *);


#endif
#endif
