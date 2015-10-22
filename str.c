#include "str.h"

#include "num.h"
#include "tbl.h"
#include "fn.h"
#include "parse.h"


#define MU_EMPTY_STR mu_empty_str()
#define MU_SPACE_STR mu_space_str()
MSTR(mu_empty_str, "")
MSTR(mu_space_str, " ")


// Mutable string access
mu_inline mu_t mstr(mbyte_t *s) {
    return (mu_t)(s - mu_offset(struct str, data) + MTSTR);
}

mu_inline const struct str *str(mu_t s) {
    return (struct str *)((muint_t)s - MTSTR);
}

mu_inline struct str *wstr(mbyte_t *s) {
    return (struct str *)(s - mu_offset(struct str, data));
}


// Common errors
static mu_noreturn mu_error_length(void) {
    mu_error(mcstr("exceeded max length in string"));
}

static mu_noreturn mu_error_unterminated(void) {
    mu_error(mcstr("unterminated string literal"));
}


// String interning
//
// Currently interning is implemented using a sorted array with a
// binary search for insertion. This was chosen for a few reasons:
// - We can't reuse the table's implementation since it relies
//   on interned strings.
// - Lower memory overhead/more cache friendly than tree
//   or trie implementation.
// - Strings don't need to be hashed, its possible strings won't
//   even need to be completely scanned during comparisons on
//   lookup/insertion.
static mu_t *str_table = 0;
static muint_t str_table_size = 0;
static muint_t str_table_len = 0;

static mint_t str_table_find(const mbyte_t *s, mlen_t len) {
    mint_t min = 0;
    mint_t max = str_table_len-1;

    // binary search for existing strings
    // strings are sorted first by length to avoid comparisons
    while (min <= max) {
        mint_t mid = (max + min) / 2;
        mint_t cmp = len > str(str_table[mid])->len ? +1 :
                     len < str(str_table[mid])->len ? -1 :
                     memcmp(s, str(str_table[mid])->data, len);

        if (cmp == 0) {
            return mid;
        } else if (cmp < 0) {
            max = mid-1;
        } else {
            min = mid+1;
        }
    }

    // Use inverted values to indicate not found but where to
    // insert since 0 is valid in both cases
    return ~min;
}

static void str_table_insert(mint_t i, mu_t s) {
    // expand the table if necessary
    if (str_table_len == str_table_size) {
        muint_t nsize;
        mu_t *ntable;

        if (str_table_size == 0)
            nsize = MU_MINALLOC / sizeof(mu_t);
        else
            nsize = str_table_size << 1;

        ntable = mu_alloc(nsize * sizeof(mu_t));
        memcpy(ntable, str_table, i * sizeof(mu_t));
        memcpy(&ntable[i+1], &str_table[i],
               (str_table_len-i) * sizeof(mu_t));
        mu_dealloc(str_table, str_table_size * sizeof(mu_t));

        str_table = ntable;
        str_table_size = nsize;
    } else {
        memmove(&str_table[i+1], &str_table[i],
                (str_table_len-i) * sizeof(mu_t));
    }

    str_table[i] = s;
    str_table_len += 1;
}

static void str_table_remove(mint_t i) {
    str_table_len -= 1;
    memmove(&str_table[i], &str_table[i+1],
            (str_table_len-i) * sizeof(mu_t));
}


// String management
static mu_t str_intern(const mbyte_t *s, muint_t len) {
    if (len > (mlen_t)-1)
        mu_error_length();

    mint_t i = str_table_find(s, len);
    if (i >= 0)
        return str_inc(str_table[i]);

    // create new string and insert
    mbyte_t *ns = mstr_create(len);
    memcpy(ns, s, len);
    str_table_insert(~i, mstr(ns));
    return str_inc(mstr(ns));
}

void str_destroy(mu_t s) {
    mint_t i = str_table_find(str(s)->data, str(s)->len);

    if (i >= 0)
        str_table_remove(i);

    ref_dealloc(s, mu_offset(struct str, data) + str(s)->len);
}


// Functions for creating mutable temporary strings
// these can avoid unnecessary allocations when interning
// since mstr_intern can reuse their internal structure
mbyte_t *mstr_create(muint_t len) {
    if (len > (mlen_t)-1)
        mu_error_length();

    struct str *s = ref_alloc(mu_offset(struct str, data) + len);
    s->len = len;
    return s->data;
}

void mstr_destroy(mbyte_t *b) {
    ref_dealloc(b - mu_offset(struct str, data), 
                mu_offset(struct str, data) + wstr(b)->len);
}

