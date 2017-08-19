/*
 * The Mu scripting language
 *
 * Copyright (c) 2016 Christopher Haster
 * Distributed under the MIT license in mu.h
 */
#include "mu.h"


// Constants
MU_DEF_STR(mu_true_key_def,  "true")
MU_DEF_STR(mu_false_key_def, "false")

MU_DEF_UINT(mu_true_def, 1)


// Uses malloc/free if MU_MALLOC is defined
#ifdef MU_MALLOC
extern void *malloc(muint_t);
extern void free(void *);
#define mu_sys_alloc(size) malloc(size)
#define mu_sys_dealloc(m, size) free(m)
#endif

// Manual memory management
// Currently just a wrapper over malloc and free
// Garuntees 8 byte alignment
void *mu_alloc(muint_t size) {
    if (size == 0) {
        return 0;
    }

#ifdef MU_DEBUG
    size += sizeof(muint_t);
#endif

    void *m = mu_sys_alloc(size);

    if (m == 0) {
        const char *message = "out of memory";
        mu_error(message, strlen(message));
    }

    mu_assert(sizeof m == sizeof(muint_t)); // garuntee address width
    mu_assert((7 & (muint_t)m) == 0); // garuntee alignment

#ifdef MU_DEBUG
    size -= sizeof(muint_t);
    *(muint_t*)&((char*)m)[size] = size;
#endif

    return m;
}

void mu_dealloc(void *m, muint_t size) {
#ifdef MU_DEBUG
    mu_assert(!m || *(muint_t*)&((char*)m)[size] == size);
#endif

    mu_sys_dealloc(m, size);
}


// System operations
mu_noreturn mu_error(const char *s, muint_t n) {
    mu_sys_error(s, n);
    mu_unreachable;
}

mu_noreturn mu_verrorf(const char *f, va_list args) {
    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_buf_vpushf(&b, &n, f, args);
    mu_error(mu_buf_getdata(b), n);
}

mu_noreturn mu_errorf(const char *f, ...) {
    va_list args;
    va_start(args, f);
    mu_verrorf(f, args);
}

static mcnt_t mu_error_bfn(mu_t *frame) {
    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_t v;

    for (muint_t i = 0; mu_tbl_next(frame[0], &i, 0, &v);) {
        mu_buf_pushf(&b, &n, "%m", v);
    }

    mu_error(mu_buf_getdata(b), n);
}

MU_DEF_STR(mu_error_key_def, "error")
MU_DEF_BFN(mu_error_def, 0xf, mu_error_bfn)

void mu_print(const char *s, muint_t n) {
    mu_sys_print(s, n);
}

void mu_vprintf(const char *f, va_list args) {
    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_buf_vpushf(&b, &n, f, args);
    mu_print(mu_buf_getdata(b), n);
    mu_dec(b);
}

void mu_printf(const char *f, ...) {
    va_list args;
    va_start(args, f);
    mu_vprintf(f, args);
    va_end(args);
}

static mcnt_t mu_print_bfn(mu_t *frame) {
    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_t v;

    for (muint_t i = 0; mu_tbl_next(frame[0], &i, 0, &v);) {
        mu_buf_pushf(&b, &n, "%m", v);
    }

    mu_print(mu_buf_getdata(b), n);
    mu_dec(b);
    mu_dec(frame[0]);
    return 0;
}

MU_DEF_STR(mu_print_key_def, "print")
MU_DEF_BFN(mu_print_def, 0xf, mu_print_bfn)

static mcnt_t mu_import_bfn(mu_t *frame) {
    mu_t name = frame[0];
    mu_checkargs(mu_isstr(name), MU_IMPORT_KEY, 0x1, frame);

    static mu_t import_history = 0;
    if (!import_history) {
        import_history = mu_tbl_create(0);
    }

    mu_t module = mu_tbl_lookup(import_history, mu_inc(name));
    if (module) {
        mu_dec(name);
        frame[0] = module;
        return 1;
    }

    module = mu_sys_import(mu_inc(name));
    mu_tbl_insert(import_history, name, mu_inc(module));
    frame[0] = module;
    return 1;
}

MU_DEF_STR(mu_import_key_def, "import")
MU_DEF_BFN(mu_import_def, 0x1, mu_import_bfn)


// Evaluation and entry into Mu
void mu_feval(const char *s, muint_t n, mu_t scope, mcnt_t fc, mu_t *frame) {
    mu_t c = mu_compile(s, n, mu_inc(scope));
    mcnt_t rets = mu_exec(c, mu_inc(scope), frame);
    mu_frameconvert(rets, fc, frame);
}

mu_t mu_veval(const char *s, muint_t n, mu_t scope, mcnt_t fc, va_list args) {
    mu_t frame[MU_FRAME];

    mu_feval(s, n, scope, fc, frame);

    for (muint_t i = 1; i < mu_framecount(fc); i++) {
        *va_arg(args, mu_t *) = frame[i];
    }

    return fc ? *frame : 0;
}

mu_t mu_eval(const char *s, muint_t n, mu_t scope, mcnt_t fc, ...) {
    va_list args;
    va_start(args, fc);
    mu_t ret = mu_veval(s, n, scope, fc, args);
    va_end(args);
    return ret;
}


// Common errors
mu_noreturn mu_errorargs(mu_t name, mcnt_t fc, mu_t *frame) {
    char c = *(char*)mu_str_getdata(name);
    bool isop = !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
    if (isop && fc == 1) {
        mu_errorf("invalid operation %m%r", name, frame[0]);
    } else if (isop && fc == 2) {
        mu_errorf("invalid operation %r %m %r", frame[0], name, frame[1]);
    } else {
        mu_t message = mu_buf_create(0);
        muint_t n = 0;
        mu_buf_pushf(&message, &n, "invalid argument in %m(", name);

        if (fc == 0xf) {
            mu_buf_pushf(&message, &n, "..");
            fc = 1;
        }

        for (muint_t i = 0; i < fc; i++) {
            mu_buf_pushf(&message, &n, "%nr%c ",
                    frame[i], 0, (i != fc-1) ? ',' : ')');
        }

        mu_error(mu_buf_getdata(message), n);
    }
}

mu_noreturn mu_errorro(const char *name) {
    mu_errorf("attempted to modify read-only %s", name);
}

mu_noreturn mu_errorlen(const char *name) {
    mu_errorf("exceeded maximum length in %s", name);
}


// wrappers for comparison operations
static mint_t (*const mu_attr_cmp[8])(mu_t, mu_t) = {
    [MTNUM] = mu_num_cmp,
    [MTSTR] = mu_str_cmp,
};

