/*
 *  String Definition
 */

#ifndef V_STR
#define V_STR

#include "var.h"

// Functions for creating strings
str_t *str_create(len_t size);

// Called by garbage collector to clean up
void str_destroy(void *);

// Returns true if both variables are equal
bool str_equals(var_t a, var_t b);

// Returns a hash for each string
hash_t str_hash(var_t v);

// Parses a string and returns a string
var_t str_parse(const str_t **off, const str_t *end);

// Returns a string representation of a string
var_t str_repr(var_t v);


#endif
