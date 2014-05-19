#include "num.h"


// Returns true if both variables are equal
bool num_equals(var_t a, var_t b) {
    return var_num(a) == var_num(b);
}


// Returns a hash for each number
// For integers this is the number
// TODO profile for faster hash
hash_t num_hash(var_t v) {
    v.type = 0;

    // take int value as base to keep
    // it linear for integers
    hash_t hash = (hash_t)v.num;

    // move decimal part around to fit into
    // an int value to add to the hash
    v.num -= hash;
    v.num += 0x100000;

    return hash ^ v.meta;
}


// Parses a string and returns a number
const unsigned char num_a[256] = {
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
var_t num_parse(str_t **off, str_t *end) {
    var_t v;
    str_t *str = *off;

    num_t scale;
    num_t sign;

    struct base {
        num_t radix;
        num_t exp;
        unsigned char exp_c;
    } base = { 10.0, 10.0, 0xe };

    register unsigned char w;


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
        w = num_a[*str++];

        if (w >= base.radix) {
            if (w == '.')
                goto fraction;
            else if (w == base.exp_c)
                goto exp;
            else
                goto done;
        }

        v.num *= base.radix;
        v.num += w;
    }

    goto done;

fraction:
    scale = 1.0;

    while (str < end) {
        w = num_a[*str++];

        if (w >= base.radix) {
            if (w == base.exp)
                goto exp;
            else
                goto done;
        }

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
        w = num_a[*str++];

        if (w >= base.radix) {
            v.num *= pow(base.radix, sign*scale);
            goto done;
        }

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
        return vstr("nan");
    } else if (isinf(v.num)) {
        var_t s = vstr("-inf");

        if (v.num > 0.0)
            s.off++;

        return s;
    } else if (v.num == 0) {
        return vstr("0");
    } else {
        var_t s;
        unsigned char *str;

        s.ref = var_alloc(16);
        str = (unsigned char *)(s.ref + 1);

        if (v.num < 0.0) {
            v.num = -v.num;
            *(str++) = '-';
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
                *(str++) = '.';

            int d = floor(v.num / digit);
            *(str++) = '0' + d;

            v.num -= d * digit;
            digit /= 10.0;
        }


        if (expform) {
            *(str++) = 'e';

            if (exp > 0) {
                *(str++) = '+';
            } else {
                *(str++) = '-';
                exp = -exp;
            }

            if (exp > 100)
                *(str++) = '0' + (int)(exp / 100);

            // exp will always be greater than 10 here
            *(str++) = '0' + (int)(exp / 10) % 10;
            *(str++) = '0' + (int)(exp / 1) % 10;  
        }


        s.len = (uint16_t)(str - s.str - sizeof(ref_t));
        s.off = 0;
        s.type = TYPE_STR;

        return s;
    }
}

