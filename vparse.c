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
#define venc(v, o) _venc(v, o, __FUNCTION__)
static void _venc(struct vstate *vs, enum vop op, const char *f) {
    printf("in %s\n", f);
    vs->ins += vs->encode(&vs->bcode[vs->ins], op, 0);
}

static void vencarg(struct vstate *vs, enum vop op, uint16_t arg) {
    vs->ins += vs->encode(&vs->bcode[vs->ins], op | VOP_ARG, arg);
}

static void vencvar(struct vstate *vs) {
    uint16_t arg;
    var_t index = tbl_lookup(vs->vars, vs->val);

    if (var_isnil(index)) {
        arg = vs->vars->len;
        tbl_assign(vs->vars, vs->val, vnum(arg));
    } else {
        arg = (uint16_t)var_num(index);
    }

    vencarg(vs, VVAR, arg);
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

        default:        return;
    }
}

static void vp_primary(struct vstate *vs) {
    switch (vs->tok) {
        case VT_IDENT:  venc(vs, VSCOPE);
                        vencvar(vs);
                        vs->indirect = true;
                        return vp_primaryop(vnext(vs));

        case VT_NUM:
        case VT_STR:    vencvar(vs);
                        vs->indirect = false;
                        return vp_primaryop(vnext(vs));

        case '[':       venc(vs, VTBL);
                        vs->paren++;
                        vp_table(vnext(vs));
                        vs->paren--;
                        vexpect(vs, ']');
                        vs->indirect = false;
                        return vp_primaryop(vs);

        case '(':       vs->paren++;
                        vp_primary(vnext(vs));
                        vs->paren--;
                        vexpect(vs, ')');
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
    vs->tok = vlex(vs);

    vp_expression(vs);

    if (vs->tok != 0)
        vunexpected(vs);

    venc(vs, VRETN | VOP_END);
    return vs->ins;
}

