/*
 *  Lexer for V
 *  Does not use Flex to take full advantage 
 *  of V's string type 
 */

#ifndef V_LEX
#define V_LEX

#include "var.h"


struct v_lex_state {
    str_t *off;
    str_t *end;
    ref_t *ref;
};

// Error handling
void yyerror(struct v_lex_state *ls, const char *s);

// Performs lexical analysis on the passed string
// Value is stored in lval and its type is returned
int yylex(var_t *lval, struct v_lex_state *ls);


// Lookup table of lex functions based 
// only on first character of token
extern int (* const yylex_a[256])(var_t *lval, struct v_lex_state *);


#endif
