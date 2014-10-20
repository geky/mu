#include "vparse.h"

#include "vlex.h"
#include "tbl.h"

#include <stdio.h>


// Parsing error handling
#define vunexpected(vs) _vunexpected(vs, __FUNCTION__)
__attribute__((noreturn))
static void _vunexpected(vstate_t *vs, const char *fn) {
    printf("\033[31munexpected (%d) in %s\033[0m\n", vs->tok, fn);
    err_parse(vs->eh);
}

// Expect a token or fail
#define vexpect(vs, tok) _vexpect(vs, tok, __FUNCTION__)
static void _vexpect(vstate_t *vs, vtok_t tok, const char *fn) {
    if (vs->tok != tok) {
        printf("\033[31mexpected (%d) not (%d) in %s\033[0m\n", tok, vs->tok, fn);
        err_parse(vs->eh);
    }

    vlex(vs);
}


// Allocation functions for managing bytecode space
static void venlarge(vstate_t *vs, int count) {
    struct vfnstate *fn = vs->fn;
    fn->ins += count;

    while (fn->ins > fn->len) {
        if (((int)fn->len << 1) > VMAXLEN)
            err_len(vs->eh);

        fn->len <<= 1;
    }
        
    fn->bcode = v_realloc(fn->bcode, fn->len, fn->len << 1, vs->eh);
}

static void venlargein(vstate_t *vs, int count, int ins) {
    venlarge(vs, count);

    memmove(&vs->fn->bcode[ins] + count,
            &vs->fn->bcode[ins], 
            vs->fn->ins-count - ins);
}


// Different encoding calls
static void venc(vstate_t *vs, vop_t op) {
    int count = vcount(op, 0);
    venlarge(vs, count);
    vencode(&vs->fn->bcode[vs->fn->ins-count], op, 0);
}

static void venca(vstate_t *vs, vop_t op, varg_t arg) {
    int count = vcount(op | VOP_ARG, arg);
    venlarge(vs, count);
    vencode(&vs->fn->bcode[vs->fn->ins-count], op | VOP_ARG, arg);
}

__attribute__((const))
static int vsize(vstate_t *vs, vop_t op) {
    return vcount(op, 0);
}

__attribute__((const))
static int vsizea(vstate_t *vs, vop_t op, varg_t arg) {
    return vcount(op | VOP_ARG, arg);
}

static int vinsert(vstate_t *vs, vop_t op, int ins) {
    vencode(&vs->fn->bcode[ins], op, 0);
    return vcount(op, 0);
}

static int vinserta(vstate_t *vs, vop_t op, varg_t arg, int ins) {
    vencode(&vs->fn->bcode[ins], op | VOP_ARG, arg);
    return vcount(VOP_ARG, arg);
}


// Helping functions for code generation
static varg_t vaccvar(vstate_t *vs, var_t v) {
    var_t index = tbl_lookup(vs->fn->vars, v);

    if (!var_isnil(index))
        return index.data;

    varg_t arg = vs->fn->vars->len;
    tbl_assign(vs->fn->vars, v, vraw(arg), vs->eh);
    return arg;
}

static void vpatch(vstate_t *vs, tbl_t *jtbl, int ins) {
    tbl_for_begin (k, v, jtbl) {
        vinserta(vs, VJUMP, ins - (v.data+vs->jsize), v.data);
    } tbl_for_end;

    tbl_dec(jtbl);
}

static tbl_t *vrevargs(tbl_t *args, veh_t *eh) {
    tbl_t *res = tbl_create(0, eh);
    int i;

    for (i = args->len-1; i >= 0; i--) {
        tbl_add(res, tbl_lookup(args, vnum(i)), eh);
    }

    tbl_dec(args);
    return res;
}

static void vunpack(vstate_t *vs, tbl_t *map) {
    venc(vs, VSCOPE);

    tbl_for_begin (k, v, map) {
        if (v.type == TYPE_TBL) {
            venca(vs, VDUP, 1);
            venc(vs, VNIL);
            venca(vs, VLOOKDN, var_num(k));
            vunpack(vs, v.tbl);
        } else {
            venca(vs, VVAR, vaccvar(vs, v));
            venca(vs, VDUP, 2);
            venca(vs, VDUP, 1);
            venca(vs, VLOOKDN, var_num(k));
            venc(vs, VINSERT);
        }
    } tbl_for_end;

    venc(vs, VDROP);
    venc(vs, VDROP);
}



