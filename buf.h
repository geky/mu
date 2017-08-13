/*
 * Dynamically allocated buffers
 */

#ifndef MU_BUF_H
#define MU_BUF_H
#include "mu.h"


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


// Creates buffer with specified size
mu_t mu_buf_create(muint_t n);
mu_t mu_buf_createdtor(muint_t n, void (*dtor)(mu_t));

mu_inline mu_t mu_buf_fromdata(const void *s, muint_t n);
mu_inline mu_t mu_buf_fromcstr(const char *s);
mu_inline mu_t mu_buf_fromchr(char s);
mu_t mu_buf_frommu(mu_t s);

// Reference counting
mu_inline mu_t mu_buf_inc(mu_t b);
mu_inline void mu_buf_dec(mu_t m);

// Buffer access
mu_inline mlen_t mu_buf_getlen(mu_t b);
mu_inline void *mu_buf_getdata(mu_t b);
mu_inline void (*mu_buf_getdtor(mu_t b))(mu_t);

// Set destructor for a buffer
void mu_buf_setdtor(mu_t *b, void (*dtor)(mu_t));

// Formatting buffers with format strings
mu_t mu_buf_vformat(const char *f, va_list args);
mu_t mu_buf_format(const char *f, ...);

// Resizes buffers into new buffers
// mu_buf_expand may end up with extra space
void mu_buf_resize(mu_t *b, muint_t n);
void mu_buf_expand(mu_t *b, muint_t n);

// Appending to buffers with amortized doubling
void mu_buf_pushdata(mu_t *b, muint_t *i, const void *s, muint_t n);
mu_inline void mu_buf_pushcstr(mu_t *b, muint_t *i, const char *s);
mu_inline void mu_buf_pushchr(mu_t *b, muint_t *i, char s);
void mu_buf_pushmu(mu_t *b, muint_t *i, mu_t s);
void mu_buf_vpushf(mu_t *b, muint_t *i, const char *f, va_list args);
void mu_buf_pushf(mu_t *b, muint_t *i, const char *fmt, ...);


// Buffer creation functions
mu_inline mu_t mu_buf_fromdata(const void *s, muint_t n) {
    mu_t b = mu_buf_create(n);
    memcpy(((struct mbuf *)(~7 & (muint_t)b))->data, s, n);
    return b;
}

mu_inline mu_t mu_buf_fromcstr(const char *s) {
    return mu_buf_fromdata(s, strlen(s));
}

mu_inline mu_t mu_buf_fromchr(char s) {
    return mu_buf_fromdata(&s, 1);
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

// Push functions
mu_inline void mu_buf_pushcstr(mu_t *b, muint_t *i, const char *c) {
    mu_buf_pushdata(b, i, c, strlen(c));
}

mu_inline void mu_buf_pushchr(mu_t *b, muint_t *i, char c) {
    mu_buf_pushdata(b, i, &c, 1);
}



// Buffer macro for allocating buffers in global space
#define MU_DEF_BUF(name, n)                                                 \
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
