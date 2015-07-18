/*
 *  Lexer for Mu
 */

#ifndef MU_LEX_H
#define MU_LEX_H
#include "mu.h"
#include "types.h"


// Mu tokens
mu_packed enum tok {
    T_END, 
    T_TERM, T_SEP,
    T_ASSIGN, T_PAIR,

    T_AND, T_OR, T_ELSE,

    T_LBLOCK, T_LET,
    T_DOT, T_ARROW,
    T_CONT, T_BREAK, T_RETURN,

    T_LPAREN, T_LTABLE,
    T_FN, T_TYPE,
    T_IF, T_WHILE, T_FOR,

    T_NIL, T_IMM, T_SYM, T_KEY,
    T_OP, T_OPASSIGN, T_EXPAND,
    T_RPAREN, T_RTABLE, T_RBLOCK
};

mu_inline bool mu_isstmt(enum tok tok) {
    return tok >= T_LBLOCK && tok < T_RPAREN;
}

mu_inline bool mu_isexpr(enum tok tok) {
    return tok >= T_LPAREN && tok < T_RPAREN;
}


// State of lexical analysis
struct lex {
    const byte_t *pos;
    const byte_t *end;

    mu_t val;
    enum tok tok;

    uintq_t indent;
    len_t line;

    uintq_t lprec;
    uintq_t rprec;
    intq_t nblock;
    uintq_t block;
    intq_t nparen;
    uintq_t paren;
};


// Keywords and symbols
mu_const mu_t mu_keys(void);
mu_const mu_t mu_syms(void);

// Initialize lexing based on pos/end in lex struct
void mu_lex_init(struct lex *);

// Performs lexical analysis on current location in string
// Updates position, stores token type in tok, and value in val
void mu_lex(struct lex *);

// Performs lexical analysis on current location in string
// Updates position, stores token type in tok, 
// but does not create and values
void mu_scan(struct lex *);


#endif
