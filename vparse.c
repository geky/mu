#include "vparse.h"

#include "vlex.h"
#include "tbl.h"

#include <assert.h>
#include <stdio.h>


// Parsing error handling
#define vunexpected(vs) _vunexpected(vs, __FUNCTION__)
__attribute__((noreturn))
static void _vunexpected(vstate_t *vs, const char *fn) {
    printf("\033[31munexpected (%d) in %s\033[0m\n", vs->tok, fn);
    assert(false); // TODO make this throw actual messages
}

// Expect a token or fail
#define vexpect(vs, tok) _vexpect(vs, tok, __FUNCTION__)
static void _vexpect(vstate_t *vs, vtok_t tok, const char *fn) {
    if (vs->tok != tok) {
        printf("\033[31mexpected (%d) not (%d) in %s\033[0m\n", tok, vs->tok, fn);
        assert(false); // TODO errors
    }

    vlex(vs);
}


// Different encoding calls
static varg_t vaccvar(vstate_t *vs, var_t v) {
    var_t index = tbl_lookup(vs->vars, v);

    if (!var_isnil(index))
        return index.data;

    varg_t arg = vs->vars->len;
    tbl_assign(vs->vars, v, vraw(arg));
    return arg;
}


static void venc(vstate_t *vs, vop_t op) {
    vs->ins += vs->encode(&vs->bcode[vs->ins], op, 0);
    assert(vs->ins <= VMAXLEN); // TODO errors
}

static void venca(vstate_t *vs, vop_t op, varg_t arg) {
    vs->ins += vs->encode(&vs->bcode[vs->ins], op | VOP_ARG, arg);
    assert(vs->ins <= VMAXLEN); // TODO errors
}

static int vsize(vstate_t *vs, vop_t op) {
    return vcount(0, op, 0);
}

static int vsizea(vstate_t *vs, vop_t op, varg_t arg) {
    return vcount(0, op | VOP_ARG, arg);
}

static int vinsert(vstate_t *vs, vop_t op, int ins) {
    return vs->encode(&vs->bcode[ins], op, 0);
}

static int vinserta(vstate_t *vs, vop_t op, varg_t arg, int ins) {
    return vs->encode(&vs->bcode[ins], op | VOP_ARG, arg);
}


static void vpatch(vstate_t *vs, tbl_t *jtbl, int ins) {
    tbl_for(k, v, jtbl, {
        vinserta(vs, VJUMP, ins - (v.data+vs->jsize), v.data);
    });

    tbl_dec(jtbl);
}


static void vunpack(vstate_t *vs, tbl_t *map) {
    venc(vs, VSCOPE);

    tbl_for(k, v, map, {
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
    });

    venc(vs, VDROP);
    venc(vs, VDROP);
}


static tbl_t *vrevargs(tbl_t *args) {
    tbl_t *res = tbl_create(0);
    int i;

    for (i = args->len-1; i >= 0; i--) {
        tbl_add(res, tbl_lookup(args, vnum(i)));
    }

    tbl_dec(args);
    return res;
}


static void venlarge(vstate_t *vs, int in, int count) {
    if (vs->bcode) {
        memmove(&vs->bcode[in] + count,
                &vs->bcode[in], 
                vs->ins - in);
    }

    vs->ins += count;
    assert(vs->ins <= VMAXLEN); // TODO errors
}



// Parser for V's grammar
static void vp_value(vstate_t *vs);
static void vp_expr_if(vstate_t *vs);
static void vp_expr_while(vstate_t *vs);
static void vp_expr_for(vstate_t *vs);
static void vp_expr_op(vstate_t *vs);
static void vp_expr(vstate_t *vs);
static void vp_args_entry(vstate_t *vs);
static void vp_args_follow(vstate_t *vs);
static void vp_args(vstate_t *vs);
static void vp_table_assign(vstate_t *vs);
static void vp_table_entry(vstate_t *vs);
static void vp_table_follow(vstate_t *vs);
static void vp_table(vstate_t *vs);
static void vp_stmt_if(vstate_t *vs);
static void vp_stmt_while(vstate_t *vs);
static void vp_stmt_for(vstate_t *vs);
static void vp_stmt_list(vstate_t *vs);
static void vp_stmt_let_op(vstate_t *vs);
static void vp_stmt_let(vstate_t *vs);
static void vp_stmt_assign(vstate_t *vs);
static void vp_stmt_follow(vstate_t *vs);
static void vp_stmt(vstate_t *vs);


static void vp_value(vstate_t *vs) {
    vp_expr(vs);

    if (vs->indirect)
        venc(vs, VLOOKUP);
}

