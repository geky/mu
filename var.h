/* 
 *  Variable types and definitions
 */

#ifndef V_VAR
#define V_VAR

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>


// Three bit type specifier located in first 
// three bits of each var
// Highest bit indicates if reference counted
typedef enum type {
    TYPE_NULL = 0x0, // null

    TYPE_NUM  = 0x3, // number - 12.3
    TYPE_STR  = 0x4, // string - 'hello'

    TYPE_TBL  = 0x5, // table - ['a':1, 'b':2, 'c':3]
    TYPE_MTB  = 0x1, // builtin table

    TYPE_FN   = 0x6, // function - fn() {5}
    TYPE_BFN  = 0x2, // builtin function

    TYPE_ERR  = 0x7  // error - error("oh no")
} type_t;


// All vars hash to 32 bits 
typedef uint32_t hash_t;

// Reference type as word type
// Must be in address which does not use lower 3 bits
typedef unsigned int ref_t __attribute__((aligned(8)));

// Base types
typedef const unsigned char str_t;

#ifdef V_USE_FLOAT
typedef float num_t;
#else
typedef double num_t;
#endif

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

#ifdef V_USE_FLOAT
                // numbers in 32-bit float mode
                num_t num;
#endif
                // string offset and length
                struct {
                    uint16_t off;
                    uint16_t len;
                };

                // pointer to table representation
                struct tbl *tbl;

                // pointer to metatable representation
                struct mtb *mtb;

                // pointer to function representation
                struct fn *fn;

                // built in function pointer
                struct var (*bfn)(struct var);
            };
        };

#ifndef V_USE_FLOAT
        // numbers in 64-bit float mode
        num_t num;
#endif
    };
} var_t;



// definitions for accessing components
#define var_ref(v)  ((ref_t*)((v).meta & ~0x7))
#define var_type(v) ((v).type)

#define var_num(v) (((var_t){{(v).bits & ~0x7}}).num)
#define var_str(v) ((v).str + (v).off)
#define var_len(v) ((v).len)


// definitions of literal vars in c
#define vnull ((var_t){{0}})
#define vnan  vnum(NAN)
#define vinf  vnum(INFINITY)

#define vnum(n) ((var_t){{TYPE_NUM | (~0x7 & ((var_t){.num=(n)}).bits)}})
#define vtb(n)  ((var_t){.type=TYPE_BTB, .btb=(n)})
#define vfn(n)  ((var_t){.type=TYPE_BFN, .bfn=(n)})

#define vstr(n) ({                              \
    static struct {                             \
        ref_t r;                                \
        const unsigned char s[sizeof(n)-1];     \
    } _vstr = { 1, {(n)}};                      \
                                                \
    ((var_t){{                                  \
        TYPE_STR | (~0x7 & ((var_t){            \
            .ref = &_vstr.r,                    \
            .off = 0,                           \
            .len = sizeof(n)-1                  \
        }).bits)                                \
    }});                                        \
})



// Memory management and garbage collection
// Each block of memory prefixed with ref_t reference
// count. Deallocated immediately when refs hit zero.
// It is up to the user to avoid cyclic dependencies
ref_t *var_alloc(size_t size);
void var_incref(var_t var);
void var_decref(var_t var);

// Returns true if both variables are the
// same type and equivalent.
bool var_equals(var_t a, var_t b);

// Returns a hash value of the given variable. 
hash_t var_hash(var_t var);

// Returns a string representation of the variable
var_t var_repr(var_t v);

// Prints variable to stdout for debugging
void var_print(var_t v);


#endif
