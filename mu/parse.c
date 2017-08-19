#include "parse.h"
#include "mu.h"


//// Definitions ////

// Tokens
typedef uint32_t mtok_t;
enum mtok {
    T_END       = 0,
    T_TERM      = 1 <<  0,
    T_SEP       = 1 <<  1,
    T_ASSIGN    = 1 <<  2,
    T_PAIR      = 1 <<  3,

    T_LET       = 1 <<  4,
    T_DOT       = 1 <<  5,
    T_ARROW     = 1 <<  6,

    T_FN        = 1 <<  7,
    T_TYPE      = 1 <<  8,

    T_IF        = 1 <<  9,
    T_WHILE     = 1 << 10,
    T_FOR       = 1 << 11,
    T_ELSE      = 1 << 12,

    T_AND       = 1 << 13,
    T_OR        = 1 << 14,

    T_CONTINUE  = 1 << 15,
    T_BREAK     = 1 << 16,
    T_RETURN    = 1 << 17,

    T_SYM       = 1 << 18,
    T_NIL       = 1 << 19,
    T_IMM       = 1 << 20,
    T_OP        = 1 << 21,
    T_EXPAND    = 1 << 22,

    T_LPAREN    = 1 << 23,
    T_RPAREN    = 1 << 24,
    T_LTABLE    = 1 << 25,
    T_RTABLE    = 1 << 26,
    T_LBLOCK    = 1 << 27,
    T_RBLOCK    = 1 << 28,
};

// Sets of tokens
#define T_ANY_OP \
    (T_OP | T_EXPAND)

#define T_ANY_SYM \
    (T_SYM | T_LET | T_FN | T_TYPE | T_IF | \
     T_WHILE | T_FOR | T_ELSE | T_AND | T_OR | \
     T_CONTINUE | T_BREAK | T_RETURN | T_NIL)

#define T_ANY_VAL \
    (T_ANY_SYM | T_ANY_OP | T_IMM | \
     T_ASSIGN | T_PAIR | T_ARROW | T_DOT)

#define T_EXPR \
    (T_LPAREN | T_LTABLE | T_FN | T_TYPE | T_IF | \
     T_WHILE | T_FOR | T_NIL | T_IMM | T_SYM | \
     T_OP | T_EXPAND)

#define T_STMT \
    (T_EXPR | T_LBLOCK | T_ASSIGN | T_LET | T_DOT | \
     T_ARROW | T_CONTINUE | T_BREAK | T_RETURN)

#define T_ANY (-1)

// Table of keywords
MU_DEF_STR(mu_kw_let_def,       "let")
MU_DEF_STR(mu_kw_else_def,      "else")
MU_DEF_STR(mu_kw_and_def,       "and")
MU_DEF_STR(mu_kw_or_def,        "or")
MU_DEF_STR(mu_kw_continue_def,  "continue")
MU_DEF_STR(mu_kw_break_def,     "break")
MU_DEF_STR(mu_kw_return_def,    "return")
MU_DEF_STR(mu_kw_fn_def,        "fn")
MU_DEF_STR(mu_kw_type_def,      "type")
MU_DEF_STR(mu_kw_if_def,        "if")
MU_DEF_STR(mu_kw_while_def,     "while")
MU_DEF_STR(mu_kw_for_def,       "for")
MU_DEF_STR(mu_kw_nil_def,       "nil")
MU_DEF_STR(mu_kw_nil2_def,      "_")
MU_DEF_STR(mu_kw_assign_def,    "=")
MU_DEF_STR(mu_kw_pair_def,      ":")
MU_DEF_STR(mu_kw_dot_def,       ".")
MU_DEF_STR(mu_kw_arrow_def,     "->")
MU_DEF_STR(mu_kw_expand_def,    "..")

MU_DEF_UINT(mu_tok_let_def,     T_LET)
MU_DEF_UINT(mu_tok_else_def,    T_ELSE)
MU_DEF_UINT(mu_tok_and_def,     T_AND)
MU_DEF_UINT(mu_tok_or_def,      T_OR)
MU_DEF_UINT(mu_tok_cont_def,    T_CONTINUE)
MU_DEF_UINT(mu_tok_break_def,   T_BREAK)
MU_DEF_UINT(mu_tok_return_def,  T_RETURN)
MU_DEF_UINT(mu_tok_fn_def,      T_FN)
MU_DEF_UINT(mu_tok_type_def,    T_TYPE)
MU_DEF_UINT(mu_tok_if_def,      T_IF)
MU_DEF_UINT(mu_tok_while_def,   T_WHILE)
MU_DEF_UINT(mu_tok_for_def,     T_FOR)
MU_DEF_UINT(mu_tok_nil_def,     T_NIL)
MU_DEF_UINT(mu_tok_assign_def,  T_ASSIGN)
MU_DEF_UINT(mu_tok_pair_def,    T_PAIR)
MU_DEF_UINT(mu_tok_dot_def,     T_DOT)
MU_DEF_UINT(mu_tok_arrow_def,   T_ARROW)
MU_DEF_UINT(mu_tok_expand_def,  T_EXPAND)

MU_DEF_TBL(mu_keywords_def, {
    { mu_kw_let_def,        mu_tok_let_def    },
    { mu_kw_else_def,       mu_tok_else_def   },
    { mu_kw_and_def,        mu_tok_and_def    },
    { mu_kw_or_def,         mu_tok_or_def     },
    { mu_kw_continue_def,   mu_tok_cont_def   },
    { mu_kw_break_def,      mu_tok_break_def  },
    { mu_kw_return_def,     mu_tok_return_def },
    { mu_kw_fn_def,         mu_tok_fn_def     },
    { mu_kw_type_def,       mu_tok_type_def   },
    { mu_kw_if_def,         mu_tok_if_def     },
    { mu_kw_while_def,      mu_tok_while_def  },
    { mu_kw_for_def,        mu_tok_for_def    },
    { mu_kw_nil_def,        mu_tok_nil_def    },
    { mu_kw_nil2_def,       mu_tok_nil_def    },
    { mu_kw_assign_def,     mu_tok_assign_def },
    { mu_kw_pair_def,       mu_tok_pair_def   },
    { mu_kw_dot_def,        mu_tok_dot_def    },
    { mu_kw_arrow_def,      mu_tok_arrow_def  },
    { mu_kw_expand_def,     mu_tok_expand_def },
})


// Classification for individual characters
typedef uint8_t mclass_t;
enum mclass {
    L_NONE,
    L_TERM, L_SEP, L_WS,
    L_OP, L_KW, L_STR, L_NUM,

    L_LBLOCK, L_RBLOCK,
    L_LTABLE, L_RTABLE,
    L_LPAREN, L_RPAREN,
};

