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


// Access to type and deferal components
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
mu_inline mlen_t mu_frame_count(mcnt_t fc) {
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

// Check arguments to function
// using a macro here helps pathing and avoids cost of pushing args
#define mu_checkargs(pred, ...) \
    ((pred) ? (void)0 : mu_errorargs(__VA_ARGS__))
mu_noreturn mu_errorargs(mu_t name, mcnt_t fc, mu_t *frame);


// Standard functions in readonly builtins table
#define MU_BUILTINS     mu_builtins_def()

// Builtin constants
#define MU_NIL          ((mu_t)0)

#define MU_TRUE         mu_true_def()
#define MU_FALSE        MU_NIL

#define MU_INF          mu_inf_def()
#define MU_NINF         mu_ninf_def()
#define MU_E            mu_e_def()
#define MU_PI           mu_pi_def()

// Builtin functions
#define MU_NUM          mu_num_def()
#define MU_STR          mu_str_def()
#define MU_TBL          mu_tbl_def()
#define MU_FN           mu_fn_def()

#define MU_NOT          mu_not_def()
#define MU_EQ           mu_eq_def()
#define MU_NEQ          mu_neq_def()
#define MU_IS           mu_is_def()
#define MU_LT           mu_lt_def()
#define MU_LTE          mu_lte_def()
#define MU_GT           mu_gt_def()
#define MU_GTE          mu_gte_def()

#define MU_ADD          mu_add_def()
#define MU_SUB          mu_sub_def()
#define MU_MUL          mu_mul_def()
#define MU_DIV          mu_div_def()
#define MU_IDIV         mu_idiv_def()
#define MU_MOD          mu_mod_def()
#define MU_POW          mu_pow_def()
#define MU_LOG          mu_log_def()
#define MU_ABS          mu_abs_def()
#define MU_FLOOR        mu_floor_def()
#define MU_CEIL         mu_ceil_def()

#define MU_COS          mu_cos_def()
#define MU_ACOS         mu_acos_def()
#define MU_SIN          mu_sin_def()
#define MU_ASIN         mu_asin_def()
#define MU_TAN          mu_tan_def()
#define MU_ATAN         mu_atan_def()

#define MU_AND          mu_and_def()
#define MU_OR           mu_or_def()
#define MU_XOR          mu_xor_def()
#define MU_DIFF         mu_diff_def()
#define MU_SHL          mu_shl_def()
#define MU_SHR          mu_shr_def()

#define MU_PARSE        mu_parse_def()
#define MU_REPR         mu_repr_def()
#define MU_ORD          mu_ord_def()
#define MU_CHR          mu_chr_def()
#define MU_BIN          mu_bin_def()
#define MU_OCT          mu_oct_def()
#define MU_HEX          mu_hex_def()

#define MU_LEN          mu_len_def()
#define MU_TAIL         mu_tail_def()
#define MU_PUSH         mu_push_def()
#define MU_POP          mu_pop_def()
#define MU_CONCAT       mu_concat_def()
#define MU_SUBSET       mu_subset_def()

#define MU_FIND         mu_find_def()
#define MU_REPLACE      mu_replace_def()
#define MU_SPLIT        mu_split_def()
#define MU_JOIN         mu_join_def()
#define MU_PAD          mu_pad_def()
#define MU_STRIP        mu_strip_def()

#define MU_BIND         mu_bind_def()
#define MU_COMP         mu_comp_def()
#define MU_MAP          mu_map_def()
#define MU_FILTER       mu_filter_def()
#define MU_REDUCE       mu_reduce_def()
#define MU_ANY          mu_any_def()
#define MU_ALL          mu_all_def()

#define MU_ITER         mu_iter_def()
#define MU_PAIRS        mu_pairs_def()
#define MU_RANGE        mu_range_def()
#define MU_REPEAT       mu_repeat_def()
#define MU_RANDOM       mu_random_def()

#define MU_ZIP          mu_zip_def()
#define MU_CHAIN        mu_chain_def()
#define MU_TAKE         mu_take_def()
#define MU_DROP         mu_drop_def()

#define MU_MIN          mu_min_def()
#define MU_MAX          mu_max_def()
#define MU_REVERSE      mu_reverse_def()
#define MU_SORT         mu_sort_def()

#define MU_ERROR        mu_error_def()
#define MU_PRINT        mu_print_def()
#define MU_IMPORT       mu_import_def()

// Builtin keys
#define MU_TRUE_KEY     mu_true_key_def()
#define MU_FALSE_KEY    mu_false_key_def()
#define MU_INF_KEY      mu_inf_key_def()
#define MU_E_KEY        mu_e_key_def()
#define MU_PI_KEY       mu_pi_key_def()

#define MU_NUM_KEY      mu_num_key_def()
#define MU_STR_KEY      mu_str_key_def()
#define MU_TBL_KEY      mu_tbl_key_def()
#define MU_KEY_FN2      mu_def_key_fn2()

#define MU_NOT_KEY      mu_not_key_def()
#define MU_EQ_KEY       mu_eq_key_def()
#define MU_NEQ_KEY      mu_neq_key_def()
#define MU_IS_KEY       mu_is_key_def()
#define MU_LT_KEY       mu_lt_key_def()
#define MU_LTE_KEY      mu_lte_key_def()
#define MU_GT_KEY       mu_gt_key_def()
#define MU_GTE_KEY      mu_gte_key_def()

#define MU_ADD_KEY      mu_add_key_def()
#define MU_SUB_KEY      mu_sub_key_def()
#define MU_MUL_KEY      mu_mul_key_def()
#define MU_DIV_KEY      mu_div_key_def()
#define MU_ABS_KEY      mu_abs_key_def()
#define MU_FLOOR_KEY    mu_floor_key_def()
#define MU_CEIL_KEY     mu_ceil_key_def()
#define MU_IDIV_KEY     mu_idiv_key_def()
#define MU_MOD_KEY      mu_mod_key_def()
#define MU_POW_KEY      mu_pow_key_def()
#define MU_LOG_KEY      mu_log_key_def()

#define MU_COS_KEY      mu_cos_key_def()
#define MU_ACOS_KEY     mu_acos_key_def()
#define MU_SIN_KEY      mu_sin_key_def()
#define MU_ASIN_KEY     mu_asin_key_def()
#define MU_TAN_KEY      mu_tan_key_def()
#define MU_ATAN_KEY     mu_atan_key_def()

#define MU_KEY_AND2     mu_def_key_and2()
#define MU_KEY_OR2      mu_def_key_or2()
#define MU_XOR_KEY      mu_xor_key_def()
#define MU_DIFF_KEY     mu_diff_key_def()
#define MU_SHL_KEY      mu_shl_key_def()
#define MU_SHR_KEY      mu_shr_key_def()

#define MU_PARSE_KEY    mu_parse_key_def()
#define MU_REPR_KEY     mu_repr_key_def()
#define MU_ORD_KEY      mu_ord_key_def()
#define MU_CHR_KEY      mu_chr_key_def()
#define MU_BIN_KEY      mu_bin_key_def()
#define MU_OCT_KEY      mu_oct_key_def()
#define MU_HEX_KEY      mu_hex_key_def()

#define MU_LEN_KEY      mu_len_key_def()
#define MU_TAIL_KEY     mu_tail_key_def()
#define MU_PUSH_KEY     mu_push_key_def()
#define MU_POP_KEY      mu_pop_key_def()
#define MU_CONCAT_KEY   mu_concat_key_def()
#define MU_SUBSET_KEY   mu_subset_key_def()

#define MU_FIND_KEY     mu_find_key_def()
#define MU_REPLACE_KEY  mu_replace_key_def()
#define MU_SPLIT_KEY    mu_split_key_def()
#define MU_JOIN_KEY     mu_join_key_def()
#define MU_PAD_KEY      mu_pad_key_def()
#define MU_STRIP_KEY    mu_strip_key_def()

#define MU_BIND_KEY     mu_bind_key_def()
#define MU_COMP_KEY     mu_comp_key_def()
#define MU_MAP_KEY      mu_map_key_def()
#define MU_FILTER_KEY   mu_filter_key_def()
#define MU_REDUCE_KEY   mu_reduce_key_def()
#define MU_ANY_KEY      mu_any_key_def()
#define MU_ALL_KEY      mu_all_key_def()

#define MU_ITER_KEY     mu_iter_key_def()
#define MU_PAIRS_KEY    mu_pairs_key_def()
#define MU_RANGE_KEY    mu_range_key_def()
#define MU_REPEAT_KEY   mu_repeat_key_def()
#define MU_RANDOM_KEY   mu_random_key_def()

#define MU_ZIP_KEY      mu_zip_key_def()
#define MU_CHAIN_KEY    mu_chain_key_def()
#define MU_TAKE_KEY     mu_take_key_def()
#define MU_DROP_KEY     mu_drop_key_def()

#define MU_MIN_KEY      mu_min_key_def()
#define MU_MAX_KEY      mu_max_key_def()
#define MU_REVERSE_KEY  mu_reverse_key_def()
#define MU_SORT_KEY     mu_sort_key_def()

#define MU_ERROR_KEY    mu_error_key_def()
#define MU_PRINT_KEY    mu_print_key_def()
#define MU_IMPORT_KEY   mu_import_key_def()


// Builtin deferating functions
mu_t mu_builtins_def(void);

mu_t mu_true_def(void);
mu_t mu_inf_def(void);
mu_t mu_ninf_def(void);
mu_t mu_e_def(void);
mu_t mu_pi_def(void);

mu_t mu_num_def(void);
mu_t mu_str_def(void);
mu_t mu_tbl_def(void);
mu_t mu_fn_def(void);

mu_t mu_not_def(void);
mu_t mu_eq_def(void);
mu_t mu_neq_def(void);
mu_t mu_is_def(void);
mu_t mu_lt_def(void);
mu_t mu_lte_def(void);
mu_t mu_gt_def(void);
mu_t mu_gte_def(void);

mu_t mu_add_def(void);
mu_t mu_sub_def(void);
mu_t mu_mul_def(void);
mu_t mu_div_def(void);
mu_t mu_abs_def(void);
mu_t mu_floor_def(void);
mu_t mu_ceil_def(void);
mu_t mu_idiv_def(void);
mu_t mu_mod_def(void);
mu_t mu_pow_def(void);
mu_t mu_log_def(void);

mu_t mu_cos_def(void);
mu_t mu_acos_def(void);
mu_t mu_sin_def(void);
mu_t mu_asin_def(void);
mu_t mu_tan_def(void);
mu_t mu_atan_def(void);

mu_t mu_and_def(void);
mu_t mu_or_def(void);
mu_t mu_xor_def(void);
mu_t mu_diff_def(void);
mu_t mu_shl_def(void);
mu_t mu_shr_def(void);

mu_t mu_parse_def(void);
mu_t mu_repr_def(void);
mu_t mu_bin_def(void);
mu_t mu_oct_def(void);
mu_t mu_hex_def(void);

mu_t mu_len_def(void);
mu_t mu_tail_def(void);
mu_t mu_push_def(void);
mu_t mu_pop_def(void);
mu_t mu_concat_def(void);
mu_t mu_subset_def(void);

mu_t mu_find_def(void);
mu_t mu_replace_def(void);
mu_t mu_split_def(void);
mu_t mu_join_def(void);
mu_t mu_pad_def(void);
mu_t mu_strip_def(void);

mu_t mu_bind_def(void);
mu_t mu_comp_def(void);
mu_t mu_map_def(void);
mu_t mu_filter_def(void);
mu_t mu_reduce_def(void);
mu_t mu_any_def(void);
mu_t mu_all_def(void);

mu_t mu_iter_def(void);
mu_t mu_pairs_def(void);
mu_t mu_range_def(void);
mu_t mu_repeat_def(void);
mu_t mu_random_def(void);

mu_t mu_zip_def(void);
mu_t mu_chain_def(void);
mu_t mu_take_def(void);
mu_t mu_drop_def(void);

mu_t mu_min_def(void);
mu_t mu_max_def(void);
mu_t mu_reverse_def(void);
mu_t mu_sort_def(void);

mu_t mu_error_def(void);
mu_t mu_print_def(void);
mu_t mu_import_def(void);

mu_t mu_true_key_def(void);
mu_t mu_false_key_def(void);
mu_t mu_inf_key_def(void);
mu_t mu_ninf_key_def(void);
mu_t mu_e_key_def(void);
mu_t mu_pi_key_def(void);

mu_t mu_num_key_def(void);
mu_t mu_str_key_def(void);
mu_t mu_tbl_key_def(void);
mu_t mu_def_key_fn2(void);

mu_t mu_not_key_def(void);
mu_t mu_eq_key_def(void);
mu_t mu_neq_key_def(void);
mu_t mu_is_key_def(void);
mu_t mu_lt_key_def(void);
mu_t mu_lte_key_def(void);
mu_t mu_gt_key_def(void);
mu_t mu_gte_key_def(void);

mu_t mu_add_key_def(void);
mu_t mu_sub_key_def(void);
mu_t mu_mul_key_def(void);
mu_t mu_div_key_def(void);
mu_t mu_abs_key_def(void);
mu_t mu_floor_key_def(void);
mu_t mu_ceil_key_def(void);
mu_t mu_idiv_key_def(void);
mu_t mu_mod_key_def(void);
mu_t mu_pow_key_def(void);
mu_t mu_log_key_def(void);

mu_t mu_cos_key_def(void);
mu_t mu_acos_key_def(void);
mu_t mu_sin_key_def(void);
mu_t mu_asin_key_def(void);
mu_t mu_tan_key_def(void);
mu_t mu_atan_key_def(void);

mu_t mu_def_key_and2(void);
mu_t mu_def_key_or2(void);
mu_t mu_xor_key_def(void);
mu_t mu_diff_key_def(void);
mu_t mu_shl_key_def(void);
mu_t mu_shr_key_def(void);

mu_t mu_parse_key_def(void);
mu_t mu_repr_key_def(void);
mu_t mu_ord_key_def(void);
mu_t mu_chr_key_def(void);
mu_t mu_bin_key_def(void);
mu_t mu_oct_key_def(void);
mu_t mu_hex_key_def(void);

mu_t mu_len_key_def(void);
mu_t mu_tail_key_def(void);
mu_t mu_push_key_def(void);
mu_t mu_pop_key_def(void);
mu_t mu_concat_key_def(void);
mu_t mu_subset_key_def(void);

mu_t mu_find_key_def(void);
mu_t mu_replace_key_def(void);
mu_t mu_split_key_def(void);
mu_t mu_join_key_def(void);
mu_t mu_pad_key_def(void);
mu_t mu_strip_key_def(void);

mu_t mu_bind_key_def(void);
mu_t mu_comp_key_def(void);
mu_t mu_map_key_def(void);
mu_t mu_filter_key_def(void);
mu_t mu_reduce_key_def(void);
mu_t mu_any_key_def(void);
mu_t mu_all_key_def(void);

mu_t mu_iter_key_def(void);
mu_t mu_pairs_key_def(void);
mu_t mu_range_key_def(void);
mu_t mu_repeat_key_def(void);
mu_t mu_random_key_def(void);

mu_t mu_zip_key_def(void);
mu_t mu_chain_key_def(void);
mu_t mu_take_key_def(void);
mu_t mu_drop_key_def(void);

mu_t mu_min_key_def(void);
mu_t mu_max_key_def(void);
mu_t mu_reverse_key_def(void);
mu_t mu_sort_key_def(void);

mu_t mu_error_key_def(void);
mu_t mu_print_key_def(void);
mu_t mu_import_key_def(void);


#endif
