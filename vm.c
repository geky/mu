#include "vm.h"

#include "var.h"
#include "tbl.h"

#include <stdarg.h>


// bytecode does not need to be portable, as it 
// is compiled ad hoc, but it does need to worry 
// about alignment issues.
static inline uint16_t varg(str_t *pc) {
    return (pc[0] << 8) | pc[1];
}

static inline int16_t vsarg(str_t *pc) {
    return (int16_t)varg(pc);
}

int vcount(uint8_t *code, enum vop op, uint16_t arg) {
    if (VOP_ARG & op)
        return 3;
    else
        return 1;
}

int vencode(uint8_t *code, enum vop op, uint16_t arg) {
    *code++ = op;

    if (VOP_ARG & op) {
        *code++ = arg >> 8;
        *code++ = 0xff & arg;
        return 3;
    } else {
        return 1;
    }
}


var_t vexec(fn_t *f, var_t scope) {
    var_t stack[f->stack]; // TODO check for overflow

    register str_t *pc = f->bcode;
    register var_t *sp = stack;

    while (1) {
        switch (VOP_OP & *pc++) {
            case VVAR:      *++sp = f->vars[varg(pc)]; pc += 2;             break;
            case VTBL:      *++sp = tbl_create(0);                          break;
            case VSCOPE:    *++sp = scope;                                  break;
            
            case VDUP:      sp[1] = sp[varg(pc)]; sp++; pc += 2;            break;
            case VDROP:     sp--;                                           break;

            case VJUMP:     pc += vsarg(pc);                                break;
            case VJFALSE:   pc += var_isnull(*sp) ? vsarg(pc) : 2;          break;
            case VJTRUE:    pc += !var_isnull(*sp) ? vsarg(pc) : 2;         break;

            case VLOOKUP:   sp -= 1; *sp = var_lookup(sp[0], sp[1]);        break;
            case VSET:      sp -= 3; var_set(sp[1], sp[2], sp[3]);          break;
            case VASSIGN:   sp -= 2; var_assign(sp[0], sp[1], sp[2]);       break;
            case VADD:      sp -= 1; var_add(sp[0], sp[1]) ;                break;
            
            case VCALL:     sp -= 1; *sp = var_call(sp[0], sp[1]);          break;
            case VTCALL:    sp -= 2; return var_call(sp[1], sp[2]); // TODO make sure this is tail calling
            case VRET:      return *sp;
            case VRETN:     return vnull;
        }
    }
}

