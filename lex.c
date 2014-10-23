#include "lex.h"

#include "parse.h"
#include "var.h"
#include "num.h"
#include "str.h"


// Creates internal tables for keywords or uses prexisting.
// Use this to initialize an op table if nescessary.
__attribute__((const))
tbl_t *mu_keys(void) {
    // Currently this is initialized at runtime
    // TODO compile them?
    // TODO currently assumes failure is impossible
    static tbl_t *keyt = 0;

    if (keyt) return keyt;

    keyt = tbl_create(0, 0);
    tbl_insert(keyt, vcstr("nil"), vraw(MT_NIL), 0);
    tbl_insert(keyt, vcstr("fn"), vraw(MT_FN), 0);
    tbl_insert(keyt, vcstr("let"), vraw(MT_LET), 0);
    tbl_insert(keyt, vcstr("return"), vraw(MT_RETURN), 0);
    tbl_insert(keyt, vcstr("if"), vraw(MT_IF), 0);
    tbl_insert(keyt, vcstr("while"), vraw(MT_WHILE), 0);
    tbl_insert(keyt, vcstr("for"), vraw(MT_FOR), 0);
    tbl_insert(keyt, vcstr("continue"), vraw(MT_CONT), 0);
    tbl_insert(keyt, vcstr("break"), vraw(MT_BREAK), 0);
    tbl_insert(keyt, vcstr("else"), vraw(MT_ELSE), 0);
    tbl_insert(keyt, vcstr("and"), vraw(MT_AND), 0);
    tbl_insert(keyt, vcstr("or"), vraw(MT_OR), 0);
    
    return keyt;
}


// Lexer definitions for Mu's tokens
extern void (* const lexs[256])(mstate_t *);

__attribute__((noreturn))
static void ml_bad(mstate_t *m);
static void ml_ws(mstate_t *m);
static void ml_com(mstate_t *m);
static void ml_op(mstate_t *m);
static void ml_kw(mstate_t *m);
static void ml_tok(mstate_t *m);
static void ml_set(mstate_t *m);
static void ml_sep(mstate_t *m);
static void ml_nl(mstate_t *m);
static void ml_num(mstate_t *m);
static void ml_str(mstate_t *m);


// Helper function for skipping whitespace
static void vskip(mstate_t *m) {
    while (m->pos < m->end) {
        if ((lexs[*m->pos] == ml_ws) || 
            (m->paren && *m->pos == '\n')) {
            m->pos++;
        } else if (*m->pos == '`') {
            m->pos++;

            if (m->pos < m->end && *m->pos == '`') {
                int count = 1;
                int seen = 0;

                while (m->pos < m->end) {
                    if (*m->pos++ != '`')
                        break;

                    count++;
                }

                while (m->pos < m->end && seen < count) {
                    if (*m->pos++ == '`')
                        seen++;
                    else
                        seen = 0;
                }
            } else {
                while (m->pos < m->end) {
                    if (*m->pos == '`' || *m->pos == '\n')
                        break;

                    m->pos++;
                }
            }
        } else {
            return;
        }
    }
}


__attribute__((noreturn))
static void ml_bad(mstate_t *m) {
    err_parse(m->eh);
}

static void ml_ws(mstate_t *m) {
    vskip(m);
    return mu_lex(m);
}

static void ml_com(mstate_t *m) {
    vskip(m);
    return mu_lex(m);
}

static void ml_op(mstate_t *m) {
    const str_t *kw = m->pos++;

    while (m->pos < m->end && (lexs[*m->pos] == ml_op ||
                               lexs[*m->pos] == ml_set))
        m->pos++;

    m->val = vstr(m->str, kw-m->str, m->pos-kw);

    kw = m->pos;
    vskip(m);
    m->op.rprec = m->pos - kw;

    if (str_equals(m->val, vcstr(".")) && lexs[*m->pos] == ml_kw) {
        m->tok = MT_KEY;
    } else if (m->left && lexs[kw[-1]] == ml_set) {
        m->tok = MT_OPSET;
        m->val.len -= 1;
    } else {
        m->tok = MT_OP;
    }
}

static void ml_kw(mstate_t *m) {
    const str_t *kw = m->pos++;

    while (m->pos < m->end && (lexs[*m->pos] == ml_kw ||
                               lexs[*m->pos] == ml_num))
        m->pos++;

    m->val = vstr(m->str, kw-m->str, m->pos-kw);
    var_t tok = tbl_lookup(m->keys, m->val);

    kw = m->pos;
    vskip(m);
    m->op.rprec = m->pos - kw;

    if (m->key && lexs[*m->pos] == ml_set) {
        m->tok = MT_IDSET;
    } else if (isnil(tok)) {
        m->tok = MT_IDENT;
    } else if (tok.data == MT_FN && lexs[*m->pos] == ml_kw) {
        m->tok = MT_FNSET;
    } else {
        m->tok = tok.data;
    }
}

