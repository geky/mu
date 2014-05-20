#include "vlex.h"

#include "v.tab.h"
#include "num.h"
#include "str.h"

#include <assert.h>


// Error handling
void verror(struct vlex *ls, const char *s) {
    assert(false); // TODO: errors: syntax error
}

// Definitions of keywords
static int kw_none(var_t *lval, struct vlex *ls) {
    (*ls->ref)++;
    return IDENT;
}

static int kw_fn(var_t *lval, struct vlex *ls) {
    //                       already read 'f'
    if (lval->len == 2 && lval->str[1] == 'n')
        return FN;
    else
        return kw_none(lval, ls);
}

static int kw_return(var_t *lval, struct vlex *ls) {
    //                       already read 'r'
    if (lval->len == 6 && lval->str[1] == 'e'
                       && lval->str[2] == 't'
                       && lval->str[3] == 'u'
                       && lval->str[4] == 'r'
                       && lval->str[5] == 'n')
        return RETURN;
    else
        return kw_none(lval, ls);
}
    

static int (* const kw_a[64])(var_t *lval, struct vlex *ls) = {
/*  @  A  B  C */   kw_none,   kw_none,   kw_none,   kw_none,
/*  D  E  F  G */   kw_none,   kw_none,   kw_none,   kw_none,
/*  H  I  J  K */   kw_none,   kw_none,   kw_none,   kw_none,
/*  L  M  N  O */   kw_none,   kw_none,   kw_none,   kw_none,
/*  P  Q  R  S */   kw_none,   kw_none,   kw_none,   kw_none,
/*  T  U  V  W */   kw_none,   kw_none,   kw_none,   kw_none,
/*  X  Y  Z  [ */   kw_none,   kw_none,   kw_none,   kw_none,
/*  \  ]  ^  _ */   kw_none,   kw_none,   kw_none,   kw_none,
/*  `  a  b  c */   kw_none,   kw_none,   kw_none,   kw_none,  
/*  d  e  f  g */   kw_none,   kw_none,   kw_fn,     kw_none,  
/*  h  i  j  k */   kw_none,   kw_none,   kw_none,   kw_none,  
/*  l  m  n  o */   kw_none,   kw_none,   kw_none,   kw_none,  
/*  p  q  r  s */   kw_none,   kw_none,   kw_return, kw_none,  
/*  t  u  v  w */   kw_none,   kw_none,   kw_none,   kw_none,  
/*  x  y  z  { */   kw_none,   kw_none,   kw_none,   kw_none,
/*  |  }  ~ 7f */   kw_none,   kw_none,   kw_none,   kw_none,
};


// Definitions of various tokens
static int lex_num(var_t *lval, struct vlex *ls);
static int lex_str(var_t *lval, struct vlex *ls);

static int lex_bad(var_t *lval, struct vlex *ls) {
    assert(false); //TODO: errors: bad parse
}

static int lex_ws(var_t *lval, struct vlex *ls) {
    ls->off++;
    return vlex(lval, ls);
}

static int lex_com(var_t *lval, struct vlex *ls) {
    ls->off++;

    if (ls->off >= ls->end || *ls->off != '`') {
        while (ls->off < ls->end) {
            unsigned char n = *ls->off++;
            if (n == '`' || n == '\n')
                break;
        }
    } else {
        int count = 1;
        int seen = 0;

        while (ls->off < ls->end) {
            if (*ls->off++ != '`')
                break;

            count++;
        }

        while (ls->off < ls->end && seen < count) {
            if (*ls->off++ == '`')
                seen++;
            else
                seen = 0;
        }
    }

    return vlex(lval, ls);
}   

static int lex_op(var_t *lval, struct vlex *ls) {
    ls->off++;
    return OP;
}


static int lex_kw(var_t *lval, struct vlex *ls) {
    str_t *kw = ls->off;

    do {
        void *w = vlex_a[*ls->off++];
        if (w != lex_kw && w != lex_num)
            break;
    } while (ls->off < ls->end);

    lval->ref = ls->ref;
    lval->type = TYPE_STR;
    lval->off = kw - lval->str;
    lval->len = ls->off - kw;

    return kw_a[0x3f & *kw](lval, ls);
}

static int lex_tok(var_t *lval, struct vlex *ls) {
    return *ls->off++;
}

static int lex_num(var_t *lval, struct vlex *ls) {
    *lval = num_parse(&ls->off, ls->end);
    return NUM;
}

