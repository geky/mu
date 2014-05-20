/*
 *  Lexer for V
 *  Does not use Flex to take full advantage 
 *  of V's string type 
 */

#ifndef V_LEX
#define V_LEX

#include "var.h"


struct vlex {
    str_t *off;
    str_t *end;
    ref_t *ref;
};

// Error handling
void verror(struct vlex *ls, const char *s);

// Performs lexical analysis on the passed string
// Value is stored in lval and its type is returned
int vlex(var_t *lval, struct vlex *ls);


// Lookup table of lex functions based 
// only on first character of token
extern int (* const vlex_a[256])(var_t *lval, struct vlex *);


#endif
