#include "vparse.h"

#include "vlex.h"
#include "tbl.h"

#include <assert.h>
#include <stdio.h>


// Macro for accepting the current token and continuing
static inline struct vstate *vnext(struct vstate *vs) {
    vs->tok = vlex(vs);
    return vs;
}

// Parsing error handling
#define vunexpected(vs) _vunexpected(vs, __FUNCTION__)
__attribute__((noreturn))
static void _vunexpected(struct vstate *vs, const char *fn) {
    printf("unexpected '%c' (%02x) in %s\n", vs->tok, vs->tok, fn);
    assert(false); // TODO make this throw actual messages
}

// Expect a token or fail
static void vexpect(struct vstate *vs, int tok) {
    if (vs->tok != tok) {
        printf("expected '%c' (%02x) not '%c' (%x)\n", tok, tok, vs->tok, vs->tok);
        assert(vlex(vs) == tok); // TODO
    }

    vnext(vs);
}


// Different encoding calls
static uint16_t vaccvar(struct vstate *vs) {
    uint16_t arg;
    var_t index = tbl_lookup(vs->vars, vs->val);

    if (var_isnil(index)) {
        arg = vs->vars->len;
        tbl_assign(vs->vars, vs->val, vnum(arg));
    } else {
        arg = (uint16_t)var_num(index);
    }

    return arg;
}


static void venlarge(struct vstate *vs, int in, int count) {
    if (vs->bcode) {
        memmove(&vs->bcode[in] + count,
                &vs->bcode[in], 
                vs->ins - in);
    }
}


static void venc(struct vstate *vs, enum vop op) {
    vs->ins += vs->encode(&vs->bcode[vs->ins], op, 0);
}

static void vencarg(struct vstate *vs, enum vop op, uint16_t arg) {
    vs->ins += vs->encode(&vs->bcode[vs->ins], op | VOP_ARG, arg);
}

static void vencvar(struct vstate *vs) {
    vencarg(vs, VVAR, vaccvar(vs));
}


static int vsized(struct vstate *vs, enum vop op) {
    return vcount(0, op, 0);
}

static int vsizearg(struct vstate *vs, enum vop op, uint16_t arg) {
    return vcount(0, op | VOP_ARG, arg);
}

static int vsizevar(struct vstate *vs) {
    return vsizearg(vs, VVAR, vaccvar(vs));
}


static void vinsert(struct vstate *vs, enum vop op, int *in) {
    int count = vs->encode(&vs->bcode[*in], op, 0);
    vs->ins += count;
    *in += count;
}

static void vinsertarg(struct vstate *vs, enum vop op, uint16_t arg, int *in) {
    int count = vs->encode(&vs->bcode[*in], op | VOP_ARG, arg);
    vs->ins += count;
    *in += count;
}

static void vinsertvar(struct vstate *vs, int *in) {
    vinsertarg(vs, VVAR, vaccvar(vs), in);
}



// Parser for V's grammar
static void vp_value(struct vstate *vs);
static void vp_primarydot(struct vstate *vs);
static void vp_primaryop(struct vstate *vs);
static void vp_primary(struct vstate *vs);
static void vp_tabassign(struct vstate *vs);
static void vp_tabident(struct vstate *vs);
static void vp_tabfollow(struct vstate *vs);
static void vp_table(struct vstate *vs);
static void vp_explet(struct vstate *vs);
static void vp_expassign(struct vstate *vs);
static void vp_expfollow(struct vstate *vs);
static void vp_expression(struct vstate *vs);


static void vp_value(struct vstate *vs) {
    vp_primary(vs);

    if (vs->indirect)
        venc(vs, VLOOKUP);
}


static void vp_primarydot(struct vstate *vs) {
    switch (vs->tok) {
        case VT_IDENT:  vencvar(vs);
                        vs->indirect = true;
                        return vp_primaryop(vnext(vs));

        default:        vunexpected(vs);
    }
}

static void vp_primaryop(struct vstate *vs) {
    switch (vs->tok) {
        case '.':       if (vs->indirect) venc(vs, VLOOKUP);
                        return vp_primarydot(vnext(vs));

        case '[':       if (vs->indirect) venc(vs, VLOOKUP);
                        vs->paren++;
                        vp_value(vnext(vs));
                        vs->paren--;
                        vexpect(vs, ']');
                        vs->indirect = true;
                        return vp_primaryop(vs);

        case '(':       if (vs->indirect) venc(vs, VLOOKUP);
                        venc(vs, VTBL);
                        vs->paren++;
                        vp_table(vnext(vs));
                        vs->paren--;
                        vexpect(vs, ')');
                        venc(vs, VCALL);
                        vs->indirect = false;
                        return vp_primaryop(vs);

        case VT_OP:     if (vs->prec <= vs->nprec) return;
                        venc(vs, VADD);
                        {   int opins = vs->opins;
                            uint8_t prec = vs->prec;
                            venlarge(vs, vs->opins, vsizevar(vs)
                                                  + vsized(vs, VTBL));
                            vinsertvar(vs, &vs->opins);
                            vinsert(vs, VTBL, &vs->opins);
                            vs->prec = vs->nprec;
                            vp_value(vnext(vs));
                            vs->opins = opins;
                            vs->prec = prec;
                        }
                        venc(vs, VADD);
                        venc(vs, VCALL);
                        return vp_primaryop(vs);

        default:        return;
    }
}

