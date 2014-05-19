/*
 *  String Definition
 */

#ifndef V_STR
#define V_STR

#include "var.h"

// Returns true if both variables are equal
bool str_equals(var_t a, var_t b);

// Returns a hash for each number
// For integers this is the number
hash_t str_hash(var_t v);

// Parses a string and returns a string
var_t str_parse(str_t **off, str_t *end);

// Returns a string representation of a string
var_t str_repr(var_t v);


#endif