static void vp_expr_if(vstate_t *vs) {
    vexpect(vs, '(');
    vp_value(vs);
    vexpect(vs, ')');

    int i_ins = vs->ins;
    vs->ins += vs->jfsize;
    vp_value(vs);

    if (vs->tok == VT_ELSE) {
        int e_ins = vs->ins;
        vs->ins += vs->jsize;
        vlex(vs);
        vp_value(vs);

        vinserta(vs, VJFALSE, (e_ins+vs->jsize) - (i_ins+vs->jfsize), i_ins);
        vinserta(vs, VJUMP, vs->ins - (e_ins+vs->jsize), e_ins);
    } else {
        venca(vs, VJUMP, vsize(vs, VNIL));
        venc(vs, VNIL);
        vinserta(vs, VJFALSE, vs->ins - (i_ins+vs->jfsize), i_ins);
    }
}

static void vp_expr_while(vstate_t *vs) {
    venc(vs, VTBL);
    int w_ins = vs->ins;

    vexpect(vs, '(');
    vp_value(vs);
    vexpect(vs, ')');

    int j_ins = vs->ins;
    vs->ins += vs->jfsize;
    vp_value(vs);
    venc(vs, VADD);

    vinserta(vs, VJFALSE, (vs->ins+vs->jsize) - (j_ins+vs->jfsize), j_ins);
    venca(vs, VJUMP, w_ins - (vs->ins+vs->jsize));
}

static void vp_expr_for(vstate_t *vs) {
    venc(vs, VTBL);

    vexpect(vs, '(');
    vp_args(vs);
    tbl_t *args = vrevargs(vs->args);
    vexpect(vs, VT_SET);
    vp_value(vs);
    venc(vs, VITER);
    vexpect(vs, ')');

    int f_ins = vs->ins;
    vs->ins += vs->jsize;

    vunpack(vs, args);
    venca(vs, VDUP, 1);
    vp_value(vs);
    venc(vs, VADD);
    venc(vs, VDROP);

    vinserta(vs, VJUMP, vs->ins - (f_ins+vs->jsize), f_ins);

    venca(vs, VDUP, 0);
    venc(vs, VTBL);
    venc(vs, VCALL);
    venca(vs, VDUP, 0);

    venca(vs, VJTRUE, (f_ins+vs->jsize) - (vs->ins+vs->jtsize));
    venc(vs, VDROP);
    venc(vs, VDROP);
}

static void vp_expr_op(vstate_t *vs) {
    switch (vs->tok) {
        case VT_DOT:    if (vs->indirect) venc(vs, VLOOKUP);
                        vlex(vs);
                        if (!vs->tok == VT_IDENT) vunexpected(vs);
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
                        vlex(vs);
                        vp_table(vs);
                        vs->paren--;
                        vexpect(vs, ')');
                        venc(vs, VCALL);
                        vs->indirect = false;
                        return vp_expr_op(vs);

        case VT_OP:     if (vs->op.prec <= vs->nprec) return;
                        if (vs->indirect) venc(vs, VLOOKUP);
                        venc(vs, VADD);
                        {   struct vopstate op = vs->op;
                            varg_t arg = vaccvar(vs, vs->val);
                            venlarge(vs, vs->op.ins, vsizea(vs, VVAR, arg) +
                                                     vsize(vs, VTBL));
                            vs->op.ins += vinserta(vs, VVAR, arg, vs->op.ins);
                            vs->op.ins += vinsert(vs, VTBL, vs->op.ins);
                            vs->op.prec = vs->nprec;
                            vlex(vs);
                            vp_value(vs);
                            vs->op = op;
                        }
                        venc(vs, VADD);
                        venc(vs, VCALL);
                        vs->indirect = false;
                        return vp_expr_op(vs);

        default:        return;
    }
}

