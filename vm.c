#include "vm.h"

#include "parse.h"
#include "types.h"
#include "num.h"
#include "str.h"
#include "fn.h"
#include "tbl.h"

#include <string.h>


// Conversion function for handling frames
void mu_fconvert(c_t sc, mu_t *sframe, c_t dc, mu_t *dframe) {
    if (dc > MU_FRAME) {
        if (sc > MU_FRAME) {
            *dframe = *sframe;
        } else {
            tbl_t *tbl = tbl_create(sc);

            for (uint_t i = 0; i < sc; i++)
                tbl_insert(tbl, muint(i), sframe[i]);

            *dframe = mtbl(tbl);
        }
    } else {
        if (sc >= MU_FRAME) {
            tbl_t *tbl = gettbl(*sframe);

            for (uint_t i = 0; i < dc; i++)
                dframe[i] = tbl_lookup(tbl, muint(i));

            tbl_dec(tbl);
        } else {
            for (uint_t i = 0; i < sc && i < dc; i++)
                dframe[i] = sframe[i];

            for (uint_t i = dc; i < sc; i++)
                mu_dec(sframe[i]);

            for (uint_t i = sc; i < dc; i++)
                dframe[i] = mnil;
        }
    }
}


// Encode the specified opcode and return its size
// Encode the specified opcode and return its size
// Note: size of the jump opcodes currently can not change based on argument
void mu_encode(void (*emit)(void *, data_t), void *p,
               op_t op, int_t d, int_t a, int_t b) {
    union {
        uint16_t i;
        data_t d[2];
    } ins = {0};

    mu_assert(op < 0xf);
    mu_assert(d < 0xf);
    ins.i |= op << 12;
    ins.i |= d << 8;

    if (op >= OP_IMM && op <= OP_DROP) {
        mu_assert(a <= 0xff);
        ins.i |= a;
    } else if (op >= OP_JTRUE && op <= OP_JFALSE) {
        mu_assert(a <= 0xff && a >= -0x100);
        ins.i |= 0xff & (a>>1);
    } else { 
        mu_assert(a <= 0xf);
        mu_assert(b <= 0xf);
        ins.i |= a << 4;
        ins.i |= b;
    }

    emit(p, ins.d[0]);
    emit(p, ins.d[1]);
}


// Instruction access functions
mu_inline enum op op(uint16_t ins) { return (enum op)(ins >> 12); }
mu_inline uint_t rd(uint16_t ins) { return 0xf & (ins >> 8); }
mu_inline uint_t ra(uint16_t ins) { return 0xf & (ins >> 4); }
mu_inline uint_t rb(uint16_t ins) { return 0xf & (ins >> 0); }
mu_inline uint_t i(uint16_t ins) { return (uint8_t)ins; }
mu_inline int_t  j(uint16_t ins) { return (int8_t)ins; }
mu_inline uint_t fa(uint16_t ins) { return ra(ins) > MU_FRAME ? 1 : ra(ins); }
mu_inline uint_t fr(uint16_t ins) { return ra(ins) > MU_FRAME ? 1 : ra(ins); }


// Disassemble bytecode for debugging and introspection
// currently just outputs to stdout
// Unsure if this should be kept as is, returned as string
// or just dropped completely.
void mu_dis(code_t *c) {
    mu_t *imms = code_imms(c);
    const uint16_t *pc = code_bcode(c);
    const uint16_t *end = pc + c->bcount/2;
    uint16_t ins;

    while (pc < end) {
        ins = *pc++;
        printf("%04x ", ins);

        switch (op(ins)) {
            case OP_IMM:
                printf("imm r%d, %d", rd(ins), i(ins));
                { str_t *repr = mu_repr(imms[i(ins)]);
                  printf("(%.*s)\n", str_getlen(repr), str_getdata(repr));
                  str_dec(repr);
                }
                break;
            case OP_FN:
                printf("fn r%d, %d\n", rd(ins), i(ins));
                break;
            case OP_TBL:
                printf("tbl r%d, %d\n", rd(ins), i(ins));
                break;
            case OP_DUP:
                printf("dup r%d, r%d\n", rd(ins), i(ins));
                break;
            case OP_DROP:
                printf("drop r%d\n", rd(ins));
                break;
            case OP_LOOKUP:
                printf("lookup r%d, r%d[r%d]\n", rd(ins), ra(ins), rb(ins));
                break;
            case OP_INSERT:
                printf("insert r%d, r%d[r%d]\n", rd(ins), ra(ins), rb(ins));
                break;
            case OP_ASSIGN:
                printf("assign r%d, r%d[r%d]\n", rd(ins), ra(ins), rb(ins));
                break;
            case OP_ILOOKUP:
                printf("ilookup r%d, r%d[r%d]\n", rd(ins), ra(ins), rb(ins));
                break;
            case OP_IINSERT:
                printf("iinsert r%d, r%d[r%d]\n", rd(ins), ra(ins), rb(ins));
                break;
            case OP_IASSIGN:
                printf("iassign r%d, r%d[r%d]\n", rd(ins), ra(ins), rb(ins));
                break;
            case OP_JTRUE:
                printf("jtrue r%d, %d\n", rd(ins), j(ins));
                break;
            case OP_JFALSE:
                printf("jfalse r%d, %d\n", rd(ins), j(ins));
                break;
            case OP_CALL:
                printf("call r%d, 0x%02x\n", rd(ins), i(ins));
                break;
            case OP_TCALL:
                printf("tcall r%d, 0x%01x\n", rd(ins), mu_args(i(ins)));
                break;
            case OP_RET:
                printf("ret r%d, 0x%01x\n", rd(ins), mu_rets(i(ins)));
                break;
        }
    }
}


