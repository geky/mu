#include "str.h"

#include "buf.h"
#include "num.h"
#include "tbl.h"
#include "fn.h"
#include "parse.h"


#define MU_EMPTY_STR mu_empty_str()
#define MU_SPACE_STR mu_space_str()
MU_GEN_STR(mu_empty_str, "")
MU_GEN_STR(mu_space_str, " ")


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
static mu_t *mu_str_table = 0;
static muint_t mu_str_table_size = 0;
static muint_t mu_str_table_len = 0;

static mint_t mu_str_table_find(const mbyte_t *s, mlen_t len) {
    mint_t min = 0;
    mint_t max = mu_str_table_len-1;

    // binary search for existing strings
    // strings are sorted first by length to avoid comparisons
    while (min <= max) {
        mint_t mid = (max + min) / 2;
        mint_t cmp = len > mu_str_getlen(mu_str_table[mid]) ? +1 :
                     len < mu_str_getlen(mu_str_table[mid]) ? -1 :
                     memcmp(s, mu_str_getdata(mu_str_table[mid]), len);

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

static void mu_str_table_insert(mint_t i, mu_t s) {
    // expand the table if necessary
    if (mu_str_table_len == mu_str_table_size) {
        muint_t nsize;
        mu_t *ntable;

        if (mu_str_table_size == 0) {
            nsize = MU_MINALLOC / sizeof(mu_t);
        } else {
            nsize = mu_str_table_size << 1;
        }

        ntable = mu_alloc(nsize * sizeof(mu_t));
        memcpy(ntable, mu_str_table, i * sizeof(mu_t));
        memcpy(&ntable[i+1], &mu_str_table[i],
               (mu_str_table_len-i) * sizeof(mu_t));
        mu_dealloc(mu_str_table, mu_str_table_size * sizeof(mu_t));

        mu_str_table = ntable;
        mu_str_table_size = nsize;
    } else {
        memmove(&mu_str_table[i+1], &mu_str_table[i],
                (mu_str_table_len-i) * sizeof(mu_t));
    }

    mu_str_table[i] = s;
    mu_str_table_len += 1;
}

static void mu_str_table_remove(mint_t i) {
    mu_str_table_len -= 1;
    memmove(&mu_str_table[i], &mu_str_table[i+1],
            (mu_str_table_len-i) * sizeof(mu_t));
}


// String management
// This can avoid unnecessary allocations sometimes since
// buf's internal structure is reused for interned strings
mu_t mu_str_intern(mu_t b, muint_t n) {
    mu_assert(mu_isbuf(b));

    mint_t i = mu_str_table_find(mu_buf_getdata(b), n);
    if (i >= 0) {
        mu_buf_dec(b);
        return mu_str_inc(mu_str_table[i]);
    }

    if (mu_buf_getdtor(b)) {
        mu_buf_setdtor(&b, 0);
    }

    if (mu_buf_getlen(b) != n) {
        mu_buf_resize(&b, n);
    }

    mu_t s = (mu_t)((muint_t)b - MTBUF + MTSTR);
    mu_str_table_insert(~i, s);
    return mu_str_inc(s);
}

mu_t mu_str_fromdata(const mbyte_t *s, muint_t n) {
    if (n > (mlen_t)-1) {
        mu_errorf("exceeded max length in string");
    }

    mint_t i = mu_str_table_find(s, n);
    if (i >= 0) {
        return mu_str_inc(mu_str_table[i]);
    }

    // create new string and insert
    mu_t b = mu_buf_create(n);
    memcpy(mu_buf_getdata(b), s, n);

    mu_t ns = (mu_t)((muint_t)b - MTBUF + MTSTR);
    mu_str_table_insert(~i, ns);
    return mu_str_inc(ns);
}

void mu_str_destroy(mu_t s) {
    mint_t i = mu_str_table_find(mu_str_getdata(s), mu_str_getlen(s));
    mu_assert(i >= 0);
    mu_str_table_remove(i);

    mu_ref_dealloc(s, mu_offsetof(struct mstr, data) + mu_str_getlen(s));
}


// String creating functions
mu_t mu_str_init(const struct mstr *s) {
    mu_t m = mu_str_intern((mu_t)((muint_t)s + MTBUF), s->len);

    if (*(mref_t *)((muint_t)m - MTSTR) != 0) {
        *(mref_t *)((muint_t)m - MTSTR) = 0;
    }

    return m;
}

mu_t mu_str_fromiter(mu_t iter) {
    return mu_fn_call(MU_JOIN, 0x21, iter, MU_EMPTY_STR);
}

mu_t mu_str_vformat(const char *f, va_list args) {
    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_buf_vformat(&b, &n, f, args);
    return mu_str_intern(b, n);
}

mu_t mu_str_format(const char *f, ...) {
    va_list args;
    va_start(args, f);
    mu_t m = mu_str_vformat(f, args);
    va_end(args);
    return m;
}


// Comparison operation
mint_t mu_str_cmp(mu_t a, mu_t b) {
    mu_assert(mu_isstr(a) && mu_isstr(b));

    if (a == b) {
        return 0;
    }

    muint_t alen = mu_str_getlen(a);
    muint_t blen = mu_str_getlen(b);
    mint_t cmp = memcmp(mu_str_getdata(a), mu_str_getdata(b),
                        alen < blen ? alen : blen);

    return cmp != 0 ? cmp : alen - blen;
}


// String iteration
bool mu_str_next(mu_t s, muint_t *ip, mu_t *cp) {
    mu_assert(mu_isstr(s));
    muint_t i = *ip;

    if (i >= mu_str_getlen(s)) {
        return false;
    }

    if (cp) *cp = mu_str_fromdata(&mu_str_getdata(s)[i], 1);
    *ip = i + 1;
    return true;
}

static mcnt_t mu_str_step(mu_t scope, mu_t *frame) {
    mu_t s = mu_tbl_lookup(scope, mu_num_fromuint(0));
    muint_t i = mu_num_getuint(mu_tbl_lookup(scope, mu_num_fromuint(1)));

    bool next = mu_str_next(s, &i, &frame[0]);
    mu_str_dec(s);
    mu_tbl_insert(scope, mu_num_fromuint(1), mu_num_fromuint(i));
    return next ? 1 : 0;
}

mu_t mu_str_iter(mu_t s) {
    mu_assert(mu_isstr(s));
    return mu_fn_fromsbfn(0x00, mu_str_step,
            mu_tbl_fromlist((mu_t[]){s, mu_num_fromuint(0)}, 2));
}


// String operations
mu_t mu_str_concat(mu_t a, mu_t b) {
    mu_assert(mu_isstr(a) && mu_isstr(b));
    muint_t an = mu_str_getlen(a);
    muint_t bn = mu_str_getlen(b);
    mu_t d = mu_buf_create(an + bn);

    memcpy((mbyte_t *)mu_buf_getdata(d), mu_str_getdata(a), an);
    memcpy((mbyte_t *)mu_buf_getdata(d)+an, mu_str_getdata(b), bn);

    mu_str_dec(a);
    mu_str_dec(b);
    return mu_str_intern(d, an + bn);
}

mu_t mu_str_subset(mu_t s, mint_t lower, mint_t upper) {
    mu_assert(mu_isstr(s));
    lower = (lower >= 0) ? lower : lower + mu_str_getlen(s);
    upper = (upper >= 0) ? upper : upper + mu_str_getlen(s);

    if (lower < 0) {
        lower = 0;
    }

    if (upper > mu_str_getlen(s)) {
        upper = mu_str_getlen(s);
    }

    if (lower >= upper) {
        return MU_EMPTY_STR;
    }

    mu_t d = mu_str_fromdata(mu_str_getdata(s) + lower, upper - lower);
    mu_str_dec(s);
    return d;
}


// String representation
mu_t mu_str_parse(const mbyte_t **ppos, const mbyte_t *end) {
    const mbyte_t *pos = *ppos;
    mbyte_t quote = *pos++;
    mu_t b = mu_buf_create(0);
    muint_t n = 0;

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
                mu_buf_push(&b, &n,
                        mu_fromascii(pos[2])*2*2*2*2*2*2*2 +
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
                mu_buf_push(&b, &n,
                        mu_fromascii(pos[2])*8*8 +
                        mu_fromascii(pos[3])*8 +
                        mu_fromascii(pos[4]));
                pos += 5;
            } else if (pos[1] == 'd' && pos < end-4 &&
                       (mu_fromascii(pos[2]) < 10 &&
                        mu_fromascii(pos[3]) < 10 &&
                        mu_fromascii(pos[4]) < 10)) {
                mu_buf_push(&b, &n,
                        mu_fromascii(pos[2])*10*10 +
                        mu_fromascii(pos[3])*10 +
                        mu_fromascii(pos[4]));
                pos += 5;
            } else if (pos[1] == 'x' && pos < end-3 &&
                       (mu_fromascii(pos[2]) < 16 &&
                        mu_fromascii(pos[3]) < 16)) {
                mu_buf_push(&b, &n,
                        mu_fromascii(pos[2])*16 +
                        mu_fromascii(pos[3]));
                pos += 4;
            } else if (pos[1] == '\\') {
                mu_buf_push(&b, &n, '\\'); pos += 2;
            } else if (pos[1] == '\'') {
                mu_buf_push(&b, &n, '\''); pos += 2;
            } else if (pos[1] == '\"') {
                mu_buf_push(&b, &n, '\"'); pos += 2;
            } else if (pos[1] == 'f') {
                mu_buf_push(&b, &n, '\f'); pos += 2;
            } else if (pos[1] == 'n') {
                mu_buf_push(&b, &n, '\n'); pos += 2;
            } else if (pos[1] == 'r') {
                mu_buf_push(&b, &n, '\r'); pos += 2;
            } else if (pos[1] == 't') {
                mu_buf_push(&b, &n, '\t'); pos += 2;
            } else if (pos[1] == 'v') {
                mu_buf_push(&b, &n, '\v'); pos += 2;
            } else if (pos[1] == '0') {
                mu_buf_push(&b, &n, '\0'); pos += 2;
            } else {
                mu_buf_push(&b, &n, '\\'); pos += 1;
            }
        } else {
            mu_buf_push(&b, &n, *pos++);
        }
    }

    if (quote != *pos++) {
        mu_errorf("unterminated string literal");
    }

    *ppos = pos;
    return mu_str_intern(b, n);
}

