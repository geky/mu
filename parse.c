#include "parse.h"

#include "lex.h"
#include "tbl.h"

#include <stdio.h>


// Parsing error handling
#define mu_unexpected(m) _mu_unexpected(m, __FUNCTION__)
__attribute__((noreturn))
static void _mu_unexpected(mstate_t *m, const char *fn) {
    printf("\033[31munexpected (%d) in %s\033[0m\n", m->tok, fn);
    err_parse(m->eh);
}

// Expect a token or fail
#define mu_expect(m, tok) _mu_expect(m, tok, __FUNCTION__)
static void _mu_expect(mstate_t *m, mtok_t tok, const char *fn) {
    if (m->tok != tok) {
        printf("\033[31mexpected (%d) not (%d) in %s\033[0m\n", tok, m->tok, fn);
        err_parse(m->eh);
    }

    mu_lex(m);
}


// Allocation functions for managing bytecode space
static void mu_enlarge(mstate_t *m, int count) {
    struct fnstate *fn = m->fn;
    fn->ins += count;

    while (fn->ins > fn->len) {
        if (((int)fn->len << 1) > MU_MAXLEN)
            err_len(m->eh);

        fn->len <<= 1;
    }
        
    fn->bcode = mu_realloc(fn->bcode, fn->len, fn->len << 1, m->eh);
}

static void mu_enlargein(mstate_t *m, int count, int ins) {
    mu_enlarge(m, count);

    memmove(&m->fn->bcode[ins] + count,
            &m->fn->bcode[ins], 
            m->fn->ins-count - ins);
}


// Different encoding calls
static void mu_enc(mstate_t *m, mop_t op) {
    int count = mu_count(op, 0);
    mu_enlarge(m, count);
    mu_encode(&m->fn->bcode[m->fn->ins-count], op, 0);
}

static void mu_enca(mstate_t *m, mop_t op, marg_t arg) {
    int count = mu_count(op | MOP_ARG, arg);
    mu_enlarge(m, count);
    mu_encode(&m->fn->bcode[m->fn->ins-count], op | MOP_ARG, arg);
}

__attribute__((const))
static int mu_size(mstate_t *m, mop_t op) {
    return mu_count(op, 0);
}

__attribute__((const))
static int mu_sizea(mstate_t *m, mop_t op, marg_t arg) {
    return mu_count(op | MOP_ARG, arg);
}

static int mu_insert(mstate_t *m, mop_t op, int ins) {
    mu_encode(&m->fn->bcode[ins], op, 0);
    return mu_count(op, 0);
}

static int mu_inserta(mstate_t *m, mop_t op, marg_t arg, int ins) {
    mu_encode(&m->fn->bcode[ins], op | MOP_ARG, arg);
    return mu_count(MOP_ARG, arg);
}


// Helping functions for code generation
static marg_t mu_accvar(mstate_t *m, var_t v) {
    var_t index = tbl_lookup(m->fn->vars, v);

    if (!isnil(index))
        return index.data;

    marg_t arg = m->fn->vars->len;
    tbl_assign(m->fn->vars, v, vraw(arg), m->eh);
    return arg;
}

static void mu_patch(mstate_t *m, tbl_t *jtbl, int ins) {
    tbl_for_begin (k, v, jtbl) {
        mu_inserta(m, MJUMP, ins - (v.data+m->jsize), v.data);
    } tbl_for_end;

    tbl_dec(jtbl);
}

static tbl_t *mu_revargs(tbl_t *args, eh_t *eh) {
    tbl_t *res = tbl_create(0, eh);
    int i;

    for (i = args->len-1; i >= 0; i--) {
        tbl_append(res, tbl_lookup(args, vnum(i)), eh);
    }

    tbl_dec(args);
    return res;
}

static void mu_unpack(mstate_t *m, tbl_t *map) {
    mu_enc(m, MSCOPE);

    tbl_for_begin (k, v, map) {
        if (v.type == MU_TBL) {
            mu_enca(m, MDUP, 1);
            mu_enc(m, MNIL);
            mu_enca(m, MLOOKDN, getnum(k));
            mu_unpack(m, v.tbl);
        } else {
            mu_enca(m, MVAR, mu_accvar(m, v));
            mu_enca(m, MDUP, 2);
            mu_enca(m, MDUP, 1);
            mu_enca(m, MLOOKDN, getnum(k));
            mu_enc(m, MINSERT);
        }
    } tbl_for_end;

    mu_enc(m, MDROP);
    mu_enc(m, MDROP);
}