mu_t mstr_intern(mbyte_t *b, muint_t len) {
    if (len > (mlen_t)-1)
        mu_error_length();

    // unfortunately, reusing the mstr struct only
    // works with exact length currently
    if (wstr(b)->len != len) {
        mu_t m = str_intern(b, len);
        mstr_dec(b);
        return m;
    }

    mint_t i = str_table_find(b, len);
    if (i >= 0) {
        mstr_dec(b);
        return str_inc(str_table[i]);
    }

    str_table_insert(~i, mstr(b));
    return str_inc(mstr(b));
}

// Functions to modify mutable strings
void mstr_insert(mbyte_t **b, muint_t *i, mbyte_t c) {
    mstr_ncat(b, i, &c, 1);
}

void mstr_concat(mbyte_t **b, muint_t *i, mu_t c) {
    mstr_ncat(b, i, str_bytes(c), str_len(c));
    str_dec(c);
}

void mstr_zcat(mbyte_t **b, muint_t *i, const char *c) {
    mstr_ncat(b, i, (mbyte_t *)c, strlen(c));
}

void mstr_ncat(mbyte_t **b, muint_t *i, const mbyte_t *c, muint_t len) {
    muint_t size = wstr(*b)->len;
    muint_t nsize = *i + len;

    if (size < nsize) {
        size += mu_offset(struct str, data);

        if (size < MU_MINALLOC)
            size = MU_MINALLOC;

        while (size < nsize + mu_offset(struct str, data))
            size <<= 1;

        size -= mu_offset(struct str, data);

        if (len > (mlen_t)-1)
            mu_error_length();

        mbyte_t *nb = mstr_create(size);
        memcpy(nb, *b, wstr(*b)->len);
        mstr_dec(*b);
        *b = nb;
    }

    memcpy(&(*b)[*i], c, len);
    *i += len;
}


// Conversion operations
mu_t str_fromnstr(const mbyte_t *s, muint_t len) {
    return str_intern(s, len);
}

mu_t str_fromcstr(const char *s) {
    return str_intern((const mbyte_t *)s, strlen(s));
}

mu_t str_frombyte(mbyte_t c) {
    return mnstr(&c, 1);
}

mu_t str_fromnum(mu_t n) {
    mu_assert(n == muint((mbyte_t)num_uint(n)));
    return str_frombyte((mbyte_t)num_uint(n));
}

mu_t str_fromiter(mu_t iter) {
    return str_join(iter, MU_EMPTY_STR);
}
    

// Comparison operation
mint_t str_cmp(mu_t a, mu_t b) {
    mu_assert(mu_isstr(a) && mu_isstr(b));

    if (a == b)
        return 0;

    muint_t alen = str_len(a);
    muint_t blen = str_len(b);
    mint_t cmp = memcmp(str_bytes(a), str_bytes(b), 
                        alen < blen ? alen : blen);

    return cmp != 0 ? cmp : alen - blen;
}


// String operations
mu_t str_concat(mu_t a, mu_t b) {
    mu_assert(mu_isstr(a) && mu_isstr(b));
    mlen_t alen = str_len(a);
    mlen_t blen = str_len(b);
    mbyte_t *d = mstr_create(alen + blen);

    memcpy(d, str_bytes(a), alen);
    memcpy(d+alen, str_bytes(b), blen);

    str_dec(a);
    str_dec(b);
    return mstr_intern(d, alen + blen);
}

mu_t str_subset(mu_t s, mu_t lower, mu_t upper) {
    mu_assert(mu_isstr(s) && mu_isnum(lower) 
              && (!upper || mu_isnum(upper)));
    mlen_t len = str_len(s);
    muint_t a;
    muint_t b;

    if (num_cmp(lower, muint(len)) >= 0)
        a = len;
    else if (num_cmp(lower, mint(-len)) <= 0)
        a = 0;
    else if (num_cmp(lower, muint(0)) < 0)
        a = str_len(s) + num_int(lower);
    else
        a = num_uint(lower);

    if (!upper)
        b = a + 1;
    else if (num_cmp(upper, muint(len)) >= 0)
        b = len;
    else if (num_cmp(upper, mint(-len)) <= 0)
        b = 0;
    else if (num_cmp(upper, muint(0)) < 0)
        b = str_len(s) + num_int(upper);
    else
        b = num_uint(upper);

    if (a > b)
        return MU_EMPTY_STR;

    mu_t d = mnstr(str_bytes(s) + a, b - a);

    str_dec(s);
    return d;
}

