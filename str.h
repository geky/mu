/*
 *  String Definition
 */

#ifdef MU_DEF
#ifndef MU_STR_DEF
#define MU_STR_DEF
#include "mu.h"


// Definition of single data entry in string type
typedef unsigned char data_t;

// Definition of Mu's string types
// Mutable strings can not be used in Mu itself
// but are useful in C for constructing Mu strings
typedef mu_aligned struct str mstr_t;
typedef const mu_aligned struct str str_t;


#endif
#else
#ifndef MU_STR_H
#define MU_STR_H
#include "types.h"


// Each string is simply stored as a length
// and dynamically determined array of data.
// Strings must be interned before use in tables
// using one of the str_create functions, after which 
// they cannot be modified without breaking things.
struct str {
    ref_t ref; // reference count
    len_t len; // string length

    data_t data[]; // string contents
};


// Functions for creating mutable temporary strings
mstr_t *mstr_create(len_t len, eh_t *eh);
void mstr_destroy(mstr_t *s);

// Function for interning strings
str_t *str_intern(str_t *s, eh_t *eh);
void str_destroy(str_t *s);

// String creating functions and macros
str_t *str_nstr(const data_t *s, len_t len, eh_t *eh);
str_t *str_cstr(const char *s, eh_t *eh);

mu_inline mu_t mnstr(const data_t *s, len_t l, eh_t *eh) {
    return mstr(str_nstr(s, l, eh));
}

mu_inline mu_t mcstr(const char *s, eh_t *eh) {
    return mstr(str_cstr(s, eh));
}

// String accessing macros
mu_inline len_t str_getlen(str_t *s) { return s->len; }
mu_inline const data_t *str_getdata(str_t *s) { return s->data; }

mu_inline const data_t *getdata(mu_t m) { return str_getdata(getstr(m)); }

// Reference counting
mu_inline void str_inc(str_t *s) { ref_inc((void *)s); }
mu_inline void str_dec(str_t *s) { ref_dec((void *)s, 
                                           (void (*)(void *))str_destroy); }


// Hashing and equality for non-interned strings
bool mstr_equals(str_t *a, str_t *b);
hash_t mstr_hash(str_t *s);

// String parsing and representation
str_t *str_parse(const data_t **off, const data_t *end, eh_t *eh);
str_t *str_repr(str_t *s, eh_t *eh);


#endif
#endif