static const mclass_t class[256] = {
/* 00 01 02 03 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 04 05 06 \a */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* \b \t \n \v */   L_NONE,     L_WS,       L_WS,       L_WS,
/* \f \r 0e 0f */   L_WS,       L_WS,       L_NONE,     L_NONE,
/* 10 11 12 13 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 14 15 16 17 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 18 19 1a 1b */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 1c 1d 1e 1f */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/*     !  "  # */   L_WS,       L_OP,       L_STR,      L_WS,
/*  $  %  &  ' */   L_OP,       L_OP,       L_OP,       L_STR,
/*  (  )  *  + */   L_LPAREN,   L_RPAREN,   L_OP,       L_OP,
/*  ,  -  .  / */   L_SEP,      L_OP,       L_OP,       L_OP,
/*  0  1  2  3 */   L_NUM,      L_NUM,      L_NUM,      L_NUM,
/*  4  5  6  7 */   L_NUM,      L_NUM,      L_NUM,      L_NUM,
/*  8  9  :  ; */   L_NUM,      L_NUM,      L_OP,       L_TERM,
/*  <  =  >  ? */   L_OP,       L_OP,       L_OP,       L_OP,
/*  @  A  B  C */   L_OP,       L_KW,       L_KW,       L_KW,
/*  D  E  F  G */   L_KW,       L_KW,       L_KW,       L_KW,
/*  H  I  J  K */   L_KW,       L_KW,       L_KW,       L_KW,
/*  L  M  N  O */   L_KW,       L_KW,       L_KW,       L_KW,
/*  P  Q  R  S */   L_KW,       L_KW,       L_KW,       L_KW,
/*  T  U  V  W */   L_KW,       L_KW,       L_KW,       L_KW,
/*  X  Y  Z  [ */   L_KW,       L_KW,       L_KW,       L_LTABLE,
/*  \  ]  ^  _ */   L_OP,       L_RTABLE,   L_OP,       L_KW,
/*  `  a  b  c */   L_OP,       L_KW,       L_KW,       L_KW,
/*  d  e  f  g */   L_KW,       L_KW,       L_KW,       L_KW,
/*  h  i  j  k */   L_KW,       L_KW,       L_KW,       L_KW,
/*  l  p  n  o */   L_KW,       L_KW,       L_KW,       L_KW,
/*  p  q  r  s */   L_KW,       L_KW,       L_KW,       L_KW,
/*  t  u  v  w */   L_KW,       L_KW,       L_KW,       L_KW,
/*  x  y  z  { */   L_KW,       L_KW,       L_KW,       L_LBLOCK,
/*  |  }  ~ 7f */   L_OP,       L_RBLOCK,   L_OP,       L_NONE,
/* 80 81 82 83 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 84 85 86 87 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 88 89 8a 8b */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 8c 8d 8e 8f */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 90 91 92 93 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 94 95 96 97 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 98 99 9a 9b */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 9c 9d 9e 9f */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* a0 a1 a2 a3 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* a4 a5 a6 a7 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* a8 a9 aa ab */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* ac ad ae af */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* b0 b1 b2 b3 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* b4 b5 b6 b7 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* b8 b9 ba bb */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* bc bd be bf */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* c0 c1 c2 c3 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* c4 c5 c6 c7 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* c8 c9 ca cb */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* cc cd ce cf */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* d0 d1 d2 d3 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* d4 d5 d6 d7 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* d8 d9 da db */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* dc dd de df */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* e0 e1 e2 e3 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* e4 e5 e6 e7 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* e8 e9 ea eb */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* ec ed ee ef */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* f0 f1 f2 f3 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* f4 f5 f6 f7 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* f8 f9 fa fb */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* fc fd fe ff */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
};

// Parsing state
typedef uint8_t mstate_t;
enum mstate {
    P_DIRECT,
    P_INDIRECT,
    P_SCOPED,
    P_CALLED,
    P_NIL
};


//// Structures ////

// Lexical analysis state
struct mlex {
    const mbyte_t *begin;
    const mbyte_t *pos;
    const mbyte_t *end;

    mtok_t tok;
    mu_t val;
    muintq_t prec;

    muintq_t indent;
    muintq_t depth;
    muintq_t block;
    muintq_t nblock;
    muintq_t paren;
    muintq_t nparen;
};

// Current match
struct mmatch {
    mu_t val;
    muintq_t prec;
};

// Parsing state
struct mparse {
    mu_t scope;
    mu_t imms;
    mu_t bcode;
    mlen_t bcount;

    mlen_t bchain;
    mlen_t cchain;

    muintq_t args;
    muintq_t regs;
    muintq_t sp;

    muintq_t depth;
    struct mlex l;
    struct mmatch m;
};

// Frame state
struct mframe {
    mlen_t target;
    mlen_t count;
    mlen_t index;
    muintq_t depth;

    uint8_t unpack  : 1;
    uint8_t insert  : 1;
    uint8_t tabled  : 1;
    uint8_t flatten : 1;
    uint8_t key     : 1;
    uint8_t call    : 1;
    uint8_t expand  : 1;
};

// Expression state
struct mexpr {
    muintq_t prec;
    muintq_t params;
    mstate_t state;
    uint8_t insert : 1;
};


//// Error handling ////

// Common errors
#define mu_checkparse(pred, ...) \
        ((pred) ? (void)0 : mu_errorparse(__VA_ARGS__))
static mu_noreturn mu_errorparse(struct mlex *l, const char *f, ...) {
    va_list args;
    va_start(args, f);

    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_buf_vpushf(&b, &n, f, args);

    const mbyte_t *p = l->begin;
    muint_t lines = 1;
    muint_t nlines = 1;

    while (p < l->pos) {
        if (*p == '#') {
            while (p < l->end && *p != '\n') {
                p++;
            }
        } else if (*p == '\n') {
            nlines++;
            p++;
        } else if (class[*p] == L_WS) {
            p++;
        } else {
            lines = nlines;
            p++;
        }
    }

    if (lines != 1) {
        mu_buf_pushf(&b, &n, " on line %wu", lines);
    }

    mu_error(mu_buf_getdata(b), n);
}

#define mu_checkchr(pred, ...) \
        ((pred) ? (void)0 : mu_errorchr(__VA_ARGS__))
static mu_noreturn mu_errorchr(struct mlex *l) {
    mu_errorparse(l, "unexpected %c", l->pos);
}

#define mu_checktoken(pred, ...) \
        ((pred) ? (void)0 : mu_errortoken(__VA_ARGS__))
static mu_noreturn mu_errortoken(struct mlex *l) {
    if (l->tok & T_ANY_VAL) {
        mu_checkparse(false, l, "unexpected %r", mu_inc(l->val));
    } else {
        mu_checkparse(false, l, "unexpected %s",
                l->tok & T_TERM   ? "terminator" :
                l->tok & T_SEP    ? "','" :
                l->tok & T_LPAREN ? "'('" :
                l->tok & T_RPAREN ? "')'" :
                l->tok & T_LTABLE ? "'['" :
                l->tok & T_RTABLE ? "']'" :
                l->tok & T_LBLOCK ? "'{'" :
                l->tok & T_RBLOCK ? "'}'" : "end");
    }
}

#define mu_checkassign(pred, ...) \
        ((pred) ? (void)0 : mu_errorassign(__VA_ARGS__))
static void mu_errorassign(struct mlex *l) {
    mu_errorparse(l, "invalid assignment");
}

