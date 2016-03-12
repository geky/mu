#include "mu.h"

#include "sys.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"
#include "parse.h"


// Constants
MUINT(mu_true, 1)


// Common errors
mu_noreturn mu_error_args(mu_t name, mc_t count, mu_t *args) {
    mu_t message = buf_create(0);
    muint_t n = 0;
    buf_format(&message, &n, "invalid argument in %m(", name);

    for (muint_t i = 0; i < count; i++) {
        buf_concat(&message, &n, mu_repr(args[i]));

        if (i != count-1) {
            buf_format(&message, &n, ", ");
        } else {
            buf_format(&message, &n, ")");
        }
    }

    mu_errorf("%ns", buf_data(message), n);
}

#define mu_error_arg1(name, ...) mu_error_args(name, 1, (mu_t[1]){__VA_ARGS__})
#define mu_error_arg2(name, ...) mu_error_args(name, 2, (mu_t[2]){__VA_ARGS__})
#define mu_error_arg3(name, ...) mu_error_args(name, 3, (mu_t[3]){__VA_ARGS__})
#define mu_error_arg4(name, ...) mu_error_args(name, 4, (mu_t[4]){__VA_ARGS__})

static mu_noreturn mu_error_ops(mu_t name, mc_t count, mu_t *args) {
    if (count == 1) {
        mu_errorf("unsupported operation %m%r", name, args[0]);
    } else {
        mu_errorf("unsupported operation %r %m %r", args[0], name, args[1]);
    }
}

#define mu_error_op1(name, ...) mu_error_ops(name, 1, (mu_t[1]){__VA_ARGS__})
#define mu_error_op2(name, ...) mu_error_ops(name, 2, (mu_t[2]){__VA_ARGS__})

static mu_noreturn mu_error_convert(mu_t name, mu_t m) {
    mu_errorf("unable to convert %r to %m", m, name);
}


// Conversion between different frame types
void mu_fconvert(mc_t dc, mc_t sc, mu_t *frame) {
    if (dc != 0xf && sc != 0xf) {
        for (muint_t i = dc; i < sc; i++) {
            mu_dec(frame[i]);
        }

        for (muint_t i = sc; i < dc; i++) {
            frame[i] = 0;
        }
    } else if (dc != 0xf) {
        mu_t t = *frame;

        for (muint_t i = 0; i < dc; i++) {
            frame[i] = tbl_lookup(t, muint(i));
        }

        tbl_dec(t);
    } else if (sc != 0xf) {
        mu_t t = tbl_create(sc);

        for (muint_t i = 0; i < sc; i++) {
            tbl_insert(t, muint(i), frame[i]);
        }

        *frame = t;
    }
}


// Destructor table
extern void str_destroy(mu_t);
extern void buf_destroy(mu_t);
extern void tbl_destroy(mu_t);
extern void fn_destroy(mu_t);

void (*const mu_destroy_table[4])(mu_t) = {
    [MTSTR-4]  = str_destroy,
    [MTBUF-4]  = buf_destroy,
    [MTTBL-4]  = tbl_destroy,
    [MTFN-4]   = fn_destroy,
};


// Table related functions performed on variables
mu_t mu_lookup(mu_t m, mu_t k) {
    if (!mu_istbl(m)) {
        mu_errorf("unable to lookup %r in %r", k, m);
    }

    return tbl_lookup(m, k);
}

void mu_insert(mu_t m, mu_t k, mu_t v) {
    if (!mu_istbl(m)) {
        mu_errorf("unable to insert %r to %r in %r", v, k, m);
    }

    return tbl_insert(m, k, v);
}

void mu_assign(mu_t m, mu_t k, mu_t v) {
    if (!mu_istbl(m)) {
        mu_errorf("unable to assign %r to %r in %r", v, k, m);
    }

    return tbl_assign(m, k, v);
}

// Function calls performed on variables
mc_t mu_tcall(mu_t m, mc_t fc, mu_t *frame) {
    if (!mu_isfn(m)) {
        mu_errorf("unable to call %r", m);
    }

    return fn_tcall(m, fc, frame);
}

void mu_fcall(mu_t m, mc_t fc, mu_t *frame) {
    mc_t rets = mu_tcall(mu_inc(m), fc >> 4, frame);
    mu_fconvert(0xf & fc, rets, frame);
}

mu_t mu_vcall(mu_t m, mc_t fc, va_list args) {
    mu_t frame[MU_FRAME];

    for (muint_t i = 0; i < mu_fcount(0xf & fc); i++) {
        frame[i] = va_arg(args, mu_t);
    }

    mu_fcall(m, fc, frame);

    for (muint_t i = 1; i < mu_fcount(fc >> 4); i++) {
        *va_arg(args, mu_t *) = frame[i];
    }

    return (fc >> 4) ? *frame : 0;
}

mu_t mu_call(mu_t m, mc_t fc, ...) {
    va_list args;
    va_start(args, fc);
    mu_t ret = mu_vcall(m, fc, args);
    va_end(args);
    return ret;
}


// Type casts and declarations
static mc_t mu_num_thunk(mu_t *frame);
MSTR(mu_num_key, "num")
MBFN(mu_num_bfn, 0x1, mu_num_thunk);
static mc_t mu_num_thunk(mu_t *frame) {
    frame[0] = mu_num(frame[0]);
    return 1;
}

mu_t mu_num(mu_t m) {
    switch (mu_type(m)) {
        case MTNIL:     return muint(0);
        case MTNUM:     return m;
        case MTSTR:     if (str_len(m) == 1) {
                            return num_fromstr(m);
                        }
                        break;
        default:        break;
    }

    mu_error_convert(MU_NUM_KEY, m);
}

static mc_t mu_str_thunk(mu_t *frame);
MSTR(mu_str_key, "str")
MBFN(mu_str_bfn, 0x1, mu_str_thunk);
static mc_t mu_str_thunk(mu_t *frame) {
    frame[0] = mu_str(frame[0]);
    return 1;
}

mu_t mu_str(mu_t m) {
    switch (mu_type(m)) {
        case MTNIL:     return mstr("");
        case MTNUM:     if (m == muint((mbyte_t)num_uint(m))) {
                            return str_fromnum(m);
                        }
                        break;
        case MTSTR:     return m;
        case MTTBL:
        case MTFN:      return str_fromiter(mu_iter(m));
        default:        mu_unreachable;
    }

    mu_error_convert(MU_STR_KEY, m);
}

static mc_t mu_tbl_thunk(mu_t *frame);
MSTR(mu_tbl_key, "tbl")
MBFN(mu_tbl_bfn, 0x2, mu_tbl_thunk)
static mc_t mu_tbl_thunk(mu_t *frame) {
    frame[0] = mu_tbl(frame[0], frame[1]);
    return 1;
}

