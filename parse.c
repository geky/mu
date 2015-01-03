#include "parse.h"

#include "vm.h"
#include "lex.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"

#include <stdio.h>
#include <string.h>


// Parsing error handling
#define unexpected(p) _unexpected(p, __FUNCTION__)
static mu_noreturn void _unexpected(parse_t *p, const char *fn) {
//    printf("\033[31munexpected (%d) in %s\033[0m\n", p->tok, fn);
    err_parse(p->eh);
}

// Expect a token or fail
#define expect(p, tok) _expect(p, tok, __FUNCTION__)
static void _expect(parse_t *p, tok_t tok, const char *fn) {
    if (p->tok != tok) {
//        printf("\033[31mexpected (%d) not (%d) in %s\033[0m\n", tok, p->tok, fn);
        err_parse(p->eh);
    }
}


// Allocation functions for managing bytecode space
static void enlarge(parse_t *p, size_t count) {
    struct fnparse *f = p->f;
    uint_t ins = f->ins + count;
    uint_t len = f->bcode->len;

    while (ins > len) {
        len = len << 1;
    }

    if (len > MU_MAXLEN)
        err_len(p->eh);

    mstr_t *bcode = mstr_create(len, p->eh);
    memcpy(bcode->data, f->bcode->data, len);
    str_dec(f->bcode);

    f->ins = ins;
    f->bcode = bcode;
}

static void enlargein(parse_t *p, size_t count, uint_t ins) {
    enlarge(p, count);

    memmove(&p->f->bcode->data[ins + count],
            &p->f->bcode->data[ins], 
            (p->f->ins - count) - ins);
}


// Different encoding calls
static mu_const size_t size(op_t op) {
    return mu_size(op << 3, 0);
}

static mu_const size_t sizea(op_t op, uint_t arg) {
    return mu_size((op << 3) | MU_ARG, arg);
}

static void encode(parse_t *p, op_t op) {
    size_t count = mu_size(op << 3, 0);
    enlarge(p, count);
    mu_encode(&p->f->bcode->data[p->f->ins - count], op << 3, 0);
}

static void encodea(parse_t *p, op_t op, uint_t arg) {
    size_t count = mu_size((op << 3) | MU_ARG, arg);
    enlarge(p, count);
    mu_encode(&p->f->bcode->data[p->f->ins - count], (op << 3) | MU_ARG, arg);
}

static len_t insert(parse_t *p, op_t op, uint_t ins) {
    mu_encode(&p->f->bcode->data[ins], op << 3, 0);
    return mu_size(op << 3, 0);
}

static len_t inserta(parse_t *p, op_t op, uint_t arg, uint_t ins) {
    mu_encode(&p->f->bcode->data[ins], (op << 3) | MU_ARG, arg);
    return mu_size((op << 3) | MU_ARG, arg);
}


// Helping functions for code generation
static uint_t accvar(parse_t *p, mu_t v) {
    mu_t index = tbl_lookup(p->f->imms, v);

    if (!isnil(index))
        return getuint(index);

    uint_t arg = tbl_getlen(p->f->imms);
    tbl_assign(p->f->imms, v, muint(arg), p->eh);
    return arg;
}

static void patch(parse_t *p, tbl_t *jtbl, uint_t ins) {
    tbl_for_begin (k, v, jtbl) {
        uint_t jump = getuint(v);
        inserta(p, OP_JUMP, (ins - (jump + p->jsize)), jump);
    } tbl_for_end;

    tbl_dec(jtbl);
}

static tbl_t *revargs(tbl_t *args, eh_t *eh) {
    tbl_t *res = tbl_create(0, eh);
    int_t i;

    for (i = args->len-1; i >= 0; i--) {
        tbl_append(res, tbl_lookup(args, mint(i)), eh);
    }

    tbl_dec(args);
    return res;
}

