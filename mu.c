#include "mu.h"

#include "sys.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"
#include "parse.h"
#include "vm.h"


// Constants
MSTR(mu_gen_key_true,  "true")      MUINT(mu_gen_true, 1)
MSTR(mu_gen_key_false, "false")     // nil


// Mu type destructors
extern void str_destroy(mu_t);
extern void buf_destroy(mu_t);
extern void tbl_destroy(mu_t);
extern void fn_destroy(mu_t);

static void (*const mu_attr_destroy[8])(mu_t) = {
    [MTSTR] = str_destroy,
    [MTBUF] = buf_destroy,
    [MTTBL] = tbl_destroy,
    [MTFN]  = fn_destroy,
};

void mu_destroy(mu_t m) {
    mu_attr_destroy[mu_type(m)](m);
}

// Mu type comparisons
static mint_t (*const mu_attr_cmp[8])(mu_t, mu_t) = {
    [MTNUM] = num_cmp,
    [MTSTR] = str_cmp,
};

// Mu type iterators
static mu_t fn_iter(mu_t m) { return m;}

static mu_t (*const mu_attr_iter[8])(mu_t) = {
    [MTSTR] = str_iter,
    [MTTBL] = tbl_iter,
    [MTFN]  = fn_iter,
};

// Mu type representations
MSTR(mu_gen_key_cdata, "cdata")

static mu_t (*const mu_attr_name[8])(void) = {
    [MTNIL] = mu_gen_key_nil,
    [MTNUM] = mu_gen_key_num,
    [MTSTR] = mu_gen_key_str,
    [MTTBL] = mu_gen_key_tbl,
    [MTFN]  = mu_gen_key_fn,
    [MTBUF] = mu_gen_key_cdata,
    [MTCD]  = mu_gen_key_cdata
};

static mu_t nil_repr(mu_t m) {
    return MU_KW_NIL;
}

static mu_t (*const mu_attr_repr[8])(mu_t) = {
    [MTNIL] = nil_repr,
    [MTNUM] = num_repr,
    [MTSTR] = str_repr,
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
            frame[i] = tbl_lookup(t, muint(i));
        }

        tbl_dec(t);
    } else if (sc != 0xf) {
        mu_t t = tbl_create(sc);

        for (muint_t i = 0; i < sc; i++) {
            tbl_insert(t, muint(i), frame[i]);
        }

        *frame = t;
    }
}


// System operations
mu_noreturn mu_error(const char *s, muint_t n) {
    sys_error(s, n);
    mu_unreachable;
}

mu_noreturn mu_verrorf(const char *f, va_list args) {
    mu_t b = buf_create(0);
    muint_t n = 0;
    buf_vformat(&b, &n, f, args);
    mu_error(buf_data(b), n);
}

mu_noreturn mu_errorf(const char *f, ...) {
    va_list args;
    va_start(args, f);
    mu_verrorf(f, args);
}

static mcnt_t mu_bfn_error(mu_t *frame) {
    mu_t b = buf_create(0);
    muint_t n = 0;
    mu_t v;

    for (muint_t i = 0; tbl_next(frame[0], &i, 0, &v);) {
        buf_format(&b, &n, "%m", v);
    }

    mu_error(buf_data(b), n);
}

MSTR(mu_gen_key_error, "error")
MBFN(mu_gen_error, 0xf, mu_bfn_error)

void mu_print(const char *s, muint_t n) {
    sys_print(s, n);
}

void mu_vprintf(const char *f, va_list args) {
    mu_t b = buf_create(0);
    muint_t n = 0;
    buf_vformat(&b, &n, f, args);
    mu_print(buf_data(b), n);
    buf_dec(b);
}

void mu_printf(const char *f, ...) {
    va_list args;
    va_start(args, f);
    mu_vprintf(f, args);
    va_end(args);
}

static mcnt_t mu_bfn_print(mu_t *frame) {
    mu_t b = buf_create(0);
    muint_t n = 0;
    mu_t v;

    for (muint_t i = 0; tbl_next(frame[0], &i, 0, &v);) {
        buf_format(&b, &n, "%m", v);
    }

    mu_print(buf_data(b), n);
    buf_dec(b);
    tbl_dec(frame[0]);
    return 0;
}

MSTR(mu_gen_key_print, "print")
MBFN(mu_gen_print, 0xf, mu_bfn_print)

static mcnt_t mu_bfn_import(mu_t *frame) {
    mu_t name = frame[0];
    if (!mu_isstr(name)) {
        mu_error_arg(MU_KEY_IMPORT, 0x1, frame);
    }

    static mu_t import_history = 0;
    if (!import_history) {
        import_history = tbl_create(0);
    }

    mu_t module = tbl_lookup(import_history, str_inc(name));
    if (module) {
        str_dec(name);
        frame[0] = module;
        return 1;
    }

    module = sys_import(str_inc(name));
    tbl_insert(import_history, name, mu_inc(module));
    frame[0] = module;
    return 1;
}

