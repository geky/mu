#include "num.h"

#include "str.h"
#include "tbl.h"
#include "fn.h"
#include "parse.h"

#include <math.h>


// Binary digits of precision
#ifdef MU64
#define MU_DIGITS (52 - 3)
#else
#define MU_DIGITS (23 - 3)
#endif


// Number constants
MU_GEN_STR(mu_gen_key_inf,  "inf")
MU_GEN_STR(mu_gen_key_ninf, "ninf")
MU_GEN_STR(mu_gen_key_e,    "e")
MU_GEN_STR(mu_gen_key_pi,   "pi")

MU_GEN_FLOAT(mu_gen_inf,    INFINITY)
MU_GEN_FLOAT(mu_gen_ninf,   -INFINITY)
MU_GEN_FLOAT(mu_gen_e,      2.71828182845904523536)
MU_GEN_FLOAT(mu_gen_pi,     3.14159265358979323846)


// Number creating macro assuming NaN and -0 not possible
mu_inline mu_t mnum(mfloat_t n) {
    return (mu_t)(MTNUM + (~7 &
        ((union { mfloat_t n; muint_t u; }){(mfloat_t)n}).u));
}

// Conversion from floats
// Number cannot be NaNs or negative zero to garuntee bitwise equality
mu_t mu_num_fromfloat(mfloat_t n) {
    if (n != n) {
        mu_errorf("operation resulted in nan");
    }

    if (n == 0) {
        n = 0;
    }

    return mnum(n);
}

// Comparison operation
mint_t mu_num_cmp(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    mfloat_t afloat = mu_num_getfloat(a);
    mfloat_t bfloat = mu_num_getfloat(b);

    return afloat > bfloat ? +1 :
           afloat < bfloat ? -1 : 0;
}


// Arithmetic operations
mu_t mu_num_neg(mu_t a) {
    mu_assert(mu_isnum(a));

    if (a == mu_num_fromuint(0)) {
        return a;
    } else {
        return mnum(-mu_num_getfloat(a));
    }
}

mu_t mu_num_add(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mu_num_fromfloat(mu_num_getfloat(a) + mu_num_getfloat(b));
}

mu_t mu_num_sub(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mu_num_fromfloat(mu_num_getfloat(a) - mu_num_getfloat(b));
}

mu_t mu_num_mul(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mu_num_fromfloat(mu_num_getfloat(a) * mu_num_getfloat(b));
}

mu_t mu_num_div(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mu_num_fromfloat(mu_num_getfloat(a) / mu_num_getfloat(b));
}

mu_t mu_num_idiv(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mu_num_fromfloat(floor(mu_num_getfloat(a) / mu_num_getfloat(b)));
}

mu_t mu_num_mod(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    mfloat_t base = mu_num_getfloat(b);
    mfloat_t mod = fmod(mu_num_getfloat(a), base);

    // Handle truncation for negative values
    if (mod*base < 0) {
        mod += base;
    }

    return mu_num_fromfloat(mod);
}

mu_t mu_num_pow(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mu_num_fromfloat(pow(mu_num_getfloat(a), mu_num_getfloat(b)));
}

mu_t mu_num_log(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && (!b || mu_isnum(b)));
    return mu_num_fromfloat(
            log(mu_num_getfloat(a)) / log(mu_num_getfloat(b ? b : MU_E)));
}

mu_t mu_num_abs(mu_t a) {
    mu_assert(mu_isnum(a));
    return mnum(fabs(mu_num_getfloat(a)));
}

mu_t mu_num_floor(mu_t a) {
    mu_assert(mu_isnum(a));
    return mu_num_fromfloat(floor(mu_num_getfloat(a)));
}

mu_t mu_num_ceil(mu_t a)  {
    mu_assert(mu_isnum(a));
    return mu_num_fromfloat(ceil(mu_num_getfloat(a)));
}

mu_t mu_num_cos(mu_t a)  {
    mu_assert(mu_isnum(a));
    return mu_num_fromfloat(cos(mu_num_getfloat(a)));
}

mu_t mu_num_acos(mu_t a) {
    mu_assert(mu_isnum(a));
    return mu_num_fromfloat(acos(mu_num_getfloat(a)));
}

mu_t mu_num_sin(mu_t a) {
    mu_assert(mu_isnum(a));
    return mu_num_fromfloat(sin(mu_num_getfloat(a)));
}