static void unpack(parse_t *p, tbl_t *map) {
    encode(p, OP_SCOPE);

    tbl_for_begin (k, v, map) {
        if (istbl(v)) {
            encodea(p, OP_DUP, 1);
            encode(p, OP_NIL);
            encodea(p, OP_LOOKDN, getuint(k));
            unpack(p, gettbl(v));
        } else {
            encodea(p, OP_IMM, accvar(p, v));
            encodea(p, OP_DUP, 2);
            encodea(p, OP_DUP, 1);
            encodea(p, OP_LOOKDN, getuint(k));
            encode(p, OP_INSERT);
        }
    } tbl_for_end;

    encode(p, OP_DROP);
    encode(p, OP_DROP);
}



// Rules for Mu's grammar
static void p_value(parse_t *p);
static void p_phrase(parse_t *p);
static void p_fn(parse_t *p);
static void p_if(parse_t *p);
static void p_while(parse_t *p);
static void p_for(parse_t *p);
static void p_expr_op(parse_t *p);
static void p_expr(parse_t *p);
static void p_args_follow(parse_t *p);
static void p_args_entry(parse_t *p);
static void p_args(parse_t *p);
static void p_table_follow(parse_t *p);
static void p_table_assign(parse_t *p);
static void p_table_entry(parse_t *p);
static void p_table(parse_t *p);
static void p_stmt_follow(parse_t *p);
static void p_stmt_insert(parse_t *p);
static void p_stmt_assign(parse_t *p);
static void p_stmt(parse_t *p);
static void p_stmt_list(parse_t *p);
static void p_stmt_entry(parse_t *p);


static void p_value(parse_t *p) {
    struct opparse op = p->op;
    p->op.lprec = -1;

    if (p->stmt) {
        p->stmt = false;
        p->left = false;
        mu_lex(p);
        p_expr(p);
        p->stmt = true;
        p->left = true;
    } else if (p->left) {
        p->left = false;
        mu_lex(p);
        p_expr(p);
        p->left = true;
    } else {
        mu_lex(p);
        p_expr(p);
    }

    p->op = op;

    if (p->indirect)
        encode(p, OP_LOOKUP);
}

static void p_phrase(parse_t *p) {
    if (p->stmt)
        p_stmt(p);
    else
        p_value(p);
}


static void p_fn(parse_t *p) {
    mu_lex(p);
    expect(p, '(');
    p_args(p);
    expect(p, ')');

    tbl_t *args = p->args;
    struct fnparse *f = p->f;
    struct jparse j = p->j;

    fn_t *nested = fn_create_nested(args, p, p->eh);

    p->j = j;
    p->f = f;

    encodea(p, OP_FN, accvar(p, mfn(nested)));
}

static void p_if(parse_t *p) {
    mu_lex(p);
    expect(p, '(');
    p_value(p);
    expect(p, ')');

    uint_t i_ins = p->f->ins;
    enlarge(p, p->jfsize);
    p_phrase(p);

    if (p->tok == T_ELSE) {
        uint_t e_ins = p->f->ins;
        enlarge(p, p->jsize);
        p_phrase(p);

        inserta(p, OP_JFALSE, (e_ins+p->jsize) - (i_ins+p->jfsize), i_ins);
        inserta(p, OP_JUMP, p->f->ins - (e_ins+p->jsize), e_ins);
    } else {
        if (!p->stmt) {
            encodea(p, OP_JUMP, size(OP_NIL));
            encode(p, OP_NIL);
        }
        inserta(p, OP_JFALSE, p->f->ins - (i_ins+p->jfsize), i_ins);
    }
}

static void p_while(parse_t *p) {
    if (!p->stmt) encode(p, OP_TBL);
    struct jparse j = p->j;
    p->j.ctbl = tbl_create(0, p->eh);
    p->j.btbl = tbl_create(0, p->eh);
    uint_t w_ins = p->f->ins;

    mu_lex(p);
    expect(p, '(');
    p_value(p);
    expect(p, ')');

    uint_t j_ins = p->f->ins;
    enlarge(p, p->jfsize);
    p_phrase(p);
    if (!p->stmt) 
        encode(p, OP_APPEND);

    inserta(p, OP_JFALSE, (p->f->ins+p->jsize) - (j_ins+p->jfsize), j_ins);
    encodea(p, OP_JUMP, w_ins - (p->f->ins+p->jsize));

    patch(p, p->j.ctbl, w_ins);
    tbl_t *btbl = p->j.btbl;
    p->j = j;

    if (p->stmt && p->tok == T_ELSE)
        p_stmt(p);

    patch(p, btbl, p->f->ins);
}

