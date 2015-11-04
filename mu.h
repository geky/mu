/*
 * Variable types and definitions
 */

#ifndef MU_TYPES_H
#define MU_TYPES_H
#include "config.h"
#include "mem.h"
#include <string.h>
#include <stdarg.h>


// Three bit type specifier located in lowest bits of each variable
// 3b00x indicates type is not reference counted
// 3b100 is the only mutable variable
// 3b11x are currently reserved
enum mtype {
    MTNIL   = 0, // nil
    MTNUM   = 1, // number
    MTSTR   = 2, // string
    MTTBL   = 4, // table
    MTFN    = 5, // function
    MTBFN   = 6, // builtin function
    MTSBFN  = 7, // scoped builtin function
};

// Declaration of mu type
// It doesn't necessarily point to anything, but using a
// void * would risk unwanted implicit conversions.
typedef struct mu *mu_t;


// Access to type and general components
mu_inline enum mtype mu_type(mu_t m) { return 7 & (muint_t)m; }
mu_inline mref_t mu_ref(mu_t m) { return *(mref_t *)(~7 & (muint_t)m); }

// Properties of variables
mu_inline bool mu_isnil(mu_t m) { return !m; }
mu_inline bool mu_isnum(mu_t m) { return mu_type(m) == MTNUM; }
mu_inline bool mu_isstr(mu_t m) { return mu_type(m) == MTSTR; }
mu_inline bool mu_istbl(mu_t m) { return mu_type(m) == MTTBL; }
mu_inline bool mu_isfn(mu_t m)  { return mu_type(m) >= MTFN;  }

mu_inline bool mu_isref(mu_t m)   { return 6 & (muint_t)m; }


// Reference counting
mu_inline mu_t mu_inc(mu_t m) {
    if (mu_isref(m))
        ref_inc(m);

    return m;
}

mu_inline void mu_dec(mu_t m) {
    extern void (*const mu_destroy_table[6])(mu_t);

    if (mu_isref(m) && ref_dec(m))
        mu_destroy_table[mu_type(m)-2](m);
}


// Multiple variables can be passed in a frame,
// which is a small array of MU_FRAME elements.
// 
// If more than MU_FRAME elements need to be passed
// a table containing the true elements is used instead.
#define MU_FRAME 4

// Type for specifying frame counts.
//
// The value of 0xf indicates a table is used.
// For function calls, the frame count is split into two 
// nibbles for arguments and return values, in that order.
typedef uint8_t mc_t;


// Conversion between different frame types
void mu_fconvert(mc_t dc, mc_t sc, mu_t *frame);

mu_inline muint_t mu_fcount(mc_t fc) {
    return (fc == 0xf) ? 1 : fc;
}

mu_inline void mu_fcopy(mc_t fc, mu_t *dframe, mu_t *sframe) {
    memcpy(dframe, sframe, sizeof(mu_t)*mu_fcount(fc));
}


// Constants
#define MU_NIL      ((mu_t)0)

#define MU_TRUE     mu_true()
#define MU_FALSE    MU_NIL

#define MU_INF      mu_inf()
#define MU_NINF     mu_ninf()
#define MU_E        mu_e()
#define MU_PI       mu_pi()
#define MU_ID       mu_id()

mu_pure mu_t mu_true(void);
mu_pure mu_t mu_inf(void);
mu_pure mu_t mu_ninf(void);
mu_pure mu_t mu_e(void);
mu_pure mu_t mu_pi(void);
mu_pure mu_t mu_id(void);


// Type casts
mu_t mu_num(mu_t m);
mu_t mu_str(mu_t m);
mu_t mu_tbl(mu_t m, mu_t tail);
mu_t mu_fn(mu_t m);

// Table related functions performed on variables
mu_t mu_lookup(mu_t m, mu_t key);
void mu_insert(mu_t m, mu_t key, mu_t val);
void mu_assign(mu_t m, mu_t key, mu_t val);

// Function calls performed on variables
mc_t mu_tcall(mu_t m, mc_t fc, mu_t *frame);
void mu_fcall(mu_t m, mc_t fc, mu_t *frame);
mu_t mu_vcall(mu_t m, mc_t fc, va_list args);
mu_t mu_call(mu_t m, mc_t fc, ...);