// Rules for V's grammar
static void vp_value(vstate_t *vs);
static void vp_phrase(vstate_t *vs);
static void vp_fn(vstate_t *vs);
static void vp_if(vstate_t *vs);
static void vp_while(vstate_t *vs);
static void vp_for(vstate_t *vs);
static void vp_expr_op(vstate_t *vs);
static void vp_expr(vstate_t *vs);
static void vp_args_follow(vstate_t *vs);
static void vp_args_entry(vstate_t *vs);
static void vp_args(vstate_t *vs);
static void vp_table_follow(vstate_t *vs);
static void vp_table_assign(vstate_t *vs);
static void vp_table_entry(vstate_t *vs);
static void vp_table(vstate_t *vs);
static void vp_stmt_follow(vstate_t *vs);
static void vp_stmt_insert(vstate_t *vs);
static void vp_stmt_assign(vstate_t *vs);
static void vp_stmt(vstate_t *vs);
static void vp_stmt_list(vstate_t *vs);
static void vp_stmt_entry(vstate_t *vs);


static void vp_value(vstate_t *vs) {
    struct vopstate op = vs->op;
    vs->op.lprec = -1;

    if (vs->stmt) {
        vs->stmt = false;
        vs->left = false;
        vp_expr(vs);
        vs->stmt = true;
        vs->left = true;
    } else if (vs->left) {
        vs->left = false;
        vp_expr(vs);
        vs->left = true;
    } else {
        vp_expr(vs);
    }

    vs->op = op;

    if (vs->indirect)
        venc(vs, VLOOKUP);
}

static void vp_phrase(vstate_t *vs) {
    if (vs->stmt)
        vp_stmt(vs);
    else
        vp_value(vs);
}


static void vp_fn(vstate_t *vs) {
    vexpect(vs, '(');
    vp_args(vs);
    tbl_t *args = vs->args;
    vexpect(vs, ')');

    struct vfnstate *f = vs->fn;
    struct vjstate j = vs->j;

    fn_t *fn = fn_create_nested(args, vs, vs->eh);
    tbl_add(f->fns, vraw((uint32_t)fn), vs->eh);

    vs->j = j;
    vs->fn = f;

    venca(vs, VFN, f->fns->len-1);
}

static void vp_if(vstate_t *vs) {
    vexpect(vs, '(');
    vp_value(vs);
    vexpect(vs, ')');

    int i_ins = vs->fn->ins;
    venlarge(vs, vs->jfsize);
    vp_phrase(vs);

    if (vs->tok == VT_ELSE) {
        int e_ins = vs->fn->ins;
        venlarge(vs, vs->jsize);
        vlex(vs);
        vp_phrase(vs);

        vinserta(vs, VJFALSE, (e_ins+vs->jsize) - (i_ins+vs->jfsize), i_ins);
        vinserta(vs, VJUMP, vs->fn->ins - (e_ins+vs->jsize), e_ins);
    } else {
        if (!vs->stmt) {
            venca(vs, VJUMP, vsize(vs, VNIL));
            venc(vs, VNIL);
        }
        vinserta(vs, VJFALSE, vs->fn->ins - (i_ins+vs->jfsize), i_ins);
    }
}

static void vp_while(vstate_t *vs) {
    if (!vs->stmt) venc(vs, VTBL);
    struct vjstate j = vs->j;
    vs->j.ctbl = tbl_create(0, vs->eh);
    vs->j.btbl = tbl_create(0, vs->eh);
    int w_ins = vs->fn->ins;

    vexpect(vs, '(');
    vp_value(vs);
    vexpect(vs, ')');

    int j_ins = vs->fn->ins;
    venlarge(vs, vs->jfsize);
    vp_phrase(vs);
    if (!vs->stmt) venc(vs, VADD);

    vinserta(vs, VJFALSE, (vs->fn->ins+vs->jsize) - (j_ins+vs->jfsize), j_ins);
    venca(vs, VJUMP, w_ins - (vs->fn->ins+vs->jsize));

    vpatch(vs, vs->j.ctbl, w_ins);
    tbl_t *btbl = vs->j.btbl;
    vs->j = j;

    if (vs->stmt && vs->tok == VT_ELSE) {
        vlex(vs);
        vp_stmt(vs);
    }

    vpatch(vs, btbl, vs->fn->ins);
}

