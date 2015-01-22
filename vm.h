/*
 * Mu's virtual machine
 */

#ifdef MU_DEF
#ifndef MU_VM_DEF
#define MU_VM_DEF

#include "mu.h"


/* Mu uses bytecode as an intermediary representation
 * during compilation. It is either executed directly
 * or then converted to opcodes for the underlying machine.
 *
 * The bytecode assumes the underlying machine is a stack
 * based architecture where the word size is a mu_t. Because
 * function calling is performed by passing tables, no other 
 * assumptions are needed and a simple virtual machine can be 
 * implemented with just a stack pointer and program counter.
 *
 * Bytecode is represented in 8 bits with optional tailing arguments
 * that can go up to 16 bits. Only 5 bits are used for encoding 
 * opcodes, the other 3 are used for flags that may help code generation.
 *
 */

// Instruction components
typedef uint16_t arg_t;
typedef int16_t sarg_t;

#define MU_COLOR   0x80 // Debugging color
#define MU_ARG     0x10 // Indicates this opcode uses an argument

typedef enum op {
/*  opcode       encoding before  after   description                                     */
    OP_IMM     = 0x14, // -       imm[i]  places constant immediate on stack
    OP_FN      = 0x15, // -       fn[i]   places function bound with scope on the stack
    OP_NIL     = 0x04, // -       nil     places nil on the stack
    OP_TBL     = 0x05, // -       []      creates a new table on the stack
    OP_SCOPE   = 0x06, // -       scope   places the scope on the stack
    OP_ARGS    = 0x07, // -       args    places the args on the stack

    OP_DUP     = 0x16, // -       s[i]    pushes element on the stack
    OP_DROP    = 0x08, // v       -       pops element off the stack

    OP_JUMP    = 0x17, // -       -       adds signed offset to pc
    OP_JFALSE  = 0x18, // v       v       jump if nil or drop
    OP_JTRUE   = 0x19, // v       v       jump if not nil or drop

    OP_LOOKUP  = 0x0a, // t k     v       looks up t[k] -> v onto stack
    OP_INSERT  = 0x0c, // t k v   t       inserts t[k] = v nonrecursively
    OP_ASSIGN  = 0x0d, // t k v   t       assigns t[k] = v recursively
    OP_APPEND  = 0x0b, // t v     t       appends t[#t] = v

    OP_ITER    = 0x09, // v       iter(v) puts iterator onto stack

    OP_CALL    = 0x02, // f a     r       calls function f(a) -> r onto stack
    OP_TCALL   = 0x03, // f a     -       returns tailcall of function f(a)
    OP_RET     = 0x01, // r       -       returns r
    OP_RETN    = 0x00, // -       -       returns nil
} op_t;


mu_inline bool op_isarg(enum op op) { return MU_ARG & op; }

mu_inline int_t op_stack(enum op op) { 
    switch (op) {
    	case OP_IMM:
    	case OP_FN:
    	case OP_NIL:
    	case OP_TBL:
    	case OP_SCOPE:
    	case OP_ARGS:
    	case OP_DUP:    return 1;

    	case OP_JUMP:   
    	case OP_JFALSE:
    	case OP_JTRUE: 
    	case OP_ITER:
    	case OP_RETN:   return 0;

    	case OP_DROP:  
    	case OP_LOOKUP:
    	case OP_APPEND:
    	case OP_CALL:
    	case OP_RET:    return -1;

    	case OP_INSERT:
    	case OP_ASSIGN:
    	case OP_TCALL:  return -2;
    }

    mu_unreachable();
}

mu_inline const char *op_name(enum op op) {
    switch (op) {
    	case OP_IMM:	return "imm";
    	case OP_FN:	    return "fn";
    	case OP_NIL:	return "nil";
    	case OP_TBL:	return "tbl";
    	case OP_SCOPE:	return "scope";
    	case OP_ARGS:	return "args";

    	case OP_DUP:	return "dup";
    	case OP_DROP:	return "drop";

    	case OP_JUMP:	return "jump";
    	case OP_JFALSE:	return "jfalse";
    	case OP_JTRUE:	return "jtrue";

    	case OP_LOOKUP:	return "lookup";
    	case OP_INSERT:	return "insert";
    	case OP_ASSIGN:	return "assign";
    	case OP_APPEND:	return "append";

    	case OP_ITER:	return "iter";

    	case OP_CALL:	return "call";
    	case OP_TCALL:	return "tcall";
    	case OP_RET:	return "ret";
    	case OP_RETN:	return "retn";

        default:        return "invalid";
    }
}


#endif
#else
#ifndef MU_VM_H
#define MU_VM_H
#define MU_DEF
#include "vm.h"
#include "types.h"
#undef MU_DEF


// Function definitions must be created by 
// machine implementations

// Return the size taken by the specified opcode
// Note: size of the jump opcodes currently can not change
// based on argument, because this is not handled by the parser
size_t mu_size(op_t op, uint_t arg);

// Encode the specified opcode and return its size
void mu_encode(data_t *code, op_t op, uint_t arg);

// Execute the bytecode
mu_t mu_exec(fn_t *f, tbl_t *args, tbl_t *scope);


#endif
#endif
