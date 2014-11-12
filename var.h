
/* 
 *  Variable types and definitions
 */

#ifdef MU_DEF
#ifndef MU_VAR_DEF
#define MU_VAR_DEF

#include "mu.h"
#include "mem.h"
#include "num.h"
#include "str.h"


// 32 bit hash size for all types
typedef uint32_t hash_t;

// Common length for all structures
typedef uint16_t len_t;

#define MU_MAXLEN UINT16_MAX


// Three bit type specifier located in lowest bits of each var
// 3b1xx indicates reference counted
// 3bx11 indicates additional table attached
enum type {
    MU_NIL = 0, // nil
    MU_NUM = 1, // number
    MU_BFN = 2, // builtin function
    MU_SFN = 3, // builtin function with scope
    MU_TBL = 4, // table
    MU_OBJ = 5, // wrapped table
    MU_STR = 6, // string
    MU_FN  = 7, // function
};

#define MU_ERR 8 // error


// declaration of var type
typedef struct var {
    union {
        // bitwise representations
        uint64_t bits;
        uint8_t  bytes[8];

        struct {
            union {
                // metadata for all vars
                uint32_t meta;

                // type specifier is encoded in lower 3 bits
                enum type type : 3;

                // reference counting is performed here for half of vars
                ref_t *ref;

                // string encoding
                str_t *str;

                // function encoding
                struct fn *fn;
            };

            union {
                // data for vars
                uint32_t data;

                // string offset and length
                struct {
                    len_t len;
                    len_t off;
                };

                // table encoding
                struct tbl *tbl;
            };
        };

        // number encoding
        num_t num;
    };
} var_t;


#endif
#else
#ifndef MU_VAR_H
#define MU_VAR_H
#define MU_DEF
#include "var.h"
#include "tbl.h"
#include "fn.h"
#include "err.h"
#undef MU_DEF

#include "mem.h"


// definitions for accessing components
mu_inline enum type type(var_t v)  { return v.type; }

mu_inline void *getptr(var_t v)    { return v.ref; }
mu_inline uint32_t getraw(var_t v) { return v.data; }
mu_inline ref_t *getref(var_t v)   { return (ref_t *)(v.meta & ~7); }
mu_inline num_t getnum(var_t v)    { v.meta &= ~7; return v.num; }
mu_inline str_t *getstart(var_t v) { return v.str; }
mu_inline str_t *getstr(var_t v)   { return v.str + v.off; }
mu_inline str_t *getend(var_t v)   { return v.str + v.off + v.len; }
mu_inline len_t getoff(var_t v)    { return v.off; }
mu_inline len_t getlen(var_t v)    { return v.len; }
mu_inline tbl_t *gettbl(var_t v)   { return v.tbl; }
mu_inline fn_t *getfn(var_t v)     { return (fn_t *)(v.meta & ~3); }
mu_inline bfn_t *getbfn(var_t v)   { return (bfn_t *)getfn(v); }
mu_inline sfn_t *getsfn(var_t v)   { return (sfn_t *)getfn(v); }


// properties of variables
mu_inline bool isnil(var_t v) { return !v.meta; }
mu_inline bool isnum(var_t v) { return type(v) == MU_NUM; }
mu_inline bool isstr(var_t v) { return type(v) == MU_STR; }
mu_inline bool istbl(var_t v) { return (6 & v.meta) == 4; }
mu_inline bool isobj(var_t v) { return type(v) == MU_OBJ; }
mu_inline bool isfn(var_t v)  { return (6 & v.meta) == 2 || type(v) == MU_FN; }
mu_inline bool iserr(var_t v) { return mu_unlikely(v.meta == MU_ERR); }

mu_inline bool hasref(var_t v)   { return 4 & v.meta; }
mu_inline bool hasscope(var_t v) { return !(3 & ~v.meta); }


// definitions of literal vars in C
#define vnil  ((var_t){{0}})
#define vnan  vnum(NAN)
#define vinf  vnum(INFINITY)
#define vninf vnum(-INFINITY)

// var constructors for C
mu_inline var_t vraw(uint32_t raw) {
    var_t v;
    v.data = raw;
    v.type = MU_NUM;
    return v;
}

mu_inline var_t vnum(num_t num) {
    var_t v;
    v.num = num;
    v.type = MU_NUM;
    return v;
}

mu_inline var_t vstr(str_t *str, len_t off, len_t len) {
    var_t v;
    v.str = str;
    v.off = off;
    v.len = len;
    v.type = MU_STR;
    return v;
}

mu_inline var_t vtbl(tbl_t *tbl) {
    var_t v;
    v.ref = (ref_t*)tbl;
    v.tbl = tbl;
    v.type = MU_TBL;
    return v;
}

mu_inline var_t vobj(tbl_t *tbl) {
    var_t v;
    v.ref = (ref_t*)tbl;
    v.tbl = tbl;
    v.type = MU_OBJ;
    return v;
}

mu_inline var_t vfn(fn_t *fn, tbl_t *scope) {
    var_t v;
    v.fn = fn;
    v.tbl = scope;
    v.type = MU_FN;
    return v;
}

mu_inline var_t vbfn(bfn_t *bfn) {
    var_t v;
    v.fn = (fn_t *)bfn;
    v.type = MU_BFN;
    return v;
}

mu_inline var_t vsfn(sfn_t *sfn, tbl_t *scope) {
    var_t v;
    v.fn = (fn_t *)sfn;
    v.tbl = scope;
    v.type = MU_SFN;
    return v;
}

mu_inline var_t verr(tbl_t *err) {
    var_t v;
    v.tbl = err;
    v.meta = MU_ERR;
    return v;
}

#define vcstr(c) ({                         \
    static const struct {                   \
        ref_t r;                            \
        len_t l;                            \
        str_t s[sizeof(c)-1];               \
    } _vcstr = { 0, sizeof(c)-1, {(c)}};    \
                                            \
    vstr(_vcstr.s, 0, sizeof(c)-1);         \
})


// Mapping of reference counting functions
extern void tbl_destroy(void *);
extern void str_destroy(void *);
extern void fn_destroy(void *);

mu_inline void var_inc(var_t v) {
    if (hasref(v))
        ref_inc(getptr(v));
    if (hasscope(v))
        ref_inc(gettbl(v));
}

mu_inline void var_dec(var_t v) {
    static void (* const dtors[4])(void *) = {
        tbl_destroy, 0, str_destroy, fn_destroy
    };

    if (hasref(v))
        ref_dec(getptr(v), dtors[3 & v.meta]);
    if (hasscope(v))
        ref_dec(gettbl(v), tbl_destroy);
}


// Returns true if both variables are the
// same type and equivalent.
bool var_equals(var_t a, var_t b);

// Returns a hash value of the given variable. 
hash_t var_hash(var_t var);

// Performs iteration on variables
var_t var_iter(var_t v, eh_t *eh);

// Returns a string representation of the variable
var_t var_repr(var_t v, eh_t *eh);

// Prints variable to stdout for debugging
void var_print(var_t v, eh_t *eh);

// Table related functions performed on variables
var_t var_lookup(var_t v, var_t key, eh_t *eh);
var_t var_lookdn(var_t v, var_t key, len_t i, eh_t *eh);
void var_assign(var_t v, var_t key, var_t val, eh_t *eh);
void var_insert(var_t v, var_t key, var_t val, eh_t *eh);
void var_append(var_t v, var_t val, eh_t *eh);

// Function calls performed on variables
var_t var_call(var_t v, tbl_t *args, eh_t *eh);
var_t var_pcall(var_t v, tbl_t *args);


#endif
#endif
