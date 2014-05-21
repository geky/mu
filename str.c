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
    str_t *str = var_str(v);
    int i;

    for (i = 0; i < v.len; i++) {
        // hash = 33*hash + str[i]
        hash = (hash << 5) + hash + str[i];
    }

    return hash - 274863188;
}

// Parses a string and returns a string
var_t str_parse(str_t **off, str_t *end) {
    str_t *str = *off + 1;
    str_t quote = **off;
    unsigned int size = 0;

    var_t s;
    unsigned char *out;


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
    s.ref = vref_alloc(size);
    out = (unsigned char *)s.str;

    str = *off + 1;
    end = str + size;

    while (str < end) {
        if (*str == '\\') {
            switch (str[1]) {
                case 'o':
                    *out++ = num_a[str[2]]*7*7 + 
                             num_a[str[3]]*7 + 
                             num_a[str[4]]; 
                    str += 4;
                    break;

                case 'd':
                    *out++ = num_a[str[2]]*10*10 + 
                             num_a[str[3]]*10 + 
                             num_a[str[4]]; 
                    str += 4;
                    break;

                case 'x':
                    *out++ = num_a[str[2]]*16 + 
                             num_a[str[3]]; 
                    str += 3;
                    break;

                case '\n': str += 2; break;
                case '\\': *out++ = '\\'; str += 2; break;
                case '\'': *out++ = '\''; str += 2; break;
                case '"':  *out++ = '"';  str += 2; break;
                case 'a':  *out++ = '\a'; str += 2; break;
                case 'b':  *out++ = '\b'; str += 2; break;
                case 'f':  *out++ = '\f'; str += 2; break;
                case 'n':  *out++ = '\n'; str += 2; break;
                case 'r':  *out++ = '\r'; str += 2; break;
                case 't':  *out++ = '\t'; str += 2; break;
                case 'v':  *out++ = '\v'; str += 2; break;
                case '0':  *out++ = '\0'; str += 2; break;
                default:   *out++ = '\\'; str += 1; break;
            }
        } else {
            *out++ = *str++;
        }
    }


    *off = str + 1;
    
    s.off = 0;
    s.len = size;
    s.type = TYPE_STR;

    return s;
}


// Returns a string representation of a string
var_t str_repr(var_t v) {
    str_t *str = var_str(v);    
    str_t *end = str + v.len;
    unsigned int size = 2;

    var_t s;
    unsigned char *out;

    while (str < end) {
        if (*str < ' ' || *str > '~' || *str == '"') {
            switch (*str) {
                case '"':
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

    s.ref = vref_alloc(size);
    out = (unsigned char *)s.str;

    *out++ = '"';

    while (str < end) {
        if (*str < ' ' || *str > '~' || *str == '"') {
            *out++ = '\\';

            switch (*str) {
                case '"':  *out++ = '"'; break;
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
                    *out++ = (*str/16 < 10) ? ('0' + *str/16) : ('a' + *str/16);
                    *out++ = (*str%16 < 10) ? ('0' + *str%16) : ('a' + *str%16);
                    break;
            }
        } else {
            *out++ = *str;
        }

        str++;
    }

    *out++ = '"';


    s.off = 0;
    s.len = size;
    s.type = TYPE_STR;

    return s;
}


