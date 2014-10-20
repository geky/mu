#include "vlex.h"

#include "vparse.h"
#include "var.h"
#include "num.h"
#include "str.h"


// Creates internal tables for keywords or uses prexisting.
// Use this to initialize an op table if nescessary.
__attribute__((const))
tbl_t *vkeys(void) {
    // Currently this is initialized at runtime
    // TODO compile them?
    // TODO currently assumes failure is impossible
    static tbl_t *vkeyt = 0;

    if (vkeyt) return vkeyt;

    vkeyt = tbl_create(0, 0);
    tbl_insert(vkeyt, vcstr("nil"), vraw(VT_NIL), 0);
    tbl_insert(vkeyt, vcstr("fn"), vraw(VT_FN), 0);
    tbl_insert(vkeyt, vcstr("let"), vraw(VT_LET), 0);
    tbl_insert(vkeyt, vcstr("return"), vraw(VT_RETURN), 0);
    tbl_insert(vkeyt, vcstr("if"), vraw(VT_IF), 0);
    tbl_insert(vkeyt, vcstr("while"), vraw(VT_WHILE), 0);
    tbl_insert(vkeyt, vcstr("for"), vraw(VT_FOR), 0);
    tbl_insert(vkeyt, vcstr("continue"), vraw(VT_CONT), 0);
    tbl_insert(vkeyt, vcstr("break"), vraw(VT_BREAK), 0);
    tbl_insert(vkeyt, vcstr("else"), vraw(VT_ELSE), 0);
    tbl_insert(vkeyt, vcstr("and"), vraw(VT_AND), 0);
    tbl_insert(vkeyt, vcstr("or"), vraw(VT_OR), 0);
    
    return vkeyt;
}


// Lexer definitions for V's tokens
extern void (* const vlexs[256])(vstate_t *);

__attribute__((noreturn))
static void vl_bad(vstate_t *vs);
static void vl_ws(vstate_t *vs);
static void vl_com(vstate_t *vs);
static void vl_op(vstate_t *vs);
static void vl_kw(vstate_t *vs);
static void vl_tok(vstate_t *vs);
static void vl_set(vstate_t *vs);
static void vl_sep(vstate_t *vs);
static void vl_nl(vstate_t *vs);
static void vl_num(vstate_t *vs);
static void vl_str(vstate_t *vs);


// Helper function for skipping whitespace
static void vskip(vstate_t *vs) {
    while (vs->pos < vs->end) {
        if ((vlexs[*vs->pos] == vl_ws) || 
            (vs->paren && *vs->pos == '\n')) {
            vs->pos++;
        } else if (*vs->pos == '`') {
            vs->pos++;

            if (vs->pos < vs->end && *vs->pos == '`') {
                int count = 1;
                int seen = 0;

                while (vs->pos < vs->end) {
                    if (*vs->pos++ != '`')
                        break;

                    count++;
                }

                while (vs->pos < vs->end && seen < count) {
                    if (*vs->pos++ == '`')
                        seen++;
                    else
                        seen = 0;
                }
            } else {
                while (vs->pos < vs->end) {
                    if (*vs->pos == '`' || *vs->pos == '\n')
                        break;

                    vs->pos++;
                }
            }
        } else {
            return;
        }
    }
}


__attribute__((noreturn))
static void vl_bad(vstate_t *vs) {
    err_parse(vs->eh);
}

static void vl_ws(vstate_t *vs) {
    vskip(vs);
    return vlex(vs);
}

static void vl_com(vstate_t *vs) {
    vskip(vs);
    return vlex(vs);
}

static void vl_op(vstate_t *vs) {
    const str_t *kw = vs->pos++;

    while (vs->pos < vs->end && (vlexs[*vs->pos] == vl_op ||
                                 vlexs[*vs->pos] == vl_set))
        vs->pos++;

    vs->val = vstr(vs->str, kw-vs->str, vs->pos-kw);

    kw = vs->pos;
    vskip(vs);
    vs->op.rprec = vs->pos - kw;

    if (str_equals(vs->val, vcstr(".")) && vlexs[*vs->pos] == vl_kw) {
        vs->tok = VT_KEY;
    } else if (vs->left && vlexs[kw[-1]] == vl_set) {
        vs->tok = VT_OPSET;
        vs->val.len -= 1;
    } else {
        vs->tok = VT_OP;
    }
}

static void vl_kw(vstate_t *vs) {
    const str_t *kw = vs->pos++;

    while (vs->pos < vs->end && (vlexs[*vs->pos] == vl_kw ||
                                 vlexs[*vs->pos] == vl_num))
        vs->pos++;

    vs->val = vstr(vs->str, kw-vs->str, vs->pos-kw);
    var_t tok = tbl_lookup(vs->keys, vs->val);

    kw = vs->pos;
    vskip(vs);
    vs->op.rprec = vs->pos - kw;

    if (vs->key && vlexs[*vs->pos] == vl_set) {
        vs->tok = VT_IDSET;
    } else if (var_isnil(tok)) {
        vs->tok = VT_IDENT;
    } else if (tok.data == VT_FN && vlexs[*vs->pos] == vl_kw) {
        vs->tok = VT_FNSET;
    } else {
        vs->tok = tok.data;
    }
}

