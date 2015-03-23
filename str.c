#include "str.h"

#include "num.h"
#include "err.h"
#include <string.h>


// Functions for creating mutable temporary strings
mstr_t *mstr_create(len_t len) {
    mstr_t *m = ref_alloc(mu_offset(str_t, data) + len*sizeof(data_t));
    m->len = len;
    return m;
}

void mstr_destroy(mstr_t *m) {
    ref_dealloc(m, mu_offset(str_t, data) + m->len*sizeof(data_t));
}

// Functions for interning strings
str_t *str_intern(str_t *m, len_t len) {
    // TODO interning
    if (m->len == len)
        return m;

    mstr_t *s = mstr_create(len);
    memcpy(s->data, m->data, len);
    return s;
}

void str_destroy(str_t *s) {
    // TODO interning
    mstr_destroy((mstr_t *)s);
}

// String creating functions
str_t *str_nstr(const data_t *s, uint_t len) {
    if (len > MU_MAXLEN)
        mu_err_len();

    mstr_t *m = mstr_create(len);
    memcpy(m->data, s, len);
    return str_intern(m, len);
}

str_t *str_cstr(const char *s) {
    return str_nstr((const data_t *)s, strlen(s));
}

// Functions to help modify mutable strings
void mstr_insert(mstr_t **m, uint_t i, data_t c) {
    mstr_nconcat(m, i, &c, 1);
}

void mstr_concat(mstr_t **m, uint_t i, str_t *s) {
    mstr_nconcat(m, i, s->data, s->len);
}

void mstr_cconcat(mstr_t **m, uint_t i, const char *s) {
    mstr_nconcat(m, i, (const data_t *)s, strlen(s));
}

void mstr_nconcat(mstr_t **m, uint_t i, const data_t *s, uint_t len) {
    uint_t size = (*m)->len;

    if (size < i+len) {
        size += mu_offset(mstr_t, data);

        if (size < MU_MINALLOC)
            size = MU_MINALLOC;

        while (size < i+len + mu_offset(mstr_t, data))
            size <<= 1;

        size -= mu_offset(mstr_t, data);

        if (size > MU_MAXLEN)
            mu_err_len();

        mstr_t *n = mstr_create(size);
        memcpy(n->data, (*m)->data, (*m)->len);
        str_dec(*m);
        *m = n;
    }

    memcpy(&(*m)->data[i], s, len);
}

// Equality for non-interned strings
bool mstr_equals(str_t *a, str_t *b) {
    return a->len == b->len && !memcmp(a->data, b->data, a->len);
}

// Hashing for non-interned strings
// based off the djb2 algorithm
hash_t mstr_hash(str_t *m) {
    const data_t *pos = m->data;
    const data_t *end = m->data + m->len;
    hash_t hash = 0;

    while (pos < end) {
        hash = (hash << 5) + hash + *pos++;
    }

    return hash;
}


// Parses a string and returns a string
str_t *str_parse(const data_t **ppos, const data_t *end) {
    const data_t *pos = *ppos + 1;
    data_t quote = **ppos;
    mstr_t *m = mstr_create(0);
    uint_t len = 0;

    while (*pos != quote) {
        if (pos == end) {
            str_dec(m);
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
str_t *str_repr(str_t *s) {
    const data_t *pos = s->data;
    const data_t *end = s->data + s->len;
    mstr_t *m = mstr_create(2);
    uint_t len = 0;

    mstr_insert(&m, len++, '\'');

    while (pos < end) {
        if (*pos < ' ' || *pos > '~' || 
            *pos == '\\' || *pos == '\'') {
            switch (*pos) {
                case '\\': mstr_cconcat(&m, len, "\\\\"); len += 2; break;
                case '\'': mstr_cconcat(&m, len, "\\'"); len += 2; break;
                case '\a': mstr_cconcat(&m, len, "\\a"); len += 2; break;
                case '\b': mstr_cconcat(&m, len, "\\b"); len += 2; break;
                case '\f': mstr_cconcat(&m, len, "\\f"); len += 2; break;
                case '\n': mstr_cconcat(&m, len, "\\n"); len += 2; break;
                case '\r': mstr_cconcat(&m, len, "\\r"); len += 2; break;
                case '\t': mstr_cconcat(&m, len, "\\t"); len += 2; break;
                case '\v': mstr_cconcat(&m, len, "\\v"); len += 2; break;
                case '\0': mstr_cconcat(&m, len, "\\0"); len += 2; break;
                default:
                    mstr_nconcat(&m, len, 
                        (data_t[]){'\\', 'x', num_ascii(*pos / 16),
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