mu_t mu_tbl(mu_t m, mu_t tail) {
    if (tail && !mu_istbl(tail)) {
        mu_error_arg2(MU_TBL_KEY, m, tail);
    }

    mu_t t;
    switch (mu_type(m)) {
        case MTNIL:     t = tbl_create(0); break;
        case MTNUM:     t = tbl_create(num_uint(m)); break;
        case MTSTR:     t = tbl_fromiter(mu_iter(m)); break;
        case MTTBL:     t = tbl_fromiter(mu_pairs(m)); break;
        case MTFN:      t = tbl_fromiter(m); break;
        default:        mu_unreachable;
    }

    tbl_settail(t, tail);
    return t;
}

static mc_t mu_fn_thunk(mu_t *frame);
MSTR(mu_fn_key, "fn_")
MBFN(mu_fn_bfn, 0x1, mu_fn_thunk)
static mc_t mu_fn_thunk(mu_t *frame) {
    frame[0] = mu_fn(frame[0]);
    return 1;
}

mu_t mu_fn(mu_t m) {
    switch (mu_type(m)) {
        case MTNIL:     return MU_ID;
        case MTFN:      return m;
        default:        break;
    }

    mu_error_convert(MU_FN_KEY, m);
}


// Logic operations
static mc_t mu_not_thunk(mu_t *frame);
MSTR(mu_not_key, "!")
MBFN(mu_not_bfn, 0x1, mu_not_thunk)
static mc_t mu_not_thunk(mu_t *frame) {
    mu_dec(frame[0]);
    frame[0] = !frame[0] ? MU_TRUE : MU_FALSE;
    return 1;
}

static mc_t mu_is_thunk(mu_t *frame);
MSTR(mu_is_key, "is")
MBFN(mu_is_bfn, 0x2, mu_is_thunk)
static mc_t mu_is_thunk(mu_t *frame) {
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = mu_is(frame[0], frame[1]) ? muint(1) : 0;
    return 1;
}

bool mu_is(mu_t a, mu_t type) {
    switch (mu_type(a)) {
        case MTNIL:     return !type;
        case MTNUM:     return type == MU_NUM_BFN;
        case MTSTR:     return type == MU_STR_BFN;
        case MTTBL:     return type == MU_TBL_BFN;
        case MTFN:      return type == MU_FN_BFN;
        default:        mu_unreachable;
    }
}

static mc_t mu_eq_thunk(mu_t *frame);
MSTR(mu_eq_key,  "==")
MBFN(mu_eq_bfn,  0x2, mu_eq_thunk)
static mc_t mu_eq_thunk(mu_t *frame) {
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = frame[0] == frame[1] ? MU_TRUE : MU_FALSE;
    return 1;
}

static mc_t mu_neq_thunk(mu_t *frame);
MSTR(mu_neq_key, "!=")
MBFN(mu_neq_bfn, 0x2, mu_neq_thunk)
static mc_t mu_neq_thunk(mu_t *frame) {
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = frame[0] != frame[1] ? MU_TRUE : MU_FALSE;
    return 1;
}

static mc_t mu_lt_thunk(mu_t *frame);
MSTR(mu_lt_key,  "<")
MBFN(mu_lt_bfn,  0x2, mu_lt_thunk)
static mc_t mu_lt_thunk(mu_t *frame) {
    mint_t cmp = mu_cmp(frame[0], frame[1]);
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = cmp < 0 ? MU_TRUE : MU_FALSE;
    return 1;
}

static mc_t mu_lte_thunk(mu_t *frame);
MSTR(mu_lte_key, "<=")
MBFN(mu_lte_bfn, 0x2, mu_lte_thunk)
static mc_t mu_lte_thunk(mu_t *frame) {
    mint_t cmp = mu_cmp(frame[0], frame[1]);
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = cmp <= 0 ? MU_TRUE : MU_FALSE;
    return 1;
}

static mc_t mu_gt_thunk(mu_t *frame);
MSTR(mu_gt_key,  ">")
MBFN(mu_gt_bfn,  0x2, mu_gt_thunk)
static mc_t mu_gt_thunk(mu_t *frame) {
    mint_t cmp = mu_cmp(frame[0], frame[1]);
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = cmp > 0 ? MU_TRUE : MU_FALSE;
    return 1;
}

static mc_t mu_gte_thunk(mu_t *frame);
MSTR(mu_gte_key, ">=")
MBFN(mu_gte_bfn, 0x2, mu_gte_thunk)
static mc_t mu_gte_thunk(mu_t *frame) {
    mint_t cmp = mu_cmp(frame[0], frame[1]);
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = cmp >= 0 ? MU_TRUE : MU_FALSE;
    return 1;
}

// does not consume
mint_t mu_cmp(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_cmp(a, b);
            case MTSTR:     return str_cmp(a, b);
            default:        break;
        }
    }

    mu_errorf("unable to compare %r and %r", a, b);
}


// Arithmetic operations
static mc_t mu_add_thunk(mu_t *frame);
MSTR(mu_add_key, "+")
MBFN(mu_add_bfn, 0x2, mu_add_thunk)
static mc_t mu_add_thunk(mu_t *frame) {
    if (!frame[1]) frame[0] = mu_pos(frame[0]);
    else           frame[0] = mu_add(frame[0], frame[1]);
    return 1;
}

mu_t mu_pos(mu_t a) {
    switch (mu_type(a)) {
        case MTNUM:     return a;
        default:        mu_error_op1(MU_ADD_KEY, a);
    }
}

mu_t mu_add(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_add(a, b);
            default:        break;
        }
    }

    mu_error_op2(MU_ADD_KEY, a, b);
}

static mc_t mu_sub_thunk(mu_t *frame);
MSTR(mu_sub_key, "-")
MBFN(mu_sub_bfn, 0x2, mu_sub_thunk)
static mc_t mu_sub_thunk(mu_t *frame) {
    if (!frame[1]) frame[0] = mu_neg(frame[0]);
    else           frame[0] = mu_sub(frame[0], frame[1]);
    return 1;
}

mu_t mu_neg(mu_t a) {
    switch (mu_type(a)) {
        case MTNUM:     return num_neg(a);
        default:        mu_error_op1(MU_SUB_KEY, a);
    }
}

mu_t mu_sub(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_sub(a, b);
            default:        break;
        }
    }

    mu_error_op2(MU_SUB_KEY, a, b);
}

static mc_t mu_mul_thunk(mu_t *frame);
MSTR(mu_mul_key, "*")
MBFN(mu_mul_bfn, 0x2, mu_mul_thunk)
static mc_t mu_mul_thunk(mu_t *frame) {
    frame[0] = mu_mul(frame[0], frame[1]);
    return 1;
}

mu_t mu_mul(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_mul(a, b);
            default:        break;
        }
    }

    mu_error_op2(MU_MUL_KEY, a, b);
}

static mc_t mu_div_thunk(mu_t *frame);
MSTR(mu_div_key, "/")
MBFN(mu_div_bfn, 0x2, mu_div_thunk)
static mc_t mu_div_thunk(mu_t *frame) {
    frame[0] = mu_div(frame[0], frame[1]);
    return 1;
}