static void vp_expr(vstate_t *vs) {
    vs->op.ins = vs->ins;

    switch (vs->tok) {
        case VT_IDENT:  venc(vs, VSCOPE);
                        venca(vs, VVAR, vaccvar(vs, vs->val));
                        vs->indirect = true;
                        vlex(vs);
                        return vp_expr_op(vs);

        case VT_NIL:    venc(vs, VNIL);
                        vs->indirect = false;
                        vlex(vs);
                        return vp_expr_op(vs);

        case VT_NUM:
        case VT_STR:    venca(vs, VVAR, vaccvar(vs, vs->val));
                        vs->indirect = false;
                        vlex(vs);
                        return vp_expr_op(vs);

        case '[':       venc(vs, VTBL);
                        vs->paren++;
                        vlex(vs);
                        vp_table(vs);
                        vs->paren--;
                        vexpect(vs, ']');
                        vs->indirect = false;
                        return vp_expr_op(vs);

        case '(':       vs->paren++;
                        {   struct vopstate op = vs->op;
                            vs->op.prec = -1;
                            vlex(vs);
                            vp_expr(vs);
                            vs->op = op;
                        }
                        vs->paren--;
                        vexpect(vs, ')');
                        return vp_expr_op(vs);

        case VT_OP:     venca(vs, VVAR, vaccvar(vs, vs->val));
                        venc(vs, VTBL);
                        {   struct vopstate op = vs->op;
                            vs->op.prec = vs->nprec;
                            vlex(vs);
                            vp_value(vs);
                            vs->op = op;
                        }
                        venc(vs, VADD);
                        venc(vs, VCALL);
                        vs->indirect = false;
                        return vp_expr_op(vs);

        case VT_IF:     vlex(vs);
                        vp_expr_if(vs);
                        vs->indirect = false;
                        return vp_expr_op(vs);

        case VT_WHILE:  vlex(vs);
                        vp_expr_while(vs);
                        vs->indirect = false;
                        return vp_expr_op(vs); 

        case VT_FOR:    vlex(vs);
                        vp_expr_for(vs);
                        vs->indirect = false;
                        return vp_expr_op(vs);

        default:        vunexpected(vs);
    }
}


static void vp_args_entry(vstate_t *vs) {
    switch (vs->tok) {
        case VT_IDENT:  tbl_add(vs->args, vs->val);
                        vlex(vs);
                        return vp_args_follow(vs);

        case VT_NUM:    tbl_add(vs->args, vs->val);
                        vlex(vs);
                        return vp_args_follow(vs);

        case '[':       {   tbl_t *args = vs->args;
                            vlex(vs);
                            vp_args(vs);
                            tbl_add(args, vtbl(vs->args));
                            vs->args = args;
                        }
                        vexpect(vs, ']');
                        return vp_args_follow(vs);

        default:        return vp_args_follow(vs);
    }
}

static void vp_args_follow(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SEP:    vlex(vs);
                        return vp_args_entry(vs);

        default:        return;
    }
}

static void vp_args(vstate_t *vs) {
    vs->args = tbl_create(0);

    return vp_args_entry(vs);
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
    switch (vs->tok) {
        case VT_IDENT:  {   varg_t arg = vaccvar(vs, vs->val);
                            vlex(vs);
                            if (vs->tok == VT_SET) {
                                venca(vs, VVAR, arg);
                            } else {
                                venc(vs, VSCOPE);
                                venca(vs, VVAR, arg);
                                vs->indirect = true;
                                vp_expr_op(vs);
                                if (vs->indirect) venc(vs, VLOOKUP);
                            }
                        }
                        return vp_table_assign(vs);
                                
        case VT_NUM:
        case VT_STR:
        case '[':
        case '(':       
        case VT_OP:     vp_value(vs);
                        return vp_table_assign(vs);

        default:        return vp_table_follow(vs);
    }
}

static void vp_table_follow(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SEP:    vlex(vs);
                        return vp_table(vs);

        default:        return;
    }
}

static void vp_table(vstate_t *vs) {
    return vp_table_entry(vs);
}


static void vp_stmt_if(vstate_t *vs) {
    vexpect(vs, '(');
    vp_value(vs);
    vexpect(vs, ')');

    int i_ins = vs->ins;
    vs->ins += vs->jfsize;
    vp_stmt(vs);

    if (vs->tok == VT_ELSE) {
        int e_ins = vs->ins;
        vs->ins += vs->jsize;
        vlex(vs);
        vp_stmt(vs);

        vinserta(vs, VJFALSE, (e_ins+vs->jsize) - (i_ins+vs->jfsize), i_ins);
        vinserta(vs, VJUMP, vs->ins - (e_ins+vs->jsize), e_ins);
    } else {
        vinserta(vs, VJFALSE, vs->ins - (i_ins+vs->jfsize), i_ins);
    }
}

static void vp_stmt_while(vstate_t *vs) {
    struct vjstate j = vs->j;
    vs->j.ctbl = tbl_create(0);
    vs->j.btbl = tbl_create(0);
    int w_ins = vs->ins;

    vexpect(vs, '(');
    vp_value(vs);
    vexpect(vs, ')');

    int j_ins = vs->ins;
    vs->ins += vs->jfsize;
    vp_stmt(vs);

    vinserta(vs, VJFALSE, (vs->ins+vs->jsize) - (j_ins+vs->jfsize), j_ins);
    venca(vs, VJUMP, w_ins - (vs->ins+vs->jsize));

    vpatch(vs, vs->j.ctbl, w_ins);
    tbl_t *btbl = vs->j.btbl;
    vs->j = j;

    if (vs->tok == VT_ELSE) {
        vlex(vs);
        vp_stmt(vs);
    }

    vpatch(vs, btbl, vs->ins);
}