static void p_for(parse_t *p) {
    if (!p->stmt) encode(p, OP_TBL);

    struct jparse j = p->j;
    p->j.ctbl = tbl_create(0, p->eh);
    p->j.btbl = tbl_create(0, p->eh);
    p->left = true;

    mu_lex(p);
    expect(p, '(');
    p_args(p);
    tbl_t *args = revargs(p->args, p->eh);
    expect(p, T_SET);
    p_value(p);
    encode(p, OP_ITER);
    expect(p, ')');

    uint_t f_ins = p->f->ins;
    enlarge(p, p->jsize);

    unpack(p, args);
    if (p->stmt) {
        p_phrase(p);
    } else {
        encodea(p, OP_DUP, 1);
        p_phrase(p);
        encode(p, OP_APPEND);
        encode(p, OP_DROP);
    }

    inserta(p, OP_JUMP, p->f->ins - (f_ins+p->jsize), f_ins);
    patch(p, p->j.ctbl, p->f->ins);

    encodea(p, OP_DUP, 0);
    encode(p, OP_TBL);
    encode(p, OP_CALL);
    encodea(p, OP_DUP, 0);

    encodea(p, OP_JTRUE, (f_ins+p->jsize) - (p->f->ins+p->jtsize));
    encode(p, OP_DROP);

    tbl_t *btbl = p->j.btbl;
    p->j = j;

    if (p->stmt && p->tok == T_ELSE)
        p_stmt(p);

    patch(p, btbl, p->f->ins);
    encode(p, OP_DROP);
}


static void p_expr_op(parse_t *p) {
    switch (p->tok) {
        case T_KEY:     if (p->indirect) encode(p, OP_LOOKUP);
                        mu_lex(p);
                        encodea(p, OP_IMM, accvar(p, p->val));
                        mu_lex(p);
                        p->indirect = true;
                        return p_expr_op(p);

        case '[':       if (p->indirect) encode(p, OP_LOOKUP);
                        p->paren++;
                        p_value(p);
                        p->paren--;
                        expect(p, ']');
                        mu_lex(p);
                        p->indirect = true;
                        return p_expr_op(p);

        case '(':       if (p->indirect) encode(p, OP_LOOKUP);
                        encode(p, OP_TBL);
                        p->paren++;
                        p_table(p);
                        p->paren--;
                        expect(p, ')');
                        encode(p, OP_CALL);
                        mu_lex(p);
                        p->indirect = false;
                        return p_expr_op(p);

        case T_OP:      if (p->op.lprec <= p->op.rprec) return;
                        if (p->indirect) encode(p, OP_LOOKUP);
                        encode(p, OP_APPEND);
                        {   struct opparse op = p->op;
                            uint_t tblarg = accvar(p, mcstr("ops", p->eh));
                            uint_t symarg = accvar(p, p->val);
                            enlargein(p, size(OP_SCOPE) +
                                         sizea(OP_IMM, tblarg) +
                                         size(OP_LOOKUP) +
                                         sizea(OP_IMM, symarg) +
                                         size(OP_LOOKUP) +
                                         size(OP_TBL), p->op.ins);
                            p->op.ins += insert(p, OP_SCOPE, p->op.ins);
                            p->op.ins += inserta(p, OP_IMM, tblarg, p->op.ins);
                            p->op.ins += insert(p, OP_LOOKUP, p->op.ins);
                            p->op.ins += inserta(p, OP_IMM, symarg, p->op.ins);
                            p->op.ins += insert(p, OP_LOOKUP, p->op.ins);
                            p->op.ins += insert(p, OP_TBL, p->op.ins);
                            p->op.lprec = p->op.rprec;
                            mu_lex(p);
                            p_expr(p);
                            p->op = op;
                        }
                        if (p->indirect) encode(p, OP_LOOKUP);
                        encode(p, OP_APPEND);
                        encode(p, OP_CALL);
                        p->indirect = false;
                        return p_expr_op(p);

        case T_AND:     if (p->op.lprec <= p->op.rprec) return;
                        if (p->indirect) encode(p, OP_LOOKUP);
                        encodea(p, OP_DUP, 0);
                        {   uint_t a_ins = p->f->ins;
                            struct opparse op = p->op;
                            enlarge(p, p->jfsize);
                            encode(p, OP_DROP);
                            p->op.lprec = p->op.rprec;
                            mu_lex(p);
                            p_expr(p);
                            if (p->indirect) encode(p, OP_LOOKUP);
                            p->op = op;
                            inserta(p, OP_JFALSE, 
                                    p->f->ins - (a_ins+p->jfsize), a_ins);
                        }
                        p->indirect = false;
                        return p_expr_op(p);
                            
        case T_OR:      if (p->op.lprec <= p->op.rprec) return;
                        if (p->indirect) encode(p, OP_LOOKUP);
                        encodea(p, OP_DUP, 0);
                        {   uint_t o_ins = p->f->ins;
                            struct opparse op = p->op;
                            enlarge(p, p->jtsize);
                            encode(p, OP_DROP);
                            p->op.lprec = p->op.rprec;
                            mu_lex(p);
                            p_expr(p);
                            if (p->indirect) encode(p, OP_LOOKUP);
                            p->op = op;
                            inserta(p, OP_JTRUE, 
                                    p->f->ins - (o_ins+p->jtsize), o_ins);
                        }
                        p->indirect = false;
                        return p_expr_op(p);

        default:        return;
    }
}

