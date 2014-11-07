#include "num.h"

#include "str.h"

#include <math.h>


// Returns true if both variables are equal
bool num_equals(var_t a, var_t b) {
    return getnum(a) == getnum(b);
}


// Returns a hash for each number
// For integers this is the number
hash_t num_hash(var_t v) {
    union {
        struct { uint32_t high; uint32_t low; };
        num_t num;
    } i, f;

    num_t num = getnum(v);

    // This magic number is the value to puts a number's mantissa 
    // directly in the integer range. After these operations, 
    // i.n and f.n will contain the integer and fractional 
    // components of the original number.
    i.num = num + 6755399441055744.0;
    f.num = num - (i.num - 6755399441055744.0);

    // The int component forms the base of the hash so integers
    // are linear. The fractional component is also used to distinguish 
    // between non-integer values.
    return i.high ^ f.low;
}


// Parses a string and returns a number
var_t num_parse(str_t **off, str_t *end) {
    str_t *str = *off;
    num_t res = 0;

    num_t scale, sign;

    struct base {
        num_t radix;
        num_t exp;
        mstr_t expc;
        mstr_t expC;
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
        res += n;
    }

    goto done;

fraction:   // determine fraction component
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
        res += scale * n;
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
            res *= pow(base.radix, sign*scale);
            goto done;
        }

        str++;
        scale *= base.radix;
        scale += n;
    }

done:       // return the result
    *off = str;

    return vnum(res);
}


// Obtains a string representation of a number
var_t num_repr(var_t v, eh_t *eh) {
    num_t num = getnum(v);

    if (num == 0) {
        return vcstr("0");
    } else if (isnan(num)) {
        return vcstr("nan");
    } else if (isinf(num)) {
        return num > 0.0 ? vcstr("inf") : vcstr("-inf");
    } else {
        mstr_t *out = str_create(MU_NUMLEN, eh);
        mstr_t *res = out;

        if (num < 0.0) {
            num = -num;
            *res++ = '-';
        }


        num_t exp = floor(log10(num));
        num_t digit = pow(10.0, exp);
        bool isexp = (exp > MU_NUMLEN-2 || exp < -(MU_NUMLEN-3));

        if (isexp) {
            num /= digit;
            digit = 1.0;
        } else if (digit < 1.0) {
            digit = 1.0;
        }


        int len = isexp ? MU_NUMLEN-6 : MU_NUMLEN-1;

        for (; len >= 0; len--) {
            if (num <= 0.0 && digit < 1.0)
                break;

            if (digit < 0.5 && digit > 0.05)
                *res++ = '.';

            num_t d = floor(num / digit);
            *res++ = num_ascii(d);

            num -= d * digit;
            digit /= 10.0;
        }


        if (isexp) {
            *res++ = 'e';

            if (exp < 0) {
                exp = -exp;
                *res++ = '-';
            }

            if (exp > 100)
                *res++ = num_ascii(((int)exp) / 100);

            // exp will always be greater than 10 here
            *res++ = num_ascii(((int)exp / 10) % 10);
            *res++ = num_ascii(((int)exp / 1) % 10);
        }

        return vstr(out, 0, res - out);
    }
}

