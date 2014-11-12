#include "vm.h"

#include "parse.h"
#include "var.h"
#include "fn.h"
#include "tbl.h"


// bytecode does not need to be portable, as it 
// is compiled ad hoc, but it does need to worry 
// about alignment issues.
mu_inline arg_t arg(const str_t *pc) {
    return (pc[0] << 8) | pc[1];
}

mu_inline sarg_t sarg(const str_t *pc) {
    return (signed)arg(pc);
}

// Return the size taken by the specified opcode
// Note: size of the jump opcode currently can not change
// based on argument, because this is not handled by the parser
int mu_size(op_t op, arg_t arg) {
    if (MU_ARG & op)
        return 3;
    else
        return 1;
}

// Encode the specified opcode and return its size
void mu_encode(mstr_t *code, op_t op, arg_t arg) {
    *code++ = op;

    if (MU_ARG & op) {
        *code++ = arg >> 8;
        *code++ = 0xff & arg;
    }
}

// Execute the bytecode
var_t mu_exec(fn_t *f, tbl_t *args, tbl_t *scope, eh_t *eh) {
    var_t stack[f->stack]; // TODO check for overflow

    register str_t *pc = f->bcode;
    register var_t *sp = stack + f->stack;

    while (1) {
        printf("pc: %d\t%02x\t", pc-f->bcode, *pc);
        printf("sp: %d\t", f->stack-(sp-stack)); 
        if (sp == stack + f->stack) {
            printf("-\n");
        } else {
            var_print(*sp, eh); printf("\n");
        }

        switch (*pc++ >> 3) {
            case OP_VAR:    sp[-1] = f->vars[arg(pc)]; pc += 2; sp--;                           break;
            case OP_FN:     sp[-1] = vfn(f->fns[arg(pc)], scope); pc += 2; sp--;                break;
            case OP_NIL:    sp[-1] = vnil; sp--;                                                break;
            case OP_TBL:    sp[-1] = vtbl(tbl_create(0, eh)); sp--;                             break;
            case OP_SCOPE:  sp[-1] = vtbl(scope); sp--;                                         break;
            case OP_ARGS:   sp[-1] = vtbl(args); sp--;                                          break;
            
            case OP_DUP:    sp[-1] = sp[arg(pc)]; sp -= 1; pc += 2;                             break;
            case OP_DROP:   sp += 1;                                                            break;

            case OP_JUMP:   pc += sarg(pc)+2;                                                   break;
            case OP_JFALSE: pc += isnil(*sp++) ? sarg(pc)+2 : 2;                                break;
            case OP_JTRUE:  pc += !isnil(*sp++) ? sarg(pc)+2 : 2;                               break;

            case OP_LOOKUP: sp[1] = var_lookup(sp[1], sp[0], eh); sp += 1;                      break;
            case OP_LOOKDN: sp[1] = var_lookdn(sp[1], sp[0], arg(pc), eh); sp += 1; pc += 2;    break;

            case OP_ASSIGN: var_assign(sp[2], sp[1], sp[0], eh); sp += 3;                       break;
            case OP_INSERT: var_insert(sp[2], sp[1], sp[0], eh); sp += 2;                       break;
            case OP_APPEND: var_append(sp[1], sp[0], eh); sp += 1;                              break;
    
            case OP_ITER:   sp[0] = var_iter(sp[0], eh);                                        break;
            
            case OP_CALL:   sp[1] = var_call(sp[1], gettbl(sp[0]), eh); sp += 1;                break;
            case OP_TCALL:  return var_call(sp[1], gettbl(sp[0]), eh); // TODO make sure this is tail calling
            case OP_RET:    return *sp;
            case OP_RETN:   return vnil;
        }
    }

    mu_unreachable();
}

