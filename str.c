#include "str.h"

#include "num.h"
#include "tbl.h"
#include "fn.h"
#include "err.h"
#include "parse.h"
#include <string.h>


// Internally used conversion between mu_t and struct str
mu_inline mu_t tostr(struct str *s) {
    return (mu_t)((muint_t)s + MU_STR);
}

mu_inline struct str *fromstr(mu_t m) {
    return (struct str *)((muint_t)m - MU_STR);
}

// Conversion from mstr's exposed pointer that gets passed around
mu_inline struct str *frommstr(mbyte_t *b) {
    return (struct str *)(b - mu_offset(struct str, data));
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
static struct str **str_table = 0;
static muint_t str_table_size = 0;
static muint_t str_table_len = 0;

static mint_t str_table_find(const mbyte_t *s, mlen_t len) {
    mint_t min = 0;
    mint_t max = str_table_len-1;

    // binary search for existing strings
    // strings are sorted first by length to avoid comparisons
    while (min <= max) {
        mint_t mid = (max + min) / 2;
        mint_t cmp = len > str_table[mid]->len ? +1 :
                     len < str_table[mid]->len ? -1 :
                     memcmp(s, str_table[mid]->data, len);

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

static void str_table_insert(mint_t i, struct str *s) {
    // expand the table if necessary
    if (str_table_len == str_table_size) {
        muint_t nsize;
        struct str **ntable;

        if (str_table_size == 0)
            nsize = MU_MINALLOC / sizeof(struct str *);
        else
            nsize = str_table_size << 1;

        ntable = mu_alloc(nsize * sizeof(struct str *));
        memcpy(ntable, str_table,
               i * sizeof(struct str *));
        memcpy(&ntable[i+1], &str_table[i],
               (str_table_len-i) * sizeof(struct str *));
        mu_dealloc(str_table, str_table_size);

        str_table = ntable;
        str_table_size = nsize;
    } else {
        memmove(&str_table[i+1], &str_table[i],
                (str_table_len-i) * sizeof(struct str *));
    }

    str_table[i] = s;
    str_table_len += 1;
}

static void str_table_remove(mint_t i) {
    str_table_len -= 1;
    memmove(&str_table[i], &str_table[i+1],
            (str_table_len-i) * sizeof(struct str *));
}


// String management
static mu_t str_intern(const mbyte_t *s, muint_t len) {
    mu_check_len(len);

    mint_t i = str_table_find(s, len);
    if (i >= 0)
        return str_inc(tostr(str_table[i]));

    // create new string and insert
    struct str *ns = ref_alloc(mu_offset(struct str, data) + len);
    memcpy(ns->data, s, len);
    ns->len = len;

    str_table_insert(~i, ns);
    return str_inc(tostr(ns));
}

void str_destroy(mu_t m) {
    mint_t i = str_table_find(str_bytes(m), str_len(m));

    if (i >= 0)
        str_table_remove(i);

    ref_dealloc(m, mu_offset(struct str, data) + str_len(m));
}

// String creating functions
mu_t mzstr(const char *s) {
    return str_intern((const mbyte_t *)s, strlen(s));
}

mu_t mnstr(const mbyte_t *s, muint_t len) {
    return str_intern(s, len);
}


// Functions for creating mutable temporary strings
// these can avoid unnecessary allocations when interning
// since mstr_intern can reuse their internal structure
mbyte_t *mstr_create(muint_t len) {
    mu_check_len(len);

    struct str *s = ref_alloc(mu_offset(struct str, data) + len);
    s->len = len;
    return s->data;
}

void mstr_destroy(mbyte_t *b) {
    struct str *s = frommstr(b);
    ref_dealloc(s, mu_offset(struct str, data) + s->len);
}

mu_t mstr_intern(mbyte_t *b, muint_t len) {
    mu_check_len(len);

    // unfortunately, reusing the mstr struct only
    // works with exact length currently
    if (frommstr(b)->len != len) {
        mu_t m = str_intern(b, len);
        mstr_dec(b);
        return m;
    }

    mint_t i = str_table_find(b, len);
    if (i >= 0) {
        mstr_dec(b);
        return str_inc(tostr(str_table[i]));
    }

    struct str *s = frommstr(b);
    str_table_insert(~i, s);
    return str_inc(tostr(s));
}

// Functions to modify mutable strings
void mstr_insert(mbyte_t **b, muint_t *i, mbyte_t c) {
    mstr_ncat(b, i, &c, 1);
}

void mstr_concat(mbyte_t **b, muint_t *i, mu_t c) {
    mstr_ncat(b, i, str_bytes(c), str_len(c));
    mu_dec(c);
}

void mstr_zcat(mbyte_t **b, muint_t *i, const char *c) {
    mstr_ncat(b, i, (mbyte_t *)c, strlen(c));
}

void mstr_ncat(mbyte_t **b, muint_t *i, const mbyte_t *c, muint_t len) {
    muint_t size = frommstr(*b)->len;
    muint_t nsize = *i + len;

    if (size < nsize) {
        size += mu_offset(struct str, data);

        if (size < MU_MINALLOC)
            size = MU_MINALLOC;

        while (size < nsize + mu_offset(struct str, data))
            size <<= 1;

        size -= mu_offset(struct str, data);
        mu_check_len(size);

        mbyte_t *nb = mstr_create(size);
        memcpy(nb, *b, frommstr(*b)->len);
        mstr_dec(*b);
        *b = nb;
    }

    memcpy(&(*b)[*i], c, len);
    *i += len;
}


// Returns a string representation of a string
mu_t str_repr(mu_t m) {
    const mbyte_t *pos = str_bytes(m);
    const mbyte_t *end = pos + str_len(m);
    mbyte_t *s = mstr_create(2);
    muint_t len = 0;

    mstr_insert(&s, &len, '\'');

    for (; pos < end; pos++) {
        if (*pos < ' ' || *pos > '~' ||
            *pos == '\\' || *pos == '\'') {
            if (*pos == '\\') mstr_zcat(&s, &len, "\\\\");
            else if (*pos == '\'') mstr_zcat(&s, &len, "\\'");
            else if (*pos == '\a') mstr_zcat(&s, &len, "\\a");
            else if (*pos == '\b') mstr_zcat(&s, &len, "\\b");
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

    mu_dec(m);
    mstr_insert(&s, &len, '\'');
    return mstr_intern(s, len);
}


// String iteration
bool str_next(mu_t s, muint_t *ip, mu_t *cp) {
    muint_t i = *ip;

    if (i >= str_len(s)) {
        *cp = mnil;
        return false;
    }

    *cp = mnstr(&str_bytes(s)[i], 1);
    *ip = i + 1;
    return true;
}

mc_t str_step(mu_t scope, mu_t *frame) {
    mu_t s = tbl_lookup(scope, muint(0));
    muint_t i = num_uint(tbl_lookup(scope, muint(1)));

    str_next(s, &i, &frame[0]);
    tbl_insert(scope, muint(1), muint(i));
    return 1;
}

mu_t str_iter(mu_t s) {
    return msbfn(0x00, str_step, mtbl({
        { muint(0), s },
        { muint(1), muint(0) }
    }));
}