// Rules for Mu's grammar
static void mp_value(mstate_t *m);
static void mp_phrase(mstate_t *m);
static void mp_fn(mstate_t *m);
static void mp_if(mstate_t *m);
static void mp_while(mstate_t *m);
static void mp_for(mstate_t *m);
static void mp_expr_op(mstate_t *m);
static void mp_expr(mstate_t *m);
static void mp_args_follow(mstate_t *m);
static void mp_args_entry(mstate_t *m);
static void mp_args(mstate_t *m);
static void mp_table_follow(mstate_t *m);
static void mp_table_assign(mstate_t *m);
static void mp_table_entry(mstate_t *m);
static void mp_table(mstate_t *m);
static void mp_stmt_follow(mstate_t *m);
static void mp_stmt_insert(mstate_t *m);
static void mp_stmt_assign(mstate_t *m);
static void mp_stmt(mstate_t *m);
static void mp_stmt_list(mstate_t *m);
static void mp_stmt_entry(mstate_t *m);


static void mp_value(mstate_t *m) {
    struct opstate op = m->op;
    m->op.lprec = -1;

    if (m->stmt) {
        m->stmt = false;
        m->left = false;
        mp_expr(m);
        m->stmt = true;
        m->left = true;
    } else if (m->left) {
        m->left = false;
        mp_expr(m);
        m->left = true;
    } else {
        mp_expr(m);
    }

    m->op = op;

    if (m->indirect)
        mu_enc(m, MLOOKUP);
}

static void mp_phrase(mstate_t *m) {
    if (m->stmt)
        mp_stmt(m);
    else
        mp_value(m);
}


static void mp_fn(mstate_t *m) {
    mu_expect(m, '(');
    mp_args(m);
    tbl_t *args = m->args;
    mu_expect(m, ')');

    struct fnstate *f = m->fn;
    struct jstate j = m->j;

    fn_t *fn = fn_create_nested(args, m, m->eh);
    tbl_append(f->fns, vraw((uint32_t)fn), m->eh);

    m->j = j;
    m->fn = f;

    mu_enca(m, MFN, f->fns->len-1);
}

static void mp_if(mstate_t *m) {
    mu_expect(m, '(');
    mp_value(m);
    mu_expect(m, ')');

    int i_ins = m->fn->ins;
    mu_enlarge(m, m->jfsize);
    mp_phrase(m);

    if (m->tok == MT_ELSE) {
        int e_ins = m->fn->ins;
        mu_enlarge(m, m->jsize);
        mu_lex(m);
        mp_phrase(m);

        mu_inserta(m, MJFALSE, (e_ins+m->jsize) - (i_ins+m->jfsize), i_ins);
        mu_inserta(m, MJUMP, m->fn->ins - (e_ins+m->jsize), e_ins);
    } else {
        if (!m->stmt) {
            mu_enca(m, MJUMP, mu_size(m, MNIL));
            mu_enc(m, MNIL);
        }
        mu_inserta(m, MJFALSE, m->fn->ins - (i_ins+m->jfsize), i_ins);
    }
}

static void mp_while(mstate_t *m) {
    if (!m->stmt) mu_enc(m, MTBL);
    struct jstate j = m->j;
    m->j.ctbl = tbl_create(0, m->eh);
    m->j.btbl = tbl_create(0, m->eh);
    int w_ins = m->fn->ins;

    mu_expect(m, '(');
    mp_value(m);
    mu_expect(m, ')');

    int j_ins = m->fn->ins;
    mu_enlarge(m, m->jfsize);
    mp_phrase(m);
    if (!m->stmt) mu_enc(m, MAPPEND);

    mu_inserta(m, MJFALSE, (m->fn->ins+m->jsize) - (j_ins+m->jfsize), j_ins);
    mu_enca(m, MJUMP, w_ins - (m->fn->ins+m->jsize));

    mu_patch(m, m->j.ctbl, w_ins);
    tbl_t *btbl = m->j.btbl;
    m->j = j;

    if (m->stmt && m->tok == MT_ELSE) {
        mu_lex(m);
        mp_stmt(m);
    }

    mu_patch(m, btbl, m->fn->ins);
}