static void ml_tok(mstate_t *m) {
    m->tok = *m->pos++;
}

static void ml_set(mstate_t *m) {
    if (!m->left)
        return ml_op(m);

    m->tok = MT_SET;
    m->pos++;
}

static void ml_sep(mstate_t *m) {
    m->tok = MT_SEP;
    m->pos++;
}

static void ml_nl(mstate_t *m) {
    if (m->paren)
        return ml_ws(m);
    else
        return ml_sep(m);
}       

static void ml_num(mstate_t *m) {
    m->tok = MT_LIT;
    m->val = num_parse(&m->pos, m->end);
}

static void ml_str(mstate_t *m) {
    m->tok = MT_LIT;
    m->val = str_parse(&m->pos, m->end, m->eh);
}


// Lookup table of lex functions based 
// only on first character of token
void (* const lexs[256])(mstate_t *) = {
/* 00 01 02 03 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* 04 05 06 \a */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* \b \t \n \v */   ml_bad,   ml_ws,    ml_nl,    ml_ws,
/* \f \r 0e 0f */   ml_ws,    ml_ws,    ml_bad,   ml_bad,
/* 10 11 12 13 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* 14 15 16 17 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* 18 19 1a 1b */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* 1c 1d 1e 1f */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/*     !  "  # */   ml_ws,    ml_op,    ml_str,   ml_op,
/*  $  %  &  ' */   ml_op,    ml_op,    ml_op,    ml_str,
/*  (  )  *  + */   ml_tok,   ml_tok,   ml_op,    ml_op,
/*  ,  -  .  / */   ml_sep,   ml_op,    ml_op,    ml_op,
/*  0  1  2  3 */   ml_num,   ml_num,   ml_num,   ml_num,
/*  4  5  6  7 */   ml_num,   ml_num,   ml_num,   ml_num,
/*  8  9  :  ; */   ml_num,   ml_num,   ml_set,   ml_sep,
/*  <  =  >  ? */   ml_op,    ml_set,   ml_op,    ml_op,
/*  @  A  B  C */   ml_op,    ml_kw,    ml_kw,    ml_kw,
/*  D  E  F  G */   ml_kw,    ml_kw,    ml_kw,    ml_kw,
/*  H  I  J  K */   ml_kw,    ml_kw,    ml_kw,    ml_kw,
/*  L  M  N  O */   ml_kw,    ml_kw,    ml_kw,    ml_kw,
/*  P  Q  R  S */   ml_kw,    ml_kw,    ml_kw,    ml_kw,
/*  T  U  V  W */   ml_kw,    ml_kw,    ml_kw,    ml_kw,
/*  X  Y  Z  [ */   ml_kw,    ml_kw,    ml_kw,    ml_tok,
/*  \  ]  ^  _ */   ml_tok,   ml_tok,   ml_op,    ml_kw,
/*  `  a  b  c */   ml_com,   ml_kw,    ml_kw,    ml_kw,   
/*  d  e  f  g */   ml_kw,    ml_kw,    ml_kw,    ml_kw,   
/*  h  i  j  k */   ml_kw,    ml_kw,    ml_kw,    ml_kw,   
/*  l  m  n  o */   ml_kw,    ml_kw,    ml_kw,    ml_kw,   
/*  p  q  r  s */   ml_kw,    ml_kw,    ml_kw,    ml_kw,   
/*  t  u  v  w */   ml_kw,    ml_kw,    ml_kw,    ml_kw,   
/*  x  y  z  { */   ml_kw,    ml_kw,    ml_kw,    ml_tok,
/*  |  }  ~ 7f */   ml_op,    ml_tok,   ml_op,    ml_bad,
/* 80 81 82 83 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* 84 85 86 87 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* 88 89 8a 8b */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* 8c 8d 8e 8f */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* 90 91 92 93 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* 94 95 96 97 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* 98 99 9a 9b */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* 9c 9d 9e 9f */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* a0 a1 a2 a3 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* a4 a5 a6 a7 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* a8 a9 aa ab */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* ac ad ae af */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* b0 b1 b2 b3 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* b4 b5 b6 b7 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* b8 b9 ba bb */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* bc bd be bf */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* c0 c1 c2 c3 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* c4 c5 c6 c7 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* c8 c9 ca cb */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* cc cd ce cf */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* d0 d1 d2 d3 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* d4 d5 d6 d7 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* d8 d9 da db */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* dc dd de df */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* e0 e1 e2 e3 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* e4 e5 e6 e7 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* e8 e9 ea eb */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* ec ed ee ef */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* f0 f1 f2 f3 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* f4 f5 f6 f7 */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* f8 f9 fa fb */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
/* fc fd fe ff */   ml_bad,   ml_bad,   ml_bad,   ml_bad,
};


// Performs lexical analysis on the passed string
// Value is stored in lval and its type is returned
void mu_lex(mstate_t *m) {
    if (m->pos < m->end)
        return lexs[*m->pos](m);
    else
        m->tok = 0;
}