// Comparison operation
bool mu_is(mu_t a, mu_t type);
mint_t mu_cmp(mu_t a, mu_t b);

// Arithmetic operations
mu_t mu_pos(mu_t a);
mu_t mu_neg(mu_t a);
mu_t mu_add(mu_t a, mu_t b);
mu_t mu_sub(mu_t a, mu_t b);
mu_t mu_mul(mu_t a, mu_t b);
mu_t mu_div(mu_t a, mu_t b);
mu_t mu_idiv(mu_t a, mu_t b);
mu_t mu_mod(mu_t a, mu_t b);
mu_t mu_pow(mu_t a, mu_t b);
mu_t mu_log(mu_t a, mu_t b);

mu_t mu_abs(mu_t a);
mu_t mu_floor(mu_t a);
mu_t mu_ceil(mu_t a);

mu_t mu_cos(mu_t a);
mu_t mu_acos(mu_t a);
mu_t mu_sin(mu_t a);
mu_t mu_asin(mu_t a);
mu_t mu_tan(mu_t a);
mu_t mu_atan(mu_t a, mu_t b);

// Bitwise/Set operations
mu_t mu_not(mu_t a);
mu_t mu_and(mu_t a, mu_t b);
mu_t mu_or(mu_t a, mu_t b);
mu_t mu_xor(mu_t a, mu_t b);
mu_t mu_diff(mu_t a, mu_t b);

mu_t mu_shl(mu_t a, mu_t b);
mu_t mu_shr(mu_t a, mu_t b);

// String representation
mu_t mu_parse(mu_t m);
mu_t mu_addr(mu_t m);
mu_t mu_repr(mu_t m);
mu_t mu_dump(mu_t m, mu_t depth, mu_t indent);

mu_t mu_bin(mu_t m);
mu_t mu_oct(mu_t m);
mu_t mu_hex(mu_t m);

// String operations
mu_t mu_find(mu_t m, mu_t sub);
mu_t mu_replace(mu_t m, mu_t sub, mu_t rep, mu_t max);

mu_t mu_split(mu_t m, mu_t delim);
mu_t mu_join(mu_t iter, mu_t delim);

mu_t mu_pad(mu_t m, mu_t len, mu_t pad);
mu_t mu_strip(mu_t m, mu_t dir, mu_t pad);

// Data structure operations
// do not consume their target
mlen_t mu_len(mu_t m);
mu_t mu_tail(mu_t m);

void mu_push(mu_t m, mu_t v, mu_t i);
mu_t mu_pop(mu_t m, mu_t i);

// consume their target
mu_t mu_concat(mu_t a, mu_t b, mu_t offset);
mu_t mu_subset(mu_t a, mu_t lower, mu_t upper);

// Function operations
mu_t mu_bind(mu_t m, mu_t args);
mu_t mu_comp(mu_t ms);

mu_t mu_map(mu_t m, mu_t iter);
mu_t mu_filter(mu_t m, mu_t iter);
mu_t mu_reduce(mu_t m, mu_t iter, mu_t inits);

bool mu_any(mu_t m, mu_t iter);
bool mu_all(mu_t m, mu_t iter);

// Iterators and generators
mu_t mu_iter(mu_t m);
mu_t mu_pairs(mu_t m);

mu_t mu_range(mu_t start, mu_t stop, mu_t step);
mu_t mu_repeat(mu_t value, mu_t times);
mu_t mu_seed(mu_t m);

// Iterator manipulation
mu_t mu_zip(mu_t iters);
mu_t mu_chain(mu_t iters);

mu_t mu_take(mu_t m, mu_t iter);
mu_t mu_drop(mu_t m, mu_t iter);

bool mu_any(mu_t m, mu_t iter);
bool mu_all(mu_t m, mu_t iter);

// Iterator ordering operations
mu_t mu_min(mu_t iter);
mu_t mu_max(mu_t iter);

mu_t mu_reverse(mu_t iter);
mu_t mu_sort(mu_t iter);

// System operations
mu_noreturn mu_error(mu_t message);
void mu_print(mu_t message);
mu_t mu_import(mu_t name);


// Standard functions are provided as C functions as well as
// Mu functions in readonly builtins table
#define MU_BUILTINS mu_builtins()
mu_pure mu_t mu_builtins(void);


