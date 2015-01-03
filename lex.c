#include "lex.h"

#include "parse.h"
#include "var.h"
#include "num.h"
#include "str.h"
#include "tbl.h"


// Creates internal tables for keywords or uses prexisting.
// Use this to initialize an op table if nescessary.
mu_const tbl_t *mu_keys(void) {
    // Currently this is initialized at runtime
    // TODO compile them?
    // TODO currently assumes failure is impossible
    static tbl_t *keyt = 0;

    if (keyt) return keyt;

    keyt = tbl_create(0, 0);
    tbl_insert(keyt, vcstr("nil", 0), vuint(T_NIL), 0);
    tbl_insert(keyt, vcstr("fn", 0), vuint(T_FN), 0);
    tbl_insert(keyt, vcstr("let", 0), vuint(T_LET), 0);
    tbl_insert(keyt, vcstr("return", 0), vuint(T_RETURN), 0);
    tbl_insert(keyt, vcstr("if", 0), vuint(T_IF), 0);
    tbl_insert(keyt, vcstr("while", 0), vuint(T_WHILE), 0);
    tbl_insert(keyt, vcstr("for", 0), vuint(T_FOR), 0);
    tbl_insert(keyt, vcstr("continue", 0), vuint(T_CONT), 0);
    tbl_insert(keyt, vcstr("break", 0), vuint(T_BREAK), 0);
    tbl_insert(keyt, vcstr("else", 0), vuint(T_ELSE), 0);
    tbl_insert(keyt, vcstr("and", 0), vuint(T_AND), 0);
    tbl_insert(keyt, vcstr("or", 0), vuint(T_OR), 0);
    
    return keyt;
}


// Lexer definitions for Mu's tokens
extern void (* const lexs[256])(parse_t *);

static mu_noreturn void l_bad(parse_t *p);
static void l_ws(parse_t *p);
static void l_com(parse_t *p);
static void l_op(parse_t *p);
static void l_kw(parse_t *p);
static void l_tok(parse_t *p);
static void l_set(parse_t *p);
static void l_sep(parse_t *p);
static void l_nl(parse_t *p);
static void l_num(parse_t *p);
static void l_str(parse_t *p);


// Helper function for skipping whitespace
static void wskip(parse_t *p) {
    while (p->pos < p->end) {
        if ((lexs[*p->pos] == l_ws) || 
            (p->paren && *p->pos == '\n')) {
            p->pos++;
        } else if (*p->pos == '`') {
            p->pos++;

            if (p->pos < p->end && *p->pos == '`') {
                int count = 1;
                int seen = 0;

                while (p->pos < p->end) {
                    if (*p->pos++ != '`')
                        break;

                    count++;
                }

                while (p->pos < p->end && seen < count) {
                    if (*p->pos++ == '`')
                        seen++;
                    else
                        seen = 0;
                }
            } else {
                while (p->pos < p->end) {
                    if (*p->pos == '`' || *p->pos == '\n')
                        break;

                    p->pos++;
                }
            }
        } else {
            return;
        }
    }
}


static mu_noreturn void l_bad(parse_t *p) {
    err_parse(p->eh);
}

static void l_ws(parse_t *p) {
    wskip(p);
    return lex(p);
}

static void l_com(parse_t *p) {
    wskip(p);
    return lex(p);
}

static void l_op(parse_t *p) {
    const data_t *kw_pos = p->pos++;
    const data_t *kw_end;

    while (p->pos < p->end && (lexs[*p->pos] == l_op ||
                               lexs[*p->pos] == l_set)) {
        p->pos++;
    }

    kw_end = p->pos;
    p->val = vnstr(kw_pos, kw_end-kw_pos, p->eh);

    wskip(p);
    p->op.rprec = p->pos - kw_end;

    // TODO make these strings preinterned so this is actually reasonable
    if (var_equals(p->val, vcstr("->", p->eh)) && p->left) {
        p->tok = T_RETURN;
    } else if (var_equals(p->val, vcstr(".", p->eh)) && lexs[*p->pos] == l_kw) {
        p->tok = T_KEY;
    } else if (p->left && lexs[kw_end[-1]] == l_set) {
        p->tok = T_OPSET;
        p->val = vnstr(kw_pos, kw_end-kw_pos-1, p->eh);
    } else {
        p->tok = T_OP;
    }
}

