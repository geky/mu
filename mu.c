#include "mu.h"

#include "sys.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"
#include "parse.h"
#include "vm.h"


// Constants
MU_GEN_STR(mu_gen_key_true,  "true")
MU_GEN_STR(mu_gen_key_false, "false")

MU_GEN_UINT(mu_gen_true, 1)


// Mu type destructors
extern void mu_str_destroy(mu_t);
extern void mu_buf_destroy(mu_t);
extern void mu_cbuf_destroy(mu_t);
extern void mu_tbl_destroy(mu_t);
extern void mu_fn_destroy(mu_t);

static void (*const mu_attr_destroy[8])(mu_t) = {
    [MTSTR]  = mu_str_destroy,
    [MTBUF]  = mu_buf_destroy,
    [MTCBUF] = mu_cbuf_destroy,
    [MTTBL]  = mu_tbl_destroy,
    [MTFN]   = mu_fn_destroy,
};

void mu_destroy(mu_t m) {
    mu_attr_destroy[mu_gettype(m)](m);
}

// Mu type comparisons
static mint_t (*const mu_attr_cmp[8])(mu_t, mu_t) = {
    [MTNUM] = mu_num_cmp,
    [MTSTR] = mu_str_cmp,
};

// Mu type iterators
static mu_t mu_fn_iter(mu_t m) { return m;}

static mu_t (*const mu_attr_iter[8])(mu_t) = {
    [MTSTR] = mu_str_iter,
    [MTTBL] = mu_tbl_iter,
    [MTFN]  = mu_fn_iter,
};

// Mu type representations
MU_GEN_STR(mu_gen_key_cdata, "cdata")

static mu_t (*const mu_attr_name[8])(void) = {
    [MTNIL]  = mu_gen_key_nil,
    [MTNUM]  = mu_gen_key_num,
    [MTSTR]  = mu_gen_key_str,
    [MTTBL]  = mu_gen_key_tbl,
    [MTFN]   = mu_gen_key_fn,
    [MTBUF]  = mu_gen_key_cdata,
    [MTCBUF] = mu_gen_key_cdata,
};

static mu_t nil_repr(mu_t m) {
    return MU_KW_NIL;
}

static mu_t (*const mu_attr_repr[8])(mu_t) = {
    [MTNIL] = nil_repr,
    [MTNUM] = mu_num_repr,
    [MTSTR] = mu_str_repr,
};


// Frame operations
void mu_frame_move(mcnt_t fc, mu_t *dframe, mu_t *sframe) {
    memcpy(dframe, sframe, sizeof(mu_t)*mu_frame_len(fc));
}

void mu_frame_convert(mcnt_t sc, mcnt_t dc, mu_t *frame) {
    if (dc != 0xf && sc != 0xf) {
        for (muint_t i = dc; i < sc; i++) {
            mu_dec(frame[i]);
        }

        for (muint_t i = sc; i < dc; i++) {
            frame[i] = 0;
        }
    } else if (dc != 0xf) {
        mu_t t = *frame;

        for (muint_t i = 0; i < dc; i++) {
            frame[i] = mu_tbl_lookup(t, mu_num_fromuint(i));
        }

        mu_tbl_dec(t);
    } else if (sc != 0xf) {
        mu_t t = mu_tbl_create(sc);

        for (muint_t i = 0; i < sc; i++) {
            mu_tbl_insert(t, mu_num_fromuint(i), frame[i]);
        }

        *frame = t;
    }
}


// System operations
mu_noreturn mu_error(const char *s, muint_t n) {
    mu_sys_error(s, n);
    mu_unreachable;
}

mu_noreturn mu_verrorf(const char *f, va_list args) {
    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_buf_vformat(&b, &n, f, args);
    mu_error(mu_buf_getdata(b), n);
}

mu_noreturn mu_errorf(const char *f, ...) {
    va_list args;
    va_start(args, f);
    mu_verrorf(f, args);
}

static mcnt_t mu_bfn_error(mu_t *frame) {
    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_t v;

    for (muint_t i = 0; mu_tbl_next(frame[0], &i, 0, &v);) {
        mu_buf_format(&b, &n, "%m", v);
    }

    mu_error(mu_buf_getdata(b), n);
}

MU_GEN_STR(mu_gen_key_error, "error")
MU_GEN_BFN(mu_gen_error, 0xf, mu_bfn_error)

void mu_print(const char *s, muint_t n) {
    mu_sys_print(s, n);
}

void mu_vprintf(const char *f, va_list args) {
    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_buf_vformat(&b, &n, f, args);
    mu_print(mu_buf_getdata(b), n);
    mu_buf_dec(b);
}

void mu_printf(const char *f, ...) {
    va_list args;
    va_start(args, f);
    mu_vprintf(f, args);
    va_end(args);
}

static mcnt_t mu_bfn_print(mu_t *frame) {
    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_t v;

    for (muint_t i = 0; mu_tbl_next(frame[0], &i, 0, &v);) {
        mu_buf_format(&b, &n, "%m", v);
    }

    mu_print(mu_buf_getdata(b), n);
    mu_buf_dec(b);
    mu_tbl_dec(frame[0]);
    return 0;
}

MU_GEN_STR(mu_gen_key_print, "print")
MU_GEN_BFN(mu_gen_print, 0xf, mu_bfn_print)

static mcnt_t mu_bfn_import(mu_t *frame) {
    mu_t name = frame[0];
    if (!mu_isstr(name)) {
        mu_error_arg(MU_KEY_IMPORT, 0x1, frame);
    }

    static mu_t import_history = 0;
    if (!import_history) {
        import_history = mu_tbl_create(0);
    }

    mu_t module = mu_tbl_lookup(import_history, mu_str_inc(name));
    if (module) {
        mu_str_dec(name);
        frame[0] = module;
        return 1;
    }

    module = mu_sys_import(mu_str_inc(name));
    mu_tbl_insert(import_history, name, mu_inc(module));
    frame[0] = module;
    return 1;
}

