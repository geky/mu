#include "str.h"
#include "mu.h"


#define MU_EMPTY_STR mu_empty_str()
#define MU_SPACE_STR mu_space_str()
MU_DEF_STR(mu_empty_str, "")
MU_DEF_STR(mu_space_str, " ")


// String access, useful for debugging
mu_inline struct mstr *mstr(mu_t s) {
    return (struct mstr *)((muint_t)s & ~7);
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
        mu_dec(b);
        return mu_inc(mu_str_table[i]);
    }

    if (mu_buf_getdtor(b)) {
        mu_buf_setdtor(&b, 0);
    }

    if (mu_buf_getlen(b) != n) {
        mu_buf_resize(&b, n);
    }

    mu_t s = (mu_t)((muint_t)b - MTBUF + MTSTR);
    mu_str_table_insert(~i, s);
    return mu_inc(s);
}

mu_t mu_str_fromdata(const void *s, muint_t n) {
    mu_checklen(n <= (mlen_t)-1, "string");

    mint_t i = mu_str_table_find(s, n);
    if (i >= 0) {
        return mu_inc(mu_str_table[i]);
    }

    // create new string and insert
    mu_t b = mu_buf_create(n);
    memcpy(mu_buf_getdata(b), s, n);

    mu_t ns = (mu_t)((muint_t)b - MTBUF + MTSTR);
    mu_str_table_insert(~i, ns);
    return mu_inc(ns);
}

void mu_str_destroy(mu_t s) {
    mint_t i = mu_str_table_find(mu_str_getdata(s), mu_str_getlen(s));
    mu_assert(i >= 0);
    mu_str_table_remove(i);

    mu_dealloc((struct mstr *)((muint_t)s - MTSTR),
            mu_offsetof(struct mstr, data) + mu_str_getlen(s));
}


// String creating functions
mu_t mu_str_init(const struct mstr *s) {
    mu_t m = mu_str_intern((mu_t)((muint_t)s + MTBUF), s->len);

    if (*(mref_t *)((muint_t)m - MTSTR) != 0) {
        *(mref_t *)((muint_t)m - MTSTR) = 0;
    }

    return m;
}

mu_t mu_str_frommu(mu_t m) {
    switch (mu_gettype(m)) {
        case MTNIL:
            return MU_EMPTY_STR;

        case MTSTR:
            return m;

        default:
            return mu_str_format("%nr", m, 0);
    }
}

mu_t mu_str_vformat(const char *f, va_list args) {
    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_buf_vpushf(&b, &n, f, args);
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

    mu_dec(b);
    return cmp != 0 ? cmp : alen - blen;
}


// String iteration
bool mu_str_next(mu_t s, muint_t *ip, mu_t *cp) {
    mu_assert(mu_isstr(s));
    muint_t i = *ip;

    if (i >= mu_str_getlen(s)) {
        return false;
    }

    if (cp) *cp = mu_str_fromdata((const mbyte_t*)mu_str_getdata(s) + i, 1);
    *ip = i + 1;
    return true;
}

static mcnt_t mu_str_step(mu_t scope, mu_t *frame) {
    mu_t s = mu_tbl_lookup(scope, mu_num_fromuint(0));
    muint_t i = mu_num_getuint(mu_tbl_lookup(scope, mu_num_fromuint(1)));

    bool next = mu_str_next(s, &i, &frame[0]);
    mu_dec(s);
    mu_tbl_insert(scope, mu_num_fromuint(1), mu_num_fromuint(i));
    return next ? 1 : 0;
}

mu_t mu_str_iter(mu_t s) {
    mu_assert(mu_isstr(s));
    return mu_fn_fromsbfn(0x00, mu_str_step,
            mu_tbl_fromlist((mu_t[]){mu_inc(s), mu_num_fromuint(0)}, 2));
}


// String operations
mu_t mu_str_concat(mu_t a, mu_t b) {
    mu_assert(mu_isstr(a) && mu_isstr(b));
    muint_t an = mu_str_getlen(a);
    muint_t bn = mu_str_getlen(b);
    mu_t d = mu_buf_create(an + bn);

    memcpy((mbyte_t *)mu_buf_getdata(d), mu_str_getdata(a), an);
    memcpy((mbyte_t *)mu_buf_getdata(d)+an, mu_str_getdata(b), bn);

    mu_dec(b);
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

    return mu_str_fromdata(
            (const mbyte_t *)mu_str_getdata(s) + lower,
            upper - lower);
}