static void l_kw(parse_t *p) {
    const data_t *kw_pos = p->pos++;
    const data_t *kw_end;

    while (p->pos < p->end && (lexs[*p->pos] == l_kw ||
                               lexs[*p->pos] == l_num)) {
        p->pos++;
    }

    kw_end = p->pos;
    p->val = vnstr(kw_pos, kw_end-kw_pos, p->eh);
    var_t tok = tbl_lookup(p->keys, p->val);

    wskip(p);
    p->op.rprec = p->pos - kw_end;

    if (p->key && lexs[*p->pos] == l_set) {
        p->tok = T_IDSET;
    } else if (isnil(tok)) {
        p->tok = T_IDENT;
    } else if (getuint(tok) == T_FN && lexs[*p->pos] == l_kw) {
        p->tok = T_FNSET;
    } else {
        p->tok = getuint(tok);
    }
}

static void l_tok(parse_t *p) {
    p->tok = *p->pos++;
}

static void l_set(parse_t *p) {
    if (!p->left)
        return l_op(p);

    p->tok = T_SET;
    p->pos++;
}

static void l_sep(parse_t *p) {
    p->tok = T_SEP;
    p->pos++;
}

static void l_nl(parse_t *p) {
    if (p->paren)
        return l_ws(p);
    else
        return l_sep(p);
}       

static void l_num(parse_t *p) {
    p->tok = T_LIT;
    p->val = vnum(num_parse(&p->pos, p->end));
}

static void l_str(parse_t *p) {
    p->tok = T_LIT;
    p->val = vstr(str_parse(&p->pos, p->end, p->eh));
}


