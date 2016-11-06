/*
 * Mu scripting language
 */

#ifndef MU_H
#define MU_H
#include "config.h"
#include "mem.h"
#include <string.h>
#include <stdarg.h>


// Three bit type specifier located in lowest bits of each variable
// 3b00x indicates type is not reference counted
enum mtype {
    MTNIL  = 0, // nil
    MTNUM  = 1, // number
    MTSTR  = 3, // string
    MTBUF  = 2, // buffer
    MTCBUF = 6, // managed buffer
    MTTBL  = 4, // table
    MTFN   = 5, // function
};

// Declaration of mu type
// It doesn't necessarily point to anything, but using a
// void * would risk unwanted implicit conversions.
typedef struct mu *mu_t;


// Access to type and general components
mu_inline enum mtype mu_gettype(mu_t m) { return 7 & (muint_t)m; }
mu_inline mref_t mu_getref(mu_t m) { return *(mref_t *)(~7 & (muint_t)m); }

// Properties of variables
mu_inline bool mu_isnil(mu_t m) { return !m; }
mu_inline bool mu_isnum(mu_t m) { return mu_gettype(m) == MTNUM; }
mu_inline bool mu_isstr(mu_t m) { return mu_gettype(m) == MTSTR; }
mu_inline bool mu_isbuf(mu_t m) { return (3 & (muint_t)m) == MTBUF; }
mu_inline bool mu_istbl(mu_t m) { return mu_gettype(m) == MTTBL; }
mu_inline bool mu_isfn(mu_t m)  { return mu_gettype(m) == MTFN;  }
mu_inline bool mu_isref(mu_t m) { return 6 & (muint_t)m; }


// Smallest addressable unit
typedef unsigned char mbyte_t;

// Length type for strings/tables
typedef muinth_t mlen_t;


// Multiple variables can be passed in a frame,
// which is a small array of MU_FRAME elements.
//
// If more than MU_FRAME elements need to be passed, the
// frame count of 0xf indicates a table containing the true
// elements is passed as the first value in the frame.
// 
// For function calls, the frame count is split into two
// nibbles for arguments and return values, in that order.
#define MU_FRAME 4

// Type for specific one or two frame counts
typedef uint8_t mcnt_t;

// Frame operations
mu_inline mlen_t mu_frame_len(mcnt_t fc) {
    return (fc > MU_FRAME) ? 1 : fc;
}

void mu_frame_move(mcnt_t fc, mu_t *dframe, mu_t *sframe);
void mu_frame_convert(mcnt_t sc, mcnt_t dc, mu_t *frame);


// Reference counting
mu_inline mu_t mu_inc(mu_t m) {
    if (mu_isref(m)) {
        mu_ref_inc(m);
    }

    return m;
}

mu_inline void mu_dec(mu_t m) {
    if (mu_isref(m) && mu_ref_dec(m)) {
        extern void mu_destroy(mu_t m);
        mu_destroy(m);
    }
}


// System operations
mu_noreturn mu_verrorf(const char *f, va_list args);
mu_noreturn mu_errorf(const char *f, ...);
mu_noreturn mu_error(const char *s, muint_t n);

void mu_vprintf(const char *f, va_list args);
void mu_printf(const char *f, ...);
void mu_print(const char *s, muint_t n);

mu_t mu_import(mu_t name);

// Evaluation and entry into Mu
void mu_feval(const char *s, muint_t n, mu_t scope, mcnt_t fc, mu_t *frame);
mu_t mu_veval(const char *s, muint_t n, mu_t scope, mcnt_t fc, va_list args);
mu_t mu_eval(const char *s, muint_t n, mu_t scope, mcnt_t fc, ...);

// Common errors
mu_noreturn mu_error_arg(mu_t name, mcnt_t count, mu_t *frame);
mu_noreturn mu_error_op(mu_t name, mcnt_t count, mu_t *args);
mu_noreturn mu_error_cast(mu_t name, mu_t m);


// Standard functions in readonly builtins table
#define MU_BUILTINS     mu_gen_builtins()

