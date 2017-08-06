/*
 * Dynamically allocated buffers
 */

#ifndef MU_BUF_H
#define MU_BUF_H
#include "mu.h"


// Creates buffer with specified size
mu_t mu_buf_create(muint_t n);

// Resizes buffers into new buffers
// mu_buf_expand may end up with extra space
void mu_buf_resize(mu_t *b, muint_t n);
void mu_buf_expand(mu_t *b, muint_t n);

// Adding to buffers with amortized doubling
void mu_buf_push(mu_t *b, muint_t *i, mbyte_t byte);
void mu_buf_append(mu_t *b, muint_t *i, const void *c, muint_t n);
void mu_buf_concat(mu_t *b, muint_t *i, mu_t c);

// Set destructor for a buffer
void mu_buf_setdtor(mu_t *b, void (*dtor)(mu_t));

// Formatting buffers with format strings
void mu_buf_vformat(mu_t *b, muint_t *i, const char *f, va_list args);
void mu_buf_format(mu_t *b, muint_t *i, const char *fmt, ...);


// Definition of internal Mu buffers
//
// Each buffer is stored as a length and array of data.
// Buffers can be mutated and used freely in C function,
// but can't be accessed from Mu.
struct mbuf {
    mref_t ref;     // reference count
    mlen_t len;     // length of allocated data
    mbyte_t data[]; // data follows
    // optional destructor stored at end
};


// Buffer creation functions
mu_inline mu_t mu_buf_fromdata(const void *s, muint_t n) {
    mu_t b = mu_buf_create(n);
    memcpy(((struct mbuf *)(~7 & (muint_t)b))->data, s, n);
    return b;
}

// Reference counting
mu_inline mu_t mu_buf_inc(mu_t b) {
    mu_assert(mu_isbuf(b));
    mu_ref_inc(b);
    return b;
}

mu_inline void mu_buf_dec(mu_t m) {
    mu_assert(mu_isbuf(m));
    mu_dec(m);
}

// Buffer access functions
mu_inline mlen_t mu_buf_getlen(mu_t b) {
    return ((struct mbuf *)(~7 & (muint_t)b))->len;
}

mu_inline void *mu_buf_getdata(mu_t b) {
    return ((struct mbuf *)(~7 & (muint_t)b))->data;
}

mu_inline void (*mu_buf_getdtor(mu_t b))(mu_t) {
    if ((MTCBUF^MTBUF) & (muint_t)b) {
        struct mbuf *buf = (struct mbuf *)((muint_t)b - MTCBUF);
        return *(void (**)(mu_t))(buf->data + mu_align(buf->len));
    } else {
        return 0;
    }
}


// Buffer macro for allocating buffers in global space
#define MU_GEN_BUF(name, n)                                                 \
mu_pure mu_t name(void) {                                                   \
    static struct {                                                         \
        mref_t ref;                                                         \
        mlen_t len;                                                         \
        mbyte_t data[n];                                                    \
    } inst = {0, n}                                                         \
                                                                            \
    return (mu_t)((muint_t)&inst + MTBUF);                                  \
}


#endif
