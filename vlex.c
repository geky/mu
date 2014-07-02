#include "vlex.h"

#include "vparse.h"
#include "var.h"
#include "num.h"
#include "str.h"

#include <assert.h>


// Currently these are initialized at runtime
// TODO compile them?
static tbl_t *vkeyt = 0;
static tbl_t *vopt = 0;

// Creates internal tables for keywords or uses prexisting.
// Use this to initialize an op table if nescessary.
tbl_t *vkeys(void) {
    if (vkeyt) return vkeyt;

    vkeyt = tbl_create(0).tbl;
    tbl_insert(vkeyt, vcstr("nil"), vrnum(VT_NIL));
    tbl_insert(vkeyt, vcstr("fn"), vrnum(VT_FN));
    tbl_insert(vkeyt, vcstr("let"), vrnum(VT_LET));
    tbl_insert(vkeyt, vcstr("return"), vrnum(VT_RETURN));
    tbl_insert(vkeyt, vcstr("if"), vrnum(VT_IF));
    tbl_insert(vkeyt, vcstr("while"), vrnum(VT_WHILE));
    tbl_insert(vkeyt, vcstr("continue"), vrnum(VT_CONT));
    tbl_insert(vkeyt, vcstr("break"), vrnum(VT_BREAK));
    tbl_insert(vkeyt, vcstr("else"), vrnum(VT_ELSE));
    return vkeyt;
}

tbl_t *vops(void) {
    if (vopt) return vopt;

    vopt = tbl_create(0).tbl;
    tbl_insert(vopt, vcstr("="), vrnum(VT_SET));
    tbl_insert(vopt, vcstr(":"), vrnum(VT_SET));
    tbl_insert(vopt, vcstr("."), vrnum(VT_DOT));
    return vopt;
}


// Lexer definitions for V's tokens
__attribute__((noreturn))
static int vl_bad(struct vstate *vs) {
    assert(false); //TODO: errors: bad parse
}

static int vl_ws(struct vstate *vs);
static int vl_com(struct vstate *vs);
static int vl_op(struct vstate *vs);
static int vl_kw(struct vstate *vs);
static int vl_tok(struct vstate *vs);
static int vl_sep(struct vstate *vs);
static int vl_set(struct vstate *vs);
static int vl_nl(struct vstate *vs);
static int vl_num(struct vstate *vs);
static int vl_str(struct vstate *vs);


static int vl_ws(struct vstate *vs) {
    vs->pos++;
    return vlex(vs);
}

static int vl_com(struct vstate *vs) {
    vs->pos++;

    if (vs->pos >= vs->end || *vs->pos != '`') {
        while (vs->pos < vs->end) {
            unsigned char n = *vs->pos++;
            if (n == '`' || n == '\n')
                break;
        }
    } else {
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
    }

    return vlex(vs);
}

static int vl_op(struct vstate *vs) {
    str_t *str = (str_t *)(vs->ref + 1);
    str_t *kw = vs->pos++;
    var_t op;

    while (vs->pos < vs->end) {
        void *w = vlex_a[*vs->pos];

        if (w != vl_op && w != vl_set)
            break;

        vs->pos++;
    };

    vs->val = vstr(str, kw-str, vs->pos-kw);

    while (1) {
        op = tbl_lookup(vs->ops, vs->val);

        if (!var_isnil(op))
            break;

        vs->pos--;
        vs->val.len--;

        assert(vs->val.len > 0); // TODO errors
    }

    vs->val = op;


    vs->nprec = 0;

    while (vs->pos < vs->end) {
        if (vlex_a[*vs->pos] != vl_ws)
            break;

        vs->pos++;
        vs->nprec++;
    }


    if (var_isnum(op) && op.data >= VT_SET
                      && op.data <= VT_DOT) {
        return op.data;
    } else if (vs->pos < vs->end && 
               vlex_a[*vs->pos] == vl_set) {
        vs->pos++;
        return VT_OPSET;
    } else {
        return VT_OP;
    }
}

static int vl_kw(struct vstate *vs) {
    str_t *str = (str_t *)(vs->ref + 1);
    str_t *kw = vs->pos++;
    var_t key;

    while (vs->pos < vs->end) {
        void *w = vlex_a[*vs->pos];

        if (w != vl_kw && w != vl_num)
            break;

        vs->pos++;
    };

    vs->val = vstr(str, kw-str, vs->pos-kw);
    key = tbl_lookup(vs->keys, vs->val);

    if (var_isnum(key) && key.data >= VT_NIL
                       && key.data <= VT_ELSE)
        return key.data;
    else
        return VT_IDENT;
}

static int vl_tok(struct vstate *vs) {
    return *vs->pos++;
}

static int vl_sep(struct vstate *vs) {
    vs->pos++;
    return VT_SEP;
}

static int vl_set(struct vstate *vs) {
    return vl_op(vs);
}

static int vl_nl(struct vstate *vs) {
    if (vs->paren)
        return vl_ws(vs);
    else
        return vl_sep(vs);
}       

static int vl_num(struct vstate *vs) {
    vs->val = num_parse(&vs->pos, vs->end);
    return VT_NUM;
}

static int vl_str(struct vstate *vs) {
    vs->val = str_parse(&vs->pos, vs->end);
    return VT_STR;
}



// Lookup table of lex functions based 
// only on first character of token
int (* const vlex_a[256])(struct vstate *) = {
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
int vlex(struct vstate *vs) {
    if (vs->pos < vs->end)
        return vlex_a[*vs->pos](vs);
    else
        return 0;
}