// String representation
static muint_t mu_str_fromascii(mbyte_t c) {
    c |= ('a' ^ 'A');
    return
        (c >= '0' && c <= '9') ? c - '0':
        (c >= 'a' && c <= 'F') ? c - 'A' + 10 : -1;
}

mu_t mu_str_parsen(const mbyte_t **ppos, const mbyte_t *end) {
    const mbyte_t *pos = *ppos;
    mbyte_t quote = *pos++;
    mu_t b = mu_buf_create(0);
    muint_t n = 0;

    if (quote != '\'' && quote != '"') {
        mu_dec(b);
        return 0;
    }

    while (pos < end-1 && *pos != quote) {
        if (*pos == '\\') {
            if (pos[1] == 'b' && pos < end-9 &&
                (mu_str_fromascii(pos[2]) < 2 &&
                 mu_str_fromascii(pos[3]) < 2 &&
                 mu_str_fromascii(pos[4]) < 2 &&
                 mu_str_fromascii(pos[5]) < 2 &&
                 mu_str_fromascii(pos[6]) < 2 &&
                 mu_str_fromascii(pos[7]) < 2 &&
                 mu_str_fromascii(pos[8]) < 2 &&
                 mu_str_fromascii(pos[9]) < 2)) {
                mu_buf_pushchr(&b, &n,
                        mu_str_fromascii(pos[2])*2*2*2*2*2*2*2 +
                        mu_str_fromascii(pos[3])*2*2*2*2*2*2 +
                        mu_str_fromascii(pos[4])*2*2*2*2*2 +
                        mu_str_fromascii(pos[5])*2*2*2*2 +
                        mu_str_fromascii(pos[6])*2*2*2 +
                        mu_str_fromascii(pos[7])*2*2 +
                        mu_str_fromascii(pos[8])*2 +
                        mu_str_fromascii(pos[9]));
                pos += 10;
            } else if (pos[1] == 'o' && pos < end-4 &&
                (mu_str_fromascii(pos[2]) < 8 &&
                 mu_str_fromascii(pos[3]) < 8 &&
                 mu_str_fromascii(pos[4]) < 8)) {
                mu_buf_pushchr(&b, &n,
                        mu_str_fromascii(pos[2])*8*8 +
                        mu_str_fromascii(pos[3])*8 +
                        mu_str_fromascii(pos[4]));
                pos += 5;
            } else if (pos[1] == 'd' && pos < end-4 &&
                       (mu_str_fromascii(pos[2]) < 10 &&
                        mu_str_fromascii(pos[3]) < 10 &&
                        mu_str_fromascii(pos[4]) < 10)) {
                mu_buf_pushchr(&b, &n,
                        mu_str_fromascii(pos[2])*10*10 +
                        mu_str_fromascii(pos[3])*10 +
                        mu_str_fromascii(pos[4]));
                pos += 5;
            } else if (pos[1] == 'x' && pos < end-3 &&
                       (mu_str_fromascii(pos[2]) < 16 &&
                        mu_str_fromascii(pos[3]) < 16)) {
                mu_buf_pushchr(&b, &n,
                        mu_str_fromascii(pos[2])*16 +
                        mu_str_fromascii(pos[3]));
                pos += 4;
            } else if (pos[1] == '\\') {
                mu_buf_pushchr(&b, &n, '\\'); pos += 2;
            } else if (pos[1] == '\'') {
                mu_buf_pushchr(&b, &n, '\''); pos += 2;
            } else if (pos[1] == '\"') {
                mu_buf_pushchr(&b, &n, '\"'); pos += 2;
            } else if (pos[1] == 'f') {
                mu_buf_pushchr(&b, &n, '\f'); pos += 2;
            } else if (pos[1] == 'n') {
                mu_buf_pushchr(&b, &n, '\n'); pos += 2;
            } else if (pos[1] == 'r') {
                mu_buf_pushchr(&b, &n, '\r'); pos += 2;
            } else if (pos[1] == 't') {
                mu_buf_pushchr(&b, &n, '\t'); pos += 2;
            } else if (pos[1] == 'v') {
                mu_buf_pushchr(&b, &n, '\v'); pos += 2;
            } else if (pos[1] == '0') {
                mu_buf_pushchr(&b, &n, '\0'); pos += 2;
            } else {
                mu_buf_pushchr(&b, &n, '\\'); pos += 1;
            }
        } else {
            mu_buf_pushchr(&b, &n, *pos++);
        }
    }

    if (quote != *pos++) {
        mu_dec(b);
        return 0;
    }

    *ppos = pos;
    return mu_str_intern(b, n);
}

