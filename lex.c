#include "lex.h"

#include "parse.h"
#include "num.h"
#include "str.h"
#include "tbl.h"


// Creates internal tables for keywords or uses prexisting.
// Use this to initialize an op table if nescessary.
mu_const tbl_t *mu_keys(void) {
    // Currently this is initialized at runtime
    // TODO compile them?
    static tbl_t *keyt = 0;

    if (keyt) return keyt;

    keyt = tbl_create(0);
    tbl_insert(keyt, mcstr("fn"), muint(T_FN));
    tbl_insert(keyt, mcstr("let"), muint(T_LET));
    tbl_insert(keyt, mcstr("return"), muint(T_RETURN));
    tbl_insert(keyt, mcstr("if"), muint(T_IF));
    tbl_insert(keyt, mcstr("while"), muint(T_WHILE));
    tbl_insert(keyt, mcstr("for"), muint(T_FOR));
    tbl_insert(keyt, mcstr("continue"), muint(T_CONT));
    tbl_insert(keyt, mcstr("break"), muint(T_BREAK));
    tbl_insert(keyt, mcstr("else"), muint(T_ELSE));
    tbl_insert(keyt, mcstr("nil"), muint(T_NIL));
    tbl_insert(keyt, mcstr("args"), muint(T_ARGS));
    tbl_insert(keyt, mcstr("scope"), muint(T_SCOPE));
    
    return keyt;
}

mu_const tbl_t *mu_syms(void) {
    // Currently this is initialized at runtime
    // TODO compile them
    static tbl_t *symt = 0;

    if (symt) return symt;

    symt = tbl_create(0);
    tbl_insert(symt, mcstr("->"), muint(T_ARROW));
    tbl_insert(symt, mcstr("&&"), muint(T_AND));
    tbl_insert(symt, mcstr("||"), muint(T_OR));
    tbl_insert(symt, mcstr("*"), muint(T_REST));
    tbl_insert(symt, mcstr("."), muint(T_DOT));
    tbl_insert(symt, mcstr("="), muint(T_ASSIGN));
    tbl_insert(symt, mcstr(":"), muint(T_COLON));

    return symt;
}


// Lexer definitions for Mu's tokens
extern void (* const lexs[256])(lex_t *);

static mu_noreturn void l_bad(lex_t *l);
static void l_ws(lex_t *l);
static void l_op(lex_t *l);
static void l_kw(lex_t *l);
static void l_tok(lex_t *l);
static void l_nl(lex_t *l);
static void l_num(lex_t *l);
static void l_str(lex_t *l);

// Helper function for skipping whitespace
static void wskip(lex_t *l) {
    while (l->pos < l->end) {
        if (*l->pos == '#') {
            while (l->pos < l->end) {
                if (*l->pos == '\n')
                    break;
                else
                    l->pos++;
            }
        } else if (lexs[*l->pos] == l_ws) {
            l->pos++;
        } else if (l->paren && *l->pos == '\n') {
            l->pos++;
        } else {
            break;
        }
    }
}

static mu_noreturn void l_bad(lex_t *l) {
    mu_err_parse();
}

static void l_ws(lex_t *l) {
    wskip(l);
    return mu_lex(l);
}

static void l_nl(lex_t *l) {
    if (!l->paren) {
        const data_t *npos = l->pos+1;

        while (lexs[*npos] == l_ws && *npos != '#') {
            npos++;
        }

        if (*npos != '\n' && *npos != '#') {
            int_t nindent = npos - (l->pos+1);

            if (nindent > MU_MAXUINTQ) {
                mu_err_parse(); // TODO better message
            } else if (nindent > l->indent) {
                l->tok = '{'; // TODO does this only happen after \n?
                l->val = muint(nindent - l->indent);
                l->indent = nindent;
                return;
            } else if (nindent < l->indent) {
                l->tok = '}';
                l->val = muint(l->indent - nindent);
                l->indent = nindent;
                return;
            }
        }

        l->tok = ';';
        l->pos = npos;
    } else {
        return l_ws(l);
    }
}

