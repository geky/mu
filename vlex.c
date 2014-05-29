#include "vlex.h"

#include "vparse.h"
#include "num.h"
#include "str.h"

#include <assert.h>


// Definitions of keywords
static int kw_none(struct vstate *vs) {
    (*vs->ref)++;
    return VTOK_IDENT;
}

// TODO ALL OF THESE ARE BROKEN: vs->val.str[ind] != string ind
static int kw_fn(struct vstate *vs) {
    //                           already read 'f'
    if (vs->val.len == 2 && vs->val.str[1] == 'n')
        return VTOK_FN;
    else
        return kw_none(vs);
}

static int kw_return(struct vstate *vs) {
    //                           already read 'r'
    if (vs->val.len == 6 && vs->val.str[1] == 'e'
                         && vs->val.str[2] == 't'
                         && vs->val.str[3] == 'u'
                         && vs->val.str[4] == 'r'
                         && vs->val.str[5] == 'n')
        return VTOK_RETURN;
    else
        return kw_none(vs);
}
    

static int (* const kw_a[64])(struct vstate *) = {
/*  @  A  B  C */         0,   kw_none,   kw_none,   kw_none,
/*  D  E  F  G */   kw_none,   kw_none,   kw_none,   kw_none,
/*  H  I  J  K */   kw_none,   kw_none,   kw_none,   kw_none,
/*  L  M  N  O */   kw_none,   kw_none,   kw_none,   kw_none,
/*  P  Q  R  S */   kw_none,   kw_none,   kw_none,   kw_none,
/*  T  U  V  W */   kw_none,   kw_none,   kw_none,   kw_none,
/*  X  Y  Z  [ */   kw_none,   kw_none,   kw_none,         0,
/*  \  ]  ^  _ */         0,         0,         0,   kw_none,
/*  `  a  b  c */         0,   kw_none,   kw_none,   kw_none,  
/*  d  e  f  g */   kw_none,   kw_none,   kw_fn,     kw_none,  
/*  h  i  j  k */   kw_none,   kw_none,   kw_none,   kw_none,  
/*  l  m  n  o */   kw_none,   kw_none,   kw_none,   kw_none,  
/*  p  q  r  s */   kw_none,   kw_none,   kw_return, kw_none,  
/*  t  u  v  w */   kw_none,   kw_none,   kw_none,   kw_none,  
/*  x  y  z  { */   kw_none,   kw_none,   kw_none,         0,
/*  |  }  ~ 7f */         0,         0,         0,         0,
};


// Definitions of various tokens
static int vl_num(struct vstate *);
static int vl_str(struct vstate *);

static int vl_bad(struct vstate *vs) {
    assert(false); //TODO: errors: bad parse
}

static int vl_ws(struct vstate *vs) {
    vs->off++;
    return vlex(vs);
}

static int vl_com(struct vstate *vs) {
    vs->off++;

    if (vs->off >= vs->end || *vs->off != '`') {
        while (vs->off < vs->end) {
            unsigned char n = *vs->off++;
            if (n == '`' || n == '\n')
                break;
        }
    } else {
        int count = 1;
        int seen = 0;

        while (vs->off < vs->end) {
            if (*vs->off++ != '`')
                break;

            count++;
        }

        while (vs->off < vs->end && seen < count) {
            if (*vs->off++ == '`')
                seen++;
            else
                seen = 0;
        }
    }

    return vlex(vs);
}   

static int vl_op(struct vstate *vs) {
    vs->off++;
    return VTOK_OP;
}


static int vl_kw(struct vstate *vs) {
    str_t *kw = vs->off;

    do {
        void *w = vlex_a[*vs->off++];
        if (w != vl_kw && w != vl_num)
            break;
    } while (vs->off < vs->end);

    
    str_t *str = (str_t*)(vs->ref + 1);
    vs->val = vstr(str, kw-str, vs->off-kw - 1);

    return kw_a[0x3f & *kw](vs);
}

static int vl_tok(struct vstate *vs) {
    return *vs->off++;
}

static int vl_num(struct vstate *vs) {
    vs->val = num_parse(&vs->off, vs->end);
    return VTOK_NUM;
}

static int vl_str(struct vstate *vs) {
    vs->val = str_parse(&vs->off, vs->end);
    return VTOK_STR;
}



// Lookup table of lex functions based 
// only on first character of token
int (* const vlex_a[256])(struct vstate *) = {
/* 00 01 02 03 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 04 05 06 \a */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* \b \t \n \v */   vl_bad,   vl_ws,    vl_tok,   vl_ws,
/* \f \r 0e 0f */   vl_ws,    vl_ws,    vl_bad,   vl_bad,
/* 10 11 12 13 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 14 15 16 17 */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 18 19 1a 1b */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/* 1c 1d 1e 1f */   vl_bad,   vl_bad,   vl_bad,   vl_bad,
/*     !  "  # */   vl_ws,    vl_op,    vl_str,   vl_op,
/*  $  %  &  ' */   vl_op,    vl_op,    vl_op,    vl_str,
/*  (  )  *  + */   vl_tok,   vl_tok,   vl_op,    vl_op,
/*  ,  -  .  / */   vl_tok,   vl_op,    vl_op,    vl_op,
/*  0  1  2  3 */   vl_num,   vl_num,   vl_num,   vl_num,
/*  4  5  6  7 */   vl_num,   vl_num,   vl_num,   vl_num,
/*  8  9  :  ; */   vl_num,   vl_num,   vl_op,    vl_tok,
/*  <  =  >  ? */   vl_op,    vl_op,    vl_op,    vl_op,
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
    if (vs->off < vs->end)
        return vlex_a[*vs->off](vs);
    else
        return 0;
}

