#include "buf.h"

#include "str.h"
#include "parse.h"


// Functions for handling buffers
mu_t buf_create(muint_t n) {
    if (n > (mlen_t)-1)
        mu_errorf("exceeded max length in buffer");

    struct buf *b = ref_alloc(sizeof(struct buf) + n);
    b->len = n;
    return (mu_t)((muint_t)b + MTBUF);
}

void buf_destroy(mu_t b) {
    ref_dealloc(b, sizeof(struct buf) + buf_len(b));
}

void buf_resize(mu_t *b, muint_t n) {
    mu_t nb = buf_create(n);
    memcpy(buf_data(nb), buf_data(*b), 
            (buf_len(nb) < buf_len(*b)) ? buf_len(nb) : buf_len(*b));
    buf_dec(*b);
    *b = nb;
}
    
void buf_expand(mu_t *b, muint_t n) {
    if (buf_len(*b) >= n)
        return;

    muint_t size = buf_len(*b) + sizeof(struct buf);

    if (size < MU_MINALLOC)
        size = MU_MINALLOC;

    while (size < n + sizeof(struct buf))
        size <<= 1;

    buf_resize(b, size - sizeof(struct buf));
}


// Concatenation functions with amortized doubling
void buf_append(mu_t *b, muint_t *i, const void *c, muint_t n) {
    buf_expand(b, *i + n);
    memcpy((mbyte_t *)buf_data(*b) + *i, c, n);
    *i += n;
}

void buf_push(mu_t *b, muint_t *i, mbyte_t byte) {
    buf_append(b, i, &byte, 1);
}

void buf_concat(mu_t *b, muint_t *i, mu_t c) {
    mu_assert(mu_isstr(c) || mu_isbuf(c));
    mu_t cbuf = (mu_t)(~(MTSTR^MTBUF) & (muint_t)c);

    buf_append(b, i, buf_data(cbuf), buf_len(cbuf));
    mu_dec(c);
}

static void buf_append_unsigned(mu_t *b, muint_t *i, muint_t u) {
    if (u == 0) {
        return buf_push(b, i, '0');
    }

    muint_t size = 0;
    muint_t u2 = u;
    while (u2 > 0) {
        size += 1;
        u2 /= 10;
    }

    buf_expand(b, *i + size);
    *i += size;

    char *c = buf_data(*b) + *i - 1;
    while (u > 0) {
        *c = mu_toascii(u % 10);
        u /= 10;
        c--;
    }
}

static void buf_append_signed(mu_t *b, muint_t *i, mint_t d) {
    if (d < 0) {
        buf_push(b, i, '-');
        d = -1;
    }

    buf_append_unsigned(b, i, d);
}

static void buf_append_hex(mu_t *b, muint_t *i, mlen_t n, muint_t x) {
    for (muint_t j = 0; j < 2*n; j++) {
        buf_push(b, i, mu_toascii((x >> 4*(n-j)) & 0xf));
    }
}

void buf_vformat(mu_t *b, muint_t *i, const char *f, va_list args) {
    while (*f) {
        if (*f != '%') {
            buf_push(b, i, *f++);
        } else {
            mlen_t n = sizeof(unsigned);
            f++;

            switch (*f) {
                case 'z': f++; break;
                case 'n': f++; n = -1; break;

                case 'w': f++; n = sizeof(muint_t);  break;
                case 'h': f++; n = sizeof(muinth_t); break;
                case 'q': f++; n = sizeof(muintq_t); break;
                case 'b': f++; n = sizeof(mbyte_t);  break;
            }

            switch (*f) {
                case '%': {
                    buf_push(b, i, '%');
                } break;

                case 'm': {
                    mu_t m = va_arg(args, mu_t);
                    if (!mu_isstr(m) && !mu_isbuf(m)) {
                        m = mu_repr(m);
                    }

                    buf_concat(b, i, m);
                } break;

                case 'r': {
                    mu_t m = va_arg(args, mu_t);
                    buf_concat(b, i, mu_repr(m));
                } break;

                case 's': {
                    const char *s = va_arg(args, const char *);

                    if (n == -1) {
                        n = va_arg(args, unsigned);
                        buf_append(b, i, s, n);
                    } else {
                        buf_append(b, i, s, strlen(s));
                    }
                } break;

                case 'u': {
                    muint_t u = n > sizeof(unsigned)
                        ? va_arg(args, muint_t)
                        : va_arg(args, unsigned);
                    buf_append_unsigned(b, i, u);
                } break;

                case 'd': {
                    muint_t d = n > sizeof(signed)
                        ? va_arg(args, mint_t)
                        : va_arg(args, signed);
                    buf_append_signed(b, i, d);
                } break;

                case 'x': {
                    muint_t u = n > sizeof(unsigned)
                        ? va_arg(args, muint_t)
                        : va_arg(args, unsigned);
                    buf_append_hex(b, i, n, u);
                } break;

                case 'c': {
                    muint_t u = n > sizeof(unsigned)
                        ? va_arg(args, muint_t)
                        : va_arg(args, unsigned);
                    buf_push(b, i, u);
                } break;
                        
                default: {
                    mu_errorf("invalid format argument");
                } break;
            }

            f++;
        }
    }
}

void buf_format(mu_t *b, muint_t *i, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    buf_vformat(b, i, fmt, args);
    va_end(args);
}