// Returns a string representation of a string
mu_t mu_str_repr(mu_t m) {
    mu_assert(mu_isstr(m));
    const mbyte_t *pos = mu_str_getdata(m);
    const mbyte_t *end = pos + mu_str_getlen(m);
    mu_t b = mu_buf_create(2 + mu_str_getlen(m));
    muint_t n = 0;

    mu_buf_push(&b, &n, '\'');

    for (; pos < end; pos++) {
        if (*pos < ' ' || *pos > '~' ||
            *pos == '\\' || *pos == '\'') {
            if (*pos == '\\')      mu_buf_format(&b, &n, "\\\\");
            else if (*pos == '\'') mu_buf_format(&b, &n, "\\'");
            else if (*pos == '\f') mu_buf_format(&b, &n, "\\f");
            else if (*pos == '\n') mu_buf_format(&b, &n, "\\n");
            else if (*pos == '\r') mu_buf_format(&b, &n, "\\r");
            else if (*pos == '\t') mu_buf_format(&b, &n, "\\t");
            else if (*pos == '\v') mu_buf_format(&b, &n, "\\v");
            else if (*pos == '\0') mu_buf_format(&b, &n, "\\0");
            else                   mu_buf_format(&b, &n, "\\x%bx", *pos);
        } else {
            mu_buf_push(&b, &n, *pos);
        }
    }

    mu_str_dec(m);
    mu_buf_push(&b, &n, '\'');
    return mu_str_intern(b, n);
}