static void mp_for(mstate_t *m) {
    if (!m->stmt) mu_enc(m, MTBL);

    struct jstate j = m->j;
    m->j.ctbl = tbl_create(0, m->eh);
    m->j.btbl = tbl_create(0, m->eh);

    mu_expect(m, '(');
    mp_args(m);
    tbl_t *args = mu_revargs(m->args, m->eh);
    mu_expect(m, MT_SET);
    mp_value(m);
    mu_enc(m, MITER);
    mu_expect(m, ')');

    int f_ins = m->fn->ins;
    mu_enlarge(m, m->jsize);

    mu_unpack(m, args);
    if (m->stmt) {
        mp_phrase(m);
    } else {
        mu_enca(m, MDUP, 1);
        mp_phrase(m);
        mu_enc(m, MAPPEND);
        mu_enc(m, MDROP);
    }

    mu_inserta(m, MJUMP, m->fn->ins - (f_ins+m->jsize), f_ins);
    mu_patch(m, m->j.ctbl, m->fn->ins);

    mu_enca(m, MDUP, 0);
    mu_enc(m, MTBL);
    mu_enc(m, MCALL);
    mu_enca(m, MDUP, 0);

    mu_enca(m, MJTRUE, (f_ins+m->jsize) - (m->fn->ins+m->jtsize));
    mu_enc(m, MDROP);

    tbl_t *btbl = m->j.btbl;
    m->j = j;

    if (m->stmt && m->tok == MT_ELSE) {
        mu_lex(m);
        mp_stmt(m);
    }

    mu_patch(m, btbl, m->fn->ins);
    mu_enc(m, MDROP);
}


static void mp_expr_op(mstate_t *m) {
    switch (m->tok) {
        case MT_KEY:    if (m->indirect) mu_enc(m, MLOOKUP);
                        mu_lex(m);
                        mu_enca(m, MVAR, mu_accvar(m, m->val));
                        mu_lex(m);
                        m->indirect = true;
                        return mp_expr_op(m);

        case '[':       if (m->indirect) mu_enc(m, MLOOKUP);
                        m->paren++;
                        mu_lex(m);
                        mp_value(m);
                        m->paren--;
                        mu_expect(m, ']');
                        m->indirect = true;
                        return mp_expr_op(m);

        case '(':       if (m->indirect) mu_enc(m, MLOOKUP);
                        mu_enc(m, MTBL);
                        m->paren++;
                        mp_table(m);
                        m->paren--;
                        mu_expect(m, ')');
                        mu_enc(m, MCALL);
                        m->indirect = false;
                        return mp_expr_op(m);

        case MT_OP:     if (m->op.lprec <= m->op.rprec) return;
                        if (m->indirect) mu_enc(m, MLOOKUP);
                        mu_enc(m, MAPPEND);
                        {   struct opstate op = m->op;
                            marg_t tblarg = mu_accvar(m, vcstr("ops"));
                            marg_t symarg = mu_accvar(m, m->val);
                            mu_enlargein(m, mu_size(m, MSCOPE) +
                                           mu_sizea(m, MVAR, tblarg) +
                                           mu_size(m, MLOOKUP) +
                                           mu_sizea(m, MVAR, symarg) +
                                           mu_size(m, MLOOKUP) +
                                           mu_size(m, MTBL), m->op.ins);
                            m->op.ins += mu_insert(m, MSCOPE, m->op.ins);
                            m->op.ins += mu_inserta(m, MVAR, tblarg, m->op.ins);
                            m->op.ins += mu_insert(m, MLOOKUP, m->op.ins);
                            m->op.ins += mu_inserta(m, MVAR, symarg, m->op.ins);
                            m->op.ins += mu_insert(m, MLOOKUP, m->op.ins);
                            m->op.ins += mu_insert(m, MTBL, m->op.ins);
                            m->op.lprec = m->op.rprec;
                            mu_lex(m);
                            mp_expr(m);
                            m->op = op;
                        }
                        if (m->indirect) mu_enc(m, MLOOKUP);
                        mu_enc(m, MAPPEND);
                        mu_enc(m, MCALL);
                        m->indirect = false;
                        return mp_expr_op(m);

        case MT_AND:    if (m->op.lprec <= m->op.rprec) return;
                        if (m->indirect) mu_enc(m, MLOOKUP);
                        mu_enca(m, MDUP, 0);
                        {   int a_ins = m->fn->ins;
                            struct opstate op = m->op;
                            mu_enlarge(m, m->jfsize);
                            mu_enc(m, MDROP);
                            m->op.lprec = m->op.rprec;
                            mu_lex(m);
                            mp_expr(m);
                            if (m->indirect) mu_enc(m, MLOOKUP);
                            m->op = op;
                            mu_inserta(m, MJFALSE, 
                                     m->fn->ins - (a_ins+m->jfsize), a_ins);
                        }
                        m->indirect = false;
                        return mp_expr_op(m);
                            
        case MT_OR:     if (m->op.lprec <= m->op.rprec) return;
                        if (m->indirect) mu_enc(m, MLOOKUP);
                        mu_enca(m, MDUP, 0);
                        {   int o_ins = m->fn->ins;
                            struct opstate op = m->op;
                            mu_enlarge(m, m->jtsize);
                            mu_enc(m, MDROP);
                            m->op.lprec = m->op.rprec;
                            mu_lex(m);
                            mp_expr(m);
                            if (m->indirect) mu_enc(m, MLOOKUP);
                            m->op = op;
                            mu_inserta(m, MJTRUE, 
                                     m->fn->ins - (o_ins+m->jtsize), o_ins);
                        }
                        m->indirect = false;
                        return mp_expr_op(m);

        default:        return;
    }
}

