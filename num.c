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
MSTR(mu_gen_key_inf,  "inf")    MFLOAT(mu_gen_inf,  INFINITY)
MSTR(mu_gen_key_ninf, "ninf")   MFLOAT(mu_gen_ninf, -INFINITY)
MSTR(mu_gen_key_e,    "e")      MFLOAT(mu_gen_e,    2.71828182845904523536)
MSTR(mu_gen_key_pi,   "pi")     MFLOAT(mu_gen_pi,   3.14159265358979323846)


// Number creating macro assuming NaN and -0 not possible
mu_inline mu_t mnum(mfloat_t n) {
    return (mu_t)(MTNUM + (~7 &
        ((union { mfloat_t n; muint_t u; }){(mfloat_t)n}).u));
}

// Conversion from floats
// Number cannot be NaNs or negative zero to garuntee bitwise equality
mu_t num_fromfloat(mfloat_t n) {
    if (n != n) {
        mu_errorf("operation resulted in nan");
    }

    if (n == 0) {
        n = 0;
    }

    return mnum(n);
}

// Other conversions
mu_t num_fromuint(muint_t n) { return muint(n); }
mu_t num_fromint(mint_t n) { return mint(n); }

mu_t num_fromstr(mu_t m) {
    mu_assert(mu_isstr(m) && str_len(m) == 1);
    mu_t n = muint(str_data(m)[0]);
    str_dec(m);
    return n;
}


// Comparison operation
mint_t num_cmp(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    mfloat_t afloat = num_float(a);
    mfloat_t bfloat = num_float(b);

    return afloat > bfloat ? +1 :
           afloat < bfloat ? -1 : 0;
}


// Arithmetic operations
mu_t num_neg(mu_t a) {
    mu_assert(mu_isnum(a));

    if (a == muint(0)) {
        return a;
    } else {
        return mnum(-num_float(a));
    }
}

mu_t num_add(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mfloat(num_float(a) + num_float(b));
}

mu_t num_sub(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mfloat(num_float(a) - num_float(b));
}

mu_t num_mul(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mfloat(num_float(a) * num_float(b));
}

mu_t num_div(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mfloat(num_float(a) / num_float(b));
}

mu_t num_idiv(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mfloat(floor(num_float(a) / num_float(b)));
}

mu_t num_mod(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    mfloat_t base = num_float(b);
    mfloat_t mod = fmod(num_float(a), base);

    // Handle truncation for negative values
    if (mod*base < 0) {
        mod += base;
    }

    return mfloat(mod);
}

mu_t num_pow(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return mfloat(pow(num_float(a), num_float(b)));
}

mu_t num_log(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && (!b || mu_isnum(b)));

    if (b) {
        return mfloat(log(num_float(a)) / log(num_float(b)));
    } else {
        return mfloat(log(num_float(a)) / log(num_float(MU_E)));
    }
}

mu_t num_abs(mu_t a) {
    mu_assert(mu_isnum(a));
    return mnum(fabs(num_float(a)));
}

mu_t num_floor(mu_t a) {
    mu_assert(mu_isnum(a));
    return mfloat(floor(num_float(a)));
}

mu_t num_ceil(mu_t a)  {
    mu_assert(mu_isnum(a));
    return mfloat(ceil(num_float(a)));
}

mu_t num_cos(mu_t a)  {
    mu_assert(mu_isnum(a));
    return mfloat(cos(num_float(a)));
}

mu_t num_acos(mu_t a) {
    mu_assert(mu_isnum(a));
    return mfloat(acos(num_float(a)));
}

mu_t num_sin(mu_t a) {
    mu_assert(mu_isnum(a));
    return mfloat(sin(num_float(a)));
}

mu_t num_asin(mu_t a) {
    mu_assert(mu_isnum(a));
    return mfloat(asin(num_float(a)));
}

mu_t num_tan(mu_t a) {
    mu_assert(mu_isnum(a));
    return mfloat(tan(num_float(a)));
}

mu_t num_atan(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && (!b || mu_isnum(b)));

    if (!b) {
        return mfloat(atan(num_float(a)));
    } else {
        return mfloat(atan2(num_float(a), num_float(b)));
    }
}

// Bitwise operations
mu_t num_not(mu_t a) {
    mu_assert(mu_isnum(a));
    return muint((muinth_t)(~num_uint(a)));
}

mu_t num_and(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return muint((muinth_t)(num_uint(a) & num_uint(b)));
}

mu_t num_or(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return muint((muinth_t)(num_uint(a) | num_uint(b)));
}