static void vl_tok(vstate_t *vs) {
    vs->tok = *vs->pos++;
}

static void vl_set(vstate_t *vs) {
    if (!vs->left)
        return vl_op(vs);

    vs->tok = VT_SET;
    vs->pos++;
}

static void vl_sep(vstate_t *vs) {
    vs->tok = VT_SEP;
    vs->pos++;
}

static void vl_nl(vstate_t *vs) {
    if (vs->paren)
        return vl_ws(vs);
    else
        return vl_sep(vs);
}       

static void vl_num(vstate_t *vs) {
    vs->tok = VT_LIT;
    vs->val = num_parse(&vs->pos, vs->end);
}

static void vl_str(vstate_t *vs) {
    vs->tok = VT_LIT;
    vs->val = str_parse(&vs->pos, vs->end, vs->eh);
}


// Lookup table of lex functions based 
// only on first character of token
void (* const vlexs[256])(vstate_t *) = {
/* 00 01 02 03 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 04 05 06 \a */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* \b \t \n \v */   vl_bad,   vl_ws,    vl_nl,    vl_ws,
/* \f \r 0e 0f */   vl_ws,    vl_ws,    vl_bad,   vl_bad,
/* 10 11 12 13 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 14 15 16 17 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 18 19 1a 1b */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 1c 1d 1e 1f */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/*     !  "  # */   vl_ws,    vl_op,    vl_str,   vl_op,
/*  $  %  &  ' */   vl_op,    vl_op,    vl_op,    vl_str,
/*  (  )  *  + */   vl_tok,   vl_tok,   vl_op,    vl_op,
/*  ,  -  .  / */   vl_sep,   vl_op,    vl_op,    vl_op,
/*  0  1  2  3 */   vl_num,   vl_num,   vl_num,   vl_num,
/*  4  5  6  7 */   vl_num,   vl_num,   vl_num,   vl_num,
/*  8  9  :  ; */   vl_num,   vl_num,   vl_set,   vl_sep,
/*  <  =  >  ? */   vl_op,    vl_set,   vl_op,    vl_op,
/*  @  A  B  C */   vl_op,    vl_kw,    vl_kw,    vl_kw,
/*  D  E  F  G */   vl_kw,    vl_kw,    vl_kw,    vl_kw,
/*  H  I  J  K */   vl_kw,    vl_kw,    vl_kw,    vl_kw,
/*  L  M  N  O */   vl_kw,    vl_kw,    vl_kw,    vl_kw,
/*  P  Q  R  S */   vl_kw,    vl_kw,    vl_kw,    vl_kw,
/*  T  U  V  W */   vl_kw,    vl_kw,    vl_kw,    vl_kw,
/*  X  Y  Z  [ */   vl_kw,    vl_kw,    vl_kw,    vl_tok,
/*  \  ]  ^  _ */   vl_tok,   vl_tok,   vl_op,    vl_kw,
/*  `  a  b  c */   vl_com,   vl_kw,    vl_kw,    vl_kw,   
/*  d  e  f  g */   vl_kw,    vl_kw,    vl_kw,    vl_kw,   
/*  h  i  j  k */   vl_kw,    vl_kw,    vl_kw,    vl_kw,   
/*  l  m  n  o */   vl_kw,    vl_kw,    vl_kw,    vl_kw,   
/*  p  q  r  s */   vl_kw,    vl_kw,    vl_kw,    vl_kw,   
/*  t  u  v  w */   vl_kw,    vl_kw,    vl_kw,    vl_kw,   
/*  x  y  z  { */   vl_kw,    vl_kw,    vl_kw,    vl_tok,
/*  |  }  ~ 7f */   vl_op,    vl_tok,   vl_op,    vl_bad,
/* 80 81 82 83 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 84 85 86 87 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 88 89 8a 8b */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 8c 8d 8e 8f */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 90 91 92 93 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 94 95 96 97 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 98 99 9a 9b */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 9c 9d 9e 9f */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* a0 a1 a2 a3 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* a4 a5 a6 a7 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* a8 a9 aa ab */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* ac ad ae af */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* b0 b1 b2 b3 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* b4 b5 b6 b7 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* b8 b9 ba bb */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* bc bd be bf */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* c0 c1 c2 c3 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* c4 c5 c6 c7 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* c8 c9 ca cb */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* cc cd ce cf */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* d0 d1 d2 d3 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* d4 d5 d6 d7 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* d8 d9 da db */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* dc dd de df */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* e0 e1 e2 e3 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* e4 e5 e6 e7 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* e8 e9 ea eb */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* ec ed ee ef */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* f0 f1 f2 f3 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* f4 f5 f6 f7 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* f8 f9 fa fb */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* fc fd fe ff */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
};


// Performs lexical analysis on the passed string
// Value is stored in lval and its type is returned
void vlex(vstate_t *vs) {
    if (vs->pos < vs->end)
        return vlexs[*vs->pos](vs);
    else
        vs->tok = 0;
}

