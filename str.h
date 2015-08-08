/*
 *  String Definition
 */

#ifndef MU_STR_H
#define MU_STR_H
#include "mu.h"
#include "types.h"
#include <string.h>


// Definition of Mu's string types
// Each string is stored as a length and array of data.
//
// Strings must be interned before use in tables, and once interned, 
// strings cannot be mutated without breaking things.
//
// Temporary mutable strings can be created and used through the 
// mstr set of functions which store this info internally
mu_aligned struct str {
    ref_t ref;     // reference count
    len_t len;     // length of string
    byte_t data[]; // string data
};

// String creation functions
mu_t mnstr(const byte_t *s, uint_t len);
mu_t mcstr(const char *s);


// String access functions
mu_inline len_t str_len(mu_t m) {
    return ((struct str *)((uint_t)m - MU_STR))->len;
}

mu_inline const byte_t *str_bytes(mu_t m) {
    return ((struct str *)((uint_t)m - MU_STR))->data;
}


// Functions for handling temporary mutable strings
byte_t *mstr_create(uint_t len);
mu_t mstr_intern(byte_t *s, uint_t len);

void mstr_insert(byte_t **s, uint_t *i, byte_t c);
void mstr_concat(byte_t **s, uint_t *i, mu_t c);
void mstr_nconcat(byte_t **s, uint_t *i, const byte_t *c, uint_t len);
void mstr_cconcat(byte_t **s, uint_t *i, const char *c);


// Reference counting
mu_inline mu_t str_inc(mu_t m) { ref_inc(m); return m; }
mu_inline void str_dec(mu_t m) {
    mu_assert(mu_isstr(m));
    extern void str_destroy(mu_t);
    if (ref_dec(m)) str_destroy(m); 
}

mu_inline byte_t *mstr_inc(byte_t *s) { ref_inc(s); return s; }
mu_inline void mstr_dec(byte_t *s) {
    extern void mstr_destroy(byte_t *);
    if (ref_dec(s)) mstr_destroy(s); 
}


// String iteration
mu_t str_iter(mu_t s);
bool str_next(mu_t s, uint_t *i, mu_t *c);

// String representation
mu_t str_repr(mu_t s);

#endif