mu_t mu_div(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_div(a, b);
            default:        break;
        }
    }

    mu_error_op2(MU_DIV_KEY, a, b);
}

static mc_t mu_abs_thunk(mu_t *frame);
MSTR(mu_abs_key, "abs")
MBFN(mu_abs_bfn, 0x1, mu_abs_thunk)
static mc_t mu_abs_thunk(mu_t *frame) {
    frame[0] = mu_abs(frame[0]);
    return 1;
}

mu_t mu_abs(mu_t a) {
    switch (mu_type(a)) {
        case MTNUM:     return num_abs(a);
        default:        mu_error_arg1(MU_ABS_KEY, a);
    }
}

static mc_t mu_floor_thunk(mu_t *frame);
MSTR(mu_floor_key, "floor")
MBFN(mu_floor_bfn, 0x1, mu_floor_thunk)
static mc_t mu_floor_thunk(mu_t *frame) {
    frame[0] = mu_floor(frame[0]);
    return 1;
}

mu_t mu_floor(mu_t a) {
    switch (mu_type(a)) {
        case MTNUM:     return num_floor(a);
        default:        mu_error_arg1(MU_FLOOR_KEY, a);
    }
}

static mc_t mu_ceil_thunk(mu_t *frame);
MSTR(mu_ceil_key, "ceil")
MBFN(mu_ceil_bfn, 0x1, mu_ceil_thunk)
static mc_t mu_ceil_thunk(mu_t *frame) {
    frame[0] = mu_ceil(frame[0]);
    return 1;
}

mu_t mu_ceil(mu_t a) {
    switch (mu_type(a)) {
        case MTNUM:     return num_ceil(a);
        default:        mu_error_arg1(MU_CEIL_KEY, a);
    }
}

static mc_t mu_idiv_thunk(mu_t *frame);
MSTR(mu_idiv_key, "//")
MBFN(mu_idiv_bfn, 0x2, mu_idiv_thunk)
static mc_t mu_idiv_thunk(mu_t *frame) {
    frame[0] = mu_idiv(frame[0], frame[1]);
    return 1;
}

mu_t mu_idiv(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_idiv(a, b);
            default:        break;
        }
    }

    mu_error_op2(MU_IDIV_KEY, a, b);
}

static mc_t mu_mod_thunk(mu_t *frame);
MSTR(mu_mod_key, "%")
MBFN(mu_mod_bfn, 0x2, mu_mod_thunk)
static mc_t mu_mod_thunk(mu_t *frame) {
    frame[0] = mu_mod(frame[0], frame[1]);
    return 1;
}

mu_t mu_mod(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_mod(a, b);
            default:        break;
        }
    }

    mu_error_op2(MU_MOD_KEY, a, b);
}

static mc_t mu_pow_thunk(mu_t *frame);
MSTR(mu_pow_key, "^")
MBFN(mu_pow_bfn, 0x2, mu_pow_thunk)
static mc_t mu_pow_thunk(mu_t *frame) {
    frame[0] = mu_pow(frame[0], frame[1]);
    return 1;
}

mu_t mu_pow(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_pow(a, b);
            default:        break;
        }
    }

    mu_error_op2(MU_POW_KEY, a, b);
}

static mc_t mu_log_thunk(mu_t *frame);
MSTR(mu_log_key, "log")
MBFN(mu_log_bfn, 0x2, mu_log_thunk)
static mc_t mu_log_thunk(mu_t *frame) {
    frame[0] = mu_log(frame[0], frame[1]);
    return 1;
}

mu_t mu_log(mu_t a, mu_t b) {
    if (!b || mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_pow(a, b);
            default:        break;
        }
    }

    mu_error_arg2(MU_LOG_KEY, a, b);
}

static mc_t mu_cos_thunk(mu_t *frame);
MSTR(mu_cos_key, "cos")
MBFN(mu_cos_bfn, 0x1, mu_cos_thunk)
static mc_t mu_cos_thunk(mu_t *frame) {
    frame[0] = mu_cos(frame[0]);
    return 1;
}

mu_t mu_cos(mu_t a) {
    switch (mu_type(a)) {
        case MTNUM:     return num_cos(a);
        default:        mu_error_arg1(MU_COS_KEY, a);
    }
}

static mc_t mu_acos_thunk(mu_t *frame);
MSTR(mu_acos_key, "acos")
MBFN(mu_acos_bfn, 0x1, mu_acos_thunk)
static mc_t mu_acos_thunk(mu_t *frame) {
    frame[0] = mu_acos(frame[0]);
    return 1;
}

mu_t mu_acos(mu_t a) {
    switch (mu_type(a)) {
        case MTNUM:     return num_acos(a);
        default:        mu_error_arg1(MU_ACOS_KEY, a);
    }
}

static mc_t mu_sin_thunk(mu_t *frame);
MSTR(mu_sin_key, "sin")
MBFN(mu_sin_bfn, 0x1, mu_sin_thunk)
static mc_t mu_sin_thunk(mu_t *frame) {
    frame[0] = mu_sin(frame[0]);
    return 1;
}

mu_t mu_sin(mu_t a) {
    switch (mu_type(a)) {
        case MTNUM:     return num_sin(a);
        default:        mu_error_arg1(MU_SIN_KEY, a);
    }
}

static mc_t mu_asin_thunk(mu_t *frame);
MSTR(mu_asin_key, "asin")
MBFN(mu_asin_bfn, 0x1, mu_asin_thunk)
static mc_t mu_asin_thunk(mu_t *frame) {
    frame[0] = mu_asin(frame[0]);
    return 1;
}

mu_t mu_asin(mu_t a) {
    switch (mu_type(a)) {
        case MTNUM:     return num_asin(a);
        default:        mu_error_arg1(MU_ASIN_KEY, a);
    }
}

static mc_t mu_tan_thunk(mu_t *frame);
MSTR(mu_tan_key, "tan")
MBFN(mu_tan_bfn, 0x1, mu_tan_thunk)
static mc_t mu_tan_thunk(mu_t *frame) {
    frame[0] = mu_tan(frame[0]);
    return 1;
}

mu_t mu_tan(mu_t a) {
    switch (mu_type(a)) {
        case MTNUM:     return num_tan(a);
        default:        mu_error_arg1(MU_TAN_KEY, a);
    }
}

static mc_t mu_atan_thunk(mu_t *frame);
MSTR(mu_atan_key, "atan")
MBFN(mu_atan_bfn, 0x2, mu_atan_thunk)
static mc_t mu_atan_thunk(mu_t *frame) {
    frame[0] = mu_atan(frame[0], frame[1]);
    return 1;
}

mu_t mu_atan(mu_t a, mu_t b) {
    if (!b || mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_atan(a, b);
            default:        break;
        }
    }

    mu_error_arg2(MU_ATAN_KEY, a, b);
}