mu_t num_xor(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return muint((muinth_t)(num_uint(a) ^ num_uint(b)));
}


mu_t num_shl(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return muint((muint_t)(num_uint(a) << num_uint(b)));
}

mu_t num_shr(mu_t a, mu_t b) {
    mu_assert(mu_isnum(a) && mu_isnum(b));
    return muint((muint_t)(num_uint(a) >> num_uint(b)));
}


// Convert string representation to variable
mu_t num_parse(const mbyte_t **ppos, const mbyte_t *end) {
    const mbyte_t *pos = *ppos;
    mu_t n = muint(0);
    mu_t sign = mint(+1);
    muint_t base = 10;

    if (pos < end && *pos == '+') {
        sign = mint(+1); pos++;
    } else if (pos < end && *pos == '-') {
        sign = mint(-1); pos++;
    }

    if (pos+2 < end && memcmp(pos, "inf", 3) == 0) {
        *ppos = pos + 3;
        return num_mul(sign, MU_INF);
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
        n = num_mul(n, muint(base));
        n = num_add(n, muint(mu_fromascii(*pos++)));
    }

    if (pos < end && *pos == '.') {
        mu_t scale = muint(1);
        pos++;

        while (pos < end && mu_fromascii(*pos) < base) {
            scale = num_mul(scale, muint(base));
            n = num_add(n, num_div(muint(mu_fromascii(*pos++)), scale));
        }
    }

    if (pos < end && (*pos == 'e' || *pos == 'E' ||
                      *pos == 'p' || *pos == 'p')) {
        mu_t expbase = (*pos == 'e' || *pos == 'E') ? muint(10) : muint(2);
        mu_t exp = muint(0);
        mu_t sign = mint(+1);
        pos++;

        if (pos < end && *pos == '+') {
            sign = mint(+1); pos++;
        } else if (pos < end && *pos == '-') {
            sign = mint(-1); pos++;
        }

        while (pos < end && mu_fromascii(*pos) < 10) {
            exp = num_mul(exp, muint(10));
            exp = num_add(exp, muint(mu_fromascii(*pos++)));
        }

        n = num_mul(n, num_pow(expbase, num_mul(sign, exp)));
    }

    *ppos = pos;
    return num_mul(sign, n);
}

// Obtains a string representation of a number
static void num_base_ipart(mu_t *s, muint_t *i, mu_t n, mu_t base) {
    muint_t j = *i;

    while (num_cmp(n, muint(0)) > 0) {
        mu_t d = num_mod(n, base);
        buf_push(s, i, mu_toascii(num_uint(d)));
        n = num_idiv(n, base);
    }

    mbyte_t *a = (mbyte_t *)buf_data(*s) + j;
    mbyte_t *b = (mbyte_t *)buf_data(*s) + *i - 1;

    while (a < b) {
        mbyte_t t = *a;
        *a = *b;
        *b = t;
        a++; b--;
    }
}

static void num_base_fpart(mu_t *s, muint_t *i, mu_t n,
                           mu_t base, muint_t digits) {
    mu_t error = num_pow(base, mint(-digits));
    mu_t digit = mint(-1);
    n = num_mod(n, muint(1));

    for (muint_t j = 0; j < digits; j++) {
        if (num_cmp(n, error) <= 0) {
            break;
        }

        if (digit == mint(-1)) {
            buf_push(s, i, '.');
        }

        mu_t p = num_pow(base, digit);
        mu_t d = num_idiv(n, p);
        buf_push(s, i, mu_toascii(num_uint(d)));

        n = num_mod(n, p);
        digit = num_sub(digit, muint(1));
    }
}

static mu_t num_base(mu_t n, char c, mu_t base, char expc, mu_t expbase) {
    if (n == muint(0)) {
        if (c) return mstr("0%c0", c);
        else   return mstr("0");
    } else if (n == MU_INF) {
        return mstr("+inf");
    } else if (n == MU_NINF) {
        return mstr("-inf");
    } else {
        mu_t s = buf_create(0);
        muint_t i = 0;

        if (num_cmp(n, muint(0)) < 0) {
            n = num_neg(n);
            buf_push(&s, &i, '-');
        }

        if (c) {
            buf_append(&s, &i, (mbyte_t[2]){'0', c}, 2);
        }

        mu_t exp = num_floor(num_log(n, expbase));
        mu_t sig = num_floor(num_log(n, base));
        mu_t digits = num_ceil(num_div(muint(MU_DIGITS),
                               num_log(base, muint(2))));

        bool scientific = num_cmp(sig, digits) >= 0 ||
                          num_cmp(sig, mint(-1)) < 0;

        if (scientific) {
            n = num_div(n, num_pow(expbase, exp));
        }

        muint_t j = i;
        num_base_ipart(&s, &i, n, base);
        num_base_fpart(&s, &i, n, base, num_uint(digits) - (i-j));

        if (scientific) {
            buf_push(&s, &i, expc);

            if (num_cmp(exp, muint(0)) < 0) {
                exp = num_neg(exp);
                buf_push(&s, &i, '-');
            }

            num_base_ipart(&s, &i, exp, muint(10));
        }

        return str_intern(s, i);
    }
}