mu_t mu_num_asin(mu_t a) {
    mu_assert(mu_isnum(a));
    return mu_num_fromfloat(asin(mu_num_getfloat(a)));
}

mu_t mu_num_tan(mu_t a) {
    mu_assert(mu_isnum(a));
    return mu_num_fromfloat(tan(mu_num_getfloat(a)));
}

mu_t mu_num_atan(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && (!b || mu_isnum(b)));

    if (!b) {
        return mu_num_fromfloat(atan(mu_num_getfloat(a)));
    } else {
        return mu_num_fromfloat(atan2(mu_num_getfloat(a), mu_num_getfloat(b)));
    }
}

// Bitwise operations
mu_t mu_num_not(mu_t a) {
    mu_assert(mu_isnum(a));
    return mu_num_fromuint((muinth_t)(~mu_num_getuint(a)));
}

mu_t mu_num_and(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mu_num_fromuint((muinth_t)(mu_num_getuint(a) & mu_num_getuint(b)));
}

mu_t mu_num_or(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mu_num_fromuint((muinth_t)(mu_num_getuint(a) | mu_num_getuint(b)));
}

mu_t mu_num_xor(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mu_num_fromuint((muinth_t)(mu_num_getuint(a) ^ mu_num_getuint(b)));
}


mu_t mu_num_shl(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mu_num_fromuint((muint_t)(mu_num_getuint(a) << mu_num_getuint(b)));
}

mu_t mu_num_shr(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mu_num_fromuint((muint_t)(mu_num_getuint(a) >> mu_num_getuint(b)));
}


// Convert string representation to variable
mu_t mu_num_parse(const mbyte_t **ppos, const mbyte_t *end) {
    const mbyte_t *pos = *ppos;
    mu_t n = mu_num_fromuint(0);
    mu_t sign = mu_num_fromint(+1);
    muint_t base = 10;

    if (pos < end && *pos == '+') {
        sign = mu_num_fromint(+1); pos++;
    } else if (pos < end && *pos == '-') {
        sign = mu_num_fromint(-1); pos++;
    }

    if (pos+2 < end && memcmp(pos, "inf", 3) == 0) {
        *ppos = pos + 3;
        return mu_num_mul(sign, MU_INF);
    }

    if (pos+2 < end && pos[0] == '0') {
        if (pos[1] == 'b' || pos[1] == 'B') {
            base = 2;  pos += 2;
        } else if (pos[1] == 'o' || pos[1] == 'O') {
            base = 8;  pos += 2;
        } else if (pos[1] == 'd' || pos[1] == 'D') {
            base = 10; pos += 2;
        } else if (pos[1] == 'x' || pos[1] == 'X') {
            base = 16; pos += 2;
        }
    }

    while (pos < end && mu_fromascii(*pos) < base) {
        n = mu_num_mul(n, mu_num_fromuint(base));
        n = mu_num_add(n, mu_num_fromuint(mu_fromascii(*pos++)));
    }

    if (pos < end && *pos == '.') {
        mu_t scale = mu_num_fromuint(1);
        pos++;

        while (pos < end && mu_fromascii(*pos) < base) {
            scale = mu_num_mul(scale, mu_num_fromuint(base));
            n = mu_num_add(n, mu_num_div(
                    mu_num_fromuint(mu_fromascii(*pos++)), scale));
        }
    }

    if (pos < end && (*pos == 'e' || *pos == 'E' ||
                      *pos == 'p' || *pos == 'p')) {
        mu_t expbase = mu_num_fromuint((*pos == 'e' || *pos == 'E') ? 10 : 2);
        mu_t exp = mu_num_fromuint(0);
        mu_t sign = mu_num_fromint(+1);
        pos++;

        if (pos < end && *pos == '+') {
            sign = mu_num_fromint(+1); pos++;
        } else if (pos < end && *pos == '-') {
            sign = mu_num_fromint(-1); pos++;
        }

        while (pos < end && mu_fromascii(*pos) < 10) {
            exp = mu_num_mul(exp, mu_num_fromuint(10));
            exp = mu_num_add(exp, mu_num_fromuint(mu_fromascii(*pos++)));
        }

        n = mu_num_mul(n, mu_num_pow(expbase, mu_num_mul(sign, exp)));
    }

    *ppos = pos;
    return mu_num_mul(sign, n);
}

