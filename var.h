/* 
 *  Variable types and definitions
 */

#ifndef MU_VAR_M
#define MU_VAR_M

#include "mem.h"

#include <stdint.h>
#include <stdbool.h>
#include <math.h>


// Three bit type specifier located in lowest bits of each var
// 3b1xx indicates reference counted
// 3bx11 indicates additional table attached
typedef enum type {
    MU_NIL = 0x0, // nil
    MU_NUM = 0x1, // number
    MU_BFN = 0x2, // builtin function
    MU_SFN = 0x3, // builtin function with scope
    MU_TBL = 0x4, // table
    MU_OBJ = 0x5, // wrapped table
    MU_STR = 0x6, // string
    MU_FN  = 0x7, // function
} type_t;


// All vars hash to 32 bits 
typedef uint32_t hash_t;

// Length type bound to 16 bits for space consumption
#define MU_MAXLEN 0xffff
typedef uint16_t len_t;

// Base types
typedef double num_t;
typedef uint8_t str_t;

// Base structs
typedef struct tbl tbl_t;
typedef struct fn fn_t;

// Base functions
#define mu_fn __attribute__((aligned(8)))
typedef mu_fn struct var bfn_t(tbl_t *a, eh_t *eh);
typedef mu_fn struct var sfn_t(tbl_t *a, tbl_t *s, eh_t *eh);


// Actual var type declariation
typedef struct var {
    union {
        // bitwise representations
        uint64_t bits;
        uint8_t  bytes[8];

        // packed metadata for all vars
        struct {
            union {
                // metadata for all vars
                uint32_t meta;
                type_t type : 3;
                ref_t *ref;

                // string representation
                const str_t *str;

                // function representations
                fn_t *fn;
                bfn_t *bfn;
                sfn_t *sfn;
            };

            union {
                // data for vars
                uint32_t data;

                // string offset and length
                struct {
                    len_t off;
                    len_t len;
                };

                // pointer to table representation
                tbl_t *tbl;
            };
        };

        // number representation
        num_t num;
    };
} var_t;


// properties of variables
static inline bool isnil(var_t v) { return !v.meta; }
static inline bool isnum(var_t v) { return v.type == MU_NUM; }
static inline bool isstr(var_t v) { return v.type == MU_STR; }
static inline bool istbl(var_t v) { return (6 & v.meta) == 4; }
static inline bool isobj(var_t v) { return v.type == MU_OBJ; }
static inline bool isfn(var_t v)  { return (6 & v.meta) == 2 || v.type == MU_FN; }

// definitions for accessing components
static inline ref_t *getref(var_t v)  { v.type = 0; return v.ref; }
static inline type_t gettype(var_t v) { return v.type; }

static inline num_t getnum(var_t v)        { v.meta &= ~3; return v.num; }
static inline const str_t *getstr(var_t v) { return v.str + v.off; }
static inline tbl_t *gettbl(var_t v)       { return v.tbl; }
static inline fn_t *getfn(var_t v)         { v.meta &= ~3; return v.fn; }


// definitions of literal vars in C
#define vnil  ((var_t){{0}})
#define vnan  vnum(NAN)
#define vinf  vnum(INFINITY)
#define vninf vnum(-INFINITY)

// var constructors for C
static inline var_t vraw(uint32_t r) {
    var_t v;
    v.data = r;
    v.type = MU_NUM;
    return v;
}

static inline var_t vnum(num_t n) {
    var_t v;
    v.num = n;
    v.type = MU_NUM;
    return v;
}

static inline var_t vstr(const str_t *s, len_t off, len_t len) {
    var_t v;
    v.str = s;
    v.off = off;
    v.len = len;
    v.type = MU_STR;
    return v;
}

static inline var_t vfn(fn_t *f, tbl_t *s) {
    var_t v;
    v.fn = f;
    v.tbl = s;
    v.type = MU_FN;
    return v;
}

static inline var_t vtbl(tbl_t *t) {
    var_t v;
    v.ref = (ref_t*)t;
    v.tbl = t;
    v.type = MU_TBL;
    return v;
}

static inline var_t vobj(tbl_t *t) {
    var_t v;
    v.ref = (ref_t*)t;
    v.tbl = t;
    v.type = MU_OBJ;
    return v;
}

static inline var_t vbfn(bfn_t *f) {
    var_t v;
    v.bfn = f;
    v.type = MU_BFN;
    return v;
}

static inline var_t vsfn(sfn_t *f, tbl_t *s) {
    var_t v;
    v.sfn = f;
    v.tbl = s;
    v.type = MU_SFN;
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

static inline void var_inc(var_t v) {
    if (4 & v.meta)
        ref_inc(v.ref);
    if (!(3 & ~v.meta))
        ref_inc(v.tbl);
}

static inline void var_dec(var_t v) {
    static void (* const dtors[4])(void *) = {
        tbl_destroy, 0, str_destroy, fn_destroy
    };

    if (4 & v.meta)
        ref_dec(v.ref, dtors[3 & v.meta]);
    if (!(3 & ~v.meta))
        ref_dec(v.tbl, tbl_destroy);
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


#endif