static mu_t mu_str_base(mu_t m, char b,
        muint_t size, muint_t mask, muint_t shift) {
    const mbyte_t *s = mu_str_getdata(m);
    mlen_t len = mu_str_getlen(m);

    mu_t buf = mu_buf_create(2 + (2+size)*len);
    mbyte_t *d = mu_buf_getdata(buf);
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
    mu_str_dec(m);
    return mu_str_intern(buf, 2 + (2+size)*len);
}

mu_t mu_str_bin(mu_t s) {
    mu_assert(mu_isstr(s));
    return mu_str_base(s, 'b', 8,   1, 1);
}

mu_t mu_str_oct(mu_t s) {
    mu_assert(mu_isstr(s));
    return mu_str_base(s, 'o', 3,   7, 3);
}

mu_t mu_str_hex(mu_t s) {
    mu_assert(mu_isstr(s));
    return mu_str_base(s, 'x', 2, 0xf, 4);
}


// String related functions in Mu
static mcnt_t mu_bfn_str(mu_t *frame) {
    mu_t m = frame[0];

    switch (mu_gettype(m)) {
        case MTNIL:
            frame[0] = MU_EMPTY_STR;
            return 1;

        case MTNUM:
            if (m == mu_num_fromuint((mbyte_t)mu_num_getuint(m))) {
                frame[0] = mu_str_fromdata((mbyte_t[]){mu_num_getuint(m)}, 1);
                return 1;
            }
            break;

        case MTSTR:
            frame[0] = frame[0];
            return 1;

        case MTTBL:
        case MTFN:
            frame[0] = m;
            mu_fn_fcall(MU_ITER, 0x11, frame);
            frame[0] = mu_str_fromiter(frame[0]);
            return 1;

        default:
            break;
    }

    mu_error_cast(MU_KEY_STR, m);
}