MSTR(mu_gen_key_import, "import")
MBFN(mu_gen_import, 0x1, mu_bfn_import)


// Evaluation and entry into Mu
void mu_feval(const char *s, muint_t n, mu_t scope, mcnt_t fc, mu_t *frame) {
    struct code *c = mu_compile(s, n);
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
    mu_t message = buf_create(0);
    muint_t n = 0;

    buf_format(&message, &n, "invalid argument in %m(", name);

    if (fc == 0xf) {
        buf_format(&message, &n, "..");
        fc = 1;
    }

    for (muint_t i = 0; i < fc; i++) {
        buf_format(&message, &n, "%nr%c ",
                frame[i], 0, (i != fc-1) ? ',' : ')');
    }

    mu_errorf("%ns", buf_data(message), n);
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

MSTR(mu_gen_key_not, "!")
MBFN(mu_gen_not, 0x1, mu_bfn_not)

static mcnt_t mu_bfn_eq(mu_t *frame) {
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = (frame[0] == frame[1]) ? MU_TRUE : MU_FALSE;
    return 1;
}

MSTR(mu_gen_key_eq, "==")
MBFN(mu_gen_eq,  0x2, mu_bfn_eq)

static mcnt_t mu_bfn_neq(mu_t *frame) {
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    frame[0] = (frame[0] != frame[1]) ? MU_TRUE : MU_FALSE;
    return 1;
}

MSTR(mu_gen_key_neq, "!=")
MBFN(mu_gen_neq, 0x2, mu_bfn_neq)

static mcnt_t mu_bfn_lt(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (mu_type(a) != mu_type(b) || !mu_attr_cmp[mu_type(a)]) {
        mu_error_op(MU_KEY_LT, 0x2, frame);
    }

    frame[0] = (mu_attr_cmp[mu_type(a)](a, b) < 0) ? MU_TRUE : MU_FALSE;
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    return 1;
}

MSTR(mu_gen_key_lt, "<")
MBFN(mu_gen_lt,  0x2, mu_bfn_lt)

static mcnt_t mu_bfn_lte(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (mu_type(a) != mu_type(b) || !mu_attr_cmp[mu_type(a)]) {
        mu_error_op(MU_KEY_LTE, 0x2, frame);
    }

    frame[0] = (mu_attr_cmp[mu_type(a)](a, b) <= 0) ? MU_TRUE : MU_FALSE;
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    return 1;
}

MSTR(mu_gen_key_lte, "<=")
MBFN(mu_gen_lte, 0x2, mu_bfn_lte)

static mcnt_t mu_bfn_gt(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (mu_type(a) != mu_type(b) || !mu_attr_cmp[mu_type(a)]) {
        mu_error_op(MU_KEY_GT, 0x2, frame);
    }

    frame[0] = (mu_attr_cmp[mu_type(a)](a, b) > 0) ? MU_TRUE : MU_FALSE;
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    return 1;
}

MSTR(mu_gen_key_gt, ">")
MBFN(mu_gen_gt,  0x2, mu_bfn_gt)

static mcnt_t mu_bfn_gte(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];
    if (mu_type(a) != mu_type(b) || !mu_attr_cmp[mu_type(a)]) {
        mu_error_op(MU_KEY_GTE, 0x2, frame);
    }

    frame[0] = (mu_attr_cmp[mu_type(a)](a, b) >= 0) ? MU_TRUE : MU_FALSE;
    mu_dec(frame[0]);
    mu_dec(frame[1]);
    return 1;
}

MSTR(mu_gen_key_gte, ">=")
MBFN(mu_gen_gte, 0x2, mu_bfn_gte)

static mcnt_t mu_bfn_is(mu_t *frame) {
    mu_t m    = frame[0];
    mu_t type = frame[1];
    mu_dec(m);
    mu_dec(type);

    frame[0] = MU_TRUE;
    switch (mu_type(m)) {
        case MTNIL: return !type;
        case MTNUM: return type == MU_NUM;
        case MTSTR: return type == MU_STR;
        case MTTBL: return type == MU_TBL;
        case MTFN:  return type == MU_FN;
        default:    return false;
    }
}

MSTR(mu_gen_key_is, "is")
MBFN(mu_gen_is, 0x2, mu_bfn_is)


// String representation
static mcnt_t mu_bfn_parse(mu_t *frame) {
    mu_t s = frame[0];
    if (!mu_isstr(s)) {
        mu_error_arg(MU_KEY_PARSE, 0x1, frame);
    }

    frame[0] = mu_parse((const char *)str_data(s), str_len(s));
    mu_dec(s);
    return 1;
}

MSTR(mu_gen_key_parse, "parse")
MBFN(mu_gen_parse, 0x1, mu_bfn_parse)

