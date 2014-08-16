/*
 *  Number Definition
 */

#ifndef V_NUM
#define V_NUM

#include "var.h"

// Max length of a string representation
#define VNUMLEN 14


// Returns true if both variables are equal
bool num_equals(var_t a, var_t b);

// Returns a hash for each number
// For integers this is the number
hash_t num_hash(var_t v);

// Parses a string and returns a number
var_t num_parse(const str_t **off, const str_t *end);

// Obtains a string representation of a number
var_t num_repr(var_t v);


// Checks to see if a number is equivalent to its hash
static inline bool num_ishash(var_t v, hash_t hash) {
    return var_isnum(v) && num_equals(v, vnum(hash));
}

// Obtains ascii value
static inline int num_val(str_t s) {
    if (s >= '0' && s <= '9')
        return s - '0';
    else if (s >= 'a' && s <= 'f')
        return s - 'a' + 10;
    else if (s >= 'A' && s <= 'F')
        return s - 'A' + 10;
    else
        return 0xff;
}

static inline str_t num_ascii(int i) {
    if (i < 10)
        return '0' + i;
    else
        return 'a' + (i-10);
}

#endif