mu_t num_repr(mu_t n) {
    mu_assert(mu_isnum(n));
    return num_base(n, 0, muint(10), 'e', muint(10));
}

mu_t num_bin(mu_t n) {
    mu_assert(mu_isnum(n));
    return num_base(n, 'b', muint(2), 'p', muint(2));
}

mu_t num_oct(mu_t n) {
    mu_assert(mu_isnum(n));
    return num_base(n, 'o', muint(8), 'p', muint(2));
}

mu_t num_hex(mu_t n) {
    mu_assert(mu_isnum(n));
    return num_base(n, 'x', muint(16), 'p', muint(2));
}


// Number related Mu functions
static mcnt_t mu_bfn_num(mu_t *frame) {
    mu_t m = frame[0];

    switch (mu_type(m)) {
        case MTNIL:
            frame[0] = muint(0);
            return 1;

        case MTNUM:
            frame[0] = m;
            return 1;

        case MTSTR:
            if (str_len(m) == 1) {
                frame[0] = num_fromstr(m);
                return 1;
            }
            break;

        default:
            break;
    }

    mu_error_cast(MU_KEY_NUM, m);
}

MSTR(mu_gen_key_num, "num")
MBFN(mu_gen_num, 0x1, mu_bfn_num)

static mcnt_t mu_bfn_add(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || (b && !mu_isnum(b))) {
        mu_error_op(MU_KEY_ADD, 0x2, frame);
    }

    if (!b) {
        frame[0] = a;
    } else {
        frame[0] = num_add(a, b);
    }

    return 1;
}

MSTR(mu_gen_key_add, "+")
MBFN(mu_gen_add, 0x2, mu_bfn_add)

static mcnt_t mu_bfn_sub(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || (b && !mu_isnum(b))) {
        mu_error_op(MU_KEY_SUB, 0x2, frame);
    }

    if (!b) {
        frame[0] = num_neg(a);
    } else {
        frame[0] = num_sub(a, b);
    }

    return 1;
}

MSTR(mu_gen_key_sub, "-")
MBFN(mu_gen_sub, 0x2, mu_bfn_sub)

static mcnt_t mu_bfn_mul(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_op(MU_KEY_MUL, 0x2, frame);
    }

    frame[0] = num_mul(a, b);
    return 1;
}

MSTR(mu_gen_key_mul, "*")
MBFN(mu_gen_mul, 0x2, mu_bfn_mul)

static mcnt_t mu_bfn_div(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_op(MU_KEY_DIV, 0x2, frame);
    }

    frame[0] = num_div(a, b);
    return 1;
}

MSTR(mu_gen_key_div, "/")
MBFN(mu_gen_div, 0x2, mu_bfn_div)

static mcnt_t mu_bfn_abs(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_ABS, 0x1, frame);
    }

    frame[0] = num_abs(a);
    return 1;
}

MSTR(mu_gen_key_abs, "abs")
MBFN(mu_gen_abs, 0x1, mu_bfn_abs)

static mcnt_t mu_bfn_floor(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_FLOOR, 0x1, frame);
    }

    frame[0] = num_floor(a);
    return 1;
}

MSTR(mu_gen_key_floor, "floor")
MBFN(mu_gen_floor, 0x1, mu_bfn_floor)

static mcnt_t mu_bfn_ceil(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_CEIL, 0x1, frame);
    }

    frame[0] = num_ceil(a);
    return 1;
}

MSTR(mu_gen_key_ceil, "ceil")
MBFN(mu_gen_ceil, 0x1, mu_bfn_ceil)

static mcnt_t mu_bfn_idiv(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_op(MU_KEY_IDIV, 0x2, frame);
    }

    frame[0] = num_idiv(a, b);
    return 1;
}

MSTR(mu_gen_key_idiv, "//")
MBFN(mu_gen_idiv, 0x2, mu_bfn_idiv)

