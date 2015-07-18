#include "str.h"

#include "num.h"
#include "err.h"
#include <string.h>


// Internally used conversion between mu_t and struct str
mu_inline mu_t mstr(struct str *s) {
    return (mu_t)((uint_t)s + MU_STR);
}

mu_inline struct str *str_str(mu_t m) {
    return (struct str *)((uint_t)m - MU_STR);
}


// Functions for creating mutable temporary strings
struct str *mstr_create(len_t len) {
    struct str *m = ref_alloc(mu_offset(struct str, data) + len);
    m->len = len;
    return m;
}

void mstr_destroy(struct str *m) {
    ref_dealloc(m, mu_offset(struct str, data) + m->len);
}

// Functions for interning strings
mu_t str_intern(struct str *s, len_t len) {
    // TODO interning
    // TODO maybe not make new alloc for odd len strings?
    if (s->len != len) {
        struct str *ns = mstr_create(len);
        memcpy(ns->data, s->data, len);
        mstr_dec(s);
        s = ns;
    }

    return mstr(s);
}

void str_destroy(mu_t m) {
    // TODO interning
    mstr_destroy(str_str(m));
}

// String creating functions
mu_t mcstr(const char *s) {
    return mnstr((const byte_t *)s, strlen(s));
}

mu_t mnstr(const byte_t *s, uint_t len) {
    if (len > MU_MAXLEN)
        mu_err_len();

    struct str *ns = mstr_create(len);
    memcpy(ns->data, s, len);
    return str_intern(ns, len);
}


// Functions to help modify mutable strings
void mstr_insert(struct str **s, uint_t i, byte_t c) {
    mstr_concat(s, i, &c, 1);
}

void mstr_concat(struct str **s, uint_t i, const byte_t *c, uint_t len) {
    uint_t size = (*s)->len;

    if (size < i+len) {
        size += mu_offset(struct str, data);

        if (size < MU_MINALLOC)
            size = MU_MINALLOC;

        while (size < i+len + mu_offset(struct str, data))
            size <<= 1;

        size -= mu_offset(struct str, data);

        if (size > MU_MAXLEN)
            mu_err_len();

        struct str *ns = mstr_create(size);
        memcpy(ns->data, (*s)->data, (*s)->len);
        mstr_dec(*s);
        *s = ns;
    }

    memcpy(&(*s)->data[i], c, len);
}

// Equality for non-interned strings
bool str_equals(mu_t a, mu_t b) {
    return str_len(a) == str_len(b) && 
           !memcmp(str_bytes(a), str_bytes(b), str_len(a));
}

// Hashing for non-interned strings
// based off the djb2 algorithm
hash_t str_hash(mu_t m) {
    const byte_t *pos = str_bytes(m);
    const byte_t *end = str_bytes(m) + str_len(m);
    hash_t hash = 0;

    while (pos < end) {
        hash = (hash << 5) + hash + *pos++;
    }

    return hash;
}


// Parses a string and returns a string
mu_t str_parse(const byte_t **ppos, const byte_t *end) {
    const byte_t *pos = *ppos + 1;
    byte_t quote = **ppos;
    struct str *m = mstr_create(0);
    uint_t len = 0;

    while (*pos != quote) {
        if (pos == end) {
            mstr_dec(m);
            mu_err_parse(); // Unterminated string
        }

        if (*pos == '\\' && end-pos >= 2) {
            switch (pos[1]) {
                case 'o': 
                    if (end-pos >= 5 && num_val(pos[2]) < 8 &&
                                        num_val(pos[3]) < 8 &&
                                        num_val(pos[4]) < 8) {
                        mstr_insert(&m, len++, num_val(pos[2])*7*7 +
                                               num_val(pos[3])*7 +
                                               num_val(pos[4]));
                        pos += 5;
                    }
                    break;
                
                case 'd':
                    if (end-pos >= 5 && num_val(pos[2]) < 10 &&
                                        num_val(pos[3]) < 10 &&
                                        num_val(pos[4]) < 10) {
                        mstr_insert(&m, len++, num_val(pos[2])*10*10 +
                                               num_val(pos[3])*10 +
                                               num_val(pos[4]));
                        pos += 5;
                    }
                    break;

                case 'x':
                    if (end-pos >= 4 && num_val(pos[2]) < 16 &&
                                        num_val(pos[3]) < 16) {
                        mstr_insert(&m, len++, num_val(pos[2])*16 +
                                               num_val(pos[3]));
                        pos += 4;
                    }
                    break;

                case '\n': pos += 2; break;

                case '\\': mstr_insert(&m, len++, '\\'); pos += 2; break;
                case '\'': mstr_insert(&m, len++, '\''); pos += 2; break;
                case '"':  mstr_insert(&m, len++,  '"'); pos += 2; break;
                case 'a':  mstr_insert(&m, len++, '\a'); pos += 2; break;
                case 'b':  mstr_insert(&m, len++, '\b'); pos += 2; break;
                case 'f':  mstr_insert(&m, len++, '\f'); pos += 2; break;
                case 'n':  mstr_insert(&m, len++, '\n'); pos += 2; break;
                case 'r':  mstr_insert(&m, len++, '\r'); pos += 2; break;
                case 't':  mstr_insert(&m, len++, '\t'); pos += 2; break;
                case 'v':  mstr_insert(&m, len++, '\v'); pos += 2; break;
                case '0':  mstr_insert(&m, len++, '\0'); pos += 2; break;
                default:   mstr_insert(&m, len++, '\\'); pos += 1; break;
            }
        } else {
            mstr_insert(&m, len++, *pos++);
        }
    }

    *ppos = pos + 1;

    return str_intern(m, len);
}


// Returns a string representation of a string
mu_t str_repr(mu_t s) {
    const byte_t *pos = str_bytes(s);
    const byte_t *end = str_bytes(s) + str_len(s);
    struct str *m = mstr_create(2);
    uint_t len = 0;

    mstr_insert(&m, len++, '\'');

    while (pos < end) {
        if (*pos < ' ' || *pos > '~' || 
            *pos == '\\' || *pos == '\'') {
            switch (*pos) {
                case '\\': mstr_concat(&m, len, (byte_t*)"\\\\", 2); len += 2; break;
                case '\'': mstr_concat(&m, len, (byte_t*)"\\'", 2); len += 2; break;
                case '\a': mstr_concat(&m, len, (byte_t*)"\\a", 2); len += 2; break;
                case '\b': mstr_concat(&m, len, (byte_t*)"\\b", 2); len += 2; break;
                case '\f': mstr_concat(&m, len, (byte_t*)"\\f", 2); len += 2; break;
                case '\n': mstr_concat(&m, len, (byte_t*)"\\n", 2); len += 2; break;
                case '\r': mstr_concat(&m, len, (byte_t*)"\\r", 2); len += 2; break;
                case '\t': mstr_concat(&m, len, (byte_t*)"\\t", 2); len += 2; break;
                case '\v': mstr_concat(&m, len, (byte_t*)"\\v", 2); len += 2; break;
                case '\0': mstr_concat(&m, len, (byte_t*)"\\0", 2); len += 2; break;
                default:
                    mstr_concat(&m, len, 
                        (byte_t[]){'\\', 'x', num_ascii(*pos / 16),
                                              num_ascii(*pos % 16)}, 4);
                    len += 4;
                    break;
            }
        } else {
            mstr_insert(&m, len++, *pos);
        }

        pos++;
    }

    mstr_insert(&m, len++, '\'');

    return str_intern(m, len);
}


