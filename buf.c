#include "buf.h"

#include "str.h"
#include "num.h"
#include "fn.h"
#include "parse.h"


// Functions for handling buffers
static mu_t cbuf_create(muint_t n, void (*dtor)(mu_t)) {
    if (n > (mlen_t)-1) {
        mu_errorf("exceeded max length in buffer");
    }

    if (!dtor) {
        struct buf *b = ref_alloc(mu_offsetof(struct buf, data) + n);
        b->len = n;
        return (mu_t)((muint_t)b + MTBUF);
    } else {
        struct buf *b = ref_alloc(mu_offsetof(struct buf, data) +
                n + sizeof(dtor));
        b->len = n;
        *(void (**)(mu_t))(b->data + b->len) = dtor;
        return (mu_t)((muint_t)b + MTCBUF);
    }
}

mu_t buf_create(muint_t n) {
    return cbuf_create(n, 0);
}

void buf_destroy(mu_t b) {
    ref_dealloc(b, mu_offsetof(struct buf, data) + buf_len(b));
}

void cbuf_destroy(mu_t b) {
    buf_dtor(b)(b);
    ref_dealloc(b, mu_offsetof(struct buf, data) +
            buf_len(b) + sizeof(void (*)(mu_t)));
}

void buf_resize(mu_t *b, muint_t n) {
    if (n > (mlen_t)-1) {
        mu_errorf("exceeded max length in buffer");
    }

    mu_t nb = cbuf_create(n, buf_dtor(*b));
    memcpy(buf_data(nb), buf_data(*b), (n < buf_len(*b)) ? n : buf_len(*b));

    buf_dec(*b);
    *b = nb;
}

void buf_expand(mu_t *b, muint_t n) {
    if (buf_len(*b) >= n) {
        return;
    }

    muint_t overhead = mu_offsetof(struct buf, data);
    if (buf_dtor(*b)) {
        overhead += sizeof(void (*)(mu_t));
    }

    muint_t size = overhead + buf_len(*b);
    if (size < MU_MINALLOC) {
        size = MU_MINALLOC;
    }

    while (size < overhead + n) {
        size <<= 1;
    }

    buf_resize(b, size - overhead);
}

void buf_setdtor(mu_t *b, void (*dtor)(mu_t)) {
    mu_t nb = cbuf_create(buf_len(*b), dtor);
    memcpy(buf_data(nb), buf_data(*b), buf_len(nb));

    buf_dec(*b);
    *b = nb;
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


// Buffer formatting
static void buf_append_unsigned(mu_t *b, muint_t *i, muint_t u) {
    if (u == 0) {
        buf_push(b, i, '0');
        return;
    }

    muint_t size = 0;
    muint_t u2 = u;
    while (u2 > 0) {
        size += 1;
        u2 /= 10;
    }

    buf_expand(b, *i + size);
    *i += size;

    char *c = (char *)buf_data(*b) + *i - 1;
    while (u > 0) {
        *c = mu_toascii(u % 10);
        u /= 10;
        c--;
    }
}

static void buf_append_signed(mu_t *b, muint_t *i, mint_t d) {
    if (d < 0) {
        buf_push(b, i, '-');
        d = -d;
    }

    buf_append_unsigned(b, i, d);
}

static void buf_append_hex(mu_t *b, muint_t *i, muint_t x, int n) {
    for (muint_t j = 0; j < 2*n; j++) {
        buf_push(b, i, mu_toascii((x >> 4*(2*n-j-1)) & 0xf));
    }
}

#define buf_va_size(va, n)  \
    ((n == -2) ? va_arg(va, unsigned) : n)

#define buf_va_uint(va, n)  \
    ((n < sizeof(unsigned)) ? va_arg(va, unsigned) : va_arg(va, muint_t))

#define buf_va_int(va, n)   \
    ((n < sizeof(signed)) ? va_arg(va, signed) : va_arg(va, mint_t))

void buf_vformat(mu_t *b, muint_t *i, const char *f, va_list args) {
    while (*f) {
        if (*f != '%') {
            buf_push(b, i, *f++);
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
                buf_va_size(args, size);
                buf_push(b, i, '%');
                break;
            } break;

            case 'm': {
                mu_t m = va_arg(args, mu_t);
                int n = buf_va_size(args, size);
                if (!mu_isstr(m) && !mu_isbuf(m)) {
                    m = fn_call(MU_REPR, 0x21, m, (n < 0) ? 0 : muint(n));
                }

                buf_concat(b, i, m);
            } break;

            case 'r': {
                mu_t m = va_arg(args, mu_t);
                int n = buf_va_size(args, size);
                m = fn_call(MU_REPR, 0x21, m, (n < 0) ? 0 : muint(n));
                buf_concat(b, i, m);
            } break;

            case 's': {
                const char *s = va_arg(args, const char *);
                int n = buf_va_size(args, size);
                buf_append(b, i, s, (n < 0) ? strlen(s) : n);
            } break;

            case 'u': {
                muint_t u = buf_va_uint(args, size);
                buf_va_size(args, size);
                buf_append_unsigned(b, i, u);
            } break;

            case 'd': {
                muint_t d = buf_va_int(args, size);
                buf_va_size(args, size);
                buf_append_signed(b, i, d);
            } break;

            case 'x': {
                muint_t u = buf_va_uint(args, size);
                int n = buf_va_size(args, size);
                buf_append_hex(b, i, u, (n < 0) ? sizeof(unsigned) : n);
            } break;

            case 'c': {
                muint_t u = buf_va_uint(args, size);
                buf_va_size(args, size);
                buf_push(b, i, u);
            } break;

            default: {
                mu_errorf("invalid format argument");
            } break;
        }
    }
}

void buf_format(mu_t *b, muint_t *i, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    buf_vformat(b, i, fmt, args);
    va_end(args);
}

