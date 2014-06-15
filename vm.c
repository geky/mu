#include "vm.h"

#include "var.h"
#include "tbl.h"

#include <stdarg.h>


// bytecode does not need to be portable, as it 
// is compiled ad hoc, but it does need to worry 
// about alignment issues.
static inline uint16_t varg(str_t *bcode) {
    return (bcode[0] << 8) | bcode[1];
}

static inline int16_t vsarg(str_t *bcode) {
    return (int16_t)varg(bcode);
}

int vcount(uint8_t *code, enum vop op, uint16_t arg) {
    return (VOP_ARG & op) ? 3 : 1;
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
        switch (0xf0 & *pc++) {
            case VCONST:    *++sp = f->vars[varg(pc)]; pc += 2;         break;
            case VTBL:      *++sp = tbl_create(0);                      break;
            case VSCOPE:    *++sp = scope;                              break;
            
            case VPUSH:     sp[1] = sp[0]; sp++;                        break;
            case VPOP:      sp--;                                       break;
            case VJUMP:     pc += vsarg(pc);                            break;
            case VJN:       pc += var_isnull(*sp--) ? vsarg(pc) : 2;    break;

            case VLOOKUP:   sp[-1] = var_lookup(sp[-1], sp[0]); sp--;   break;
            case VSET:      var_set(sp[-2], sp[-1], sp[0]); sp -= 3;    break;
            case VASSIGN:   var_assign(sp[-2], sp[-1], sp[0]); sp -= 2; break;
            case VADD:      var_add(sp[-1], sp[0]); sp -= 1;            break;
            
            case VCALL:     sp[-1] = var_call(sp[-1], sp[0]); sp--;     break;
            case VTCALL:    return var_call(sp[-1], sp[0]); sp--; // TODO make sure this is tail calling
            case VRET:      printf("end: %d\n", sp-stack); return *sp;
            case VRETN:     return vnull;
        }
    }
}