static void vp_primary(struct vstate *vs) {
    switch (vs->tok) {
        case VT_IDENT:  vs->opins = vs->ins;
                        venc(vs, VSCOPE);
                        vencvar(vs);
                        vs->indirect = true;
                        return vp_primaryop(vnext(vs));

        case VT_NUM:
        case VT_STR:    vs->opins = vs->ins;
                        vencvar(vs);
                        vs->indirect = false;
                        return vp_primaryop(vnext(vs));

        case '[':       vs->opins = vs->ins;
                        venc(vs, VTBL);
                        vs->paren++;
                        vp_table(vnext(vs));
                        vs->paren--;
                        vexpect(vs, ']');
                        vs->indirect = false;
                        return vp_primaryop(vs);

        case '(':       vs->opins = vs->ins;
                        vs->paren++;
                        vp_primary(vnext(vs));
                        vs->paren--;
                        vexpect(vs, ')');
                        return vp_primaryop(vs);

        case VT_OP:     vencvar(vs);
                        venc(vs, VTBL);
                        {   int opins = vs->opins;
                            uint8_t prec = vs->prec;
                            vs->prec = vs->nprec;
                            vp_value(vnext(vs));
                            vs->opins = opins;
                            vs->prec = prec;
                        }
                        venc(vs, VADD);
                        venc(vs, VCALL);
                        return vp_primaryop(vs);

        default:        vunexpected(vs);
    }
}

static void vp_tabassign(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SET:    vp_value(vnext(vs));
                        venc(vs, VINSERT);
                        return vp_tabfollow(vs);

        default:        venc(vs, VADD);
                        return vp_tabfollow(vs);
    }
}

static void vp_tabident(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SET:    vencvar(vs);
                        return vp_tabassign(vs);

        default:        venc(vs, VSCOPE);
                        vencvar(vs);
                        vs->indirect = true;
                        vp_primaryop(vs);
                        if (vs->indirect) venc(vs, VLOOKUP);
                        return vp_tabassign(vs);
    }
}

static void vp_tabfollow(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SEP:    return vp_table(vnext(vs));

        default:        return;
    }
}

static void vp_table(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SEP:    return vp_table(vnext(vs));

        case VT_IDENT:  return vp_tabident(vnext(vs));

        case VT_NUM:
        case VT_STR:
        case '[':
        case '(':       vp_value(vs);
                        return vp_tabassign(vs);

        default:        return;
    }
}

static void vp_explet(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SET:    if (!vs->indirect) vunexpected(vs);
                        vp_value(vnext(vs));
                        venc(vs, VINSERT);
                        venc(vs, VDROP);
                        return vp_expfollow(vs);

        default:        vunexpected(vs);
    }
}

static void vp_expassign(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SET:    if (!vs->indirect) vunexpected(vs);
                        vp_value(vnext(vs));
                        venc(vs, VASSIGN);
                        return vp_expfollow(vs);
                        
        default:        if (vs->indirect) venc(vs, VLOOKUP);
                        venc(vs, VDROP);
                        return vp_expfollow(vs);
    }
}

static void vp_expfollow(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SEP:    return vp_expression(vnext(vs));

        default:        return;
    }
}

static void vp_expression(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SEP:    return vp_expression(vnext(vs));

        case VT_RETURN: vp_value(vnext(vs));
                        venc(vs, VRET);
                        return vp_expfollow(vs);

        case VT_LET:    vp_primary(vnext(vs));
                        return vp_explet(vs);

        case VT_END:    return;

        default:        vp_primary(vs);
                        return vp_expassign(vs);
    }
}


// Parses V source code and evaluates the result
int vparse(struct vstate *vs) {
    vs->ins = 0;
    vs->paren = 0;
    vs->prec = -1;
    vs->tok = vlex(vs);

    vp_expression(vs);

    if (vs->tok != 0)
        vunexpected(vs);

    venc(vs, VRETN | VOP_END);
    return vs->ins;
}

