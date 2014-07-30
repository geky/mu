#include "str.h"

#include "num.h"
#include "mem.h"

#include <assert.h>


// Returns true if both variables are equal
bool str_equals(var_t a, var_t b) {
    return (a.len == b.len) && !memcmp(var_str(a), var_str(b), a.len);
}

// Returns a hash for each number
// For integers this is the number
hash_t str_hash(var_t v) {
    // based off the djb2 algorithm
    hash_t hash = 5381;
    const str_t *str = var_str(v);
    int i;

    for (i = 0; i < v.len; i++) {
        // hash = 33*hash + str[i]
        hash = (hash << 5) + hash + str[i];
    }

    return hash;
}

// Parses a string and returns a string
var_t str_parse(const str_t **off, const str_t *end) {
    const str_t *str = *off + 1;
    const str_t quote = **off;
    int size = 0;

    str_t *s, *out;


    while (str < end) {
        if (*str == quote) {
            goto load;

        } else if (*str == '\\') {
            switch (str[1]) {
                case 'o': 
                    if (end-str >= 5 && num_a[str[2]] < 8 &&
                                        num_a[str[3]] < 8 &&
                                        num_a[str[4]] < 8) {
                        str += 4;
                    }
                    break;
                
                case 'd':
                    if (end-str >= 5 && num_a[str[2]] < 10 &&
                                        num_a[str[3]] < 10 &&
                                        num_a[str[4]] < 10) {
                        str += 4;
                    }
                    break;

                case 'x':
                    if (end-str >= 4 && num_a[str[2]] < 16 &&
                                        num_a[str[3]] < 16) {
                        str += 3;
                    }
                    break;

                case '\n': 
                    if (end-str >= 2) {
                        size -= 1; 
                        str += 1;
                    }
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
                    if (end-str >= 2) {
                        str += 1;
                    }
                    break;
            }
        }

        size++;
        str++;
    }

    assert(false); // TODO error here: unterminated string


load:
    out = vref_alloc(size);
    s = out;

    str = *off + 1;
    end = str + size;

    while (str < end) {
        if (*str == '\\') {
            switch (str[1]) {
                case 'o':
                    *s++ = num_a[str[2]]*7*7 + 
                           num_a[str[3]]*7 + 
                           num_a[str[4]]; 
                    str += 4;
                    break;

                case 'd':
                    *s++ = num_a[str[2]]*10*10 + 
                           num_a[str[3]]*10 + 
                           num_a[str[4]]; 
                    str += 4;
                    break;

                case 'x':
                    *s++ = num_a[str[2]]*16 + 
                           num_a[str[3]]; 
                    str += 3;
                    break;

                case '\n': str += 2; break;
                case '\\': *s++ = '\\'; str += 2; break;
                case '\'': *s++ = '\''; str += 2; break;
                case '"':  *s++ = '"';  str += 2; break;
                case 'a':  *s++ = '\a'; str += 2; break;
                case 'b':  *s++ = '\b'; str += 2; break;
                case 'f':  *s++ = '\f'; str += 2; break;
                case 'n':  *s++ = '\n'; str += 2; break;
                case 'r':  *s++ = '\r'; str += 2; break;
                case 't':  *s++ = '\t'; str += 2; break;
                case 'v':  *s++ = '\v'; str += 2; break;
                case '0':  *s++ = '\0'; str += 2; break;
                default:   *s++ = '\\'; str += 1; break;
            }
        } else {
            *s++ = *str++;
        }
    }


    *off = str + 1;

    return vstr(out, 0, size);
}


// Returns a string representation of a string
var_t str_repr(var_t v) {
    const str_t *str = var_str(v);    
    const str_t *end = str + v.len;
    int size = 2;

    str_t *s, *out;

    while (str < end) {
        if (*str < ' ' || *str > '~' || *str == '\'') {
            switch (*str) {
                case '\'':
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


    str = var_str(v);

    out = vref_alloc(size);
    s = out;

    *s++ = '\'';

    while (str < end) {
        if (*str < ' ' || *str > '~' || *str == '\'') {
            *s++ = '\\';

            switch (*str) {
                case '\'': *s++ = '\''; break;
                case '\a': *s++ = 'a'; break;
                case '\b': *s++ = 'b'; break;
                case '\f': *s++ = 'f'; break;
                case '\n': *s++ = 'n'; break;
                case '\r': *s++ = 'r'; break;
                case '\t': *s++ = 't'; break;
                case '\v': *s++ = 'v'; break;
                case '\0': *s++ = '0'; break;
                default:
                    *s++ = 'x';
                    *s++ = (*str/16 < 10) ? ('0' + *str/16) : ('a'-10 + *str/16);
                    *s++ = (*str%16 < 10) ? ('0' + *str%16) : ('a'-10 + *str%16);
                    break;
            }
        } else {
            *s++ = *str;
        }

        str++;
    }

    *s++ = '\'';

    return vstr(out, 0, size);
}


