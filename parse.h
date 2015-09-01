/*
 *  Parser for Mu
 */

#ifndef MU_PARSE_H
#define MU_PARSE_H
#include "mu.h"
#include "fn.h"


// Parse literals without side-effects
mu_t mu_parse(mu_t m);
mu_t mu_nparse(const mbyte_t **pos, const mbyte_t *end);

// Compile Mu source code into code objects
struct code *mu_compile(mu_t s);
struct code *mu_ncompile(const mbyte_t *pos, const mbyte_t *end);


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
