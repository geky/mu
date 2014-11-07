/*
 *  String Definition
 */

#ifdef MU_DEF
#ifndef MU_STR_DEF
#define MU_STR_DEF

#include "mu.h"


// Definition of Mu's string type 
// and Mu's mutable string type
typedef uint8_t mstr_t;
typedef const mstr_t str_t;


#endif
#else
#ifndef MU_STR_H
#define MU_STR_H
#define MU_DEF
#include "str.h"
#include "var.h"
#include "err.h"
#undef MU_DEF


// Functions for creating strings
mstr_t *str_create(len_t size, eh_t *eh);

// Called by garbage collector to clean up
void str_destroy(void *);

// Returns true if both variables are equal
bool str_equals(var_t a, var_t b);

// Returns a hash for each string
hash_t str_hash(var_t v);

// Parses a string and returns a string
var_t str_parse(const str_t **off, const str_t *end, eh_t *eh);

// Returns a string representation of a string
var_t str_repr(var_t v, eh_t *eh);


#endif
#endif
