/*
 * First class function variable type
 */

#ifndef V_FN
#define V_FN

#include "var.h"
#include "tbl.h"


struct fn {
    const str_t *bcode; // function bytecode

    len_t acount;   // number of arguments
    len_t vcount;   // number of stored vars
    len_t bcount;   // length of bytecode

    len_t stack;    // amount of stack usage

    var_t *vars;    // stored vars
};

// Functions for managing functions
// Each function is preceded with a reference count
// which is used as its handle in a var
fn_t *fn_create(tbl_t *args, var_t code, tbl_t *ops, tbl_t *keys);

// Called by garbage collector to clean up
void fn_destroy(void *);


// Call a function. Each function takes a table
// of arguments, and returns a single variable.
var_t fn_call(fn_t *, tbl_t *args, tbl_t *scope);


// Returns a string representation of a function
var_t fn_repr(var_t v);


#endif