// Obtains a string representation of a number
static void mu_num_base_ipart(mu_t *s, muint_t *i, mu_t n, mu_t base) {
    muint_t j = *i;

    while (mu_num_cmp(n, mu_num_fromuint(0)) > 0) {
        mu_t d = mu_num_mod(n, base);
        mu_buf_push(s, i, mu_toascii(mu_num_getuint(d)));
        n = mu_num_idiv(n, base);
    }

    mbyte_t *a = (mbyte_t *)mu_buf_getdata(*s) + j;
    mbyte_t *b = (mbyte_t *)mu_buf_getdata(*s) + *i - 1;

    while (a < b) {
        mbyte_t t = *a;
        *a = *b;
        *b = t;
        a++; b--;
    }
}

static void mu_num_base_fpart(mu_t *s, muint_t *i, mu_t n,
                           mu_t base, muint_t digits) {
    mu_t error = mu_num_pow(base, mu_num_fromint(-digits));
    mu_t digit = mu_num_fromint(-1);
    n = mu_num_mod(n, mu_num_fromuint(1));

    for (muint_t j = 0; j < digits; j++) {
        if (mu_num_cmp(n, error) <= 0) {
            break;
        }

        if (digit == mu_num_fromint(-1)) {
            mu_buf_push(s, i, '.');
        }

        mu_t p = mu_num_pow(base, digit);
        mu_t d = mu_num_idiv(n, p);
        mu_buf_push(s, i, mu_toascii(mu_num_getuint(d)));

        n = mu_num_mod(n, p);
        digit = mu_num_sub(digit, mu_num_fromuint(1));
    }
}

static mu_t mu_num_base(mu_t n, char c, mu_t base, char expc, mu_t expbase) {
    if (n == mu_num_fromuint(0)) {
        if (c) return mu_str_format("0%c0", c);
        else   return mu_str_format("0");
    } else if (n == MU_INF) {
        return mu_str_format("+inf");
    } else if (n == MU_NINF) {
        return mu_str_format("-inf");
    } else {
        mu_t s = mu_buf_create(0);
        muint_t i = 0;

        if (mu_num_cmp(n, mu_num_fromuint(0)) < 0) {
            n = mu_num_neg(n);
            mu_buf_push(&s, &i, '-');
        }

        if (c) {
            mu_buf_append(&s, &i, (mbyte_t[2]){'0', c}, 2);
        }

        mu_t exp = mu_num_floor(mu_num_log(n, expbase));
        mu_t sig = mu_num_floor(mu_num_log(n, base));
        mu_t digits = mu_num_ceil(mu_num_div(mu_num_fromuint(MU_DIGITS),
                               mu_num_log(base, mu_num_fromuint(2))));

        bool scientific = mu_num_cmp(sig, digits) >= 0 ||
                          mu_num_cmp(sig, mu_num_fromint(-1)) < 0;

        if (scientific) {
            n = mu_num_div(n, mu_num_pow(expbase, exp));
        }

        muint_t j = i;
        mu_num_base_ipart(&s, &i, n, base);
        mu_num_base_fpart(&s, &i, n, base, mu_num_getuint(digits) - (i-j));

        if (scientific) {
            mu_buf_push(&s, &i, expc);

            if (mu_num_cmp(exp, mu_num_fromuint(0)) < 0) {
                exp = mu_num_neg(exp);
                mu_buf_push(&s, &i, '-');
            }

            mu_num_base_ipart(&s, &i, exp, mu_num_fromuint(10));
        }

        return mu_str_intern(s, i);
    }
}

mu_t mu_num_repr(mu_t n) {
    mu_assert(mu_isnum(n));
    return mu_num_base(n, 0, mu_num_fromuint(10), 'e', mu_num_fromuint(10));
}

mu_t mu_num_bin(mu_t n) {
    mu_assert(mu_isnum(n));
    return mu_num_base(n, 'b', mu_num_fromuint(2), 'p', mu_num_fromuint(2));
}

mu_t mu_num_oct(mu_t n) {
    mu_assert(mu_isnum(n));
    return mu_num_base(n, 'o', mu_num_fromuint(8), 'p', mu_num_fromuint(2));
}

mu_t mu_num_hex(mu_t n) {
    mu_assert(mu_isnum(n));
    return mu_num_base(n, 'x', mu_num_fromuint(16), 'p', mu_num_fromuint(2));
}