MU_GEN_STR(mu_gen_key_import, "import")
MU_GEN_BFN(mu_gen_import, 0x1, mu_bfn_import)


// Evaluation and entry into Mu
void mu_feval(const char *s, muint_t n, mu_t scope, mcnt_t fc, mu_t *frame) {
    mu_t c = mu_compile(s, n);
    mcnt_t rets = mu_exec(c, mu_inc(scope), frame);
    mu_frame_convert(rets, fc, frame);
}

mu_t mu_veval(const char *s, muint_t n, mu_t scope, mcnt_t fc, va_list args) {
    mu_t frame[MU_FRAME];

    mu_feval(s, n, scope, fc, frame);

    for (muint_t i = 1; i < mu_frame_len(fc); i++) {
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
mu_noreturn mu_error_arg(mu_t name, mcnt_t fc, mu_t *frame) {
    mu_t message = mu_buf_create(0);
    muint_t n = 0;

    mu_buf_format(&message, &n, "invalid argument in %m(", name);

    if (fc == 0xf) {
        mu_buf_format(&message, &n, "..");
        fc = 1;
    }

    for (muint_t i = 0; i < fc; i++) {
        mu_buf_format(&message, &n, "%nr%c ",
                frame[i], 0, (i != fc-1) ? ',' : ')');
    }

    mu_errorf("%ns", mu_buf_getdata(message), n);
}

mu_noreturn mu_error_op(mu_t name, mcnt_t fc, mu_t *frame) {
    if (fc < 2 || !frame[1]) {
        mu_errorf("invalid operation %m%r", name, frame[0]);
    } else {
        mu_errorf("invalid operation %r %m %r", frame[0], name, frame[1]);
    }
}

mu_noreturn mu_error_cast(mu_t name, mu_t m) {
    mu_errorf("invalid conversion from %r to %m", m, name);
}


// wrappers for comparison operations
static mcnt_t mu_bfn_not(mu_t *frame) {
    mu_dec(frame[0]);
    frame[0] = !frame[0] ? MU_TRUE : MU_FALSE;
    return 1;
}

MU_GEN_STR(mu_gen_key_not, "!")
MU_GEN_BFN(mu_gen_not, 0x1, mu_bfn_not)

static mcnt_t mu_bfn_eq(mu_t *frame) {
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = (frame[0] == frame[1]) ? MU_TRUE : MU_FALSE;
    return 1;
}

MU_GEN_STR(mu_gen_key_eq, "==")
MU_GEN_BFN(mu_gen_eq,  0x2, mu_bfn_eq)

static mcnt_t mu_bfn_neq(mu_t *frame) {
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = (frame[0] != frame[1]) ? MU_TRUE : MU_FALSE;
    return 1;
}

MU_GEN_STR(mu_gen_key_neq, "!=")
MU_GEN_BFN(mu_gen_neq, 0x2, mu_bfn_neq)

static mcnt_t mu_bfn_lt(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (mu_gettype(a) != mu_gettype(b) || !mu_attr_cmp[mu_gettype(a)]) {
        mu_error_op(MU_KEY_LT, 0x2, frame);
    }

    frame[0] = (mu_attr_cmp[mu_gettype(a)](a, b) < 0) ? MU_TRUE : MU_FALSE;
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    return 1;
}

MU_GEN_STR(mu_gen_key_lt, "<")
MU_GEN_BFN(mu_gen_lt,  0x2, mu_bfn_lt)

static mcnt_t mu_bfn_lte(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (mu_gettype(a) != mu_gettype(b) || !mu_attr_cmp[mu_gettype(a)]) {
        mu_error_op(MU_KEY_LTE, 0x2, frame);
    }

    frame[0] = (mu_attr_cmp[mu_gettype(a)](a, b) <= 0) ? MU_TRUE : MU_FALSE;
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    return 1;
}

MU_GEN_STR(mu_gen_key_lte, "<=")
MU_GEN_BFN(mu_gen_lte, 0x2, mu_bfn_lte)

static mcnt_t mu_bfn_gt(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (mu_gettype(a) != mu_gettype(b) || !mu_attr_cmp[mu_gettype(a)]) {
        mu_error_op(MU_KEY_GT, 0x2, frame);
    }

    frame[0] = (mu_attr_cmp[mu_gettype(a)](a, b) > 0) ? MU_TRUE : MU_FALSE;
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    return 1;
}

MU_GEN_STR(mu_gen_key_gt, ">")
MU_GEN_BFN(mu_gen_gt,  0x2, mu_bfn_gt)

static mcnt_t mu_bfn_gte(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (mu_gettype(a) != mu_gettype(b) || !mu_attr_cmp[mu_gettype(a)]) {
        mu_error_op(MU_KEY_GTE, 0x2, frame);
    }

    frame[0] = (mu_attr_cmp[mu_gettype(a)](a, b) >= 0) ? MU_TRUE : MU_FALSE;
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    return 1;
}

MU_GEN_STR(mu_gen_key_gte, ">=")
MU_GEN_BFN(mu_gen_gte, 0x2, mu_bfn_gte)

static mcnt_t mu_bfn_is(mu_t *frame) {
    mu_t m    = frame[0];
    mu_t type = frame[1];
    mu_dec(m);
    mu_dec(type);

    frame[0] = MU_TRUE;
    switch (mu_gettype(m)) {
        case MTNIL: return !type;
        case MTNUM: return type == MU_NUM;
        case MTSTR: return type == MU_STR;
        case MTTBL: return type == MU_TBL;
        case MTFN:  return type == MU_FN;
        default:    return false;
    }
}

MU_GEN_STR(mu_gen_key_is, "is")
MU_GEN_BFN(mu_gen_is, 0x2, mu_bfn_is)


// String representation
static mcnt_t mu_bfn_parse(mu_t *frame) {
    mu_t s = frame[0];
    if (!mu_isstr(s)) {
        mu_error_arg(MU_KEY_PARSE, 0x1, frame);
    }

    frame[0] = mu_parse((const char *)mu_str_getdata(s), mu_str_getlen(s));
    mu_dec(s);
    return 1;
}

MU_GEN_STR(mu_gen_key_parse, "parse")
MU_GEN_BFN(mu_gen_parse, 0x1, mu_bfn_parse)

static mcnt_t mu_bfn_repr(mu_t *frame) {
    mu_t m     = frame[0];
    mu_t depth = frame[1] ? frame[1] : mu_num_fromuint(1);
    if (!mu_isnum(depth)) {
        mu_error_arg(MU_KEY_REPR, 0x2, frame);
    }

    if (mu_attr_repr[mu_gettype(m)]) {
        frame[0] = mu_attr_repr[mu_gettype(m)](m);
        return 1;
    } else if (mu_istbl(m) && mu_num_cmp(depth, mu_num_fromuint(0)) > 0) {
        frame[0] = mu_tbl_dump(m, depth);
        return 1;
    } else {
        mu_dec(m);
        frame[0] = mu_str_format("<%m 0x%wx>",
                mu_attr_name[mu_gettype(m)](),
                (muint_t)m & ~7);
        return 1;
    }
}

MU_GEN_STR(mu_gen_key_repr, "repr")
MU_GEN_BFN(mu_gen_repr, 0x2, mu_bfn_repr)

static mcnt_t mu_bfn_bin(mu_t *frame) {
    mu_t m = frame[0];

    if (mu_isnum(m)) {
        frame[0] = mu_num_bin(m);
        return 1;
    } else if (mu_isstr(m)) {
        frame[0] = mu_str_bin(m);
        return 1;
    } else {
        mu_error_arg(MU_KEY_BIN, 0x1, frame);
    }
}

MU_GEN_STR(mu_gen_key_bin, "bin")
MU_GEN_BFN(mu_gen_bin, 0x1, mu_bfn_bin)

static mcnt_t mu_bfn_oct(mu_t *frame) {
    mu_t m = frame[0];

    if (mu_isnum(m)) {
        frame[0] = mu_num_oct(m);
        return 1;
    } else if (mu_isstr(m)) {
        frame[0] = mu_str_oct(m);
        return 1;
    } else {
        mu_error_arg(MU_KEY_OCT, 0x1, frame);
    }
}

MU_GEN_STR(mu_gen_key_oct, "oct")
MU_GEN_BFN(mu_gen_oct, 0x1, mu_bfn_oct)

static mcnt_t mu_bfn_hex(mu_t *frame) {
    mu_t m = frame[0];

    if (mu_isnum(m)) {
        frame[0] = mu_num_hex(m);
        return 1;
    } else if (mu_isstr(m)) {
        frame[0] = mu_str_hex(m);
        return 1;
    } else {
        mu_error_arg(MU_KEY_HEX, 0x1, frame);
    }
}

MU_GEN_STR(mu_gen_key_hex, "hex")
MU_GEN_BFN(mu_gen_hex, 0x1, mu_bfn_hex)


// Data structure operations
static mint_t mu_clamp(mu_t n, mint_t lower, mint_t upper) {
    mu_assert(mu_isnum(n));

    if (mu_num_cmp(n, mu_num_fromint(lower)) < 0) {
        return lower;
    } else if (mu_num_cmp(n, mu_num_fromint(upper)) > 0) {
        return upper;
    } else {
        return mu_num_getint(n);
    }
}

static mcnt_t mu_bfn_len(mu_t *frame) {
    mu_t a = frame[0];

    if (mu_isstr(a)) {
        frame[0] = mu_num_fromuint(mu_str_getlen(a));
        mu_dec(a);
        return 1;
    } else if (mu_istbl(a)) {
        frame[0] = mu_num_fromuint(mu_tbl_getlen(a));
        mu_dec(a);
        return 1;
    } else {
        mu_error_arg(MU_KEY_LEN, 0x1, frame);
    }
}

MU_GEN_STR(mu_gen_key_len, "len")
MU_GEN_BFN(mu_gen_len, 0x1, mu_bfn_len)

static mcnt_t mu_bfn_concat(mu_t *frame) {
    mu_t a      = frame[0];
    mu_t b      = frame[1];
    mu_t offset = frame[2];
    if (offset && !mu_isnum(offset)) {
        mu_error_op(MU_KEY_CONCAT, 0x3, frame);
    }

    if (mu_isstr(a) && mu_isstr(b)) {
        frame[0] = mu_str_concat(a, b);
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = mu_tbl_concat(a, b, offset);
        return 1;
    } else {
        mu_error_op(MU_KEY_CONCAT, 0x3, frame);
    }
}

MU_GEN_STR(mu_gen_key_concat, "++")
MU_GEN_BFN(mu_gen_concat, 0x3, mu_bfn_concat)

static mcnt_t mu_bfn_subset(mu_t *frame) {
    mu_t a     = frame[0];
    mu_t lower = frame[1];
    mu_t upper = frame[2];
    if (!mu_isnum(lower) || (upper && !mu_isnum(upper))) {
        mu_error_arg(MU_KEY_SUBSET, 0x3, frame);
    }

    mint_t loweri = mu_clamp(lower, -(mint_t)(mlen_t)-1, (mlen_t)-1);
    mint_t upperi;
    if (!upper) {
        upperi = loweri+1;
    } else {
        upperi = mu_clamp(upper, -(mint_t)(mlen_t)-1, (mlen_t)-1);
    }

    if (mu_isstr(a)) {
        frame[0] = mu_str_subset(a, loweri, upperi);
        return 1;
    } else if (mu_istbl(a)) {
        frame[0] = mu_tbl_subset(a, loweri, upperi);
        return 1;
    } else {
        mu_error_arg(MU_KEY_SUBSET, 0x3, frame);
    }
}

MU_GEN_STR(mu_gen_key_subset, "sub")
MU_GEN_BFN(mu_gen_subset, 0x3, mu_bfn_subset)

static mcnt_t mu_bfn_push(mu_t *frame) {
    mu_t t = frame[0];
    mu_t v = frame[1];
    mu_t i = frame[2];
    if (!mu_istbl(t) || (i && !mu_isnum(i))) {
        mu_error_arg(MU_KEY_PUSH, 0x3, frame);
    }

    mint_t ii;
    if (!i) {
        ii = mu_tbl_getlen(t);
    } else {
        ii = mu_clamp(i, -(mint_t)(mlen_t)-1, (mlen_t)-1);
    }

    mu_tbl_push(t, v, ii);
    return 0;
}

MU_GEN_STR(mu_gen_key_push, "push")
MU_GEN_BFN(mu_gen_push, 0x3, mu_bfn_push)

static mcnt_t mu_bfn_pop(mu_t *frame) {
    mu_t t = frame[0];
    mu_t i = frame[1];
    if (!mu_istbl(t) || (i && !mu_isnum(i))) {
        mu_error_arg(MU_KEY_POP, 0x2, frame);
    }

    mint_t ii;
    if (!i) {
        ii = mu_tbl_getlen(t) - 1;
    } else {
        ii = mu_clamp(i, -(mint_t)(mlen_t)-1, (mlen_t)-1);
    }

    frame[0] = mu_tbl_pop(t, ii);
    return 1;
}

MU_GEN_STR(mu_gen_key_pop, "pop")
MU_GEN_BFN(mu_gen_pop, 0x2, mu_bfn_pop)

static mcnt_t mu_bfn_and(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];

    if (mu_isnum(a) && mu_isnum(b)) {
        frame[0] = mu_num_and(a, b);
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = mu_tbl_and(a, b);
        return 1;
    } else {
        mu_error_op(MU_KEY_AND2, 0x2, frame);
    }
}

MU_GEN_STR(mu_gen_key_and2, "&")
MU_GEN_BFN(mu_gen_and, 0x2, mu_bfn_and)

static mcnt_t mu_bfn_or(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];

    if (mu_isnum(a) && mu_isnum(b)) {
        frame[0] = mu_num_or(a, b);
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = mu_tbl_or(a, b);
        return 1;
    } else {
        mu_error_op(MU_KEY_OR2, 0x2, frame);
    }
}

