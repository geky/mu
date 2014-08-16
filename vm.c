#include "vm.h"

#include "var.h"
#include "tbl.h"

#include <stdarg.h>


// bytecode does not need to be portable, as it 
// is compiled ad hoc, but it does need to worry 
// about alignment issues.
static inline varg_t varg(const str_t *pc) {
    return (pc[0] << 8) | pc[1];
}

static inline vsarg_t vsarg(const str_t *pc) {
    return (signed)varg(pc);
}

// Return the size taken by the specified opcode
// Note: size of the jump opcode currently can not change
// based on argument, because this is not handled by the parser
int vcount(vop_t op, varg_t arg) {
    if (VOP_ARG & op)
        return 3;
    else
        return 1;
}

// Encode the specified opcode and return its size
void vencode(str_t *code, vop_t op, varg_t arg) {
    *code++ = op;

    if (VOP_ARG & op) {
        *code++ = arg >> 8;
        *code++ = 0xff & arg;
    }
}

// Execute the bytecode
var_t vexec(fn_t *f, tbl_t *args, tbl_t *scope) {
    var_t stack[f->stack]; // TODO check for overflow

    register const str_t *pc = f->bcode;
    register var_t *sp = stack + f->stack;

    while (1) {
        printf("pc: %d\t%02x\t", pc-f->bcode, *pc);
        printf("sp: %d\t", f->stack-(sp-stack)); 
        if (sp == stack + f->stack) {
            printf("-\n");
        } else {
            var_print(*sp); printf("\n");
        }

        switch (VOP_OP & *pc++) {
            case VVAR:      *--sp = f->vars[varg(pc)]; pc += 2;             break;
            case VFN:       *--sp = vfn(f->fns[varg(pc)], scope); pc += 2; break;
            case VNIL:      *--sp = vnil;                                   break;
            case VTBL:      *--sp = vtbl(tbl_create(0));                    break;
            case VSCOPE:    *--sp = vtbl(scope);                            break;
            case VARGS:     *--sp = vtbl(args);                             break;
            
            case VDUP:      sp[-1] = sp[varg(pc)]; sp--; pc += 2;           break;
            case VDROP:     sp++;                                           break;

            case VJUMP:     pc += vsarg(pc)+2;                              break;
            case VJFALSE:   pc += var_isnil(*sp++) ? vsarg(pc)+2 : 2;       break;
            case VJTRUE:    pc += !var_isnil(*sp++) ? vsarg(pc)+2 : 2;      break;

            case VLOOKUP:   sp += 1; *sp = var_lookup(sp[0], sp[-1]);       break;
            case VLOOKDN:   sp += 1; *sp = var_lookdn(sp[0], sp[-1], varg(pc)); 
                            pc += 2;                                        break;

            case VASSIGN:   sp += 3; var_assign(sp[-1], sp[-2], sp[-3]);    break;
            case VINSERT:   sp += 2; var_insert(sp[0], sp[-1], sp[-2]);     break;
            case VADD:      sp += 1; var_add(sp[0], sp[-1]) ;               break;

            case VITER:     *sp = var_iter(*sp);                            break;
            
            case VCALL:     sp += 1; *sp = var_call(sp[0], sp[-1].tbl);     break;
            case VTCALL:    sp += 2; return var_call(sp[-1], sp[-2].tbl); // TODO make sure this is tail calling
            case VRET:      return *sp;
            case VRETN:     return vnil;
        }
    }
}

