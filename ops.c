#include "ops.h"


// Look up table for opcode names
const char *const op_names[0x20] = {
    "invalid",

    "bind",
    "lookup",
    "flookup"
    "insert",
    "finsert",
    "assign",
    "fassign",

    "dup"
    "pad",
    "drop",

    "ret",   "ret",
    "tcall", "tcall",
    "call",  "call",

    "jump",   "jump",
    "jtrue",  "jtrue",
    "jfalse", "jfalse",

    "imm", "imm",
    "sym", "sym",
    "fn",  "fn",
    "tbl", "tbl"
};