// Builtin constants
#define MU_NIL          ((mu_t)0)

#define MU_TRUE         mu_gen_true()
#define MU_FALSE        MU_NIL

#define MU_INF          mu_gen_inf()
#define MU_NINF         mu_gen_ninf()
#define MU_E            mu_gen_e()
#define MU_PI           mu_gen_pi()
#define MU_ID           mu_gen_id()

// Builtin functions
#define MU_NUM          mu_gen_num()
#define MU_STR          mu_gen_str()
#define MU_TBL          mu_gen_tbl()
#define MU_FN           mu_gen_fn()

#define MU_NOT          mu_gen_not()
#define MU_EQ           mu_gen_eq()
#define MU_NEQ          mu_gen_neq()
#define MU_IS           mu_gen_is()
#define MU_LT           mu_gen_lt()
#define MU_LTE          mu_gen_lte()
#define MU_GT           mu_gen_gt()
#define MU_GTE          mu_gen_gte()

#define MU_ADD          mu_gen_add()
#define MU_SUB          mu_gen_sub()
#define MU_MUL          mu_gen_mul()
#define MU_DIV          mu_gen_div()
#define MU_ABS          mu_gen_abs()
#define MU_FLOOR        mu_gen_floor()
#define MU_CEIL         mu_gen_ceil()
#define MU_IDIV         mu_gen_idiv()
#define MU_MOD          mu_gen_mod()
#define MU_POW          mu_gen_pow()
#define MU_LOG          mu_gen_log()

#define MU_COS          mu_gen_cos()
#define MU_ACOS         mu_gen_acos()
#define MU_SIN          mu_gen_sin()
#define MU_ASIN         mu_gen_asin()
#define MU_TAN          mu_gen_tan()
#define MU_ATAN         mu_gen_atan()

#define MU_AND          mu_gen_and()
#define MU_OR           mu_gen_or()
#define MU_XOR          mu_gen_xor()
#define MU_DIFF         mu_gen_diff()
#define MU_SHL          mu_gen_shl()
#define MU_SHR          mu_gen_shr()

#define MU_PARSE        mu_gen_parse()
#define MU_REPR         mu_gen_repr()
#define MU_BIN          mu_gen_bin()
#define MU_OCT          mu_gen_oct()
#define MU_HEX          mu_gen_hex()

#define MU_LEN          mu_gen_len()
#define MU_TAIL         mu_gen_tail()
#define MU_PUSH         mu_gen_push()
#define MU_POP          mu_gen_pop()
#define MU_CONCAT       mu_gen_concat()
#define MU_SUBSET       mu_gen_subset()

#define MU_FIND         mu_gen_find()
#define MU_REPLACE      mu_gen_replace()
#define MU_SPLIT        mu_gen_split()
#define MU_JOIN         mu_gen_join()
#define MU_PAD          mu_gen_pad()
#define MU_STRIP        mu_gen_strip()

#define MU_BIND         mu_gen_bind()
#define MU_COMP         mu_gen_comp()
#define MU_MAP          mu_gen_map()
#define MU_FILTER       mu_gen_filter()
#define MU_REDUCE       mu_gen_reduce()
#define MU_ANY          mu_gen_any()
#define MU_ALL          mu_gen_all()

#define MU_ITER         mu_gen_iter()
#define MU_PAIRS        mu_gen_pairs()
#define MU_RANGE        mu_gen_range()
#define MU_REPEAT       mu_gen_repeat()
#define MU_SEED         mu_gen_seed()

#define MU_ZIP          mu_gen_zip()
#define MU_CHAIN        mu_gen_chain()
#define MU_TAKE         mu_gen_take()
#define MU_DROP         mu_gen_drop()

#define MU_MIN          mu_gen_min()
#define MU_MAX          mu_gen_max()
#define MU_REVERSE      mu_gen_reverse()
#define MU_SORT         mu_gen_sort()

#define MU_ERROR        mu_gen_error()
#define MU_PRINT        mu_gen_print()
#define MU_IMPORT       mu_gen_import()