static void vp_stmt_for(vstate_t *vs) {
    struct vjstate j = vs->j;
    vs->j.ctbl = tbl_create(0);
    vs->j.btbl = tbl_create(0);

    vexpect(vs, '(');
    vp_args(vs);
    tbl_t *args = vrevargs(vs->args);
    vexpect(vs, VT_SET);
    vp_value(vs);
    venc(vs, VITER);
    vexpect(vs, ')');

    int f_ins = vs->ins;
    vs->ins += vs->jsize;

    vunpack(vs, args);
    vp_stmt(vs);

    vinserta(vs, VJUMP, vs->ins - (f_ins+vs->jsize), f_ins);
    vpatch(vs, vs->j.ctbl, vs->ins);

    venca(vs, VDUP, 0);
    venc(vs, VTBL);
    venc(vs, VCALL);
    venca(vs, VDUP, 0);

    venca(vs, VJTRUE, (f_ins+vs->jsize) - (vs->ins+vs->jtsize));
    venc(vs, VDROP);

    tbl_t *btbl = vs->j.btbl;
    vs->j = j;

    if (vs->tok == VT_ELSE) {
        vlex(vs);
        vp_stmt(vs);
    }

    vpatch(vs, btbl, vs->ins);
    venc(vs, VDROP);
}

static void vp_stmt_list(vstate_t *vs) {
    vp_stmt(vs);
    return vp_stmt_follow(vs);
}

static void vp_stmt_let_op(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SET:    if (!vs->indirect) vunexpected(vs);
                        vlex(vs);
                        vp_value(vs);
                        venc(vs, VINSERT);
                        venc(vs, VDROP);
                        return;

        default:        vunexpected(vs);
    }
}

static void vp_stmt_let(vstate_t *vs) {
    switch (vs->tok) {
        case '[':       vlex(vs);
                        vp_args(vs);
                        vexpect(vs, ']');
                        vexpect(vs, VT_SET);
                        vp_value(vs);
                        vunpack(vs, vs->args);
                        return;

        default:        vp_expr(vs);
                        return vp_stmt_let_op(vs);
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
                        venca(vs, VVAR, vaccvar(vs, vs->val));
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

static void vp_stmt_follow(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SEP:    vlex(vs);
                        return vp_stmt_list(vs);

        default:        return;
    }
}

static void vp_stmt(vstate_t *vs) {
    switch (vs->tok) {
        case '{':       vlex(vs);
                        vp_stmt_list(vs);
                        vexpect(vs, '}');
                        return;

        case VT_RETURN: vlex(vs);
                        vp_value(vs);
                        venc(vs, VRET);
                        return;

        case VT_LET:    vlex(vs);
                        return vp_stmt_let(vs);

        case VT_IF:     vlex(vs);
                        return vp_stmt_if(vs);

        case VT_WHILE:  vlex(vs);
                        return vp_stmt_while(vs); 

        case VT_FOR:    vlex(vs);
                        return vp_stmt_for(vs);

        case VT_CONT:   if (!vs->j.ctbl) vunexpected(vs);
                        vlex(vs);
                        tbl_add(vs->j.ctbl, vraw(vs->ins));
                        vs->ins += vs->jsize;
                        return;

        case VT_BREAK:  if (!vs->j.btbl) vunexpected(vs);
                        vlex(vs);
                        tbl_add(vs->j.btbl, vraw(vs->ins));
                        vs->ins += vs->jsize;
                        return;

        case VT_IDENT:
        case VT_NUM:
        case VT_STR:
        case '[':  
        case '(': 
        case VT_OP:     vp_expr(vs);
                        return vp_stmt_assign(vs);

        default:        return;
    }
}



// Parses V source code and evaluates the result
void vparse(vstate_t *vs) {
    vs->ins = 0;
    vs->paren = 0;
    vs->op.prec = -1;
    vs->j = (struct vjstate){0};

    vs->jsize = vsizea(vs, VJUMP, 0);
    vs->jtsize = vsizea(vs, VJTRUE, 0);
    vs->jfsize = vsizea(vs, VJFALSE, 0);

    vlex(vs);
    vp_stmt_list(vs);

    if (vs->tok != 0)
        vunexpected(vs);

    venc(vs, VRETN);
}

