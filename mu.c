#include "mu.h"

#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"
#include "err.h"
#include "parse.h"


// Conversion between different frame types
void mu_fto(mc_t dc, mc_t sc, mu_t *frame) {
    if (dc != 0xf && sc != 0xf) {
        for (muint_t i = dc; i < sc; i++)
            mu_dec(frame[i]);

        for (muint_t i = sc; i < dc; i++)
            frame[i] = mnil;

    } else if (dc != 0xf) {
        mu_t t = *frame;

        for (muint_t i = 0; i < dc; i++)
            frame[i] = tbl_lookup(t, muint(i));

        tbl_dec(t);
    } else if (sc != 0xf) {
        mu_t t = tbl_create(sc);

        for (muint_t i = 0; i < sc; i++)
            tbl_insert(t, muint(i), frame[i]);

        *frame = t;
    }
}


// Destructor table
extern void str_destroy(mu_t);
extern void tbl_destroy(mu_t);
extern void fn_destroy(mu_t);

void (*const mu_destroy_table[6])(mu_t) = {
    tbl_destroy, tbl_destroy,
    str_destroy, fn_destroy,
    fn_destroy, fn_destroy,
};


// Table related functions performed on variables
mu_t mu_lookup(mu_t m, mu_t k) {
    switch (mu_type(m)) {
        case MU_TBL:
        case MU_RTBL:   return tbl_lookup(m, k);
        default:        mu_err_undefined();
    }
}

void mu_insert(mu_t m, mu_t k, mu_t v) {
    switch (mu_type(m)) {
        case MU_TBL:
        case MU_RTBL:   return tbl_insert(m, k, v);
        default:        mu_err_undefined();
    }
}

void mu_assign(mu_t m, mu_t k, mu_t v) {
    switch (mu_type(m)) {
        case MU_TBL:
        case MU_RTBL:   return tbl_assign(m, k, v);
        default:        mu_err_undefined();
    }
}

// Function calls performed on variables
mc_t mu_tcall(mu_t m, mc_t fc, mu_t *frame) {
    extern mc_t bfn_tcall(mu_t f, mc_t fc, mu_t *frame);
    extern mc_t sfn_tcall(mu_t f, mc_t fc, mu_t *frame);
    extern mc_t mfn_tcall(mu_t f, mc_t fc, mu_t *frame);

    switch (mu_type(m)) {
        case MU_FN:     return mfn_tcall(m, fc, frame);
        case MU_BFN:    return bfn_tcall(m, fc, frame);
        case MU_SFN:    return sfn_tcall(m, fc, frame);
        default:        mu_err_undefined();
    }
}

void mu_fcall(mu_t m, mc_t fc, mu_t *frame) {
    mc_t rets = mu_tcall(m, fc >> 4, frame);
    mu_fto(0xf & fc, rets, frame);
}

mu_t mu_vcall(mu_t m, mc_t fc, va_list args) {
    mu_t frame[MU_FRAME];

    for (muint_t i = 0; i < mu_fcount(0xf & fc); i++)
        frame[i] = va_arg(args, mu_t);

    mu_fcall(m, fc, frame);

    for (muint_t i = 1; i < mu_fcount(fc >> 4); i++)
        *va_arg(args, mu_t *) = frame[i];

    return (fc >> 4) ? *frame : mnil;
}

mu_t mu_call(mu_t m, mc_t fc, ...) {
    va_list args;
    va_start(args, fc);
    mu_t ret = mu_vcall(m, fc, args);
    va_end(args);
    return ret;
}


// Type casts and declarations
mu_t mu_num(mu_t m) {
    switch (mu_type(m)) {
        case MU_NIL:    return muint(0);
        case MU_NUM:    return m;
        case MU_STR:    return num_fromstr(m);
        default:        mu_err_undefined();
    }
}

mu_t mu_str(mu_t m) {
    switch (mu_type(m)) {
        case MU_NIL:    return mcstr("");
        case MU_NUM:    return str_fromnum(m);
        case MU_STR:    return m;
        case MU_TBL:    
        case MU_RTBL:   
        case MU_FN:
        case MU_BFN:
        case MU_SFN:    return str_fromiter(mu_iter(m));
        default:        mu_err_undefined();
    }
}

mu_t mu_tbl(mu_t m, mu_t tail) {
    if (tail && !mu_istbl(tail))
        mu_err_undefined();

    mu_t t;
    switch (mu_type(m)) {
        case MU_NIL:    t = tbl_create(0); break;
        case MU_NUM:    t = tbl_fromnum(m); break;
        case MU_STR:    t = tbl_fromiter(mu_iter(m)); break;
        case MU_TBL:    
        case MU_RTBL:   t = tbl_fromiter(mu_pairs(m)); break;
        case MU_FN:
        case MU_BFN:
        case MU_SFN:    t = tbl_fromiter(m); break;
        default:        mu_unreachable;
    }

    tbl_inherit(t, tail);
    return t;
}