static void p_expr(parse_t *p) {
    p->op.ins = p->f->ins;

    switch (p->tok) {
        case T_IDENT:   encode(p, OP_SCOPE);
                        encodea(p, OP_IMM, accvar(p, p->val));
                        mu_lex(p);
                        p->indirect = true;
                        return p_expr_op(p);

        case T_NIL:     encode(p, OP_SCOPE);
                        encode(p, OP_NIL);
                        mu_lex(p);
                        p->indirect = true;
                        return p_expr_op(p);

        case T_LIT:     encodea(p, OP_IMM, accvar(p, p->val));
                        mu_lex(p);
                        p->indirect = false;
                        return p_expr_op(p);

        case '[':       encode(p, OP_TBL);
                        p_table(p);
                        expect(p, ']');
                        mu_lex(p);
                        p->indirect = false;
                        return p_expr_op(p);

        case '(':       p->paren++;
                        p_value(p);
                        p->paren--;
                        expect(p, ')');
                        mu_lex(p);
                        return p_expr_op(p);

        case T_OP:      encode(p, OP_SCOPE);
                        encodea(p, OP_IMM, accvar(p, mcstr("ops", p->eh)));
                        encode(p, OP_LOOKUP);
                        encodea(p, OP_IMM, accvar(p, p->val));
                        encode(p, OP_LOOKUP);
                        encode(p, OP_TBL);
                        p_value(p);
                        encode(p, OP_APPEND);
                        encode(p, OP_CALL);
                        p->indirect = false;
                        return p_expr_op(p);

        case T_IF:      p_if(p);
                        p->indirect = false;
                        return p_expr_op(p);

        case T_WHILE:   p_while(p);
                        p->indirect = false;
                        return p_expr_op(p); 

        case T_FOR:     p_for(p);
                        p->indirect = false;
                        return p_expr_op(p);

        case T_FN:      p_fn(p);
                        p->indirect = false;
                        return p_expr_op(p);

        default:        unexpected(p);
    }
}


static void p_args_follow(parse_t *p) {
    switch (p->tok) {
        case T_SEP:     return p_args_entry(p);

        default:        return;
    }
}

