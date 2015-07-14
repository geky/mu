#include "lex.h"

#include "parse.h"
#include "num.h"
#include "str.h"
#include "tbl.h"


// Classification of characters
mu_packed enum class {
    L_NONE,
    L_TERM, L_COMMA,

    L_NL, L_COMMENT, L_WS,
    L_OP, L_KW, L_STR, L_NUM,

    L_LBLOCK, L_RBLOCK,
    L_LTABLE, L_RTABLE,
    L_LPAREN, L_RPAREN,
};

static const enum class class[256] = {
/* 00 01 02 03 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 04 05 06 \a */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* \b \t \n \v */   L_NONE,     L_WS,       L_NL,       L_WS,
/* \f \r 0e 0f */   L_WS,       L_WS,       L_NONE,     L_NONE,
/* 10 11 12 13 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 14 15 16 17 */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 18 19 1a 1b */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/* 1c 1d 1e 1f */   L_NONE,     L_NONE,     L_NONE,     L_NONE,
/*     !  "  # */   L_WS,       L_OP,       L_STR,      L_COMMENT,
/*  $  %  &  ' */   L_OP,       L_OP,       L_OP,       L_STR,
/*  (  )  *  + */   L_LPAREN,   L_RPAREN,   L_OP,       L_OP,
/*  ,  -  .  / */   L_COMMA,    L_OP,       L_OP,       L_OP,
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


// Internal tables for keywords/symbols
mu_const tbl_t *mu_keys(void) {
    static tbl_t *keyt = 0;

    if (keyt) return keyt;

    keyt = tbl_create(0);
    tbl_insert(keyt, mcstr("let"),      muint(T_LET));
    tbl_insert(keyt, mcstr("else"),     muint(T_ELSE));
    tbl_insert(keyt, mcstr("and"),      muint(T_AND));
    tbl_insert(keyt, mcstr("or"),       muint(T_OR));
    tbl_insert(keyt, mcstr("continue"), muint(T_CONT));
    tbl_insert(keyt, mcstr("break"),    muint(T_BREAK));
    tbl_insert(keyt, mcstr("return"),   muint(T_RETURN));
    tbl_insert(keyt, mcstr("fn"),       muint(T_FN));
    tbl_insert(keyt, mcstr("type"),     muint(T_TYPE));
    tbl_insert(keyt, mcstr("if"),       muint(T_IF));
    tbl_insert(keyt, mcstr("while"),    muint(T_WHILE));
    tbl_insert(keyt, mcstr("for"),      muint(T_FOR));
    tbl_insert(keyt, mcstr("_"),        muint(T_NIL));
    tbl_insert(keyt, mcstr("nil"),      muint(T_NIL));
    return keyt;
}

mu_const tbl_t *mu_syms(void) {
    static tbl_t *symt = 0;

    if (symt) return symt;

    symt = tbl_create(0);
    tbl_insert(symt, mcstr("="),  muint(T_ASSIGN));
    tbl_insert(symt, mcstr(":"),  muint(T_PAIR));
    tbl_insert(symt, mcstr("."),  muint(T_DOT));
    tbl_insert(symt, mcstr("->"), muint(T_ARROW));
    tbl_insert(symt, mcstr(".."), muint(T_EXPAND));
    return symt;
}


// Helper function for skipping whitespace
static void wskip(struct lex *l) {
    while (l->pos < l->end) {
        if (*l->pos == '#') {
            while (l->pos < l->end && *l->pos != '\n')
                l->pos++;
        } else if (*l->pos == '\n' && l->paren) {
            l->line++;
            l->pos++;
        } else if (class[*l->pos] == L_WS) {
            l->pos++;
        } else {
            break;
        }
    }
}

// Lexer definitions for non-trivial tokens
static void l_nl(struct lex *l) {
    const data_t *npos = l->pos+1;

    while (npos < l->end && class[*npos] == L_WS)
        npos++;

    if (*npos != '\n' && *npos != '#') {
        int nindent = npos - (l->pos+1);

        if (nindent > MU_MAXUINTQ) {
            mu_err_parse(); // TODO better message
        } else if (nindent > l->indent) {
            l->tok = T_LBLOCK;
            l->nblock = nindent - l->indent;
            l->indent = nindent;
            return;
        } else if (nindent < l->indent) {
            l->tok = T_RBLOCK;
            l->nblock = nindent - l->indent;
            l->indent = nindent;
            return;
        }
    }

    l->tok = T_TERM;
    l->pos = npos;
    l->line++;
}

static void l_op(struct lex *l) {
    const data_t *op_pos = l->pos++;

    while (l->pos < l->end && class[*l->pos] == L_OP)
        l->pos++;

    const data_t *op_end = l->pos;

    l->val = mnstr(op_pos, op_end-op_pos);
    mu_t tok = tbl_lookup(mu_syms(), l->val);

    wskip(l);
    l->rprec = l->pos - op_end;

    // TODO make these strings preinterned so this is actually reasonable
    if (!isnil(tok)) {
        l->tok = getuint(tok);
    } else if (op_end[-1] == '=') {
        l->tok = T_OPASSIGN;
        l->val = mnstr(op_pos, op_end-op_pos-1);
    } else {
        l->tok = T_OP;
    }
}

static void l_kw(struct lex *l) {
    const data_t *kw_pos = l->pos++;

    while (l->pos < l->end && (class[*l->pos] == L_KW ||
                               class[*l->pos] == L_NUM))
        l->pos++;

    const data_t *kw_end = l->pos;

    l->val = mnstr(kw_pos, kw_end-kw_pos);
    mu_t tok = tbl_lookup(mu_keys(), l->val);

    wskip(l);
    l->rprec = l->pos - kw_end;

    if (l->pos < l->end && *l->pos == ':' &&
        !(l->pos+1 < l->end && class[l->pos[1]] == L_OP)) {
        l->tok = T_KEY;
    } else if (!isnil(tok)) {
        l->tok = getuint(tok);
    } else {
        l->tok = T_SYM;
    }
}

static void l_num(struct lex *l) {
    l->tok = T_IMM;
    l->val = mnum(num_parse(&l->pos, l->end));
}

static void l_str(struct lex *l) {
    l->tok = T_IMM;
    l->val = mstr(str_parse(&l->pos, l->end));
}


void mu_lex_init(struct lex *l) {
    while (l->pos < l->end && class[*l->pos] == L_WS) {
        l->pos++;
        l->indent++;
    }

    l->block = 1 + l->indent;
    mu_lex(l);
}

void mu_lex(struct lex *l) {
    l->block += l->nblock;
    l->nblock = 0;
    l->paren += l->nparen;
    l->nparen = 0;

    if (l->pos >= l->end) {
        l->tok = 0;
        return;
    }

    switch (class[*l->pos]) {
        case L_NONE:    return mu_err_parse();

        case L_NL:      if (!l->paren) return l_nl(l);
        case L_COMMENT:
        case L_WS:      wskip(l); return mu_lex(l);

        case L_OP:      return l_op(l);
        case L_KW:      return l_kw(l);
        case L_STR:     return l_str(l);
        case L_NUM:     return l_num(l);

        case L_TERM:    l->tok = T_TERM; l->pos++; return;
        case L_COMMA:   l->tok = T_SEP;  l->pos++; return;

        case L_LBLOCK:  l->tok = T_LBLOCK; l->nblock = +1; l->pos++; return;
        case L_RBLOCK:  l->tok = T_RBLOCK; l->nblock = -1; l->pos++; return;
        case L_LTABLE:  l->tok = T_LTABLE; l->nparen = +1; l->pos++; return;
        case L_RTABLE:  l->tok = T_RTABLE; l->nparen = -1; l->pos++; return;
        case L_LPAREN:  l->tok = T_LPAREN; l->nparen = +1; l->pos++; return;
        case L_RPAREN:  l->tok = T_RPAREN; l->nparen = -1; l->pos++; return;
    }
}

// TODO handle this seperately
void mu_scan(struct lex *l) {
    return mu_lex(l);
}

