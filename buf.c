#include "buf.h"

#include "str.h"


// Repeated errors
static mu_noreturn mu_error_length(void) {
    mu_error(mcstr("exceeded max length in buffer"));
}


// Functions for handling buffers
mu_t buf_create(muint_t n) {
    if (n > (mlen_t)-1)
        mu_error_length();

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

    mu_t nb = buf_create(size - sizeof(struct buf));
    memcpy(buf_data(nb), buf_data(*b), buf_len(*b));
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

void buf_appendz(mu_t *b, muint_t *i, const char *c) {
    buf_append(b, i, c, strlen(c));
}

void buf_concat(mu_t *b, muint_t *i, mu_t c) {
    mu_assert(mu_isstr(c) || mu_isbuf(c));
    mu_t cbuf = (mu_t)(~(MTSTR^MTBUF) & (muint_t)c);

    buf_append(b, i, buf_data(cbuf), buf_len(cbuf));
    mu_dec(c);
}