#define mu_checkscope(pred, ...) \
        ((pred) ? (void)0 : mu_errorscope(__VA_ARGS__))
static void mu_errorscope(struct mlex *l, mu_t m) {
    mu_errorparse(l, "undefined %r", m);
}


//// Lexical analysis ////

// Lexer definitions for non-trivial tokens
static void l_indent(struct mlex *l) {
    const mbyte_t *nl = 0;
    muintq_t nindent = 0;
    while (l->pos < l->end) {
        if (*l->pos == '#') {
            while (l->pos < l->end && *l->pos != '\n') {
                l->pos++;
            }
        } else if (*l->pos == '\n') {
            nl = l->pos++;
            nindent = 0;
        } else if (class[*l->pos] == L_WS) {
            l->pos++;
            nindent++;
        } else {
            break;
        }
    }

    if (nindent != l->indent) {
        l->tok = nindent > l->indent ? T_LBLOCK : T_RBLOCK;
        l->nblock += nindent - l->indent;
        l->indent = nindent;
        if (nl) l->pos = nl;
    } else {
        l->tok = T_TERM;
    }
}

static void l_op(struct mlex *l) {
    const mbyte_t *begin = l->pos++;

    while (l->pos < l->end && class[*l->pos] == L_OP) {
        l->pos++;
    }

    l->val = mu_str_fromdata(begin, l->pos-begin);
    mu_t tok = mu_tbl_lookup(MU_KEYWORDS, mu_inc(l->val));
    l->tok = tok ? mu_num_getuint(tok) : T_OP;
}

static void l_kw(struct mlex *l) {
    const mbyte_t *begin = l->pos++;

    while (l->pos < l->end && (class[*l->pos] == L_KW ||
                               class[*l->pos] == L_NUM))
        l->pos++;

    l->val = mu_str_fromdata(begin, l->pos-begin);
    mu_t tok = mu_tbl_lookup(MU_KEYWORDS, mu_inc(l->val));
    l->tok = tok ? mu_num_getuint(tok) : T_SYM;
}

static void l_num(struct mlex *l) {
    l->val = mu_num_parsen(&l->pos, l->end);
    mu_checkparse(l->val, l, "invalid number literal");
    l->tok = T_IMM;
}

static void l_str(struct mlex *l) {
    l->val = mu_str_parsen(&l->pos, l->end);
    mu_checkparse(l->val, l, "unterminated string literal");
    l->tok = T_IMM;
}


static void lex_next(struct mlex *l) {
    // Update previous token's state
    l->block = l->nblock;
    l->paren = l->nparen;
    l->val = 0;

    // Determine token
    if (l->pos >= l->end) {
        l->block -= l->indent;
        l->tok = 0;
        return;
    }

    mclass_t lclass = class[*l->pos];
    switch (lclass) {
        case L_NONE:    mu_checkchr(false, l);

        case L_WS:                   l_indent(l); break;
        case L_OP:      l->prec = 0; l_op(l);     break;
        case L_KW:      l->prec = 1; l_kw(l);     break;
        case L_STR:                  l_str(l);    break;
        case L_NUM:                  l_num(l);    break;

        case L_TERM:    l->tok = T_TERM; l->pos++; break;
        case L_SEP:     l->tok = T_SEP;  l->pos++; break;

        case L_LBLOCK:  l->tok = T_LBLOCK; l->nblock++; l->pos++; break;
        case L_RBLOCK:  l->tok = T_RBLOCK; l->nblock--; l->pos++; break;
        case L_LTABLE:  l->tok = T_LTABLE; l->nparen++; l->pos++; break;
        case L_RTABLE:  l->tok = T_RTABLE; l->nparen--; l->pos++; break;
        case L_LPAREN:  l->tok = T_LPAREN; l->nparen++; l->pos++; break;
        case L_RPAREN:  l->tok = T_RPAREN; l->nparen--; l->pos++; break;
    }

    // Use trailing whitespace for precedence
    const mbyte_t *end = l->pos;
    while (l->pos < l->end) {
        if (*l->pos == '#') {
            while (l->pos < l->end && *l->pos != '\n') {
                l->pos++;
            }
        } else if (class[*l->pos] == L_WS &&
                   (l->nparen || *l->pos != '\n')) {
            l->pos++;
        } else {
            break;
        }
    }

    l->prec += 2*(l->pos - end);
}

static void lex_init(struct mlex *l, const mbyte_t *pos, const mbyte_t *end) {
    l->begin = pos;
    l->pos = pos;
    l->end = end;

    lex_next(l);
}

mu_inline struct mlex lex_inc(struct mlex l) {
    mu_inc(l.val);
    return l;
}

mu_inline void lex_dec(struct mlex l) {
    mu_dec(l.val);
}


//// Lexing shortcuts ////
static mu_noreturn unexpected(struct mparse *p) {
    mu_checktoken(false, &p->l);
    mu_unreachable;
}

static bool next(struct mparse *p, mtok_t tok) {
    return p->l.tok & tok;
}

static bool match(struct mparse *p, mtok_t tok) {
    if (next(p, tok)) {
        mu_dec(p->m.val);
        p->m.val = p->l.val;
        p->m.prec = p->l.prec;
        lex_next(&p->l);
        return true;
    } else {
        return false;
    }
}

static void expect(struct mparse *p, mtok_t tok) {
    if (!match(p, tok)) {
        unexpected(p);
    }
}

static bool lookahead(struct mparse *p, mtok_t a, mtok_t b) {
    if (!next(p, a)) {
        return false;
    }

    struct mlex l = lex_inc(p->l);
    if (match(p, a) && next(p, b)) {
        lex_dec(l);
        return true;
    }

    lex_dec(p->l);
    p->l = l;
    return false;
}


//// Encoding operations ////

// Actual encoding is defered to virtual machine
static void emit(struct mparse *p, mbyte_t byte) {
    muint_t bcount = p->bcount;
    mu_buf_pushchr(&p->bcode, &bcount, byte);
    p->bcount = bcount;
}

static void encode(struct mparse *p, mop_t op,
                   minth_t d, minth_t a, minth_t b,
                   mint_t sdiff) {
    p->sp += sdiff;

    if (p->sp+1 > p->regs) {
        p->regs = p->sp+1;
    }

    mu_encode((void (*)(void *, mbyte_t))emit, p, op, d, a, b);
}

static void patch(struct mparse *p, mlen_t offset, minth_t j) {
    mbyte_t *bcode = mu_buf_getdata(p->bcode);
    mu_patch(&bcode[offset], j);
}

static void patch_all(struct mparse *p, mlen_t chain, minth_t offset) {
    muint_t current = 0;
    mbyte_t *bcode = mu_buf_getdata(p->bcode);

    while (chain) {
        current += chain;
        chain = mu_patch(&bcode[current], offset - current);
    }
}

// Storing immediates/code objects
#define IMM_NIL imm_nil()
MU_DEF_BFN(imm_nil, 0, 0)

