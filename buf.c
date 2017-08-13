#include "buf.h"

#include "str.h"
#include "num.h"
#include "fn.h"
#include "parse.h"


// Functions for handling buffers
mu_t mu_buf_create(muint_t n) {
    return mu_buf_createdtor(n, 0);
}

mu_t mu_buf_createdtor(muint_t n, void (*dtor)(mu_t)) {
    if (n > (mlen_t)-1) {
        mu_errorf("exceeded max length in buffer");
    }

    if (!dtor) {
        struct mbuf *b = mu_ref_alloc(mu_offsetof(struct mbuf, data) + n);
        b->len = n;
        return (mu_t)((muint_t)b + MTBUF);
    } else {
        struct mbuf *b = mu_ref_alloc(mu_offsetof(struct mbuf, data) +
                mu_align(n) + sizeof(dtor));
        b->len = n;
        *(void (**)(mu_t))(b->data + mu_align(b->len)) = dtor;
        return (mu_t)((muint_t)b + MTCBUF);
    }
}

void mu_buf_destroy(mu_t b) {
    mu_ref_dealloc(b, mu_offsetof(struct mbuf, data) + mu_buf_getlen(b));
}

void mu_buf_destroydtor(mu_t b) {
    mu_buf_getdtor(b)(b);
    mu_ref_dealloc(b, mu_offsetof(struct mbuf, data) +
            mu_align(mu_buf_getlen(b)) + sizeof(void (*)(mu_t)));
}

void mu_buf_resize(mu_t *b, muint_t n) {
    if (n > (mlen_t)-1) {
        mu_errorf("exceeded max length in buffer");
    }

    mu_t nb = mu_buf_createdtor(n, mu_buf_getdtor(*b));
    memcpy(mu_buf_getdata(nb), mu_buf_getdata(*b),
            (n < mu_buf_getlen(*b)) ? n : mu_buf_getlen(*b));

    mu_buf_dec(*b);
    *b = nb;
}

void mu_buf_expand(mu_t *b, muint_t n) {
    if (mu_buf_getlen(*b) >= n) {
        return;
    }

    muint_t overhead = mu_offsetof(struct mbuf, data);
    if (mu_buf_getdtor(*b)) {
        overhead += sizeof(void (*)(mu_t));
    }

    muint_t size = overhead + mu_buf_getlen(*b);
    if (size < MU_MINALLOC) {
        size = MU_MINALLOC;
    }

    while (size < overhead + n) {
        size <<= 1;
    }

    mu_buf_resize(b, size - overhead);
}

void mu_buf_setdtor(mu_t *b, void (*dtor)(mu_t)) {
    mu_t nb = mu_buf_createdtor(mu_buf_getlen(*b), dtor);
    memcpy(mu_buf_getdata(nb), mu_buf_getdata(*b), mu_buf_getlen(nb));

    mu_buf_dec(*b);
    *b = nb;
}

// From functions
mu_t mu_buf_frommu(mu_t c) {
    mu_assert(mu_isstr(c) || mu_isbuf(c));
    mu_t cbuf = (mu_t)(~(MTSTR^MTBUF) & (muint_t)c);

    mu_t b = mu_buf_fromdata(mu_buf_getdata(cbuf), mu_buf_getlen(cbuf));
    mu_dec(c);
    return b;
}

mu_t mu_buf_vformat(const char *f, va_list args) {
    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_buf_vpushf(&b, &n, f, args);

    if (mu_buf_getlen(b) != n) {
        mu_buf_resize(&b, n);
    }

    return b;
}

mu_t mu_buf_format(const char *f, ...) {
    va_list args;
    va_start(args, f);
    mu_t m = mu_str_vformat(f, args);
    va_end(args);
    return m;
}

// Concatenation functions with amortized doubling
void mu_buf_pushdata(mu_t *b, muint_t *i, const void *c, muint_t n) {
    mu_buf_expand(b, *i + n);
    memcpy((mbyte_t *)mu_buf_getdata(*b) + *i, c, n);
    *i += n;
}

void mu_buf_pushmu(mu_t *b, muint_t *i, mu_t c) {
    mu_assert(mu_isstr(c) || mu_isbuf(c));
    mu_t cbuf = (mu_t)(~(MTSTR^MTBUF) & (muint_t)c);

    mu_buf_pushdata(b, i, mu_buf_getdata(cbuf), mu_buf_getlen(cbuf));
    mu_dec(c);
}


