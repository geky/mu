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
    TYPE_NULL = 0x0, // null

    TYPE_NUM  = 0x3, // number - 12.3
    TYPE_STR  = 0x4, // string - "hello"

    TYPE_TBL  = 0x6, // table - ['a':1, 'b':2, 'c':3]
    TYPE_MTBL = 0x7, // builtin table

    TYPE_FN   = 0x5, // function - fn() {5}
    TYPE_BFN  = 0x1, // builtin function
} type_t;


// All vars hash to 32 bits 
typedef uint32_t hash_t;

// Base types
typedef const uint8_t str_t;
typedef double num_t;
typedef struct tbl tbl_t;
typedef struct fn fn_t;
typedef struct var bfn_t(struct var);

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
                str_t *str;
            };

            union {
                // data for vars
                uint32_t data;
                bool ro: 1;

                // string offset and length
                struct {
                    uint16_t off;
                    uint16_t len;
                };

                // pointer to table representation
                tbl_t *tbl;

                // pointer to function representation
                fn_t *fn;

                // built in function pointer
                bfn_t *bfn;
            };
        };

        // number representation
        num_t num;
    };
} var_t;


// properties of variables
static inline bool var_isnull(var_t v)  { return !v.meta; }
static inline bool var_isref(var_t v)   { return 0x4 & v.meta; }
static inline bool var_isconst(var_t v) { return v.ro; }
static inline bool var_isnum(var_t v)   { return v.type == TYPE_NUM; }
static inline bool var_isstr(var_t v)   { return v.type == TYPE_STR; }
static inline bool var_istbl(var_t v)   { return (0x6 & v.meta) == 0x6; }
static inline bool var_isfn(var_t v)    { return (0x3 & v.meta) == 0x1; }
static inline bool var_ismtbl(var_t v)  { return v.type == TYPE_MTBL; }

// definitions for accessing components
static inline ref_t *var_ref(var_t v) { v.type = 0; return v.ref; }
static inline enum type var_type(var_t v) { return v.type; }

static inline num_t var_num(var_t v) { v.type = 0; return v.num; }
static inline str_t *var_str(var_t v) { return v.str + v.off; }


// definitions of literal vars in c
#define vnull ((var_t){{0}})
#define vnan  vnum(NAN)
#define vinf  vnum(INFINITY)

static inline var_t vnum(num_t n) {
    var_t v;
    v.num = n;
    v.type = TYPE_NUM;
    return v;
}

static inline var_t vstr(str_t *s, uint16_t off, uint16_t len) {
    var_t v;
    v.str = s;
    v.off = off;
    v.len = len;
    v.type = TYPE_STR;
    return v;
}

static inline var_t vtbl(tbl_t *t) {
    var_t v;
    v.ref = (ref_t*)t;
    v.tbl = t;
    v.type = TYPE_TBL;
    return v;
}

static inline var_t vfn(fn_t *f) {
    var_t v;
    v.ref = (ref_t*)f;
    v.fn = f;
    v.type = TYPE_FN;
    return v;
}

static inline var_t vmtbl(tbl_t *t) {
    var_t v;
    v.ref = (ref_t*)t;
    v.tbl = t;
    v.type = TYPE_MTBL;
    return v;
}

static inline var_t vbfn(bfn_t *f) {
    var_t v;
    v.bfn = f;
    v.type = TYPE_BFN;
    return v;
}

#define vcstr(c) ({                     \
    static struct {                     \
        ref_t r;                        \
        str_t s[sizeof(c)-1];           \
    } _vcstr = { 2, {(c)}};             \
                                        \
    vstr(_vcstr.s, 0, sizeof(c)-1);     \
})


// Mapping of reference counting functions
static inline void var_incref(var_t v) { vref_inc(v.ref); }
static inline void var_decref(var_t v) { vref_dec(v.ref); }


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
void var_set(var_t v, var_t key, var_t val);
void var_add(var_t v, var_t val);

// Function calls performed on variables
var_t var_call(var_t v, var_t args);


// Cleans up memory of a variable
void vdestroy(void *);


#endif
