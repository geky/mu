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
    T_FN       = 6,
    T_FNSET    = 7,

    T_LET      = 8,
    T_IF       = 9,
    T_ELSE     = 10,
    T_WHILE    = 11,
    T_FOR      = 12,
    T_CONT     = 13,
    T_BREAK    = 14,
    T_RETURN   = 15,

    T_ARROW    = 16,
    T_AND      = 17,
    T_OR       = 18,

    T_NIL      = 19,
    T_ARGS     = 20,
    T_SCOPE    = 21,

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


typedef struct lex lex_t;


#endif
#else
#ifndef MU_LEX_H
#define MU_LEX_H
#define MU_DEF
#include "lex.h"
#include "types.h"
#undef MU_DEF

// State of lexical analysis
struct lex {
    const data_t *pos;
    const data_t *end;

    mu_t val;
    tok_t tok;

    uintq_t lprec;
    uintq_t rprec;
    uintq_t indent;
    uintq_t paren;
    uintq_t lookahead : 1;
};


// Creates internal tables for keywords or uses prexisting.
mu_const tbl_t *mu_keys(void);

// Performs lexical analysis on current location in string
// Updates position, stores token type in tok, and value in val
void mu_lex(lex_t *);


#endif
#endif
