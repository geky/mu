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
static varg_t vaccvar(vstate_t *vs) {
    varg_t arg;
    var_t index = tbl_lookup(vs->vars, vs->val);

    if (var_isnil(index)) {
        arg = vs->vars->len;
        tbl_assign(vs->vars, vs->val, vnum(arg));
    } else {
        arg = (varg_t)var_num(index);
    }

    return arg;
}


static void venc(vstate_t *vs, vop_t op) {
    vs->ins += vs->encode(&vs->bcode[vs->ins], op, 0);
    assert(vs->ins <= VMAXLEN); // TODO errors
}

static void vencarg(vstate_t *vs, vop_t op, varg_t arg) {
    vs->ins += vs->encode(&vs->bcode[vs->ins], op | VOP_ARG, arg);
    assert(vs->ins <= VMAXLEN); // TODO errors
}

static void vencvar(vstate_t *vs) {
    vencarg(vs, VVAR, vaccvar(vs));
}


static int vsize(vstate_t *vs, vop_t op) {
    return vcount(0, op, 0);
}

static int vsizearg(vstate_t *vs, vop_t op, varg_t arg) {
    return vcount(0, op | VOP_ARG, arg);
}

static int vsizevar(vstate_t *vs) {
    return vsizearg(vs, VVAR, vaccvar(vs));
}


static int vinsert(vstate_t *vs, vop_t op, int in) {
    return vs->encode(&vs->bcode[in], op, 0);
}

static int vinsertarg(vstate_t *vs, vop_t op, varg_t arg, int in) {
    return vs->encode(&vs->bcode[in], op | VOP_ARG, arg);
}

static int vinsertvar(vstate_t *vs, int in) {
    return vinsertarg(vs, VVAR, vaccvar(vs), in);
}


static void vpatch(vstate_t *vs) {
    int jsize = vsizearg(vs, VJUMP, 0);

    tbl_for(k, v, vs->j.tbl, {
        vinsertarg(vs, VJUMP, vs->ins - (v.data+jsize), v.data);
    });

    tbl_dec(vs->j.tbl);
    vs->j.tbl = 0;
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
static void vp_expr_op(vstate_t *vs);
static void vp_expr(vstate_t *vs);
static void vp_table(vstate_t *vs);
static void vp_table_assign(vstate_t *vs);
static void vp_table_ident(vstate_t *vs);
static void vp_table_follow(vstate_t *vs);
static void vp_table_entry(vstate_t *vs);
static void vp_stmt_if(vstate_t *vs);
static void vp_stmt_while(vstate_t *vs);
static void vp_stmt_list(vstate_t *vs);
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

    int ifins = vs->ins;
    vs->ins += vsizearg(vs, VJFALSE, 0);
    vp_value(vs);

    if (vs->tok == VT_ELSE) {
        int elins = vs->ins;
        vs->ins += vsizearg(vs, VJUMP, 0);
        vlex(vs);
        vp_value(vs);

        vinsertarg(vs, VJFALSE,
            (elins+vsizearg(vs, VJUMP, 0)) -
            (ifins+vsizearg(vs, VJFALSE, 0)), ifins);
        vinsertarg(vs, VJUMP,
            vs->ins - (elins+vsizearg(vs, VJUMP, 0)), elins);
    } else {
        vencarg(vs, VJUMP, vsize(vs, VNIL));
        venc(vs, VNIL);
        vinsertarg(vs, VJFALSE,
            vs->ins - (ifins+vsizearg(vs, VJFALSE, 0)), ifins);
    }
}

static void vp_expr_while(vstate_t *vs) {
    venc(vs, VTBL);

    struct vjstate j = vs->j;
    vs->j.ins = vs->ins;
    vs->j.tbl = tbl_create(0);

    vexpect(vs, '(');
    vp_value(vs);
    vexpect(vs, ')');

    int whins = vs->ins;
    vs->ins += vsizearg(vs, VJFALSE, 0);
    vp_value(vs);
    venc(vs, VADD);

    vinsertarg(vs, VJFALSE,
        (vs->ins+vsizearg(vs, VJUMP, 0)) -
        (whins+vsizearg(vs, VJFALSE, 0)), whins);
    vencarg(vs, VJUMP,
        vs->j.ins - (vs->ins+vsizearg(vs, VJUMP, 0)));

    vpatch(vs);
    vs->j = j;
}

static void vp_expr_op(vstate_t *vs) {
    switch (vs->tok) {
        case VT_DOT:    if (vs->indirect) venc(vs, VLOOKUP);
                        vlex(vs);
                        if (!vs->tok == VT_IDENT) vunexpected(vs);
                        vencvar(vs);
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
                            venlarge(vs, vs->op.ins, vsizevar(vs)
                                                   + vsize(vs, VTBL));
                            vs->op.ins += vinsertvar(vs, vs->op.ins);
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
                        vencvar(vs);
                        vs->indirect = true;
                        vlex(vs);
                        return vp_expr_op(vs);

        case VT_NIL:    venc(vs, VNIL);
                        vs->indirect = false;
                        vlex(vs);
                        return vp_expr_op(vs);

        case VT_NUM:
        case VT_STR:    vencvar(vs);
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

        case VT_OP:     vencvar(vs);
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

        default:        vunexpected(vs);
    }
}