static void mp_expr(mstate_t *m) {
    m->op.ins = m->fn->ins;

    switch (m->tok) {
        case MT_IDENT:  mu_enc(m, MSCOPE);
                        mu_enca(m, MVAR, mu_accvar(m, m->val));
                        m->indirect = true;
                        mu_lex(m);
                        return mp_expr_op(m);

        case MT_NIL:    mu_enc(m, MSCOPE);
                        mu_enc(m, MNIL);
                        m->indirect = true;
                        mu_lex(m);
                        return mp_expr_op(m);

        case MT_LIT:    mu_enca(m, MVAR, mu_accvar(m, m->val));
                        m->indirect = false;
                        mu_lex(m);
                        return mp_expr_op(m);

        case '[':       mu_enc(m, MTBL);
                        mp_table(m);
                        mu_expect(m, ']');
                        m->indirect = false;
                        return mp_expr_op(m);

        case '(':       m->paren++;
                        mu_lex(m);
                        mp_value(m);
                        m->paren--;
                        mu_expect(m, ')');
                        return mp_expr_op(m);

        case MT_OP:     mu_enc(m, MSCOPE);
                        mu_enca(m, MVAR, mu_accvar(m, vcstr("ops")));
                        mu_enc(m, MLOOKUP);
                        mu_enca(m, MVAR, mu_accvar(m, m->val));
                        mu_enc(m, MLOOKUP);
                        mu_enc(m, MTBL);
                        mu_lex(m);
                        mp_value(m);
                        mu_enc(m, MAPPEND);
                        mu_enc(m, MCALL);
                        m->indirect = false;
                        return mp_expr_op(m);

        case MT_IF:     mu_lex(m);
                        mp_if(m);
                        m->indirect = false;
                        return mp_expr_op(m);

        case MT_WHILE:  mu_lex(m);
                        mp_while(m);
                        m->indirect = false;
                        return mp_expr_op(m); 

        case MT_FOR:    mu_lex(m);
                        mp_for(m);
                        m->indirect = false;
                        return mp_expr_op(m);

        case MT_FN:     mu_lex(m);
                        mp_fn(m);
                        m->indirect = false;
                        return mp_expr_op(m);

        default:        mu_unexpected(m);
    }
}