static mcnt_t mu_not_bfn(mu_t *frame) {
    mu_dec(frame[0]);
    frame[0] = !frame[0] ? MU_TRUE : MU_FALSE;
    return 1;
}

MU_DEF_STR(mu_not_key_def, "!")
MU_DEF_BFN(mu_not_def, 0x1, mu_not_bfn)

static mcnt_t mu_eq_bfn(mu_t *frame) {
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = (frame[0] == frame[1]) ? MU_TRUE : MU_FALSE;
    return 1;
}

MU_DEF_STR(mu_eq_key_def, "==")
MU_DEF_BFN(mu_eq_def,  0x2, mu_eq_bfn)

static mcnt_t mu_neq_bfn(mu_t *frame) {
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = (frame[0] != frame[1]) ? MU_TRUE : MU_FALSE;
    return 1;
}

MU_DEF_STR(mu_neq_key_def, "!=")
MU_DEF_BFN(mu_neq_def, 0x2, mu_neq_bfn)

static mcnt_t mu_lt_bfn(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    mu_checkargs(
            mu_gettype(a) == mu_gettype(b) &&
            mu_attr_cmp[mu_gettype(a)],
            MU_LT_KEY, 0x2, frame);

    frame[0] = (mu_attr_cmp[mu_gettype(a)](a, b) < 0) ? MU_TRUE : MU_FALSE;
    mu_dec(a);
    return 1;
}

MU_DEF_STR(mu_lt_key_def, "<")
MU_DEF_BFN(mu_lt_def,  0x2, mu_lt_bfn)

static mcnt_t mu_lte_bfn(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    mu_checkargs(
            mu_gettype(a) == mu_gettype(b) &&
            mu_attr_cmp[mu_gettype(a)],
            MU_LTE_KEY, 0x2, frame);

    frame[0] = (mu_attr_cmp[mu_gettype(a)](a, b) <= 0) ? MU_TRUE : MU_FALSE;
    mu_dec(a);
    return 1;
}

MU_DEF_STR(mu_lte_key_def, "<=")
MU_DEF_BFN(mu_lte_def, 0x2, mu_lte_bfn)

static mcnt_t mu_gt_bfn(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    mu_checkargs(
            mu_gettype(a) == mu_gettype(b) &&
            mu_attr_cmp[mu_gettype(a)],
            MU_GT_KEY, 0x2, frame);

    frame[0] = (mu_attr_cmp[mu_gettype(a)](a, b) > 0) ? MU_TRUE : MU_FALSE;
    mu_dec(a);
    return 1;
}

MU_DEF_STR(mu_gt_key_def, ">")
MU_DEF_BFN(mu_gt_def,  0x2, mu_gt_bfn)

static mcnt_t mu_gte_bfn(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    mu_checkargs(
            mu_gettype(a) == mu_gettype(b) &&
            mu_attr_cmp[mu_gettype(a)],
            MU_GTE_KEY, 0x2, frame);

    frame[0] = (mu_attr_cmp[mu_gettype(a)](a, b) >= 0) ? MU_TRUE : MU_FALSE;
    mu_dec(a);
    return 1;
}

MU_DEF_STR(mu_gte_key_def, ">=")
MU_DEF_BFN(mu_gte_def, 0x2, mu_gte_bfn)

static mcnt_t mu_is_bfn(mu_t *frame) {
    mu_t m    = frame[0];
    mu_t type = frame[1];
    frame[0] = MU_TRUE;
    mu_dec(type);

    if (mu_istbl(type)) {
        while (m) {
            mu_t tail;
            if (mu_istbl(m)) {
                tail = mu_tbl_gettail(m);
            } else if (mu_isbuf(m)) {
                tail = mu_buf_gettail(m);
            } else {
                tail = 0;
            }
            mu_dec(m);

            if (tail == type) {
                mu_dec(tail);
                return true;
            }

            m = tail;
        }

        return false;
    }

    mu_dec(m);
    switch (mu_gettype(m)) {
        case MTNIL:  return !type;
        case MTNUM:  return type == MU_NUM;
        case MTSTR:  return type == MU_STR;
        case MTTBL: 
        case MTRTBL: return type == MU_TBL;
        case MTFN:   return type == MU_FN;
        default:     return false;
    }
}

MU_DEF_STR(mu_is_key_def, "is")
MU_DEF_BFN(mu_is_def, 0x2, mu_is_bfn)


// String representation
static mcnt_t mu_parse_bfn(mu_t *frame) {
    mu_t s = frame[0];
    mu_checkargs(mu_isstr(s), MU_PARSE_KEY, 0x1, frame);

    frame[0] = mu_parse(mu_str_getdata(s), mu_str_getlen(s));
    mu_dec(s);
    return 1;
}

MU_DEF_STR(mu_parse_key_def, "parse")
MU_DEF_BFN(mu_parse_def, 0x1, mu_parse_bfn)

static mcnt_t mu_repr_bfn(mu_t *frame) {
    mu_t m     = frame[0];
    mu_t depth = frame[1];
    mu_checkargs(!depth || mu_isnum(depth), MU_REPR_KEY, 0x2, frame);

    frame[0] = mu_repr(m, depth);
    return 1;
}

MU_DEF_STR(mu_repr_key_def, "repr")
MU_DEF_BFN(mu_repr_def, 0x2, mu_repr_bfn)

static mcnt_t mu_ord_bfn(mu_t *frame) {
    mu_t m = frame[0];
    mu_checkargs(mu_isstr(m) && mu_str_getlen(m) == 1,
            MU_ORD_KEY, 0x1, frame);

    frame[0] = mu_num_fromuint(*(const mbyte_t *)mu_str_getdata(m));
    mu_dec(m);
    return 1;
}

MU_DEF_STR(mu_ord_key_def, "ord")
MU_DEF_BFN(mu_ord_def, 0x1, mu_ord_bfn)

static mcnt_t mu_chr_bfn(mu_t *frame) {
    mu_t m = frame[0];
    mu_checkargs(m == mu_num_fromuint((mbyte_t)mu_num_getuint(m)),
            MU_CHR_KEY, 0x1, frame);

    frame[0] = mu_str_fromdata((mbyte_t[]){mu_num_getuint(m)}, 1);
    return 1;
}

MU_DEF_STR(mu_chr_key_def, "chr")
MU_DEF_BFN(mu_chr_def, 0x1, mu_chr_bfn)

static mcnt_t mu_bin_bfn(mu_t *frame) {
    mu_t m = frame[0];
    mu_checkargs(mu_isnum(m), MU_BIN_KEY, 0x1, frame);

    frame[0] = mu_num_bin(m);
    return 1;
}