static void vp_table(vstate_t *vs) {
    vp_table_entry(vs);
    return vp_table_follow(vs);
}

static void vp_table_assign(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SET:    vlex(vs);
                        vp_value(vs);
                        venc(vs, VINSERT);
                        return;

        default:        venc(vs, VADD);
                        return;
    }
}

static void vp_table_ident(vstate_t *vs) {
    int ins = vs->ins;
    vencvar(vs);
    vlex(vs);

    switch (vs->tok) {
        case VT_SET:    return vp_table_assign(vs);

        default:        venlarge(vs, ins, vsize(vs, VSCOPE));
                        vinsert(vs, VSCOPE, ins);
                        vs->indirect = true;
                        vp_expr_op(vs);
                        if (vs->indirect) venc(vs, VLOOKUP);
                        return vp_table_assign(vs);
    }
}

static void vp_table_follow(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SEP:    vlex(vs);
                        return vp_table(vs);

        default:        return;
    }
}

static void vp_table_entry(vstate_t *vs) {
    switch (vs->tok) {
        case VT_IDENT:  return vp_table_ident(vs);

        case VT_NUM:
        case VT_STR:
        case '[':
        case '(':       
        case VT_OP:     vp_value(vs);
                        return vp_table_assign(vs);

        default:        return;
    }
}

static void vp_stmt_if(vstate_t *vs) {
    vexpect(vs, '(');
    vp_value(vs);
    vexpect(vs, ')');

    int ifins = vs->ins;
    vs->ins += vsizearg(vs, VJFALSE, 0);
    vp_stmt(vs); 

    if (vs->tok == VT_ELSE) {
        int elins = vs->ins;
        vs->ins += vsizearg(vs, VJUMP, 0);
        vlex(vs);
        vp_stmt(vs);

        vinsertarg(vs, VJFALSE,
            (elins+vsizearg(vs, VJUMP, 0)) -
            (ifins+vsizearg(vs, VJFALSE, 0)), ifins);
        vinsertarg(vs, VJUMP,
            vs->ins - (elins+vsizearg(vs, VJUMP, 0)), elins);
    } else {
        vinsertarg(vs, VJFALSE,
            vs->ins - (ifins+vsizearg(vs, VJFALSE, 0)), ifins);
    }
}

static void vp_stmt_while(vstate_t *vs) {
    struct vjstate j = vs->j;
    vs->j.ins = vs->ins;
    vs->j.tbl = tbl_create(0);

    vexpect(vs, '(');
    vp_value(vs);
    vexpect(vs, ')');

    int whins = vs->ins;
    vs->ins += vsizearg(vs, VJFALSE, 0);
    vp_stmt(vs);

    vinsertarg(vs, VJFALSE,
        (vs->ins+vsizearg(vs, VJUMP, 0)) -
        (whins+vsizearg(vs, VJFALSE, 0)), whins);
    vencarg(vs, VJUMP,
        vs->j.ins - (vs->ins+vsizearg(vs, VJUMP, 0)));

    if (vs->tok == VT_ELSE) {
        vlex(vs);
        vp_stmt(vs);
    }

    vpatch(vs);
    vs->j = j;
}

static void vp_stmt_list(vstate_t *vs) {
    vp_stmt(vs);
    return vp_stmt_follow(vs);
}

static void vp_stmt_let(vstate_t *vs) {
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

static void vp_stmt_assign(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SET:    if (!vs->indirect) vunexpected(vs);
                        vlex(vs);
                        vp_value(vs);
                        venc(vs, VASSIGN);
                        return;

        case VT_OPSET:  if (!vs->indirect) vunexpected(vs);
                        vencvar(vs);
                        venc(vs, VTBL);
                        vencarg(vs, VDUP, 3);
                        vencarg(vs, VDUP, 3);
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
                        vp_expr(vs);
                        return vp_stmt_let(vs);

        case VT_IF:     vlex(vs);
                        return vp_stmt_if(vs);

        case VT_WHILE:  vlex(vs);
                        return vp_stmt_while(vs); 

        case VT_CONT:   if (!vs->j.tbl) vunexpected(vs);
                        vlex(vs);
                        vencarg(vs, VJUMP,
                            vs->j.ins - (vs->ins + vsizearg(vs, VJUMP, 0)));
                        return;

        case VT_BREAK:  if (!vs->j.tbl) vunexpected(vs);
                        vlex(vs);
                        tbl_add(vs->j.tbl, vraw(vs->ins));
                        vs->ins += vsizearg(vs, VJUMP, 0);
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
    vs->j.tbl = 0;

    vlex(vs);
    vp_stmt_list(vs);

    if (vs->tok != 0)
        vunexpected(vs);

    venc(vs, VRETN | VOP_END);
}