// Function keys and variables
#define MU_TRUE_KEY mu_true_key()
mu_pure mu_t mu_true_key(void);
#define MU_FALSE_KEY mu_false_key()
mu_pure mu_t mu_false_key(void);
#define MU_INF_KEY mu_inf_key()
mu_pure mu_t mu_inf_key(void);
#define MU_E_KEY mu_e_key()
mu_pure mu_t mu_e_key(void);
#define MU_PI_KEY mu_pi_key()
mu_pure mu_t mu_pi_key(void);
#define MU_ID_KEY mu_id_key()
mu_pure mu_t mu_id_key(void);

#define MU_NUM_KEY mu_num_key()
#define MU_NUM_BFN mu_num_bfn()
mu_pure mu_t mu_num_key(void);
mu_pure mu_t mu_num_bfn(void);
#define MU_STR_KEY mu_str_key()
#define MU_STR_BFN mu_str_bfn()
mu_pure mu_t mu_str_key(void);
mu_pure mu_t mu_str_bfn(void);
#define MU_TBL_KEY mu_tbl_key()
#define MU_TBL_BFN mu_tbl_bfn()
mu_pure mu_t mu_tbl_key(void);
mu_pure mu_t mu_tbl_bfn(void);
#define MU_FN_KEY mu_fn_key()
#define MU_FN_BFN mu_fn_bfn()
mu_pure mu_t mu_fn_key(void);
mu_pure mu_t mu_fn_bfn(void);

#define MU_NOT_KEY mu_not_key()
#define MU_NOT_BFN mu_not_bfn()
mu_pure mu_t mu_not_key(void);
mu_pure mu_t mu_not_bfn(void);
#define MU_EQ_KEY mu_eq_key()
#define MU_EQ_BFN mu_eq_bfn()
mu_pure mu_t mu_eq_key(void);
mu_pure mu_t mu_eq_bfn(void);
#define MU_NEQ_KEY mu_neq_key()
#define MU_NEQ_BFN mu_neq_bfn()
mu_pure mu_t mu_neq_key(void);
mu_pure mu_t mu_neq_bfn(void);
#define MU_IS_KEY mu_is_key()
#define MU_IS_BFN mu_is_bfn()
mu_pure mu_t mu_is_key(void);
mu_pure mu_t mu_is_bfn(void);
#define MU_LT_KEY mu_lt_key()
#define MU_LT_BFN mu_lt_bfn()
mu_pure mu_t mu_lt_key(void);
mu_pure mu_t mu_lt_bfn(void);
#define MU_LTE_KEY mu_lte_key()
#define MU_LTE_BFN mu_lte_bfn()
mu_pure mu_t mu_lte_key(void);
mu_pure mu_t mu_lte_bfn(void);
#define MU_GT_KEY mu_gt_key()
#define MU_GT_BFN mu_gt_bfn()
mu_pure mu_t mu_gt_key(void);
mu_pure mu_t mu_gt_bfn(void);
#define MU_GTE_KEY mu_gte_key()
#define MU_GTE_BFN mu_gte_bfn()
mu_pure mu_t mu_gte_key(void);
mu_pure mu_t mu_gte_bfn(void);

