#include "parse.h"

#include "vm.h"
#include "lex.h"
#include "var.h"
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
static void enlarge(parse_t *p, int count) {
    struct fnparse *fn = p->fn;
    fn->ins += count;

    while (fn->ins > fn->len) {
        if (((int)fn->len << 1) > MU_MAXLEN)
            err_len(p->eh);

        fn->len <<= 1;
    }
        
    fn->bcode = mu_realloc(fn->bcode, fn->len, fn->len << 1, p->eh);
}

static void enlargein(parse_t *p, int count, int ins) {
    enlarge(p, count);

    memmove(&p->fn->bcode[ins] + count,
            &p->fn->bcode[ins], 
            p->fn->ins-count - ins);
}


// Different encoding calls
static mu_const int size(op_t op) {
    return mu_size(op << 3, 0);
}

static mu_const int sizea(op_t op, arg_t arg) {
    return mu_size((op << 3) | MU_ARG, arg);
}

static void encode(parse_t *p, op_t op) {
    int count = mu_size(op << 3, 0);
    enlarge(p, count);
    mu_encode(&p->fn->bcode[p->fn->ins-count], op << 3, 0);
}

static void encodea(parse_t *p, op_t op, arg_t arg) {
    int count = mu_size((op << 3) | MU_ARG, arg);
    enlarge(p, count);
    mu_encode(&p->fn->bcode[p->fn->ins-count], (op << 3) | MU_ARG, arg);
}

static int insert(parse_t *p, op_t op, int ins) {
    mu_encode(&p->fn->bcode[ins], op << 3, 0);
    return mu_size(op << 3, 0);
}

static int inserta(parse_t *p, op_t op, arg_t arg, int ins) {
    mu_encode(&p->fn->bcode[ins], (op << 3) | MU_ARG, arg);
    return mu_size((op << 3) | MU_ARG, arg);
}


// Helping functions for code generation
static arg_t accvar(parse_t *p, var_t v) {
    var_t index = tbl_lookup(p->fn->vars, v);

    if (!isnil(index))
        return getraw(index);

    arg_t arg = p->fn->vars->len;
    tbl_assign(p->fn->vars, v, vraw(arg), p->eh);
    return arg;
}

static void patch(parse_t *p, tbl_t *jtbl, int ins) {
    tbl_for_begin (k, v, jtbl) {
        inserta(p, OP_JUMP, ins - (getraw(v)+p->jsize), getraw(v));
    } tbl_for_end;

    tbl_dec(jtbl);
}