// Bitwise/Set operations
static mc_t mu_and_thunk(mu_t *frame);
MSTR(mu_and_key, "&")
MBFN(mu_and_bfn, 0x2, mu_and_thunk)
static mc_t mu_and_thunk(mu_t *frame) {
    frame[0] = mu_and(frame[0], frame[1]);
    return 1;
}

mu_t mu_and(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_and(a, b);
            case MTTBL:     return tbl_and(a, b);
            default:        break;
        }
    }

    mu_error_op2(MU_AND_KEY, a, b);
}

static mc_t mu_or_thunk(mu_t *frame);
MSTR(mu_or_key, "|")
MBFN(mu_or_bfn, 0x2, mu_or_thunk)
static mc_t mu_or_thunk(mu_t *frame) {
    frame[0] = mu_or(frame[0], frame[1]);
    return 1;
}

mu_t mu_or(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_or(a, b);
            case MTTBL:     return tbl_or(a, b);
            default:        break;
        }
    }

    mu_error_op2(MU_OR_KEY, a, b);
}


static mc_t mu_xor_thunk(mu_t *frame);
MSTR(mu_xor_key, "~")
MBFN(mu_xor_bfn, 0x2, mu_xor_thunk)
static mc_t mu_xor_thunk(mu_t *frame) {
    if (!frame[1]) frame[0] = mu_not(frame[0]);
    else           frame[0] = mu_xor(frame[0], frame[1]);
    return 1;
}

mu_t mu_not(mu_t a) {
    switch (mu_type(a)) {
        case MTNUM:     return num_not(a);
        default:        mu_error_op1(MU_XOR_KEY, a);
    }
}

mu_t mu_xor(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_xor(a, b);
            case MTTBL:     return tbl_xor(a, b);
            default:        break;
        }
    }

    mu_error_op2(MU_XOR_KEY, a, b);
}

static mc_t mu_diff_thunk(mu_t *frame);
MSTR(mu_diff_key, "&~")
MBFN(mu_diff_bfn, 0x2, mu_diff_thunk)
static mc_t mu_diff_thunk(mu_t *frame) {
    frame[0] = mu_diff(frame[0], frame[1]);
    return 1;
}

mu_t mu_diff(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_xor(a, num_not(b));
            case MTTBL:     return tbl_diff(a, b);
            default:        break;
        }
    }

    mu_error_op2(MU_DIFF_KEY, a, b);
}

static mc_t mu_shl_thunk(mu_t *frame);
MSTR(mu_shl_key, "<<")
MBFN(mu_shl_bfn, 0x2, mu_shl_thunk)
static mc_t mu_shl_thunk(mu_t *frame) {
    frame[0] = mu_shl(frame[0], frame[1]);
    return 1;
}

mu_t mu_shl(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_shl(a, b);
            default:        break; 
        }
    }

    mu_error_op2(MU_SHL_KEY, a, b);
}

static mc_t mu_shr_thunk(mu_t *frame);
MSTR(mu_shr_key, ">>")
MBFN(mu_shr_bfn, 0x2, mu_shr_thunk)
static mc_t mu_shr_thunk(mu_t *frame) {
    frame[0] = mu_shr(frame[0], frame[1]);
    return 1;
}

mu_t mu_shr(mu_t a, mu_t b) {
    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTNUM:     return num_shr(a, b);
            default:        break;
        }
    }

    mu_error_op2(MU_SHR_KEY, a, b);
}


// String representation
static mc_t mu_parse_thunk(mu_t *frame);
MSTR(mu_parse_key, "parse")
MBFN(mu_parse_bfn, 0x1, mu_parse_thunk)
static mc_t mu_parse_thunk(mu_t *frame) {
    if (!mu_isstr(frame[0])) {
        mu_error_arg1(MU_PARSE_KEY, frame[0]);
    }

    mu_t v = mu_parse((const char *)str_data(frame[0]), str_len(frame[0]));
    str_dec(frame[0]);
    frame[0] = v;
    return 1;
}

static mc_t mu_repr_thunk(mu_t *frame);
MSTR(mu_repr_key, "repr")
MBFN(mu_repr_bfn, 0x2, mu_repr_thunk)
static mc_t mu_repr_thunk(mu_t *frame) {
    frame[0] = mu_dump(frame[0], frame[1]);
    return 1;
}

MSTR(mu_cdata_key, "cdata")

mu_t mu_addr(mu_t m) {
    static mu_t (*const names[8])(void) = {
        [MTNIL] = mu_kw_nil,
        [MTNUM] = mu_num_key,
        [MTSTR] = mu_str_key,
        [MTTBL] = mu_tbl_key,
        [MTFN]  = mu_kw_fn,
        [MTBUF] = mu_cdata_key,
        [MTCD]  = mu_cdata_key
    };

    mu_dec(m);
    return mstr("<%m 0x%wx>", names[mu_type(m)](), m);
}

mu_t mu_repr(mu_t m) {
    return mu_dump(m, 0);
}

mu_t mu_dump(mu_t m, mu_t depth) {
    if (depth && !mu_isnum(depth)) {
        mu_error_arg2(MU_REPR_KEY, m, depth);
    }

    switch (mu_type(m)) {
        case MTNIL:     return MU_KW_NIL;
        case MTNUM:     return num_repr(m);
        case MTSTR:     return str_repr(m);
        case MTTBL:     return tbl_dump(m, depth);
        case MTFN:      return mu_addr(m);
        default:        mu_unreachable;
    }
}

static mc_t mu_bin_thunk(mu_t *frame);
MSTR(mu_bin_key, "bin")
MBFN(mu_bin_bfn, 0x1, mu_bin_thunk)
static mc_t mu_bin_thunk(mu_t *frame) {
    frame[0] = mu_bin(frame[0]);
    return 1;
}
mu_t mu_bin(mu_t m) {
    switch (mu_type(m)) {
        case MTNUM:     return num_bin(m);
        case MTSTR:     return str_bin(m);
        default:        mu_error_arg1(MU_BIN_KEY, m);
    }
}

static mc_t mu_oct_thunk(mu_t *frame);
MSTR(mu_oct_key, "oct")
MBFN(mu_oct_bfn, 0x1, mu_oct_thunk)
static mc_t mu_oct_thunk(mu_t *frame) {
    frame[0] = mu_oct(frame[0]);
    return 1;
}

mu_t mu_oct(mu_t m) {
    switch (mu_type(m)) {
        case MTNUM:     return num_oct(m);
        case MTSTR:     return str_oct(m);
        default:        mu_error_arg1(MU_OCT_KEY, m);
    }
}

static mc_t mu_hex_thunk(mu_t *frame);
MSTR(mu_hex_key, "hex")
MBFN(mu_hex_bfn, 0x1, mu_hex_thunk)
static mc_t mu_hex_thunk(mu_t *frame) {
    frame[0] = mu_hex(frame[0]);
    return 1;
}

mu_t mu_hex(mu_t m) {
    switch (mu_type(m)) {
        case MTNUM:     return num_hex(m);
        case MTSTR:     return str_hex(m);
        default:        mu_error_arg1(MU_HEX_KEY, m);
    }
}