mu_t mu_str_parse(const char *s, muint_t n) {
    const mbyte_t *pos = (const mbyte_t *)s;
    const mbyte_t *end = (const mbyte_t *)pos + n;

    mu_t m = mu_str_parsen(&pos, end);

    if (pos != end) {
        mu_dec(m);
        return 0;
    }

    return m;
}

// Returns a string representation of a string
mu_t mu_str_repr(mu_t m) {
    mu_assert(mu_isstr(m));
    const mbyte_t *pos = mu_str_getdata(m);
    const mbyte_t *end = pos + mu_str_getlen(m);
    mu_t b = mu_buf_create(2 + mu_str_getlen(m));
    muint_t n = 0;

    mu_buf_pushchr(&b, &n, '\'');

    for (; pos < end; pos++) {
        if (*pos < ' ' || *pos > '~' ||
            *pos == '\\' || *pos == '\'') {
            if (*pos == '\\')      mu_buf_pushf(&b, &n, "\\\\");
            else if (*pos == '\'') mu_buf_pushf(&b, &n, "\\'");
            else if (*pos == '\f') mu_buf_pushf(&b, &n, "\\f");
            else if (*pos == '\n') mu_buf_pushf(&b, &n, "\\n");
            else if (*pos == '\r') mu_buf_pushf(&b, &n, "\\r");
            else if (*pos == '\t') mu_buf_pushf(&b, &n, "\\t");
            else if (*pos == '\v') mu_buf_pushf(&b, &n, "\\v");
            else if (*pos == '\0') mu_buf_pushf(&b, &n, "\\0");
            else                   mu_buf_pushf(&b, &n, "\\x%bx", *pos);
        } else {
            mu_buf_pushchr(&b, &n, *pos);
        }
    }

    mu_buf_pushchr(&b, &n, '\'');
    return mu_str_intern(b, n);
}


// String related functions in Mu
static mcnt_t mu_str_bfn(mu_t *frame) {
    mu_t m = mu_str_frommu(mu_inc(frame[0]));
    mu_checkargs(m, MU_STR_KEY, 0x1, frame);
    mu_dec(frame[0]);
    frame[0] = m;
    return 1;
}

MU_DEF_STR(mu_str_key_def, "str")
MU_DEF_BFN(mu_str_def, 0x1, mu_str_bfn)

static mcnt_t mu_find_bfn(mu_t *frame) {
    mu_t s = frame[0];
    mu_t m = frame[1];
    mu_checkargs(mu_isstr(s) && mu_isstr(m),
            MU_FIND_KEY, 0x2, frame);

    const mbyte_t *sb = mu_str_getdata(s);
    mlen_t slen = mu_str_getlen(s);
    const mbyte_t *mb = mu_str_getdata(m);
    mlen_t mlen = mu_str_getlen(m);

    for (muint_t i = 0; i+mlen <= slen; i++) {
        if (memcmp(&sb[i], mb, mlen) == 0) {
            mu_dec(m);
            mu_dec(s);
            frame[0] = mu_num_fromuint(i);
            frame[1] = mu_num_fromuint(i + mlen);
            return 2;
        }
    }

    mu_dec(s);
    mu_dec(m);
    return 0;
}

MU_DEF_STR(mu_find_key_def, "find")
MU_DEF_BFN(mu_find_def, 0x2, mu_find_bfn)

static mcnt_t mu_replace_bfn(mu_t *frame) {
    mu_t s = frame[0];
    mu_t m = frame[1];
    mu_t r = frame[2];
    mu_checkargs(mu_isstr(s) && mu_isstr(m) && mu_isstr(r),
            MU_REPLACE, 0x3, frame);

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
            mu_buf_pushmu(&d, &n, mu_inc(r));
            i += mlen;
        }

        if (!match || mlen == 0) {
            mu_buf_pushchr(&d, &n, sb[i]);
            i += 1;
        }
    }

    mu_buf_pushdata(&d, &n, &sb[i], slen-i);

    mu_dec(s);
    mu_dec(m);
    mu_dec(r);
    frame[0] = mu_str_intern(d, n);
    return 1;
}