// Builtin keys
#define MU_KEY_TRUE     mu_gen_key_true()
#define MU_KEY_FALSE    mu_gen_key_false()
#define MU_KEY_INF      mu_gen_key_inf()
#define MU_KEY_E        mu_gen_key_e()
#define MU_KEY_PI       mu_gen_key_pi()
#define MU_KEY_ID       mu_gen_key_id()

#define MU_KEY_NUM      mu_gen_key_num()
#define MU_KEY_STR      mu_gen_key_str()
#define MU_KEY_TBL      mu_gen_key_tbl()
#define MU_KEY_FN2      mu_gen_key_fn2()

#define MU_KEY_NOT      mu_gen_key_not()
#define MU_KEY_EQ       mu_gen_key_eq()
#define MU_KEY_NEQ      mu_gen_key_neq()
#define MU_KEY_IS       mu_gen_key_is()
#define MU_KEY_LT       mu_gen_key_lt()
#define MU_KEY_LTE      mu_gen_key_lte()
#define MU_KEY_GT       mu_gen_key_gt()
#define MU_KEY_GTE      mu_gen_key_gte()

#define MU_KEY_ADD      mu_gen_key_add()
#define MU_KEY_SUB      mu_gen_key_sub()
#define MU_KEY_MUL      mu_gen_key_mul()
#define MU_KEY_DIV      mu_gen_key_div()
#define MU_KEY_ABS      mu_gen_key_abs()
#define MU_KEY_FLOOR    mu_gen_key_floor()
#define MU_KEY_CEIL     mu_gen_key_ceil()
#define MU_KEY_IDIV     mu_gen_key_idiv()
#define MU_KEY_MOD      mu_gen_key_mod()
#define MU_KEY_POW      mu_gen_key_pow()
#define MU_KEY_LOG      mu_gen_key_log()

#define MU_KEY_COS      mu_gen_key_cos()
#define MU_KEY_ACOS     mu_gen_key_acos()
#define MU_KEY_SIN      mu_gen_key_sin()
#define MU_KEY_ASIN     mu_gen_key_asin()
#define MU_KEY_TAN      mu_gen_key_tan()
#define MU_KEY_ATAN     mu_gen_key_atan()

#define MU_KEY_AND2     mu_gen_key_and2()
#define MU_KEY_OR2      mu_gen_key_or2()
#define MU_KEY_XOR      mu_gen_key_xor()
#define MU_KEY_DIFF     mu_gen_key_diff()
#define MU_KEY_SHL      mu_gen_key_shl()
#define MU_KEY_SHR      mu_gen_key_shr()

#define MU_KEY_PARSE    mu_gen_key_parse()
#define MU_KEY_REPR     mu_gen_key_repr()
#define MU_KEY_BIN      mu_gen_key_bin()
#define MU_KEY_OCT      mu_gen_key_oct()
#define MU_KEY_HEX      mu_gen_key_hex()

#define MU_KEY_LEN      mu_gen_key_len()
#define MU_KEY_TAIL     mu_gen_key_tail()
#define MU_KEY_PUSH     mu_gen_key_push()
#define MU_KEY_POP      mu_gen_key_pop()
#define MU_KEY_CONCAT   mu_gen_key_concat()
#define MU_KEY_SUBSET   mu_gen_key_subset()

#define MU_KEY_FIND     mu_gen_key_find()
#define MU_KEY_REPLACE  mu_gen_key_replace()
#define MU_KEY_SPLIT    mu_gen_key_split()
#define MU_KEY_JOIN     mu_gen_key_join()
#define MU_KEY_PAD      mu_gen_key_pad()
#define MU_KEY_STRIP    mu_gen_key_strip()

#define MU_KEY_BIND     mu_gen_key_bind()
#define MU_KEY_COMP     mu_gen_key_comp()
#define MU_KEY_MAP      mu_gen_key_map()
#define MU_KEY_FILTER   mu_gen_key_filter()
#define MU_KEY_REDUCE   mu_gen_key_reduce()
#define MU_KEY_ANY      mu_gen_key_any()
#define MU_KEY_ALL      mu_gen_key_all()