// Number related Mu functions
static mcnt_t mu_bfn_num(mu_t *frame) {
    mu_t m = frame[0];

    switch (mu_gettype(m)) {
        case MTNIL:
            frame[0] = mu_num_fromuint(0);
            return 1;

        case MTNUM:
            frame[0] = m;
            return 1;

        case MTSTR:
            if (mu_str_getlen(m) == 1) {
                frame[0] = mu_num_fromuint(mu_str_getdata(m)[0]);
                mu_str_dec(m);
                return 1;
            }
            break;

        default:
            break;
    }

    mu_error_cast(MU_KEY_NUM, m);
}

MU_GEN_STR(mu_gen_key_num, "num")
MU_GEN_BFN(mu_gen_num, 0x1, mu_bfn_num)

static mcnt_t mu_bfn_add(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || (b && !mu_isnum(b))) {
        mu_error_op(MU_KEY_ADD, 0x2, frame);
    }

    if (!b) {
        frame[0] = a;
    } else {
        frame[0] = mu_num_add(a, b);
    }

    return 1;
}

MU_GEN_STR(mu_gen_key_add, "+")
MU_GEN_BFN(mu_gen_add, 0x2, mu_bfn_add)

static mcnt_t mu_bfn_sub(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || (b && !mu_isnum(b))) {
        mu_error_op(MU_KEY_SUB, 0x2, frame);
    }

    if (!b) {
        frame[0] = mu_num_neg(a);
    } else {
        frame[0] = mu_num_sub(a, b);
    }

    return 1;
}

MU_GEN_STR(mu_gen_key_sub, "-")
MU_GEN_BFN(mu_gen_sub, 0x2, mu_bfn_sub)

static mcnt_t mu_bfn_mul(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_op(MU_KEY_MUL, 0x2, frame);
    }

    frame[0] = mu_num_mul(a, b);
    return 1;
}

MU_GEN_STR(mu_gen_key_mul, "*")
MU_GEN_BFN(mu_gen_mul, 0x2, mu_bfn_mul)

static mcnt_t mu_bfn_div(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_op(MU_KEY_DIV, 0x2, frame);
    }

    frame[0] = mu_num_div(a, b);
    return 1;
}

MU_GEN_STR(mu_gen_key_div, "/")
MU_GEN_BFN(mu_gen_div, 0x2, mu_bfn_div)

static mcnt_t mu_bfn_abs(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_ABS, 0x1, frame);
    }

    frame[0] = mu_num_abs(a);
    return 1;
}

MU_GEN_STR(mu_gen_key_abs, "abs")
MU_GEN_BFN(mu_gen_abs, 0x1, mu_bfn_abs)

static mcnt_t mu_bfn_floor(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_FLOOR, 0x1, frame);
    }

    frame[0] = mu_num_floor(a);
    return 1;
}

MU_GEN_STR(mu_gen_key_floor, "floor")
MU_GEN_BFN(mu_gen_floor, 0x1, mu_bfn_floor)

static mcnt_t mu_bfn_ceil(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_CEIL, 0x1, frame);
    }

    frame[0] = mu_num_ceil(a);
    return 1;
}

MU_GEN_STR(mu_gen_key_ceil, "ceil")
MU_GEN_BFN(mu_gen_ceil, 0x1, mu_bfn_ceil)

static mcnt_t mu_bfn_idiv(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_op(MU_KEY_IDIV, 0x2, frame);
    }

    frame[0] = mu_num_idiv(a, b);
    return 1;
}

MU_GEN_STR(mu_gen_key_idiv, "//")
MU_GEN_BFN(mu_gen_idiv, 0x2, mu_bfn_idiv)

static mcnt_t mu_bfn_mod(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_op(MU_KEY_MOD, 0x2, frame);
    }

    frame[0] = mu_num_mod(a, b);
    return 1;
}

MU_GEN_STR(mu_gen_key_mod, "%")
MU_GEN_BFN(mu_gen_mod, 0x2, mu_bfn_mod)

static mcnt_t mu_bfn_pow(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_arg(MU_KEY_POW, 0x2, frame);
    }

    frame[0] = mu_num_pow(a, b);
    return 1;
}

MU_GEN_STR(mu_gen_key_pow, "^")
MU_GEN_BFN(mu_gen_pow, 0x2, mu_bfn_pow)

