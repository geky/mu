#include "parse.h"

#include "vm.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"


//// Definitions ////

// Tokens
typedef uint32_t mtok_t;
enum tok {
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
MSTR(mu_kw_let,         "let")      MUINT(mu_tok_let,     T_LET)
MSTR(mu_kw_else,        "else")     MUINT(mu_tok_else,    T_ELSE)
MSTR(mu_kw_and,         "and")      MUINT(mu_tok_and,     T_AND)
MSTR(mu_kw_or,          "or")       MUINT(mu_tok_or,      T_OR)
MSTR(mu_kw_continue,    "continue") MUINT(mu_tok_cont,    T_CONTINUE)
MSTR(mu_kw_break,       "break")    MUINT(mu_tok_break,   T_BREAK)
MSTR(mu_kw_return,      "return")   MUINT(mu_tok_return,  T_RETURN)
MSTR(mu_kw_fn,          "fn")       MUINT(mu_tok_fn,      T_FN)
MSTR(mu_kw_type,        "type")     MUINT(mu_tok_type,    T_TYPE)
MSTR(mu_kw_if,          "if")       MUINT(mu_tok_if,      T_IF)
MSTR(mu_kw_while,       "while")    MUINT(mu_tok_while,   T_WHILE)
MSTR(mu_kw_for,         "for")      MUINT(mu_tok_for,     T_FOR)
MSTR(mu_kw_nil,         "nil")      MUINT(mu_tok_nil,     T_NIL)
MSTR(mu_kw_nil2,        "_")
MSTR(mu_kw_assign,      "=")        MUINT(mu_tok_assign,  T_ASSIGN)
MSTR(mu_kw_pair,        ":")        MUINT(mu_tok_pair,    T_PAIR)
MSTR(mu_kw_dot,         ".")        MUINT(mu_tok_dot,     T_DOT)
MSTR(mu_kw_arrow,       "->")       MUINT(mu_tok_arrow,   T_ARROW)
MSTR(mu_kw_expand,      "..")       MUINT(mu_tok_expand,  T_EXPAND)

MTBL(mu_keywords, {
    { mu_kw_let,        mu_tok_let    },
    { mu_kw_else,       mu_tok_else   },
    { mu_kw_and,        mu_tok_and    },
    { mu_kw_or,         mu_tok_or     },
    { mu_kw_continue,   mu_tok_cont   },
    { mu_kw_break,      mu_tok_break  },
    { mu_kw_return,     mu_tok_return },
    { mu_kw_fn,         mu_tok_fn     },
    { mu_kw_type,       mu_tok_type   },
    { mu_kw_if,         mu_tok_if     },
    { mu_kw_while,      mu_tok_while  },
    { mu_kw_for,        mu_tok_for    },
    { mu_kw_nil,        mu_tok_nil    },
    { mu_kw_nil2,       mu_tok_nil    },
    { mu_kw_assign,     mu_tok_assign },
    { mu_kw_pair,       mu_tok_pair   },
    { mu_kw_dot,        mu_tok_dot    },
    { mu_kw_arrow,      mu_tok_arrow  },
    { mu_kw_expand,     mu_tok_expand },
})


// Classification for individual characters
typedef uint8_t mclass_t;
enum class {
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
enum state {
    P_DIRECT,
    P_INDIRECT,
    P_SCOPED,
    P_CALLED,
    P_NIL
};


//// Structures ////

// Lexical analysis state
struct lex {
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
struct match {
    mu_t val;
    muintq_t prec;
};

// Parsing state
struct parse {
    mu_t imms;
    mu_t fns;

    mu_t bcode;
    mlen_t bcount;

    mlen_t bchain;
    mlen_t cchain;

    muintq_t args;
    muintq_t scope;
    muintq_t regs;
    muintq_t sp;