MU_DEF_STR(mu_bin_key_def, "bin")
MU_DEF_BFN(mu_bin_def, 0x1, mu_bin_bfn)

static mcnt_t mu_oct_bfn(mu_t *frame) {
    mu_t m = frame[0];
    mu_checkargs(mu_isnum(m), MU_OCT_KEY, 0x1, frame);

    frame[0] = mu_num_oct(m);
    return 1;
}

MU_DEF_STR(mu_oct_key_def, "oct")
MU_DEF_BFN(mu_oct_def, 0x1, mu_oct_bfn)

static mcnt_t mu_hex_bfn(mu_t *frame) {
    mu_t m = frame[0];
    mu_checkargs(mu_isnum(m), MU_HEX_KEY, 0x1, frame);

    frame[0] = mu_num_hex(m);
    return 1;
}

MU_DEF_STR(mu_hex_key_def, "hex")
MU_DEF_BFN(mu_hex_def, 0x1, mu_hex_bfn)


// Data structure operations
static mcnt_t mu_len_bfn(mu_t *frame) {
    mu_t a = frame[0];
    mu_checkargs(mu_isstr(a) || mu_istbl(a),
            MU_LEN_KEY, 0x1, frame);

    if (mu_isstr(a)) {
        frame[0] = mu_num_fromuint(mu_str_getlen(a));
        mu_dec(a);
        return 1;
    } else if (mu_istbl(a)) {
        frame[0] = mu_num_fromuint(mu_tbl_getlen(a));
        mu_dec(a);
        return 1;
    }

    mu_unreachable;
}

MU_DEF_STR(mu_len_key_def, "len")
MU_DEF_BFN(mu_len_def, 0x1, mu_len_bfn)

static mcnt_t mu_tail_bfn(mu_t *frame) {
    mu_t t = frame[0];
    mu_checkargs(mu_istbl(frame[0]) || mu_isbuf(frame[0]),
            MU_TAIL_KEY, 0x1, frame);

    if (mu_istbl(t)) {
        frame[0] = mu_tbl_gettail(t);
        mu_dec(t);
        return 1;
    } else if (mu_isbuf(t)) {
        frame[0] = mu_buf_gettail(t);
        mu_dec(t);
        return 1;
    }

    mu_unreachable;
}

MU_DEF_STR(mu_tail_key_def, "tail")
MU_DEF_BFN(mu_tail_def, 0x1, mu_tail_bfn)

static mcnt_t mu_const_bfn(mu_t *frame) {
    mu_t t = frame[0];
    if (mu_istbl(t)) {
        frame[0] = mu_tbl_const(t);
        mu_dec(t);
    }

    return 1;
}

MU_DEF_STR(mu_const_key_def, "const")
MU_DEF_BFN(mu_const_def, 0x1, mu_const_bfn)

static mcnt_t mu_concat_bfn(mu_t *frame) {
    mu_t a      = frame[0];
    mu_t b      = frame[1];
    mu_t offset = frame[2];
    mu_checkargs(
        ((mu_isstr(a) && mu_isstr(b)) ||
         (mu_istbl(a) && mu_istbl(b))) &&
        (!offset || mu_isnum(offset)),
        MU_CONCAT_KEY, 0x3, frame);

    if (mu_isstr(a) && mu_isstr(b)) {
        frame[0] = mu_str_concat(a, b);
        mu_dec(a);
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = mu_tbl_concat(a, b, offset);
        mu_dec(a);
        return 1;
    }

    mu_unreachable;
}

MU_DEF_STR(mu_concat_key_def, "++")
MU_DEF_BFN(mu_concat_def, 0x3, mu_concat_bfn)

static mcnt_t mu_subset_bfn(mu_t *frame) {
    mu_t a     = frame[0];
    mu_t lower = frame[1];
    mu_t upper = frame[2];
    mu_checkargs(
            (mu_isstr(a) || mu_istbl(a)) &&
            mu_isnum(lower) && (!upper ||mu_isnum(upper)),
            MU_SUBSET_KEY, 0x3, frame);

    mint_t loweri = mu_num_clampint(lower,
            -(mint_t)(mlen_t)-1, (mlen_t)-1);
    mint_t upperi;
    if (!upper) {
        upperi = loweri+1;
    } else {
        upperi = mu_num_clampint(upper,
                -(mint_t)(mlen_t)-1, (mlen_t)-1);
    }

    if (mu_isstr(a)) {
        frame[0] = mu_str_subset(a, loweri, upperi);
        mu_dec(a);
        return 1;
    } else if (mu_istbl(a)) {
        frame[0] = mu_tbl_subset(a, loweri, upperi);
        mu_dec(a);
        return 1;
    }

    mu_unreachable;
}

MU_DEF_STR(mu_subset_key_def, "sub")
MU_DEF_BFN(mu_subset_def, 0x3, mu_subset_bfn)

static mcnt_t mu_push_bfn(mu_t *frame) {
    mu_t t = frame[0];
    mu_t v = frame[1];
    mu_t i = frame[2];
    mu_checkargs(mu_istbl(t) && (!i || mu_isnum(i)),
            MU_PUSH_KEY, 0x3, frame);

    mint_t ii;
    if (!i) {
        ii = mu_tbl_getlen(t);
    } else {
        ii = mu_num_clampint(i,
                -(mint_t)(mlen_t)-1, (mlen_t)-1);
    }

    mu_tbl_push(t, v, ii);
    mu_dec(t);
    return 0;
}

MU_DEF_STR(mu_push_key_def, "push")
MU_DEF_BFN(mu_push_def, 0x3, mu_push_bfn)

static mcnt_t mu_pop_bfn(mu_t *frame) {
    mu_t t = frame[0];
    mu_t i = frame[1];
    mu_checkargs(mu_istbl(t) && (!i || mu_isnum(i)),
            MU_POP_KEY, 0x2, frame);

    mint_t ii;
    if (!i) {
        ii = mu_tbl_getlen(t) - 1;
    } else {
        ii = mu_num_clampint(i,
                -(mint_t)(mlen_t)-1, (mlen_t)-1);
    }

    frame[0] = mu_tbl_pop(t, ii);
    mu_dec(t);
    return 1;
}

MU_DEF_STR(mu_pop_key_def, "pop")
MU_DEF_BFN(mu_pop_def, 0x2, mu_pop_bfn)

