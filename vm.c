#include "vm.h"

#include "parse.h"
#include "types.h"
#include "num.h"
#include "str.h"
#include "fn.h"
#include "tbl.h"
#include "err.h"
#include <string.h>


// Encode the specified opcode and return its size
// Encode the specified opcode and return its size
// Note: size of the jump opcodes currently can not change based on argument
// TODO: change asserts to err throwing checks
void mu_encode(void (*emit)(void *, byte_t), void *p,
               op_t op, int_t d, int_t a, int_t b) {
    union {
        uint16_t i;
        byte_t d[2];
    } ins = {0};

    mu_assert(op <= 0xf);
    mu_assert(d <= 0xf);
    ins.i |= op << 12;
    ins.i |= d << 8;

    if (op >= OP_IMM && op <= OP_DROP) {
        mu_assert(a <= 0xff);
        ins.i |= a;
    } else if (op >= OP_JFALSE && op <= OP_JUMP) {
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
mu_inline uint_t fr(uint16_t ins) { return rb(ins) > MU_FRAME ? 1 : rb(ins); }


// Disassemble bytecode for debugging and introspection
// currently just outputs to stdout
// Unsure if this should be kept as is, returned as string
// or just dropped completely.
void mu_dis(struct code *c) {
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
                { mu_t repr = mu_repr(imms[i(ins)]);
                  printf("(%.*s)\n", str_len(repr), str_bytes(repr));
                  str_dec(repr);
                }
                break;
            case OP_FN:
                printf("fn r%d, %d\n", rd(ins), i(ins));
                break;
            case OP_TBL:
                printf("tbl r%d, %d\n", rd(ins), i(ins));
                break;
            case OP_MOVE:
                printf("move r%d, r%d\n", rd(ins), i(ins));
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
            case OP_LOOKDN:
                printf("lookdn r%d, r%d[r%d]\n", rd(ins), ra(ins), rb(ins));
                break;
            case OP_INSERT:
                printf("insert r%d, r%d[r%d]\n", rd(ins), ra(ins), rb(ins));
                break;
            case OP_ASSIGN:
                printf("assign r%d, r%d[r%d]\n", rd(ins), ra(ins), rb(ins));
                break;
            case OP_JUMP:
                printf("jump %d\n", j(ins));
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
                printf("tcall r%d, 0x%02x\n", rd(ins), i(ins));
                break;
            case OP_RET:
                printf("ret r%d, 0x%02x\n", rd(ins), i(ins));
                break;
        }
    }
}


// Execute bytecode
void mu_exec(struct fn *fn, frame_t c, mu_t *frame) {
    // Allocate temporary variables
    register const uint16_t *pc;
    mu_t *imms;
    struct code **fns;

    register mu_t scratch;
    register uint16_t ins;

reenter:
    // TODO Just here for debugging
    printf("-- dis --\n");
    printf("regs: %u, scope: %u, args: %u\n", 
           fn->flags.regs, fn->flags.scope, fn->flags.args);
    mu_dis(fn->code);

    {   // Setup the registers and scope
        mu_t regs[fn->flags.regs];
        mu_fconvert(fn->flags.args, &regs[1], c >> 4, frame);
        regs[0] = tbl_extend(fn->flags.scope, fn->closure);

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
                    regs[rd(ins)] = fn_create(fns[i(ins)], regs[0]);
                    break;

                case OP_TBL:
                    regs[rd(ins)] = tbl_create(i(ins));
                    break;

                case OP_MOVE:
                    regs[rd(ins)] = regs[i(ins)];
                    break;

                case OP_DUP:
                    regs[rd(ins)] = mu_inc(regs[i(ins)]);
                    break;

                case OP_DROP:
                    mu_dec(regs[rd(ins)]);
                    break;

                case OP_LOOKUP:
                    scratch = mu_lookup(regs[ra(ins)], regs[rb(ins)]);
                    mu_dec(regs[rb(ins)]);
                    regs[rd(ins)] = scratch;
                    break;

                case OP_LOOKDN:
                    scratch = mu_lookup(regs[ra(ins)], regs[rb(ins)]);
                    mu_dec(regs[ra(ins)]);
                    mu_dec(regs[rb(ins)]);
                    regs[rd(ins)] = scratch;
                    break;

                case OP_INSERT:
                    mu_insert(regs[ra(ins)], regs[rb(ins)], regs[rd(ins)]);
                    break;

                case OP_ASSIGN:
                    mu_assign(regs[ra(ins)], regs[rb(ins)], regs[rd(ins)]);
                    break;

                case OP_JUMP:
                    pc += j(ins);
                    break;

                case OP_JTRUE:
                    if (regs[rd(ins)])
                        pc += j(ins);
                    break;

                case OP_JFALSE:
                    if (!regs[rd(ins)])
                        pc += j(ins);
                    break;

                case OP_CALL:
                    memcpy(frame, &regs[rd(ins)+1], sizeof(mu_t)*fa(ins));
                    mu_fcall(regs[rd(ins)], i(ins), frame);
                    memcpy(&regs[rd(ins)], frame, sizeof(mu_t)*fr(ins));
                    break;

                case OP_TCALL:
                    scratch = regs[rd(ins)];
                    c = (ra(ins) << 4) | (c & 0xf);
                    memcpy(frame, &regs[rd(ins)+1], sizeof(mu_t)*fa(ins));
                    mu_dec(regs[0]);
                    fn_dec(mfn_(fn)); // TODO????
                    goto tailcall;

                case OP_RET:
                    mu_dec(regs[0]);
                    fn_dec(mfn_(fn)); // TODO??
                    mu_fconvert(c & 0xf, frame, rb(ins), &regs[rd(ins)]);
                    return;

                default:
                    mu_cerr(mcstr("invalid opcode"),
                            mcstr("invalid opcode"));
            }
        }
    }

    mu_unreachable;

tailcall:
    // Use a direct goto to garuntee a tail call when the target is
    // another mu function. Otherwise, we just try our hardest to get
    // a tail call emitted. This has been shown to be pretty unlikely
    // on gcc due to the dynamic array for registers which causes some
    // stack checking to get pushed all the way to the epilogue.
    // TODO how to integrate this with structure hiding in fn.c?
    if (mu_isfn(scratch) && fn_fn_(scratch)->flags.type == 0) {
        fn = fn_fn_(scratch);
        goto reenter;
    } else {
        return mu_fcall(scratch, c, frame);
    }
}