static void vp_for(vstate_t *vs) {
    if (!vs->stmt) venc(vs, VTBL);

    struct vjstate j = vs->j;
    vs->j.ctbl = tbl_create(0, vs->eh);
    vs->j.btbl = tbl_create(0, vs->eh);

    vexpect(vs, '(');
    vp_args(vs);
    tbl_t *args = vrevargs(vs->args, vs->eh);
    vexpect(vs, VT_SET);
    vp_value(vs);
    venc(vs, VITER);
    vexpect(vs, ')');

    int f_ins = vs->fn->ins;
    venlarge(vs, vs->jsize);

    vunpack(vs, args);
    if (vs->stmt) {
        vp_phrase(vs);
    } else {
        venca(vs, VDUP, 1);
        vp_phrase(vs);
        venc(vs, VADD);
        venc(vs, VDROP);
    }

    vinserta(vs, VJUMP, vs->fn->ins - (f_ins+vs->jsize), f_ins);
    vpatch(vs, vs->j.ctbl, vs->fn->ins);

    venca(vs, VDUP, 0);
    venc(vs, VTBL);
    venc(vs, VCALL);
    venca(vs, VDUP, 0);

    venca(vs, VJTRUE, (f_ins+vs->jsize) - (vs->fn->ins+vs->jtsize));
    venc(vs, VDROP);

    tbl_t *btbl = vs->j.btbl;
    vs->j = j;

    if (vs->stmt && vs->tok == VT_ELSE) {
        vlex(vs);
        vp_stmt(vs);
    }

    vpatch(vs, btbl, vs->fn->ins);
    venc(vs, VDROP);
}


