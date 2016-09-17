/*
 * Dynamically allocated buffers
 */

#ifndef MU_BUF_H
#define MU_BUF_H
#include "mu.h"


// Creates buffer with specified size
mu_t buf_create(muint_t n);

// Resizes buffers into new buffers
// buf_expand may end up with extra space
void buf_resize(mu_t *b, muint_t n);
void buf_expand(mu_t *b, muint_t n);

// Adding to buffers with amortized doubling
void buf_push(mu_t *b, muint_t *i, mbyte_t byte);
void buf_append(mu_t *b, muint_t *i, const void *c, muint_t n);
void buf_concat(mu_t *b, muint_t *i, mu_t c);

// Formatting buffers with format strings
void buf_vformat(mu_t *b, muint_t *i, const char *f, va_list args);
void buf_format(mu_t *b, muint_t *i, const char *fmt, ...);


// Definition of internal Mu buffers
//
// Each buffer is stored as a length and array of data.
// Buffers can be mutated and used freely in C function,
// but can't be accessed from Mu.
struct buf {
    mref_t ref;     // reference count
    mlen_t len;     // length of allocated data
    // data follows
};


// Buffer creation functions
mu_inline mu_t mbuf(const void *s, muint_t n) {
    mu_t b = buf_create(n);
    memcpy((struct buf *)((muint_t)b - MTBUF) + 1, s, n);
    return b;
}

// Reference counting
mu_inline mu_t buf_inc(mu_t b) {
    mu_assert(mu_isbuf(b));
    ref_inc(b);
    return b;
}

mu_inline void buf_dec(mu_t m) {
    mu_assert(mu_isbuf(m));
    extern void buf_destroy(mu_t);
    if (ref_dec(m)) {
        buf_destroy(m);
    }
}

// Buffer access functions
mu_inline mlen_t buf_len(mu_t b) {
    return ((struct buf *)((muint_t)b - MTBUF))->len;
}

mu_inline void *buf_data(mu_t b) {
    return (struct buf *)((muint_t)b - MTBUF) + 1;
}


// Buffer macro for allocating buffers in global space
#define MBUF(name, n)                                                       \
mu_pure mu_t name(void) {                                                   \
    static struct {                                                         \
        struct buf buf;                                                     \
        mbyte_t data[n];                                                    \
    } inst = {{0, n}, 0}                                                    \
                                                                            \
    return (mu_t)((muint_t)&inst + MTBUF);                                  \
}


#endif