#define MU_ADD_KEY mu_add_key()
#define MU_ADD_BFN mu_add_bfn()
mu_pure mu_t mu_add_key(void);
mu_pure mu_t mu_add_bfn(void);
#define MU_SUB_KEY mu_sub_key()
#define MU_SUB_BFN mu_sub_bfn()
mu_pure mu_t mu_sub_key(void);
mu_pure mu_t mu_sub_bfn(void);
#define MU_MUL_KEY mu_mul_key()
#define MU_MUL_BFN mu_mul_bfn()
mu_pure mu_t mu_mul_key(void);
mu_pure mu_t mu_mul_bfn(void);
#define MU_DIV_KEY mu_div_key()
#define MU_DIV_BFN mu_div_bfn()
mu_pure mu_t mu_div_key(void);
mu_pure mu_t mu_div_bfn(void);
#define MU_ABS_KEY mu_abs_key()
#define MU_ABS_BFN mu_abs_bfn()
mu_pure mu_t mu_abs_key(void);
mu_pure mu_t mu_abs_bfn(void);
#define MU_FLOOR_KEY mu_floor_key()
#define MU_FLOOR_BFN mu_floor_bfn()
mu_pure mu_t mu_floor_key(void);
mu_pure mu_t mu_floor_bfn(void);
#define MU_CEIL_KEY mu_ceil_key()
#define MU_CEIL_BFN mu_ceil_bfn()
mu_pure mu_t mu_ceil_key(void);
mu_pure mu_t mu_ceil_bfn(void);
#define MU_IDIV_KEY mu_idiv_key()
#define MU_IDIV_BFN mu_idiv_bfn()
mu_pure mu_t mu_idiv_key(void);
mu_pure mu_t mu_idiv_bfn(void);
#define MU_MOD_KEY mu_mod_key()
#define MU_MOD_BFN mu_mod_bfn()
mu_pure mu_t mu_mod_key(void);
mu_pure mu_t mu_mod_bfn(void);
#define MU_POW_KEY mu_pow_key()
#define MU_POW_BFN mu_pow_bfn()
mu_pure mu_t mu_pow_key(void);
mu_pure mu_t mu_pow_bfn(void);
#define MU_LOG_KEY mu_log_key()
#define MU_LOG_BFN mu_log_bfn()
mu_pure mu_t mu_log_key(void);
mu_pure mu_t mu_log_bfn(void);

#define MU_COS_KEY mu_cos_key()
#define MU_COS_BFN mu_cos_bfn()
mu_pure mu_t mu_cos_key(void);
mu_pure mu_t mu_cos_bfn(void);
#define MU_ACOS_KEY mu_acos_key()
#define MU_ACOS_BFN mu_acos_bfn()
mu_pure mu_t mu_acos_key(void);
mu_pure mu_t mu_acos_bfn(void);
#define MU_SIN_KEY mu_sin_key()
#define MU_SIN_BFN mu_sin_bfn()
mu_pure mu_t mu_sin_key(void);
mu_pure mu_t mu_sin_bfn(void);
#define MU_ASIN_KEY mu_asin_key()
#define MU_ASIN_BFN mu_asin_bfn()
mu_pure mu_t mu_asin_key(void);
mu_pure mu_t mu_asin_bfn(void);
#define MU_TAN_KEY mu_tan_key()
#define MU_TAN_BFN mu_tan_bfn()
mu_pure mu_t mu_tan_key(void);
mu_pure mu_t mu_tan_bfn(void);
#define MU_ATAN_KEY mu_atan_key()
#define MU_ATAN_BFN mu_atan_bfn()
mu_pure mu_t mu_atan_key(void);
mu_pure mu_t mu_atan_bfn(void);

#define MU_AND_KEY mu_and_key()
#define MU_AND_BFN mu_and_bfn()
mu_pure mu_t mu_and_key(void);
mu_pure mu_t mu_and_bfn(void);
#define MU_OR_KEY mu_or_key()
#define MU_OR_BFN mu_or_bfn()
mu_pure mu_t mu_or_key(void);
mu_pure mu_t mu_or_bfn(void);
#define MU_XOR_KEY mu_xor_key()
#define MU_XOR_BFN mu_xor_bfn()
mu_pure mu_t mu_xor_key(void);
mu_pure mu_t mu_xor_bfn(void);
#define MU_DIFF_KEY mu_diff_key()
#define MU_DIFF_BFN mu_diff_bfn()
mu_pure mu_t mu_diff_key(void);
mu_pure mu_t mu_diff_bfn(void);
#define MU_SHL_KEY mu_shl_key()
#define MU_SHL_BFN mu_shl_bfn()
mu_pure mu_t mu_shl_key(void);
mu_pure mu_t mu_shl_bfn(void);
#define MU_SHR_KEY mu_shr_key()
#define MU_SHR_BFN mu_shr_bfn()
mu_pure mu_t mu_shr_key(void);
mu_pure mu_t mu_shr_bfn(void);