// Data structure operations
// These do not consume their first argument
static mc_t mu_len_thunk(mu_t *frame);
MSTR(mu_len_key, "len")
MBFN(mu_len_bfn, 0x1, mu_len_thunk)
static mc_t mu_len_thunk(mu_t *frame) {
    mlen_t len = mu_len(frame[0]);
    mu_dec(frame[0]);
    frame[0] = muint(len);
    return 1;
}

mlen_t mu_len(mu_t m) {
    switch (mu_type(m)) {
        case MTSTR:     return str_len(m);
        case MTTBL:     return tbl_len(m);
        default:        mu_error_arg1(MU_LEN_KEY, m);
    }
}

static mc_t mu_tail_thunk(mu_t *frame);
MSTR(mu_tail_key, "tail")
MBFN(mu_tail_bfn, 0x1, mu_tail_thunk)
static mc_t mu_tail_thunk(mu_t *frame) {
    mu_t tail = mu_tail(frame[0]);
    mu_dec(frame[0]);
    frame[0] = tail;
    return 1;
}

mu_t mu_tail(mu_t m) {
    switch (mu_type(m)) {
        case MTTBL:     return tbl_tail(m);
        default:        mu_error_arg1(MU_TAIL_KEY, m);
    }
}

static mc_t mu_push_thunk(mu_t *frame);
MSTR(mu_push_key, "push")
MBFN(mu_push_bfn, 0x3, mu_push_thunk)
static mc_t mu_push_thunk(mu_t *frame) {
    mu_push(frame[0], frame[1], frame[2]);
    mu_dec(frame[0]);
    return 0;
}

void mu_push(mu_t m, mu_t v, mu_t i) {
    if (!i || mu_isnum(i)) {
        switch (mu_type(m)) {
            case MTTBL:     return tbl_push(m, v, i);
            default:        break;
        }
    }

    mu_error_arg3(MU_PUSH_KEY, m, v, i);
}

static mc_t mu_pop_thunk(mu_t *frame);
MSTR(mu_pop_key, "pop")
MBFN(mu_pop_bfn, 0x2, mu_pop_thunk)
static mc_t mu_pop_thunk(mu_t *frame) {
    mu_t v = mu_pop(frame[0], frame[1]);
    mu_dec(frame[0]);
    frame[0] = v;
    return 1;
}

mu_t mu_pop(mu_t m, mu_t i) {
    if (!i || mu_isnum(i)) {
        switch (mu_type(m)) {
            case MTTBL:     return tbl_pop(m, i);
            default:        break;
        }
    }

    mu_error_arg2(MU_POP_KEY, m, i);
}

static mc_t mu_concat_thunk(mu_t *frame);
MSTR(mu_concat_key, "++")
MBFN(mu_concat_bfn, 0x3, mu_concat_thunk)
static mc_t mu_concat_thunk(mu_t *frame) {
    frame[0] = mu_concat(frame[0], frame[1], frame[2]);
    return 1;
}

mu_t mu_concat(mu_t a, mu_t b, mu_t offset) {
    if (offset && !mu_isnum(offset)) {
        mu_error_arg3(MU_CONCAT_KEY, a, b, offset);
    }

    if (mu_type(a) == mu_type(b)) {
        switch (mu_type(a)) {
            case MTSTR:     return str_concat(a, b);
            case MTTBL:     return tbl_concat(a, b, offset);
            default:        break;
        }
    }

    mu_error_op2(MU_CONCAT_KEY, a, b);
}

static mc_t mu_subset_thunk(mu_t *frame);
MSTR(mu_subset_key, "sub")
MBFN(mu_subset_bfn, 0x3, mu_subset_thunk)
static mc_t mu_subset_thunk(mu_t *frame) {
    frame[0] = mu_subset(frame[0], frame[1], frame[2]);
    return 1;
}

mu_t mu_subset(mu_t m, mu_t lower, mu_t upper) {
    if (mu_isnum(lower) && (!upper || mu_isnum(upper))) {
        switch (mu_type(m)) {
            case MTSTR:     return str_subset(m, lower, upper);
            case MTTBL:     return tbl_subset(m, lower, upper);
            default:        break;
        }
    }

    mu_error_arg3(MU_SUBSET_KEY, m, lower, upper);
}


// String operations
static mc_t mu_find_thunk(mu_t *frame);
MSTR(mu_find_key, "find")
MBFN(mu_find_bfn, 0x2, mu_find_thunk)
static mc_t mu_find_thunk(mu_t *frame) {
    mu_t len = muint(mu_len(frame[0]));
    frame[0] = mu_find(frame[0], frame[1]);
    frame[1] = len;
    return 2;
}

mu_t mu_find(mu_t m, mu_t s) {
    if (mu_type(m) == mu_type(s)) {
        switch (mu_type(m)) {
            case MTSTR:     return str_find(m, s);
            default:        break;
        }
    }

    mu_error_arg2(MU_FIND_KEY, m, s);
}

static mc_t mu_replace_thunk(mu_t *frame);
MSTR(mu_replace_key, "replace")
MBFN(mu_replace_bfn, 0x3, mu_replace_thunk)
static mc_t mu_replace_thunk(mu_t *frame) {
    frame[0] = mu_replace(frame[0], frame[1], frame[2]);
    return 1;
}

mu_t mu_replace(mu_t m, mu_t r, mu_t s) {
    if (mu_type(m) == mu_type(r) && mu_type(m) == mu_type(s)) {
        switch (mu_type(m)) {
            case MTSTR:     return str_replace(m, r, s);
            default:        break;
        }
    }

    mu_error_arg3(MU_REPLACE_KEY, m, r, s);
}

static mc_t mu_split_thunk(mu_t *frame);
MSTR(mu_split_key, "split")
MBFN(mu_split_bfn, 0x2, mu_split_thunk)
static mc_t mu_split_thunk(mu_t *frame) {
    frame[0] = mu_split(frame[0], frame[1]);
    return 1;
}

mu_t mu_split(mu_t m, mu_t delim) {
    if (!delim || mu_isstr(delim)) {
        switch (mu_type(m)) {
            case MTSTR:     return str_split(m, delim);
            default:        break;
        }
    }

    mu_error_arg2(MU_SPLIT_KEY, m, delim);
}

static mc_t mu_join_thunk(mu_t *frame);
MSTR(mu_join_key, "join")
MBFN(mu_join_bfn, 0x2, mu_join_thunk)
static mc_t mu_join_thunk(mu_t *frame) {
    frame[0] = mu_join(frame[0], frame[1]);
    return 1;
}

mu_t mu_join(mu_t m, mu_t delim) {
    if (!delim || mu_isstr(delim)) {
        return str_join(mu_iter(m), delim);
    }

    mu_error_arg2(MU_JOIN_KEY, m, delim);
}

