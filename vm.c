#include "vm.h"

#include "var.h"
#include "tbl.h"

#include <stdarg.h>


// bytecode does not need to be portable, as it 
// is compiled ad hoc, but it does need to worry 
// about alignment issues.
static inline marg_t marg(const str_t *pc) {
    return (pc[0] << 8) | pc[1];
}

static inline msarg_t msarg(const str_t *pc) {
    return (signed)marg(pc);
}

// Return the size taken by the specified opcode
// Note: size of the jump opcode currently can not change
// based on argument, because this is not handled by the parser
int mu_count(mop_t op, marg_t arg) {
    if (MOP_ARG & op)
        return 3;
    else
        return 1;
}

// Encode the specified opcode and return its size
void mu_encode(str_t *code, mop_t op, marg_t arg) {
    *code++ = op;

    if (MOP_ARG & op) {
        *code++ = arg >> 8;
        *code++ = 0xff & arg;
    }
}

// Execute the bytecode
var_t mu_exec(fn_t *f, tbl_t *args, tbl_t *scope, eh_t *eh) {
    var_t stack[f->stack]; // TODO check for overflow

    register const str_t *pc = f->bcode;
    register var_t *sp = stack + f->stack;

    mu_on_err_begin (eh) while (1) {
        printf("pc: %d\t%02x\t", pc-f->bcode, *pc);
        printf("sp: %d\t", f->stack-(sp-stack)); 
        if (sp == stack + f->stack) {
            printf("-\n");
        } else {
            var_print(*sp, eh); printf("\n");
        }

        switch (MOP_OP & *pc++) {
            case MVAR:    *--sp = f->vars[marg(pc)]; pc += 2;                                  break;
            case MFN:     *--sp = vfn(f->fns[marg(pc)], scope); pc += 2;                       break;
            case MNIL:    *--sp = vnil;                                                        break;
            case MTBL:    *--sp = vtbl(tbl_create(0, eh));                                     break;
            case MSCOPE:  *--sp = vtbl(scope);                                                 break;
            case MARGS:   *--sp = vtbl(args);                                                  break;
            
            case MDUP:    sp[-1] = sp[marg(pc)]; sp -= 1; pc += 2;                             break;
            case MDROP:   sp += 1;                                                             break;

            case MJUMP:   pc += msarg(pc)+2;                                                   break;
            case MJFALSE: pc += isnil(*sp++) ? msarg(pc)+2 : 2;                                break;
            case MJTRUE:  pc += !isnil(*sp++) ? msarg(pc)+2 : 2;                               break;

            case MLOOKUP: sp[1] = var_lookup(sp[1], sp[0], eh); sp += 1;                       break;
            case MLOOKDN: sp[1] = var_lookdn(sp[1], sp[0], marg(pc), eh); sp += 1; pc += 2;    break;

            case MASSIGN: var_assign(sp[2], sp[1], sp[0], eh); sp += 3;                        break;
            case MINSERT: var_insert(sp[2], sp[1], sp[0], eh); sp += 2;                        break;
            case MAPPEND: var_append(sp[1], sp[0], eh); sp += 1;                               break;
    
            case MITER:   sp[0] = var_iter(sp[0], eh);                                         break;
            
            case MCALL:   sp[1] = var_call(sp[1], sp[0].tbl, eh); sp += 1;                     break;
            case MTCALL:  return var_call(sp[1], sp[0].tbl, eh); // TODO make sure this is tail calling
            case MRET:    return *sp;
            case MRETN:   return vnil;
        }
    } mu_on_err_do {
        while (sp != stack) {
            var_dec(*sp--);
        }
    } mu_on_err_end;

    __builtin_unreachable();
}