static void vp_expr_op(vstate_t *vs) {
    switch (vs->tok) {
        case VT_KEY:    if (vs->indirect) venc(vs, VLOOKUP);
                        vlex(vs);
                        venca(vs, VVAR, vaccvar(vs, vs->val));
                        vlex(vs);
                        vs->indirect = true;
                        return vp_expr_op(vs);

        case '[':       if (vs->indirect) venc(vs, VLOOKUP);
                        vs->paren++;
                        vlex(vs);
                        vp_value(vs);
                        vs->paren--;
                        vexpect(vs, ']');
                        vs->indirect = true;
                        return vp_expr_op(vs);

        case '(':       if (vs->indirect) venc(vs, VLOOKUP);
                        venc(vs, VTBL);
                        vs->paren++;
                        vp_table(vs);
                        vs->paren--;
                        vexpect(vs, ')');
                        venc(vs, VCALL);
                        vs->indirect = false;
                        return vp_expr_op(vs);

        case VT_OP:     if (vs->op.lprec <= vs->op.rprec) return;
                        if (vs->indirect) venc(vs, VLOOKUP);
                        venc(vs, VADD);
                        {   struct vopstate op = vs->op;
                            varg_t tblarg = vaccvar(vs, vcstr("ops"));
                            varg_t symarg = vaccvar(vs, vs->val);
                            venlargein(vs, vsize(vs, VSCOPE) +
                                           vsizea(vs, VVAR, tblarg) +
                                           vsize(vs, VLOOKUP) +
                                           vsizea(vs, VVAR, symarg) +
                                           vsize(vs, VLOOKUP) +
                                           vsize(vs, VTBL), vs->op.ins);
                            vs->op.ins += vinsert(vs, VSCOPE, vs->op.ins);
                            vs->op.ins += vinserta(vs, VVAR, tblarg, vs->op.ins);
                            vs->op.ins += vinsert(vs, VLOOKUP, vs->op.ins);
                            vs->op.ins += vinserta(vs, VVAR, symarg, vs->op.ins);
                            vs->op.ins += vinsert(vs, VLOOKUP, vs->op.ins);
                            vs->op.ins += vinsert(vs, VTBL, vs->op.ins);
                            vs->op.lprec = vs->op.rprec;
                            vlex(vs);
                            vp_expr(vs);
                            vs->op = op;
                        }
                        if (vs->indirect) venc(vs, VLOOKUP);
                        venc(vs, VADD);
                        venc(vs, VCALL);
                        vs->indirect = false;
                        return vp_expr_op(vs);

        case VT_AND:    if (vs->op.lprec <= vs->op.rprec) return;
                        if (vs->indirect) venc(vs, VLOOKUP);
                        venca(vs, VDUP, 0);
                        {   int a_ins = vs->fn->ins;
                            struct vopstate op = vs->op;
                            venlarge(vs, vs->jfsize);
                            venc(vs, VDROP);
                            vs->op.lprec = vs->op.rprec;
                            vlex(vs);
                            vp_expr(vs);
                            if (vs->indirect) venc(vs, VLOOKUP);
                            vs->op = op;
                            vinserta(vs, VJFALSE, 
                                     vs->fn->ins - (a_ins+vs->jfsize), a_ins);
                        }
                        vs->indirect = false;
                        return vp_expr_op(vs);
                            
        case VT_OR:     if (vs->op.lprec <= vs->op.rprec) return;
                        if (vs->indirect) venc(vs, VLOOKUP);
                        venca(vs, VDUP, 0);
                        {   int o_ins = vs->fn->ins;
                            struct vopstate op = vs->op;
                            venlarge(vs, vs->jtsize);
                            venc(vs, VDROP);
                            vs->op.lprec = vs->op.rprec;
                            vlex(vs);
                            vp_expr(vs);
                            if (vs->indirect) venc(vs, VLOOKUP);
                            vs->op = op;
                            vinserta(vs, VJTRUE, 
                                     vs->fn->ins - (o_ins+vs->jtsize), o_ins);
                        }
                        vs->indirect = false;
                        return vp_expr_op(vs);

        default:        return;
    }
}

static void vp_expr(vstate_t *vs) {
    vs->op.ins = vs->fn->ins;

    switch (vs->tok) {
        case VT_IDENT:  venc(vs, VSCOPE);
                        venca(vs, VVAR, vaccvar(vs, vs->val));
                        vs->indirect = true;
                        vlex(vs);
                        return vp_expr_op(vs);

        case VT_NIL:    venc(vs, VSCOPE);
                        venc(vs, VNIL);
                        vs->indirect = true;
                        vlex(vs);
                        return vp_expr_op(vs);

        case VT_LIT:    venca(vs, VVAR, vaccvar(vs, vs->val));
                        vs->indirect = false;
                        vlex(vs);
                        return vp_expr_op(vs);

        case '[':       venc(vs, VTBL);
                        vp_table(vs);
                        vexpect(vs, ']');
                        vs->indirect = false;
                        return vp_expr_op(vs);

        case '(':       vs->paren++;
                        vlex(vs);
                        vp_value(vs);
                        vs->paren--;
                        vexpect(vs, ')');
                        return vp_expr_op(vs);

        case VT_OP:     venc(vs, VSCOPE);
                        venca(vs, VVAR, vaccvar(vs, vcstr("ops")));
                        venc(vs, VLOOKUP);
                        venca(vs, VVAR, vaccvar(vs, vs->val));
                        venc(vs, VLOOKUP);
                        venc(vs, VTBL);
                        vlex(vs);
                        vp_value(vs);
                        venc(vs, VADD);
                        venc(vs, VCALL);
                        vs->indirect = false;
                        return vp_expr_op(vs);

        case VT_IF:     vlex(vs);
                        vp_if(vs);
                        vs->indirect = false;
                        return vp_expr_op(vs);

        case VT_WHILE:  vlex(vs);
                        vp_while(vs);
                        vs->indirect = false;
                        return vp_expr_op(vs); 

        case VT_FOR:    vlex(vs);
                        vp_for(vs);
                        vs->indirect = false;
                        return vp_expr_op(vs);

        case VT_FN:     vlex(vs);
                        vp_fn(vs);
                        vs->indirect = false;
                        return vp_expr_op(vs);

        default:        vunexpected(vs);
    }
}