static mcnt_t mu_and_bfn(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    mu_checkargs(
        (mu_isnum(a) && mu_isnum(b)) ||
        (mu_istbl(a) && mu_istbl(b)),
        MU_AND_KEY, 0x2, frame);

    if (mu_isnum(a) && mu_isnum(b)) {
        frame[0] = mu_num_and(a, b);
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = mu_tbl_and(a, b);
        mu_dec(a);
        return 1;
    }

    mu_unreachable;
}

MU_DEF_STR(mu_and_key_def, "&")
MU_DEF_BFN(mu_and_def, 0x2, mu_and_bfn)

static mcnt_t mu_or_bfn(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    mu_checkargs(
        (mu_isnum(a) && mu_isnum(b)) ||
        (mu_istbl(a) && mu_istbl(b)),
        MU_OR_KEY, 0x2, frame);

    if (mu_isnum(a) && mu_isnum(b)) {
        frame[0] = mu_num_or(a, b);
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = mu_tbl_or(a, b);
        mu_dec(a);
        return 1;
    }

    mu_unreachable;
}

MU_DEF_STR(mu_or_key_def, "|")
MU_DEF_BFN(mu_or_def, 0x2, mu_or_bfn)

static mcnt_t mu_xor_bfn(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    mu_checkargs(
        (mu_isnum(a) && mu_isnum(b)) ||
        (mu_istbl(a) && mu_istbl(b)),
        MU_XOR_KEY, 0x2, frame);

    if (mu_isnum(a) && !b) {
        frame[0] = mu_num_not(a);
        return 1;
    } else if (mu_isnum(a) && mu_isnum(b)) {
        frame[0] = mu_num_xor(a, b);
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = mu_tbl_xor(a, b);
        mu_dec(a);
        return 1;
    }

    mu_unreachable;
}

MU_DEF_STR(mu_xor_key_def, "~")
MU_DEF_BFN(mu_xor_def, 0x2, mu_xor_bfn)

static mcnt_t mu_diff_bfn(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    mu_checkargs(
        (mu_isnum(a) && mu_isnum(b)) ||
        (mu_istbl(a) && mu_istbl(b)),
        MU_DIFF_KEY, 0x2, frame);

    if (mu_isnum(a) && mu_isnum(b)) {
        frame[0] = mu_num_xor(a, mu_num_not(b));
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = mu_tbl_diff(a, b);
        mu_dec(a);
        return 1;
    }

    mu_unreachable;
}

MU_DEF_STR(mu_diff_key_def, "&~")
MU_DEF_BFN(mu_diff_def, 0x2, mu_diff_bfn)


// Iterators and deferators
static mu_t mu_fn_iter(mu_t m) { return mu_inc(m); }
static mu_t (*const mu_attr_iter[8])(mu_t) = {
    [MTSTR]  = mu_str_iter,
    [MTTBL]  = mu_tbl_iter,
    [MTRTBL] = mu_tbl_iter,
    [MTFN]   = mu_fn_iter,
};

static mcnt_t mu_iter_bfn(mu_t *frame) {
    mu_t m = frame[0];
    mu_checkargs(mu_attr_iter[mu_gettype(m)],
            MU_ITER_KEY, 0x1, frame);

    frame[0] = mu_attr_iter[mu_gettype(m)](m);
    mu_dec(m);
    return 1;
}

MU_DEF_STR(mu_iter_key_def, "iter")
MU_DEF_BFN(mu_iter_def, 0x1, mu_iter_bfn)

static mcnt_t mu_pairs_bfn(mu_t *frame) {
    mu_t m = frame[0];
    mu_checkargs(mu_istbl(m) || mu_attr_iter[mu_gettype(m)],
            MU_PAIRS_KEY, 0x1, frame);

    if (mu_istbl(m)) {
        frame[0] = mu_tbl_pairs(m);
        mu_dec(m);
        return 1;
    } else if (mu_attr_iter[mu_gettype(m)]) {
        mu_fn_fcall(MU_RANGE, 0x01, frame);
        frame[1] = mu_attr_iter[mu_gettype(m)](m);
        mu_dec(m);
        return mu_fn_tcall(MU_ZIP, 0x2, frame);
    }

    mu_unreachable;
}

MU_DEF_STR(mu_pairs_key_def, "pairs")
MU_DEF_BFN(mu_pairs_def, 0x1, mu_pairs_bfn)


// Functions over iterators
static mcnt_t mu_map_step_bfn(mu_t scope, mu_t *frame) {
    mu_t f = mu_tbl_lookup(scope, mu_num_fromuint(0));
    mu_t i = mu_tbl_lookup(scope, mu_num_fromuint(1));

    while (mu_fn_next(i, 0xf, frame)) {
        mu_fn_fcall(f, 0xff, frame);
        mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        if (m) {
            mu_dec(m);
            mu_dec(f);
            mu_dec(i);
            return 0xf;
        }
        mu_dec(frame[0]);
    }

    mu_dec(f);
    mu_dec(i);
    return 0;
}

static mcnt_t mu_map_bfn(mu_t *frame) {
    mu_t f    = frame[0];
    mu_t iter = frame[1];
    mu_checkargs(mu_isfn(f),
            MU_MAP_KEY, 0x2, frame);

    frame[0] = iter;
    mu_fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    frame[0] = mu_fn_fromsbfn(0, mu_map_step_bfn,
            mu_tbl_fromlist((mu_t[]){f, iter}, 2));
    return 1;
}

MU_DEF_STR(mu_map_key_def, "map")
MU_DEF_BFN(mu_map_def, 0x2, mu_map_bfn)

static mcnt_t mu_filter_step_bfn(mu_t scope, mu_t *frame) {
    mu_t f = mu_tbl_lookup(scope, mu_num_fromuint(0));
    mu_t i = mu_tbl_lookup(scope, mu_num_fromuint(1));

    while (mu_fn_next(i, 0xf, frame)) {
        mu_t m = mu_inc(frame[0]);
        mu_fn_fcall(f, 0xf1, frame);
        if (frame[0]) {
            mu_dec(frame[0]);
            frame[0] = m;
            mu_dec(f);
            mu_dec(i);
            return 0xf;
        }
        mu_dec(m);
    }

    mu_dec(f);
    mu_dec(i);
    return 0;
}

static mcnt_t mu_filter_bfn(mu_t *frame) {
    mu_t f    = frame[0];
    mu_t iter = frame[1];
    mu_checkargs(mu_isfn(f), MU_FILTER_KEY, 0x2, frame);

    frame[0] = iter;
    mu_fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    frame[0] = mu_fn_fromsbfn(0, mu_filter_step_bfn,
            mu_tbl_fromlist((mu_t[]){f, iter}, 2));
    return 1;
}

MU_DEF_STR(mu_filter_key_def, "filter")
MU_DEF_BFN(mu_filter_def, 0x2, mu_filter_bfn)

