/* 
 * Variable types and definitions
 */

#ifdef MU_DEF
#ifndef MU_TYPES_DEF
#define MU_TYPES_DEF
#include "mu.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"


// readonly bit in lowest bit of tables
#define MU_RO 1

// Three bit type specifier located in lowest bits of each variable
// 3b00x indicates type is not reference counted
enum mu_type {
    MU_NIL = 0, // nil
    MU_NUM = 1, // number
    MU_STR = 2, // string
    MU_FN  = 3, // function
    MU_TBL = 4, // table
    MU_OBJ = 6, // object

    MU_ROTBL = MU_RO | MU_TBL, // readonly table
    MU_ROOBJ = MU_RO | MU_OBJ, // readonly object
};

// Declaration of mu type
typedef union mu {
    // bitwise representations
    uint_t bits;

    // number representation
    num_t num;

    // string representation
    str_t *str;

    // table representation
    tbl_t *tbl;

    // function representation
    fn_t *fn;
} mu_t;


#endif
#else
#ifndef MU_TYPES_H
#define MU_TYPES_H
#define MU_DEF
#include "types.h"
#undef MU_DEF
#include "mem.h"
#include <string.h> // TODO rm?
#include <math.h>


// Definitions of literal variables
#define mnil  ((mu_t){0})
#define minf  mnum(INFINITY)
#define mninf mnum(-INFINITY)

// Variable constructors
mu_inline mu_t mnum(num_t num) {
    union { num_t n; uint_t u; } u = { num };
    return (mu_t){(~7 & u.u) | MU_NUM};
}

mu_inline mu_t mstr(str_t *str) {
    return (mu_t){(~7 & (uint_t)str) | MU_STR};
}

mu_inline mu_t mfn(fn_t *fn) {
    return (mu_t){(~7 & (uint_t)fn) | MU_FN};
}

mu_inline mu_t mtbl(tbl_t *tbl) {
    return (mu_t){(~6 & (uint_t)tbl) | MU_TBL};
}

mu_inline mu_t mobj(tbl_t *tbl) {
    return (mu_t){(~6 & (uint_t)tbl) | MU_OBJ};
}


// type access
mu_inline enum mu_type gettype(mu_t m) { return 7 & m.bits; }

// properties of variables
mu_inline bool isnil(mu_t m) { return !m.bits; }
mu_inline bool isnum(mu_t m) { return gettype(m) == MU_NUM; }
mu_inline bool isstr(mu_t m) { return gettype(m) == MU_STR; }
mu_inline bool isfn(mu_t m)  { return gettype(m) == MU_FN;  }
mu_inline bool istbl(mu_t m) { return (6 & m.bits) == MU_TBL; }
mu_inline bool isobj(mu_t m) { return 6 & ~m.bits; }
mu_inline bool isref(mu_t m) { return 6 & m.bits; }
mu_inline bool isro(mu_t m)  { return 1 & m.bits; }

// definitions for accessing components
mu_inline void  *getptr(mu_t m) { return (void *)(~7 & m.bits); }
mu_inline ref_t *getref(mu_t m) { return (ref_t *)getptr(m); }
mu_inline len_t  getlen(mu_t m) { return *(len_t *)(getref(m) + 1); }

mu_inline num_t  getnum(mu_t m) { 
    mu_assert(isnum(m)); 
    return ((mu_t){~7 & m.bits}).num;
}

mu_inline str_t *getstr(mu_t m) {
    mu_assert(isstr(m));
    return ((mu_t){~7 & m.bits}).str; 
}

mu_inline fn_t  *getfn(mu_t m)  { 
    mu_assert(isfn(m));
    return ((mu_t){~7 & m.bits}).fn; 
}

mu_inline tbl_t *gettbl(mu_t m) { 
    mu_assert(istbl(m));
    return ((mu_t){~6 & m.bits}).tbl;
}


// Reference counting
extern void str_destroy(str_t *);
extern void fn_destroy(fn_t *);
extern void tbl_destroy(tbl_t *);

mu_inline mu_t mu_inc(mu_t m) {
    if (isref(m))
        ref_inc(getref(m));

    return m;
}

mu_inline void mu_dec(mu_t m) {
    static void (* const dtors[6])(void *) = {
        (void (*)(void *))str_destroy, (void (*)(void *))fn_destroy,
        (void (*)(void *))tbl_destroy, (void (*)(void *))tbl_destroy, 
        0, 0
    };

    if (isref(m))
        ref_dec(getref(m), dtors[gettype(m)-2]);
}


// Returns true if both variables are the
// same type and equivalent.
bool mu_equals(mu_t a, mu_t b);

// Returns a hash value of the given variable. 
hash_t mu_hash(mu_t m);

// Performs iteration on variables
fn_t *mu_iter(mu_t m);

// Returns a string representation of the variable
str_t *mu_repr(mu_t m);

// Table related functions performed on variables
mu_t mu_lookup(mu_t m, mu_t key);
mu_t mu_lookdn(mu_t m, mu_t key, hash_t i);
void mu_assign(mu_t m, mu_t key, mu_t val);
void mu_insert(mu_t m, mu_t key, mu_t val);

// Function calls performed on variables
void mu_fcall(mu_t m, frame_t c, mu_t *frame);
mu_t mu_call(mu_t m, frame_t c, ...);


#endif
#endif