// Execute bytecode
void mu_exec(fn_t *fn, c_t c, mu_t *frame) {
    // Allocate termporary variables
    register const uint16_t *pc;
    mu_t *imms;
    struct code **fns;

    register mu_t scratch;
    register uint16_t ins;

reenter:
    {   // Setup the registers and scope
        mu_t regs[fn->flags.regs];
        mu_fconvert(mu_args(c), frame, fn->flags.args, &regs[1]);
        regs[0] = mtbl(tbl_extend(fn->flags.scope, fn->closure));

        // Setup other state
        imms = code_imms(fn->code);
        fns = code_fns(fn->code);
        pc = code_bcode(fn->code);

        // Enter main execution loop
        while (1) {
            ins = *pc++;

            switch (op(ins)) {
                case OP_IMM:
                    regs[rd(ins)] = mu_inc(imms[i(ins)]);
                    break;

                case OP_FN:
                    regs[rd(ins)] = mfn(fn_create(fns[i(ins)], gettbl(regs[0])));
                    break;

                case OP_TBL:
                    regs[rd(ins)] = mtbl(tbl_create(i(ins)));
                    break;

                case OP_DUP:
                    regs[rd(ins)] = mu_inc(regs[i(ins)]);
                    break;

                case OP_DROP:
                    mu_dec(regs[rd(ins)]);
                    break;

                case OP_LOOKUP:
                    scratch = mu_lookup(regs[ra(ins)], regs[rb(ins)]);
                    mu_dec(regs[ra(ins)]);
                    mu_dec(regs[rb(ins)]);
                    regs[rd(ins)] = scratch;
                    break;

                case OP_INSERT:
                    mu_insert(regs[ra(ins)], regs[rb(ins)], regs[rd(ins)]);
                    mu_dec(regs[ra(ins)]);
                    break;

                case OP_ASSIGN:
                    mu_assign(regs[ra(ins)], regs[rb(ins)], regs[rd(ins)]);
                    mu_dec(regs[ra(ins)]);
                    break;

                case OP_ILOOKUP:
                    scratch = mu_lookup(regs[ra(ins)], regs[rb(ins)]);
                    mu_dec(regs[rb(ins)]);
                    regs[rd(ins)] = scratch;
                    break;

                case OP_IINSERT:
                    mu_insert(regs[ra(ins)], regs[rb(ins)], regs[rd(ins)]);
                    break;

                case OP_IASSIGN:
                    mu_assign(regs[ra(ins)], regs[rb(ins)], regs[rd(ins)]);
                    break;

                case OP_JTRUE:
                    if (!isnil(regs[rd(ins)]))
                        pc += j(ins);
                    break;

                case OP_JFALSE:
                    if (isnil(regs[rd(ins)]))
                        pc += j(ins);
                    break;

                case OP_CALL:
                    memcpy(frame, &regs[rd(ins)+1], sizeof(mu_t)*fa(ins));
                    mu_fcall(regs[rd(ins)], i(ins), frame);
                    memcpy(&regs[rd(ins)], frame, sizeof(mu_t)*fr(ins));
                    break;

                case OP_TCALL:
                    scratch = regs[rd(ins)];
                    c = mu_c(ra(ins), mu_rets(c));
                    memcpy(frame, &regs[rd(ins)+1], sizeof(mu_t)*fa(ins));
                    mu_dec(regs[0]);
                    fn_dec(fn);
                    goto tailcall;

                case OP_RET:
                    mu_dec(regs[0]);
                    fn_dec(fn);
                    mu_fconvert(rb(ins), &regs[rd(ins)], mu_rets(c), frame);
                    return;

                default:
                    mu_cerr(str_cstr("invalid opcode"),
                            str_cstr("invalid opcode"));
            }
        }
    }

    mu_unreachable();

tailcall:
    // Use a direct goto to garuntee a tail call when the target is
    // another mu function. Otherwise, we just try our hardest to get
    // a tail call emitted. This has been shown to be pretty unlikely
    // on gcc due to the dynamic array for registers which causes some
    // stack checking to get pushed all the way to the epilogue.
    if (isfn(scratch) && getfn(scratch)->flags.type == 0) {
        fn = getfn(scratch);
        goto reenter;
    } else {
        return mu_fcall(scratch, c, frame);
    }
}