MU_GEN_STR(mu_gen_key_or2, "|")
MU_GEN_BFN(mu_gen_or, 0x2, mu_bfn_or)

static mcnt_t mu_bfn_xor(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];

    if (mu_isnum(a) && !b) {
        frame[0] = mu_num_not(a);
        return 1;
    } else if (mu_isnum(a) && mu_isnum(b)) {
        frame[0] = mu_num_xor(a, b);
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = mu_tbl_xor(a, b);
        return 1;
    } else {
        mu_error_op(MU_KEY_XOR, 0x2, frame);
    }
}

MU_GEN_STR(mu_gen_key_xor, "~")
MU_GEN_BFN(mu_gen_xor, 0x2, mu_bfn_xor)

static mcnt_t mu_bfn_diff(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];

    if (mu_isnum(a) && mu_isnum(b)) {
        frame[0] = mu_num_xor(a, mu_num_not(b));
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = mu_tbl_diff(a, b);
        return 1;
    } else {
        mu_error_op(MU_KEY_DIFF, 0x2, frame);
    }
}

MU_GEN_STR(mu_gen_key_diff, "&~")
MU_GEN_BFN(mu_gen_diff, 0x2, mu_bfn_diff)


// Iterators and generators
static mcnt_t mu_bfn_iter(mu_t *frame) {
    mu_t m = frame[0];
    if (!mu_attr_iter[mu_gettype(m)]) {
        mu_error_cast(MU_KEY_ITER, m);
    }

    frame[0] = mu_attr_iter[mu_gettype(m)](m);
    return 1;
}