#define MU_KEY_ITER     mu_gen_key_iter()
#define MU_KEY_PAIRS    mu_gen_key_pairs()
#define MU_KEY_RANGE    mu_gen_key_range()
#define MU_KEY_REPEAT   mu_gen_key_repeat()
#define MU_KEY_SEED     mu_gen_key_seed()

#define MU_KEY_ZIP      mu_gen_key_zip()
#define MU_KEY_CHAIN    mu_gen_key_chain()
#define MU_KEY_TAKE     mu_gen_key_take()
#define MU_KEY_DROP     mu_gen_key_drop()

#define MU_KEY_MIN      mu_gen_key_min()
#define MU_KEY_MAX      mu_gen_key_max()
#define MU_KEY_REVERSE  mu_gen_key_reverse()
#define MU_KEY_SORT     mu_gen_key_sort()

#define MU_KEY_ERROR    mu_gen_key_error()
#define MU_KEY_PRINT    mu_gen_key_print()
#define MU_KEY_IMPORT   mu_gen_key_import()


// Builtin generating functions
mu_t mu_gen_builtins(void);

mu_t mu_gen_true(void);
mu_t mu_gen_inf(void);
mu_t mu_gen_ninf(void);
mu_t mu_gen_e(void);
mu_t mu_gen_pi(void);
mu_t mu_gen_id(void);

mu_t mu_gen_num(void);
mu_t mu_gen_str(void);
mu_t mu_gen_tbl(void);
mu_t mu_gen_fn(void);

mu_t mu_gen_not(void);
mu_t mu_gen_eq(void);
mu_t mu_gen_neq(void);
mu_t mu_gen_is(void);
mu_t mu_gen_lt(void);
mu_t mu_gen_lte(void);
mu_t mu_gen_gt(void);
mu_t mu_gen_gte(void);

mu_t mu_gen_add(void);
mu_t mu_gen_sub(void);
mu_t mu_gen_mul(void);
mu_t mu_gen_div(void);
mu_t mu_gen_abs(void);
mu_t mu_gen_floor(void);
mu_t mu_gen_ceil(void);
mu_t mu_gen_idiv(void);
mu_t mu_gen_mod(void);
mu_t mu_gen_pow(void);
mu_t mu_gen_log(void);

mu_t mu_gen_cos(void);
mu_t mu_gen_acos(void);
mu_t mu_gen_sin(void);
mu_t mu_gen_asin(void);
mu_t mu_gen_tan(void);
mu_t mu_gen_atan(void);

mu_t mu_gen_and(void);
mu_t mu_gen_or(void);
mu_t mu_gen_xor(void);
mu_t mu_gen_diff(void);
mu_t mu_gen_shl(void);
mu_t mu_gen_shr(void);

mu_t mu_gen_parse(void);
mu_t mu_gen_repr(void);
mu_t mu_gen_bin(void);
mu_t mu_gen_oct(void);
mu_t mu_gen_hex(void);

mu_t mu_gen_len(void);
mu_t mu_gen_tail(void);
mu_t mu_gen_push(void);
mu_t mu_gen_pop(void);
mu_t mu_gen_concat(void);
mu_t mu_gen_subset(void);

mu_t mu_gen_find(void);
mu_t mu_gen_replace(void);
mu_t mu_gen_split(void);
mu_t mu_gen_join(void);
mu_t mu_gen_pad(void);
mu_t mu_gen_strip(void);

mu_t mu_gen_bind(void);
mu_t mu_gen_comp(void);
mu_t mu_gen_map(void);
mu_t mu_gen_filter(void);
mu_t mu_gen_reduce(void);
mu_t mu_gen_any(void);
mu_t mu_gen_all(void);

mu_t mu_gen_iter(void);
mu_t mu_gen_pairs(void);
mu_t mu_gen_range(void);
mu_t mu_gen_repeat(void);
mu_t mu_gen_seed(void);

mu_t mu_gen_zip(void);
mu_t mu_gen_chain(void);
mu_t mu_gen_take(void);
mu_t mu_gen_drop(void);

