/* 
 * Variable types and definitions
 */

#ifdef MU_DEF
#ifndef MU_VAR_DEF
#define MU_VAR_DEF
#include "mu.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"


// readonly bit in lowest bit of tables
#define MU_RO 1

// Three bit type specifier located in lowest bits of each var
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


// Declaration of var type
typedef union var {
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
} var_t;


#endif
#else
#ifndef MU_VAR_H
#define MU_VAR_H
#define MU_DEF
#include "var.h"
#include "err.h"
#undef MU_DEF
#include "mem.h"


// Definitions of literal variables
#define vnil  ((var_t){0})
#define vinf  vnum(INFINITY)
#define vninf vnum(-INFINITY)

// Variable constructors
mu_inline var_t vnum(num_t num) {
    return (var_t){(~7 & *(uint_t *)&num) | MU_NUM};
}

mu_inline var_t vstr(str_t *str) {
    return (var_t){(~7 & (uint_t)str) | MU_STR};
}

mu_inline var_t vfn(fn_t *fn) {
    return (var_t){(~7 & (uint_t)fn) | MU_FN};
}

mu_inline var_t vtbl(tbl_t *tbl) {
    return (var_t){(~6 & (uint_t)tbl) | MU_TBL};
}

mu_inline var_t vobj(tbl_t *tbl) {
    return (var_t){(~6 & (uint_t)tbl) | MU_OBJ};
}


// type access
mu_inline enum mu_type gettype(var_t v) { return 7 & v.bits; }

// properties of variables
mu_inline bool isnil(var_t v) { return !v.bits; }
mu_inline bool isnum(var_t v) { return gettype(v) == MU_NUM; }
mu_inline bool isstr(var_t v) { return gettype(v) == MU_STR; }
mu_inline bool isfn(var_t v)  { return gettype(v) == MU_FN;  }
mu_inline bool istbl(var_t v) { return (6 & v.bits) == MU_TBL; }
mu_inline bool isobj(var_t v) { return 6 & ~v.bits; }
mu_inline bool isref(var_t v) { return 6 & v.bits; }
mu_inline bool isro(var_t v)  { return 1 & v.bits; }

// definitions for accessing components
mu_inline void  *getptr(var_t v) { return (void *)(~7 & v.bits); }
mu_inline ref_t *getref(var_t v) { return (ref_t *)getptr(v); }
mu_inline len_t  getlen(var_t v) { return *(len_t *)(getref(v) + 1); }

mu_inline num_t  getnum(var_t v) { 
    mu_assert(isnum(v)); 
    return ((var_t){~7 & v.bits}).num;
}

mu_inline str_t *getstr(var_t v) {
    mu_assert(isstr(v));
    return ((var_t){~7 & v.bits}).str; 
}

mu_inline fn_t  *getfn(var_t v)  { 
    mu_assert(isfn(v));
    return ((var_t){~7 & v.bits}).fn; 
}

mu_inline tbl_t *gettbl(var_t v) { 
    mu_assert(istbl(v));
    return ((var_t){~6 & v.bits}).tbl;
}


// Reference counting
extern void str_destroy(str_t *);
extern void fn_destroy(fn_t *);
extern void tbl_destroy(tbl_t *);

mu_inline void var_inc(var_t v) {
    if (isref(v))
        ref_inc(getref(v));
}

mu_inline void var_dec(var_t v) {
    static void (* const dtors[6])(ref_t *) = {
        (void*)str_destroy, (void*)fn_destroy,
        (void*)tbl_destroy, (void*)tbl_destroy, 
        0, 0
    };

    if (isref(v))
        ref_dec(getref(v), dtors[gettype(v)-2]);
}


// Returns true if both variables are the
// same type and equivalent.
bool var_equals(var_t a, var_t b);

// Returns a hash value of the given variable. 
hash_t var_hash(var_t v);

// Performs iteration on variables
fn_t *var_iter(var_t v, eh_t *eh);

// Returns a string representation of the variable
str_t *var_repr(var_t v, eh_t *eh);

// Table related functions performed on variables
var_t var_lookup(var_t v, var_t key, eh_t *eh);
var_t var_lookdn(var_t v, var_t key, hash_t i, eh_t *eh);
void var_assign(var_t v, var_t key, var_t val, eh_t *eh);
void var_insert(var_t v, var_t key, var_t val, eh_t *eh);
void var_append(var_t v, var_t val, eh_t *eh);

// Function calls performed on variables
var_t var_call(var_t v, tbl_t *args, eh_t *eh);


#endif
#endif