MU_GEN_STR(mu_gen_key_str, "str")
MU_GEN_BFN(mu_gen_str, 0x1, mu_bfn_str)

static mcnt_t mu_bfn_find(mu_t *frame) {
    mu_t s = frame[0];
    mu_t m = frame[1];
    if (!mu_isstr(s) || !mu_isstr(m)) {
        mu_error_arg(MU_KEY_FIND, 0x2, frame);
    }

    const mbyte_t *sb = mu_str_getdata(s);
    mlen_t slen = mu_str_getlen(s);
    const mbyte_t *mb = mu_str_getdata(m);
    mlen_t mlen = mu_str_getlen(m);

    for (muint_t i = 0; i+mlen <= slen; i++) {
        if (memcmp(&sb[i], mb, mlen) == 0) {
            mu_str_dec(m);
            mu_str_dec(s);
            frame[0] = mu_num_fromuint(i);
            frame[1] = mu_num_fromuint(i + mlen);
            return 2;
        }
    }

    mu_str_dec(s);
    mu_str_dec(m);
    return 0;
}

MU_GEN_STR(mu_gen_key_find, "find")
MU_GEN_BFN(mu_gen_find, 0x2, mu_bfn_find)

static mcnt_t mu_bfn_replace(mu_t *frame) {
    mu_t s = frame[0];
    mu_t m = frame[1];
    mu_t r = frame[2];
    if (!mu_isstr(s) || !mu_isstr(m) || !mu_isstr(r)) {
        mu_error_arg(MU_KEY_REPLACE, 0x3, frame);
    }

    const mbyte_t *sb = mu_str_getdata(s);
    mlen_t slen = mu_str_getlen(s);
    const mbyte_t *mb = mu_str_getdata(m);
    mlen_t mlen = mu_str_getlen(m);

    mu_t d = mu_buf_create(slen);
    muint_t n = 0;
    muint_t i = 0;

    while (i+mlen <= slen) {
        bool match = memcmp(&sb[i], mb, mlen) == 0;

        if (match) {
            mu_buf_concat(&d, &n, mu_str_inc(r));
            i += mlen;
        }

        if (!match || mlen == 0) {
            mu_buf_push(&d, &n, sb[i]);
            i += 1;
        }
    }

    mu_buf_append(&d, &n, &sb[i], slen-i);

    mu_str_dec(s);
    mu_str_dec(m);
    mu_str_dec(r);
    frame[0] = mu_str_intern(d, n);
    return 1;
}

MU_GEN_STR(mu_gen_key_replace, "replace")
MU_GEN_BFN(mu_gen_replace, 0x3, mu_bfn_replace)


static mcnt_t mu_str_split_step(mu_t scope, mu_t *frame) {
    mu_t a = mu_tbl_lookup(scope, mu_num_fromuint(0));
    const mbyte_t *ab = mu_str_getdata(a);
    mlen_t alen = mu_str_getlen(a);
    muint_t i = mu_num_getuint(mu_tbl_lookup(scope, mu_num_fromuint(2)));

    if (i > alen) {
        mu_str_dec(a);
        return 0;
    }

    mu_t s = mu_tbl_lookup(scope, mu_num_fromuint(1));
    const mbyte_t *sb = mu_str_getdata(s);
    mlen_t slen = mu_str_getlen(s);

    muint_t j = i;
    for (; j < alen; j++) {
        if (j+slen <= alen && memcmp(&ab[j], sb, slen) == 0) {
            break;
        }
    }

    frame[0] = mu_str_fromdata(ab+i, j-i);
    mu_tbl_insert(scope, mu_num_fromuint(2), mu_num_fromuint(j+slen));
    mu_str_dec(a);
    mu_str_dec(s);
    return 1;
}

static mcnt_t mu_bfn_split(mu_t *frame) {
    mu_t s     = frame[0];
    mu_t delim = frame[1] ? frame[1] : MU_SPACE_STR;
    if (!mu_isstr(s) || !mu_isstr(delim)) {
        mu_error_arg(MU_KEY_SPLIT, 0x2, frame);
    }

    if (mu_str_getlen(delim) == 0) {
        frame[0] = mu_str_iter(s);
        return 1;
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_str_split_step,
            mu_tbl_fromlist((mu_t[]){s, delim, mu_num_fromuint(0)}, 3));
    return 1;
}