static mcnt_t mu_bfn_repr(mu_t *frame) {
    mu_t m     = frame[0];
    mu_t depth = frame[1] ? frame[1] : muint(1);
    if (!mu_isnum(depth)) {
        mu_error_arg(MU_KEY_REPR, 0x2, frame);
    }

    if (mu_attr_repr[mu_type(m)]) {
        frame[0] = mu_attr_repr[mu_type(m)](m);
        return 1;
    } else if (mu_istbl(m) && num_cmp(depth, muint(0)) > 0) {
        frame[0] = tbl_dump(m, depth);
        return 1;
    } else {
        mu_dec(m);
        frame[0] = mstr("<%m 0x%wx>",
                mu_attr_name[mu_type(m)](),
                (muint_t)m & ~7);
        return 1;
    }
}

MSTR(mu_gen_key_repr, "repr")
MBFN(mu_gen_repr, 0x2, mu_bfn_repr)

static mcnt_t mu_bfn_bin(mu_t *frame) {
    mu_t m = frame[0];

    if (mu_isnum(m)) {
        frame[0] = num_bin(m);
        return 1;
    } else if (mu_isstr(m)) {
        frame[0] = str_bin(m);
        return 1;
    } else {
        mu_error_arg(MU_KEY_BIN, 0x1, frame);
    }
}

MSTR(mu_gen_key_bin, "bin")
MBFN(mu_gen_bin, 0x1, mu_bfn_bin)

static mcnt_t mu_bfn_oct(mu_t *frame) {
    mu_t m = frame[0];

    if (mu_isnum(m)) {
        frame[0] = num_oct(m);
        return 1;
    } else if (mu_isstr(m)) {
        frame[0] = str_oct(m);
        return 1;
    } else {
        mu_error_arg(MU_KEY_OCT, 0x1, frame);
    }
}

MSTR(mu_gen_key_oct, "oct")
MBFN(mu_gen_oct, 0x1, mu_bfn_oct)

static mcnt_t mu_bfn_hex(mu_t *frame) {
    mu_t m = frame[0];

    if (mu_isnum(m)) {
        frame[0] = num_hex(m);
        return 1;
    } else if (mu_isstr(m)) {
        frame[0] = str_hex(m);
        return 1;
    } else {
        mu_error_arg(MU_KEY_HEX, 0x1, frame);
    }
}

MSTR(mu_gen_key_hex, "hex")
MBFN(mu_gen_hex, 0x1, mu_bfn_hex)


// Data structure operations
static mint_t mu_clamp(mu_t n, mint_t lower, mint_t upper) {
    mu_assert(mu_isnum(n));

    if (num_cmp(n, mint(lower)) < 0) {
        return lower;
    } else if (num_cmp(n, mint(upper)) > 0) {
        return upper;
    } else {
        return num_int(n);
    }
}

static mcnt_t mu_bfn_len(mu_t *frame) {
    mu_t a = frame[0];

    if (mu_isstr(a)) {
        frame[0] = muint(str_len(a));
        mu_dec(a);
        return 1;
    } else if (mu_istbl(a)) {
        frame[0] = muint(tbl_len(a));
        mu_dec(a);
        return 1;
    } else {
        mu_error_arg(MU_KEY_LEN, 0x1, frame);
    }
}

MSTR(mu_gen_key_len, "len")
MBFN(mu_gen_len, 0x1, mu_bfn_len)

static mcnt_t mu_bfn_concat(mu_t *frame) {
    mu_t a      = frame[0];
    mu_t b      = frame[1];
    mu_t offset = frame[2];
    if (offset && !mu_isnum(offset)) {
        mu_error_op(MU_KEY_CONCAT, 0x3, frame);
    }

    if (mu_isstr(a) && mu_isstr(b)) {
        frame[0] = str_concat(a, b);
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = tbl_concat(a, b, offset);
        return 1;
    } else {
        mu_error_op(MU_KEY_CONCAT, 0x3, frame);
    }
}

MSTR(mu_gen_key_concat, "++")
MBFN(mu_gen_concat, 0x3, mu_bfn_concat)

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
        frame[0] = str_subset(a, loweri, upperi);
        return 1;
    } else if (mu_istbl(a)) {
        frame[0] = tbl_subset(a, loweri, upperi);
        return 1;
    } else {
        mu_error_arg(MU_KEY_SUBSET, 0x3, frame);
    }
}

MSTR(mu_gen_key_subset, "sub")
MBFN(mu_gen_subset, 0x3, mu_bfn_subset)

static mcnt_t mu_bfn_push(mu_t *frame) {
    mu_t t = frame[0];
    mu_t v = frame[1];
    mu_t i = frame[2];
    if (!mu_istbl(t) || (i && !mu_isnum(i))) {
        mu_error_arg(MU_KEY_PUSH, 0x3, frame);
    }

    mint_t ii;
    if (!i) {
        ii = tbl_len(t);
    } else {
        ii = mu_clamp(i, -(mint_t)(mlen_t)-1, (mlen_t)-1);
    }

    tbl_push(t, v, ii);
    return 0;
}