static void l_op(lex_t *l) {
    const data_t *kw_pos = l->pos++;
    const data_t *kw_end;

    while (l->pos < l->end && (lexs[*l->pos] == l_op ||
                               *l->pos == ':' ||
                               *l->pos == '=')) {
        l->pos++;
    }

    kw_end = l->pos;
    l->val = mnstr(kw_pos, kw_end-kw_pos);
    mu_t tok = tbl_lookup(mu_syms(), l->val);

    wskip(l);
    l->rprec = l->pos - kw_end;

    // TODO make these strings preinterned so this is actually reasonable
    if (!isnil(tok)) {
        l->tok = getuint(tok);
    } else if (kw_end[-1] == '=') {
        l->tok = T_OPSET;
        l->val = mnstr(kw_pos, kw_end-kw_pos-1);
    } else {
        l->tok = T_OP;
    }
}

static void l_kw(lex_t *l) {
    const data_t *kw_pos = l->pos++;
    const data_t *kw_end;

    while (l->pos < l->end && (lexs[*l->pos] == l_kw ||
                               lexs[*l->pos] == l_num)) {
        l->pos++;
    }

    kw_end = l->pos;
    l->val = mnstr(kw_pos, kw_end-kw_pos);
    mu_t tok = tbl_lookup(mu_keys(), l->val);

    wskip(l);
    l->rprec = l->pos - kw_end;

    if (isnil(tok)) {
        l->tok = T_SYM;
    } else if (getuint(tok) == T_FN && lexs[*l->pos] == l_kw) {
        l->tok = T_FNSET;
    } else {
        l->tok = getuint(tok);
    }
}

static void l_tok(lex_t *l) {
    l->tok = *l->pos++;

    // Note: this is really just for brackets
    l->val = muint(1);
}

static void l_num(lex_t *l) {
    l->tok = T_IMM;
    l->val = mnum(num_parse(&l->pos, l->end));
}

static void l_str(lex_t *l) {
    l->tok = T_IMM;
    l->val = mstr(str_parse(&l->pos, l->end));
}


// Lookup table of lex functions based 
// only on first character of token
void (* const lexs[256])(lex_t *) = {
/* 00 01 02 03 */   l_bad,  l_bad,  l_bad,  l_bad,
/* 04 05 06 \a */   l_bad,  l_bad,  l_bad,  l_bad,
/* \b \t \n \v */   l_bad,  l_ws,   l_nl,   l_ws,
/* \f \r 0e 0f */   l_ws,   l_ws,   l_bad,  l_bad,
/* 10 11 12 13 */   l_bad,  l_bad,  l_bad,  l_bad,
/* 14 15 16 17 */   l_bad,  l_bad,  l_bad,  l_bad,
/* 18 19 1a 1b */   l_bad,  l_bad,  l_bad,  l_bad,
/* 1c 1d 1e 1f */   l_bad,  l_bad,  l_bad,  l_bad,
/*     !  "  # */   l_ws,   l_op,   l_str,  l_ws,
/*  $  %  &  ' */   l_op,   l_op,   l_op,   l_str,
/*  (  )  *  + */   l_tok,  l_tok,  l_op,   l_op,
/*  ,  -  .  / */   l_tok,  l_op,   l_op,   l_op,
/*  0  1  2  3 */   l_num,  l_num,  l_num,  l_num,
/*  4  5  6  7 */   l_num,  l_num,  l_num,  l_num,
/*  8  9  :  ; */   l_num,  l_num,  l_tok,  l_tok,
/*  <  =  >  ? */   l_op,   l_tok,  l_op,   l_op,
/*  @  A  B  C */   l_op,   l_kw,   l_kw,   l_kw,
/*  D  E  F  G */   l_kw,   l_kw,   l_kw,   l_kw,
/*  H  I  J  K */   l_kw,   l_kw,   l_kw,   l_kw,
/*  L  M  N  O */   l_kw,   l_kw,   l_kw,   l_kw,
/*  P  Q  R  S */   l_kw,   l_kw,   l_kw,   l_kw,
/*  T  U  V  W */   l_kw,   l_kw,   l_kw,   l_kw,
/*  X  Y  Z  [ */   l_kw,   l_kw,   l_kw,   l_tok,
/*  \  ]  ^  _ */   l_tok,  l_tok,  l_op,   l_kw,
/*  `  a  b  c */   l_op,   l_kw,   l_kw,   l_kw,   
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
void mu_lex(lex_t *l) {
    if (l->pos >= l->end)
        l->tok = 0;
    else
        lexs[*l->pos](l);
}

// TODO implement this correctly
void mu_scan(lex_t *l) {
    return mu_lex(l);
}

