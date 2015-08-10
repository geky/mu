#include "num.h"

#include "str.h"
#include "parse.h"


// Max length of a string representation of a number
#define MU_NUMLEN 12

// Obtains a string representation of a number
mu_t num_repr(mu_t m) {
    mfloat_t n = num_float(m);

    if (n == 0) {
        return mcstr("0");
    } else if (isnan(n)) {
        return mcstr("nan");
    } else if (isinf(n)) {
        return n > 0.0 ? mcstr("inf") : mcstr("-inf");
    } else {
        mbyte_t *s = mstr_create(MU_NUMLEN);
        mbyte_t *pos = s;

        if (n < 0.0) {
            n = -n;
            *pos++ = '-';
        }

        mfloat_t exp = floor(log10(n));
        mfloat_t digit = pow(10, exp);
        mint_t count;

        bool isexp = (exp > MU_NUMLEN-2 || exp < -(MU_NUMLEN-3));

        if (isexp) {
            n /= digit;
            digit = 1;
            count = MU_NUMLEN-6;
        } else {
            digit = digit < 1 ? 1 : digit;
            count = MU_NUMLEN-1;
        }

        for (; count >= 0; count--) {
            if (n <= 0 && digit < 1)
                break;

            if (digit < 0.5 && digit > 0.05)
                *pos++ = '.';

            mfloat_t d = floor(n / digit);
            *pos++ = mu_toascii((mint_t)d);

            n -= d * digit;
            digit /= 10.0f;
        }

        if (isexp) {
            *pos++ = 'e';

            if (exp < 0) {
                exp = -exp;
                *pos++ = '-';
            }

            if (exp > 100) *pos++ = mu_toascii((mint_t)exp / 100);
            if (exp > 10)  *pos++ = mu_toascii(((mint_t)exp / 10) % 10);
            if (exp > 1)   *pos++ = mu_toascii(((mint_t)exp / 1) % 10);
        }

        return mstr_intern(s, pos - s);
    }
}