mu_t str_find(mu_t a, mu_t s) {
    mu_assert(mu_isstr(a) && mu_isstr(s));
    const mbyte_t *ab = str_bytes(a);
    mlen_t alen = str_len(a);
    const mbyte_t *sb = str_bytes(s);
    mlen_t slen = str_len(s);

    for (muint_t i = 0; i+slen <= alen; i++) {
        if (memcmp(&ab[i], sb, slen) == 0) {
            str_dec(a);
            str_dec(s);
            return mint(i);
        }
    }

    str_dec(a);
    str_dec(s);
    return 0;
}

mu_t str_replace(mu_t a, mu_t s, mu_t r, mu_t mmax) {
    mu_assert(mu_isstr(a) && mu_isstr(s) && mu_isstr(r) 
              && (!mmax || mu_isnum(mmax)));
    const mbyte_t *ab = str_bytes(a);
    mlen_t alen = str_len(a);
    const mbyte_t *sb = str_bytes(s);
    mlen_t slen = str_len(s);

    muint_t count = 0;
    muint_t max;

    if (!mmax || num_cmp(mmax, muint(alen+1)) >= 0)
        max = alen+1;
    else if (num_cmp(mmax, muint(0)) <= 0)
        max = 0;
    else
        max = num_uint(mmax);

    mbyte_t *db = mstr_create(alen);
    muint_t dlen = 0;
    muint_t i = 0;

    while (i+slen <= alen && count < max) {
        bool match = memcmp(&ab[i], sb, slen) == 0;

        if (match) {
            mstr_concat(&db, &dlen, str_inc(r));
            count++;
            i += slen;
        }

        if (!match || slen == 0) {
            if (i >= alen)
                break;

            mstr_insert(&db, &dlen, ab[i]);
            i += 1;
        }
    }

    mstr_ncat(&db, &dlen, &ab[i], alen-i);

    str_dec(a);
    str_dec(s);
    str_dec(r);
    return mstr_intern(db, dlen);
}

static mc_t str_split_step(mu_t scope, mu_t *frame) {
    mu_t a = tbl_lookup(scope, muint(0));
    const mbyte_t *ab = str_bytes(a);
    mlen_t alen = str_len(a);
    muint_t i = num_uint(tbl_lookup(scope, muint(2)));

    if (i > alen) {
        str_dec(a);
        return 0;
    }

    mu_t s = tbl_lookup(scope, muint(1));
    const mbyte_t *sb = str_bytes(s);
    mlen_t slen = str_len(s);

    muint_t j = i;
    for (; j < alen; j++) {
        if (j+slen <= alen && memcmp(&ab[j], sb, slen) == 0)
            break;
    }

    frame[0] = mnstr(ab+i, j-i);
    tbl_insert(scope, muint(2), muint(j+slen));
    str_dec(a);
    str_dec(s);
    return 1;
}

mu_t str_split(mu_t s, mu_t delim) {
    mu_assert(mu_isstr(s) && (!delim || mu_isstr(delim)));

    if (!delim)
        delim = MU_SPACE_STR;
    else if (str_len(delim) == 0)
        return str_iter(s);

    return msbfn(0x0, str_split_step, mlist({
        s, delim, muint(0),
    }));
}

mu_t str_join(mu_t i, mu_t delim) {
    mu_assert(mu_isfn(i) && (!delim || mu_isstr(delim)));
    mu_t frame[MU_FRAME];
    mbyte_t *b = mstr_create(0);
    muint_t len = 0;
    bool first = true;

    if (!delim)
        delim = MU_SPACE_STR;

    while (fn_next(i, 0x1, frame)) {
        if (!mu_isstr(frame[0]))
            mu_error(mlist({
                mcstr("invalid value "),
                mu_repr(frame[0]),
                mcstr(" passed to join")}));

        if (first)
            first = false;
        else
            mstr_concat(&b, &len, str_inc(delim));

        mstr_concat(&b, &len, frame[0]);
    }

    fn_dec(i);
    str_dec(delim);
    return mstr_intern(b, len);
}

mu_t str_pad(mu_t s, mu_t mlen, mu_t pad) {
    mu_assert(mu_isstr(s) && mu_isnum(mlen) &&
              (!pad || (mu_isstr(pad) && str_len(pad) > 0)));
    
    if (!pad)
        pad = MU_SPACE_STR;

    bool left;
    muint_t len;

    if (num_cmp(mlen, muint(0)) < 0) {
        left = false;
        len = num_uint(num_neg(mlen));
    } else {
        left = true;
        len = num_uint(mlen);
    }

    if (str_len(s) >= len) {
        str_dec(pad);
        return s;
    }

    mbyte_t *db = mstr_create(len);
    muint_t dlen = 0;
    muint_t count = (len - str_len(s)) / str_len(pad);

    if (left)
        mstr_concat(&db, &dlen, s);

    for (muint_t i = 0; i < count; i++)
        mstr_concat(&db, &dlen, str_inc(pad));
    
    if (!left)
        mstr_concat(&db, &dlen, s);

    mu_dec(pad);
    return mstr_intern(db, dlen);
}