static muint_t imm(struct mparse *p, mu_t m) {
    if (!m) {
        m = IMM_NIL;
    }

    mu_t mindex = mu_tbl_lookup(p->imms, mu_inc(m));

    if (mindex) {
        mu_dec(m);
        return mu_num_getuint(mindex);
    }

    muint_t index = mu_tbl_getlen(p->imms);
    mu_tbl_insert(p->imms, m, mu_num_fromuint(index));
    return index;
}

// Scope checking, does not consume
static void scopecheck(struct mparse *p, mu_t m, bool insert) {
    if (insert) {
        mu_tbl_insert(p->scope, mu_inc(m), IMM_NIL);
    } else {
        mu_t s = mu_tbl_lookup(p->scope, mu_inc(m));
        mu_checkscope(s, &p->l, m);
        mu_dec(s);
    }
}

// More complicated encoding operations
static muint_t offset(struct mexpr *e) {
    if (e->state == P_INDIRECT) {
        return 2;
    } else if (e->state == P_SCOPED) {
        return 1;
    } else {
        return 0;
    }
}

static void encload(struct mparse *p, struct mexpr *e, mint_t offset) {
    if (e->state == P_SCOPED) {
        encode(p, MU_OP_LOOKUP, p->sp+offset, 0, p->sp, +offset);
    } else if (e->state == P_INDIRECT) {
        encode(p, MU_OP_LOOKDN, p->sp+offset-1, p->sp-1, p->sp, +offset-1);
    } else if (e->state == P_NIL) {
        encode(p, MU_OP_IMM, p->sp+offset+1, imm(p, 0), 0, +offset+1);
    } else {
        if (e->state == P_CALLED) {
            encode(p, MU_OP_CALL, p->sp-(e->params == 0xf ? 1 : e->params),
                   (e->params << 4) | 1, 0,
                   -(e->params == 0xf ? 1 : e->params));
        }

        if (offset != 0) {
            encode(p, MU_OP_MOVE, p->sp+offset, p->sp, 0, +offset);
        }
    }
}

static void encstore(struct mparse *p, struct mexpr *e,
                         bool insert, mint_t offset) {
    if (e->state == P_NIL) {
        encode(p, MU_OP_DROP, p->sp-offset, 0, 0, 0);
    } else if (e->state == P_SCOPED) {
        encode(p, insert ? MU_OP_INSERT : MU_OP_ASSIGN,
               p->sp-offset-1, 0, p->sp, -1);
    } else if (e->state == P_INDIRECT) {
        encode(p, insert ? MU_OP_INSERT : MU_OP_ASSIGN,
               p->sp-offset-2, p->sp-1, p->sp, 0);
        encode(p, MU_OP_DROP, p->sp-1, 0, 0, -2);
    } else {
        mu_checkassign(false, &p->l);
    }
}

// Completing a parse and deferating the final code object
static mu_t compile(struct mparse *p, bool weak) {
    extern void mu_code_destroy(mu_t);
    mu_t b = mu_buf_createdtor(
            mu_offsetof(struct mcode, data) +
            sizeof(mu_t)*mu_tbl_getlen(p->imms) +
            p->bcount,
            mu_code_destroy);

    struct mcode *code = mu_buf_getdata(b);
    code->args = p->args;
    code->flags = MFN_SCOPED | (weak ? MFN_WEAK : 0);
    code->regs = p->regs;
    code->locals = mu_tbl_getlen(p->scope);
    code->icount = mu_tbl_getlen(p->imms);
    code->bcount = p->bcount;

    mu_t *imms = mu_code_getimms(b);
    mu_t k, v;
    for (muint_t i = 0; mu_tbl_next(p->imms, &i, &k, &v);) {
        imms[mu_num_getuint(v)] = (k == IMM_NIL) ? 0 : k;
    }

    mbyte_t *bcode = mu_code_getbcode(b);
    memcpy(bcode, mu_buf_getdata(p->bcode), p->bcount);

    mu_dec(p->imms);
    mu_dec(p->bcode);
    mu_dec(p->scope);
    mu_dec(p->m.val);
    return b;
}


//// Scanning rules ////
static void s_block(struct mparse *p, struct mframe *f);
static void s_expr(struct mparse *p, struct mframe *f, muintq_t prec);
static void s_frame(struct mparse *p, struct mframe *f, bool update);

static void s_block(struct mparse *p, struct mframe *f) {
    muintq_t depth = p->l.paren;
    while (match(p, T_STMT & ~T_LBLOCK) ||
           (p->l.paren > p->l.depth && match(p, T_SEP)) ||
           (p->l.paren > depth && match(p, T_RPAREN | T_RTABLE))) {}

    if (match(p, T_LBLOCK)) {
        muintq_t block = p->l.block;
        while (p->l.block >= block &&
               match(p, T_ANY));
    }
}

static void s_expr(struct mparse *p, struct mframe *f, muintq_t prec) {
    while (match(p, T_LPAREN)) {}

    while (true) {
        if (match(p, T_LPAREN)) {
            muintq_t depth = p->l.paren;
            while (p->l.paren >= depth && match(p, T_ANY)) {}
            f->call = true;

        } else if (match(p, T_LTABLE)) {
            muintq_t depth = p->l.paren;
            while (p->l.paren >= depth && match(p, T_ANY)) {}
            f->call = false;

        } else if (match(p, T_FN | T_TYPE | T_IF | T_WHILE | T_FOR | T_ELSE)) {
            s_block(p, f);
            f->call = false;

        } else if (match(p, T_SYM | T_NIL | T_IMM | T_DOT | T_ARROW)) {
            f->call = false;

        } else if (prec > p->l.prec && match(p, T_ANY_OP)) {
            bool call = next(p, T_EXPR);
            s_expr(p, f, p->m.prec);
            f->call = call;

        } else if (prec > p->l.prec && match(p, T_AND | T_OR)) {
            s_expr(p, f, p->m.prec);
            f->call = false;

        } else if (!(f->count == 0 &&
                     p->l.paren > p->l.depth &&
                     match(p, T_RPAREN))) {
            return;
        }
    }
}

static void s_frame(struct mparse *p, struct mframe *f, bool update) {
    struct mlex l = lex_inc(p->l);
    f->depth = p->l.depth; p->l.depth = p->l.paren;

    do {
        f->call = false;
        if (!next(p, T_EXPR & ~T_EXPAND)) {
            break;
        }

        s_expr(p, f, -1);
        if (match(p, T_PAIR)) {
            f->tabled = true;
            s_expr(p, f, -1);
        }

        f->count++;
    } while (p->l.paren != f->depth && match(p, T_SEP));

    if (match(p, T_EXPAND)) {
        f->expand = true;
        s_expr(p, f, -1);
    }

    p->l.depth = f->depth;
    if (!update) {
        lex_dec(p->l);
        p->l = l;
    } else {
        lex_dec(l);
    }

    f->tabled = f->tabled || f->expand || f->count > MU_FRAME;
    f->target = f->count;
    f->call = f->call && f->count == 1 && !f->tabled;
}