static mcnt_t mu_reduce_bfn(mu_t *frame) {
    mu_t f    = mu_tbl_pop(frame[0], 0);
    mu_t iter = mu_tbl_pop(frame[0], 0);
    mu_t acc  = frame[0];
    mu_checkargs(mu_isfn(f), MU_REDUCE_KEY, 0x3, (mu_t[]){f, iter, acc});

    frame[0] = iter;
    mu_fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    if (mu_tbl_getlen(acc) == 0) {
        mu_dec(acc);
        mu_fn_fcall(iter, 0x0f, frame);
        acc = frame[0];
    }

    while (mu_fn_next(iter, 0xf, frame)) {
        frame[0] = mu_tbl_concat(acc, frame[0], 0);
        mu_fn_fcall(f, 0xff, frame);
        acc = frame[0];
    }

    mu_dec(f);
    mu_dec(iter);
    frame[0] = acc;
    return 0xf;
}

MU_DEF_STR(mu_reduce_key_def, "reduce")
MU_DEF_BFN(mu_reduce_def, 0xf, mu_reduce_bfn)

static mcnt_t mu_any_bfn(mu_t *frame) {
    mu_t pred = frame[0];
    mu_t iter = frame[1];
    mu_checkargs(mu_isfn(pred), MU_ANY_KEY, 0x2, frame);

    frame[0] = iter;
    mu_fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    frame[0] = MU_TRUE;
    while (mu_fn_next(iter, 0xf, frame)) {
        mu_fn_fcall(pred, 0xf1, frame);
        if (frame[0]) {
            mu_dec(frame[0]);
            mu_dec(pred);
            mu_dec(iter);
            return 1;
        }
    }

    mu_dec(pred);
    mu_dec(iter);
    return 0;
}

MU_DEF_STR(mu_any_key_def, "any")
MU_DEF_BFN(mu_any_def, 0x2, mu_any_bfn)

static mcnt_t mu_all_bfn(mu_t *frame) {
    mu_t pred = frame[0];
    mu_t iter = frame[1];
    mu_checkargs(mu_isfn(pred), MU_ALL_KEY, 0x2, frame);

    frame[0] = iter;
    mu_fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    frame[0] = MU_TRUE;
    while (mu_fn_next(iter, 0xf, frame)) {
        mu_fn_fcall(pred, 0xf1, frame);
        if (frame[0]) {
            mu_dec(frame[0]);
        } else {
            mu_dec(pred);
            mu_dec(iter);
            return 0;
        }
    }

    mu_dec(pred);
    mu_dec(iter);
    return 1;
}

MU_DEF_STR(mu_all_key_def, "all")
MU_DEF_BFN(mu_all_def, 0x2, mu_all_bfn)


// Other iterators/deferators
static mcnt_t mu_range_step_bfn(mu_t scope, mu_t *frame) {
    mu_t *a = mu_buf_getdata(scope);

    if ((mu_num_cmp(a[2], mu_num_fromuint(0)) > 0 &&
         mu_num_cmp(a[0], a[1]) >= 0) ||
        (mu_num_cmp(a[2], mu_num_fromuint(0)) < 0 && 
         mu_num_cmp(a[0], a[1]) <= 0)) {
        return 0;
    }

    frame[0] = a[0];
    a[0] = mu_num_add(a[0], a[2]);
    return 1;
}

static mcnt_t mu_range_bfn(mu_t *frame) {
    if (!frame[1]) {
        frame[1] = frame[0];
        frame[0] = 0;
    }

    mu_t start = frame[0] ? frame[0] : mu_num_fromuint(0);
    mu_t stop  = frame[1] ? frame[1] : MU_INF;
    mu_t step  = frame[2];
    mu_checkargs(
            mu_isnum(start) && mu_isnum(stop) &&
            (!step || mu_isnum(step)),
            MU_RANGE_KEY, 0x3, frame);

    if (!step) {
        step = mu_num_fromint(mu_num_cmp(start, stop) < 0 ? 1 : -1);
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_range_step_bfn,
            mu_buf_fromdata((mu_t[]){start, stop, step}, 3*sizeof(mu_t)));
    return 1;
}

MU_DEF_STR(mu_range_key_def, "range")
MU_DEF_BFN(mu_range_def, 0x3, mu_range_bfn)

static mcnt_t mu_repeat_step_bfn(mu_t scope, mu_t *frame) {
    mu_t i = mu_tbl_lookup(scope, mu_num_fromuint(1));
    if (mu_num_cmp(i, mu_num_fromuint(0)) <= 0) {
        return 0;
    }

    frame[0] = mu_tbl_lookup(scope, mu_num_fromuint(0));
    mu_tbl_insert(scope, mu_num_fromuint(1),
            mu_num_sub(i, mu_num_fromuint(1)));
    return 1;
}

static mcnt_t mu_repeat_bfn(mu_t *frame) {
    mu_t m     = frame[0];
    mu_t count = frame[1] ? frame[1] : MU_INF;
    mu_checkargs(mu_isnum(count), MU_REPEAT_KEY, 0x2, frame);

    frame[0] = mu_fn_fromsbfn(0x0, mu_repeat_step_bfn,
            mu_tbl_fromlist((mu_t[]){m, count}, 2));
    return 1;
}

MU_DEF_STR(mu_repeat_key_def, "repeat")
MU_DEF_BFN(mu_repeat_def, 0x2, mu_repeat_bfn)


// Iterator manipulation
static mcnt_t mu_zip_step_bfn(mu_t scope, mu_t *frame) {
    mu_t iters = mu_tbl_lookup(scope, mu_num_fromuint(1));

    if (!iters) {
        mu_t iteriter = mu_tbl_lookup(scope, mu_num_fromuint(0));
        iters = mu_tbl_create(0);

        while (mu_fn_next(iteriter, 0x1, frame)) {
            mu_tbl_insert(iters, mu_num_fromuint(mu_tbl_getlen(iters)),
                    mu_fn_call(MU_ITER, 0x11, frame[0]));
        }

        mu_dec(iteriter);
        mu_tbl_insert(scope, mu_num_fromuint(1), mu_inc(iters));
    }

    mu_t acc = mu_tbl_create(mu_tbl_getlen(iters));
    mu_t iter;

    for (muint_t i = 0; mu_tbl_next(iters, &i, 0, &iter);) {
        mu_fn_fcall(iter, 0x0f, frame);
        mu_dec(iter);

        mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        if (!m) {
            mu_dec(acc);
            mu_dec(frame[0]);
            mu_dec(iters);
            return 0;
        }
        mu_dec(m);

        acc = mu_tbl_concat(acc, frame[0], 0);
    }

    mu_dec(iters);
    frame[0] = acc;
    return 0xf;
}