static mc_t mu_pad_thunk(mu_t *frame);
MSTR(mu_pad_key, "pad")
MBFN(mu_pad_bfn, 0x3, mu_pad_thunk)
static mc_t mu_pad_thunk(mu_t *frame) {
    frame[0] = mu_pad(frame[0], frame[1], frame[2]);
    return 1;
}

mu_t mu_pad(mu_t m, mu_t len, mu_t pad) {
    if (mu_isnum(len) && 
        (!pad || (mu_isstr(pad) && str_len(pad) > 0))) {
        switch (mu_type(m)) {
            case MTSTR:     return str_pad(m, len, pad);
            default:        break;
        }
    }

    mu_error_arg3(MU_PAD_KEY, m, len, pad);
}

static mc_t mu_strip_thunk(mu_t *frame);
MSTR(mu_strip_key, "strip")
MBFN(mu_strip_bfn, 0x3, mu_strip_thunk)
static mc_t mu_strip_thunk(mu_t *frame) {
    if (mu_isstr(frame[1])) {
        mu_dec(frame[2]);
        frame[2] = frame[1];
        frame[1] = 0;
    }

    frame[0] = mu_strip(frame[0], frame[1], frame[2]);
    return 1;
}

mu_t mu_strip(mu_t m, mu_t dir, mu_t pad) {
    if ((!dir || mu_isnum(dir)) && 
        (!pad || (mu_isstr(pad) && str_len(pad) > 0))) {
        switch (mu_type(m)) {
            case MTSTR:     return str_strip(m, dir, pad);
            default:        break;
        }
    }

    mu_error_arg3(MU_STRIP_KEY, m, dir, pad);
}


// Function operations
static mc_t mu_bind_thunk(mu_t *frame);
MSTR(mu_bind_key, "bind")
MBFN(mu_bind_bfn, 0xf, mu_bind_thunk)
static mc_t mu_bind_thunk(mu_t *frame) {
    mu_t m = tbl_pop(frame[0], muint(0));
    frame[0] = mu_bind(m, frame[0]);
    return 1;
}

mu_t mu_bind(mu_t m, mu_t args) {
    return fn_bind(m, args);
}

static mc_t mu_comp_thunk(mu_t *frame);
MSTR(mu_comp_key, "comp")
MBFN(mu_comp_bfn, 0xf, mu_comp_thunk)
static mc_t mu_comp_thunk(mu_t *frame) {
    frame[0] = mu_comp(frame[0]);
    return 1;
}

mu_t mu_comp(mu_t ms) {
    return fn_comp(ms);
}

static mc_t mu_map_thunk(mu_t *frame);
MSTR(mu_map_key, "map")
MBFN(mu_map_bfn, 0x2, mu_map_thunk)
static mc_t mu_map_thunk(mu_t *frame) {
    frame[0] = mu_map(frame[0], frame[1]);
    return 1;
}

mu_t mu_map(mu_t m, mu_t iter) {
    switch (mu_type(m)) {
        case MTFN:      return fn_map(m, iter);
        default:        break;
    }

    mu_error_arg2(MU_MAP_KEY, m, iter);
}

static mc_t mu_filter_thunk(mu_t *frame);
MSTR(mu_filter_key, "filter")
MBFN(mu_filter_bfn, 0x2, mu_filter_thunk)
static mc_t mu_filter_thunk(mu_t *frame) {
    frame[0] = mu_filter(frame[0], frame[1]);
    return 1;
}

mu_t mu_filter(mu_t m, mu_t iter) {
    switch (mu_type(m)) {
        case MTFN:      return fn_filter(m, iter);
        default:        break;
    }

    mu_error_arg2(MU_FILTER_KEY, m, iter);
}

static mc_t mu_reduce_thunk(mu_t *frame);
MSTR(mu_reduce_key, "reduce")
MBFN(mu_reduce_bfn, 0xf, mu_reduce_thunk)
static mc_t mu_reduce_thunk(mu_t *frame) {
    mu_t f = tbl_pop(frame[0], muint(0));
    mu_t i = tbl_pop(frame[0], muint(0));
    frame[0] = mu_reduce(f, i, frame[0]);
    return 0xf;
}

mu_t mu_reduce(mu_t m, mu_t iter, mu_t inits) {
    switch (mu_type(m)) {
        case MTFN:      return fn_reduce(m, iter, inits);
        default:        break;
    }

    mu_error_arg3(MU_REDUCE_KEY, m, iter, inits);
}

static mc_t mu_any_thunk(mu_t *frame);
MSTR(mu_any_key, "any")
MBFN(mu_any_bfn, 0x2, mu_any_thunk)
static mc_t mu_any_thunk(mu_t *frame) {
    frame[0] = mu_any(frame[0], frame[1]) ? MU_TRUE : MU_FALSE;
    return 1;
}

bool mu_any(mu_t m, mu_t iter) {
    switch (mu_type(m)) {
        case MTFN:      return fn_any(m, iter);
        default:        break;
    }

    mu_error_arg2(MU_ANY_KEY, m, iter);
}

static mc_t mu_all_thunk(mu_t *frame);
MSTR(mu_all_key, "all")
MBFN(mu_all_bfn, 0x2, mu_all_thunk)
static mc_t mu_all_thunk(mu_t *frame) {
    frame[0] = mu_all(frame[0], frame[1]) ? MU_TRUE : MU_FALSE;
    return 1;
}

bool mu_all(mu_t m, mu_t iter) {
    switch (mu_type(m)) {
        case MTFN:      return fn_all(m, iter);
        default:        break;
    }

    mu_error_arg2(MU_ALL_KEY, m, iter);
}


// Iterators and generators
static mc_t mu_iter_thunk(mu_t *frame);
MSTR(mu_iter_key, "iter")
MBFN(mu_iter_bfn, 0x1, mu_iter_thunk)
static mc_t mu_iter_thunk(mu_t *frame) {
    frame[0] = mu_iter(frame[0]);
    return 1;
}

mu_t mu_iter(mu_t m) {
    switch (mu_type(m)) {
        case MTSTR:     return str_iter(m);
        case MTTBL:     return tbl_iter(m);
        case MTFN:      return m;
        default:        break;
    }

    mu_error_convert(MU_ITER_KEY, m);
}

static mc_t mu_pairs_thunk(mu_t *frame);
MSTR(mu_pairs_key, "pairs")
MBFN(mu_pairs_bfn, 0x1, mu_pairs_thunk)
static mc_t mu_pairs_thunk(mu_t *frame) {
    frame[0] = mu_pairs(frame[0]);
    return 1;
}

mu_t mu_pairs(mu_t m) {
    switch (mu_type(m)) {
        case MTTBL:     return tbl_pairs(m);
        default:        return mu_zip(mlist({
                            mu_range(0, 0, 0), mu_iter(m)
                        }));
    }
}

static mc_t mu_range_thunk(mu_t *frame);
MSTR(mu_range_key, "range")
MBFN(mu_range_bfn, 0x3, mu_range_thunk)
static mc_t mu_range_thunk(mu_t *frame) {
    if (!frame[1]) {
        frame[1] = frame[0];
        frame[0] = 0;
    }

    frame[0] = mu_range(frame[0], frame[1], frame[2]);
    return 1;
}