static void p_args_entry(parse_t *p) {
    mu_lex(p);

    switch (p->tok) {
        case T_FN:
        case T_IF:
        case T_WHILE:
        case T_FOR:
        case T_IDENT:  
        case T_NIL:
        case T_LIT:     tbl_append(p->args, p->val, p->eh);
                        mu_lex(p);
                        return p_args_follow(p);

        case '[':       {   tbl_t *args = p->args;
                            p_args(p);
                            tbl_append(args, mtbl(p->args), p->eh);
                            p->args = args;
                        }
                        expect(p, ']');
                        mu_lex(p);
                        return p_args_follow(p);

        default:        return p_args_follow(p);
    }
}

static void p_args(parse_t *p) {
    p->args = tbl_create(0, p->eh);

    return p_args_entry(p);
}


static void p_table_follow(parse_t *p) {
    switch (p->tok) {
        case T_SEP:     return p_table(p);

        default:        return;
    }
}

static void p_table_assign(parse_t *p) {
    switch (p->tok) {
        case T_SET:     p_value(p);
                        encode(p, OP_INSERT);
                        return p_table_follow(p);

        default:        encode(p, OP_APPEND);
                        return p_table_follow(p);
    }
}
        
static void p_table_entry(parse_t *p) {
    p->key = true;
    mu_lex(p);
    p->key = false;

    switch (p->tok) {
        case T_IDSET:   encodea(p, OP_IMM, accvar(p, p->val));
                        mu_lex(p);
                        expect(p, T_SET);
                        p_value(p);
                        encode(p, OP_INSERT);
                        return p_table_follow(p);

        case T_FNSET:   mu_lex(p);
                        expect(p, T_IDENT);
                        encodea(p, OP_IMM, accvar(p, p->val));
                        p_fn(p);
                        encode(p, OP_INSERT);
                        return p_table_follow(p);

        case T_SEP:     return p_table(p);

        case T_IDENT:
        case T_NIL:
        case T_FN:
        case T_IF:
        case T_WHILE:
        case T_FOR:
        case T_LIT:
        case T_OP:
        case '[':
        case '(':       {   struct opparse op = p->op;
                            p->op.lprec = -1;
                            p_expr(p);
                            p->op = op;
                        }
                        if (p->indirect) encode(p, OP_LOOKUP);
                        return p_table_assign(p);

        default:        return;
    }
}

static void p_table(parse_t *p) {
    uintq_t paren = p->paren;
    p->paren = false;

    p->left = true;
    p_table_entry(p);

    p->paren = paren;
}


static void p_stmt_follow(parse_t *p) {
    switch (p->tok) {
        case T_SEP:     return p_stmt_entry(p);

        default:        return;
    }
}

static void p_stmt_insert(parse_t *p) {
    mu_lex(p);

    switch (p->tok) {
        case '[':       p_args(p);
                        expect(p, ']');
                        mu_lex(p);
                        expect(p, T_SET);
                        p_value(p);
                        unpack(p, p->args);
                        return;

        default:        p_expr(p);
                        if (!p->indirect) unexpected(p);
                        expect(p, T_SET);
                        p_value(p);
                        encode(p, OP_INSERT);
                        encode(p, OP_DROP);
                        return;
    }
}

static void p_stmt_assign(parse_t *p) {
    switch (p->tok) {
        case T_SET:     if (!p->indirect) unexpected(p);
                        p_value(p);
                        encode(p, OP_ASSIGN);
                        return;

        case T_OPSET:   if (!p->indirect) unexpected(p);
                        encode(p, OP_SCOPE);
                        encodea(p, OP_IMM, accvar(p, mcstr("ops", p->eh)));
                        encode(p, OP_LOOKUP);
                        encodea(p, OP_IMM, accvar(p, p->val));
                        encode(p, OP_LOOKUP);
                        encode(p, OP_TBL);
                        encodea(p, OP_DUP, 3);
                        encodea(p, OP_DUP, 3);
                        encode(p, OP_LOOKUP);
                        encode(p, OP_APPEND);
                        p_value(p);
                        encode(p, OP_APPEND);
                        encode(p, OP_CALL);
                        encode(p, OP_ASSIGN);
                        return;
                        
        default:        if (p->indirect) encode(p, OP_LOOKUP);
                        encode(p, OP_DROP);
                        return;
    }
}

