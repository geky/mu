#include "num.h"
#include "str.h"


// Returns true if both variables are equal
bool num_equals(var_t a, var_t b) {
    return var_num(a) == var_num(b);
}


// Returns a hash for each number
// For integers this is the number
hash_t num_hash(var_t v) {
    var_t i;
    v.type = 0;

    // This magic number is the value to puts a number's mantissa 
    // directly in the integer range. After these operations, 
    // i.num and v.num will contain the integer and fractional 
    // components of the original number.
    i.num = v.num + 6755399441055744.0;
    v.num = v.num - (i.num - 6755399441055744.0);

    // The int component forms the base of the hash so integers
    // are linear. The fractional component is also used to distinguish 
    // between non-integer values.
    return i.meta ^ v.data;
}


// Parses a string and returns a number
var_t num_parse(const str_t **off, const str_t *end) {
    const str_t *str = *off;
    num_t res = 0;

    num_t scale, sign;

    struct base {
        num_t radix;
        num_t exp;
        str_t expc;
        str_t expC;
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
var_t num_repr(var_t v, veh_t *eh) {
    v.type = 0;

    if (v.num == 0) {
        return vcstr("0");
    } else if (isnan(v.num)) {
        return vcstr("nan");
    } else if (isinf(v.num)) {
        var_t s = vcstr("-inf");

        if (v.num > 0.0) {
            s.off++;
            s.len--;
        }

        return s;
    } else {
        str_t *out = str_create(VNUMLEN, eh);
        str_t *res = out;

        if (v.num < 0.0) {
            v.num = -v.num;
            *res++ = '-';
        }


        num_t exp = floor(log10(v.num));
        num_t digit = pow(10.0, exp);
        bool isexp = (exp > VNUMLEN-2 || exp < -(VNUMLEN-3));

        if (isexp) {
            v.num /= digit;
            digit = 1.0;
        } else if (digit < 1.0) {
            digit = 1.0;
        }


        int len = isexp ? VNUMLEN-6 : VNUMLEN-1;

        for (; len >= 0; len--) {
            if (v.num <= 0.0 && digit < 1.0)
                break;

            if (digit < 0.5 && digit > 0.05)
                *res++ = '.';

            num_t d = floor(v.num / digit);
            *res++ = num_ascii(d);

            v.num -= d * digit;
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