static void vp_args_follow(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SEP:    vlex(vs);
                        return vp_args_entry(vs);

        default:        return;
    }
}

static void vp_args_entry(vstate_t *vs) {
    switch (vs->tok) {
        case VT_FN:
        case VT_IF:
        case VT_WHILE:
        case VT_FOR:
        case VT_IDENT:  
        case VT_NIL:
        case VT_LIT:    tbl_add(vs->args, vs->val, vs->eh);
                        vlex(vs);
                        return vp_args_follow(vs);

        case '[':       {   tbl_t *args = vs->args;
                            vlex(vs);
                            vp_args(vs);
                            tbl_add(args, vtbl(vs->args), vs->eh);
                            vs->args = args;
                        }
                        vexpect(vs, ']');
                        return vp_args_follow(vs);

        default:        return vp_args_follow(vs);
    }
}

static void vp_args(vstate_t *vs) {
    vs->args = tbl_create(0, vs->eh);

    return vp_args_entry(vs);
}


static void vp_table_follow(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SEP:    return vp_table(vs);

        default:        return;
    }
}

static void vp_table_assign(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SET:    vlex(vs);
                        vp_value(vs);
                        venc(vs, VINSERT);
                        return vp_table_follow(vs);

        default:        venc(vs, VADD);
                        return vp_table_follow(vs);
    }
}
        
static void vp_table_entry(vstate_t *vs) {
    vs->key = true;
    vlex(vs);
    vs->key = false;

    switch (vs->tok) {
        case VT_IDSET:  venca(vs, VVAR, vaccvar(vs, vs->val));
                        vlex(vs);
                        vexpect(vs, VT_SET);
                        vp_value(vs);
                        venc(vs, VINSERT);
                        return vp_table_follow(vs);

        case VT_FNSET:  vlex(vs);
                        vexpect(vs, VT_IDENT);
                        venca(vs, VVAR, vaccvar(vs, vs->val));
                        vp_fn(vs);
                        venc(vs, VINSERT);
                        return vp_table_follow(vs);

        case VT_SEP:    return vp_table(vs);

        case VT_IDENT:
        case VT_NIL:
        case VT_FN:
        case VT_IF:
        case VT_WHILE:
        case VT_FOR:
        case VT_LIT:
        case VT_OP:
        case '[':
        case '(':       {   struct vopstate op = vs->op;
                            vs->op.lprec = -1;
                            vp_expr(vs);
                            vs->op = op;
                        }
                        if (vs->indirect) venc(vs, VLOOKUP);
                        return vp_table_assign(vs);

        default:        return;
    }
}

static void vp_table(vstate_t *vs) {
    uint8_t paren = vs->paren;
    vs->paren = false;

    vs->left = true;
    vp_table_entry(vs);

    vs->paren = paren;
}


static void vp_stmt_follow(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SEP:    vlex(vs);
                        return vp_stmt_entry(vs);

        default:        return;
    }
}

static void vp_stmt_insert(vstate_t *vs) {
    switch (vs->tok) {
        case '[':       vlex(vs);
                        vp_args(vs);
                        vexpect(vs, ']');
                        vexpect(vs, VT_SET);
                        vp_value(vs);
                        vunpack(vs, vs->args);
                        return;

        default:        vp_expr(vs);
                        if (!vs->indirect) vunexpected(vs);
                        vexpect(vs, VT_SET);
                        vp_value(vs);
                        venc(vs, VINSERT);
                        venc(vs, VDROP);
                        return;
    }
}

