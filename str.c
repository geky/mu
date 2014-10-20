#include "str.h"

#include "num.h"
#include "mem.h"


// Functions for creating strings
str_t *str_create(len_t size, veh_t *eh) {
    len_t *len = vref_alloc(size + sizeof(len_t), eh);
    *len = size;

    return (str_t *)(len + 1);
}

// Called by garbage collector to clean up
void str_destroy(void *m) {
    vref_dealloc(m, *(len_t *)m + sizeof(len_t));
}

// Returns true if both variables are equal
bool str_equals(var_t a, var_t b) {
    return (a.len == b.len) && !memcmp(var_str(a), var_str(b), a.len);
}

// Returns a hash for each string
// based off the djb2 algorithm
hash_t str_hash(var_t v) {
    const str_t *str = var_str(v);
    const str_t *end = str + v.len;
    hash_t hash = 5381;

    while (str < end) {
        // hash = 33*hash + *str++
        hash = (hash << 5) + hash + *str++;
    }

    return hash;
}

// Parses a string and returns a string
var_t str_parse(const str_t **off, const str_t *end, veh_t *eh) {
    const str_t *str = *off + 1;
    str_t quote = **off;
    int size = 0;

    while (*str != quote) {
        if (str == end)
            err_parse(eh); // Unterminated string

        if (*str == '\\' && end-str >= 2) {
            switch (str[1]) {
                case 'o': 
                    if (end-str >= 5 && num_val(str[2]) < 8 &&
                                        num_val(str[3]) < 8 &&
                                        num_val(str[4]) < 8) {
                        size++;
                        str += 5;
                    }
                    break;
                
                case 'd':
                    if (end-str >= 5 && num_val(str[2]) < 10 &&
                                        num_val(str[3]) < 10 &&
                                        num_val(str[4]) < 10) {
                        size++;
                        str += 5;
                    }
                    break;

                case 'x':
                    if (end-str >= 4 && num_val(str[2]) < 16 &&
                                        num_val(str[3]) < 16) {
                        size++;
                        str += 4;
                    }
                    break;

                case '\n': 
                    str += 2;
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
                    str += 2;
                    break;
            }
        } else {
            size++;
            str++;
        }
    }

    if (size > VMAXLEN)
        err_len(eh);

    str_t *out = str_create(size, eh);
    str_t *res = out;
    str = *off + 1;

    while (*str != quote) {
        if (*str == '\\') {
            switch (str[1]) {
                case 'o':
                    *res++ = num_val(str[2])*7*7 +
                             num_val(str[3])*7 +
                             num_val(str[4]);
                    str += 4;
                    break;

                case 'd':
                    *res++ = num_val(str[2])*10*10 +
                             num_val(str[3])*10 +
                             num_val(str[4]);
                    str += 4;
                    break;

                case 'x':
                    *res++ = num_val(str[2])*16 +
                             num_val(str[3]);
                    str += 3;
                    break;

                case '\n': str += 2; break;
                case '\\': *res++ = '\\'; str += 2; break;
                case '\'': *res++ = '\''; str += 2; break;
                case '"':  *res++ = '"';  str += 2; break;
                case 'a':  *res++ = '\a'; str += 2; break;
                case 'b':  *res++ = '\b'; str += 2; break;
                case 'f':  *res++ = '\f'; str += 2; break;
                case 'n':  *res++ = '\n'; str += 2; break;
                case 'r':  *res++ = '\r'; str += 2; break;
                case 't':  *res++ = '\t'; str += 2; break;
                case 'v':  *res++ = '\v'; str += 2; break;
                case '0':  *res++ = '\0'; str += 2; break;
                default:   *res++ = '\\'; str += 1; break;
            }
        } else {
            *res++ = *str++;
        }
    }


    *off = str + 1;

    return vstr(out, 0, size);
}


// Returns a string representation of a string
var_t str_repr(var_t v, veh_t *eh) {
    const str_t *str = var_str(v);    
    const str_t *end = str + v.len;
    int size = 2;

    while (str < end) {
        if (*str < ' ' || *str > '~' || 
            *str == '\\' || *str == '\'') {
            switch (*str) {
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
        str++;
    }

    if (size > VMAXLEN)
        err_len(eh);

    str_t *out = str_create(size, eh);
    str_t *res = out;
    str = var_str(v);

    *res++ = '\'';

    while (str < end) {
        if (*str < ' ' || *str > '~' || 
            *str == '\\' || *str == '\'') {
            *res++ = '\\';

            switch (*str) {
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
                    *res++ = num_ascii(*str / 16);
                    *res++ = num_ascii(*str % 16);
                    break;
            }
        } else {
            *res++ = *str;
        }

        str++;
    }

    *res++ = '\'';

    return vstr(out, 0, size);
}