// Lookup table of lex functions based 
// only on first character of token
void (* const lexs[256])(parse_t *) = {
/* 00 01 02 03 */   l_bad,  l_bad,  l_bad,  l_bad,
/* 04 05 06 \a */   l_bad,  l_bad,  l_bad,  l_bad,
/* \b \t \n \v */   l_bad,  l_ws,   l_nl,   l_ws,
/* \f \r 0e 0f */   l_ws,   l_ws,   l_bad,  l_bad,
/* 10 11 12 13 */   l_bad,  l_bad,  l_bad,  l_bad,
/* 14 15 16 17 */   l_bad,  l_bad,  l_bad,  l_bad,
/* 18 19 1a 1b */   l_bad,  l_bad,  l_bad,  l_bad,
/* 1c 1d 1e 1f */   l_bad,  l_bad,  l_bad,  l_bad,
/*     !  "  # */   l_ws,   l_op,   l_str,  l_op,
/*  $  %  &  ' */   l_op,   l_op,   l_op,   l_str,
/*  (  )  *  + */   l_tok,  l_tok,  l_op,   l_op,
/*  ,  -  .  / */   l_sep,  l_op,   l_op,   l_op,
/*  0  1  2  3 */   l_num,  l_num,  l_num,  l_num,
/*  4  5  6  7 */   l_num,  l_num,  l_num,  l_num,
/*  8  9  :  ; */   l_num,  l_num,  l_set,  l_sep,
/*  <  =  >  ? */   l_op,   l_set,  l_op,   l_op,
/*  @  A  B  C */   l_op,   l_kw,   l_kw,   l_kw,
/*  D  E  F  G */   l_kw,   l_kw,   l_kw,   l_kw,
/*  H  I  J  K */   l_kw,   l_kw,   l_kw,   l_kw,
/*  L  M  N  O */   l_kw,   l_kw,   l_kw,   l_kw,
/*  P  Q  R  S */   l_kw,   l_kw,   l_kw,   l_kw,
/*  T  U  V  W */   l_kw,   l_kw,   l_kw,   l_kw,
/*  X  Y  Z  [ */   l_kw,   l_kw,   l_kw,   l_tok,
/*  \  ]  ^  _ */   l_tok,  l_tok,  l_op,   l_kw,
/*  `  a  b  c */   l_com,  l_kw,   l_kw,   l_kw,   
/*  d  e  f  g */   l_kw,   l_kw,   l_kw,   l_kw,   
/*  h  i  j  k */   l_kw,   l_kw,   l_kw,   l_kw,   
/*  l  p  n  o */   l_kw,   l_kw,   l_kw,   l_kw,   
/*  p  q  r  s */   l_kw,   l_kw,   l_kw,   l_kw,   
/*  t  u  v  w */   l_kw,   l_kw,   l_kw,   l_kw,   
/*  x  y  z  { */   l_kw,   l_kw,   l_kw,   l_tok,
/*  |  }  ~ 7f */   l_op,   l_tok,  l_op,   l_bad,
/* 80 81 82 83 */   l_bad,  l_bad,  l_bad,  l_bad,
/* 84 85 86 87 */   l_bad,  l_bad,  l_bad,  l_bad,
/* 88 89 8a 8b */   l_bad,  l_bad,  l_bad,  l_bad,
/* 8c 8d 8e 8f */   l_bad,  l_bad,  l_bad,  l_bad,
/* 90 91 92 93 */   l_bad,  l_bad,  l_bad,  l_bad,
/* 94 95 96 97 */   l_bad,  l_bad,  l_bad,  l_bad,
/* 98 99 9a 9b */   l_bad,  l_bad,  l_bad,  l_bad,
/* 9c 9d 9e 9f */   l_bad,  l_bad,  l_bad,  l_bad,
/* a0 a1 a2 a3 */   l_bad,  l_bad,  l_bad,  l_bad,
/* a4 a5 a6 a7 */   l_bad,  l_bad,  l_bad,  l_bad,
/* a8 a9 aa ab */   l_bad,  l_bad,  l_bad,  l_bad,
/* ac ad ae af */   l_bad,  l_bad,  l_bad,  l_bad,
/* b0 b1 b2 b3 */   l_bad,  l_bad,  l_bad,  l_bad,
/* b4 b5 b6 b7 */   l_bad,  l_bad,  l_bad,  l_bad,
/* b8 b9 ba bb */   l_bad,  l_bad,  l_bad,  l_bad,
/* bc bd be bf */   l_bad,  l_bad,  l_bad,  l_bad,
/* c0 c1 c2 c3 */   l_bad,  l_bad,  l_bad,  l_bad,
/* c4 c5 c6 c7 */   l_bad,  l_bad,  l_bad,  l_bad,
/* c8 c9 ca cb */   l_bad,  l_bad,  l_bad,  l_bad,
/* cc cd ce cf */   l_bad,  l_bad,  l_bad,  l_bad,
/* d0 d1 d2 d3 */   l_bad,  l_bad,  l_bad,  l_bad,
/* d4 d5 d6 d7 */   l_bad,  l_bad,  l_bad,  l_bad,
/* d8 d9 da db */   l_bad,  l_bad,  l_bad,  l_bad,
/* dc dd de df */   l_bad,  l_bad,  l_bad,  l_bad,
/* e0 e1 e2 e3 */   l_bad,  l_bad,  l_bad,  l_bad,
/* e4 e5 e6 e7 */   l_bad,  l_bad,  l_bad,  l_bad,
/* e8 e9 ea eb */   l_bad,  l_bad,  l_bad,  l_bad,
/* ec ed ee ef */   l_bad,  l_bad,  l_bad,  l_bad,
/* f0 f1 f2 f3 */   l_bad,  l_bad,  l_bad,  l_bad,
/* f4 f5 f6 f7 */   l_bad,  l_bad,  l_bad,  l_bad,
/* f8 f9 fa fb */   l_bad,  l_bad,  l_bad,  l_bad,
/* fc fd fe ff */   l_bad,  l_bad,  l_bad,  l_bad,
};


// Performs lexical analysis on the passed string
void lex(parse_t *p) {
    if (p->pos < p->end)
        lexs[*p->pos](p);
    else
        p->tok = 0;
}

