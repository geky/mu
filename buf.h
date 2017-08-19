/*
 * Dynamically allocated buffers
 */

#ifndef MU_BUF_H
#define MU_BUF_H
#include "config.h"
#include "types.h"


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

// Destructor type for deallocating buffers
typedef void mdtor_t(mu_t);


// Creates buffer with specified size
mu_t mu_buf_create(muint_t n);
mu_t mu_buf_createdtor(muint_t n, mdtor_t *dtor);
mu_t mu_buf_createtail(muint_t n, mdtor_t *dtor, mu_t tail);

mu_inline mu_t mu_buf_fromdata(const void *s, muint_t n);
mu_inline mu_t mu_buf_fromcstr(const char *s);
mu_inline mu_t mu_buf_fromchr(char s);
mu_t mu_buf_frommu(mu_t s);

// Buffer access
mu_inline bool mu_isdtor(mu_t b, mdtor_t *dtor);

mu_inline mlen_t mu_buf_getlen(mu_t b);
mu_inline void *mu_buf_getdata(mu_t b);
mu_inline mdtor_t *mu_buf_getdtor(mu_t b);
mu_inline mu_t mu_buf_gettail(mu_t b);

// Set destructor for a buffer
void mu_buf_setdtor(mu_t *b, mdtor_t *dtor);
void mu_buf_settail(mu_t *b, mu_t tail);

// Support for read-only attribute access through tail
mu_t mu_buf_lookup(mu_t b, mu_t k);

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

// Buffer access functions
mu_inline bool mu_isdtor(mu_t b, mdtor_t *dtor) {
    if (mu_gettype(b) != MTDBUF) {
        return false;
    }

    struct mbuf *buf = (struct mbuf *)((muint_t)b - MTDBUF);
    return *(mdtor_t **)(buf->data + mu_align(buf->len)) == dtor;
}

mu_inline mlen_t mu_buf_getlen(mu_t b) {
    return ((struct mbuf *)(~7 & (muint_t)b))->len;
}

mu_inline void *mu_buf_getdata(mu_t b) {
    return ((struct mbuf *)(~7 & (muint_t)b))->data;
}

mu_inline mdtor_t *mu_buf_getdtor(mu_t b) {
    if ((MTDBUF^MTBUF) & (muint_t)b) {
        struct mbuf *buf = (struct mbuf *)((muint_t)b - MTDBUF);
        return *(mdtor_t **)(buf->data + mu_align(buf->len));
    } else {
        return 0;
    }
}

mu_inline mu_t mu_buf_gettail(mu_t b) {
    if ((MTDBUF^MTBUF) & (muint_t)b) {
        struct mbuf *buf = (struct mbuf *)((muint_t)b - MTDBUF);
        return mu_inc(*(mu_t *)(buf->data + mu_align(buf->len) +
                sizeof(mdtor_t *)));
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
    } inst = {0, n, {0}};                                                   \
                                                                            \
    return (mu_t)((muint_t)&inst + MTBUF);                                  \
}

#define MU_DEF_BUFDTOR(name, n, dtor)                                       \
mu_pure mu_t name(void) {                                                   \
    static struct {                                                         \
        mref_t ref;                                                         \
        mlen_t len;                                                         \
        mbyte_t data[n];                                                    \
        mdtor_t *dtor;                                                      \
        mu_t tail;                                                          \
    } inst = {0, n, {0}, 0};                                                \
                                                                            \
    return (mu_t)((muint_t)&inst + MTDBUF);                                 \
}

#define MU_DEF_BUFTAIL(name, n, dtor, tail)                                 \
mu_pure mu_t name(void) {                                                   \
    static mu_t ref = 0;                                                    \
    static struct {                                                         \
        mref_t ref;                                                         \
        mlen_t len;                                                         \
        mbyte_t data[n];                                                    \
        mdtor_t *dtor;                                                      \
        mu_t tail;                                                          \
    } inst = {0};                                                           \
                                                                            \
    extern mu_t mu_buf_inittail(struct mbuf *, mlen_t,                      \
            mdtor_t *, mu_t (*)(void));                                     \
    if (!ref) {                                                             \
        ref = mu_buf_inittail((struct mbuf *)&inst, n, dtor, tail);         \
    }                                                                       \
                                                                            \
    return ref;                                                             \
}


#endif