#define MU_PARSE_KEY mu_parse_key()
#define MU_PARSE_BFN mu_parse_bfn()
mu_pure mu_t mu_parse_key(void);
mu_pure mu_t mu_parse_bfn(void);
#define MU_REPR_KEY mu_repr_key()
#define MU_REPR_BFN mu_repr_bfn()
mu_pure mu_t mu_repr_key(void);
mu_pure mu_t mu_repr_bfn(void);
#define MU_BIN_KEY mu_bin_key()
#define MU_BIN_BFN mu_bin_bfn()
mu_pure mu_t mu_bin_key(void);
mu_pure mu_t mu_bin_bfn(void);
#define MU_OCT_KEY mu_oct_key()
#define MU_OCT_BFN mu_oct_bfn()
mu_pure mu_t mu_oct_key(void);
mu_pure mu_t mu_oct_bfn(void);
#define MU_HEX_KEY mu_hex_key()
#define MU_HEX_BFN mu_hex_bfn()
mu_pure mu_t mu_hex_key(void);
mu_pure mu_t mu_hex_bfn(void);

#define MU_LEN_KEY mu_len_key()
#define MU_LEN_BFN mu_len_bfn()
mu_pure mu_t mu_len_key(void);
mu_pure mu_t mu_len_bfn(void);
#define MU_TAIL_KEY mu_tail_key()
#define MU_TAIL_BFN mu_tail_bfn()
mu_pure mu_t mu_tail_key(void);
mu_pure mu_t mu_tail_bfn(void);
#define MU_PUSH_KEY mu_push_key()
#define MU_PUSH_BFN mu_push_bfn()
mu_pure mu_t mu_push_key(void);
mu_pure mu_t mu_push_bfn(void);
#define MU_POP_KEY mu_pop_key()
#define MU_POP_BFN mu_pop_bfn()
mu_pure mu_t mu_pop_key(void);
mu_pure mu_t mu_pop_bfn(void);
#define MU_CONCAT_KEY mu_concat_key()
#define MU_CONCAT_BFN mu_concat_bfn()
mu_pure mu_t mu_concat_key(void);
mu_pure mu_t mu_concat_bfn(void);
#define MU_SUBSET_KEY mu_subset_key()
#define MU_SUBSET_BFN mu_subset_bfn()
mu_pure mu_t mu_subset_key(void);
mu_pure mu_t mu_subset_bfn(void);

#define MU_FIND_KEY mu_find_key()
#define MU_FIND_BFN mu_find_bfn()
mu_pure mu_t mu_find_key(void);
mu_pure mu_t mu_find_bfn(void);
#define MU_REPLACE_KEY mu_replace_key()
#define MU_REPLACE_BFN mu_replace_bfn()
mu_pure mu_t mu_replace_key(void);
mu_pure mu_t mu_replace_bfn(void);
#define MU_SPLIT_KEY mu_split_key()
#define MU_SPLIT_BFN mu_split_bfn()
mu_pure mu_t mu_split_key(void);
mu_pure mu_t mu_split_bfn(void);
#define MU_JOIN_KEY mu_join_key()
#define MU_JOIN_BFN mu_join_bfn()
mu_pure mu_t mu_join_key(void);
mu_pure mu_t mu_join_bfn(void);
#define MU_PAD_KEY mu_pad_key()
#define MU_PAD_BFN mu_pad_bfn()
mu_pure mu_t mu_pad_key(void);
mu_pure mu_t mu_pad_bfn(void);
#define MU_STRIP_KEY mu_strip_key()
#define MU_STRIP_BFN mu_strip_bfn()
mu_pure mu_t mu_strip_key(void);
mu_pure mu_t mu_strip_bfn(void);

#define MU_BIND_KEY mu_bind_key()
#define MU_BIND_BFN mu_bind_bfn()
mu_pure mu_t mu_bind_key(void);
mu_pure mu_t mu_bind_bfn(void);
#define MU_COMP_KEY mu_comp_key()
#define MU_COMP_BFN mu_comp_bfn()
mu_pure mu_t mu_comp_key(void);
mu_pure mu_t mu_comp_bfn(void);
#define MU_MAP_KEY mu_map_key()
#define MU_MAP_BFN mu_map_bfn()
mu_pure mu_t mu_map_key(void);
mu_pure mu_t mu_map_bfn(void);
#define MU_FILTER_KEY mu_filter_key()
#define MU_FILTER_BFN mu_filter_bfn()
mu_pure mu_t mu_filter_key(void);
mu_pure mu_t mu_filter_bfn(void);
#define MU_REDUCE_KEY mu_reduce_key()
#define MU_REDUCE_BFN mu_reduce_bfn()
mu_pure mu_t mu_reduce_key(void);
mu_pure mu_t mu_reduce_bfn(void);
#define MU_ANY_KEY mu_any_key()
#define MU_ANY_BFN mu_any_bfn()
mu_pure mu_t mu_any_key(void);
mu_pure mu_t mu_any_bfn(void);
#define MU_ALL_KEY mu_all_key()
#define MU_ALL_BFN mu_all_bfn()
mu_pure mu_t mu_all_key(void);
mu_pure mu_t mu_all_bfn(void);