mu_t mu_fn(mu_t m) {
    switch (mu_type(m)) {
        case MU_NIL:    return MU_ID;
        case MU_FN:
        case MU_BFN:
        case MU_SFN:    return m;
        default:        mu_err_undefined();
    }
}


// Comparison operations
// does not consume!
bool mu_is(mu_t a, mu_t type) {
    switch (mu_type(a)) {
        case MU_NIL:    return !type;
        case MU_NUM:    return type == MU_NUM_TYPE;
        case MU_STR:    return type == MU_STR_TYPE;
        case MU_RTBL:
        case MU_TBL:    return type == MU_TBL_TYPE;
        case MU_FN:
        case MU_BFN:
        case MU_SFN:    return type == MU_FN_TYPE;
        default:        mu_unreachable;
    }
}    

mint_t mu_cmp(mu_t a, mu_t b) {
    if (mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_cmp(a, b);
        case MU_STR:    return str_cmp(a, b);
        default:        mu_err_undefined();
    }
}


// Arithmetic operations
mu_t mu_pos(mu_t a) {
    switch (mu_type(a)) {
        case MU_NUM:    return a;
        default:        mu_err_undefined();
    }
}

mu_t mu_neg(mu_t a) {
    switch (mu_type(a)) {
        case MU_NUM:    return num_neg(a);
        default:        mu_err_undefined();
    }
}

mu_t mu_add(mu_t a, mu_t b) {
    if (mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_add(a, b);
        default:        mu_err_undefined();
    }
}

mu_t mu_sub(mu_t a, mu_t b) {
    if (mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_sub(a, b);
        default:        mu_err_undefined();
    }
}

mu_t mu_mul(mu_t a, mu_t b) {
    if (mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_mul(a, b);
        default:        mu_err_undefined();
    }
}

mu_t mu_div(mu_t a, mu_t b) {
    if (mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_div(a, b);
        default:        mu_err_undefined();
    }
}

mu_t mu_abs(mu_t a) {
    switch (mu_type(a)) {
        case MU_NUM:    return num_abs(a);
        default:        mu_err_undefined();
    }
}

mu_t mu_floor(mu_t a) {
    switch (mu_type(a)) {
        case MU_NUM:    return num_floor(a);
        default:        mu_err_undefined();
    }
}

mu_t mu_ceil(mu_t a) {
    switch (mu_type(a)) {
        case MU_NUM:    return num_ceil(a);
        default:        mu_err_undefined();
    }
}

mu_t mu_idiv(mu_t a, mu_t b) {
    if (mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_idiv(a, b);
        default:        mu_err_undefined();
    }
}

mu_t mu_mod(mu_t a, mu_t b) {
    if (mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_mod(a, b);
        default:        mu_err_undefined();
    }
}

mu_t mu_pow(mu_t a, mu_t b) {
    if (mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_pow(a, b);
        default:        mu_err_undefined();
    }
}

mu_t mu_log(mu_t a, mu_t b) {
    if (b && mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_log(a, b);
        default:        mu_err_undefined();
    }
}

mu_t mu_cos(mu_t a) {
    switch (mu_type(a)) {
        case MU_NUM:    return num_cos(a);
        default:        mu_err_undefined();
    }
}

mu_t mu_acos(mu_t a) {
    switch (mu_type(a)) {
        case MU_NUM:    return num_acos(a);
        default:        mu_err_undefined();
    }
}

mu_t mu_sin(mu_t a) {
    switch (mu_type(a)) {
        case MU_NUM:    return num_sin(a);
        default:        mu_err_undefined();
    }
}

mu_t mu_asin(mu_t a) {
    switch (mu_type(a)) {
        case MU_NUM:    return num_asin(a);
        default:        mu_err_undefined();
    }
}

mu_t mu_tan(mu_t a) {
    switch (mu_type(a)) {
        case MU_NUM:    return num_tan(a);
        default:        mu_err_undefined();
    }
}

mu_t mu_atan(mu_t a, mu_t b) {
    if (b && mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_atan(a, b);
        default:        mu_err_undefined();
    }
}


// Bitwise/Set operations
mu_t mu_not(mu_t a) {
    switch (mu_type(a)) {
        case MU_NUM:    return num_not(a);
        default:        mu_err_undefined();
    }
}

mu_t mu_and(mu_t a, mu_t b) {
    if (mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_and(a, b);
        case MU_TBL:
        case MU_RTBL:   return tbl_and(a, b);
        default:        mu_err_undefined();
    }
}