mu_t mu_range(mu_t start, mu_t stop, mu_t step) {
    if ((!start || mu_isnum(start)) &&
        (!stop || mu_isnum(stop)) &&
        (!step || mu_isnum(step))) {
        return fn_range(start, stop, step);
    }

    mu_error_arg3(MU_RANGE_KEY, start, stop, step);
}

static mc_t mu_repeat_thunk(mu_t *frame);
MSTR(mu_repeat_key, "repeat")
MBFN(mu_repeat_bfn, 0x2, mu_repeat_thunk)
static mc_t mu_repeat_thunk(mu_t *frame) {
    frame[0] = mu_repeat(frame[0], frame[1]);
    return 1;
}

mu_t mu_repeat(mu_t value, mu_t times) {
    if (!times || mu_isnum(times)) {
        return fn_repeat(value, times);
    }

    mu_error_arg2(MU_REPEAT_KEY, value, times);
}

static mc_t mu_seed_thunk(mu_t *frame);
MSTR(mu_seed_key, "seed")
MBFN(mu_seed_bfn, 0x1, mu_seed_thunk)
static mc_t mu_seed_thunk(mu_t *frame) {
    frame[0] = mu_seed(frame[0]);
    return 1;
}

mu_t mu_seed(mu_t m) {
    if (!m || mu_isnum(m)) {
        return num_seed(m);
    }

    mu_error_arg1(MU_SEED_KEY, m);
}


// Iterator manipulation
static mc_t mu_zip_thunk(mu_t *frame);
MSTR(mu_zip_key, "zip")
MBFN(mu_zip_bfn, 0xf, mu_zip_thunk)
static mc_t mu_zip_thunk(mu_t *frame) {
    if (tbl_len(frame[0]) == 0) {
        mu_errorf("no arguments passed to zip");
    } else if (tbl_len(frame[0]) == 1) {
        mu_t iter = tbl_lookup(frame[0], muint(0));
        mu_dec(frame[0]);
        frame[0] = mu_zip(iter);
    } else {
        frame[0] = mu_zip(frame[0]);
    }

    return 1;
}

mu_t mu_zip(mu_t iters) {
    return fn_zip(mu_iter(iters));
}

static mc_t mu_chain_thunk(mu_t *frame);
MSTR(mu_chain_key, "chain")
MBFN(mu_chain_bfn, 0xf, mu_chain_thunk)
static mc_t mu_chain_thunk(mu_t *frame) {
    if (tbl_len(frame[0]) == 0) {
        mu_errorf("no arguments passed to chain");
    } else if (tbl_len(frame[0]) == 1) {
        mu_t iter = tbl_lookup(frame[0], muint(0));
        mu_dec(frame[0]);
        frame[0] = mu_chain(iter);
    } else {
        frame[0] = mu_chain(frame[0]);
    }

    return 1;
}

mu_t mu_chain(mu_t iters) {
    return fn_chain(mu_iter(iters));
}

static mc_t mu_take_thunk(mu_t *frame);
MSTR(mu_take_key, "take")
MBFN(mu_take_bfn, 0x2, mu_take_thunk)
static mc_t mu_take_thunk(mu_t *frame) {
    frame[0] = mu_take(frame[0], frame[1]);
    return 1;
}

mu_t mu_take(mu_t m, mu_t iter) {
    return fn_take(m, mu_iter(iter));
}

static mc_t mu_drop_thunk(mu_t *frame);
MSTR(mu_drop_key, "drop")
MBFN(mu_drop_bfn, 0x2, mu_drop_thunk)
static mc_t mu_drop_thunk(mu_t *frame) {
    frame[0] = mu_drop(frame[0], frame[1]);
    return 1;
}

mu_t mu_drop(mu_t m, mu_t iter) {
    return fn_drop(m, mu_iter(iter));
}


// Iterator ordering
static mc_t mu_min_thunk(mu_t *frame);
MSTR(mu_min_key, "min")
MBFN(mu_min_bfn, 0xf, mu_min_thunk)
static mc_t mu_min_thunk(mu_t *frame) {
    if (tbl_len(frame[0]) == 1) {
        mu_t iter = tbl_lookup(frame[0], muint(0));
        mu_dec(frame[0]);
        frame[0] = mu_min(iter);
    } else {
        frame[0] = mu_min(frame[0]);
    }

    return 0xf;
}

mu_t mu_min(mu_t iter) {
    return fn_min(mu_iter(iter));
}

static mc_t mu_max_thunk(mu_t *frame);
MSTR(mu_max_key, "max")
MBFN(mu_max_bfn, 0xf, mu_max_thunk)
static mc_t mu_max_thunk(mu_t *frame) {
    if (tbl_len(frame[0]) == 1) {
        mu_t iter = tbl_lookup(frame[0], muint(0));
        mu_dec(frame[0]);
        frame[0] = mu_max(iter);
    } else {
        frame[0] = mu_max(frame[0]);
    }

    return 0xf;
}

mu_t mu_max(mu_t iter) {
    return fn_max(mu_iter(iter));
}

static mc_t mu_reverse_thunk(mu_t *frame);
MSTR(mu_reverse_key, "reverse")
MBFN(mu_reverse_bfn, 0x1, mu_reverse_thunk)
static mc_t mu_reverse_thunk(mu_t *frame) {
    frame[0] = mu_reverse(frame[0]);
    return 1;
}

mu_t mu_reverse(mu_t iter) {
    return fn_reverse(mu_iter(iter));
}

static mc_t mu_sort_thunk(mu_t *frame);
MSTR(mu_sort_key, "sort")
MBFN(mu_sort_bfn, 0x1, mu_sort_thunk)
static mc_t mu_sort_thunk(mu_t *frame) {
    frame[0] = mu_sort(frame[0]);
    return 1;
}

mu_t mu_sort(mu_t iter) {
    return fn_sort(mu_iter(iter));
}


// System operations
static mc_t mu_error_thunk(mu_t *frame);
MSTR(mu_error_key, "error")
MBFN(mu_error_bfn, 0xf, mu_error_thunk)
static mc_t mu_error_thunk(mu_t *frame) {
    mu_t b = buf_create(0);
    muint_t n = 0;
    mu_t v;

    for (muint_t i = 0; tbl_next(frame[0], &i, 0, &v);) {
        buf_format(&b, &n, "%m", v);
    }

    mu_error(buf_data(b), n);
    mu_unreachable;
}

mu_noreturn mu_error(const char *s, muint_t n) {
    sys_error(s, n);
    mu_unreachable;
}

mu_noreturn mu_verrorf(const char *f, va_list args) {
    mu_t b = buf_create(0);
    muint_t n = 0;
    buf_vformat(&b, &n, f, args);
    mu_error(buf_data(b), n);
    mu_unreachable;
}

