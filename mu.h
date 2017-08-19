/*
 * The Mu scripting language
 *
 * Copyright (c) 2016 Christopher Haster
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef MU_H
#define MU_H
#include "config.h"
#include "types.h"
#include "sys.h"
#include "num.h"
#include "buf.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"
#include "parse.h"
#include "vm.h"


// Manual memory management
// simple wrapper over malloc and free if available
// returns 0 when size == 0
void *mu_alloc(muint_t size);
void mu_dealloc(void *, muint_t size);

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

// Checks for various conditions
// using a macro here helps pathing and avoids cost of pushing args
#define mu_checkargs(pred, ...) \
    ((pred) ? (void)0 : mu_errorargs(__VA_ARGS__))
mu_noreturn mu_errorargs(mu_t name, mcnt_t fc, mu_t *frame);
#define mu_checkconst(pred, ...) \
    ((pred) ? (void)0 : mu_errorro(__VA_ARGS__))
mu_noreturn mu_errorro(const char *name);
#define mu_checklen(pred, ...) \
    ((pred) ? (void)0 : mu_errorlen(__VA_ARGS__))
mu_noreturn mu_errorlen(const char *name);

// Declaration of mu constants, requires other MU_DEF_* for definition
#define MU_DEF(name) \
extern mu_pure mu_t name(void);


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
#define MU_CONST        mu_const_def()
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
#define MU_FN_KEY      mu_fn_key_def()

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

#define MU_AND_KEY     mu_and_key_def()
#define MU_OR_KEY      mu_or_key_def()
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
#define MU_CONST_KEY    mu_const_key_def()
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
MU_DEF(mu_builtins_def)

MU_DEF(mu_true_def)
MU_DEF(mu_inf_def)
MU_DEF(mu_ninf_def)
MU_DEF(mu_e_def)
MU_DEF(mu_pi_def)

MU_DEF(mu_num_def)
MU_DEF(mu_str_def)
MU_DEF(mu_tbl_def)
MU_DEF(mu_fn_def)

MU_DEF(mu_not_def)
MU_DEF(mu_eq_def)
MU_DEF(mu_neq_def)
MU_DEF(mu_is_def)
MU_DEF(mu_lt_def)
MU_DEF(mu_lte_def)
MU_DEF(mu_gt_def)
MU_DEF(mu_gte_def)

MU_DEF(mu_add_def)
MU_DEF(mu_sub_def)
MU_DEF(mu_mul_def)
MU_DEF(mu_div_def)
MU_DEF(mu_abs_def)
MU_DEF(mu_floor_def)
MU_DEF(mu_ceil_def)
MU_DEF(mu_idiv_def)
MU_DEF(mu_mod_def)
MU_DEF(mu_pow_def)
MU_DEF(mu_log_def)

MU_DEF(mu_cos_def)
MU_DEF(mu_acos_def)
MU_DEF(mu_sin_def)
MU_DEF(mu_asin_def)
MU_DEF(mu_tan_def)
MU_DEF(mu_atan_def)

MU_DEF(mu_and_def)
MU_DEF(mu_or_def)
MU_DEF(mu_xor_def)
MU_DEF(mu_diff_def)
MU_DEF(mu_shl_def)
MU_DEF(mu_shr_def)

MU_DEF(mu_parse_def)
MU_DEF(mu_repr_def)
MU_DEF(mu_bin_def)
MU_DEF(mu_oct_def)
MU_DEF(mu_hex_def)

MU_DEF(mu_len_def)
MU_DEF(mu_tail_def)
MU_DEF(mu_push_def)
MU_DEF(mu_pop_def)
MU_DEF(mu_concat_def)
MU_DEF(mu_subset_def)

MU_DEF(mu_find_def)
MU_DEF(mu_replace_def)
MU_DEF(mu_split_def)
MU_DEF(mu_join_def)
MU_DEF(mu_pad_def)
MU_DEF(mu_strip_def)

MU_DEF(mu_bind_def)
MU_DEF(mu_comp_def)
MU_DEF(mu_map_def)
MU_DEF(mu_filter_def)
MU_DEF(mu_reduce_def)
MU_DEF(mu_any_def)
MU_DEF(mu_all_def)

MU_DEF(mu_iter_def)
MU_DEF(mu_pairs_def)
MU_DEF(mu_range_def)
MU_DEF(mu_repeat_def)
MU_DEF(mu_random_def)

MU_DEF(mu_zip_def)
MU_DEF(mu_chain_def)
MU_DEF(mu_take_def)
MU_DEF(mu_drop_def)

MU_DEF(mu_min_def)
MU_DEF(mu_max_def)
MU_DEF(mu_reverse_def)
MU_DEF(mu_sort_def)

MU_DEF(mu_error_def)
MU_DEF(mu_print_def)
MU_DEF(mu_import_def)

MU_DEF(mu_true_key_def)
MU_DEF(mu_false_key_def)
MU_DEF(mu_inf_key_def)
MU_DEF(mu_ninf_key_def)
MU_DEF(mu_e_key_def)
MU_DEF(mu_pi_key_def)

MU_DEF(mu_num_key_def)
MU_DEF(mu_str_key_def)
MU_DEF(mu_tbl_key_def)
MU_DEF(mu_fn_key_def)

MU_DEF(mu_not_key_def)
MU_DEF(mu_eq_key_def)
MU_DEF(mu_neq_key_def)
MU_DEF(mu_is_key_def)
MU_DEF(mu_lt_key_def)
MU_DEF(mu_lte_key_def)
MU_DEF(mu_gt_key_def)
MU_DEF(mu_gte_key_def)

MU_DEF(mu_add_key_def)
MU_DEF(mu_sub_key_def)
MU_DEF(mu_mul_key_def)
MU_DEF(mu_div_key_def)
MU_DEF(mu_abs_key_def)
MU_DEF(mu_floor_key_def)
MU_DEF(mu_ceil_key_def)
MU_DEF(mu_idiv_key_def)
MU_DEF(mu_mod_key_def)
MU_DEF(mu_pow_key_def)
MU_DEF(mu_log_key_def)

MU_DEF(mu_cos_key_def)
MU_DEF(mu_acos_key_def)
MU_DEF(mu_sin_key_def)
MU_DEF(mu_asin_key_def)
MU_DEF(mu_tan_key_def)
MU_DEF(mu_atan_key_def)

MU_DEF(mu_and_key_def)
MU_DEF(mu_or_key_def)
MU_DEF(mu_xor_key_def)
MU_DEF(mu_diff_key_def)
MU_DEF(mu_shl_key_def)
MU_DEF(mu_shr_key_def)

MU_DEF(mu_parse_key_def)
MU_DEF(mu_repr_key_def)
MU_DEF(mu_ord_key_def)
MU_DEF(mu_chr_key_def)
MU_DEF(mu_bin_key_def)
MU_DEF(mu_oct_key_def)
MU_DEF(mu_hex_key_def)

MU_DEF(mu_len_key_def)
MU_DEF(mu_tail_key_def)
MU_DEF(mu_push_key_def)
MU_DEF(mu_pop_key_def)
MU_DEF(mu_concat_key_def)
MU_DEF(mu_subset_key_def)

MU_DEF(mu_find_key_def)
MU_DEF(mu_replace_key_def)
MU_DEF(mu_split_key_def)
MU_DEF(mu_join_key_def)
MU_DEF(mu_pad_key_def)
MU_DEF(mu_strip_key_def)

MU_DEF(mu_bind_key_def)
MU_DEF(mu_comp_key_def)
MU_DEF(mu_map_key_def)
MU_DEF(mu_filter_key_def)
MU_DEF(mu_reduce_key_def)
MU_DEF(mu_any_key_def)
MU_DEF(mu_all_key_def)

MU_DEF(mu_iter_key_def)
MU_DEF(mu_pairs_key_def)
MU_DEF(mu_range_key_def)
MU_DEF(mu_repeat_key_def)
MU_DEF(mu_random_key_def)

MU_DEF(mu_zip_key_def)
MU_DEF(mu_chain_key_def)
MU_DEF(mu_take_key_def)
MU_DEF(mu_drop_key_def)

MU_DEF(mu_min_key_def)
MU_DEF(mu_max_key_def)
MU_DEF(mu_reverse_key_def)
MU_DEF(mu_sort_key_def)

MU_DEF(mu_error_key_def)
MU_DEF(mu_print_key_def)
MU_DEF(mu_import_key_def)


#endif
