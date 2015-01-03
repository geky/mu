#include "str.h"

#include "num.h"
#include "err.h"

#include <string.h>


// Functions for creating mutable temporary strings
mstr_t *mstr_create(len_t len, eh_t *eh) {
    mstr_t *m = ref_alloc(sizeof(str_t) + len*sizeof(data_t), eh);
    m->len = len;
    return m;
}

void mstr_destroy(mstr_t *m) {
    ref_dealloc(m, sizeof(str_t) + m->len);
}

// Functions for interning strings
str_t *str_intern(str_t *m, eh_t *eh) {
    return m; // TODO interning
}

void str_destroy(str_t *s) {
    mstr_destroy((mstr_t *)s); // TODO interning
}

// String creating functions
str_t *str_nstr(const data_t *s, len_t len, eh_t *eh) {
    mstr_t *m = mstr_create(len, eh);
    memcpy(m->data, s, len);
    return str_intern(m, eh);
}

str_t *str_sstr(const data_t *s, size_t len, eh_t *eh) {
    if (len > MU_MAXLEN)
        err_len(eh);

    return str_nstr(s, len, eh);
}

str_t *str_cstr(const char *s, eh_t *eh) {
    return str_sstr((const data_t *)s, strlen(s), eh);
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
str_t *str_parse(const data_t **ppos, const data_t *end, eh_t *eh) {
    const data_t *pos = *ppos + 1;
    data_t quote = **ppos;
    size_t size = 0;

    while (*pos != quote) {
        if (pos == end)
            err_parse(eh); // Unterminated string

        if (*pos == '\\' && end-pos >= 2) {
            switch (pos[1]) {
                case 'o': 
                    if (end-pos >= 5 && num_val(pos[2]) < 8 &&
                                        num_val(pos[3]) < 8 &&
                                        num_val(pos[4]) < 8) {
                        size++;
                        pos += 5;
                    }
                    break;
                
                case 'd':
                    if (end-pos >= 5 && num_val(pos[2]) < 10 &&
                                        num_val(pos[3]) < 10 &&
                                        num_val(pos[4]) < 10) {
                        size++;
                        pos += 5;
                    }
                    break;

                case 'x':
                    if (end-pos >= 4 && num_val(pos[2]) < 16 &&
                                        num_val(pos[3]) < 16) {
                        size++;
                        pos += 4;
                    }
                    break;

                case '\n': 
                    pos += 2;
                    break;

                case '\\':
                case '\'':
                case '"':
                case 'a':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                case 'v':
                case '0': 
                    size++;
                    pos += 2;
                    break;
            }
        } else {
            size++;
            pos++;
        }
    }

    if (size > MU_MAXLEN)
        err_len(eh);

    mstr_t *m = mstr_create(size, eh);
    data_t *out = m->data;
    pos = *ppos + 1;

    while (*pos != quote) {
        if (*pos == '\\') {
            switch (pos[1]) {
                case 'o':
                    *out++ = num_val(pos[2])*7*7 +
                             num_val(pos[3])*7 +
                             num_val(pos[4]);
                    pos += 4;
                    break;

                case 'd':
                    *out++ = num_val(pos[2])*10*10 +
                             num_val(pos[3])*10 +
                             num_val(pos[4]);
                    pos += 4;
                    break;

                case 'x':
                    *out++ = num_val(pos[2])*16 +
                             num_val(pos[3]);
                    pos += 3;
                    break;

                case '\n': pos += 2; break;
                case '\\': *out++ = '\\'; pos += 2; break;
                case '\'': *out++ = '\''; pos += 2; break;
                case '"':  *out++ = '"';  pos += 2; break;
                case 'a':  *out++ = '\a'; pos += 2; break;
                case 'b':  *out++ = '\b'; pos += 2; break;
                case 'f':  *out++ = '\f'; pos += 2; break;
                case 'n':  *out++ = '\n'; pos += 2; break;
                case 'r':  *out++ = '\r'; pos += 2; break;
                case 't':  *out++ = '\t'; pos += 2; break;
                case 'v':  *out++ = '\v'; pos += 2; break;
                case '0':  *out++ = '\0'; pos += 2; break;
                default:   *out++ = '\\'; pos += 1; break;
            }
        } else {
            *out++ = *pos++;
        }
    }

    *ppos = pos + 1;

    return str_intern(m, eh);
}


// Returns a string representation of a string
str_t *str_repr(str_t *s, eh_t *eh) {
    const data_t *pos = s->data;
    const data_t *end = s->data + s->len;
    size_t size = 2;

    while (pos < end) {
        if (*pos < ' ' || *pos > '~' || 
            *pos == '\\' || *pos == '\'') {
            switch (*pos) {
                case '\'':
                case '\\':
                case '\a':
                case '\b':
                case '\f':
                case '\n':
                case '\r':
                case '\t':
                case '\v':
                case '\0': size += 1; break;
                default: size += 3; break;
            }
        }

        size++;
        pos++;
    }

    if (size > MU_MAXLEN)
        err_len(eh);

    mstr_t *m = mstr_create(size, eh);
    data_t *out = m->data;
    pos = s->data;

    *out++ = '\'';

    while (pos < end) {
        if (*pos < ' ' || *pos > '~' || 
            *pos == '\\' || *pos == '\'') {
            *out++ = '\\';

            switch (*pos) {
                case '\\': *out++ = '\\'; break;
                case '\'': *out++ = '\''; break;
                case '\a': *out++ = 'a'; break;
                case '\b': *out++ = 'b'; break;
                case '\f': *out++ = 'f'; break;
                case '\n': *out++ = 'n'; break;
                case '\r': *out++ = 'r'; break;
                case '\t': *out++ = 't'; break;
                case '\v': *out++ = 'v'; break;
                case '\0': *out++ = '0'; break;
                default:
                    *out++ = 'x';
                    *out++ = num_ascii(*pos / 16);
                    *out++ = num_ascii(*pos % 16);
                    break;
            }
        } else {
            *out++ = *pos;
        }

        pos++;
    }

    *out++ = '\'';

    return str_intern(m, eh);
}


