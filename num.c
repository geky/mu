#include "num.h"

#include "str.h"
#include "err.h"
#include "parse.h"


// Binary digits of precision
#ifdef MU64
#define MU_DIGITS (52 - 3)
#else
#define MU_DIGITS (23 - 3)
#endif


// Number creating macros
// Number cannot be NaNs or negative zero to garuntee bitwise equality
mu_t mfloat(mfloat_t n) {
    if (mu_unlikely(n != n))
        mu_cerr(mcstr("nan_result"), mcstr("Operation resulted in NaN"));

    if (n == 0)
        n = 0;

    union { mfloat_t n; muint_t u; } v = { n };
    return (mu_t)(MU_NUM + (~7 & v.u));
}


// Comparison operation
mint_t num_cmp(mu_t a, mu_t b) {
    mfloat_t afloat = num_float(a);
    mfloat_t bfloat = num_float(b);

    return afloat > bfloat ? +1 :
           afloat < bfloat ? -1 : 0;
}


// Arithmetic operations
mu_t num_neg(mu_t a) { return mfloat(-num_float(a)); }
mu_t num_add(mu_t a, mu_t b) { return mfloat(num_float(a) + num_float(b)); }
mu_t num_sub(mu_t a, mu_t b) { return mfloat(num_float(a) - num_float(b)); }
mu_t num_mul(mu_t a, mu_t b) { return mfloat(num_float(a) * num_float(b)); }
mu_t num_div(mu_t a, mu_t b) { return mfloat(num_float(a) / num_float(b)); }


mu_t num_abs(mu_t a) { return mfloat(fabs(num_float(a))); }
mu_t num_floor(mu_t a) { return mfloat(floor(num_float(a))); }
mu_t num_ceil(mu_t a)  { return mfloat(ceil(num_float(a))); }

mu_t num_idiv(mu_t a, mu_t b) {
    return mfloat(floor(num_float(a) / num_float(b)));
}

mu_t num_mod(mu_t a, mu_t b) {
    mfloat_t base = num_float(b);
    mfloat_t mod = fmod(num_float(a), base);

    // Handle truncation for negative values
    if (mod*base < 0)
        mod += base;

    return mfloat(mod);
}


mu_t num_pow(mu_t a, mu_t b) {
    return mfloat(pow(num_float(a), num_float(b))); 
}

mu_t num_log(mu_t a, mu_t b) {
    return mfloat(log(num_float(a)) / log(num_float(b)));
}


mu_t num_cos(mu_t a)  { return mfloat(cos(num_float(a))); }
mu_t num_acos(mu_t a) { return mfloat(acos(num_float(a))); }
mu_t num_sin(mu_t a)  { return mfloat(sin(num_float(a))); }
mu_t num_asin(mu_t a) { return mfloat(asin(num_float(a))); }
mu_t num_tan(mu_t a)  { return mfloat(tan(num_float(a))); }
mu_t num_atan(mu_t a) { return mfloat(atan(num_float(a))); }

mu_t num_atan2(mu_t a, mu_t b) {
    return mfloat(atan2(num_float(a), num_float(b)));
}


// Conversion from string representing bytes
mu_t num_fromstr(mu_t m) {
    const mbyte_t *b = str_bytes(m);
    mlen_t len = str_len(m);
    mu_t n = muint(0);

    for (muint_t i = 0; i < len; i++) {
        n = num_mul(n, muint(256));
        n = num_add(n, muint(b[i]));
    }

    str_dec(m);
    return n;
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
        return num_mul(sign, minf);
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
            scale = num_div(scale, muint(base));
            n = num_add(n, num_mul(scale, muint(mu_fromascii(*pos++))));
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
static void num_base_ipart(mbyte_t **s, muint_t *i, mu_t n, mu_t base) {
    muint_t j = *i;

    while (num_cmp(n, muint(0)) > 0) {
        mu_t d = num_mod(n, base);
        mstr_insert(s, i, mu_toascii(num_uint(d)));
        n = num_idiv(n, base);
    }

    mbyte_t *a = &(*s)[j];
    mbyte_t *b = &(*s)[*i - 1];

    while (a < b) {
        mbyte_t t = *a;
        *a = *b;
        *b = t;
        a++; b--;
    }
}

static void num_base_fpart(mbyte_t **s, muint_t *i, mu_t n, 
                           mu_t base, muint_t digits) {
    mu_t error = num_pow(base, mint(-digits));
    mu_t digit = mint(-1);
    n = num_mod(n, muint(1));

    for (muint_t j = 0; j < digits; j++) {
        if (num_cmp(n, error) <= 0)
            break;

        if (digit == mint(-1))
            mstr_insert(s, i, '.');

        mu_t p = num_pow(base, digit);
        mu_t d = num_idiv(n, p);
        mstr_insert(s, i, mu_toascii(num_uint(d)));

        n = num_mod(n, p);
        digit = num_sub(digit, muint(1));
    }
}

static mu_t num_base(mu_t n, char c, mu_t base, char expc, mu_t expbase) {
    if (n == muint(0)) {
        if (c) return mnstr((mbyte_t[3]){'0', c, '0'}, 3);
        else   return mcstr("0");
    } else if (n == minf) {
        return mcstr("+inf");
    } else if (n == mninf) {
        return mcstr("-inf");
    } else {
        mbyte_t *s = mstr_create(0);
        muint_t i = 0;

        if (num_cmp(n, muint(0)) < 0) {
            n = num_neg(n);
            mstr_insert(&s, &i, '-');
        }

        if (c) {
            mstr_ncat(&s, &i, (mbyte_t[2]){'0', c}, 2);
        }

        mu_t exp = num_floor(num_log(n, expbase));
        mu_t sig = num_floor(num_log(n, base));
        mu_t digits = num_ceil(num_div(muint(MU_DIGITS), 
                               num_log(base, muint(2))));

        bool scientific = num_cmp(sig, digits) >= 0 ||
                          num_cmp(sig, mint(-1)) < 0;

        if (scientific)
            n = num_div(n, num_pow(expbase, exp));

        muint_t j = i;
        num_base_ipart(&s, &i, n, base);
        num_base_fpart(&s, &i, n, base, num_uint(digits) - (i-j));

        if (scientific) {
            mstr_insert(&s, &i, expc);

            if (num_cmp(exp, muint(0)) < 0) {
                exp = num_neg(exp);
                mstr_insert(&s, &i, '-');
            }

            num_base_ipart(&s, &i, exp, muint(10));
        }

        return mstr_intern(s, i);
    }
}

mu_t num_repr(mu_t n) { return num_base(n, 0,   muint(10), 'e', muint(10)); }
mu_t num_bin(mu_t n)  { return num_base(n, 'b', muint(2),  'p', muint(2)); }
mu_t num_oct(mu_t n)  { return num_base(n, 'o', muint(8),  'p', muint(2)); }
mu_t num_hex(mu_t n)  { return num_base(n, 'x', muint(16), 'p', muint(2)); }