MU_GEN_STR(mu_gen_key_iter, "iter")
MU_GEN_BFN(mu_gen_iter, 0x1, mu_bfn_iter)

static mcnt_t mu_bfn_pairs(mu_t *frame) {
    mu_t m = frame[0];

    if (mu_istbl(m)) {
        frame[0] = mu_tbl_pairs(m);
        return 1;
    } else if (mu_attr_iter[mu_gettype(m)]) {
        frame[0] = mu_fn_call(MU_RANGE, 0x01);
        frame[1] = mu_attr_iter[mu_gettype(m)](m);
        return mu_fn_tcall(MU_ZIP, 0x2, frame);
    } else {
        mu_error_cast(MU_KEY_PAIRS, m);
    }
}

MU_GEN_STR(mu_gen_key_pairs, "pairs")
MU_GEN_BFN(mu_gen_pairs, 0x1, mu_bfn_pairs)


// Functions over iterators
static mcnt_t mu_bfn_map_step(mu_t scope, mu_t *frame) {
    mu_t f = mu_tbl_lookup(scope, mu_num_fromuint(0));
    mu_t i = mu_tbl_lookup(scope, mu_num_fromuint(1));

    while (mu_fn_next(i, 0xf, frame)) {
        mu_fn_fcall(f, 0xff, frame);
        mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        if (m) {
            mu_dec(m);
            mu_fn_dec(f);
            mu_fn_dec(i);
            return 0xf;
        }
        mu_tbl_dec(frame[0]);
    }

    mu_fn_dec(f);
    mu_fn_dec(i);
    return 0;
}

static mcnt_t mu_bfn_map(mu_t *frame) {
    mu_t f    = frame[0];
    mu_t iter = frame[1];
    if (!mu_isfn(f)) {
        mu_error_arg(MU_KEY_MAP, 0x2, frame);
    }

    frame[0] = iter;
    mu_fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    frame[0] = mu_fn_fromsbfn(0, mu_bfn_map_step,
            mu_tbl_fromlist((mu_t[]){f, iter}, 2));
    return 1;
}

