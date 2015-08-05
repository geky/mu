#include "vm.h"

#include "parse.h"
#include "types.h"
#include "num.h"
#include "str.h"
#include "fn.h"
#include "tbl.h"
#include "err.h"
#include <string.h>


// Instruction access functions
mu_inline enum op op(uint16_t ins) { return (enum op)(ins >> 12); }
mu_inline uint_t  rd(uint16_t ins) { return 0xf & (ins >> 8); }
mu_inline uint_t  ra(uint16_t ins) { return 0xf & (ins >> 4); }
mu_inline uint_t  rb(uint16_t ins) { return 0xf & (ins >> 0); }
mu_inline uint_t  i(uint16_t ins)  { return (uint8_t)ins; }
mu_inline int_t   j(uint16_t ins)  { return (int8_t)ins; }
mu_inline uint_t  fa(uint16_t ins) { return ra(ins) > MU_FRAME ? 1 : ra(ins); }
mu_inline uint_t  fr(uint16_t ins) { return rb(ins) > MU_FRAME ? 1 : rb(ins); }


// Encode the specified opcode and return its size
// Encode the specified opcode and return its size
// Note: size of the jump opcodes currently can not change based on argument
// TODO: change asserts to err throwing checks
void mu_encode(void (*emit)(void *, byte_t), void *p,
               op_t op, int_t d, int_t a, int_t b) {
    uint16_t ins = 0;
    mu_assert(op <= 0xf);
    mu_assert(d <= 0xf);
    ins |= op << 12;
    ins |= d << 8;

    if (op >= OP_IMM && op <= OP_DROP) {
        mu_assert(a <= 0xff);
        ins |= a;
    } else if (op >= OP_JFALSE && op <= OP_JUMP) {
        mu_assert(a-2 <= 0xff && a-2 >= -0x100);
        ins |= 0xff & ((a-2)>>1);
    } else {
        mu_assert(a <= 0xf);
        mu_assert(b <= 0xf);
        ins |= a << 4;
        ins |= b;
    }

    emit(p, ((byte_t *)&ins)[0]);
    emit(p, ((byte_t *)&ins)[1]);
}

int_t mu_patch(void *c, int_t nj) {
    uint16_t ins = *(uint16_t *)c;
    mu_assert(op(ins) >= OP_JFALSE && op(ins) <= OP_JUMP);
    mu_assert(nj-2 <= 0xff && nj-2 >= -0x100);

    int_t pj = (j(ins) << 1)+2;
    ins = (ins & 0xff00) | (0xff & ((nj-2) >> 1));
    *(uint16_t *)c = ins;

    return pj;
}


// Disassemble bytecode for debugging and introspection
// currently just outputs to stdout
// Unsure if this should be kept as is, returned as string
// or just dropped completely.
void mu_dis(struct code *c) {
    mu_t *imms = code_imms(c);
    struct code **fns = code_fns(c);
    const uint16_t *pc = code_bcode(c);
    const uint16_t *end = pc + c->bcount/2;
    uint16_t ins;

    printf("-- dis 0x%08x --\n", (uint_t)c);
    printf("regs: %u, scope: %u, args: %x\n",
           c->regs, c->scope, c->args);

    if (c->icount > 0) {
        printf("imms:\n");
        for (uint_t i = 0; i < c->icount; i++) {
            mu_t repr = mu_repr(imms[i]);
            printf("%08x (%.*s)\n", (uint_t)imms[i], str_len(repr), str_bytes(repr));
            str_dec(repr);
        }
    }

    if (c->fcount > 0) {
        printf("fns:\n");
        for (uint_t i = 0; i < c->fcount; i++) {
            printf("%08x\n", (uint_t)fns[i]);
        }
    }

    printf("bcode:\n");
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
void mu_exec(struct code *c, mu_t scope, frame_t fc, mu_t *frame) {
    // Allocate temporary variables
    register const uint16_t *pc;
    mu_t *imms;
    struct code **fns;

    register mu_t scratch;

reenter:
    // TODO Just here for debugging
    mu_dis(c);
    //

    {   // Setup the registers and scope
        mu_t regs[c->regs];
        regs[0] = scope;
        mu_fconvert(c->args, &regs[1], fc >> 4, frame);

        // Setup other state
        imms = code_imms(c);
        fns = code_fns(c);
        pc = code_bcode(c);

        // Enter main execution loop
        while (1) {
            register uint16_t ins = *pc++;

            switch (op(ins)) {
                case OP_IMM:
                    regs[rd(ins)] = mu_inc(imms[i(ins)]);
                    break;

                case OP_FN:
                    regs[rd(ins)] = mfn(fns[i(ins)], regs[0]);
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

                case OP_RET:
                    mu_dec(regs[0]);
                    code_dec(c);
                    mu_fconvert(fc & 0xf, frame, rb(ins), &regs[rd(ins)]);
                    return;

                case OP_TCALL:
                    scratch = regs[rd(ins)];
                    fc = (ra(ins) << 4) | (fc & 0xf);
                    memcpy(frame, &regs[rd(ins)+1], sizeof(mu_t)*fa(ins));
                    mu_dec(regs[0]);
                    code_dec(c);

                    // Use a direct goto to garuntee a tail call when the target
                    // is another mu function. Otherwise, we just try our hardest
                    // to get a tail call emitted.
                    if (mu_type(scratch) == MU_FN) {
                        c = fn_code(scratch);
                        scope = tbl_extend(c->scope, fn_closure(scratch));
                        goto reenter;
                    } else {
                        return mu_fcall(scratch, fc, frame);
                    }

                default:
                    mu_cerr(mcstr("invalid opcode"),
                            mcstr("invalid opcode"));
            }
        }
    }

    mu_unreachable;
}