static mcnt_t mu_bfn_log(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || (b && !mu_isnum(b))) {
        mu_error_arg(MU_KEY_LOG, 0x2, frame);
    }

    frame[0] = mu_num_log(a, b);
    return 1;
}

MU_GEN_STR(mu_gen_key_log, "log")
MU_GEN_BFN(mu_gen_log, 0x2, mu_bfn_log)

static mcnt_t mu_bfn_cos(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_COS, 0x1, frame);
    }

    frame[0] = mu_num_cos(a);
    return 1;
}

MU_GEN_STR(mu_gen_key_cos, "cos")
MU_GEN_BFN(mu_gen_cos, 0x1, mu_bfn_cos)

static mcnt_t mu_bfn_acos(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_ACOS, 0x1, frame);
    }

    frame[0] = mu_num_acos(a);
    return 1;
}

MU_GEN_STR(mu_gen_key_acos, "acos")
MU_GEN_BFN(mu_gen_acos, 0x1, mu_bfn_acos)

static mcnt_t mu_bfn_sin(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_SIN, 0x1, frame);
    }

    frame[0] = mu_num_sin(a);
    return 1;
}

MU_GEN_STR(mu_gen_key_sin, "sin")
MU_GEN_BFN(mu_gen_sin, 0x1, mu_bfn_sin)

static mcnt_t mu_bfn_asin(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_ASIN, 0x1, frame);
    }

    frame[0] = mu_num_asin(a);
    return 1;
}

MU_GEN_STR(mu_gen_key_asin, "asin")
MU_GEN_BFN(mu_gen_asin, 0x1, mu_bfn_asin)

static mcnt_t mu_bfn_tan(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_TAN, 0x1, frame);
    }

    frame[0] = mu_num_tan(a);
    return 1;
}

MU_GEN_STR(mu_gen_key_tan, "tan")
MU_GEN_BFN(mu_gen_tan, 0x1, mu_bfn_tan)

static mcnt_t mu_bfn_atan(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || (b && !mu_isnum(b))) {
        mu_error_arg(MU_KEY_ATAN, 0x2, frame);
    }

    frame[0] = mu_num_atan(a, b);
    return 1;
}

MU_GEN_STR(mu_gen_key_atan, "atan")
MU_GEN_BFN(mu_gen_atan, 0x2, mu_bfn_atan)

static mcnt_t mu_bfn_shl(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_op(MU_KEY_SHL, 0x2, frame);
    }

    frame[0] = mu_num_shl(a, b);
    return 1;
}

MU_GEN_STR(mu_gen_key_shl, "<<")
MU_GEN_BFN(mu_gen_shl, 0x2, mu_bfn_shl)

static mcnt_t mu_bfn_shr(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_op(MU_KEY_SHR, 0x2, frame);
    }

    frame[0] = mu_num_shr(a, b);
    return 1;
}

MU_GEN_STR(mu_gen_key_shr, ">>")
MU_GEN_BFN(mu_gen_shr, 0x2, mu_bfn_shr)


// Random number generation
// Based on xorshift128+ with wordsize as seed/output
#ifdef MU64
#define XORSHIFT1 23
#define XORSHIFT2 17
#define XORSHIFT3 26
#else
#define XORSHIFT1 15
#define XORSHIFT2 18
#define XORSHIFT3 11
#endif

static mcnt_t mu_num_random(mu_t scope, mu_t *frame) {
    muint_t *a = mu_buf_getdata(scope);
    muint_t x = a[0];
    muint_t y = a[1];

    x ^= x << XORSHIFT1;
    x ^= x >> XORSHIFT2;
    x ^= y ^ (y >> XORSHIFT3);

    a[0] = y;
    a[1] = x;
    frame[0] = mu_num_div(mu_num_fromuint(x + y),
            mu_num_add(mu_num_fromuint((muint_t)-1), mu_num_fromuint(1)));
    return 1;
}

static mcnt_t mu_num_seed(mu_t *frame) {
    mu_t seed = frame[0] ? frame[0] : mu_num_fromuint(0);
    if (!mu_isnum(seed)) {
        mu_error_arg(MU_KEY_SEED, 0x1, frame);
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_num_random, mu_buf_fromdata(
            (mu_t[]){seed, seed}, 2*sizeof(mu_t)));
    return 1;
}

MU_GEN_STR(mu_gen_key_seed, "seed")
MU_GEN_BFN(mu_gen_seed, 0x1, mu_num_seed)