static void mp_args_follow(mstate_t *m) {
    switch (m->tok) {
        case MT_SEP:    mu_lex(m);
                        return mp_args_entry(m);

        default:        return;
    }
}

static void mp_args_entry(mstate_t *m) {
    switch (m->tok) {
        case MT_FN:
        case MT_IF:
        case MT_WHILE:
        case MT_FOR:
        case MT_IDENT:  
        case MT_NIL:
        case MT_LIT:    tbl_append(m->args, m->val, m->eh);
                        mu_lex(m);
                        return mp_args_follow(m);

        case '[':       {   tbl_t *args = m->args;
                            mu_lex(m);
                            mp_args(m);
                            tbl_append(args, vtbl(m->args), m->eh);
                            m->args = args;
                        }
                        mu_expect(m, ']');
                        return mp_args_follow(m);

        default:        return mp_args_follow(m);
    }
}

static void mp_args(mstate_t *m) {
    m->args = tbl_create(0, m->eh);

    return mp_args_entry(m);
}


static void mp_table_follow(mstate_t *m) {
    switch (m->tok) {
        case MT_SEP:    return mp_table(m);

        default:        return;
    }
}

static void mp_table_assign(mstate_t *m) {
    switch (m->tok) {
        case MT_SET:    mu_lex(m);
                        mp_value(m);
                        mu_enc(m, MINSERT);
                        return mp_table_follow(m);

        default:        mu_enc(m, MAPPEND);
                        return mp_table_follow(m);
    }
}
        
static void mp_table_entry(mstate_t *m) {
    m->key = true;
    mu_lex(m);
    m->key = false;

    switch (m->tok) {
        case MT_IDSET:  mu_enca(m, MVAR, mu_accvar(m, m->val));
                        mu_lex(m);
                        mu_expect(m, MT_SET);
                        mp_value(m);
                        mu_enc(m, MINSERT);
                        return mp_table_follow(m);

        case MT_FNSET:  mu_lex(m);
                        mu_expect(m, MT_IDENT);
                        mu_enca(m, MVAR, mu_accvar(m, m->val));
                        mp_fn(m);
                        mu_enc(m, MINSERT);
                        return mp_table_follow(m);

        case MT_SEP:    return mp_table(m);

        case MT_IDENT:
        case MT_NIL:
        case MT_FN:
        case MT_IF:
        case MT_WHILE:
        case MT_FOR:
        case MT_LIT:
        case MT_OP:
        case '[':
        case '(':       {   struct opstate op = m->op;
                            m->op.lprec = -1;
                            mp_expr(m);
                            m->op = op;
                        }
                        if (m->indirect) mu_enc(m, MLOOKUP);
                        return mp_table_assign(m);

        default:        return;
    }
}

static void mp_table(mstate_t *m) {
    uint8_t paren = m->paren;
    m->paren = false;

    m->left = true;
    mp_table_entry(m);

    m->paren = paren;
}


static void mp_stmt_follow(mstate_t *m) {
    switch (m->tok) {
        case MT_SEP:    mu_lex(m);
                        return mp_stmt_entry(m);

        default:        return;
    }
}

static void mp_stmt_insert(mstate_t *m) {
    switch (m->tok) {
        case '[':       mu_lex(m);
                        mp_args(m);
                        mu_expect(m, ']');
                        mu_expect(m, MT_SET);
                        mp_value(m);
                        mu_unpack(m, m->args);
                        return;

        default:        mp_expr(m);
                        if (!m->indirect) mu_unexpected(m);
                        mu_expect(m, MT_SET);
                        mp_value(m);
                        mu_enc(m, MINSERT);
                        mu_enc(m, MDROP);
                        return;
    }
}