//// Grammar rules ////
static void p_fn(struct mparse *p, bool weak);
static void p_if(struct mparse *p, bool expr);
static void p_while(struct mparse *p);
static void p_for(struct mparse *p);
static void p_expr(struct mparse *p);
static void p_subexpr(struct mparse *p, struct mexpr *e);
static void p_postexpr(struct mparse *p, struct mexpr *e);
static void p_entry(struct mparse *p, struct mframe *f);
static void p_frame(struct mparse *p, struct mframe *f);
static void p_assign(struct mparse *p, bool insert);
static void p_return(struct mparse *p);
static void p_stmt(struct mparse *p);
static void p_block(struct mparse *p, bool root);

static void p_fn(struct mparse *p, bool weak) {
    struct mparse q = {
        .bcode = mu_buf_create(0),
        .bcount = 0,

        .scope = mu_tbl_createtail(0, mu_inc(p->scope)),
        .imms = mu_tbl_create(0),
        .bchain = -1,
        .cchain = -1,
        .regs = 1,

        .l = p->l,
    };

    expect(&q, T_LPAREN);
    struct mframe f = {.unpack = true, .insert = true};
    s_frame(&q, &f, false);
    q.sp = f.tabled ? 1 : f.count;
    q.args = f.tabled ? 0xf : f.count;
    p_frame(&q, &f);
    expect(&q, T_RPAREN);

    p_stmt(&q);
    encode(&q, MU_OP_RET, 0, 0, 0, 0);

    p->l = q.l;

    mu_t c = compile(&q, weak);
    encode(p, MU_OP_FN, p->sp+1, imm(p, c), 0, +1);
}

static void p_if(struct mparse *p, bool expr) {
    expect(p, T_LPAREN);
    p_expr(p);
    expect(p, T_RPAREN);

    mlen_t cond_offset = p->bcount;
    encode(p, MU_OP_JFALSE, p->sp, 0, 0, 0);
    encode(p, MU_OP_DROP, p->sp, 0, 0, -1);

    if (expr) {
        p_expr(p);
    } else {
        p_stmt(p);
    }

    if (next(p, T_ELSE) || (!expr && lookahead(p, T_TERM, T_ELSE))) {
        expect(p, T_ELSE);
        mlen_t exit_offset = p->bcount;
        encode(p, MU_OP_JUMP, 0, 0, 0, -expr);
        mlen_t else_offset = p->bcount;

        if (expr) {
            p_expr(p);
        } else {
            p_stmt(p);
        }

        patch(p, cond_offset, else_offset - cond_offset);
        patch(p, exit_offset, p->bcount - exit_offset);
    } else if (!expr) {
        patch(p, cond_offset, p->bcount - cond_offset);
    } else {
        unexpected(p);
    }
}

static void p_while(struct mparse *p) {
    mlen_t while_offset = p->bcount;
    expect(p, T_LPAREN);
    p_expr(p);
    expect(p, T_RPAREN);

    mlen_t cond_offset = p->bcount;
    encode(p, MU_OP_JFALSE, p->sp, 0, 0, 0);
    encode(p, MU_OP_DROP, p->sp, 0, 0, -1);

    mlen_t bchain = p->bchain; p->bchain = 0;
    mlen_t cchain = p->cchain; p->cchain = 0;

    p_stmt(p);

    encode(p, MU_OP_JUMP, 0, while_offset - p->bcount, 0, 0);
    patch(p, cond_offset, p->bcount - cond_offset);

    patch_all(p, p->bchain, p->bcount);
    patch_all(p, p->cchain, while_offset);
    p->bchain = bchain;
    p->cchain = cchain;
}

static void p_for(struct mparse *p) {
    expect(p, T_LPAREN);
    struct mlex ll = lex_inc(p->l);
    struct mframe f = {.unpack = true, .insert = true};
    s_frame(p, &f, true);

    expect(p, T_ASSIGN);
    mu_checkassign(f.count != 0 || f.tabled, &p->l);

    encode(p, MU_OP_IMM, p->sp+1, imm(p, MU_ITER_KEY), 0, +1);
    encode(p, MU_OP_LOOKUP, p->sp, 0, p->sp, 0);
    p_expr(p);
    encode(p, MU_OP_CALL, p->sp-1, 0x11, 0, -1);

    mlen_t for_offset = p->bcount;
    mlen_t cond_offset;
    encode(p, MU_OP_DUP, p->sp+1, p->sp, 0, +1);
    if (f.tabled) {
        encode(p, MU_OP_CALL, p->sp, 0x0f, 0, 0);
        encode(p, MU_OP_IMM, p->sp+1, imm(p, mu_num_fromuint(0)), 0, +1);
        encode(p, MU_OP_LOOKUP, p->sp, p->sp-1, p->sp, 0);
        cond_offset = p->bcount;
        encode(p, MU_OP_JFALSE, p->sp, 0, 0, 0);
        encode(p, MU_OP_DROP, p->sp, 0, 0, -1);
    } else {
        encode(p, MU_OP_CALL, p->sp, 0 | f.count, 0, +f.count-1);
        cond_offset = p->bcount;
        encode(p, MU_OP_JFALSE, p->sp-f.count+1, 0, 0, 0);
    }
    mlen_t count = f.tabled ? 1 : f.count;
    struct mlex lr = p->l;
    p->l = ll;

    p_frame(p, &f);
    expect(p, T_ASSIGN);
    lex_dec(p->l); p->l = lr;
    expect(p, T_RPAREN);


    mlen_t bchain = p->bchain; p->bchain = 0;
    mlen_t cchain = p->cchain; p->cchain = 0;

    p_stmt(p);

    encode(p, MU_OP_JUMP, 0, for_offset - p->bcount, 0, 0);
    patch(p, cond_offset, p->bcount - cond_offset);
    for (muint_t i = 0; i < count; i++) {
        encode(p, MU_OP_DROP, p->sp+1 + i, 0, 0, 0);
    }

    patch_all(p, p->bchain, p->bcount);
    patch_all(p, p->cchain, for_offset);
    p->bchain = bchain;
    p->cchain = cchain;

    encode(p, MU_OP_DROP, p->sp, 0, 0, -1);
}

static void p_expr(struct mparse *p) {
    muintq_t depth = p->l.depth; p->l.depth = p->l.paren;
    struct mexpr e = {.prec = -1};
    p_subexpr(p, &e);
    encload(p, &e, 0);
    p->l.depth = depth;
}

