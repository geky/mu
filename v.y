%{

#include "var.h"
#include "vlex.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define YYSTYPE var_t

%}


%define api.pure
%parse-param {struct v_lex_state *ls}
%lex-param   {struct v_lex_state *ls}

%token FN RETURN
%token IDENT
%token NUM STR
%token OP LOP ROP
%token DOT ASSIGN SET AND OR
%token SPACE
%token TERM

%start expression


%%


expression  : expression TERM statement
            | expression TERM
            | statement
            | /* empty */
            ;

statement   : '{' expression '}'
            | process
            ;

process     : value
            ;

value       : primary
            ;

primary     : IDENT                 { printf("found identifier\n"); }
            | literal
            | '(' value ')'
            ;

literal     : NUM                   { printf("found number: "); var_print($1); printf("\n"); }
            | STR                   { printf("found string: "); var_print($1); printf("\n"); }
            | table                 { printf("found table\n");    }
            | function              { printf("found function\n"); }
            ;

table       : '[' expression ']'
            ;

function    : FN '(' arguments ')' statement
            ;

arguments   : argumentsl
            | /* empty */
            ;

argumentsl  : argumentsl TERM IDENT
            | IDENT
            ;


%%