// Buffer formatting
static mbyte_t mu_buf_toascii(muint_t c) {
    return (c < 10) ? '0' + c : 'a' + (c-10);
}

static void mu_buf_append_unsigned(mu_t *b, muint_t *i, muint_t u) {
    if (u == 0) {
        mu_buf_pushchr(b, i, '0');
        return;
    }

    muint_t size = 0;
    muint_t u2 = u;
    while (u2 > 0) {
        size += 1;
        u2 /= 10;
    }

    mu_buf_expand(b, *i + size);
    *i += size;

    char *c = (char *)mu_buf_getdata(*b) + *i - 1;
    while (u > 0) {
        *c = mu_buf_toascii(u % 10);
        u /= 10;
        c--;
    }
}

static void mu_buf_append_signed(mu_t *b, muint_t *i, mint_t d) {
    if (d < 0) {
        mu_buf_pushchr(b, i, '-');
        d = -d;
    }

    mu_buf_append_unsigned(b, i, d);
}

static void mu_buf_append_hex(mu_t *b, muint_t *i, muint_t x, int n) {
    for (muint_t j = 0; j < 2*n; j++) {
        mu_buf_pushchr(b, i, mu_buf_toascii((x >> 4*(2*n-j-1)) & 0xf));
    }
}

#define mu_buf_va_size(va, n)  \
    ((n == -2) ? va_arg(va, unsigned) : n)

#define mu_buf_va_uint(va, n)  \
    ((n < sizeof(unsigned)) ? va_arg(va, unsigned) : va_arg(va, muint_t))

#define mu_buf_va_int(va, n)   \
    ((n < sizeof(signed)) ? va_arg(va, signed) : va_arg(va, mint_t))

void mu_buf_vpushf(mu_t *b, muint_t *i, const char *f, va_list args) {
    while (*f) {
        if (*f != '%') {
            mu_buf_pushchr(b, i, *f++);
            continue;
        }
        f++;

        int size = -1;
        switch (*f) {
            case 'n': f++; size = -2;               break;
            case 'w': f++; size = sizeof(muint_t);  break;
            case 'h': f++; size = sizeof(muinth_t); break;
            case 'q': f++; size = sizeof(muintq_t); break;
            case 'b': f++; size = sizeof(mbyte_t);  break;
        }

        switch (*f++) {
            case '%': {
                mu_buf_va_size(args, size);
                mu_buf_pushchr(b, i, '%');
                break;
            } break;

            case 'm': {
                mu_t m = va_arg(args, mu_t);
                int n = mu_buf_va_size(args, size);
                if (!mu_isstr(m) && !mu_isbuf(m)) {
                    m = mu_fn_call(MU_REPR, 0x21,
                            m, (n < 0) ? 0 : mu_num_fromuint(n));
                }

                mu_buf_pushmu(b, i, m);
            } break;

            case 'r': {
                mu_t m = va_arg(args, mu_t);
                int n = mu_buf_va_size(args, size);
                m = mu_fn_call(MU_REPR, 0x21,
                        m, (n < 0) ? 0 : mu_num_fromuint(n));
                mu_buf_pushmu(b, i, m);
            } break;

            case 's': {
                const char *s = va_arg(args, const char *);
                int n = mu_buf_va_size(args, size);
                mu_buf_pushdata(b, i, s, (n < 0) ? strlen(s) : n);
            } break;

            case 'u': {
                muint_t u = mu_buf_va_uint(args, size);
                mu_buf_va_size(args, size);
                mu_buf_append_unsigned(b, i, u);
            } break;

            case 'd': {
                muint_t d = mu_buf_va_int(args, size);
                mu_buf_va_size(args, size);
                mu_buf_append_signed(b, i, d);
            } break;

            case 'x': {
                muint_t u = mu_buf_va_uint(args, size);
                int n = mu_buf_va_size(args, size);
                mu_buf_append_hex(b, i, u, (n < 0) ? sizeof(unsigned) : n);
            } break;

            case 'c': {
                muint_t u = mu_buf_va_uint(args, size);
                mu_buf_va_size(args, size);
                mu_buf_pushchr(b, i, u);
            } break;

            default: {
                mu_errorf("invalid format argument");
            } break;
        }
    }
}

void mu_buf_pushf(mu_t *b, muint_t *i, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    mu_buf_vpushf(b, i, fmt, args);
    va_end(args);
}