MSTR(mu_gen_key_push, "push")
MBFN(mu_gen_push, 0x3, mu_bfn_push)

static mcnt_t mu_bfn_pop(mu_t *frame) {
    mu_t t = frame[0];
    mu_t i = frame[1];
    if (!mu_istbl(t) || (i && !mu_isnum(i))) {
        mu_error_arg(MU_KEY_POP, 0x2, frame);
    }

    mint_t ii;
    if (!i) {
        ii = tbl_len(t) - 1;
    } else {
        ii = mu_clamp(i, -(mint_t)(mlen_t)-1, (mlen_t)-1);
    }

    frame[0] = tbl_pop(t, ii);
    return 1;
}

MSTR(mu_gen_key_pop, "pop")
MBFN(mu_gen_pop, 0x2, mu_bfn_pop)

static mcnt_t mu_bfn_and(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];

    if (mu_isnum(a) && mu_isnum(b)) {
        frame[0] = num_and(a, b);
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = tbl_and(a, b);
        return 1;
    } else {
        mu_error_op(MU_KEY_AND2, 0x2, frame);
    }
}

MSTR(mu_gen_key_and2, "&")
MBFN(mu_gen_and, 0x2, mu_bfn_and)

static mcnt_t mu_bfn_or(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];

    if (mu_isnum(a) && mu_isnum(b)) {
        frame[0] = num_or(a, b);
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = tbl_or(a, b);
        return 1;
    } else {
        mu_error_op(MU_KEY_OR2, 0x2, frame);
    }
}

MSTR(mu_gen_key_or2, "|")
MBFN(mu_gen_or, 0x2, mu_bfn_or)

static mcnt_t mu_bfn_xor(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];

    if (mu_isnum(a) && !b) {
        frame[0] = num_not(a);
        return 1;
    } else if (mu_isnum(a) && mu_isnum(b)) {
        frame[0] = num_xor(a, b);
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = tbl_xor(a, b);
        return 1;
    } else {
        mu_error_op(MU_KEY_XOR, 0x2, frame);
    }
}

MSTR(mu_gen_key_xor, "~")
MBFN(mu_gen_xor, 0x2, mu_bfn_xor)

static mcnt_t mu_bfn_diff(mu_t *frame) {
    mu_t a = frame[0];
    mu_t b = frame[1];

    if (mu_isnum(a) && mu_isnum(b)) {
        frame[0] = num_xor(a, num_not(b));
        return 1;
    } else if (mu_istbl(a) && mu_istbl(b)) {
        frame[0] = tbl_diff(a, b);
        return 1;
    } else {
        mu_error_op(MU_KEY_DIFF, 0x2, frame);
    }
}

MSTR(mu_gen_key_diff, "&~")
MBFN(mu_gen_diff, 0x2, mu_bfn_diff)


// Iterators and generators
static mcnt_t mu_bfn_iter(mu_t *frame) {
    mu_t m = frame[0];
    if (!mu_attr_iter[mu_type(m)]) {
        mu_error_cast(MU_KEY_ITER, m);
    }

    frame[0] = mu_attr_iter[mu_type(m)](m);
    return 1;
}

MSTR(mu_gen_key_iter, "iter")
MBFN(mu_gen_iter, 0x1, mu_bfn_iter)

static mcnt_t fn_pairs(mu_t *frame) {
    mu_t m = frame[0];

    if (mu_istbl(m)) {
        frame[0] = tbl_pairs(m);
        return 1;
    } else if (mu_attr_iter[mu_type(m)]) {
        frame[0] = fn_call(MU_RANGE, 0x01);
        frame[1] = mu_attr_iter[mu_type(m)](m);
        return fn_tcall(MU_ZIP, 0x2, frame);
    } else {
        mu_error_cast(MU_KEY_PAIRS, m);
    }
}

MSTR(mu_gen_key_pairs, "pairs")
MBFN(mu_gen_pairs, 0x1, fn_pairs)


// Functions over iterators
static mcnt_t fn_map_step(mu_t scope, mu_t *frame) {
    mu_t f = tbl_lookup(scope, muint(0));
    mu_t i = tbl_lookup(scope, muint(1));

    while (fn_next(i, 0xf, frame)) {
        fn_fcall(f, 0xff, frame);
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (m) {
            mu_dec(m);
            fn_dec(f);
            fn_dec(i);
            return 0xf;
        }
        tbl_dec(frame[0]);
    }

    fn_dec(f);
    fn_dec(i);
    return 0;
}

static mcnt_t fn_map(mu_t *frame) {
    mu_t f    = frame[0];
    mu_t iter = frame[1];
    if (!mu_isfn(f)) {
        mu_error_arg(MU_KEY_MAP, 0x2, frame);
    }

    frame[0] = iter;
    fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    frame[0] = msbfn(0, fn_map_step, mlist({f, iter}));
    return 1;
}