static mcnt_t mu_zip_bfn(mu_t *frame) {
    mu_t iter;
    if (mu_tbl_getlen(frame[0]) == 0) {
        mu_errorf("no arguments passed to zip");
    } else if (mu_tbl_getlen(frame[0]) == 1) {
        iter = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        mu_dec(frame[0]);
    } else {
        iter = frame[0];
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_zip_step_bfn,
            mu_tbl_fromlist((mu_t[]){mu_fn_call(MU_ITER, 0x11, iter)}, 1));
    return 1;
}

MU_DEF_STR(mu_zip_key_def, "zip")
MU_DEF_BFN(mu_zip_def, 0xf, mu_zip_bfn)

static mcnt_t mu_chain_step_bfn(mu_t scope, mu_t *frame) {
    mu_t iter = mu_tbl_lookup(scope, mu_num_fromuint(1));

    if (iter) {
        mu_fn_fcall(iter, 0x0f, frame);
        mu_dec(iter);

        mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        if (m) {
            mu_dec(m);
            return 0xf;
        }
        mu_dec(frame[0]);
    }

    mu_t iters = mu_tbl_lookup(scope, mu_num_fromuint(0));
    mu_fn_fcall(iters, 0x01, frame);
    mu_dec(iters);
    if (frame[0]) {
        mu_tbl_insert(scope, mu_num_fromuint(1),
                mu_fn_call(MU_ITER, 0x11, frame[0]));
        return mu_chain_step_bfn(scope, frame);
    }

    return 0;
}

static mcnt_t mu_chain_bfn(mu_t *frame) {
    mu_t iter;
    if (mu_tbl_getlen(frame[0]) == 0) {
        mu_errorf("no arguments passed to chain");
    } else if (mu_tbl_getlen(frame[0]) == 1) {
        iter = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        mu_dec(frame[0]);
    } else {
        iter = frame[0];
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_chain_step_bfn,
            mu_tbl_fromlist((mu_t[]){mu_fn_call(MU_ITER, 0x11, iter)}, 1));
    return 1;
}

MU_DEF_STR(mu_chain_key_def, "chain")
MU_DEF_BFN(mu_chain_def, 0xf, mu_chain_bfn)

static mcnt_t mu_take_count_step_bfn(mu_t scope, mu_t *frame) {
    mu_t i = mu_tbl_lookup(scope, mu_num_fromuint(0));
    if (mu_num_cmp(i, mu_num_fromuint(0)) <= 0) {
        return 0;
    }

    mu_tbl_insert(scope, mu_num_fromuint(0),
            mu_num_sub(i, mu_num_fromuint(1)));
    mu_t iter = mu_tbl_lookup(scope, mu_num_fromuint(1));
    return mu_fn_tcall(iter, 0x0, frame);
}

static mcnt_t mu_take_while_step_bfn(mu_t scope, mu_t *frame) {
    mu_t iter = mu_tbl_lookup(scope, mu_num_fromuint(1));
    mu_fn_fcall(iter, 0x0f, frame);
    mu_dec(iter);

    mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
    if (!m) {
        mu_dec(frame[0]);
        return 0;
    }
    mu_dec(m);

    m = mu_inc(frame[0]);
    mu_t cond = mu_tbl_lookup(scope, mu_num_fromuint(0));
    mu_fn_fcall(cond, 0xf1, frame);
    mu_dec(cond);
    if (!frame[0]) {
        mu_dec(m);
        return 0;
    }
    mu_dec(frame[0]);
    frame[0] = m;
    return 0xf;
}

static mcnt_t mu_take_bfn(mu_t *frame) {
    mu_t m    = frame[0];
    mu_t iter = frame[1];
    mu_checkargs(mu_isnum(m) || mu_isfn(m),
            MU_TAKE_KEY, 0x2, frame);

    frame[0] = iter;
    mu_fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    mcnt_t (*mu_take_step_bfn)(mu_t scope, mu_t *frame);
    if (mu_isnum(m)) {
        mu_take_step_bfn = mu_take_count_step_bfn;
    } else {
        mu_take_step_bfn = mu_take_while_step_bfn;
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_take_step_bfn,
            mu_tbl_fromlist((mu_t[]){m, iter}, 2));
    return 1;
}

MU_DEF_STR(mu_take_key_def, "take")
MU_DEF_BFN(mu_take_def, 0x2, mu_take_bfn)

static mcnt_t mu_drop_count_step_bfn(mu_t scope, mu_t *frame) {
    mu_t iter = mu_tbl_lookup(scope, mu_num_fromuint(1));
    mu_t i = mu_tbl_lookup(scope, mu_num_fromuint(0));

    if (i) {
        while (mu_num_cmp(i, mu_num_fromuint(0)) > 0) {
            mu_fn_fcall(iter, 0x01, frame);
            if (!frame[0]) {
                mu_dec(iter);
                return 0;
            }
            mu_dec(frame[0]);
            i = mu_num_sub(i, mu_num_fromuint(1));
        }

        mu_tbl_insert(scope, mu_num_fromuint(0), 0);
    }

    return mu_fn_tcall(iter, 0x0, frame);
}

static mcnt_t mu_drop_while_step_bfn(mu_t scope, mu_t *frame) {
    mu_t iter = mu_tbl_lookup(scope, mu_num_fromuint(1));
    mu_t cond = mu_tbl_lookup(scope, mu_num_fromuint(0));

    if (cond) {
        while (mu_fn_next(iter, 0xf, frame)) {
            mu_t m = mu_inc(frame[0]);
            mu_fn_fcall(cond, 0xf1, frame);
            if (!frame[0]) {
                mu_dec(iter);
                mu_dec(cond);
                frame[0] = m;
                mu_tbl_insert(scope, mu_num_fromuint(0), 0);
                return 0xf;
            }
            mu_dec(m);
        }

        mu_dec(iter);
        mu_dec(cond);
        return 0;
    }

    return mu_fn_tcall(iter, 0x0, frame);
}

static mcnt_t mu_drop_bfn(mu_t *frame) {
    mu_t m    = frame[0];
    mu_t iter = frame[1];
    mu_checkargs(mu_isnum(m) || mu_isfn(m),
            MU_TAKE_KEY, 0x2, frame);

    frame[0] = iter;
    mu_fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    mcnt_t (*mu_drop_step_bfn)(mu_t scope, mu_t *frame);
    if (mu_isnum(m)) {
        mu_drop_step_bfn = mu_drop_count_step_bfn;
    } else {
        mu_drop_step_bfn = mu_drop_while_step_bfn;
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_drop_step_bfn,
            mu_tbl_fromlist((mu_t[]){m, iter}, 2));
    return 1;
}