mu_t str_strip(mu_t s, mu_t dir, mu_t pad) {
    mu_assert(mu_isstr(s) && (!dir || mu_isnum(dir)) &&
              (!pad || (mu_isstr(pad) && str_len(pad) > 0)));

    if (!pad)
        pad = MU_SPACE_STR;

    const mbyte_t *pos = str_bytes(s);
    const mbyte_t *end = pos + str_len(s);

    const mbyte_t *pb = str_bytes(pad);
    mlen_t plen = str_len(pad);

    if (!dir || num_cmp(dir, muint(0)) <= 0) {
        while (end-pos >= plen && memcmp(pos, pb, plen) == 0)
            pos += plen;
    }
            
    if (!dir || num_cmp(dir, muint(0)) >= 0) {
        while (end-pos >= plen && memcmp(end-plen, pb, plen) == 0)
            end -= plen;
    }

    mu_t d = mnstr(pos, end-pos);
    str_dec(s);
    str_dec(pad);
    return d;
}


// String iteration
bool str_next(mu_t s, muint_t *ip, mu_t *cp) {
    mu_assert(mu_isstr(s));
    muint_t i = *ip;

    if (i >= str_len(s))
        return false;

    if (cp) *cp = str_frombyte(str_bytes(s)[i]);
    *ip = i + 1;
    return true;
}

mc_t str_step(mu_t scope, mu_t *frame) {
    mu_t s = tbl_lookup(scope, muint(0));
    muint_t i = num_uint(tbl_lookup(scope, muint(1)));

    bool next = str_next(s, &i, &frame[0]);
    str_dec(s);
    tbl_insert(scope, muint(1), muint(i));
    return next ? 1 : 0;
}

mu_t str_iter(mu_t s) {
    mu_assert(mu_isstr(s));
    return msbfn(0x00, str_step, mlist({s, muint(0)}));
}


// String representation
mu_t str_parse(const mbyte_t **ppos, const mbyte_t *end) {
    const mbyte_t *pos = *ppos;

    mbyte_t quote = *pos++;
    if (quote != '\'' && quote != '"')
        mu_error_unterminated();

    mbyte_t *s = mstr_create(0);
    muint_t len = 0;

    while (pos < end-1 && *pos != quote) {
        if (*pos == '\\') {
            if (pos[1] == 'b' && pos < end-9 &&
                (mu_fromascii(pos[2]) < 2 &&
                 mu_fromascii(pos[3]) < 2 &&
                 mu_fromascii(pos[4]) < 2 &&
                 mu_fromascii(pos[5]) < 2 &&
                 mu_fromascii(pos[6]) < 2 &&
                 mu_fromascii(pos[7]) < 2 &&
                 mu_fromascii(pos[8]) < 2 &&
                 mu_fromascii(pos[9]) < 2)) {
                mstr_insert(&s, &len, mu_fromascii(pos[2])*2*2*2*2*2*2*2 +
                                      mu_fromascii(pos[3])*2*2*2*2*2*2 +
                                      mu_fromascii(pos[4])*2*2*2*2*2 +
                                      mu_fromascii(pos[5])*2*2*2*2 +
                                      mu_fromascii(pos[6])*2*2*2 +
                                      mu_fromascii(pos[7])*2*2 +
                                      mu_fromascii(pos[8])*2 +
                                      mu_fromascii(pos[9]));
                pos += 10;
            } else if (pos[1] == 'o' && pos < end-4 &&
                (mu_fromascii(pos[2]) < 8 &&
                 mu_fromascii(pos[3]) < 8 &&
                 mu_fromascii(pos[4]) < 8)) {
                mstr_insert(&s, &len, mu_fromascii(pos[2])*8*8 +
                                      mu_fromascii(pos[3])*8 +
                                      mu_fromascii(pos[4]));
                pos += 5;
            } else if (pos[1] == 'd' && pos < end-4 &&
                       (mu_fromascii(pos[2]) < 10 &&
                        mu_fromascii(pos[3]) < 10 &&
                        mu_fromascii(pos[4]) < 10)) {
                mstr_insert(&s, &len, mu_fromascii(pos[2])*10*10 +
                                      mu_fromascii(pos[3])*10 +
                                      mu_fromascii(pos[4]));
                pos += 5;
            } else if (pos[1] == 'x' && pos < end-3 &&
                       (mu_fromascii(pos[2]) < 16 &&
                        mu_fromascii(pos[3]) < 16)) {
                mstr_insert(&s, &len, mu_fromascii(pos[2])*16 +
                                      mu_fromascii(pos[3]));
                pos += 4;
            } else if (pos[1] == '\\') {
                mstr_insert(&s, &len, '\\'); pos += 2;
            } else if (pos[1] == '\'') {
                mstr_insert(&s, &len, '\''); pos += 2;
            } else if (pos[1] == '"') {
                mstr_insert(&s, &len,  '"'); pos += 2;
            } else if (pos[1] == 'f') {
                mstr_insert(&s, &len, '\f'); pos += 2;
            } else if (pos[1] == 'n') {
                mstr_insert(&s, &len, '\n'); pos += 2;
            } else if (pos[1] == 'r') {
                mstr_insert(&s, &len, '\r'); pos += 2;
            } else if (pos[1] == 't') {
                mstr_insert(&s, &len, '\t'); pos += 2;
            } else if (pos[1] == 'v') {
                mstr_insert(&s, &len, '\v'); pos += 2;
            } else if (pos[1] == '0') {
                mstr_insert(&s, &len, '\0'); pos += 2;
            } else {
                mstr_insert(&s, &len, '\\'); pos += 1;
            }
        } else {
            mstr_insert(&s, &len, *pos++);
        }
    }

    if (quote != *pos++)
        mu_error_unterminated();

    *ppos = pos;
    return mstr_intern(s, len);
}