MU_DEF_STR(mu_replace_key_def, "replace")
MU_DEF_BFN(mu_replace_def, 0x3, mu_replace_bfn)


static mcnt_t mu_str_split_step(mu_t scope, mu_t *frame) {
    mu_t a = mu_tbl_lookup(scope, mu_num_fromuint(0));
    const mbyte_t *ab = mu_str_getdata(a);
    mlen_t alen = mu_str_getlen(a);
    muint_t i = mu_num_getuint(mu_tbl_lookup(scope, mu_num_fromuint(2)));

    if (i > alen) {
        mu_dec(a);
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
    mu_dec(a);
    mu_dec(s);
    return 1;
}

static mcnt_t mu_split_bfn(mu_t *frame) {
    mu_t s     = frame[0];
    mu_t delim = frame[1] ? frame[1] : MU_EMPTY_STR;
    mu_checkargs(mu_isstr(s) && mu_isstr(delim), MU_SPLIT_KEY, 0x2, frame);

    if (mu_str_getlen(delim) == 0) {
        frame[0] = mu_str_iter(s);
        mu_dec(s);
        return 1;
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_str_split_step,
            mu_tbl_fromlist((mu_t[]){s, delim, mu_num_fromuint(0)}, 3));
    return 1;
}

MU_DEF_STR(mu_split_key_def, "split")
MU_DEF_BFN(mu_split_def, 0x2, mu_split_bfn)

static mcnt_t mu_join_bfn(mu_t *frame) {
    mu_t iter  = frame[0];
    mu_t delim = frame[1] ? frame[1] : MU_EMPTY_STR;
    mu_checkargs(mu_isstr(delim), MU_JOIN_KEY, 0x2, frame);

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
            mu_buf_pushmu(&b, &n, mu_inc(delim));
        }

        mu_buf_pushmu(&b, &n, frame[0]);
    }

    mu_dec(iter);
    mu_dec(delim);
    frame[0] = mu_str_intern(b, n);
    return 1;
}

MU_DEF_STR(mu_join_key_def, "join")
MU_DEF_BFN(mu_join_def, 0x2, mu_join_bfn)

static mcnt_t mu_pad_bfn(mu_t *frame) {
    mu_t s    = frame[0];
    mu_t mlen = frame[1];
    mu_t pad  = frame[2] ? frame[2] : MU_SPACE_STR;
    mu_checkargs(
            mu_isstr(s) && mu_isnum(mlen) &&
            mu_isstr(pad) && mu_str_getlen(pad) > 0,
            MU_PAD_KEY, 0x3, frame);

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
        mu_dec(pad);
        return 1;
    }

    mu_t d = mu_buf_create(len);
    muint_t n = 0;
    muint_t count = (len - mu_str_getlen(s)) / mu_str_getlen(pad);

    if (left) {
        mu_buf_pushmu(&d, &n, s);
    }

    for (muint_t i = 0; i < count; i++) {
        mu_buf_pushmu(&d, &n, mu_inc(pad));
    }

    if (!left) {
        mu_buf_pushmu(&d, &n, s);
    }

    mu_dec(pad);
    frame[0] = mu_str_intern(d, n);
    return 1;
}

MU_DEF_STR(mu_pad_key_def, "pad")
MU_DEF_BFN(mu_pad_def, 0x3, mu_pad_bfn)

static mcnt_t mu_strip_bfn(mu_t *frame) {
    if (mu_isstr(frame[1])) {
        mu_dec(frame[2]);
        frame[2] = frame[1];
        frame[1] = 0;
    }

    mu_t s   = frame[0];
    mu_t dir = frame[1];
    mu_t pad = frame[2] ? frame[2] : MU_SPACE_STR;
    mu_checkargs(
            mu_isstr(s) && (!dir || mu_isnum(dir)) &&
            mu_isstr(pad) && mu_str_getlen(pad) > 0,
            MU_STR_KEY, 0x3, frame);

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
    mu_dec(s);
    mu_dec(pad);
    return 1;
}

MU_DEF_STR(mu_strip_key_def, "strip")
MU_DEF_BFN(mu_strip_def, 0x3, mu_strip_bfn)