#define MU_ITER_KEY mu_iter_key()
#define MU_ITER_BFN mu_iter_bfn()
mu_pure mu_t mu_iter_key(void);
mu_pure mu_t mu_iter_bfn(void);
#define MU_PAIRS_KEY mu_pairs_key()
#define MU_PAIRS_BFN mu_pairs_bfn()
mu_pure mu_t mu_pairs_key(void);
mu_pure mu_t mu_pairs_bfn(void);
#define MU_RANGE_KEY mu_range_key()
#define MU_RANGE_BFN mu_range_bfn()
mu_pure mu_t mu_range_key(void);
mu_pure mu_t mu_range_bfn(void);
#define MU_REPEAT_KEY mu_repeat_key()
#define MU_REPEAT_BFN mu_repeat_bfn()
mu_pure mu_t mu_repeat_key(void);
mu_pure mu_t mu_repeat_bfn(void);
#define MU_SEED_KEY mu_seed_key()
#define MU_SEED_BFN mu_seed_bfn()
mu_pure mu_t mu_seed_key(void);
mu_pure mu_t mu_seed_bfn(void);

#define MU_ZIP_KEY mu_zip_key()
#define MU_ZIP_BFN mu_zip_bfn()
mu_pure mu_t mu_zip_key(void);
mu_pure mu_t mu_zip_bfn(void);
#define MU_CHAIN_KEY mu_chain_key()
#define MU_CHAIN_BFN mu_chain_bfn()
mu_pure mu_t mu_chain_key(void);
mu_pure mu_t mu_chain_bfn(void);
#define MU_TAKE_KEY mu_take_key()
#define MU_TAKE_BFN mu_take_bfn()
mu_pure mu_t mu_take_key(void);
mu_pure mu_t mu_take_bfn(void);
#define MU_DROP_KEY mu_drop_key()
#define MU_DROP_BFN mu_drop_bfn()
mu_pure mu_t mu_drop_key(void);
mu_pure mu_t mu_drop_bfn(void);

#define MU_MIN_KEY mu_min_key()
#define MU_MIN_BFN mu_min_bfn()
mu_pure mu_t mu_min_key(void);
mu_pure mu_t mu_min_bfn(void);
#define MU_MAX_KEY mu_max_key()
#define MU_MAX_BFN mu_max_bfn()
mu_pure mu_t mu_max_key(void);
mu_pure mu_t mu_max_bfn(void);
#define MU_REVERSE_KEY mu_reverse_key()
#define MU_REVERSE_BFN mu_reverse_bfn()
mu_pure mu_t mu_reverse_key(void);
mu_pure mu_t mu_reverse_bfn(void);
#define MU_SORT_KEY mu_sort_key()
#define MU_SORT_BFN mu_sort_bfn()
mu_pure mu_t mu_sort_key(void);
mu_pure mu_t mu_sort_bfn(void);

#define MU_ERROR_KEY mu_error_key()
#define MU_ERROR_BFN mu_error_bfn()
mu_pure mu_t mu_error_key(void);
mu_pure mu_t mu_error_bfn(void);
#define MU_PRINT_KEY mu_print_key()
#define MU_PRINT_BFN mu_print_bfn()
mu_pure mu_t mu_print_key(void);
mu_pure mu_t mu_print_bfn(void);
#define MU_IMPORT_KEY mu_import_key()
#define MU_IMPORT_BFN mu_import_bfn()
mu_pure mu_t mu_import_key(void);
mu_pure mu_t mu_import_bfn(void);


#endif