static void vp_stmt_assign(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SET:    if (!vs->indirect) vunexpected(vs);
                        vlex(vs);
                        vp_value(vs);
                        venc(vs, VASSIGN);
                        return;

        case VT_OPSET:  if (!vs->indirect) vunexpected(vs);
                        venc(vs, VSCOPE);
                        venca(vs, VVAR, vaccvar(vs, vcstr("ops")));
                        venc(vs, VLOOKUP);
                        venca(vs, VVAR, vaccvar(vs, vs->val));
                        venc(vs, VLOOKUP);
                        venc(vs, VTBL);
                        venca(vs, VDUP, 3);
                        venca(vs, VDUP, 3);
                        venc(vs, VLOOKUP);
                        venc(vs, VADD);
                        vlex(vs);
                        vp_value(vs);
                        venc(vs, VADD);
                        venc(vs, VCALL);
                        venc(vs, VASSIGN);
                        return;
                        
        default:        if (vs->indirect) venc(vs, VLOOKUP);
                        venc(vs, VDROP);
                        return;
    }
}

static void vp_stmt(vstate_t *vs) {
    switch (vs->tok) {
        case '{':       vlex(vs);
                        vp_stmt_list(vs);
                        vexpect(vs, '}');
                        return;

        case VT_LET:    vlex(vs);
                        return vp_stmt_insert(vs);

        case VT_IF:     vlex(vs);
                        return vp_if(vs);

        case VT_WHILE:  vlex(vs);
                        return vp_while(vs); 

        case VT_FOR:    vlex(vs);
                        return vp_for(vs);

        case VT_RETURN: vlex(vs);
                        vp_value(vs);
                        venc(vs, VRET);
                        return;

        case VT_CONT:   if (!vs->j.ctbl) vunexpected(vs);
                        vlex(vs);
                        tbl_add(vs->j.ctbl, vraw(vs->fn->ins), vs->eh);
                        venlarge(vs, vs->jsize);
                        return;

        case VT_BREAK:  if (!vs->j.btbl) vunexpected(vs);
                        vlex(vs);
                        tbl_add(vs->j.btbl, vraw(vs->fn->ins), vs->eh);
                        venlarge(vs, vs->jsize);
                        return;

        case VT_FNSET:  vlex(vs);
                        vexpect(vs, VT_IDENT);
                        venc(vs, VSCOPE);
                        venca(vs, VVAR, vaccvar(vs, vs->val));
                        vp_fn(vs);
                        venc(vs, VINSERT);
                        venc(vs, VDROP);
                        return;

        case VT_SEP:    vlex(vs);
                        return vp_stmt_list(vs);

        case VT_IDENT:
        case VT_FN:
        case VT_LIT:
        case VT_NIL:
        case VT_OP:
        case '[':
        case '(':       vp_expr(vs);
                        return vp_stmt_assign(vs);

        default:        return;

    }
}

static void vp_stmt_list(vstate_t *vs) {
    uint8_t paren = vs->paren;
    vs->paren = false;

    vs->stmt = true;
    vs->left = true;
    vp_stmt_entry(vs);

    vs->paren = paren;
}

static void vp_stmt_entry(vstate_t *vs) {
    vp_stmt(vs);
    return vp_stmt_follow(vs);
}



// Parses V source into bytecode
void vparse_init(vstate_t *vs, var_t code) {
    vs->ref = var_ref(code);
    vs->str = code.str;
    vs->pos = code.str + code.off;
    vs->end = code.str + code.off + code.len;

    vs->key = false;
    vs->stmt = true;
    vs->left = true;
    vs->paren = false;
    vs->keys = vkeys();

    vs->jsize = vsizea(vs, VJUMP, 0);
    vs->jtsize = vsizea(vs, VJTRUE, 0);
    vs->jfsize = vsizea(vs, VJFALSE, 0);

    vlex(vs);
}

void vparse_args(vstate_t *vs, tbl_t *args) {
    if (args && args->len > 0) {
        venc(vs, VARGS);
        vunpack(vs, args);
    }
}

void vparse_top(vstate_t *vs) {
    vs->j = (struct vjstate){0};

    vp_stmt_list(vs);

    if (vs->tok != 0)
        vunexpected(vs);

    venc(vs, VRETN);
}

void vparse_nested(vstate_t *vs) {
    vs->j = (struct vjstate){0};

    vp_stmt(vs);

    venc(vs, VRETN);
}