mu_noreturn mu_errorf(const char *f, ...) {
    va_list args;
    va_start(args, f);
    mu_verrorf(f, args);
    mu_unreachable;
}

static mc_t mu_print_thunk(mu_t *frame);
MSTR(mu_print_key, "print")
MBFN(mu_print_bfn, 0xf, mu_print_thunk)
static mc_t mu_print_thunk(mu_t *frame) {
    mu_t b = buf_create(0);
    muint_t n = 0;
    mu_t v;

    for (muint_t i = 0; tbl_next(frame[0], &i, 0, &v);) {
        buf_format(&b, &n, "%m", v);
    }

    mu_print(buf_data(b), n);
    buf_dec(b);
    tbl_dec(frame[0]);
    return 0;
}

void mu_print(const char *s, muint_t n) {
    return sys_print(s, n);
}

void mu_vprintf(const char *f, va_list args) {
    mu_t b = buf_create(0);
    muint_t n = 0;
    buf_vformat(&b, &n, f, args);
    mu_print(buf_data(b), n);
    buf_dec(b);
}

void mu_printf(const char *f, ...) {
    va_list args;
    va_start(args, f);
    mu_vprintf(f, args);
    va_end(args);
}

static mc_t mu_import_thunk(mu_t *frame);
MSTR(mu_import_key, "import")
MBFN(mu_import_bfn, 0x1, mu_import_thunk)
static mc_t mu_import_thunk(mu_t *frame) {
    frame[0] = mu_import(frame[0]);
    return 1;
}

mu_t mu_import(mu_t name) {
    if (!mu_isstr(name)) {
        mu_error_arg1(MU_IMPORT_KEY, name);
    }

    static mu_t import_history = 0;
    if (!import_history) {
        import_history = tbl_create(0);
    }

    mu_t module = tbl_lookup(import_history, str_inc(name));
    if (module) {
        str_dec(name);
        return module;
    }

    module = sys_import(str_inc(name));
    tbl_insert(import_history, name, mu_inc(module));
    return module;
}


// Constant keys
MSTR(mu_nil_key,    "nil")
MSTR(mu_true_key,   "true")
MSTR(mu_false_key,  "false")
MSTR(mu_inf_key,    "inf")
MSTR(mu_ninf_key,   "ninf")
MSTR(mu_e_key,      "e")
MSTR(mu_pi_key,     "pi")
MSTR(mu_id_key,     "id")

// Builtins table
MTBL(mu_builtins, {
    // Constants
    { mu_true_key,      mu_true },
    { mu_inf_key,       mu_inf },
    { mu_e_key,         mu_e },
    { mu_pi_key,        mu_pi },
    { mu_id_key,        mu_id },

    // Type casts
    { mu_num_key,       mu_num_bfn },
    { mu_str_key,       mu_str_bfn },
    { mu_tbl_key,       mu_tbl_bfn },
    { mu_fn_key,        mu_fn_bfn },

    // Logic operations
    { mu_not_key,       mu_not_bfn },
    { mu_eq_key,        mu_eq_bfn },
    { mu_neq_key,       mu_neq_bfn },
    { mu_is_key,        mu_is_bfn },
    { mu_lt_key,        mu_lt_bfn },
    { mu_lte_key,       mu_lte_bfn },
    { mu_gt_key,        mu_gt_bfn },
    { mu_gte_key,       mu_gte_bfn },

    // Arithmetic operations
    { mu_add_key,       mu_add_bfn },
    { mu_sub_key,       mu_sub_bfn },
    { mu_mul_key,       mu_mul_bfn },
    { mu_div_key,       mu_div_bfn },

    { mu_abs_key,       mu_abs_bfn },
    { mu_floor_key,     mu_floor_bfn },
    { mu_ceil_key,      mu_ceil_bfn },
    { mu_idiv_key,      mu_idiv_bfn },
    { mu_mod_key,       mu_mod_bfn },

    { mu_pow_key,       mu_pow_bfn },
    { mu_log_key,       mu_log_bfn },

    { mu_cos_key,       mu_cos_bfn },
    { mu_acos_key,      mu_acos_bfn },
    { mu_sin_key,       mu_sin_bfn },
    { mu_asin_key,      mu_asin_bfn },
    { mu_tan_key,       mu_tan_bfn },
    { mu_atan_key,      mu_atan_bfn },

    // Bitwise/Set operations
    { mu_and_key,       mu_and_bfn },
    { mu_or_key,        mu_or_bfn },
    { mu_xor_key,       mu_xor_bfn },
    { mu_diff_key,      mu_diff_bfn },

    { mu_shl_key,       mu_shl_bfn },
    { mu_shr_key,       mu_shr_bfn },

    // String representation
    { mu_parse_key,     mu_parse_bfn },
    { mu_repr_key,      mu_repr_bfn },

    { mu_bin_key,       mu_bin_bfn },
    { mu_oct_key,       mu_oct_bfn },
    { mu_hex_key,       mu_hex_bfn },

    // Data structure operations
    { mu_len_key,       mu_len_bfn },
    { mu_tail_key,      mu_tail_bfn },

    { mu_push_key,      mu_push_bfn },
    { mu_pop_key,       mu_pop_bfn },

    { mu_concat_key,    mu_concat_bfn },
    { mu_subset_key,    mu_subset_bfn },

    // String operations
    { mu_find_key,      mu_find_bfn },
    { mu_replace_key,   mu_replace_bfn },
    { mu_split_key,     mu_split_bfn },
    { mu_join_key,      mu_join_bfn },
    { mu_pad_key,       mu_pad_bfn },
    { mu_strip_key,     mu_strip_bfn },

    // Function operations
    { mu_bind_key,      mu_bind_bfn },
    { mu_comp_key,      mu_comp_bfn },

    { mu_map_key,       mu_map_bfn },
    { mu_filter_key,    mu_filter_bfn },
    { mu_reduce_key,    mu_reduce_bfn },

    { mu_any_key,       mu_any_bfn },
    { mu_all_key,       mu_all_bfn },

    // Iterators and generators
    { mu_iter_key,      mu_iter_bfn },
    { mu_pairs_key,     mu_pairs_bfn },

    { mu_range_key,     mu_range_bfn },
    { mu_repeat_key,    mu_repeat_bfn },
    { mu_seed_key,      mu_seed_bfn },

    // Iterator manipulation
    { mu_zip_key,       mu_zip_bfn },
    { mu_chain_key,     mu_chain_bfn },

    { mu_take_key,      mu_take_bfn },
    { mu_drop_key,      mu_drop_bfn },

    // Iterator ordering
    { mu_min_key,       mu_min_bfn },
    { mu_max_key,       mu_max_bfn },

    { mu_reverse_key,   mu_reverse_bfn },
    { mu_sort_key,      mu_sort_bfn },

    // System operations
    { mu_error_key,     mu_error_bfn },
    { mu_print_key,     mu_print_bfn },
    { mu_import_key,    mu_import_bfn },
})

