#include "str.h"

#include "num.h"
#include "mem.h"


// Functions for creating strings
str_t *str_create(len_t size, eh_t *eh) {
    len_t *len = ref_alloc(size + sizeof(len_t), eh);
    *len = size;

    return (str_t *)(len + 1);
}

// Called by garbage collector to clean up
void str_destroy(void *m) {
    ref_dealloc(m, *(len_t *)m + sizeof(len_t));
}

// Returns true if both variables are equal
bool str_equals(var_t a, var_t b) {
    return (a.len == b.len) && !memcmp(getstr(a), getstr(b), a.len);
}

// Returns a hash for each string
// based off the djb2 algorithm
hash_t str_hash(var_t v) {
    const str_t *str = getstr(v);
    const str_t *end = str + v.len;
    hash_t hash = 5381;

    while (str < end) {
        // hash = 33*hash + *str++
        hash = (hash << 5) + hash + *str++;
    }

    return hash;
}

// Parses a string and returns a string
var_t str_parse(const str_t **off, const str_t *end, eh_t *eh) {
    const str_t *pos = *off + 1;
    str_t quote = **off;
    int size = 0;

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

    str_t *out = str_create(size, eh);
    str_t *res = out;
    pos = *off + 1;

    while (*pos != quote) {
        if (*pos == '\\') {
            switch (pos[1]) {
                case 'o':
                    *res++ = num_val(pos[2])*7*7 +
                             num_val(pos[3])*7 +
                             num_val(pos[4]);
                    pos += 4;
                    break;

                case 'd':
                    *res++ = num_val(pos[2])*10*10 +
                             num_val(pos[3])*10 +
                             num_val(pos[4]);
                    pos += 4;
                    break;

                case 'x':
                    *res++ = num_val(pos[2])*16 +
                             num_val(pos[3]);
                    pos += 3;
                    break;

                case '\n': pos += 2; break;
                case '\\': *res++ = '\\'; pos += 2; break;
                case '\'': *res++ = '\''; pos += 2; break;
                case '"':  *res++ = '"';  pos += 2; break;
                case 'a':  *res++ = '\a'; pos += 2; break;
                case 'b':  *res++ = '\b'; pos += 2; break;
                case 'f':  *res++ = '\f'; pos += 2; break;
                case 'n':  *res++ = '\n'; pos += 2; break;
                case 'r':  *res++ = '\r'; pos += 2; break;
                case 't':  *res++ = '\t'; pos += 2; break;
                case 'v':  *res++ = '\v'; pos += 2; break;
                case '0':  *res++ = '\0'; pos += 2; break;
                default:   *res++ = '\\'; pos += 1; break;
            }
        } else {
            *res++ = *pos++;
        }
    }


    *off = pos + 1;

    return vstr(out, 0, size);
}


// Returns a string representation of a string
var_t str_repr(var_t v, eh_t *eh) {
    const str_t *pos = getstr(v);    
    const str_t *end = pos + v.len;
    int size = 2;

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

    str_t *out = str_create(size, eh);
    str_t *res = out;
    pos = getstr(v);

    *res++ = '\'';

    while (pos < end) {
        if (*pos < ' ' || *pos > '~' || 
            *pos == '\\' || *pos == '\'') {
            *res++ = '\\';

            switch (*pos) {
                case '\\': *res++ = '\\'; break;
                case '\'': *res++ = '\''; break;
                case '\a': *res++ = 'a'; break;
                case '\b': *res++ = 'b'; break;
                case '\f': *res++ = 'f'; break;
                case '\n': *res++ = 'n'; break;
                case '\r': *res++ = 'r'; break;
                case '\t': *res++ = 't'; break;
                case '\v': *res++ = 'v'; break;
                case '\0': *res++ = '0'; break;
                default:
                    *res++ = 'x';
                    *res++ = num_ascii(*pos / 16);
                    *res++ = num_ascii(*pos % 16);
                    break;
            }
        } else {
            *res++ = *pos;
        }

        pos++;
    }

    *res++ = '\'';

    return vstr(out, 0, size);
}