static tbl_t *revargs(tbl_t *args, eh_t *eh) {
    tbl_t *res = tbl_create(0, eh);
    int i;

    for (i = args->len-1; i >= 0; i--) {
        tbl_append(res, tbl_lookup(args, vnum(i)), eh);
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
            encodea(p, OP_LOOKDN, getnum(k));
            unpack(p, gettbl(v));
        } else {
            encodea(p, OP_VAR, accvar(p, v));
            encodea(p, OP_DUP, 2);
            encodea(p, OP_DUP, 1);
            encodea(p, OP_LOOKDN, getnum(k));
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
        lex(p);
        p_expr(p);
        p->stmt = true;
        p->left = true;
    } else if (p->left) {
        p->left = false;
        lex(p);
        p_expr(p);
        p->left = true;
    } else {
        lex(p);
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
    lex(p);
    expect(p, '(');
    p_args(p);
    expect(p, ')');

    tbl_t *args = p->args;
    struct fnparse *f = p->fn;
    struct jparse j = p->j;

    fn_t *fn = fn_create_nested(args, p, p->eh);
    tbl_append(f->fns, vraw((uint32_t)fn), p->eh);

    p->j = j;
    p->fn = f;

    encodea(p, OP_FN, f->fns->len-1);
}

static void p_if(parse_t *p) {
    lex(p);
    expect(p, '(');
    p_value(p);
    expect(p, ')');

    int i_ins = p->fn->ins;
    enlarge(p, p->jfsize);
    p_phrase(p);

    if (p->tok == T_ELSE) {
        int e_ins = p->fn->ins;
        enlarge(p, p->jsize);
        p_phrase(p);

        inserta(p, OP_JFALSE, (e_ins+p->jsize) - (i_ins+p->jfsize), i_ins);
        inserta(p, OP_JUMP, p->fn->ins - (e_ins+p->jsize), e_ins);
    } else {
        if (!p->stmt) {
            encodea(p, OP_JUMP, size(OP_NIL));
            encode(p, OP_NIL);
        }
        inserta(p, OP_JFALSE, p->fn->ins - (i_ins+p->jfsize), i_ins);
    }
}

static void p_while(parse_t *p) {
    if (!p->stmt) encode(p, OP_TBL);
    struct jparse j = p->j;
    p->j.ctbl = tbl_create(0, p->eh);
    p->j.btbl = tbl_create(0, p->eh);
    int w_ins = p->fn->ins;

    lex(p);
    expect(p, '(');
    p_value(p);
    expect(p, ')');

    int j_ins = p->fn->ins;
    enlarge(p, p->jfsize);
    p_phrase(p);
    if (!p->stmt) 
        encode(p, OP_APPEND);

    inserta(p, OP_JFALSE, (p->fn->ins+p->jsize) - (j_ins+p->jfsize), j_ins);
    encodea(p, OP_JUMP, w_ins - (p->fn->ins+p->jsize));

    patch(p, p->j.ctbl, w_ins);
    tbl_t *btbl = p->j.btbl;
    p->j = j;

    if (p->stmt && p->tok == T_ELSE)
        p_stmt(p);

    patch(p, btbl, p->fn->ins);
}

static void p_for(parse_t *p) {
    if (!p->stmt) encode(p, OP_TBL);

    struct jparse j = p->j;
    p->j.ctbl = tbl_create(0, p->eh);
    p->j.btbl = tbl_create(0, p->eh);
    p->left = true;

    lex(p);
    expect(p, '(');
    p_args(p);
    tbl_t *args = revargs(p->args, p->eh);
    expect(p, T_SET);
    p_value(p);
    encode(p, OP_ITER);
    expect(p, ')');

    int f_ins = p->fn->ins;
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

    inserta(p, OP_JUMP, p->fn->ins - (f_ins+p->jsize), f_ins);
    patch(p, p->j.ctbl, p->fn->ins);

    encodea(p, OP_DUP, 0);
    encode(p, OP_TBL);
    encode(p, OP_CALL);
    encodea(p, OP_DUP, 0);

    encodea(p, OP_JTRUE, (f_ins+p->jsize) - (p->fn->ins+p->jtsize));
    encode(p, OP_DROP);

    tbl_t *btbl = p->j.btbl;
    p->j = j;

    if (p->stmt && p->tok == T_ELSE)
        p_stmt(p);

    patch(p, btbl, p->fn->ins);
    encode(p, OP_DROP);
}


static void p_expr_op(parse_t *p) {
    switch (p->tok) {
        case T_KEY:     if (p->indirect) encode(p, OP_LOOKUP);
                        lex(p);
                        encodea(p, OP_VAR, accvar(p, p->val));
                        lex(p);
                        p->indirect = true;
                        return p_expr_op(p);

        case '[':       if (p->indirect) encode(p, OP_LOOKUP);
                        p->paren++;
                        p_value(p);
                        p->paren--;
                        expect(p, ']');
                        lex(p);
                        p->indirect = true;
                        return p_expr_op(p);

        case '(':       if (p->indirect) encode(p, OP_LOOKUP);
                        encode(p, OP_TBL);
                        p->paren++;
                        p_table(p);
                        p->paren--;
                        expect(p, ')');
                        encode(p, OP_CALL);
                        lex(p);
                        p->indirect = false;
                        return p_expr_op(p);

        case T_OP:      if (p->op.lprec <= p->op.rprec) return;
                        if (p->indirect) encode(p, OP_LOOKUP);
                        encode(p, OP_APPEND);
                        {   struct opparse op = p->op;
                            arg_t tblarg = accvar(p, vcstr("ops"));
                            arg_t symarg = accvar(p, p->val);
                            enlargein(p, size(OP_SCOPE) +
                                         sizea(OP_VAR, tblarg) +
                                         size(OP_LOOKUP) +
                                         sizea(OP_VAR, symarg) +
                                         size(OP_LOOKUP) +
                                         size(OP_TBL), p->op.ins);
                            p->op.ins += insert(p, OP_SCOPE, p->op.ins);
                            p->op.ins += inserta(p, OP_VAR, tblarg, p->op.ins);
                            p->op.ins += insert(p, OP_LOOKUP, p->op.ins);
                            p->op.ins += inserta(p, OP_VAR, symarg, p->op.ins);
                            p->op.ins += insert(p, OP_LOOKUP, p->op.ins);
                            p->op.ins += insert(p, OP_TBL, p->op.ins);
                            p->op.lprec = p->op.rprec;
                            lex(p);
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
                        {   int a_ins = p->fn->ins;
                            struct opparse op = p->op;
                            enlarge(p, p->jfsize);
                            encode(p, OP_DROP);
                            p->op.lprec = p->op.rprec;
                            lex(p);
                            p_expr(p);
                            if (p->indirect) encode(p, OP_LOOKUP);
                            p->op = op;
                            inserta(p, OP_JFALSE, 
                                     p->fn->ins - (a_ins+p->jfsize), a_ins);
                        }
                        p->indirect = false;
                        return p_expr_op(p);
                            
        case T_OR:      if (p->op.lprec <= p->op.rprec) return;
                        if (p->indirect) encode(p, OP_LOOKUP);
                        encodea(p, OP_DUP, 0);
                        {   int o_ins = p->fn->ins;
                            struct opparse op = p->op;
                            enlarge(p, p->jtsize);
                            encode(p, OP_DROP);
                            p->op.lprec = p->op.rprec;
                            lex(p);
                            p_expr(p);
                            if (p->indirect) encode(p, OP_LOOKUP);
                            p->op = op;
                            inserta(p, OP_JTRUE, 
                                     p->fn->ins - (o_ins+p->jtsize), o_ins);
                        }
                        p->indirect = false;
                        return p_expr_op(p);

        default:        return;
    }
}

static void p_expr(parse_t *p) {
    p->op.ins = p->fn->ins;

    switch (p->tok) {
        case T_IDENT:   encode(p, OP_SCOPE);
                        encodea(p, OP_VAR, accvar(p, p->val));
                        lex(p);
                        p->indirect = true;
                        return p_expr_op(p);

        case T_NIL:     encode(p, OP_SCOPE);
                        encode(p, OP_NIL);
                        lex(p);
                        p->indirect = true;
                        return p_expr_op(p);

        case T_LIT:     encodea(p, OP_VAR, accvar(p, p->val));
                        lex(p);
                        p->indirect = false;
                        return p_expr_op(p);

        case '[':       encode(p, OP_TBL);
                        p_table(p);
                        expect(p, ']');
                        lex(p);
                        p->indirect = false;
                        return p_expr_op(p);

        case '(':       p->paren++;
                        p_value(p);
                        p->paren--;
                        expect(p, ')');
                        lex(p);
                        return p_expr_op(p);

        case T_OP:      encode(p, OP_SCOPE);
                        encodea(p, OP_VAR, accvar(p, vcstr("ops")));
                        encode(p, OP_LOOKUP);
                        encodea(p, OP_VAR, accvar(p, p->val));
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
    lex(p);

    switch (p->tok) {
        case T_FN:
        case T_IF:
        case T_WHILE:
        case T_FOR:
        case T_IDENT:  
        case T_NIL:
        case T_LIT:     tbl_append(p->args, p->val, p->eh);
                        lex(p);
                        return p_args_follow(p);

        case '[':       {   tbl_t *args = p->args;
                            p_args(p);
                            tbl_append(args, vtbl(p->args), p->eh);
                            p->args = args;
                        }
                        expect(p, ']');
                        lex(p);
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
    lex(p);
    p->key = false;

    switch (p->tok) {
        case T_IDSET:   encodea(p, OP_VAR, accvar(p, p->val));
                        lex(p);
                        expect(p, T_SET);
                        p_value(p);
                        encode(p, OP_INSERT);
                        return p_table_follow(p);

        case T_FNSET:   lex(p);
                        expect(p, T_IDENT);
                        encodea(p, OP_VAR, accvar(p, p->val));
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
    uint8_t paren = p->paren;
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
    lex(p);

    switch (p->tok) {
        case '[':       p_args(p);
                        expect(p, ']');
                        lex(p);
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
                        encodea(p, OP_VAR, accvar(p, vcstr("ops")));
                        encode(p, OP_LOOKUP);
                        encodea(p, OP_VAR, accvar(p, p->val));
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
    lex(p);

    switch (p->tok) {
        case '{':       p_stmt_list(p);
                        expect(p, '}');
                        lex(p);
                        return;

        case T_LET:     return p_stmt_insert(p);
        case T_IF:      return p_if(p);
        case T_WHILE:   return p_while(p); 
        case T_FOR:     return p_for(p);

        case T_RETURN:  p_value(p);
                        encode(p, OP_RET);
                        return;

        case T_CONT:    if (!p->j.ctbl) unexpected(p);
                        tbl_append(p->j.ctbl, vraw(p->fn->ins), p->eh);
                        enlarge(p, p->jsize);
                        lex(p);
                        return;

        case T_BREAK:   if (!p->j.btbl) unexpected(p);
                        tbl_append(p->j.btbl, vraw(p->fn->ins), p->eh);
                        enlarge(p, p->jsize);
                        lex(p);
                        return;

        case T_FNSET:   lex(p);
                        expect(p, T_IDENT);
                        encode(p, OP_SCOPE);
                        encodea(p, OP_VAR, accvar(p, p->val));
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
    uint8_t paren = p->paren;
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
parse_t *parse_create(var_t code, eh_t *eh) {
    parse_t *p = mu_alloc(sizeof(parse_t), eh);

    p->ref = getref(code);
    p->str = getstart(code);
    p->pos = getstr(code);
    p->end = getend(code);

    p->key = false;
    p->paren = false;
    p->keys = mu_keys();

    p->jsize = sizea(OP_JUMP, 0);
    p->jtsize = sizea(OP_JTRUE, 0);
    p->jfsize = sizea(OP_JFALSE, 0);

    p->eh = eh;

    return p;
}

void parse_destroy(parse_t *p) {
    mu_dealloc(p, sizeof(parse_t));
}

void parse_args(parse_t *p, tbl_t *args) {
    if (args && tbl_len(args) > 0) {
        encode(p, OP_ARGS);
        unpack(p, args);
    }
}

void parse_stmts(parse_t *p) {
    p->j = (struct jparse){0};
    p->op.lprec = -1;
    p->stmt = true;
    p->left = true;

    p_stmt_list(p);

    encode(p, OP_RETN);
}

void parse_stmt(parse_t *p) {
    p->j = (struct jparse){0};
    p->op.lprec = -1;
    p->stmt = true;
    p->left = true;

    p_stmt(p);

    encode(p, OP_RETN);
}

void parse_expr(parse_t *p) {
    p->j = (struct jparse){0};
    p->op.lprec = -1;
    p->stmt = false;
    p->left = false;
    p->paren = true;

    p_value(p);

    encode(p, OP_RET);
}

void parse_end(parse_t *p) {
    if (p->tok != T_END)
        unexpected(p);
}

