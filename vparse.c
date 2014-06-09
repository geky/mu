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

static void vencoff(struct vstate *vs, enum vop op, int16_t off) {
    vs->ins += vs->encode(&vs->bcode[vs->ins], op | VARG_OFF, &off);
}

static void vencvar(struct vstate *vs, enum vop op, var_t var) {
    vs->ins += vs->encode(&vs->bcode[vs->ins], op | VARG_VAR, &var);
}

// Handlers for defering instructions
static void vdefer(struct vstate *vs, enum vop op) {
    vs->op = op;
}

static void vdefervar(struct vstate *vs, enum vop op) {
    vs->op = op | VARG_VAR;
}

static void vencdefed(struct vstate *vs, enum vreg r) {
    if (vs->op & VARG_VAR) 
        vencvar(vs, vs->op | r, vs->val);
    else
        venc(vs, vs->op | r);
}


// Parser for V's grammar
static void vp_value(struct vstate *vs);
static void vp_primary(struct vstate *vs);
static void vp_table(struct vstate *vs);
static void vp_exprtarget(struct vstate *vs);
static void vp_expltarget(struct vstate *vs);
static void vp_expptarget(struct vstate *vs);
static void vp_expfollow(struct vstate *vs);
static void vp_expression(struct vstate *vs);


static void vp_value(struct vstate *vs) {
    switch (vs->tok) {
        case '[':       vencdefed(vs, VREG_T);
                        vs->paren++;
                        vp_primary(vnext(vs));
                        vexpect(vs, ']');
                        vencdefed(vs, VREG_K);
                        vdefer(vs, VLOOKUP);
                        return vp_value(vs);

        case '(':       vencdefed(vs, VREG_F);
                        vs->paren++;
                        vdefer(vs, VTBL);
                        vp_table(vnext(vs));
                        vexpect(vs, ')');
                        vs->paren--;
                        vdefer(vs, VCALL);
                        return vp_value(vs);

        default:        return;
    }
}

static void vp_primary(struct vstate *vs) {
    switch (vs->tok) {
        case VT_IDENT:  vencvar(vs, VLIT | VREG_K, vs->val);
                        vdefer(vs, VLOOKUP);
                        return vp_value(vnext(vs));

        case VT_NUM:
        case VT_STR:    vdefervar(vs, VLIT);
                        return vp_value(vnext(vs));

        case '[':       vs->paren++;
                        vdefer(vs, VTBL);
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

/*static void vp_tabtarget(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SET:    vencdefed(vs, VREG_K);
                        vp_primary(vnext(vs));
                        vencdefed(vs, VREG_V);
                        venc(vs, VASSIGN | VREG_T);
                        return;

        default:        return;
    }
}

static void vp_tabfollow(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SEP:    return vp_table(vnext(vs));

        default:        return;
    }
}*/

static void vp_table(struct vstate *vs) {
/*    switch (vs->tok) {
        case VT_SEP:    return vp_table(vnext(vs));

        case VT_IDENT:  vencdefed(vs, VREG_T);
                        vdefervar(vs, VLIT);
                        vp_tabtarget(vnext(vs));
                        return vp_tabfollow(vs);

        default:        vencdefed(vs, VREG_T);
                        vp_primary(vs);
                        vp_tabtarget(vnext(vs));
                        return vp_tabfollow(vs);
    }*/
}

static void vp_exprtarget(struct vstate *vs) {
    switch (vs->tok) {
        case '[':       vencdefed(vs, VREG_T);
                        vs->paren++;
                        vp_primary(vnext(vs));
                        vencdefed(vs, VREG_K);
                        vexpect(vs, ']');
                        vs->paren--;
                        return vp_expltarget(vs);

        case '(':       vencdefed(vs, VREG_F);
                        vs->paren++;
                        vdefer(vs, VTBL);
                        vp_table(vnext(vs));
                        vexpect(vs, ')');
                        vs->paren--;
                        vdefer(vs, VCALL);
                        return vp_exprtarget(vs);

        default:        vencdefed(vs, VREG_V);
                        return;
    }
}

static void vp_expltarget(struct vstate *vs) {
    switch (vs->tok) {
        case VT_SET:    vp_primary(vnext(vs));
                        vencdefed(vs, VREG_V);
                        venc(vs, VSET | VREG_T);
                        return;

        default:        vdefer(vs, VLOOKUP);
                        return vp_exprtarget(vs);
    }
}

static void vp_expptarget(struct vstate *vs) {
    switch (vs->tok) {
        case '[':       venc(vs, VPUSH | VREG_T);
                        vp_expltarget(vs);
                        venc(vs, VPOP | VREG_T);
                        return;

        default:        return vp_expltarget(vs);
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
                        vencdefed(vs, VREG_V);
                        venc(vs, VRET);
                        return vp_expfollow(vs);

        case VT_IDENT:  vencvar(vs, VLIT | VREG_K, vs->val);
                        vp_expptarget(vnext(vs));
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

    venc(vs, VRETN | 0x3);
    return vs->ins;
}