MU_GEN_STR(mu_gen_key_map, "map")
MU_GEN_BFN(mu_gen_map, 0x2, mu_bfn_map)

static mcnt_t mu_bfn_filter_step(mu_t scope, mu_t *frame) {
    mu_t f = mu_tbl_lookup(scope, mu_num_fromuint(0));
    mu_t i = mu_tbl_lookup(scope, mu_num_fromuint(1));

    while (mu_fn_next(i, 0xf, frame)) {
        mu_t m = mu_tbl_inc(frame[0]);
        mu_fn_fcall(f, 0xf1, frame);
        if (frame[0]) {
            mu_dec(frame[0]);
            frame[0] = m;
            mu_dec(f);
            mu_dec(i);
            return 0xf;
        }
        mu_tbl_dec(m);
    }

    mu_fn_dec(f);
    mu_fn_dec(i);
    return 0;
}

static mcnt_t mu_bfn_filter(mu_t *frame) {
    mu_t f    = frame[0];
    mu_t iter = frame[1];
    if (!mu_isfn(f)) {
        mu_error_arg(MU_KEY_FILTER, 0x2, frame);
    }

    frame[0] = iter;
    mu_fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    frame[0] = mu_fn_fromsbfn(0, mu_bfn_filter_step,
            mu_tbl_fromlist((mu_t[]){f, iter}, 2));
    return 1;
}

MU_GEN_STR(mu_gen_key_filter, "filter")
MU_GEN_BFN(mu_gen_filter, 0x2, mu_bfn_filter)

static mcnt_t mu_bfn_reduce(mu_t *frame) {
    mu_t f    = mu_tbl_pop(frame[0], 0);
    mu_t iter = mu_tbl_pop(frame[0], 0);
    mu_t acc  = frame[0];
    if (!mu_isfn(f)) {
        mu_error_arg(MU_KEY_REDUCE, 0x3, (mu_t[]){f, iter, acc});
    }

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

    mu_fn_dec(f);
    mu_fn_dec(iter);
    frame[0] = acc;
    return 0xf;
}

MU_GEN_STR(mu_gen_key_reduce, "reduce")
MU_GEN_BFN(mu_gen_reduce, 0xf, mu_bfn_reduce)

static mcnt_t mu_bfn_any(mu_t *frame) {
    mu_t pred = frame[0];
    mu_t iter = frame[1];
    if (!mu_isfn(pred)) {
        mu_error_arg(MU_KEY_ANY, 0x2, frame);
    }

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

    mu_fn_dec(pred);
    mu_fn_dec(iter);
    return 0;
}

MU_GEN_STR(mu_gen_key_any, "any")
MU_GEN_BFN(mu_gen_any, 0x2, mu_bfn_any)

static mcnt_t mu_bfn_all(mu_t *frame) {
    mu_t pred = frame[0];
    mu_t iter = frame[1];
    if (!mu_isfn(pred)) {
        mu_error_arg(MU_KEY_ALL, 0x2, frame);
    }

    frame[0] = iter;
    mu_fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    frame[0] = MU_TRUE;
    while (mu_fn_next(iter, 0xf, frame)) {
        mu_fn_fcall(pred, 0xf1, frame);
        if (frame[0]) {
            mu_dec(frame[0]);
        } else {
            mu_fn_dec(pred);
            mu_fn_dec(iter);
            return 0;
        }
    }

    mu_fn_dec(pred);
    mu_fn_dec(iter);
    return 1;
}

MU_GEN_STR(mu_gen_key_all, "all")
MU_GEN_BFN(mu_gen_all, 0x2, mu_bfn_all)