mu_t mu_gen_min(void);
mu_t mu_gen_max(void);
mu_t mu_gen_reverse(void);
mu_t mu_gen_sort(void);

mu_t mu_gen_error(void);
mu_t mu_gen_print(void);
mu_t mu_gen_import(void);

mu_t mu_gen_key_true(void);
mu_t mu_gen_key_false(void);
mu_t mu_gen_key_inf(void);
mu_t mu_gen_key_ninf(void);
mu_t mu_gen_key_e(void);
mu_t mu_gen_key_pi(void);
mu_t mu_gen_key_id(void);

mu_t mu_gen_key_num(void);
mu_t mu_gen_key_str(void);
mu_t mu_gen_key_tbl(void);
mu_t mu_gen_key_fn2(void);

mu_t mu_gen_key_not(void);
mu_t mu_gen_key_eq(void);
mu_t mu_gen_key_neq(void);
mu_t mu_gen_key_is(void);
mu_t mu_gen_key_lt(void);
mu_t mu_gen_key_lte(void);
mu_t mu_gen_key_gt(void);
mu_t mu_gen_key_gte(void);

mu_t mu_gen_key_add(void);
mu_t mu_gen_key_sub(void);
mu_t mu_gen_key_mul(void);
mu_t mu_gen_key_div(void);
mu_t mu_gen_key_abs(void);
mu_t mu_gen_key_floor(void);
mu_t mu_gen_key_ceil(void);
mu_t mu_gen_key_idiv(void);
mu_t mu_gen_key_mod(void);
mu_t mu_gen_key_pow(void);
mu_t mu_gen_key_log(void);

mu_t mu_gen_key_cos(void);
mu_t mu_gen_key_acos(void);
mu_t mu_gen_key_sin(void);
mu_t mu_gen_key_asin(void);
mu_t mu_gen_key_tan(void);
mu_t mu_gen_key_atan(void);

mu_t mu_gen_key_and2(void);
mu_t mu_gen_key_or2(void);
mu_t mu_gen_key_xor(void);
mu_t mu_gen_key_diff(void);
mu_t mu_gen_key_shl(void);
mu_t mu_gen_key_shr(void);

mu_t mu_gen_key_parse(void);
mu_t mu_gen_key_repr(void);
mu_t mu_gen_key_bin(void);
mu_t mu_gen_key_oct(void);
mu_t mu_gen_key_hex(void);

mu_t mu_gen_key_len(void);
mu_t mu_gen_key_tail(void);
mu_t mu_gen_key_push(void);
mu_t mu_gen_key_pop(void);
mu_t mu_gen_key_concat(void);
mu_t mu_gen_key_subset(void);

mu_t mu_gen_key_find(void);
mu_t mu_gen_key_replace(void);
mu_t mu_gen_key_split(void);
mu_t mu_gen_key_join(void);
mu_t mu_gen_key_pad(void);
mu_t mu_gen_key_strip(void);

mu_t mu_gen_key_bind(void);
mu_t mu_gen_key_comp(void);
mu_t mu_gen_key_map(void);
mu_t mu_gen_key_filter(void);
mu_t mu_gen_key_reduce(void);
mu_t mu_gen_key_any(void);
mu_t mu_gen_key_all(void);

mu_t mu_gen_key_iter(void);
mu_t mu_gen_key_pairs(void);
mu_t mu_gen_key_range(void);
mu_t mu_gen_key_repeat(void);
mu_t mu_gen_key_seed(void);

mu_t mu_gen_key_zip(void);
mu_t mu_gen_key_chain(void);
mu_t mu_gen_key_take(void);
mu_t mu_gen_key_drop(void);

mu_t mu_gen_key_min(void);
mu_t mu_gen_key_max(void);
mu_t mu_gen_key_reverse(void);
mu_t mu_gen_key_sort(void);

mu_t mu_gen_key_error(void);
mu_t mu_gen_key_print(void);
mu_t mu_gen_key_import(void);


#endif
