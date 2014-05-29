/*
 *  Lexer for V
 *  Does not use Flex to take full advantage 
 *  of V's string type 
 */

#ifndef V_LEX
#define V_LEX

#include "var.h"
#include "vparse.h"


// Performs lexical analysis on the passed string
// Value is stored in val and a token's type is returned
int vlex(struct vstate *);


// Lookup table of lex functions based 
// only on first character of token
extern int (* const vlex_a[256])(struct vstate *);


#endif