MSTR(mu_gen_key_map, "map")
MBFN(mu_gen_map, 0x2, fn_map)

static mcnt_t fn_filter_step(mu_t scope, mu_t *frame) {
    mu_t f = tbl_lookup(scope, muint(0));
    mu_t i = tbl_lookup(scope, muint(1));

    while (fn_next(i, 0xf, frame)) {
        mu_t m = tbl_inc(frame[0]);
        fn_fcall(f, 0xf1, frame);
        if (frame[0]) {
            mu_dec(frame[0]);
            frame[0] = m;
            mu_dec(f);
            mu_dec(i);
            return 0xf;
        }
        tbl_dec(m);
    }

    fn_dec(f);
    fn_dec(i);
    return 0;
}

static mcnt_t fn_filter(mu_t *frame) {
    mu_t f    = frame[0];
    mu_t iter = frame[1];
    if (!mu_isfn(f)) {
        mu_error_arg(MU_KEY_FILTER, 0x2, frame);
    }

    frame[0] = iter;
    fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    frame[0] = msbfn(0, fn_filter_step, mlist({f, iter}));
    return 1;
}

MSTR(mu_gen_key_filter, "filter")
MBFN(mu_gen_filter, 0x2, fn_filter)

static mcnt_t fn_reduce(mu_t *frame) {
    mu_t f    = tbl_pop(frame[0], 0);
    mu_t iter = tbl_pop(frame[0], 0);
    mu_t acc  = frame[0];
    if (!mu_isfn(f)) {
        mu_error_arg(MU_KEY_REDUCE, 0x3, (mu_t[]){f, iter, acc});
    }

    frame[0] = iter;
    fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    if (tbl_len(acc) == 0) {
        mu_dec(acc);
        fn_fcall(iter, 0x0f, frame);
        acc = frame[0];
    }

    while (fn_next(iter, 0xf, frame)) {
        frame[0] = tbl_concat(acc, frame[0], 0);
        fn_fcall(f, 0xff, frame);
        acc = frame[0];
    }

    fn_dec(f);
    fn_dec(iter);
    frame[0] = acc;
    return 0xf;
}

MSTR(mu_gen_key_reduce, "reduce")
MBFN(mu_gen_reduce, 0xf, fn_reduce)

static mcnt_t fn_any(mu_t *frame) {
    mu_t pred = frame[0];
    mu_t iter = frame[1];
    if (!mu_isfn(pred)) {
        mu_error_arg(MU_KEY_ANY, 0x2, frame);
    }

    frame[0] = iter;
    fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    frame[0] = MU_TRUE;
    while (fn_next(iter, 0xf, frame)) {
        fn_fcall(pred, 0xf1, frame);
        if (frame[0]) {
            mu_dec(frame[0]);
            mu_dec(pred);
            mu_dec(iter);
            return 1;
        }
    }

    fn_dec(pred);
    fn_dec(iter);
    return 0;
}

MSTR(mu_gen_key_any, "any")
MBFN(mu_gen_any, 0x2, fn_any)

static mcnt_t fn_all(mu_t *frame) {
    mu_t pred = frame[0];
    mu_t iter = frame[1];
    if (!mu_isfn(pred)) {
        mu_error_arg(MU_KEY_ALL, 0x2, frame);
    }

    frame[0] = iter;
    fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    frame[0] = MU_TRUE;
    while (fn_next(iter, 0xf, frame)) {
        fn_fcall(pred, 0xf1, frame);
        if (frame[0]) {
            mu_dec(frame[0]);
        } else {
            fn_dec(pred);
            fn_dec(iter);
            return 0;
        }
    }

    fn_dec(pred);
    fn_dec(iter);
    return 1;
}

MSTR(mu_gen_key_all, "all")
MBFN(mu_gen_all, 0x2, fn_all)


// Other iterators/generators
static mcnt_t fn_range_step(mu_t scope, mu_t *frame) {
    mu_t *a = buf_data(scope);

    if ((num_cmp(a[2], muint(0)) > 0 && num_cmp(a[0], a[1]) >= 0) ||
        (num_cmp(a[2], muint(0)) < 0 && num_cmp(a[0], a[1]) <= 0)) {
        return 0;
    }

    frame[0] = a[0];
    a[0] = num_add(a[0], a[2]);
    return 1;
}

static mcnt_t fn_range(mu_t *frame) {
    if (!frame[1]) {
        frame[1] = frame[0];
        frame[0] = 0;
    }

    mu_t start = frame[0] ? frame[0] : muint(0);
    mu_t stop  = frame[1] ? frame[1] : MU_INF;
    mu_t step  = frame[2];
    if (!mu_isnum(start) || !mu_isnum(stop) || (step && !mu_isnum(step))) {
        mu_error_arg(MU_KEY_RANGE, 0x3, frame);
    }

    if (!step) {
        step = mint(num_cmp(start, stop) < 0 ? 1 : -1);
    }

    frame[0] = msbfn(0x0, fn_range_step,
            mbuf((mu_t[]){start, stop, step}, 3*sizeof(mu_t)));
    return 1;
}