    muintq_t depth;
    struct lex l;
    struct match m;
};

// Frame state
struct frame {
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
struct expr {
    muintq_t prec;
    muintq_t params;
    mstate_t state;
};


//// Error handling ////

// Common errors
static mu_noreturn mu_error_parse(struct lex *l, mu_t message) {
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

    if (lines == 1) {
        mu_errorf("%m", message);
    } else {
        mu_errorf("%m on line %wu", message, lines);
    }
}

static mu_noreturn mu_error_character(struct lex *l) {
    mu_error_parse(l, str_format("unexpected %r", str_create(l->pos, 1)));
}

static mu_noreturn mu_error_token(struct lex *l) {
    mu_t b = buf_create(0);
    muint_t n = 0;
    buf_format(&b, &n, "unexpected ");

    if (l->tok & T_ANY_VAL) {
        buf_concat(&b, &n, mu_repr(mu_inc(l->val)));
    } else if (l->tok & T_TERM) {
        buf_format(&b, &n, "terminator");
    } else if (l->tok & T_SEP) {
        buf_format(&b, &n, "','");
    } else if (l->tok & T_LPAREN) {
        buf_format(&b, &n, "'('");
    } else if (l->tok & T_RPAREN) {
        buf_format(&b, &n, "')'");
    } else if (l->tok & T_LTABLE) {
        buf_format(&b, &n, "'['");
    } else if (l->tok & T_RTABLE) {
        buf_format(&b, &n, "']'");
    } else if (l->tok & T_LBLOCK) {
        buf_format(&b, &n, "'{'");
    } else if (l->tok & T_RBLOCK) {
        buf_format(&b, &n, "'}'");
    } else {
        buf_format(&b, &n, "end");
    }

    mu_error_parse(l, str_intern(b, n));
}

static mu_noreturn mu_error_assignment(struct lex *l) {
    mu_error_parse(l, mstr("invalid assignment"));
}

static mu_noreturn mu_error_expression(mbyte_t c) {
    mu_errorf("unexpected %r in expression", str_create(&c, 1));
}


//// Lexical analysis ////

// Lexer definitions for non-trivial tokens
static void l_indent(struct lex *l) {
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

static void l_op(struct lex *l) {
    const mbyte_t *begin = l->pos++;

    while (l->pos < l->end && class[*l->pos] == L_OP) {
        l->pos++;
    }

    l->val = str_create(begin, l->pos-begin);
    mu_t tok = tbl_lookup(MU_KEYWORDS, mu_inc(l->val));
    l->tok = tok ? num_uint(tok) : T_OP;
}

static void l_kw(struct lex *l) {
    const mbyte_t *begin = l->pos++;

    while (l->pos < l->end && (class[*l->pos] == L_KW ||
                               class[*l->pos] == L_NUM))
        l->pos++;

    l->val = str_create(begin, l->pos-begin);
    mu_t tok = tbl_lookup(MU_KEYWORDS, mu_inc(l->val));
    l->tok = tok ? num_uint(tok) : T_SYM;
}

static void l_num(struct lex *l) {
    l->val = num_parse(&l->pos, l->end);
    l->tok = T_IMM;
}

static void l_str(struct lex *l) {
    l->val = str_parse(&l->pos, l->end);
    l->tok = T_IMM;
}


static void lex_next(struct lex *l) {
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
        case L_NONE:    mu_error_character(l);

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

static void lex_init(struct lex *l, const mbyte_t *pos, const mbyte_t *end) {
    l->begin = pos;
    l->pos = pos;
    l->end = end;

    lex_next(l);
}

mu_inline struct lex lex_inc(struct lex l) {
    mu_inc(l.val);
    return l;
}

mu_inline void lex_dec(struct lex l) {
    mu_dec(l.val);
}


//// Lexing shortcuts ////
static mu_noreturn unexpected(struct parse *p) {
    mu_error_token(&p->l);
}

static bool next(struct parse *p, mtok_t tok) {
    return p->l.tok & tok;
}

static bool match(struct parse *p, mtok_t tok) {
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

static void expect(struct parse *p, mtok_t tok) {
    if (!match(p, tok)) {
        unexpected(p);
    }
}

static bool lookahead(struct parse *p, mtok_t a, mtok_t b) {
    if (!next(p, a)) {
        return false;
    }

    struct lex l = lex_inc(p->l);
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
static void emit(struct parse *p, mbyte_t byte) {
    muint_t bcount = p->bcount;
    buf_push(&p->bcode, &bcount, byte);
    p->bcount = bcount;
}

static void encode(struct parse *p, enum op op,
                   minth_t d, minth_t a, minth_t b,
                   mint_t sdiff) {
    p->sp += sdiff;

    if (p->sp+1 > p->regs) {
        p->regs = p->sp+1;
    }

    mu_encode((void (*)(void *, mbyte_t))emit, p, op, d, a, b);
}

static void patch(struct parse *p, mlen_t offset, minth_t j) {
    mbyte_t *bcode = buf_data(p->bcode);
    mu_patch(&bcode[offset], j);
}

static void patch_all(struct parse *p, mlen_t chain, minth_t offset) {
    muint_t current = 0;
    mbyte_t *bcode = buf_data(p->bcode);

    while (chain) {
        current += chain;
        chain = mu_patch(&bcode[current], offset - current);
    }
}

// Storing immediates/code objects
#define IMM_NIL imm_nil()
MBFN(imm_nil, 0, 0)

static muint_t imm(struct parse *p, mu_t m) {
    if (!m) {
        m = IMM_NIL;
    }

    mu_t mindex = tbl_lookup(p->imms, mu_inc(m));

    if (mindex) {
        mu_dec(m);
        return num_uint(mindex);
    }

    muint_t index = tbl_len(p->imms);
    tbl_insert(p->imms, m, muint(index));
    return index;
}

static muint_t fn(struct parse *p, struct code *code) {
    muint_t index = tbl_len(p->fns);
    tbl_insert(p->fns, muint(index), mu_wrap(code));
    return index;
}

// More complicated encoding operations
static muint_t offset(struct expr *e) {
    if (e->state == P_INDIRECT) {
        return 2;
    } else if (e->state == P_SCOPED) {
        return 1;
    } else {
        return 0;
    }
}

static void encode_load(struct parse *p, struct expr *e, mint_t offset) {
    if (e->state == P_INDIRECT) {
        encode(p, OP_LOOKDN, p->sp+offset-1, p->sp-1, p->sp, +offset-1);
    } else if (e->state == P_SCOPED) {
        encode(p, OP_LOOKUP, p->sp+offset, 0, p->sp, +offset);
    } else if (e->state == P_NIL) {
        encode(p, OP_IMM, p->sp+offset+1, imm(p, 0), 0, +offset+1);
    } else {
        if (e->state == P_CALLED) {
            encode(p, OP_CALL, p->sp-(e->params == 0xf ? 1 : e->params),
                   (e->params << 4) | 1, 0,
                   -(e->params == 0xf ? 1 : e->params));
        }

        if (offset != 0) {
            encode(p, OP_MOVE, p->sp+offset, p->sp, 0, +offset);
        }
    }
}

static void encode_store(struct parse *p, struct expr *e,
                         bool insert, mint_t offset) {
    if (e->state == P_NIL) {
        encode(p, OP_DROP, p->sp-offset, 0, 0, 0);
    } else if (e->state == P_SCOPED) {
        encode(p, insert ? OP_INSERT : OP_ASSIGN,
               p->sp-offset-1, 0, p->sp, -1);
    } else if (e->state == P_INDIRECT) {
        encode(p, insert ? OP_INSERT : OP_ASSIGN,
               p->sp-offset-2, p->sp-1, p->sp, 0);
        encode(p, OP_DROP, p->sp-1, 0, 0, -2);
    } else {
        mu_error_assignment(&p->l);
    }
}

// Completing a parse and generating the final code object
static struct code *compile(struct parse *p) {
    struct code *code = ref_alloc(
        sizeof(struct code) +
        sizeof(mu_t)*tbl_len(p->imms) +
        sizeof(struct code *)*tbl_len(p->fns) +
        p->bcount);

    code->args = p->args;
    code->type = 0;
    code->regs = p->regs;
    code->scope = p->scope;
    code->icount = tbl_len(p->imms);
    code->fcount = tbl_len(p->fns);
    code->bcount = p->bcount;

    mu_t *imms = code_imms(code);
    struct code **fns = code_fns(code);
    mbyte_t *bcode = (mbyte_t *)code_bcode(code);

    mu_t k, v;
    for (muint_t i = 0; tbl_next(p->imms, &i, &k, &v);) {
        imms[num_uint(v)] = (k == IMM_NIL) ? 0 : k;
    }

    for (muint_t i = 0; tbl_next(p->fns, &i, &k, &v);) {
        fns[num_uint(k)] = mu_unwrap(v);
    }

    memcpy(bcode, buf_data(p->bcode), p->bcount);

    tbl_dec(p->imms);
    tbl_dec(p->fns);
    buf_dec(p->bcode);
    mu_dec(p->m.val);

    return code;
}


//// Scanning rules ////
static void s_block(struct parse *p, struct frame *f);
static void s_expr(struct parse *p, struct frame *f, muintq_t prec);
static void s_frame(struct parse *p, struct frame *f, bool update);

static void s_block(struct parse *p, struct frame *f) {
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

static void s_expr(struct parse *p, struct frame *f, muintq_t prec) {
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

static void s_frame(struct parse *p, struct frame *f, bool update) {
    struct lex l = lex_inc(p->l);
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
static void p_fn(struct parse *p);
static void p_if(struct parse *p, bool expr);
static void p_while(struct parse *p);
static void p_for(struct parse *p);
static void p_expr(struct parse *p);
static void p_subexpr(struct parse *p, struct expr *e);
static void p_postexpr(struct parse *p, struct expr *e);
static void p_entry(struct parse *p, struct frame *f);
static void p_frame(struct parse *p, struct frame *f);
static void p_assign(struct parse *p, bool insert);
static void p_return(struct parse *p);
static void p_stmt(struct parse *p);
static void p_block(struct parse *p, bool root);

static void p_fn(struct parse *p) {
    struct parse q = {
        .bcode = buf_create(0),
        .bcount = 0,

        .imms = tbl_create(0),
        .fns = tbl_create(0),
        .bchain = -1,
        .cchain = -1,

        .regs = 1,
        .scope = MU_MINALLOC / sizeof(muint_t),

        .l = p->l,
    };

    expect(&q, T_LPAREN);
    struct frame f = {.unpack = true, .insert = true};
    s_frame(&q, &f, false);
    q.sp = f.tabled ? 1 : f.count;
    q.args = f.tabled ? 0xf : f.count;
    p_frame(&q, &f);
    expect(&q, T_RPAREN);

    p_stmt(&q);
    encode(&q, OP_RET, 0, 0, 0, 0);

    p->l = q.l;
    encode(p, OP_FN, p->sp+1, fn(p, compile(&q)), 0, +1);
}

static void p_if(struct parse *p, bool expr) {
    expect(p, T_LPAREN);
    p_expr(p);
    expect(p, T_RPAREN);

    mlen_t cond_offset = p->bcount;
    encode(p, OP_JFALSE, p->sp, 0, 0, 0);
    encode(p, OP_DROP, p->sp, 0, 0, -1);

    if (expr) {
        p_expr(p);
    } else {
        p_stmt(p);
    }

    if (next(p, T_ELSE) || (!expr && lookahead(p, T_TERM, T_ELSE))) {
        expect(p, T_ELSE);
        mlen_t exit_offset = p->bcount;
        encode(p, OP_JUMP, 0, 0, 0, -expr);
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

static void p_while(struct parse *p) {
    mlen_t while_offset = p->bcount;
    expect(p, T_LPAREN);
    p_expr(p);
    expect(p, T_RPAREN);

    mlen_t cond_offset = p->bcount;
    encode(p, OP_JFALSE, p->sp, 0, 0, 0);
    encode(p, OP_DROP, p->sp, 0, 0, -1);

    mlen_t bchain = p->bchain; p->bchain = 0;
    mlen_t cchain = p->cchain; p->cchain = 0;

    p_stmt(p);

    encode(p, OP_JUMP, 0, while_offset - p->bcount, 0, 0);
    patch(p, cond_offset, p->bcount - cond_offset);

    patch_all(p, p->bchain, p->bcount);
    patch_all(p, p->cchain, while_offset);
    p->bchain = bchain;
    p->cchain = cchain;
}

static void p_for(struct parse *p) {
    expect(p, T_LPAREN);
    struct lex ll = lex_inc(p->l);
    struct frame f = {.unpack = true, .insert = true};
    s_frame(p, &f, true);

    expect(p, T_ASSIGN);
    if (f.count == 0 && !f.tabled) {
        mu_error_assignment(&p->l);
    }

    encode(p, OP_IMM, p->sp+1, imm(p, MU_ITER_KEY), 0, +1);
    encode(p, OP_LOOKUP, p->sp, 0, p->sp, 0);
    p_expr(p);
    encode(p, OP_CALL, p->sp-1, 0x11, 0, -1);

    mlen_t for_offset = p->bcount;
    mlen_t cond_offset;
    encode(p, OP_DUP, p->sp+1, p->sp, 0, +1);
    if (f.tabled) {
        encode(p, OP_CALL, p->sp, 0x0f, 0, 0);
        encode(p, OP_IMM, p->sp+1, imm(p, muint(0)), 0, +1);
        encode(p, OP_LOOKUP, p->sp, p->sp-1, p->sp, 0);
        cond_offset = p->bcount;
        encode(p, OP_JFALSE, p->sp, 0, 0, 0);
        encode(p, OP_DROP, p->sp, 0, 0, -1);
    } else {
        encode(p, OP_CALL, p->sp, 0 | f.count, 0, +f.count-1);
        cond_offset = p->bcount;
        encode(p, OP_JFALSE, p->sp-f.count+1, 0, 0, 0);
    }
    mlen_t count = f.tabled ? 1 : f.count;
    struct lex lr = p->l;
    p->l = ll;

    p_frame(p, &f);
    expect(p, T_ASSIGN);
    lex_dec(p->l); p->l = lr;
    expect(p, T_RPAREN);


    mlen_t bchain = p->bchain; p->bchain = 0;
    mlen_t cchain = p->cchain; p->cchain = 0;

    p_stmt(p);

    encode(p, OP_JUMP, 0, for_offset - p->bcount, 0, 0);
    patch(p, cond_offset, p->bcount - cond_offset);
    for (muint_t i = 0; i < count; i++) {
        encode(p, OP_DROP, p->sp+1 + i, 0, 0, 0);
    }

    patch_all(p, p->bchain, p->bcount);
    patch_all(p, p->cchain, for_offset);
    p->bchain = bchain;
    p->cchain = cchain;

    encode(p, OP_DROP, p->sp, 0, 0, -1);
}

static void p_expr(struct parse *p) {
    muintq_t depth = p->l.depth; p->l.depth = p->l.paren;
    struct expr e = {.prec = -1};
    p_subexpr(p, &e);
    encode_load(p, &e, 0);
    p->l.depth = depth;
}

static void p_subexpr(struct parse *p, struct expr *e) {
    if (match(p, T_LPAREN)) {
        muintq_t prec = e->prec; e->prec = -1;
        p_subexpr(p, e);
        e->prec = prec;
        expect(p, T_RPAREN);
        return p_postexpr(p, e);

    } else if (match(p, T_LTABLE)) {
        struct frame f = {.unpack = false};
        s_frame(p, &f, false);
        f.tabled = true;
        p_frame(p, &f);
        expect(p, T_RTABLE);
        e->state = P_DIRECT;
        return p_postexpr(p, e);

    } else if (lookahead(p, T_ANY_OP, T_EXPR)) {
        encode(p, OP_IMM, p->sp+1, imm(p, mu_inc(p->m.val)), 0, +1);
        encode(p, OP_LOOKUP, p->sp, 0, p->sp, 0);
        muintq_t prec = e->prec; e->prec = p->m.prec;
        p_subexpr(p, e);
        e->prec = prec;
        encode_load(p, e, 0);
        e->state = P_CALLED;
        e->params = 1;
        return p_postexpr(p, e);

    } else if (match(p, T_FN)) {
        p_fn(p);
        e->state = P_DIRECT;
        return p_postexpr(p, e);

    } else if (match(p, T_IF)) {
        p_if(p, true);
        e->state = P_DIRECT;
        return p_postexpr(p, e);

    } else if (match(p, T_IMM)) {
        encode(p, OP_IMM, p->sp+1, imm(p, mu_inc(p->m.val)), 0, +1);
        e->state = P_DIRECT;
        return p_postexpr(p, e);

    } else if (match(p, T_NIL)) {
        e->state = P_NIL;
        return p_postexpr(p, e);

    } else if (match(p, T_SYM | T_ANY_OP)) {
        encode(p, OP_IMM, p->sp+1, imm(p, mu_inc(p->m.val)), 0, +1);
        e->state = P_SCOPED;
        return p_postexpr(p, e);
    }

    unexpected(p);
}

static void p_postexpr(struct parse *p, struct expr *e) {
    if (match(p, T_LPAREN)) {
        encode_load(p, e, 0);
        struct frame f = {.unpack = false};
        s_frame(p, &f, false);
        f.tabled = f.tabled || f.call;
        p_frame(p, &f);
        expect(p, T_RPAREN);
        e->state = P_CALLED;
        e->params = f.tabled ? 0xf : f.count;
        return p_postexpr(p, e);

    } else if (match(p, T_LTABLE)) {
        encode_load(p, e, 0);
        p_expr(p);
        expect(p, T_RTABLE);
        e->state = P_INDIRECT;
        return p_postexpr(p, e);

    } else if (match(p, T_DOT)) {
        expect(p, T_ANY_SYM);
        encode_load(p, e, 0);
        encode(p, OP_IMM, p->sp+1, imm(p, mu_inc(p->m.val)), 0, +1);
        e->state = P_INDIRECT;
        return p_postexpr(p, e);

    } else if (match(p, T_ARROW)) {
        expect(p, T_ANY_SYM);
        mu_t sym = mu_inc(p->m.val);
        if (next(p, T_LPAREN)) {
            struct lex l = lex_inc(p->l);
            expect(p, T_LPAREN);
            struct frame f = {.unpack = false};
            s_frame(p, &f, false);
            if (!f.tabled && !f.call && f.target != MU_FRAME) {
                encode_load(p, e, 1);
                encode(p, OP_IMM, p->sp-1, imm(p, sym), 0, 0);
                encode(p, OP_LOOKUP, p->sp-1, p->sp, p->sp-1, 0);
                p_frame(p, &f);
                expect(p, T_RPAREN);
                e->state = P_CALLED;
                e->params = f.count + 1;
                return p_postexpr(p, e);
            }
            lex_dec(p->l); p->l = l;
        }
        encode_load(p, e, 2);
        encode(p, OP_IMM, p->sp-1, imm(p, sym), 0, 0);
        encode(p, OP_LOOKUP, p->sp-1, p->sp, p->sp-1, 0);
        encode(p, OP_IMM, p->sp-2, imm(p, MU_BIND_KEY), 0, 0);
        encode(p, OP_LOOKUP, p->sp-2, 0, p->sp-2, 0);
        encode(p, OP_CALL, p->sp-2, 0x21, 0, -2);
        e->state = P_DIRECT;
        return p_postexpr(p, e);

    } else if (e->prec > p->l.prec && match(p, T_ANY_OP)) {
        encode_load(p, e, 1);
        encode(p, OP_IMM, p->sp-1, imm(p, mu_inc(p->m.val)), 0, 0);
        encode(p, OP_LOOKUP, p->sp-1, 0, p->sp-1, 0);
        muintq_t prec = e->prec; e->prec = p->m.prec;
        p_subexpr(p, e);
        encode_load(p, e, 0);
        e->prec = prec;
        e->state = P_CALLED;
        e->params = 2;
        return p_postexpr(p, e);

    } else if (e->prec > p->l.prec && match(p, T_AND)) {
        encode_load(p, e, 0);
        mlen_t offset = p->bcount;
        encode(p, OP_JFALSE, p->sp, 0, 0, 0);
        encode(p, OP_DROP, p->sp, 0, 0, -1);
        muintq_t prec = e->prec; e->prec = p->m.prec;
        p_subexpr(p, e);
        encode_load(p, e, 0);
        e->prec = prec;
        patch(p, offset, p->bcount - offset);
        e->state = P_DIRECT;
        return p_postexpr(p, e);

    } else if (e->prec > p->l.prec && match(p, T_OR)) {
        encode_load(p, e, 0);
        mlen_t offset = p->bcount;
        encode(p, OP_JTRUE, p->sp, 0, 0, -1);
        muintq_t prec = e->prec; e->prec = p->m.prec;
        p_subexpr(p, e);
        encode_load(p, e, 0);
        e->prec = prec;
        patch(p, offset, p->bcount - offset);
        e->state = P_DIRECT;
        return p_postexpr(p, e);
    }
}

static void p_entry(struct parse *p, struct frame *f) {
    struct expr e = {.prec = -1};
    f->key = false;

    if (lookahead(p, T_ANY_SYM, T_PAIR)) {
        encode(p, OP_IMM, p->sp+1, imm(p, mu_inc(p->m.val)), 0, +1);
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
            encode_load(p, &e, 1);
            encode(p, OP_IMM, p->sp+1, imm(p, 0), 0, +1);
            encode(p, OP_LOOKUP, p->sp-2, p->sp-3, p->sp-1, 0);
            encode(p, OP_INSERT, p->sp, p->sp-3, p->sp-1, -2);
        } else if (f->unpack) {
            encode_load(p, &e, 0);
            encode(p, f->count == f->target-1 ? OP_LOOKDN : OP_LOOKUP,
                   p->sp, p->sp-1, p->sp, 0);
        } else {
            encode_load(p, &e, 0);
        }

        if (f->key && !next(p, T_EXPR)) {
            encode(p, OP_IMM, p->sp+1, imm(p, mu_inc(p->m.val)), 0, +1);
            e.state = P_SCOPED;
        } else if (!(f->unpack && next(p, T_LTABLE))) {
            p_subexpr(p, &e);
        }

        f->key = true;
    } else if (f->tabled) {
        if (f->unpack && f->expand) {
            encode(p, OP_IMM, p->sp+1, imm(p, MU_POP_KEY), 0, +1);
            encode(p, OP_LOOKUP, p->sp, 0, p->sp, 0);
            encode(p, OP_DUP, p->sp+1, p->sp-1-offset(&e), 0, +1);
            encode(p, OP_IMM, p->sp+1, imm(p, muint(f->index)), 0, +1);
            encode(p, OP_CALL, p->sp-2, 0x21, 0, -2);
        } else if (f->unpack) {
            encode(p, OP_IMM, p->sp+1, imm(p, muint(f->index)), 0, +1);
            encode(p, f->count == f->target-1 ? OP_LOOKDN : OP_LOOKUP,
                   p->sp, p->sp-1-offset(&e), p->sp, 0);
        }
    } else {
        if (f->unpack && next(p, T_LTABLE) && f->count < f->target-1) {
            encode(p, OP_MOVE, p->sp+1,
                   p->sp - (f->target-1 - f->count), 0, +1);
        }
    }

    if (f->unpack && match(p, T_LTABLE)) {
        struct frame nf = {.unpack = true};
        s_frame(p, &nf, false);
        nf.tabled = true;
        p_frame(p, &nf);
        p->sp -= f->tabled || f->count < f->target-1;
        f->count -= 1;
        expect(p, T_RTABLE);
    } else if (f->unpack) {
        if (f->key) {
            encode_store(p, &e, f->insert, 0);
            p->sp -= 1;
        } else if (f->tabled) {
            p->sp -= 1;
            encode_store(p, &e, f->insert, -(offset(&e)+1));
        } else {
            encode_store(p, &e, f->insert, f->target-1 - f->count);
        }
    } else {
        encode_load(p, &e, 0);

        if (f->key) {
            encode(p, OP_INSERT, p->sp, p->sp-2, p->sp-1, -2);
        } else if (f->tabled) {
            encode(p, OP_IMM, p->sp+1, imm(p, muint(f->index)), 0, +1);
            encode(p, OP_INSERT, p->sp-1, p->sp-2, p->sp, -2);
        } else if (f->count >= f->target) {
            encode(p, OP_DROP, p->sp, 0, 0, -1);
        }
    }
}

static void p_frame(struct parse *p, struct frame *f) {
    if (!f->unpack && f->call) {
        struct expr e = {.prec = -1};
        p_subexpr(p, &e);
        encode(p, OP_CALL, p->sp-(e.params == 0xf ? 1 : e.params),
               (e.params << 4) | (f->tabled ? 0xf : f->target), 0,
               +(f->tabled ? 1 : f->target)
               -(e.params == 0xf ? 1 : e.params)-1);
        return;
    } else if (!f->unpack && f->tabled && !f->call &&
               !(f->expand && f->target == 0)) {
        encode(p, OP_TBL, p->sp+1, f->count, 0, +1);
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
            struct expr e = {.prec = -1};
            p_subexpr(p, &e);
            encode_store(p, &e, f->insert, 0);
            p->sp -= 1;
        } else if (f->count > 0) {
            encode(p, OP_MOVE, p->sp+1, p->sp, 0, +1);
            encode(p, OP_IMM, p->sp-1, imm(p, MU_CONCAT_KEY), 0, 0);
            encode(p, OP_LOOKUP, p->sp-1, 0, p->sp-1, 0);
            p_expr(p);
            encode(p, OP_IMM, p->sp+1, imm(p, muint(f->index)), 0, +1);
            encode(p, OP_CALL, p->sp-3, 0x31, 0, -3);
        } else {
            p_expr(p);
        }
    }

    if (f->unpack && !f->tabled) {
        p->sp -= f->count;
    } else if (!f->unpack && f->tabled && f->flatten) {
        encode(p, OP_MOVE, p->sp + f->target, p->sp, 0, +f->target);

        for (mlen_t i = 0; i < f->target; i++) {
            encode(p, OP_IMM, p->sp-1 - (f->target-1-i),
                   imm(p, muint(i)), 0, 0);
            encode(p, i == f->target-1 ? OP_LOOKDN : OP_LOOKUP,
                   p->sp-1 - (f->target-1-i), p->sp,
                   p->sp-1 - (f->target-1-i),
                   -(f->target-1 == i));
        }
    } else if (!f->unpack && !f->tabled) {
        while (f->target > f->count) {
            encode(p, OP_IMM, p->sp+1, imm(p, 0), 0, +1);
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

static void p_assign(struct parse *p, bool insert) {
    struct lex ll = lex_inc(p->l);
    struct frame fl = {};
    s_frame(p, &fl, true);

    if (match(p, T_ASSIGN)) {
        struct frame fr = {.unpack = false, .insert = insert};
        s_frame(p, &fr, false);

        if ((fr.count == 0 && !fr.tabled) ||
            (fl.count == 0 && !fl.tabled)) {
            mu_error_assignment(&p->l);
        } else {
            fr.tabled = fr.tabled || fl.tabled;
            fr.target = fl.count;
            fr.flatten = !fl.tabled;
            p_frame(p, &fr);
        }

        struct lex lr = p->l;
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

static void p_return(struct parse *p) {
    // Remove any leftover iterators
    muintq_t sp = p->sp;
    while (p->sp != 0) {
        encode(p, OP_DROP, p->sp, 0, 0, -1);
    }

    struct frame f = {.unpack = false};
    s_frame(p, &f, false);

    if (f.call) {
        struct expr e = {.prec = -1};
        p_subexpr(p, &e);
        encode(p, OP_TCALL,
               p->sp - (e.params == 0xf ? 1 : e.params),
               e.params, 0,
               -(e.params == 0xf ? 1 : e.params)-1);
    } else {
        p_frame(p, &f);
        encode(p, OP_RET,
               p->sp - (f.tabled ? 0 : f.count-1),
               f.tabled ? 0xf : f.count, 0,
               -(f.tabled ? 1 : f.count));
    }

    p->sp = sp;
}

static void p_stmt(struct parse *p) {
    if (next(p, T_LBLOCK)) {
        p_block(p, false);

    } else if (lookahead(p, T_FN, T_ANY_SYM | T_ANY_OP)) {
        expect(p, T_ANY_SYM | T_ANY_OP);
        mu_t sym = mu_inc(p->m.val);
        p_fn(p);
        encode(p, OP_IMM, p->sp+1, imm(p, sym), 0, +1);
        encode(p, OP_INSERT, p->sp-1, 0, p->sp, -2);

    } else if (match(p, T_IF)) {
        p_if(p, false);

    } else if (match(p, T_WHILE)) {
        p_while(p);

    } else if (match(p, T_FOR)) {
        p_for(p);

    } else if (match(p, T_BREAK)) {
        if (p->bchain == (mlen_t)-1) {
            mu_error_parse(&p->l, mstr("break outside of loop"));
        }

        mlen_t offset = p->bcount;
        encode(p, OP_JUMP, 0, p->bchain ? p->bchain-p->bcount : 0, 0, 0);
        p->bchain = offset;

    } else if (match(p, T_CONTINUE)) {
        if (p->cchain == (mlen_t)-1) {
            mu_error_parse(&p->l, mstr("continue outside of loop"));
        }

        mlen_t offset = p->bcount;
        encode(p, OP_JUMP, 0, p->cchain ? p->cchain-p->bcount : 0, 0, 0);
        p->cchain = offset;

    } else if (match(p, T_ARROW | T_RETURN)) {
        p_return(p);

    } else if (match(p, T_LET)) {
        p_assign(p, true);

    } else {
        p_assign(p, false);
    }
}

static void p_block(struct parse *p, bool root) {
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
mu_t mu_parse(const char *s, muint_t n) {
    const mbyte_t *pos = (const mbyte_t *)s;
    const mbyte_t *end = (const mbyte_t *)pos + n;

    mu_t v = mu_nparse(&pos, end);

    if (pos < end) {
        mu_error_expression(*pos);
    }

    return v;
}

mu_t mu_nparse(const mbyte_t **ppos, const mbyte_t *end) {
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
        case L_NUM:     val = num_parse(&pos, end); break;
        case L_STR:     val = str_parse(&pos, end); break;
        case L_LTABLE:  val = tbl_parse(&pos, end); break;

        case L_KW: {
            const mbyte_t *start = pos++;
            while (pos < end && (class[*pos] == L_KW ||
                                 class[*pos] == L_NUM))
                pos++;

            val = str_create(start, pos-start);
            sym = true;
        } break;

        default:        mu_error_expression(*pos);
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

    if (sym && *pos != ':') {
        mu_error_expression(str_data(val)[0]);
    }

    *ppos = pos;
    return val;
}

struct code *mu_compile(const char *s, muint_t n) {
    struct parse p = {
        .bcode = buf_create(0),
        .imms = tbl_create(0),
        .fns = tbl_create(0),
        .bchain = -1,
        .cchain = -1,

        .regs = 1,
        .scope = MU_MINALLOC / sizeof(muint_t),
    };

    lex_init(&p.l, (const mbyte_t *)s, (const mbyte_t *)s+n);
    p_block(&p, true);
    if (p.l.tok) {
        unexpected(&p);
    }

    encode(&p, OP_RET, 0, 0, 0, 0);
    lex_dec(p.l);
    return compile(&p);
}

struct code *mu_ncompile(const mbyte_t **pos, const mbyte_t *end) {
    struct parse p = {
        .bcode = buf_create(0),
        .imms = tbl_create(0),
        .fns = tbl_create(0),
        .bchain = -1,
        .cchain = -1,

        .regs = 1,
        .scope = MU_MINALLOC / sizeof(muint_t),
    };

    lex_init(&p.l, *pos, end);
    p_block(&p, true);
    *pos = p.l.pos;

    encode(&p, OP_RET, 0, 0, 0, 0);
    lex_dec(p.l);
    return compile(&p);
}