static void mp_stmt_assign(mstate_t *m) {
    switch (m->tok) {
        case MT_SET:    if (!m->indirect) mu_unexpected(m);
                        mu_lex(m);
                        mp_value(m);
                        mu_enc(m, MASSIGN);
                        return;

        case MT_OPSET:  if (!m->indirect) mu_unexpected(m);
                        mu_enc(m, MSCOPE);
                        mu_enca(m, MVAR, mu_accvar(m, vcstr("ops")));
                        mu_enc(m, MLOOKUP);
                        mu_enca(m, MVAR, mu_accvar(m, m->val));
                        mu_enc(m, MLOOKUP);
                        mu_enc(m, MTBL);
                        mu_enca(m, MDUP, 3);
                        mu_enca(m, MDUP, 3);
                        mu_enc(m, MLOOKUP);
                        mu_enc(m, MAPPEND);
                        mu_lex(m);
                        mp_value(m);
                        mu_enc(m, MAPPEND);
                        mu_enc(m, MCALL);
                        mu_enc(m, MASSIGN);
                        return;
                        
        default:        if (m->indirect) mu_enc(m, MLOOKUP);
                        mu_enc(m, MDROP);
                        return;
    }
}

static void mp_stmt(mstate_t *m) {
    switch (m->tok) {
        case '{':       mu_lex(m);
                        mp_stmt_list(m);
                        mu_expect(m, '}');
                        return;

        case MT_LET:    mu_lex(m);
                        return mp_stmt_insert(m);

        case MT_IF:     mu_lex(m);
                        return mp_if(m);

        case MT_WHILE:  mu_lex(m);
                        return mp_while(m); 

        case MT_FOR:    mu_lex(m);
                        return mp_for(m);

        case MT_RETURN: mu_lex(m);
                        mp_value(m);
                        mu_enc(m, MRET);
                        return;

        case MT_CONT:   if (!m->j.ctbl) mu_unexpected(m);
                        mu_lex(m);
                        tbl_append(m->j.ctbl, vraw(m->fn->ins), m->eh);
                        mu_enlarge(m, m->jsize);
                        return;

        case MT_BREAK:  if (!m->j.btbl) mu_unexpected(m);
                        mu_lex(m);
                        tbl_append(m->j.btbl, vraw(m->fn->ins), m->eh);
                        mu_enlarge(m, m->jsize);
                        return;

        case MT_FNSET:  mu_lex(m);
                        mu_expect(m, MT_IDENT);
                        mu_enc(m, MSCOPE);
                        mu_enca(m, MVAR, mu_accvar(m, m->val));
                        mp_fn(m);
                        mu_enc(m, MINSERT);
                        mu_enc(m, MDROP);
                        return;

        case MT_SEP:    mu_lex(m);
                        return mp_stmt_list(m);

        case MT_IDENT:
        case MT_FN:
        case MT_LIT:
        case MT_NIL:
        case MT_OP:
        case '[':
        case '(':       mp_expr(m);
                        return mp_stmt_assign(m);

        default:        return;

    }
}

static void mp_stmt_list(mstate_t *m) {
    uint8_t paren = m->paren;
    m->paren = false;

    m->stmt = true;
    m->left = true;
    mp_stmt_entry(m);

    m->paren = paren;
}

static void mp_stmt_entry(mstate_t *m) {
    mp_stmt(m);
    return mp_stmt_follow(m);
}



// Parses Mu source into bytecode
void mu_parse_init(mstate_t *m, var_t code) {
    m->ref = getref(code);
    m->str = code.str;
    m->pos = code.str + code.off;
    m->end = code.str + code.off + code.len;

    m->key = false;
    m->stmt = true;
    m->left = true;
    m->paren = false;
    m->keys = mu_keys();

    m->jsize = mu_sizea(m, MJUMP, 0);
    m->jtsize = mu_sizea(m, MJTRUE, 0);
    m->jfsize = mu_sizea(m, MJFALSE, 0);

    mu_lex(m);
}

void mu_parse_args(mstate_t *m, tbl_t *args) {
    if (args && args->len > 0) {
        mu_enc(m, MARGS);
        mu_unpack(m, args);
    }
}

void mu_parse_top(mstate_t *m) {
    m->j = (struct jstate){0};

    mp_stmt_list(m);

    if (m->tok != 0)
        mu_unexpected(m);

    mu_enc(m, MRETN);
}

void mu_parse_nested(mstate_t *m) {
    m->j = (struct jstate){0};

    mp_stmt(m);

    mu_enc(m, MRETN);
}