MSTR(mu_gen_key_range, "range")
MBFN(mu_gen_range, 0x3, fn_range)

static mcnt_t fn_repeat_step(mu_t scope, mu_t *frame) {
    mu_t i = tbl_lookup(scope, muint(1));
    if (num_cmp(i, muint(0)) <= 0) {
        return 0;
    }

    frame[0] = tbl_lookup(scope, muint(0));
    tbl_insert(scope, muint(1), num_sub(i, muint(1)));
    return 1;
}

static mcnt_t fn_repeat(mu_t *frame) {
    mu_t m     = frame[0];
    mu_t count = frame[1] ? frame[1] : MU_INF;
    if (!mu_isnum(count)) {
        mu_error_arg(MU_KEY_REPEAT, 0x2, frame);
    }

    frame[0] = msbfn(0x0, fn_repeat_step, mlist({m, count}));
    return 1;
}

MSTR(mu_gen_key_repeat, "repeat")
MBFN(mu_gen_repeat, 0x2, fn_repeat)


// Iterator manipulation
static mcnt_t fn_zip_step(mu_t scope, mu_t *frame) {
    mu_t iters = tbl_lookup(scope, muint(1));

    if (!iters) {
        mu_t iteriter = tbl_lookup(scope, muint(0));
        iters = tbl_create(0);

        while (fn_next(iteriter, 0x1, frame)) {
            tbl_insert(iters, muint(tbl_len(iters)),
                    fn_call(MU_ITER, 0x11, frame[0]));
        }

        fn_dec(iteriter);
        tbl_insert(scope, muint(1), mu_inc(iters));
    }

    mu_t acc = tbl_create(tbl_len(iters));
    mu_t iter;

    for (muint_t i = 0; tbl_next(iters, &i, 0, &iter);) {
        fn_fcall(iter, 0x0f, frame);
        fn_dec(iter);

        mu_t m = tbl_lookup(frame[0], muint(0));
        if (!m) {
            mu_dec(acc);
            mu_dec(frame[0]);
            tbl_dec(iters);
            return 0;
        }
        mu_dec(m);

        acc = tbl_concat(acc, frame[0], 0);
    }

    tbl_dec(iters);
    frame[0] = acc;
    return 0xf;
}

static mcnt_t fn_zip(mu_t *frame) {
    mu_t iter;
    if (tbl_len(frame[0]) == 0) {
        mu_errorf("no arguments passed to zip");
    } else if (tbl_len(frame[0]) == 1) {
        iter = tbl_lookup(frame[0], muint(0));
        mu_dec(frame[0]);
    } else {
        iter = frame[0];
    }

    frame[0] = msbfn(0x0, fn_zip_step,
            mlist({fn_call(MU_ITER, 0x11, iter)}));
    return 1;
}

MSTR(mu_gen_key_zip, "zip")
MBFN(mu_gen_zip, 0xf, fn_zip)

static mcnt_t fn_chain_step(mu_t scope, mu_t *frame) {
    mu_t iter = tbl_lookup(scope, muint(1));

    if (iter) {
        fn_fcall(iter, 0x0f, frame);
        fn_dec(iter);

        mu_t m = tbl_lookup(frame[0], muint(0));
        if (m) {
            mu_dec(m);
            return 0xf;
        }
        mu_dec(frame[0]);
    }

    mu_t iters = tbl_lookup(scope, muint(0));
    fn_fcall(iters, 0x01, frame);
    fn_dec(iters);
    if (frame[0]) {
        tbl_insert(scope, muint(1), fn_call(MU_ITER, 0x11, frame[0]));
        return fn_chain_step(scope, frame);
    }

    return 0;
}

static mcnt_t fn_chain(mu_t *frame) {
    mu_t iter;
    if (tbl_len(frame[0]) == 0) {
        mu_errorf("no arguments passed to chain");
    } else if (tbl_len(frame[0]) == 1) {
        iter = tbl_lookup(frame[0], muint(0));
        mu_dec(frame[0]);
    } else {
        iter = frame[0];
    }

    frame[0] = msbfn(0x0, fn_chain_step,
            mlist({fn_call(MU_ITER, 0x11, iter)}));
    return 1;
}

MSTR(mu_gen_key_chain, "chain")
MBFN(mu_gen_chain, 0xf, fn_chain)

static mcnt_t fn_take_count_step(mu_t scope, mu_t *frame) {
    mu_t i = tbl_lookup(scope, muint(0));
    if (num_cmp(i, muint(0)) <= 0) {
        return 0;
    }

    tbl_insert(scope, muint(0), num_sub(i, muint(1)));
    mu_t iter = tbl_lookup(scope, muint(1));
    return fn_tcall(iter, 0x0, frame);
}