static void p_stmt(parse_t *p) {
    mu_lex(p);

    switch (p->tok) {
        case '{':       p_stmt_list(p);
                        expect(p, '}');
                        mu_lex(p);
                        return;

        case T_LET:     return p_stmt_insert(p);
        case T_IF:      return p_if(p);
        case T_WHILE:   return p_while(p); 
        case T_FOR:     return p_for(p);

        case T_RETURN:  p_value(p);
                        encode(p, OP_RET);
                        return;

        case T_CONT:    if (!p->j.ctbl) unexpected(p);
                        tbl_append(p->j.ctbl, muint(p->f->ins), p->eh);
                        enlarge(p, p->jsize);
                        mu_lex(p);
                        return;

        case T_BREAK:   if (!p->j.btbl) unexpected(p);
                        tbl_append(p->j.btbl, muint(p->f->ins), p->eh);
                        enlarge(p, p->jsize);
                        mu_lex(p);
                        return;

        case T_FNSET:   mu_lex(p);
                        expect(p, T_IDENT);
                        encode(p, OP_SCOPE);
                        encodea(p, OP_IMM, accvar(p, p->val));
                        p_fn(p);
                        encode(p, OP_INSERT);
                        encode(p, OP_DROP);
                        return;

        case T_SEP:     return p_stmt_list(p);

        case T_IDENT:
        case T_FN:
        case T_LIT:
        case T_NIL:
        case T_OP:
        case '[':
        case '(':       p_expr(p);
                        return p_stmt_assign(p);

        default:        return;

    }
}

static void p_stmt_list(parse_t *p) {
    uintq_t paren = p->paren;
    p->paren = false;

    p->stmt = true;
    p->left = true;
    p_stmt_entry(p);

    p->paren = paren;
}

static void p_stmt_entry(parse_t *p) {
    p_stmt(p);
    return p_stmt_follow(p);
}



// Parses Mu source into bytecode
parse_t *mu_parse_create(mu_t code, eh_t *eh) {
    parse_t *p = mu_alloc(sizeof(parse_t), eh);

    p->ref = getref(code);
    p->str = getdata(code);
    p->pos = getdata(code);
    p->end = getdata(code) + getlen(code);

    p->key = false;
    p->paren = false;
    p->keys = mu_keys();

    p->jsize = sizea(OP_JUMP, 0);
    p->jtsize = sizea(OP_JTRUE, 0);
    p->jfsize = sizea(OP_JFALSE, 0);

    p->eh = eh;

    return p;
}

void mu_parse_destroy(parse_t *p) {
    mu_dealloc(p, sizeof(parse_t));
}

void mu_parse_args(parse_t *p, tbl_t *args) {
    if (args && tbl_getlen(args) > 0) {
        encode(p, OP_ARGS);
        unpack(p, args);
    }
}

void mu_parse_stmts(parse_t *p) {
    p->j = (struct jparse){0};
    p->op.lprec = -1;
    p->stmt = true;
    p->left = true;

    p_stmt_list(p);

    encode(p, OP_RETN);
}

void mu_parse_stmt(parse_t *p) {
    p->j = (struct jparse){0};
    p->op.lprec = -1;
    p->stmt = true;
    p->left = true;

    p_stmt(p);

    encode(p, OP_RETN);
}

void mu_parse_expr(parse_t *p) {
    p->j = (struct jparse){0};
    p->op.lprec = -1;
    p->stmt = false;
    p->left = false;
    p->paren = true;

    p_value(p);

    encode(p, OP_RET);
}

void mu_parse_end(parse_t *p) {
    if (p->tok != T_END)
        unexpected(p);
}