static void p_subexpr(struct mparse *p, struct mexpr *e) {
    if (match(p, T_LPAREN)) {
        muintq_t prec = e->prec; e->prec = -1;
        p_subexpr(p, e);
        e->prec = prec;
        expect(p, T_RPAREN);
        p_postexpr(p, e);

    } else if (match(p, T_LTABLE)) {
        struct mframe f = {.unpack = false};
        s_frame(p, &f, false);
        f.tabled = true;
        p_frame(p, &f);
        expect(p, T_RTABLE);
        e->state = P_DIRECT;
        p_postexpr(p, e);

    } else if (lookahead(p, T_ANY_OP, T_EXPR)) {
        scopecheck(p, p->m.val, false);
        encode(p, MU_OP_IMM, p->sp+1, imm(p, mu_inc(p->m.val)), 0, +1);
        encode(p, MU_OP_LOOKUP, p->sp, 0, p->sp, 0);
        muintq_t prec = e->prec; e->prec = p->m.prec;
        p_subexpr(p, e);
        e->prec = prec;
        encload(p, e, 0);
        e->state = P_CALLED;
        e->params = 1;
        p_postexpr(p, e);

    } else if (match(p, T_FN)) {
        p_fn(p, false);
        e->state = P_DIRECT;
        p_postexpr(p, e);

    } else if (match(p, T_IF)) {
        p_if(p, true);
        e->state = P_DIRECT;
        p_postexpr(p, e);

    } else if (match(p, T_IMM)) {
        encode(p, MU_OP_IMM, p->sp+1, imm(p, mu_inc(p->m.val)), 0, +1);
        e->state = P_DIRECT;
        p_postexpr(p, e);

    } else if (match(p, T_NIL)) {
        e->state = P_NIL;
        p_postexpr(p, e);

    } else if (match(p, T_SYM | T_ANY_OP)) {
        scopecheck(p, p->m.val, e->insert);
        encode(p, MU_OP_IMM, p->sp+1, imm(p, mu_inc(p->m.val)), 0, +1);
        e->state = P_SCOPED;
        p_postexpr(p, e);

    } else {
        unexpected(p);
    }
}

static void p_postexpr(struct mparse *p, struct mexpr *e) {
    if (match(p, T_LPAREN)) {
        encload(p, e, 0);
        struct mframe f = {.unpack = false};
        s_frame(p, &f, false);
        f.tabled = f.tabled || f.call;
        p_frame(p, &f);
        expect(p, T_RPAREN);
        e->state = P_CALLED;
        e->params = f.tabled ? 0xf : f.count;
        p_postexpr(p, e);

    } else if (match(p, T_LTABLE)) {
        encload(p, e, 0);
        p_expr(p);
        expect(p, T_RTABLE);
        e->state = P_INDIRECT;
        p_postexpr(p, e);

    } else if (match(p, T_DOT)) {
        expect(p, T_ANY_SYM);
        encload(p, e, 0);
        encode(p, MU_OP_IMM, p->sp+1, imm(p, mu_inc(p->m.val)), 0, +1);
        e->state = P_INDIRECT;
        p_postexpr(p, e);

    } else if (match(p, T_ARROW)) {
        expect(p, T_ANY_SYM);
        mu_t sym = mu_inc(p->m.val);
        if (next(p, T_LPAREN)) {
            struct mlex l = lex_inc(p->l);
            expect(p, T_LPAREN);
            struct mframe f = {.unpack = false};
            s_frame(p, &f, false);
            if (!f.tabled && !f.call && f.target != MU_FRAME) {
                encload(p, e, 1);
                encode(p, MU_OP_IMM, p->sp-1, imm(p, sym), 0, 0);
                encode(p, MU_OP_LOOKUP, p->sp-1, p->sp, p->sp-1, 0);
                p_frame(p, &f);
                expect(p, T_RPAREN);
                e->state = P_CALLED;
                e->params = f.count + 1;
                p_postexpr(p, e);
                return;
            }
            lex_dec(p->l); p->l = l;
        }
        encload(p, e, 2);
        encode(p, MU_OP_IMM, p->sp-1, imm(p, sym), 0, 0);
        encode(p, MU_OP_LOOKUP, p->sp-1, p->sp, p->sp-1, 0);
        encode(p, MU_OP_IMM, p->sp-2, imm(p, MU_BIND_KEY), 0, 0);
        encode(p, MU_OP_LOOKUP, p->sp-2, 0, p->sp-2, 0);
        encode(p, MU_OP_CALL, p->sp-2, 0x21, 0, -2);
        e->state = P_DIRECT;
        p_postexpr(p, e);

    } else if (e->prec > p->l.prec && match(p, T_ANY_OP)) {
        encload(p, e, 1);
        scopecheck(p, p->m.val, false);
        encode(p, MU_OP_IMM, p->sp-1, imm(p, mu_inc(p->m.val)), 0, 0);
        encode(p, MU_OP_LOOKUP, p->sp-1, 0, p->sp-1, 0);
        muintq_t prec = e->prec; e->prec = p->m.prec;
        p_subexpr(p, e);
        encload(p, e, 0);
        e->prec = prec;
        e->state = P_CALLED;
        e->params = 2;
        p_postexpr(p, e);

    } else if (e->prec > p->l.prec && match(p, T_AND)) {
        encload(p, e, 0);
        mlen_t offset = p->bcount;
        encode(p, MU_OP_JFALSE, p->sp, 0, 0, 0);
        encode(p, MU_OP_DROP, p->sp, 0, 0, -1);
        muintq_t prec = e->prec; e->prec = p->m.prec;
        p_subexpr(p, e);
        encload(p, e, 0);
        e->prec = prec;
        patch(p, offset, p->bcount - offset);
        e->state = P_DIRECT;
        p_postexpr(p, e);

    } else if (e->prec > p->l.prec && match(p, T_OR)) {
        encload(p, e, 0);
        mlen_t offset = p->bcount;
        encode(p, MU_OP_JTRUE, p->sp, 0, 0, -1);
        muintq_t prec = e->prec; e->prec = p->m.prec;
        p_subexpr(p, e);
        encload(p, e, 0);
        e->prec = prec;
        patch(p, offset, p->bcount - offset);
        e->state = P_DIRECT;
        p_postexpr(p, e);
    }
}