MU_GEN_STR(mu_gen_key_split, "split")
MU_GEN_BFN(mu_gen_split, 0x2, mu_bfn_split)

static mcnt_t mu_bfn_join(mu_t *frame) {
    mu_t iter  = frame[0];
    mu_t delim = frame[1] ? frame[1] : MU_SPACE_STR;
    if (!mu_isstr(delim)) {
        mu_error_arg(MU_KEY_JOIN, 0x2, frame);
    }

    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    bool first = true;

    iter = mu_fn_call(MU_ITER, 0x11, iter);

    while (mu_fn_next(iter, 0x1, frame)) {
        if (!mu_isstr(frame[0])) {
            mu_errorf("invalid value %r passed to join", frame[0]);
        }

        if (first) {
            first = false;
        } else {
            mu_buf_concat(&b, &n, mu_str_inc(delim));
        }

        mu_buf_concat(&b, &n, frame[0]);
    }

    mu_fn_dec(iter);
    mu_str_dec(delim);
    frame[0] = mu_str_intern(b, n);
    return 1;
}

MU_GEN_STR(mu_gen_key_join, "join")
MU_GEN_BFN(mu_gen_join, 0x2, mu_bfn_join)

static mcnt_t mu_bfn_pad(mu_t *frame) {
    mu_t s    = frame[0];
    mu_t mlen = frame[1];
    mu_t pad  = frame[2] ? frame[2] : MU_SPACE_STR;
    if (!mu_isstr(s) ||
        !mu_isnum(mlen) ||
        (!mu_isstr(pad) || mu_str_getlen(pad) == 0)) {
        mu_error_arg(MU_KEY_PAD, 0x3, frame);
    }

    bool left;
    muint_t len;

    if (mu_num_cmp(mlen, mu_num_fromuint(0)) < 0) {
        left = false;
        len = mu_num_getuint(mu_num_neg(mlen));
    } else {
        left = true;
        len = mu_num_getuint(mlen);
    }

    if (mu_str_getlen(s) >= len) {
        mu_str_dec(pad);
        return 1;
    }

    mu_t d = mu_buf_create(len);
    muint_t n = 0;
    muint_t count = (len - mu_str_getlen(s)) / mu_str_getlen(pad);

    if (left) {
        mu_buf_concat(&d, &n, s);
    }

    for (muint_t i = 0; i < count; i++) {
        mu_buf_concat(&d, &n, mu_str_inc(pad));
    }

    if (!left) {
        mu_buf_concat(&d, &n, s);
    }

    mu_dec(pad);
    frame[0] = mu_str_intern(d, n);
    return 1;
}

MU_GEN_STR(mu_gen_key_pad, "pad")
MU_GEN_BFN(mu_gen_pad, 0x3, mu_bfn_pad)

static mcnt_t mu_bfn_strip(mu_t *frame) {
    if (mu_isstr(frame[1])) {
        mu_dec(frame[2]);
        frame[2] = frame[1];
        frame[1] = 0;
    }

    mu_t s   = frame[0];
    mu_t dir = frame[1];
    mu_t pad = frame[2] ? frame[2] : MU_SPACE_STR;
    if (!mu_isstr(s) ||
        (dir && !mu_isnum(dir)) ||
        (!mu_isstr(pad) || mu_str_getlen(pad) == 0)) {
        mu_error_arg(MU_KEY_STR, 0x3, frame);
    }

    const mbyte_t *pos = mu_str_getdata(s);
    const mbyte_t *end = pos + mu_str_getlen(s);

    const mbyte_t *pb = mu_str_getdata(pad);
    mlen_t plen = mu_str_getlen(pad);

    if (!dir || mu_num_cmp(dir, mu_num_fromuint(0)) <= 0) {
        while (end-pos >= plen && memcmp(pos, pb, plen) == 0) {
            pos += plen;
        }
    }

    if (!dir || mu_num_cmp(dir, mu_num_fromuint(0)) >= 0) {
        while (end-pos >= plen && memcmp(end-plen, pb, plen) == 0) {
            end -= plen;
        }
    }

    frame[0] = mu_str_fromdata(pos, end-pos);
    mu_str_dec(s);
    mu_str_dec(pad);
    return 1;
}

MU_GEN_STR(mu_gen_key_strip, "strip")
MU_GEN_BFN(mu_gen_strip, 0x3, mu_bfn_strip)
