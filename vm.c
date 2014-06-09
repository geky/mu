#include "vm.h"

#include "var.h"
#include "tbl.h"

#include <stdarg.h>


// bytecode does not need to be portable, as it 
// is compiled ad hoc, but it does need to worry 
// about alignment issues.
static inline uint16_t vc_off(str_t *bcode) {
    return bcode[0] | (bcode[1] << 8);
}

static inline var_t vc_var(str_t *bcode) {
    return (var_t){{
        .bytes = {
            bcode[0],
            bcode[1],
            bcode[2],
            bcode[3],
            bcode[4],
            bcode[5],
            bcode[6],
            bcode[7]
        }
    }};
}

int vcount(uint8_t *code, enum vop op, void *arg) {
    if (0x8 & op)
        return 9;
    else if (0x4 & op)
        return 3;
    else
        return 1;
}

int vencode(uint8_t *code, enum vop op, void *arg) {
    *code++ = op;

    if (0x8 & op) {
        *code++ = ((var_t *)arg)->bytes[0];
        *code++ = ((var_t *)arg)->bytes[1];
        *code++ = ((var_t *)arg)->bytes[2];
        *code++ = ((var_t *)arg)->bytes[3];
        *code++ = ((var_t *)arg)->bytes[4];
        *code++ = ((var_t *)arg)->bytes[5];
        *code++ = ((var_t *)arg)->bytes[6];
        *code++ = ((var_t *)arg)->bytes[7];
        return 9;
    } else if (0x4 & op) {
        *code++ = (*(uint16_t *)arg) >> 8;
        *code++ = 0xff & (*(uint16_t *)arg);
        return 3;
    } else {
        return 1;
    }
}

var_t vexec(str_t *pc, uint16_t s, tbl_t *scope) {
    union {
        struct { var_t v; var_t k; var_t t; var_t f; };
        var_t a[4];
    } reg = {{ .t = vtbl(scope) }};

    var_t stack[s]; // TODO check for overflow

    var_t *sp = stack;
    uint8_t op;


    while (1) {
        op = *pc++;

        switch (0xf0 & op) {
            case VLIT:      reg.a[0x3 & op] = vc_var(pc); pc += 8;      break;
            case VTBL:      reg.a[0x3 & op] = tbl_create(0);            break;
            case VPUSH:     *sp++ = reg.a[0x3 & op];                    break;
            case VPOP:      reg.a[0x3 & op] = *--sp;                    break;

            case VJUMP:     pc += vc_off(pc);                           break;
            case VJEQ:      pc += var_isnull(reg.v) ? vc_off(pc) : 2;   break;
            case VJNE:      pc += !var_isnull(reg.v) ? vc_off(pc) : 2;  break;

            case VLOOKUP:   reg.a[0x3 & op] = var_lookup(reg.t, reg.k); break;
            case VASSIGN:   var_assign(reg.t, reg.k, reg.v); 
                            reg.a[0x3 & op] = reg.t;                    break;
            case VSET:      var_set(reg.t, reg.k, reg.v);               
                            reg.a[0x3 & op] = reg.t;                    break;
            case VADD:      var_add(reg.t, reg.v);                      
                            reg.a[0x3 & op] = reg.t;                    break;
            
            case VCALL:     reg.a[0x3 & op] = var_call(reg.f, reg.t);   break;
            case VTCALL:    return var_call(reg.f, reg.t);   // TODO make sure this is tail calling
            case VRET:      return reg.v;
            case VRETN:     return vnull;
        }
    }
}