MU_DEF_STR(mu_drop_key_def, "drop")
MU_DEF_BFN(mu_drop_def, 0x2, mu_drop_bfn)


// Iterator ordering
static mcnt_t mu_min_bfn(mu_t *frame) {
    if (mu_tbl_getlen(frame[0]) == 1) {
        mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        mu_dec(frame[0]);
        frame[0] = m;
    }

    mu_fn_fcall(MU_ITER, 0x11, frame);
    mu_t iter = frame[0];

    mu_fn_fcall(iter, 0x0f, frame);
    mu_t min_frame = frame[0];
    mu_t min = mu_tbl_lookup(min_frame, mu_num_fromuint(0));
    if (!min) {
        mu_errorf("no elements passed to min");
    }

    mtype_t type = mu_gettype(min);
    mint_t (*cmp)(mu_t, mu_t) = mu_attr_cmp[type];
    if (!cmp) {
        mu_errorf("unable to compare %r", min);
    }

    while (mu_fn_next(iter, 0xf, frame)) {
        mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        if (mu_gettype(m) != type) {
            mu_errorf("unable to compare %r and %r", min, m);
        }

        if (cmp(m, mu_inc(min)) < 0) {
            mu_dec(min);
            mu_dec(min_frame);
            min = m;
            min_frame = frame[0];
        } else {
            mu_dec(m);
            mu_dec(frame[0]);
        }
    }

    mu_dec(iter);
    mu_dec(min);
    frame[0] = min_frame;
    return 0xf;
}

MU_DEF_STR(mu_min_key_def, "min")
MU_DEF_BFN(mu_min_def, 0xf, mu_min_bfn)

static mcnt_t mu_max_bfn(mu_t *frame) {
    if (mu_tbl_getlen(frame[0]) == 1) {
        mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        mu_dec(frame[0]);
        frame[0] = m;
    }

    mu_fn_fcall(MU_ITER, 0x11, frame);
    mu_t iter = frame[0];

    mu_fn_fcall(iter, 0x0f, frame);
    mu_t max_frame = frame[0];
    mu_t max = mu_tbl_lookup(max_frame, mu_num_fromuint(0));
    if (!max) {
        mu_errorf("no elements passed to max");
    }

    mtype_t type = mu_gettype(max);
    mint_t (*cmp)(mu_t, mu_t) = mu_attr_cmp[type];
    if (!cmp) {
        mu_errorf("unable to compare %r", max);
    }

    while (mu_fn_next(iter, 0xf, frame)) {
        mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        if (mu_gettype(m) != type) {
            mu_errorf("unable to compare %r and %r", max, m);
        }

        if (cmp(m, mu_inc(max)) >= 0) {
            mu_dec(max);
            mu_dec(max_frame);
            max = m;
            max_frame = frame[0];
        } else {
            mu_dec(m);
            mu_dec(frame[0]);
        }
    }

    mu_dec(iter);
    mu_dec(max);
    frame[0] = max_frame;
    return 0xf;
}

MU_DEF_STR(mu_max_key_def, "max")
MU_DEF_BFN(mu_max_def, 0xf, mu_max_bfn)

static mcnt_t mu_reverse_step_bfn(mu_t scope, mu_t *frame) {
    mu_t i = mu_tbl_lookup(scope, mu_num_fromuint(1));
    if (mu_num_cmp(i, mu_num_fromuint(0)) < 0) {
        return 0;
    }

    mu_t store = mu_tbl_lookup(scope, mu_num_fromuint(0));
    frame[0] = mu_tbl_lookup(store, i);
    mu_dec(store);

    mu_tbl_insert(scope, mu_num_fromuint(1),
            mu_num_sub(i, mu_num_fromuint(1)));
    return 0xf;
}

static mcnt_t mu_reverse_bfn(mu_t *frame) {
    mu_fn_fcall(MU_ITER, 0x11, frame);
    mu_t iter = frame[0];
    mu_t store = mu_tbl_create(0);

    while (mu_fn_next(iter, 0xf, frame)) {
        mu_tbl_insert(store, mu_num_fromuint(mu_tbl_getlen(store)), frame[0]);
    }

    mu_dec(iter);

    frame[0] = mu_fn_fromsbfn(0x0, mu_reverse_step_bfn,
            mu_tbl_fromlist((mu_t[]){
                store, mu_num_fromuint(mu_tbl_getlen(store)-1)
            }, 2));
    return 1;
}

MU_DEF_STR(mu_reverse_key_def, "reverse")
MU_DEF_BFN(mu_reverse_def, 0x1, mu_reverse_bfn)

// Simple iterative merge sort
static void mu_fn_merge_sort(mu_t elems) {
    // Uses arrays (first elem, frame) to keep from
    // looking up the elem each time.
    // We use two arrays so we can just flip each merge.
    muint_t len = mu_tbl_getlen(elems);
    mu_t (*a)[2] = mu_alloc(len*sizeof(mu_t[2]));
    mu_t (*b)[2] = mu_alloc(len*sizeof(mu_t[2]));

    mtype_t type = MTNUM;

    for (muint_t i = 0, j = 0; mu_tbl_next(elems, &i, 0, &a[j][1]); j++) {
        a[j][0] = mu_tbl_lookup(a[j][1], mu_num_fromuint(0));

        if (j == 0) {
            type = mu_gettype(a[j][0]);
        } else {
            if (type != mu_gettype(a[j][0])) {
                mu_errorf("unable to compare %r and %r", a[0][0], a[j][0]);
            }
        }
    }

    mint_t (*cmp)(mu_t, mu_t) = mu_attr_cmp[type];
    if (!cmp) {
        mu_errorf("unable to compare %r", a[0][0]);
    }

    for (muint_t slice = 1; slice < len; slice *= 2) {
        for (muint_t i = 0; i < len; i += 2*slice) {
            muint_t x = 0;
            muint_t y = 0;

            for (muint_t j = 0; j < 2*slice && i+j < len; j++) {
                if (y >= slice || i+slice+y >= len || (x < slice &&
                        cmp(a[i+x][0], mu_inc(a[i+slice+y][0])) <= 0)) {
                    memcpy(b[i+j], a[i+x], sizeof(mu_t[2]));
                    x++;
                } else {
                    memcpy(b[i+j], a[i+slice+y], sizeof(mu_t[2]));
                    y++;
                }
            }
        }

        mu_t (*t)[2] = a;
        a = b;
        b = t;
    }

    // Reuse the table to store results
    for (muint_t i = 0; i < len; i++) {
        mu_dec(a[i][0]);
        mu_tbl_insert(elems, mu_num_fromuint(i), a[i][1]);
    }

    // Since both arrays identical, it doesn't matter if they've been swapped
    mu_dealloc(a, len*sizeof(mu_t[2]));
    mu_dealloc(b, len*sizeof(mu_t[2]));
}

