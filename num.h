/*
 *  Number Definition
 */

#ifndef V_NUM
#define V_NUM

#include "var.h"


// Returns true if both variables are equal
bool num_equals(var_t a, var_t b);

// Returns a hash for each number
// For integers this is the number
hash_t num_hash(var_t v);

// Parses a string and returns a number
var_t num_parse(str_t **off, str_t *end);

// Returns a string representation of a number
var_t num_repr(var_t v);


// lookup table for ascii numerical values
extern const unsigned char num_a[256];


#endif