mu_t mu_or(mu_t a, mu_t b) {
    if (mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_or(a, b);
        case MU_TBL:
        case MU_RTBL:   return tbl_or(a, b);
        default:        mu_err_undefined();
    }
}

mu_t mu_xor(mu_t a, mu_t b) {
    if (mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_xor(a, b);
        case MU_TBL:
        case MU_RTBL:   return tbl_xor(a, b);
        default:        mu_err_undefined();
    }
}

mu_t mu_diff(mu_t a, mu_t b) {
    if (mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    switch (mu_type(a)) {
        case MU_NUM:    return num_xor(a, num_not(b));
        case MU_TBL:
        case MU_RTBL:   return tbl_diff(a, b);
        default:        mu_err_undefined();
    }
}

mu_t mu_shl(mu_t a, mu_t b) {
    if (!mu_isnum(b))
        mu_err_undefined();

    switch (mu_type(a)) {
        case MU_NUM:    return num_shl(a, b);
        default:        mu_err_undefined();
    }
}

mu_t mu_shr(mu_t a, mu_t b) {
    if (!mu_isnum(b))
        mu_err_undefined();

    switch (mu_type(a)) {
        case MU_NUM:    return num_shr(a, b);
        default:        mu_err_undefined();
    }
}

// String representation
mu_t mu_repr(mu_t m) {
    return mu_dump(m, mnil, mnil);
}

mu_t mu_dump(mu_t m, mu_t indent, mu_t depth) {
    if ((indent && !mu_isnum(indent)) || (depth && !mu_isnum(depth)))
        mu_err_undefined();

    switch (mu_type(m)) {
        case MU_NIL:    return mcstr("nil");
        case MU_NUM:    return num_repr(m);
        case MU_STR:    return str_repr(m);
        case MU_TBL:
        case MU_RTBL:   return tbl_dump(m, indent, depth);
        case MU_FN:
        case MU_BFN:
        case MU_SFN:    return fn_repr(m);
        default:        mu_unreachable;
    }
}

mu_t mu_bin(mu_t m) {
    switch (mu_type(m)) {
        case MU_NUM:    return num_bin(m);
        case MU_STR:    return str_bin(m);
        default:        mu_err_undefined();
    }
}

mu_t mu_oct(mu_t m) {
    switch (mu_type(m)) {
        case MU_NUM:    return num_oct(m);
        case MU_STR:    return str_oct(m);
        default:        mu_err_undefined();
    }
}

mu_t mu_hex(mu_t m) {
    switch (mu_type(m)) {
        case MU_NUM:    return num_hex(m);
        case MU_STR:    return str_hex(m);
        default:        mu_err_undefined();
    }
}

// Data structure operations
// These do not consume their first argument
mlen_t mu_len(mu_t m) {
    switch (mu_type(m)) {
        case MU_STR:    return str_len(m);
        case MU_TBL:    
        case MU_RTBL:   return tbl_len(m);
        default:        mu_err_undefined();
    }
}

mu_t mu_tail(mu_t m) {
    switch (mu_type(m)) {
        case MU_TBL:
        case MU_RTBL:   return tbl_tail(m);
        default:        mu_err_undefined();
    }
}

mu_t mu_const(mu_t m) { // TODO This one does consume, should note these
    switch (mu_type(m)) {
        case MU_TBL:    return tbl_const(m);
        default:        return m;
    }
}

void mu_push(mu_t m, mu_t v, mu_t i) {
    if (i && !mu_isnum(i))
        mu_err_undefined();

    switch (mu_type(m)) {
        case MU_TBL:
        case MU_RTBL:   return tbl_push(m, v, i);
        default:        mu_err_undefined();
    }
}

mu_t mu_pop(mu_t m, mu_t i) {
    if (i && !mu_isnum(i))
        mu_err_undefined();

    switch (mu_type(m)) {
        case MU_TBL:
        case MU_RTBL:   return tbl_pop(m, i);
        default:        mu_err_undefined();
    }
}

mu_t mu_concat(mu_t a, mu_t b, mu_t offset) {
    if (mu_type(a) != mu_type(b))
        mu_cerr(mcstr("incompatible types"),
                mcstr("incompatible arguments"));

    if (offset && !mu_isnum(offset))
        mu_err_undefined();

    switch (mu_type(a)) {
        case MU_STR:    return str_concat(a, b);
        case MU_TBL:
        case MU_RTBL:   return tbl_concat(a, b, offset);
        default:        mu_err_undefined();
    }
}

mu_t mu_subset(mu_t m, mu_t lower, mu_t upper) {
    if (!mu_isnum(lower) || (upper && !mu_isnum(upper)))
        mu_err_undefined();

    switch (mu_type(m)) {
        case MU_STR:    return str_subset(m, lower, upper);
        case MU_TBL:    
        case MU_RTBL:   return tbl_subset(m, lower, upper);
        default:        mu_err_undefined();
    }
}

// String operations
mu_t mu_find(mu_t m, mu_t sub) {
    if (!mu_isstr(sub))
        mu_err_undefined();

    switch (mu_type(m)) {
        case MU_STR:    return str_find(m, sub);
        default:        mu_err_undefined();
    }
}

mu_t mu_replace(mu_t m, mu_t sub, mu_t rep, mu_t max) {
    if (!mu_isstr(sub) || !mu_isstr(rep) ||
        (max && !mu_isnum(max)))
        mu_err_undefined();

    switch (mu_type(m)) {
        case MU_STR:    return str_replace(m, sub, rep, max);
        default:        mu_err_undefined();
    }
}

mu_t mu_split(mu_t m, mu_t delim) {
    if (delim && !mu_isstr(delim))
        mu_err_undefined();

    switch (mu_type(m)) {
        case MU_STR:    return str_split(m, delim);
        default:        mu_err_undefined();
    }
}

mu_t mu_join(mu_t m, mu_t delim) {
    if (delim && !mu_isstr(delim))
        mu_err_undefined();

    return str_join(mu_iter(m), delim);
}

mu_t mu_pad(mu_t m, mu_t len, mu_t pad) {
    if (!mu_isnum(len) || (pad && !mu_isstr(pad)))
        mu_err_undefined();

    switch (mu_type(m)) {
        case MU_STR:    return str_pad(m, len, pad);
        default:        mu_err_undefined();
    }
}

mu_t mu_strip(mu_t m, mu_t dir, mu_t pad) {
    if ((dir && !mu_isnum(dir)) || (pad && !mu_isstr(pad)))
        mu_err_undefined();

    switch (mu_type(m)) {
        case MU_STR:    return str_strip(m, dir, pad);
        default:        mu_err_undefined();
    }
}

// Function operations
mu_t mu_bind(mu_t m, mu_t args) {
    if (!mu_istbl(args))
        mu_err_undefined();

    return fn_bind(m, args);
}

mu_t mu_comp(mu_t ms) {
    return fn_comp(ms);
}

mu_t mu_map(mu_t m, mu_t iter) {
    return fn_map(m, mu_iter(iter));
}

mu_t mu_filter(mu_t m, mu_t iter) {
    return fn_filter(m, mu_iter(iter));
}

mu_t mu_reduce(mu_t m, mu_t iter, mu_t inits) {
    if (inits && !mu_istbl(inits))
        mu_err_undefined();

    return fn_reduce(m, mu_iter(iter), inits);
}

bool mu_any(mu_t m, mu_t iter) {
    return fn_any(m, mu_iter(iter));
}

bool mu_all(mu_t m, mu_t iter) {
    return fn_all(m, mu_iter(iter));
}

// Iterators and generators
mu_t mu_iter(mu_t m) {
    switch (mu_type(m)) {
        case MU_STR:    return str_iter(m);
        case MU_TBL:
        case MU_RTBL:   return tbl_iter(m);
        case MU_FN:
        case MU_BFN:
        case MU_SFN:    return m;
        default:        mu_err_undefined();
    }
}

mu_t mu_pairs(mu_t m) {
    switch (mu_type(m)) {
        case MU_TBL:
        case MU_RTBL:   return tbl_pairs(m);
        default:        return mu_zip(mmlist({
                            mu_range(mnil, mnil, mnil), mu_iter(m)
                        }));
    }
}

mu_t mu_range(mu_t start, mu_t stop, mu_t step) {
    if ((start && !mu_isnum(start)) ||
        (stop && !mu_isnum(stop)) ||
        (step && !mu_isnum(step)))
        mu_err_undefined();

    return fn_range(start, stop, step);
}

mu_t mu_repeat(mu_t value, mu_t times) {
    if (times && !mu_isnum(times))
        mu_err_undefined();

    return fn_repeat(value, times);
}

// Iterator manipulation
mu_t mu_zip(mu_t iters) {
    return fn_zip(mu_iter(iters));
}

mu_t mu_chain(mu_t iters) {
    return fn_chain(mu_iter(iters));
}

mu_t mu_take(mu_t m, mu_t iter) {
    return fn_take(m, mu_iter(iter));
}

mu_t mu_drop(mu_t m, mu_t iter) {
    return fn_drop(m, mu_iter(iter));
}

// Iterator ordering
mu_t mu_min(mu_t iter) {
    return fn_min(mu_iter(iter));
}

mu_t mu_max(mu_t iter) {
    return fn_max(mu_iter(iter));
}

mu_t mu_reverse(mu_t iter) {
    return fn_reverse(mu_iter(iter));
}

mu_t mu_sort(mu_t iter) {
    return fn_sort(mu_iter(iter));
}

// Random number generation
mu_t mu_seed(mu_t m) {
    if (!mu_isnum(m))
        mu_err_undefined();

    return num_seed(m);
}

mu_t mu_random(void) {
    mu_t gen = tbl_lookup(MU_BUILTINS, mcstr("random"));
    return mu_call(gen, 0x01);
}


// Builtin table bindings

// Type casts
static mc_t mb_num(mu_t *frame) {
    frame[0] = mu_num(frame[0]);
    return 1;
}

static mc_t mb_str(mu_t *frame) {
    frame[0] = mu_str(frame[0]);
    return 1;
}

static mc_t mb_tbl(mu_t *frame) {
    frame[0] = mu_tbl(frame[0], frame[1]);
    return 1;
}

static mc_t mb_fn(mu_t *frame) {
    frame[0] = mu_fn(frame[0]);
    return 1;
}

// Logic operations
static mc_t mb_not(mu_t *frame) {
    mu_dec(frame[0]);
    frame[0] = !frame[0] ? muint(1) : mnil;
    return 1;
}

static mc_t mb_equals(mu_t *frame) {
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = frame[0] == frame[1] ? muint(1) : mnil;
    return 1;
}

static mc_t mb_not_equals(mu_t *frame) {
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = frame[0] != frame[1] ? muint(1) : mnil;
    return 1;
}

static mc_t mb_is(mu_t *frame) {
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = mu_is(frame[0], frame[1]) ? muint(1) : mnil;
    return 1;
}

static mc_t mb_lt(mu_t *frame) {
    mint_t cmp = mu_cmp(frame[0], frame[1]);
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = cmp < 0 ? muint(1) : mnil;
    return 1;
}

static mc_t mb_lte(mu_t *frame) {
    mint_t cmp = mu_cmp(frame[0], frame[1]);
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = cmp <= 0 ? muint(1) : mnil;
    return 1;
}

static mc_t mb_gt(mu_t *frame) {
    mint_t cmp = mu_cmp(frame[0], frame[1]);
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = cmp > 0 ? muint(1) : mnil;
    return 1;
}

static mc_t mb_gte(mu_t *frame) {
    mint_t cmp = mu_cmp(frame[0], frame[1]);
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = cmp >= 0 ? muint(1) : mnil;
    return 1;
}

// Arithmetic operations
static mc_t mb_add(mu_t *frame) {
    if (!frame[1]) frame[0] = mu_pos(frame[0]);
    else           frame[0] = mu_add(frame[0], frame[1]);
    return 1;
}

static mc_t mb_sub(mu_t *frame) {
    if (!frame[1]) frame[0] = mu_neg(frame[0]);
    else           frame[0] = mu_sub(frame[0], frame[1]);
    return 1;
}

static mc_t mb_mul(mu_t *frame) {
    frame[0] = mu_mul(frame[0], frame[1]);
    return 1;
}

static mc_t mb_div(mu_t *frame) {
    frame[0] = mu_div(frame[0], frame[1]);
    return 1;
}

static mc_t mb_abs(mu_t *frame) {
    frame[0] = mu_abs(frame[0]);
    return 1;
}

static mc_t mb_floor(mu_t *frame) {
    frame[0] = mu_floor(frame[0]);
    return 1;
}

static mc_t mb_ceil(mu_t *frame) {
    frame[0] = mu_ceil(frame[0]);
    return 1;
}

static mc_t mb_idiv(mu_t *frame) {
    frame[0] = mu_idiv(frame[0], frame[1]);
    return 1;
}

static mc_t mb_mod(mu_t *frame) {
    frame[0] = mu_mod(frame[0], frame[1]);
    return 1;
}

static mc_t mb_pow(mu_t *frame) {
    frame[0] = mu_pow(frame[0], frame[1]);
    return 1;
}

static mc_t mb_log(mu_t *frame) {
    frame[0] = mu_log(frame[0], frame[1]);
    return 1;
}

static mc_t mb_cos(mu_t *frame) {
    frame[0] = mu_cos(frame[0]);
    return 1;
}

static mc_t mb_acos(mu_t *frame) {
    frame[0] = mu_acos(frame[0]);
    return 1;
}

static mc_t mb_sin(mu_t *frame) {
    frame[0] = mu_sin(frame[0]);
    return 1;
}

static mc_t mb_asin(mu_t *frame) {
    frame[0] = mu_asin(frame[0]);
    return 1;
}

static mc_t mb_tan(mu_t *frame) {
    frame[0] = mu_tan(frame[0]);
    return 1;
}

static mc_t mb_atan(mu_t *frame) {
    frame[0] = mu_atan(frame[0], frame[1]);
    return 1;
}

// Bitwise operations
static mc_t mb_and(mu_t *frame) {
    frame[0] = mu_and(frame[0], frame[1]);
    return 1;
}

static mc_t mb_or(mu_t *frame) {
    frame[0] = mu_or(frame[0], frame[1]);
    return 1;
}

static mc_t mb_xor(mu_t *frame) {
    if (!frame[1]) frame[0] = mu_not(frame[0]);
    else           frame[0] = mu_xor(frame[0], frame[1]);
    return 1;
}

static mc_t mb_diff(mu_t *frame) {
    frame[0] = mu_diff(frame[0], frame[1]);
    return 1;
}

static mc_t mb_shl(mu_t *frame) {
    frame[0] = mu_shl(frame[0], frame[1]);
    return 1;
}

static mc_t mb_shr(mu_t *frame) {
    frame[0] = mu_shr(frame[0], frame[1]);
    return 1;
}

// String representation
static mc_t mb_parse(mu_t *frame) {
    frame[0] = mu_parse(frame[0]);
    return 1;
}

static mc_t mb_repr(mu_t *frame) {
    frame[0] = mu_dump(frame[0], frame[1], frame[2]);
    return 1;
}

static mc_t mb_bin(mu_t *frame) {
    frame[0] = mu_bin(frame[0]);
    return 1;
}

static mc_t mb_oct(mu_t *frame) {
    frame[0] = mu_oct(frame[0]);
    return 1;
}

static mc_t mb_hex(mu_t *frame) {
    frame[0] = mu_hex(frame[0]);
    return 1;
}

// Data struct operations
static mc_t mb_len(mu_t *frame) {
    mlen_t len = mu_len(frame[0]);
    mu_dec(frame[0]);
    frame[0] = muint(len);
    return 1;
}

static mc_t mb_tail(mu_t *frame) {
    mu_t tail = mu_tail(frame[0]);
    mu_dec(frame[0]);
    frame[0] = tail;
    return 1;
}

static mc_t mb_const(mu_t *frame) {
    frame[0] = mu_const(frame[0]);
    return 1;
}

static mc_t mb_push(mu_t *frame) {
    mu_push(frame[0], frame[1], frame[2]);
    mu_dec(frame[0]);
    return 0;
}

static mc_t mb_pop(mu_t *frame) {
    mu_t v = mu_pop(frame[0], frame[1]);
    mu_dec(frame[0]);
    frame[0] = v;
    return 1;
}

static mc_t mb_concat(mu_t *frame) {
    frame[0] = mu_concat(frame[0], frame[1], frame[2]);
    return 1;
}

static mc_t mb_subset(mu_t *frame) {
    frame[0] = mu_subset(frame[0], frame[1], frame[2]);
    return 1;
}

// String operations
static mc_t mb_find(mu_t *frame) {
    frame[0] = mu_find(frame[0], frame[1]);
    return 1;
}

static mc_t mb_replace(mu_t *frame) {
    frame[0] = mu_replace(frame[0], frame[1], frame[2], frame[3]);
    return 1;
}

static mc_t mb_split(mu_t *frame) {
    frame[0] = mu_split(frame[0], frame[1]);
    return 1;
}

static mc_t mb_join(mu_t *frame) {
    frame[0] = mu_join(frame[0], frame[1]);
    return 1;
}

static mc_t mb_pad(mu_t *frame) {
    frame[0] = mu_pad(frame[0], frame[1], frame[2]);
    return 1;
}

static mc_t mb_strip(mu_t *frame) {
    if (mu_isstr(frame[1])) {
        mu_dec(frame[2]);
        frame[2] = frame[1];
        frame[1] = mnil;
    }

    frame[0] = mu_strip(frame[0], frame[1], frame[2]);
    return 1;
}
        
// Function operations
static mc_t mb_bind(mu_t *frame) {
    mu_t m = tbl_pop(frame[0], muint(0));
    frame[0] = mu_bind(m, frame[0]);
    return 1;
}

static mc_t mb_comp(mu_t *frame) {
    frame[0] = mu_comp(frame[0]);
    return 1;
}

static mc_t mb_map(mu_t *frame) {
    frame[0] = mu_map(frame[0], frame[1]);
    return 1;
}

static mc_t mb_filter(mu_t *frame) {
    frame[0] = mu_filter(frame[0], frame[1]);
    return 1;
}

static mc_t mb_reduce(mu_t *frame) {
    mu_t f = tbl_pop(frame[0], muint(0));
    mu_t i = tbl_pop(frame[0], muint(0));
    frame[0] = mu_reduce(f, i, frame[0]);
    return 0xf;
}

static mc_t mb_any(mu_t *frame) {
    frame[0] = mu_any(frame[0], frame[1]) ? muint(1) : mnil;
    return 1;
}

static mc_t mb_all(mu_t *frame) {
    frame[0] = mu_all(frame[0], frame[1]) ? muint(1) : mnil;
    return 1;
}

// Iterators and generators
static mc_t mb_iter(mu_t *frame) {
    frame[0] = mu_iter(frame[0]);
    return 1;
}

static mc_t mb_pairs(mu_t *frame) {
    frame[0] = mu_pairs(frame[0]);
    return 1;
}

static mc_t mb_range(mu_t *frame) {
    if (!frame[1]) {
        frame[1] = frame[0];
        frame[0] = mnil;
    }

    frame[0] = mu_range(frame[0], frame[1], frame[2]);
    return 1;
}

static mc_t mb_repeat(mu_t *frame) {
    frame[0] = mu_repeat(frame[0], frame[1]);
    return 1;
}

// Iterator manipulation
static mc_t mb_zip(mu_t *frame) {
    if (tbl_len(frame[0]) == 0) {
        mu_err_undefined();
    } else if (tbl_len(frame[0]) == 1) {
        mu_t iter = tbl_lookup(frame[0], muint(0));
        mu_dec(frame[0]);
        frame[0] = mu_zip(iter);
    } else {
        frame[0] = mu_zip(frame[0]);
    }

    return 1;
}

static mc_t mb_chain(mu_t *frame) {
    if (tbl_len(frame[0]) == 0) {
        mu_err_undefined();
    } else if (tbl_len(frame[0]) == 1) {
        mu_t iter = tbl_lookup(frame[0], muint(0));
        mu_dec(frame[0]);
        frame[0] = mu_chain(iter);
    } else {
        frame[0] = mu_chain(frame[0]);
    }

    return 1;
}

static mc_t mb_take(mu_t *frame) {
    frame[0] = mu_take(frame[0], frame[1]);
    return 1;
}

static mc_t mb_drop(mu_t *frame) {
    frame[0] = mu_drop(frame[0], frame[1]);
    return 1;
}

// Iterator ordering
static mc_t mb_min(mu_t *frame) {
    if (tbl_len(frame[0]) == 0) {
        mu_err_undefined();
    } else if (tbl_len(frame[0]) == 1) {
        mu_t iter = tbl_lookup(frame[0], muint(0));
        mu_dec(frame[0]);
        frame[0] = mu_min(iter);
    } else {
        frame[0] = mu_min(frame[0]);
    }

    return 0xf;
}

static mc_t mb_max(mu_t *frame) {
    if (tbl_len(frame[0]) == 0) {
        mu_err_undefined();
    } else if (tbl_len(frame[0]) == 1) {
        mu_t iter = tbl_lookup(frame[0], muint(0));
        mu_dec(frame[0]);
        frame[0] = mu_max(iter);
    } else {
        frame[0] = mu_max(frame[0]);
    }

    return 0xf;
}

static mc_t mb_reverse(mu_t *frame) {
    frame[0] = mu_reverse(frame[0]);
    return 1;
}

static mc_t mb_sort(mu_t *frame) {
    frame[0] = mu_sort(frame[0]);
    return 1;
}

static mc_t mb_seed(mu_t *frame) {
    frame[0] = mu_seed(frame[0]);
    return 1;
}


// Types
mu_pure mu_t mu_num_type(void) { return mcfn(0x1, mb_num); }
mu_pure mu_t mu_str_type(void) { return mcfn(0x1, mb_str); }
mu_pure mu_t mu_tbl_type(void) { return mcfn(0x2, mb_tbl); }
mu_pure mu_t mu_fn_type(void)  { return mcfn(0x1, mb_fn); }


// Builtins table
mu_pure mu_t mu_builtins(void) {
    return mctbl({
        // Constants
        { mcstr("true"),    muint(1) },
        { mcstr("inf"),     MU_INF },
        { mcstr("e"),       MU_E },
        { mcstr("pi"),      MU_PI },
        { mcstr("id"),      MU_ID },

        // Type casts
        { mcstr("num"),     MU_NUM_TYPE },
        { mcstr("str"),     MU_STR_TYPE },
        { mcstr("tbl"),     MU_TBL_TYPE },
        { mcstr("fn_"),     MU_FN_TYPE },

        // Logic operations
        { mcstr("!"),       mcfn(0x1, mb_not) },
        { mcstr("=="),      mcfn(0x2, mb_equals) },
        { mcstr("!="),      mcfn(0x2, mb_not_equals) },
        { mcstr("is"),      mcfn(0x2, mb_is) },
        { mcstr("<"),       mcfn(0x2, mb_lt) },
        { mcstr("<="),      mcfn(0x2, mb_lte) },
        { mcstr(">"),       mcfn(0x2, mb_gt) },
        { mcstr(">="),      mcfn(0x2, mb_gte) },

        // Arithmetic operations
        { mcstr("+"),       mcfn(0x2, mb_add) },
        { mcstr("-"),       mcfn(0x2, mb_sub) },
        { mcstr("*"),       mcfn(0x2, mb_mul) },
        { mcstr("/"),       mcfn(0x2, mb_div) },

        { mcstr("abs"),     mcfn(0x1, mb_abs) },
        { mcstr("floor"),   mcfn(0x1, mb_floor) },
        { mcstr("ceil"),    mcfn(0x1, mb_ceil) },
        { mcstr("//"),      mcfn(0x2, mb_idiv) },
        { mcstr("%"),       mcfn(0x2, mb_mod) },

        { mcstr("^"),       mcfn(0x2, mb_pow) },
        { mcstr("log"),     mcfn(0x2, mb_log) },

        { mcstr("cos"),     mcfn(0x1, mb_cos) },
        { mcstr("acos"),    mcfn(0x1, mb_acos) },
        { mcstr("sin"),     mcfn(0x1, mb_sin) },
        { mcstr("asin"),    mcfn(0x1, mb_asin) },
        { mcstr("tan"),     mcfn(0x1, mb_tan) },
        { mcstr("atan"),    mcfn(0x2, mb_atan) },

        // Bitwise/Set operations
        { mcstr("&"),       mcfn(0x2, mb_and) },
        { mcstr("|"),       mcfn(0x2, mb_or) },
        { mcstr("~"),       mcfn(0x2, mb_xor) },
        { mcstr("&~"),      mcfn(0x2, mb_diff) },

        { mcstr("<<"),      mcfn(0x2, mb_shl) },
        { mcstr(">>"),      mcfn(0x2, mb_shr) },

        // String representation
        { mcstr("parse"),   mcfn(0x1, mb_parse) },
        { mcstr("repr"),    mcfn(0x3, mb_repr) },

        { mcstr("bin"),     mcfn(0x1, mb_bin) },
        { mcstr("oct"),     mcfn(0x1, mb_oct) },
        { mcstr("hex"),     mcfn(0x1, mb_hex) },

        // Data structure operations
        { mcstr("len"),     mcfn(0x1, mb_len) },
        { mcstr("tail"),    mcfn(0x1, mb_tail) },
        { mcstr("const"),   mcfn(0x1, mb_const) },

        { mcstr("push"),    mcfn(0x3, mb_push) },
        { mcstr("pop"),     mcfn(0x2, mb_pop) },

        { mcstr("++"),      mcfn(0x3, mb_concat) },
        { mcstr("sub"),     mcfn(0x3, mb_subset) },

        // String operations
        { mcstr("find"),    mcfn(0x2, mb_find) },
        { mcstr("replace"), mcfn(0x4, mb_replace) },
        { mcstr("split"),   mcfn(0x2, mb_split) },
        { mcstr("join"),    mcfn(0x2, mb_join) },
        { mcstr("pad"),     mcfn(0x3, mb_pad) },
        { mcstr("strip"),   mcfn(0x3, mb_strip) },

        // Function operations
        { mcstr("bind"),    mcfn(0xf, mb_bind) },
        { mcstr("comp"),    mcfn(0xf, mb_comp) },

        { mcstr("map"),     mcfn(0x2, mb_map) },
        { mcstr("filter"),  mcfn(0x2, mb_filter) },
        { mcstr("reduce"),  mcfn(0xf, mb_reduce) },

        { mcstr("any"),     mcfn(0x2, mb_any) },
        { mcstr("all"),     mcfn(0x2, mb_all) },

        // Iterators and generators
        { mcstr("iter"),    mcfn(0x1, mb_iter) },
        { mcstr("pairs"),   mcfn(0x1, mb_pairs) },

        { mcstr("range"),   mcfn(0x3, mb_range) },
        { mcstr("repeat"),  mcfn(0x2, mb_repeat) },

        // Iterator manipulation
        { mcstr("zip"),     mcfn(0xf, mb_zip) },
        { mcstr("chain"),   mcfn(0xf, mb_chain) },

        { mcstr("take"),    mcfn(0x2, mb_take) },
        { mcstr("drop"),    mcfn(0x2, mb_drop) },

        // Iterator ordering
        { mcstr("min"),     mcfn(0xf, mb_min) },
        { mcstr("max"),     mcfn(0xf, mb_max) },

        { mcstr("reverse"), mcfn(0x1, mb_reverse) },
        { mcstr("sort"),    mcfn(0x1, mb_sort) },

        // Random number generation
        { mcstr("seed"),    mcfn(0x1, mb_seed) },
        { mcstr("random"),  mu_seed(muint(0)) },
    });
}
