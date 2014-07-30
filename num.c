#include "num.h"


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
const str_t num_a[256] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  '.', 0xff,
       0,    1,    2,    3,    4,    5,    6,    7,    8,    9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff,   10,   11,   12,   13,   14,   15, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  'o',
     'p', 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  'x', 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff,   10,   11,   12,   13,   14,   15, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  'o',
     'p', 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  'x', 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

// TODO add bounds checking
var_t num_parse(const str_t **off, const str_t *end) {
    var_t v;
    const str_t *str = *off;

    num_t scale;
    num_t sign;

    struct base {
        num_t radix;
        num_t exp;
        str_t exp_c;
    } base = { 10.0, 10.0, 0xe };

    register str_t w;


    // determine the base
    if (end-str > 2 && *str == '0') {
        str++; // discard initial zero

        switch (num_a[*str]) {
            case 0xb:
                base = (struct base){ 2.0, 2.0, 'p' };
                str++;
                break;

            case 'o':
                base = (struct base){ 8.0, 2.0, 'p' };
                str++;
                break;

            case 'x':
                base = (struct base){ 16.0, 2.0, 'p' };
                str++;
                break;
        } 
    }

//integer:
    v.num = 0;

    while (str < end) {
        w = num_a[*str];

        if (w >= base.radix) {
            if (w == '.')
                goto fraction;
            else if (w == base.exp_c)
                goto exp;
            else
                goto done;
        }

        str++;
        v.num *= base.radix;
        v.num += w;
    }

    goto done;

fraction:
    scale = 1.0;

    while (str < end) {
        w = num_a[*str];

        if (w >= base.radix) {
            if (w == base.exp)
                goto exp;
            else
                goto done;
        }

        str++;
        scale /= base.radix;
        v.num += scale * w;
    }

    goto done;

exp:
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
        w = num_a[*str];

        if (w >= base.radix) {
            v.num *= pow(base.radix, sign*scale);
            goto done;
        }

        str++;
        scale *= base.radix;
        scale += w;
    }

done:
    *off = str;

    v.type = TYPE_NUM;

    return v;
}


// Returns a string representation of a number
var_t num_repr(var_t v) {
    v.type = 0;

    if (isnan(v.num)) {
        return vcstr("nan");

    } else if (isinf(v.num)) {
        var_t s = vcstr("-inf");

        if (v.num > 0.0)
            s.off++;

        return s;

    } else if (v.num == 0) {
        return vcstr("0");

    } else {
        str_t *s, *out;

        out = vref_alloc(16);
        s = out;


        if (v.num < 0.0) {
            v.num = -v.num;
            *s++ = '-';
        }


        int exp = floor(log10(v.num));
        num_t digit = pow(10.0, exp);
        bool expform = (exp > 14 || exp < -13);

        if (expform) {
            v.num /= digit;
            digit = 1.0;
        } else if (digit < 1.0) {
            digit = 1.0;
        }


        int len = expform ? 10 : 15;

        for (; len >= 0; len--) {
            if (v.num <= 0.0 && digit < 1.0)
                break;

            if (digit < 0.5 && digit > 0.05)
                *s++ = '.';

            int d = floor(v.num / digit);
            *s++ = '0' + d;

            v.num -= d * digit;
            digit /= 10.0;
        }


        if (expform) {
            *s++ = 'e';

            if (exp > 0) {
                *s++ = '+';
            } else {
                *s++ = '-';
                exp = -exp;
            }

            if (exp > 100)
                *s++ = '0' + (int)(exp / 100);

            // exp will always be greater than 10 here
            *s++ = '0' + (int)(exp / 10) % 10;
            *s++ = '0' + (int)(exp / 1) % 10;  
        }

        return vstr(out, 0, s - out);
    }
}

