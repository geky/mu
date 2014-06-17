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
static void vexpect(struct vstate *vs, vtok_t tok) {
    if (vs->tok != tok) {
        printf("expected '%c' (%02x) not '%c' (%x)\n", tok, tok, vs->tok, vs->tok);
        assert(vlex(vs) == tok); // TODO
    }

    vnext(vs);
}

// Different encoding calls
static void venc(struct vstate *vs, enum vop op) {
    vs->ins += vs->encode(&vs->bcode[vs->ins], op, 0);
}

static void vencarg(struct vstate *vs, enum vop op, uint16_t arg) {
    vs->ins += vs->encode(&vs->bcode[vs->ins], op | VOP_ARG, arg);
}

static void vencvar(struct vstate *vs) {
    uint16_t arg;
    var_t index = tbl_lookup(vs->vars, vs->val);

    if (var_isnull(index)) {
        arg = vs->vars->len;
        tbl_assign(vs->vars, vs->val, vnum(arg));
    } else {
        arg = (uint16_t)var_num(index);
    }

    vencarg(vs, VVAR, arg);
}



// Parser for V's grammar
static void vp_value(struct vstate *vs);
static void vp_primary(struct vstate *vs);
static void vp_table(struct vstate *vs);
static void vp_exprtarget(struct vstate *vs);
static void vp_expltarget(struct vstate *vs);
static void vp_expfollow(struct vstate *vs);
static void vp_expression(struct vstate *vs);


static void vp_dot(struct vstate *vs) {
    switch (vs->tok) {
        case VT_IDENT:  vencvar(vs);
                        venc(vs, VLOOKUP);
                        return vp_value(vnext(vs));

        default:        return;
    }
}

static void vp_value(struct vstate *vs) {
    switch (vs->tok) {
        case '.':       return vp_dot(vnext(vs));

        case '[':       vs->paren++;
                        vp_primary(vnext(vs));
                        vexpect(vs, ']');
                        vs->paren--;
                        venc(vs, VLOOKUP);
                        return vp_value(vs);

        case '(':       vs->paren++;
                        venc(vs, VTBL);
                        vp_table(vnext(vs));
                        vexpect(vs, ')');
                        vs->paren--;
                        venc(vs, VCALL);
                        return vp_value(vs);

        default:        return;
    }
}

static void vp_primary(struct vstate *vs) {
    switch (vs->tok) {
        case VT_IDENT:  venc(vs, VSCOPE);
                        vencvar(vs);
                        venc(vs, VLOOKUP);
                        return vp_value(vnext(vs));

        case VT_NUM:
        case VT_STR:    vencvar(vs);
                        return vp_value(vnext(vs));

        case '[':       vs->paren++;
                        venc(vs, VTBL);
                        vp_table(vnext(vs));
                        vexpect(vs, ']');
                        vs->paren--;
                        return vp_value(vs);

        case '(':       vs->paren++;
                        vp_primary(vnext(vs));
                        vexpect(vs, ')');
                        vs->paren--;
                        return vp_value(vs);

        default:        return;
    }
}

static void vp_tabrtarget(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SET:    vp_primary(vnext(vs));
                        venc(vs, VASSIGN);
                        return;

        default:        venc(vs, VADD);
                        return;
    }
}

static void vp_tabltarget(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SET:    return vp_tabrtarget(vs);

        default:        venc(vs, VLOOKUP);
                        vp_value(vnext(vs));
                        return vp_tabrtarget(vs);
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

        case VT_IDENT:  vencvar(vs);
                        vp_tabltarget(vnext(vs));
                        return vp_tabfollow(vs);

        case VT_NUM:
        case VT_STR:
        case '[':     
        case '(':       vp_primary(vs);
                        vp_tabrtarget(vs);
                        return vp_tabfollow(vs);

        default:        return;
    }
}

static void vp_expdot(struct vstate *vs) {
    switch (vs->tok) {
        case VT_IDENT:  vencvar(vs);
                        return vp_expltarget(vs);

        default:        return;
    }
}

static void vp_exprtarget(struct vstate *vs) {
    switch (vs->tok) {
        case '.':       return vp_expdot(vnext(vs));

        case '[':       vs->paren++;
                        vp_primary(vnext(vs));
                        vexpect(vs, ']');
                        vs->paren--;
                        return vp_expltarget(vs);

        case '(':       vs->paren++;
                        venc(vs, VTBL);
                        vp_table(vnext(vs));
                        vexpect(vs, ')');
                        vs->paren--;
                        venc(vs, VCALL);
                        return vp_exprtarget(vs);

        default:        return;
    }
}

static void vp_expltarget(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SET:    vp_primary(vnext(vs));
                        venc(vs, VSET);
                        return;

        default:        venc(vs, VLOOKUP);
                        return vp_exprtarget(vs);
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

        case VT_RETURN: vp_primary(vnext(vs));
                        venc(vs, VRET);
                        return vp_expfollow(vs);

        case VT_IDENT:  venc(vs, VSCOPE);
                        vencvar(vs);
                        vp_expltarget(vnext(vs));
                        return vp_expfollow(vs);

        default:        vp_primary(vs);
                        vp_exprtarget(vnext(vs));
                        return vp_expfollow(vs);
    }
}


// Parses V source code and evaluates the result
int vparse(struct vstate *vs) {
    vs->ins = 0;
    vs->paren = 0;
    vs->tok = vlex(vs);

    vp_expression(vs);

    if (vs->tok != 0)
        vunexpected(vs);

    venc(vs, VRETN | VOP_END);
    return vs->ins;
}

