/*
 *  Parser for Mu
 */

#ifndef MU_PARSE_H
#define MU_PARSE_H
#include "mu.h"
#include "types.h"
#include "fn.h"


// Parse literals without side-effects
mu_t parse_num(mu_t s);
mu_t parse_str(mu_t s);

// Parse Mu code objects
struct code *parse_expr(mu_t s);
struct code *parse_fn(mu_t s);
struct code *parse_module(mu_t s);


// Conversion to/from ascii
mu_inline muint_t mu_fromascii(mbyte_t c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else
        return -1;
}

mu_inline mbyte_t mu_toascii(muint_t c) {
    if (c < 10)
        return '0' + c;
    else
        return 'a' + (c-10);
}


#endif