// Returns a string representation of a string
mu_t str_repr(mu_t m) {
    mu_assert(mu_isstr(m));
    const mbyte_t *pos = str_bytes(m);
    const mbyte_t *end = pos + str_len(m);
    mbyte_t *s = mstr_create(2 + str_len(m));
    muint_t len = 0;

    mstr_insert(&s, &len, '\'');

    for (; pos < end; pos++) {
        if (*pos < ' ' || *pos > '~' ||
            *pos == '\\' || *pos == '\'') {
            if (*pos == '\\') mstr_zcat(&s, &len, "\\\\");
            else if (*pos == '\'') mstr_zcat(&s, &len, "\\'");
            else if (*pos == '\f') mstr_zcat(&s, &len, "\\f");
            else if (*pos == '\n') mstr_zcat(&s, &len, "\\n");
            else if (*pos == '\r') mstr_zcat(&s, &len, "\\r");
            else if (*pos == '\t') mstr_zcat(&s, &len, "\\t");
            else if (*pos == '\v') mstr_zcat(&s, &len, "\\v");
            else if (*pos == '\0') mstr_zcat(&s, &len, "\\0");
            else mstr_ncat(&s, &len, (mbyte_t[]){
                    '\\', 'x', mu_toascii(*pos / 16),
                               mu_toascii(*pos % 16)}, 4);
        } else {
            mstr_insert(&s, &len, *pos);
        }
    }

    str_dec(m);
    mstr_insert(&s, &len, '\'');
    return mstr_intern(s, len);
}

static mu_t str_base(mu_t m, char b, muint_t size, muint_t mask, muint_t shift) {
    const mbyte_t *s = str_bytes(m);
    mlen_t len = str_len(m);

    mbyte_t *d = mstr_create(2 + (2+size)*len);
    d[0] = '\'';

    for (muint_t i = 0; i < len; i++) {
        mbyte_t c = s[i];
        d[1 + (2+size)*i+0] = '\\';
        d[1 + (2+size)*i+1] = b;

        for (muint_t j = 0; j < size; j++) {
            d[1 + (2+size)*i+2 + (size-1-j)] = mu_toascii(c & mask);
            c >>= shift;
        }
    }

    d[1 + (2+size)*len] = '\'';
    str_dec(m);
    return mstr_intern(d, 2 + (2+size)*len);
}

mu_t str_bin(mu_t s) {
    mu_assert(mu_isstr(s));
    return str_base(s, 'b', 8,   1, 1);
}

mu_t str_oct(mu_t s) {
    mu_assert(mu_isstr(s));
    return str_base(s, 'o', 3,   7, 3);
}

mu_t str_hex(mu_t s) {
    mu_assert(mu_isstr(s));
    return str_base(s, 'x', 2, 0xf, 4);
}