static void p_entry(struct mparse *p, struct mframe *f) {
    struct mexpr e = {.prec = -1, .insert = f->insert};
    f->key = false;

    if (lookahead(p, T_ANY_SYM, T_PAIR)) {
        encode(p, MU_OP_IMM, p->sp+1, imm(p, mu_inc(p->m.val)), 0, +1);
        e.state = P_DIRECT;
        f->key = true;
    } else if (!(f->unpack && next(p, T_LTABLE))) {
        p_subexpr(p, &e);

        while (f->count == 0 &&
               p->l.paren > p->l.depth &&
               match(p, T_RPAREN)) {
            e.prec = -1;
            p_postexpr(p, &e);
        }
    }

    if (match(p, T_PAIR)) {
        if (f->unpack && f->expand) {
            encload(p, &e, 1);
            encode(p, MU_OP_IMM, p->sp+1, imm(p, 0), 0, +1);
            encode(p, MU_OP_LOOKUP, p->sp-2, p->sp-3, p->sp-1, 0);
            encode(p, MU_OP_INSERT, p->sp, p->sp-3, p->sp-1, -2);
        } else if (f->unpack) {
            encload(p, &e, 0);
            encode(p, f->count == f->target-1 ? MU_OP_LOOKDN : MU_OP_LOOKUP,
                    p->sp, p->sp-1, p->sp,
                    f->count == f->target-1 ? -1 : 0);
        } else {
            encload(p, &e, 0);
        }

        if (f->key && !next(p, T_EXPR)) {
            encode(p, MU_OP_IMM, p->sp+1, imm(p, mu_inc(p->m.val)), 0, +1);
            e.state = P_SCOPED;
        } else if (!(f->unpack && next(p, T_LTABLE))) {
            p_subexpr(p, &e);
        }

        f->key = true;
    } else if (f->tabled) {
        if (f->unpack && f->expand) {
            encode(p, MU_OP_IMM, p->sp+1, imm(p, MU_POP_KEY), 0, +1);
            encode(p, MU_OP_LOOKUP, p->sp, 0, p->sp, 0);
            encode(p, MU_OP_DUP, p->sp+1, p->sp-1-offset(&e), 0, +1);
            encode(p, MU_OP_IMM, p->sp+1,
                    imm(p, mu_num_fromuint(f->index)), 0, +1);
            encode(p, MU_OP_CALL, p->sp-2, 0x21, 0, -2);
        } else if (f->unpack) {
            encode(p, MU_OP_IMM, p->sp+1,
                    imm(p, mu_num_fromuint(f->index)), 0, +1);
            encode(p, f->count == f->target-1 ? MU_OP_LOOKDN : MU_OP_LOOKUP,
                   p->sp, p->sp-1-offset(&e), p->sp, 0);
        }
    } else {
        if (f->unpack && next(p, T_LTABLE) && f->count < f->target-1) {
            encode(p, MU_OP_MOVE, p->sp+1,
                   p->sp - (f->target-1 - f->count), 0, +1);
        }
    }

    if (f->unpack && match(p, T_LTABLE)) {
        struct mframe nf = {.unpack = true, .insert = f->insert};
        s_frame(p, &nf, false);
        nf.tabled = true;
        p_frame(p, &nf);
        p->sp -= f->tabled || f->count < f->target-1;
        f->count -= 1;
        expect(p, T_RTABLE);
    } else if (f->unpack) {
        if (f->key) {
            encstore(p, &e, f->insert, 0);
            p->sp -= 1;
        } else if (f->tabled) {
            p->sp -= 1;
            encstore(p, &e, f->insert, -(offset(&e)+1));
        } else {
            encstore(p, &e, f->insert, f->target-1 - f->count);
        }
    } else {
        encload(p, &e, 0);

        if (f->key) {
            encode(p, MU_OP_INSERT, p->sp, p->sp-2, p->sp-1, -2);
        } else if (f->tabled) {
            encode(p, MU_OP_IMM, p->sp+1,
                    imm(p, mu_num_fromuint(f->index)), 0, +1);
            encode(p, MU_OP_INSERT, p->sp-1, p->sp-2, p->sp, -2);
        } else if (f->count >= f->target) {
            encode(p, MU_OP_DROP, p->sp, 0, 0, -1);
        }
    }
}

static void p_frame(struct mparse *p, struct mframe *f) {
    if (!f->unpack && f->call) {
        struct mexpr e = {.prec = -1, .insert = f->insert};
        p_subexpr(p, &e);
        encode(p, MU_OP_CALL, p->sp-(e.params == 0xf ? 1 : e.params),
               (e.params << 4) | (f->tabled ? 0xf : f->target), 0,
               +(f->tabled ? 1 : f->target)
               -(e.params == 0xf ? 1 : e.params)-1);
        return;
    } else if (!f->unpack && f->tabled && !f->call &&
               !(f->expand && f->target == 0)) {
        encode(p, MU_OP_TBL, p->sp+1, f->count, 0, +1);
    }

    f->count = 0;
    f->index = 0;
    f->depth = p->l.depth; p->l.depth = p->l.paren;

    while (match(p, T_LPAREN)) {}

    do {
        if (!next(p, T_EXPR) || match(p, T_EXPAND)) {
            break;
        }

        p_entry(p, f);
        f->index += !f->key;
        f->count += 1;
    } while (p->l.paren != f->depth && match(p, T_SEP));

    if (f->expand) {
        if (f->unpack) {
            struct mexpr e = {.prec = -1, .insert = f->insert};
            p_subexpr(p, &e);
            encstore(p, &e, f->insert, 0);
            p->sp -= 1;
        } else if (f->count > 0) {
            encode(p, MU_OP_MOVE, p->sp+1, p->sp, 0, +1);
            encode(p, MU_OP_IMM, p->sp-1, imm(p, MU_CONCAT_KEY), 0, 0);
            encode(p, MU_OP_LOOKUP, p->sp-1, 0, p->sp-1, 0);
            p_expr(p);
            encode(p, MU_OP_IMM, p->sp+1,
                    imm(p, mu_num_fromuint(f->index)), 0, +1);
            encode(p, MU_OP_CALL, p->sp-3, 0x31, 0, -3);
        } else {
            p_expr(p);
        }
    }

    if (f->unpack && !f->tabled) {
        p->sp -= f->count;
    } else if (!f->unpack && f->tabled && f->flatten) {
        encode(p, MU_OP_MOVE, p->sp + f->target, p->sp, 0, +f->target);

        for (mlen_t i = 0; i < f->target; i++) {
            encode(p, MU_OP_IMM, p->sp-1 - (f->target-1-i),
                   imm(p, mu_num_fromuint(i)), 0, 0);
            encode(p, i == f->target-1 ? MU_OP_LOOKDN : MU_OP_LOOKUP,
                   p->sp-1 - (f->target-1-i), p->sp,
                   p->sp-1 - (f->target-1-i),
                   -(f->target-1 == i));
        }
    } else if (!f->unpack && !f->tabled) {
        while (f->target > f->count) {
            encode(p, MU_OP_IMM, p->sp+1, imm(p, 0), 0, +1);
            f->count++;
        }
    }

    while (p->l.paren > p->l.depth) {
        expect(p, T_RPAREN);
    }

    if (next(p, T_EXPR)) {
        unexpected(p);
    }

    p->l.depth = f->depth;
}

static void p_assign(struct mparse *p, bool insert) {
    struct mlex ll = lex_inc(p->l);
    struct mframe fl = {.unpack = false, .insert = insert};
    s_frame(p, &fl, true);

    if (match(p, T_ASSIGN)) {
        struct mframe fr = {.unpack = false};
        s_frame(p, &fr, false);

        mu_checkassign(
                (fr.count != 0 || fr.tabled) &&
                (fl.count != 0 || fl.tabled), &p->l);

        fr.tabled = fr.tabled || fl.tabled;
        fr.target = fl.count;
        fr.flatten = !fl.tabled;
        p_frame(p, &fr);

        struct mlex lr = p->l;
        p->l = ll;

        fl.unpack = true;
        p_frame(p, &fl);
        expect(p, T_ASSIGN);
        lex_dec(p->l); p->l = lr;
    } else if (!insert) {
        lex_dec(p->l); p->l = ll;

        fl.unpack = false;
        fl.tabled = false;
        fl.target = 0;
        p_frame(p, &fl);
    } else {
        unexpected(p);
    }
}

