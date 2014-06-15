/*
 * First class function variable type
 */

#ifndef V_FN
#define V_FN

#include "var.h"
#include "tbl.h"


struct fn {
    tbl_t *scope;
    uint16_t acount;
    uint16_t vcount;
    uint16_t stack;

    var_t *vars;
    str_t *bcode;
};

// Functions for managing functions
// Each function is preceded with a reference count
// which is used as its handle in a var
var_t fn_create(var_t args, var_t code, var_t scope);

// Called by garbage collector to clean up
void fn_destroy(void *);


// Call a function. Each function takes a table
// of arguments, and returns a single variable.
var_t fn_call(fn_t *, tbl_t *args);


// Returns a string representation of a function
var_t fn_repr(var_t v);
var_t bfn_repr(var_t v);


#endif