static mcnt_t mu_bfn_mod(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_op(MU_KEY_MOD, 0x2, frame);
    }

    frame[0] = num_mod(a, b);
    return 1;
}

MSTR(mu_gen_key_mod, "%")
MBFN(mu_gen_mod, 0x2, mu_bfn_mod)

static mcnt_t mu_bfn_pow(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_arg(MU_KEY_POW, 0x2, frame);
    }

    frame[0] = num_pow(a, b);
    return 1;
}

MSTR(mu_gen_key_pow, "^")
MBFN(mu_gen_pow, 0x2, mu_bfn_pow)

static mcnt_t mu_bfn_log(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || (b && !mu_isnum(b))) {
        mu_error_arg(MU_KEY_LOG, 0x2, frame);
    }

    frame[0] = num_log(a, b);
    return 1;
}

MSTR(mu_gen_key_log, "log")
MBFN(mu_gen_log, 0x2, mu_bfn_log)

static mcnt_t mu_bfn_cos(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_COS, 0x1, frame);
    }

    frame[0] = num_cos(a);
    return 1;
}

MSTR(mu_gen_key_cos, "cos")
MBFN(mu_gen_cos, 0x1, mu_bfn_cos)

static mcnt_t mu_bfn_acos(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_ACOS, 0x1, frame);
    }

    frame[0] = num_acos(a);
    return 1;
}

MSTR(mu_gen_key_acos, "acos")
MBFN(mu_gen_acos, 0x1, mu_bfn_acos)

static mcnt_t mu_bfn_sin(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_SIN, 0x1, frame);
    }

    frame[0] = num_sin(a);
    return 1;
}

MSTR(mu_gen_key_sin, "sin")
MBFN(mu_gen_sin, 0x1, mu_bfn_sin)

static mcnt_t mu_bfn_asin(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_ASIN, 0x1, frame);
    }

    frame[0] = num_asin(a);
    return 1;
}

MSTR(mu_gen_key_asin, "asin")
MBFN(mu_gen_asin, 0x1, mu_bfn_asin)

static mcnt_t mu_bfn_tan(mu_t *frame) {
    mu_t a = frame[0];
    if (!mu_isnum(a)) {
        mu_error_arg(MU_KEY_TAN, 0x1, frame);
    }

    frame[0] = num_tan(a);
    return 1;
}

MSTR(mu_gen_key_tan, "tan")
MBFN(mu_gen_tan, 0x1, mu_bfn_tan)

static mcnt_t mu_bfn_atan(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || (b && !mu_isnum(b))) {
        mu_error_arg(MU_KEY_ATAN, 0x2, frame);
    }

    frame[0] = num_atan(a, b);
    return 1;
}

MSTR(mu_gen_key_atan, "atan")
MBFN(mu_gen_atan, 0x2, mu_bfn_atan)

static mcnt_t mu_bfn_shl(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_op(MU_KEY_SHL, 0x2, frame);
    }

    frame[0] = num_shl(a, b);
    return 1;
}

MSTR(mu_gen_key_shl, "<<")
MBFN(mu_gen_shl, 0x2, mu_bfn_shl)

static mcnt_t mu_bfn_shr(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (!mu_isnum(a) || !mu_isnum(b)) {
        mu_error_op(MU_KEY_SHR, 0x2, frame);
    }

    frame[0] = num_shr(a, b);
    return 1;
}

MSTR(mu_gen_key_shr, ">>")
MBFN(mu_gen_shr, 0x2, mu_bfn_shr)


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

static mcnt_t num_random(mu_t scope, mu_t *frame) {
    muint_t *a = buf_data(scope);
    muint_t x = a[0];
    muint_t y = a[1];

    x ^= x << XORSHIFT1;
    x ^= x >> XORSHIFT2;
    x ^= y ^ (y >> XORSHIFT3);

    a[0] = y;
    a[1] = x;
    frame[0] = num_div(muint(x + y), num_add(muint((muint_t)-1), muint(1)));
    return 1;
}

static mcnt_t num_seed(mu_t *frame) {
    mu_t seed = frame[0] ? frame[0] : muint(0);
    if (!mu_isnum(seed)) {
        mu_error_arg(MU_KEY_SEED, 0x1, frame);
    }

    frame[0] = msbfn(0x0, num_random, mbuf(
            (mu_t[]){seed, seed}, 2*sizeof(mu_t)));
    return 1;
}

MSTR(mu_gen_key_seed, "seed")
MBFN(mu_gen_seed, 0x1, num_seed)