static void p_return(struct mparse *p) {
    // Remove any leftover iterators
    muintq_t sp = p->sp;
    while (p->sp != 0) {
        encode(p, MU_OP_DROP, p->sp, 0, 0, -1);
    }

    struct mframe f = {.unpack = false};
    s_frame(p, &f, false);

    if (f.call) {
        struct mexpr e = {.prec = -1};
        p_subexpr(p, &e);
        encode(p, MU_OP_TCALL,
               p->sp - (e.params == 0xf ? 1 : e.params),
               e.params, 0,
               -(e.params == 0xf ? 1 : e.params)-1);
    } else {
        p_frame(p, &f);
        encode(p, MU_OP_RET,
               p->sp - (f.tabled ? 0 : f.count-1),
               f.tabled ? 0xf : f.count, 0,
               -(f.tabled ? 1 : f.count));
    }

    p->sp = sp;
}

static void p_stmt(struct mparse *p) {
    if (next(p, T_LBLOCK)) {
        p_block(p, false);

    } else if (lookahead(p, T_FN, T_ANY_SYM | T_ANY_OP)) {
        expect(p, T_ANY_SYM | T_ANY_OP);
        mu_t sym = mu_inc(p->m.val);
        scopecheck(p, sym, true);
        p_fn(p, true);
        encode(p, MU_OP_IMM, p->sp+1, imm(p, sym), 0, +1);
        encode(p, MU_OP_INSERT, p->sp-1, 0, p->sp, -2);

    } else if (match(p, T_IF)) {
        p_if(p, false);

    } else if (match(p, T_WHILE)) {
        p_while(p);

    } else if (match(p, T_FOR)) {
        p_for(p);

    } else if (match(p, T_BREAK)) {
        mu_checkparse(p->bchain != (mlen_t)-1,
                &p->l, "break outside of loop");

        mlen_t offset = p->bcount;
        encode(p, MU_OP_JUMP, 0, p->bchain ? p->bchain-p->bcount : 0, 0, 0);
        p->bchain = offset;

    } else if (match(p, T_CONTINUE)) {
        mu_checkparse(p->bchain != (mlen_t)-1,
                &p->l, "continue outside of loop");

        mlen_t offset = p->bcount;
        encode(p, MU_OP_JUMP, 0, p->cchain ? p->cchain-p->bcount : 0, 0, 0);
        p->cchain = offset;

    } else if (match(p, T_ARROW | T_RETURN)) {
        p_return(p);

    } else if (match(p, T_LET)) {
        p_assign(p, true);

    } else {
        p_assign(p, false);
    }
}

static void p_block(struct mparse *p, bool root) {
    muintq_t block = p->l.block;
    muintq_t paren = p->l.paren; p->l.paren = 0;
    muintq_t depth = p->l.depth; p->l.depth = -1;

    while (match(p, T_LBLOCK)) {}

    do {
        p_stmt(p);
    } while ((root || p->l.block > block) &&
             match(p, T_TERM | T_LBLOCK | T_RBLOCK));

    if (p->l.block > block) {
        expect(p, T_RBLOCK);
    }

    p->l.paren = paren;
    p->l.depth = depth;
}


//// Parsing functions ////
MU_DEF_STR(mu_cdata_key_def, "cdata")
static mu_t (*const mu_attr_name[8])(void) = {
    [MTNIL]  = mu_kw_nil_def,
    [MTNUM]  = mu_num_key_def,
    [MTSTR]  = mu_str_key_def,
    [MTTBL]  = mu_tbl_key_def,
    [MTRTBL] = mu_tbl_key_def,
    [MTFN]   = mu_kw_fn_def,
    [MTBUF]  = mu_cdata_key_def,
    [MTDBUF] = mu_cdata_key_def,
};

mu_t mu_repr(mu_t m, mu_t depth) {
    mu_t r;
    switch (mu_gettype(m)) {
        case MTNIL:  r = MU_KW_NIL; break;
        case MTNUM:  r = mu_num_repr(m); break;
        case MTSTR:  r = mu_str_repr(m); break;
        case MTTBL:
        case MTRTBL: r = mu_tbl_repr(m, depth); break;
        default:
            r = mu_str_format("<%m 0x%wx>",
                    mu_attr_name[mu_gettype(m)](),
                    (muint_t)m & ~7);
            break;
    }

    mu_dec(m);
    return r;
}

mu_t mu_parsen(const mbyte_t **ppos, const mbyte_t *end) {
    const mbyte_t *pos = *ppos;
    mu_t val;
    bool sym = false;

    while (pos < end) {
        if (*pos == '#') {
            while (pos < end && *pos != '\n') {
                pos++;
            }
        } else if (class[*pos] == L_WS) {
            pos++;
        } else {
            break;
        }
    }

    switch (class[*pos]) {
        case L_OP:
        case L_NUM:     val = mu_num_parsen(&pos, end); break;
        case L_STR:     val = mu_str_parsen(&pos, end); break;
        case L_LTABLE:  val = mu_tbl_parsen(&pos, end); break;

        case L_KW: {
            const mbyte_t *start = pos++;
            while (pos < end && (class[*pos] == L_KW ||
                                 class[*pos] == L_NUM))
                pos++;

            val = mu_str_fromdata(start, pos-start);
            sym = true;
        } break;

        default:        return 0;
    }

    while (pos < end) {
        if (*pos == '#') {
            while (pos < end && *pos != '\n') {
                pos++;
            }
        } else if (class[*pos] == L_WS) {
            pos++;
        } else {
            break;
        }
    }

    if (!(!sym || *pos == ':')) {
        mu_dec(val);
        return 0;
    }

    *ppos = pos;
    return val;
}

mu_t mu_parse(const char *s, muint_t n) {
    const mbyte_t *pos = (const mbyte_t *)s;
    const mbyte_t *end = (const mbyte_t *)pos + n;

    mu_t v = mu_parsen(&pos, end);

    if (pos != end) {
        mu_dec(v);
        return 0;
    }

    return v;
}

mu_t mu_compilen(const mbyte_t **pos, const mbyte_t *end, mu_t scope) {
    struct mparse p = {
        .scope = mu_tbl_createtail(0, scope),
        .bcode = mu_buf_create(0),
        .imms = mu_tbl_create(0),
        .bchain = -1,
        .cchain = -1,
        .regs = 1,
    };

    lex_init(&p.l, *pos, end);
    p_block(&p, true);
    *pos = p.l.pos;

    encode(&p, MU_OP_RET, 0, 0, 0, 0);
    lex_dec(p.l);
    return compile(&p, false);
}

mu_t mu_compile(const char *s, muint_t n, mu_t scope) {
    struct mparse p = {
        .scope = mu_tbl_createtail(0, scope),
        .bcode = mu_buf_create(0),
        .imms = mu_tbl_create(0),
        .bchain = -1,
        .cchain = -1,
        .regs = 1,
    };

    lex_init(&p.l, (const mbyte_t *)s, (const mbyte_t *)s+n);
    p_block(&p, true);
    if (p.l.tok) {
        unexpected(&p);
    }

    encode(&p, MU_OP_RET, 0, 0, 0, 0);
    lex_dec(p.l);
    return compile(&p, false);
}