static int lex_str(var_t *lval, struct vlex *ls) {
    *lval = str_parse(&ls->off, ls->end);
    return STR;
}



// Lookup table of lex functions based 
// only on first character of token
int (* const vlex_a[256])(var_t *lval, struct vlex *) = {
/* 00 01 02 03 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* 04 05 06 \a */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* \b \t \n \v */   lex_bad,   lex_ws,    lex_tok,   lex_ws,
/* \f \r 0e 0f */   lex_ws,    lex_ws,    lex_bad,   lex_bad,
/* 10 11 12 13 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* 14 15 16 17 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* 18 19 1a 1b */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* 1c 1d 1e 1f */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/*     !  "  # */   lex_ws,    lex_op,    lex_str,   lex_op,
/*  $  %  &  ' */   lex_op,    lex_op,    lex_op,    lex_str,
/*  (  )  *  + */   lex_tok,   lex_tok,   lex_op,    lex_op,
/*  ,  -  .  / */   lex_tok,   lex_op,    lex_op,    lex_op,
/*  0  1  2  3 */   lex_num,   lex_num,   lex_num,   lex_num,
/*  4  5  6  7 */   lex_num,   lex_num,   lex_num,   lex_num,
/*  8  9  :  ; */   lex_num,   lex_num,   lex_op,    lex_tok,
/*  <  =  >  ? */   lex_op,    lex_op,    lex_op,    lex_op,
/*  @  A  B  C */   lex_op,    lex_kw,    lex_kw,    lex_kw,
/*  D  E  F  G */   lex_kw,    lex_kw,    lex_kw,    lex_kw,
/*  H  I  J  K */   lex_kw,    lex_kw,    lex_kw,    lex_kw,
/*  L  M  N  O */   lex_kw,    lex_kw,    lex_kw,    lex_kw,
/*  P  Q  R  S */   lex_kw,    lex_kw,    lex_kw,    lex_kw,
/*  T  U  V  W */   lex_kw,    lex_kw,    lex_kw,    lex_kw,
/*  X  Y  Z  [ */   lex_kw,    lex_kw,    lex_kw,    lex_tok,
/*  \  ]  ^  _ */   lex_tok,   lex_tok,   lex_op,    lex_kw,
/*  `  a  b  c */   lex_com,   lex_kw,    lex_kw,    lex_kw,   
/*  d  e  f  g */   lex_kw,    lex_kw,    lex_kw,    lex_kw,   
/*  h  i  j  k */   lex_kw,    lex_kw,    lex_kw,    lex_kw,   
/*  l  m  n  o */   lex_kw,    lex_kw,    lex_kw,    lex_kw,   
/*  p  q  r  s */   lex_kw,    lex_kw,    lex_kw,    lex_kw,   
/*  t  u  v  w */   lex_kw,    lex_kw,    lex_kw,    lex_kw,   
/*  x  y  z  { */   lex_kw,    lex_kw,    lex_kw,    lex_tok,
/*  |  }  ~ 7f */   lex_op,    lex_tok,   lex_op,    lex_bad,
/* 80 81 82 83 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* 84 85 86 87 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* 88 89 8a 8b */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* 8c 8d 8e 8f */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* 90 91 92 93 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* 94 95 96 97 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* 98 99 9a 9b */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* 9c 9d 9e 9f */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* a0 a1 a2 a3 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* a4 a5 a6 a7 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* a8 a9 aa ab */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* ac ad ae af */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* b0 b1 b2 b3 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* b4 b5 b6 b7 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* b8 b9 ba bb */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* bc bd be bf */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* c0 c1 c2 c3 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* c4 c5 c6 c7 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* c8 c9 ca cb */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* cc cd ce cf */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* d0 d1 d2 d3 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* d4 d5 d6 d7 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* d8 d9 da db */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* dc dd de df */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* e0 e1 e2 e3 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* e4 e5 e6 e7 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* e8 e9 ea eb */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* ec ed ee ef */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* f0 f1 f2 f3 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* f4 f5 f6 f7 */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* f8 f9 fa fb */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
/* fc fd fe ff */   lex_bad,   lex_bad,   lex_bad,   lex_bad,
};

// Performs lexical analysis on the passed string
// Value is stored in lval and its type is returned
int vlex(var_t *lval, struct vlex *ls) {
    if (ls->off < ls->end)
        return vlex_a[*ls->off](lval, ls);
    else
        return 0;
}

