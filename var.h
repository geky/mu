/* 
 *  Variable types and definitions
 */

#ifndef V_VAR
#define V_VAR

#include "mem.h"

#include <stdint.h>
#include <stdbool.h>
#include <math.h>


// Three bit type specifier located in first 
// three bits of each var
// Highest bit indicates if reference counted
typedef enum type {
    TYPE_NIL = 0x0, // nil - nil
    TYPE_NUM = 0x1, // number - 12.3

    TYPE_BFN = 0x2, // builtin function
    TYPE_SFN = 0x3, // builtin function with scope

    TYPE_STR = 0x4, // string - "hello"
    TYPE_FN  = 0x5, // function - fn() { return 5 }

    TYPE_TBL = 0x6, // table - ['a':1, 'b':2, 'c':3]
    TYPE_OBJ = 0x7, // wrapped table - obj(['a':1, 'b':2])
} type_t;


// All vars hash to 32 bits 
typedef uint32_t hash_t;

// Length type bound to 16 bits for space consumption
typedef uint16_t len_t;
#define vmaxlen 0xffff

// Base types
typedef double num_t;
typedef uint8_t str_t;

// Base structs
typedef struct tbl tbl_t;
typedef struct fn fn_t;
typedef struct var bfn_t(tbl_t *a);
typedef struct var sfn_t(tbl_t *a, tbl_t *s);


// Actual var type declariation
typedef struct var {
    union {
        // bitwise representation
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

                // function representation
                fn_t *fn;

                // builtin scope representation
                tbl_t *stbl;
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

                // builtin function pointer
                bfn_t *bfn;

                // builtin function pointer with scope
                sfn_t *sfn;
            };
        };

        // number representation
        num_t num;
    };
} var_t;


// properties of variables
static inline bool var_isnil(var_t v) { return !v.meta; }
static inline bool var_isnum(var_t v) { return v.type == TYPE_NUM; }
static inline bool var_isstr(var_t v) { return v.type == TYPE_STR; }
static inline bool var_istbl(var_t v) { return (0x6 & v.meta) == 0x6; }
static inline bool var_isobj(var_t v) { return v.type == TYPE_OBJ; }

// definitions for accessing components
static inline ref_t *var_ref(var_t v)     { v.type = 0; return v.ref; }
static inline enum type var_type(var_t v) { return v.type; }

static inline num_t var_num(var_t v)        { v.type = 0; return v.num; }
static inline const str_t *var_str(var_t v) { return v.str + v.off; }
static inline fn_t *var_fn(var_t v)         { v.meta--; return v.fn; }
static inline tbl_t *var_tbl(var_t v)       { return v.tbl; }


// definitions of literal vars in C
#define vnil ((var_t){{0}})
#define vnan vnum(NAN)
#define vinf vnum(INFINITY)


// var constructors for C
static inline var_t vraw(uint32_t r) {
    var_t v;
    v.data = r;
    v.type = TYPE_NUM;
    return v;
}

static inline var_t vnum(num_t n) {
    var_t v;
    v.num = n;
    v.type = TYPE_NUM;
    return v;
}

static inline var_t vstr(const str_t *s, len_t off, len_t len) {
    var_t v;
    v.str = s;
    v.off = off;
    v.len = len;
    v.type = TYPE_STR;
    return v;
}

static inline var_t vfn(fn_t *f, tbl_t *s) {
    var_t v;
    v.fn = f;
    v.tbl = s;
    v.type = TYPE_FN;
    return v;
}

static inline var_t vtbl(tbl_t *t) {
    var_t v;
    v.tbl = t;
    v.type = TYPE_TBL;
    return v;
}

static inline var_t vobj(tbl_t *t) {
    var_t v;
    v.tbl = t;
    v.type = TYPE_OBJ;
    return v;
}

static inline var_t vbfn(bfn_t *f) {
    var_t v;
    v.bfn = f;
    v.type = TYPE_BFN;
    return v;
}

static inline var_t vsfn(sfn_t *f, tbl_t *s) {
    var_t v;
    v.sfn = f;
    v.stbl = s;
    v.type = TYPE_SFN;
    return v;
}

#define vcstr(c) ({                     \
    static struct {                     \
        ref_t r;                        \
        const str_t s[sizeof(c)-1];     \
    } _vcstr = { 1, {(c)}};             \
                                        \
    _vcstr.r++;                         \
    vstr(_vcstr.s, 0, sizeof(c)-1);     \
})


// Mapping of reference counting functions
extern void tbl_destroy(void *);
extern void (*const vdtor_a[8])(void*);

static inline void var_inc(var_t v) {
    if (v.type >= TYPE_SFN) {
        if (v.type <= TYPE_FN)
            vref_inc(v.ref);
        if (v.type >= TYPE_FN)
            vref_inc(v.tbl);
    }
}

static inline void var_dec(var_t v) {
    if (v.type >= TYPE_SFN) {
        if (v.type <= TYPE_FN)
            vref_dec(v.ref, vdtor_a[v.type]);
        if (v.type >= TYPE_FN)
            vref_dec(v.tbl, tbl_destroy);
    }
}


// Returns true if both variables are the
// same type and equivalent.
bool var_equals(var_t a, var_t b);

// Returns a hash value of the given variable. 
hash_t var_hash(var_t var);

// Returns a string representation of the variable
var_t var_repr(var_t v);

// Prints variable to stdout for debugging
void var_print(var_t v);

// Table related functions performed on variables
var_t var_lookup(var_t v, var_t key);
void var_assign(var_t v, var_t key, var_t val);
void var_insert(var_t v, var_t key, var_t val);
void var_add(var_t v, var_t val);

// Function calls performed on variables
var_t var_call(var_t v, tbl_t *args);


#endif