static mcnt_t fn_take_while_step(mu_t scope, mu_t *frame) {
    mu_t iter = tbl_lookup(scope, muint(1));
    fn_fcall(iter, 0x0f, frame);
    fn_dec(iter);

    mu_t m = tbl_lookup(frame[0], muint(0));
    if (!m) {
        mu_dec(frame[0]);
        return 0;
    }
    mu_dec(m);

    m = mu_inc(frame[0]);
    mu_t cond = tbl_lookup(scope, muint(0));
    fn_fcall(cond, 0xf1, frame);
    fn_dec(cond);
    if (!frame[0]) {
        tbl_dec(m);
        return 0;
    }
    mu_dec(frame[0]);
    frame[0] = m;
    return 0xf;
}

static mcnt_t fn_take(mu_t *frame) {
    mu_t m    = frame[0];
    mu_t iter = frame[1];
    if (!mu_isnum(m) && !mu_isfn(m)) {
        mu_error_arg(MU_KEY_TAKE, 0x2, frame);
    }

    frame[0] = iter;
    fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    mcnt_t (*fn_take_step)(mu_t scope, mu_t *frame);
    if (mu_isnum(m)) {
        fn_take_step = fn_take_count_step;
    } else {
        fn_take_step = fn_take_while_step;
    }

    frame[0] = msbfn(0x0, fn_take_step, mlist({m, iter}));
    return 1;
}

MSTR(mu_gen_key_take, "take")
MBFN(mu_gen_take, 0x2, fn_take)

static mcnt_t fn_drop_count_step(mu_t scope, mu_t *frame) {
    mu_t iter = tbl_lookup(scope, muint(1));
    mu_t i = tbl_lookup(scope, muint(0));

    if (i) {
        while (num_cmp(i, muint(0)) > 0) {
            fn_fcall(iter, 0x01, frame);
            if (!frame[0]) {
                mu_dec(iter);
                return 0;
            }
            mu_dec(frame[0]);
            i = num_sub(i, muint(1));
        }

        tbl_insert(scope, muint(0), 0);
    }

    return fn_tcall(iter, 0x0, frame);
}

static mcnt_t fn_drop_while_step(mu_t scope, mu_t *frame) {
    mu_t iter = tbl_lookup(scope, muint(1));
    mu_t cond = tbl_lookup(scope, muint(0));

    if (cond) {
        while (fn_next(iter, 0xf, frame)) {
            mu_t m = tbl_inc(frame[0]);
            fn_fcall(cond, 0xf1, frame);
            if (!frame[0]) {
                fn_dec(iter);
                fn_dec(cond);
                frame[0] = m;
                tbl_insert(scope, muint(0), 0);
                return 0xf;
            }
            mu_dec(m);
        }

        fn_dec(iter);
        fn_dec(cond);
        return 0;
    }

    return fn_tcall(iter, 0x0, frame);
}

static mcnt_t fn_drop(mu_t *frame) {
    mu_t m    = frame[0];
    mu_t iter = frame[1];
    if (!mu_isnum(m) && !mu_isfn(m)) {
        mu_error_arg(MU_KEY_TAKE, 0x2, frame);
    }

    frame[0] = iter;
    fn_fcall(MU_ITER, 0x11, frame);
    iter = frame[0];

    mcnt_t (*fn_drop_step)(mu_t scope, mu_t *frame);
    if (mu_isnum(m)) {
        fn_drop_step = fn_drop_count_step;
    } else {
        fn_drop_step = fn_drop_while_step;
    }

    frame[0] = msbfn(0x0, fn_drop_step, mlist({m, iter}));
    return 1;
}

MSTR(mu_gen_key_drop, "drop")
MBFN(mu_gen_drop, 0x2, fn_drop)


// Iterator ordering
static mcnt_t fn_min(mu_t *frame) {
    if (tbl_len(frame[0]) == 1) {
        mu_t m = tbl_lookup(frame[0], muint(0));
        mu_dec(frame[0]);
        frame[0] = m;
    }

    fn_fcall(MU_ITER, 0x11, frame);
    mu_t iter = frame[0];

    fn_fcall(iter, 0x0f, frame);
    mu_t min_frame = frame[0];
    mu_t min = tbl_lookup(min_frame, muint(0));
    if (!min) {
        mu_errorf("no elements passed to min");
    }

    enum mtype type = mu_type(min);
    mint_t (*cmp)(mu_t, mu_t) = mu_attr_cmp[type];
    if (!cmp) {
        mu_errorf("unable to compare %r", min);
    }

    while (fn_next(iter, 0xf, frame)) {
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (mu_type(m) != type) {
            mu_errorf("unable to compare %r and %r", min, m);
        }

        if (cmp(m, min) < 0) {
            mu_dec(min);
            tbl_dec(min_frame);
            min = m;
            min_frame = frame[0];
        } else {
            mu_dec(m);
            tbl_dec(frame[0]);
        }
    }

    fn_dec(iter);
    mu_dec(min);
    frame[0] = min_frame;
    return 0xf;
}

