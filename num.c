#include "num.h"

#include "str.h"


// Max length of a string representation of a number
#define MU_NUMLEN 12


// Parses a string and returns a number
mu_t num_parse(const byte_t **off, const byte_t *end) {
    const byte_t *str = *off;
    num_t res = 0;

    num_t scale, sign;

    struct base {
        num_t radix;
        num_t exp;
        byte_t expc;
        byte_t expC;
    } base = { 10.0, 10.0, 'e', 'E' };


    // determine the base
    if (end-str > 2 && *str == '0') {
        switch (str[1]) {
            case 'b': case 'B':
                base = (struct base){ 2.0, 2.0, 'p', 'P' };
                str += 2;
                break;

            case 'o': case 'O':
                base = (struct base){ 8.0, 2.0, 'p', 'P' };
                str += 2;
                break;

            case 'x': case 'X':
                base = (struct base){ 16.0, 2.0, 'p', 'P' };
                str += 2;
                break;
        }
    }

    // determine the integer component
    while (str < end) {
        int n = num_val(*str);

        if (n >= base.radix) {
            if (*str == '.')
                goto fraction;
            else if (*str == base.expc || *str == base.expC)
                goto exp;
            else
                goto done;
        }

        str++;
        res *= base.radix;
        res += (num_t)n;
    }

    goto done;

fraction:   // determine fraction component
    str++; // consume dot
    scale = 1.0;

    while (str < end) {
        int n = num_val(*str);

        if (n >= base.radix) {
            if (*str == base.expc || *str == base.expC)
                goto exp;
            else
                goto done;
        }

        str++;
        scale /= base.radix;
        res += scale * (num_t)n;
    }

    goto done;

exp:        // determine exponent component
    scale = 0.0;
    sign = 1.0;

    if (end-str > 1) {
        if (*str == '+') {
            str++;
        } else if (*str == '-') {
            sign = -1.0;
            str++;
        }
    }

    while (str < end) {
        int n = num_val(*str);

        if (n >= base.radix) {
            res *= (num_t)pow(base.radix, sign*scale);
            goto done;
        }

        str++;
        scale *= base.radix;
        scale += (num_t)n;
    }

done:       // return the result
    *off = str;

    return mnum(res);
}


// Obtains a string representation of a number
mu_t num_repr(mu_t m) {
    num_t n = num_num(m);

    if (n == 0) {
        return mcstr("0");
    } else if (isnan(n)) {
        return mcstr("nan");
    } else if (isinf(n)) {
        return n > 0.0 ? mcstr("inf") : mcstr("-inf");
    } else {
        byte_t *s = mstr_create(MU_NUMLEN);
        byte_t *out = s;

        if (n < 0.0) {
            n = -n;
            *out++ = '-';
        }

        num_t exp = floor(log10(n));
        num_t digit = pow(10.0, exp);
        bool isexp = (exp > MU_NUMLEN-2 || exp < -(MU_NUMLEN-3));

        if (isexp) {
            n /= digit;
            digit = 1.0;
        } else if (digit < 1.0) {
            digit = 1.0;
        }


        int len = isexp ? MU_NUMLEN-6 : MU_NUMLEN-1;

        for (; len >= 0; len--) {
            if (n <= 0.0 && digit < 1.0)
                break;

            if (digit < 0.5 && digit > 0.05)
                *out++ = '.';

            num_t d = floor(n / digit);
            *out++ = num_ascii((int_t)d);

            n -= d * digit;
            digit /= 10.0f;
        }


        if (isexp) {
            *out++ = 'e';

            if (exp < 0) {
                exp = -exp;
                *out++ = '-';
            }

            if (exp > 100)
                *out++ = num_ascii(((int_t)exp) / 100);

            // exp will always be greater than 10 here
            *out++ = num_ascii(((int_t)exp / 10) % 10);
            *out++ = num_ascii(((int_t)exp / 1) % 10);
        }

        return mstr_intern(s, out - s);
    }
}