// Other iterators/generators
static mcnt_t mu_bfn_range_step(mu_t scope, mu_t *frame) {
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

static mcnt_t mu_bfn_range(mu_t *frame) {
    if (!frame[1]) {
        frame[1] = frame[0];
        frame[0] = 0;
    }

    mu_t start = frame[0] ? frame[0] : mu_num_fromuint(0);
    mu_t stop  = frame[1] ? frame[1] : MU_INF;
    mu_t step  = frame[2];
    if (!mu_isnum(start) || !mu_isnum(stop) || (step && !mu_isnum(step))) {
        mu_error_arg(MU_KEY_RANGE, 0x3, frame);
    }

    if (!step) {
        step = mu_num_fromint(mu_num_cmp(start, stop) < 0 ? 1 : -1);
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_bfn_range_step,
            mu_buf_fromdata((mu_t[]){start, stop, step}, 3*sizeof(mu_t)));
    return 1;
}

MU_GEN_STR(mu_gen_key_range, "range")
MU_GEN_BFN(mu_gen_range, 0x3, mu_bfn_range)

static mcnt_t mu_bfn_repeat_step(mu_t scope, mu_t *frame) {
    mu_t i = mu_tbl_lookup(scope, mu_num_fromuint(1));
    if (mu_num_cmp(i, mu_num_fromuint(0)) <= 0) {
        return 0;
    }

    frame[0] = mu_tbl_lookup(scope, mu_num_fromuint(0));
    mu_tbl_insert(scope, mu_num_fromuint(1),
            mu_num_sub(i, mu_num_fromuint(1)));
    return 1;
}

static mcnt_t mu_bfn_repeat(mu_t *frame) {
    mu_t m     = frame[0];
    mu_t count = frame[1] ? frame[1] : MU_INF;
    if (!mu_isnum(count)) {
        mu_error_arg(MU_KEY_REPEAT, 0x2, frame);
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_bfn_repeat_step,
            mu_tbl_fromlist((mu_t[]){m, count}, 2));
    return 1;
}

MU_GEN_STR(mu_gen_key_repeat, "repeat")
MU_GEN_BFN(mu_gen_repeat, 0x2, mu_bfn_repeat)


// Iterator manipulation
static mcnt_t mu_bfn_zip_step(mu_t scope, mu_t *frame) {
    mu_t iters = mu_tbl_lookup(scope, mu_num_fromuint(1));

    if (!iters) {
        mu_t iteriter = mu_tbl_lookup(scope, mu_num_fromuint(0));
        iters = mu_tbl_create(0);

        while (mu_fn_next(iteriter, 0x1, frame)) {
            mu_tbl_insert(iters, mu_num_fromuint(mu_tbl_getlen(iters)),
                    mu_fn_call(MU_ITER, 0x11, frame[0]));
        }

        mu_fn_dec(iteriter);
        mu_tbl_insert(scope, mu_num_fromuint(1), mu_inc(iters));
    }

    mu_t acc = mu_tbl_create(mu_tbl_getlen(iters));
    mu_t iter;

    for (muint_t i = 0; mu_tbl_next(iters, &i, 0, &iter);) {
        mu_fn_fcall(iter, 0x0f, frame);
        mu_fn_dec(iter);

        mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        if (!m) {
            mu_dec(acc);
            mu_dec(frame[0]);
            mu_tbl_dec(iters);
            return 0;
        }
        mu_dec(m);

        acc = mu_tbl_concat(acc, frame[0], 0);
    }

    mu_tbl_dec(iters);
    frame[0] = acc;
    return 0xf;
}

static mcnt_t mu_bfn_zip(mu_t *frame) {
    mu_t iter;
    if (mu_tbl_getlen(frame[0]) == 0) {
        mu_errorf("no arguments passed to zip");
    } else if (mu_tbl_getlen(frame[0]) == 1) {
        iter = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        mu_dec(frame[0]);
    } else {
        iter = frame[0];
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_bfn_zip_step,
            mu_tbl_fromlist((mu_t[]){mu_fn_call(MU_ITER, 0x11, iter)}, 1));
    return 1;
}

MU_GEN_STR(mu_gen_key_zip, "zip")
MU_GEN_BFN(mu_gen_zip, 0xf, mu_bfn_zip)

static mcnt_t mu_bfn_chain_step(mu_t scope, mu_t *frame) {
    mu_t iter = mu_tbl_lookup(scope, mu_num_fromuint(1));

    if (iter) {
        mu_fn_fcall(iter, 0x0f, frame);
        mu_fn_dec(iter);

        mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        if (m) {
            mu_dec(m);
            return 0xf;
        }
        mu_dec(frame[0]);
    }

    mu_t iters = mu_tbl_lookup(scope, mu_num_fromuint(0));
    mu_fn_fcall(iters, 0x01, frame);
    mu_fn_dec(iters);
    if (frame[0]) {
        mu_tbl_insert(scope, mu_num_fromuint(1),
                mu_fn_call(MU_ITER, 0x11, frame[0]));
        return mu_bfn_chain_step(scope, frame);
    }

    return 0;
}

static mcnt_t mu_bfn_chain(mu_t *frame) {
    mu_t iter;
    if (mu_tbl_getlen(frame[0]) == 0) {
        mu_errorf("no arguments passed to chain");
    } else if (mu_tbl_getlen(frame[0]) == 1) {
        iter = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        mu_dec(frame[0]);
    } else {
        iter = frame[0];
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_bfn_chain_step,
            mu_tbl_fromlist((mu_t[]){mu_fn_call(MU_ITER, 0x11, iter)}, 1));
    return 1;
}

MU_GEN_STR(mu_gen_key_chain, "chain")
MU_GEN_BFN(mu_gen_chain, 0xf, mu_bfn_chain)

static mcnt_t mu_bfn_take_count_step(mu_t scope, mu_t *frame) {
    mu_t i = mu_tbl_lookup(scope, mu_num_fromuint(0));
    if (mu_num_cmp(i, mu_num_fromuint(0)) <= 0) {
        return 0;
    }

    mu_tbl_insert(scope, mu_num_fromuint(0),
            mu_num_sub(i, mu_num_fromuint(1)));
    mu_t iter = mu_tbl_lookup(scope, mu_num_fromuint(1));
    return mu_fn_tcall(iter, 0x0, frame);
}

static mcnt_t mu_bfn_take_while_step(mu_t scope, mu_t *frame) {
    mu_t iter = mu_tbl_lookup(scope, mu_num_fromuint(1));
    mu_fn_fcall(iter, 0x0f, frame);
    mu_fn_dec(iter);

    mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
    if (!m) {
        mu_dec(frame[0]);
        return 0;
    }
    mu_dec(m);

    m = mu_inc(frame[0]);
    mu_t cond = mu_tbl_lookup(scope, mu_num_fromuint(0));
    mu_fn_fcall(cond, 0xf1, frame);
    mu_fn_dec(cond);
    if (!frame[0]) {
        mu_tbl_dec(m);
        return 0;
    }
    mu_dec(frame[0]);
    frame[0] = m;
    return 0xf;
}

static mcnt_t mu_bfn_take(mu_t *frame) {
    mu_t m    = frame[0];
    mu_t iter = frame[1];
    if (!mu_isnum(m) && !mu_isfn(m)) {
        mu_error_arg(MU_KEY_TAKE, 0x2, frame);
    }

    frame[0] = iter;
    mu_fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    mcnt_t (*mu_bfn_take_step)(mu_t scope, mu_t *frame);
    if (mu_isnum(m)) {
        mu_bfn_take_step = mu_bfn_take_count_step;
    } else {
        mu_bfn_take_step = mu_bfn_take_while_step;
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_bfn_take_step,
            mu_tbl_fromlist((mu_t[]){m, iter}, 2));
    return 1;
}

MU_GEN_STR(mu_gen_key_take, "take")
MU_GEN_BFN(mu_gen_take, 0x2, mu_bfn_take)

static mcnt_t mu_bfn_drop_count_step(mu_t scope, mu_t *frame) {
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

static mcnt_t mu_bfn_drop_while_step(mu_t scope, mu_t *frame) {
    mu_t iter = mu_tbl_lookup(scope, mu_num_fromuint(1));
    mu_t cond = mu_tbl_lookup(scope, mu_num_fromuint(0));

    if (cond) {
        while (mu_fn_next(iter, 0xf, frame)) {
            mu_t m = mu_tbl_inc(frame[0]);
            mu_fn_fcall(cond, 0xf1, frame);
            if (!frame[0]) {
                mu_fn_dec(iter);
                mu_fn_dec(cond);
                frame[0] = m;
                mu_tbl_insert(scope, mu_num_fromuint(0), 0);
                return 0xf;
            }
            mu_dec(m);
        }

        mu_fn_dec(iter);
        mu_fn_dec(cond);
        return 0;
    }

    return mu_fn_tcall(iter, 0x0, frame);
}

static mcnt_t mu_bfn_drop(mu_t *frame) {
    mu_t m    = frame[0];
    mu_t iter = frame[1];
    if (!mu_isnum(m) && !mu_isfn(m)) {
        mu_error_arg(MU_KEY_TAKE, 0x2, frame);
    }

    frame[0] = iter;
    mu_fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    mcnt_t (*mu_bfn_drop_step)(mu_t scope, mu_t *frame);
    if (mu_isnum(m)) {
        mu_bfn_drop_step = mu_bfn_drop_count_step;
    } else {
        mu_bfn_drop_step = mu_bfn_drop_while_step;
    }

    frame[0] = mu_fn_fromsbfn(0x0, mu_bfn_drop_step,
            mu_tbl_fromlist((mu_t[]){m, iter}, 2));
    return 1;
}

MU_GEN_STR(mu_gen_key_drop, "drop")
MU_GEN_BFN(mu_gen_drop, 0x2, mu_bfn_drop)


// Iterator ordering
static mcnt_t mu_bfn_min(mu_t *frame) {
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

    enum mtype type = mu_gettype(min);
    mint_t (*cmp)(mu_t, mu_t) = mu_attr_cmp[type];
    if (!cmp) {
        mu_errorf("unable to compare %r", min);
    }

    while (mu_fn_next(iter, 0xf, frame)) {
        mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        if (mu_gettype(m) != type) {
            mu_errorf("unable to compare %r and %r", min, m);
        }

        if (cmp(m, min) < 0) {
            mu_dec(min);
            mu_tbl_dec(min_frame);
            min = m;
            min_frame = frame[0];
        } else {
            mu_dec(m);
            mu_tbl_dec(frame[0]);
        }
    }

    mu_fn_dec(iter);
    mu_dec(min);
    frame[0] = min_frame;
    return 0xf;
}

MU_GEN_STR(mu_gen_key_min, "min")
MU_GEN_BFN(mu_gen_min, 0xf, mu_bfn_min)

static mcnt_t mu_bfn_max(mu_t *frame) {
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

    enum mtype type = mu_gettype(max);
    mint_t (*cmp)(mu_t, mu_t) = mu_attr_cmp[type];
    if (!cmp) {
        mu_errorf("unable to compare %r", max);
    }

    while (mu_fn_next(iter, 0xf, frame)) {
        mu_t m = mu_tbl_lookup(frame[0], mu_num_fromuint(0));
        if (mu_gettype(m) != type) {
            mu_errorf("unable to compare %r and %r", max, m);
        }

        if (cmp(m, max) >= 0) {
            mu_dec(max);
            mu_tbl_dec(max_frame);
            max = m;
            max_frame = frame[0];
        } else {
            mu_dec(m);
            mu_tbl_dec(frame[0]);
        }
    }

    mu_fn_dec(iter);
    mu_dec(max);
    frame[0] = max_frame;
    return 0xf;
}

MU_GEN_STR(mu_gen_key_max, "max")
MU_GEN_BFN(mu_gen_max, 0xf, mu_bfn_max)

static mcnt_t mu_bfn_reverse_step(mu_t scope, mu_t *frame) {
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

static mcnt_t mu_bfn_reverse(mu_t *frame) {
    mu_fn_fcall(MU_ITER, 0x11, frame);
    mu_t iter = frame[0];
    mu_t store = mu_tbl_create(0);

    while (mu_fn_next(iter, 0xf, frame)) {
        mu_tbl_insert(store, mu_num_fromuint(mu_tbl_getlen(store)), frame[0]);
    }

    mu_fn_dec(iter);

    frame[0] = mu_fn_fromsbfn(0x0, mu_bfn_reverse_step,
            mu_tbl_fromlist((mu_t[]){
                store, mu_num_fromuint(mu_tbl_getlen(store)-1)
            }, 2));
    return 1;
}

MU_GEN_STR(mu_gen_key_reverse, "reverse")
MU_GEN_BFN(mu_gen_reverse, 0x1, mu_bfn_reverse)

// Simple iterative merge sort
static void mu_fn_merge_sort(mu_t elems) {
    // Uses arrays (first elem, frame) to keep from
    // looking up the elem each time.
    // We use two arrays so we can just flip each merge.
    muint_t len = mu_tbl_getlen(elems);
    mu_t (*a)[2] = mu_alloc(len*sizeof(mu_t[2]));
    mu_t (*b)[2] = mu_alloc(len*sizeof(mu_t[2]));

    enum mtype type = MTNUM;

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
                if (y >= slice || i+slice+y >= len ||
                    (x < slice && cmp(a[i+x][0], a[i+slice+y][0]) <= 0)) {
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

static mcnt_t mu_bfn_sort_step(mu_t scope, mu_t *frame) {
    mu_t store = mu_tbl_lookup(scope, mu_num_fromuint(0));
    muint_t i = mu_num_getuint(mu_tbl_lookup(scope, mu_num_fromuint(1)));

    bool next = mu_tbl_next(store, &i, 0, &frame[0]);
    mu_dec(store);
    mu_tbl_insert(scope, mu_num_fromuint(1), mu_num_fromuint(i));
    return next ? 0xf : 0;
}

static mcnt_t mu_bfn_sort(mu_t *frame) {
    mu_fn_fcall(MU_ITER, 0x11, frame);
    mu_t iter = frame[0];
    mu_t store = mu_tbl_create(0);

    while (mu_fn_next(iter, 0xf, frame)) {
        mu_tbl_insert(store, mu_num_fromuint(mu_tbl_getlen(store)), frame[0]);
    }

    mu_dec(iter);

    mu_fn_merge_sort(store);

    frame[0] = mu_fn_fromsbfn(0x0, mu_bfn_sort_step,
            mu_tbl_fromlist((mu_t[]){store, mu_num_fromuint(0)}, 2));
    return 1;
}

MU_GEN_STR(mu_gen_key_sort, "sort")
MU_GEN_BFN(mu_gen_sort, 0x1, mu_bfn_sort)


// Builtins table
MU_GEN_TBL(mu_gen_builtins, {
    // Constants
    { mu_gen_key_true,      mu_gen_true },
    { mu_gen_key_inf,       mu_gen_inf },
    { mu_gen_key_e,         mu_gen_e },
    { mu_gen_key_pi,        mu_gen_pi },
    { mu_gen_key_id,        mu_gen_id },

    // Type casts
    { mu_gen_key_num,       mu_gen_num },
    { mu_gen_key_str,       mu_gen_str },
    { mu_gen_key_tbl,       mu_gen_tbl },
    { mu_gen_key_fn2,       mu_gen_fn },

    // Logic operations
    { mu_gen_key_not,       mu_gen_not },
    { mu_gen_key_eq,        mu_gen_eq },
    { mu_gen_key_neq,       mu_gen_neq },
    { mu_gen_key_is,        mu_gen_is },
    { mu_gen_key_lt,        mu_gen_lt },
    { mu_gen_key_lte,       mu_gen_lte },
    { mu_gen_key_gt,        mu_gen_gt },
    { mu_gen_key_gte,       mu_gen_gte },

    // Arithmetic operations
    { mu_gen_key_add,       mu_gen_add },
    { mu_gen_key_sub,       mu_gen_sub },
    { mu_gen_key_mul,       mu_gen_mul },
    { mu_gen_key_div,       mu_gen_div },

    { mu_gen_key_abs,       mu_gen_abs },
    { mu_gen_key_floor,     mu_gen_floor },
    { mu_gen_key_ceil,      mu_gen_ceil },
    { mu_gen_key_idiv,      mu_gen_idiv },
    { mu_gen_key_mod,       mu_gen_mod },

    { mu_gen_key_pow,       mu_gen_pow },
    { mu_gen_key_log,       mu_gen_log },

    { mu_gen_key_cos,       mu_gen_cos },
    { mu_gen_key_acos,      mu_gen_acos },
    { mu_gen_key_sin,       mu_gen_sin },
    { mu_gen_key_asin,      mu_gen_asin },
    { mu_gen_key_tan,       mu_gen_tan },
    { mu_gen_key_atan,      mu_gen_atan },

    // Bitwise/Set operations
    { mu_gen_key_and2,      mu_gen_and },
    { mu_gen_key_or2,       mu_gen_or },
    { mu_gen_key_xor,       mu_gen_xor },
    { mu_gen_key_diff,      mu_gen_diff },

    { mu_gen_key_shl,       mu_gen_shl },
    { mu_gen_key_shr,       mu_gen_shr },

    // String representation
    { mu_gen_key_parse,     mu_gen_parse },
    { mu_gen_key_repr,      mu_gen_repr },

    { mu_gen_key_bin,       mu_gen_bin },
    { mu_gen_key_oct,       mu_gen_oct },
    { mu_gen_key_hex,       mu_gen_hex },

    // Data structure operations
    { mu_gen_key_len,       mu_gen_len },
    { mu_gen_key_tail,      mu_gen_tail },

    { mu_gen_key_push,      mu_gen_push },
    { mu_gen_key_pop,       mu_gen_pop },

    { mu_gen_key_concat,    mu_gen_concat },
    { mu_gen_key_subset,    mu_gen_subset },

    // String operations
    { mu_gen_key_find,      mu_gen_find },
    { mu_gen_key_replace,   mu_gen_replace },
    { mu_gen_key_split,     mu_gen_split },
    { mu_gen_key_join,      mu_gen_join },
    { mu_gen_key_pad,       mu_gen_pad },
    { mu_gen_key_strip,     mu_gen_strip },

    // Function operations
    { mu_gen_key_bind,      mu_gen_bind },
    { mu_gen_key_comp,      mu_gen_comp },

    { mu_gen_key_map,       mu_gen_map },
    { mu_gen_key_filter,    mu_gen_filter },
    { mu_gen_key_reduce,    mu_gen_reduce },

    { mu_gen_key_any,       mu_gen_any },
    { mu_gen_key_all,       mu_gen_all },

    // Iterators and generators
    { mu_gen_key_iter,      mu_gen_iter },
    { mu_gen_key_pairs,     mu_gen_pairs },

    { mu_gen_key_range,     mu_gen_range },
    { mu_gen_key_repeat,    mu_gen_repeat },
    { mu_gen_key_seed,      mu_gen_seed },

    // Iterator manipulation
    { mu_gen_key_zip,       mu_gen_zip },
    { mu_gen_key_chain,     mu_gen_chain },

    { mu_gen_key_take,      mu_gen_take },
    { mu_gen_key_drop,      mu_gen_drop },

    // Iterator ordering
    { mu_gen_key_min,       mu_gen_min },
    { mu_gen_key_max,       mu_gen_max },

    { mu_gen_key_reverse,   mu_gen_reverse },
    { mu_gen_key_sort,      mu_gen_sort },

    // System operations
    { mu_gen_key_error,     mu_gen_error },
    { mu_gen_key_print,     mu_gen_print },
    { mu_gen_key_import,    mu_gen_import },
})