MSTR(mu_gen_key_min, "min")
MBFN(mu_gen_min, 0xf, fn_min)

static mcnt_t fn_max(mu_t *frame) {
    if (tbl_len(frame[0]) == 1) {
        mu_t m = tbl_lookup(frame[0], muint(0));
        mu_dec(frame[0]);
        frame[0] = m;
    }

    fn_fcall(MU_ITER, 0x11, frame);
    mu_t iter = frame[0];

    fn_fcall(iter, 0x0f, frame);
    mu_t max_frame = frame[0];
    mu_t max = tbl_lookup(max_frame, muint(0));
    if (!max) {
        mu_errorf("no elements passed to max");
    }

    enum mtype type = mu_type(max);
    mint_t (*cmp)(mu_t, mu_t) = mu_attr_cmp[type];
    if (!cmp) {
        mu_errorf("unable to compare %r", max);
    }

    while (fn_next(iter, 0xf, frame)) {
        mu_t m = tbl_lookup(frame[0], muint(0));
        if (mu_type(m) != type) {
            mu_errorf("unable to compare %r and %r", max, m);
        }

        if (cmp(m, max) >= 0) {
            mu_dec(max);
            tbl_dec(max_frame);
            max = m;
            max_frame = frame[0];
        } else {
            mu_dec(m);
            tbl_dec(frame[0]);
        }
    }

    fn_dec(iter);
    mu_dec(max);
    frame[0] = max_frame;
    return 0xf;
}

MSTR(mu_gen_key_max, "max")
MBFN(mu_gen_max, 0xf, fn_max)

static mcnt_t fn_reverse_step(mu_t scope, mu_t *frame) {
    mu_t i = tbl_lookup(scope, muint(1));
    if (num_cmp(i, muint(0)) < 0) {
        return 0;
    }

    mu_t store = tbl_lookup(scope, muint(0));
    frame[0] = tbl_lookup(store, i);
    mu_dec(store);

    tbl_insert(scope, muint(1), num_sub(i, muint(1)));
    return 0xf;
}

static mcnt_t fn_reverse(mu_t *frame) {
    fn_fcall(MU_ITER, 0x11, frame);
    mu_t iter = frame[0];
    mu_t store = tbl_create(0);

    while (fn_next(iter, 0xf, frame)) {
        tbl_insert(store, muint(tbl_len(store)), frame[0]);
    }

    fn_dec(iter);

    frame[0] = msbfn(0x0, fn_reverse_step, mlist({
        store, muint(tbl_len(store)-1)
    }));
    return 1;
}

MSTR(mu_gen_key_reverse, "reverse")
MBFN(mu_gen_reverse, 0x1, fn_reverse)

// Simple iterative merge sort
static void fn_merge_sort(mu_t elems) {
    // Uses arrays (first elem, frame) to keep from
    // looking up the elem each time.
    // We use two arrays so we can just flip each merge.
    muint_t len = tbl_len(elems);
    mu_t (*a)[2] = mu_alloc(len*sizeof(mu_t[2]));
    mu_t (*b)[2] = mu_alloc(len*sizeof(mu_t[2]));

    enum mtype type = MTNUM;

    for (muint_t i = 0, j = 0; tbl_next(elems, &i, 0, &a[j][1]); j++) {
        a[j][0] = tbl_lookup(a[j][1], muint(0));

        if (j == 0) {
            type = mu_type(a[j][0]);
        } else {
            if (type != mu_type(a[j][0])) {
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
        tbl_insert(elems, muint(i), a[i][1]);
    }

    // Since both arrays identical, it doesn't matter if they've been swapped
    mu_dealloc(a, len*sizeof(mu_t[2]));
    mu_dealloc(b, len*sizeof(mu_t[2]));
}

static mcnt_t fn_sort_step(mu_t scope, mu_t *frame) {
    mu_t store = tbl_lookup(scope, muint(0));
    muint_t i = num_uint(tbl_lookup(scope, muint(1)));

    bool next = tbl_next(store, &i, 0, &frame[0]);
    mu_dec(store);
    tbl_insert(scope, muint(1), muint(i));
    return next ? 0xf : 0;
}

static mcnt_t fn_sort(mu_t *frame) {
    fn_fcall(MU_ITER, 0x11, frame);
    mu_t iter = frame[0];
    mu_t store = tbl_create(0);

    while (fn_next(iter, 0xf, frame)) {
        tbl_insert(store, muint(tbl_len(store)), frame[0]);
    }

    mu_dec(iter);

    fn_merge_sort(store);

    frame[0] = msbfn(0x0, fn_sort_step, mlist({store, muint(0)}));
    return 1;
}

MSTR(mu_gen_key_sort, "sort")
MBFN(mu_gen_sort, 0x1, fn_sort)


// Builtins table
MTBL(mu_gen_builtins, {
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