static mcnt_t mu_sort_step_bfn(mu_t scope, mu_t *frame) {
    mu_t store = mu_tbl_lookup(scope, mu_num_fromuint(0));
    muint_t i = mu_num_getuint(mu_tbl_lookup(scope, mu_num_fromuint(1)));

    bool next = mu_tbl_next(store, &i, 0, &frame[0]);
    mu_dec(store);
    mu_tbl_insert(scope, mu_num_fromuint(1), mu_num_fromuint(i));
    return next ? 0xf : 0;
}

static mcnt_t mu_sort_bfn(mu_t *frame) {
    mu_fn_fcall(MU_ITER, 0x11, frame);
    mu_t iter = frame[0];
    mu_t store = mu_tbl_create(0);

    while (mu_fn_next(iter, 0xf, frame)) {
        mu_tbl_insert(store, mu_num_fromuint(mu_tbl_getlen(store)), frame[0]);
    }

    mu_dec(iter);

    mu_fn_merge_sort(store);

    frame[0] = mu_fn_fromsbfn(0x0, mu_sort_step_bfn,
            mu_tbl_fromlist((mu_t[]){store, mu_num_fromuint(0)}, 2));
    return 1;
}

MU_DEF_STR(mu_sort_key_def, "sort")
MU_DEF_BFN(mu_sort_def, 0x1, mu_sort_bfn)


// Builtins table
MU_DEF_TBL(mu_builtins_def, {
    // Constants
    { mu_true_key_def,      mu_true_def },
    { mu_inf_key_def,       mu_inf_def },
    { mu_e_key_def,         mu_e_def },
    { mu_pi_key_def,        mu_pi_def },

    // Type casts
    { mu_num_key_def,       mu_num_def },
    { mu_str_key_def,       mu_str_def },
    { mu_tbl_key_def,       mu_tbl_def },
    { mu_fn_key_def,       mu_fn_def },

    // Logic operations
    { mu_not_key_def,       mu_not_def },
    { mu_eq_key_def,        mu_eq_def },
    { mu_neq_key_def,       mu_neq_def },
    { mu_is_key_def,        mu_is_def },
    { mu_lt_key_def,        mu_lt_def },
    { mu_lte_key_def,       mu_lte_def },
    { mu_gt_key_def,        mu_gt_def },
    { mu_gte_key_def,       mu_gte_def },

    // Arithmetic operations
    { mu_add_key_def,       mu_add_def },
    { mu_sub_key_def,       mu_sub_def },
    { mu_mul_key_def,       mu_mul_def },
    { mu_div_key_def,       mu_div_def },

    { mu_abs_key_def,       mu_abs_def },
    { mu_floor_key_def,     mu_floor_def },
    { mu_ceil_key_def,      mu_ceil_def },
    { mu_idiv_key_def,      mu_idiv_def },
    { mu_mod_key_def,       mu_mod_def },

    { mu_pow_key_def,       mu_pow_def },
    { mu_log_key_def,       mu_log_def },

    { mu_cos_key_def,       mu_cos_def },
    { mu_acos_key_def,      mu_acos_def },
    { mu_sin_key_def,       mu_sin_def },
    { mu_asin_key_def,      mu_asin_def },
    { mu_tan_key_def,       mu_tan_def },
    { mu_atan_key_def,      mu_atan_def },

    // Bitwise/Set operations
    { mu_and_key_def,       mu_and_def },
    { mu_or_key_def,        mu_or_def },
    { mu_xor_key_def,       mu_xor_def },
    { mu_diff_key_def,      mu_diff_def },

    { mu_shl_key_def,       mu_shl_def },
    { mu_shr_key_def,       mu_shr_def },

    // String representation
    { mu_parse_key_def,     mu_parse_def },
    { mu_repr_key_def,      mu_repr_def },
    { mu_ord_key_def,       mu_ord_def },
    { mu_chr_key_def,       mu_chr_def },
    { mu_bin_key_def,       mu_bin_def },
    { mu_oct_key_def,       mu_oct_def },
    { mu_hex_key_def,       mu_hex_def },

    // Data structure operations
    { mu_len_key_def,       mu_len_def },
    { mu_tail_key_def,      mu_tail_def },
    { mu_const_key_def,     mu_const_def },

    { mu_push_key_def,      mu_push_def },
    { mu_pop_key_def,       mu_pop_def },

    { mu_concat_key_def,    mu_concat_def },
    { mu_subset_key_def,    mu_subset_def },

    // String operations
    { mu_find_key_def,      mu_find_def },
    { mu_replace_key_def,   mu_replace_def },
    { mu_split_key_def,     mu_split_def },
    { mu_join_key_def,      mu_join_def },
    { mu_pad_key_def,       mu_pad_def },
    { mu_strip_key_def,     mu_strip_def },

    // Function operations
    { mu_bind_key_def,      mu_bind_def },
    { mu_comp_key_def,      mu_comp_def },

    { mu_map_key_def,       mu_map_def },
    { mu_filter_key_def,    mu_filter_def },
    { mu_reduce_key_def,    mu_reduce_def },

    { mu_any_key_def,       mu_any_def },
    { mu_all_key_def,       mu_all_def },

    // Iterators and deferators
    { mu_iter_key_def,      mu_iter_def },
    { mu_pairs_key_def,     mu_pairs_def },

    { mu_range_key_def,     mu_range_def },
    { mu_repeat_key_def,    mu_repeat_def },
    { mu_random_key_def,    mu_random_def },

    // Iterator manipulation
    { mu_zip_key_def,       mu_zip_def },
    { mu_chain_key_def,     mu_chain_def },

    { mu_take_key_def,      mu_take_def },
    { mu_drop_key_def,      mu_drop_def },

    // Iterator ordering
    { mu_min_key_def,       mu_min_def },
    { mu_max_key_def,       mu_max_def },

    { mu_reverse_key_def,   mu_reverse_def },
    { mu_sort_key_def,      mu_sort_def },

    // System operations
    { mu_error_key_def,     mu_error_def },
    { mu_print_key_def,     mu_print_def },
    { mu_import_key_def,    mu_import_def },
})
