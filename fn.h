/*
 * First class function variable type
 */

#ifndef V_FN
#define V_FN

#include "var.h"
#include "tbl.h"


struct fn {
    tbl_t *scope;
    uint16_t alen;
    
    void *bcode;
    var_t code;

    var_t args[0];
};


// Functions for managing functions
// Each function is preceded with a reference count
// which is used as its handle in a var

// Called by garbage collector to clean up
void fn_destroy(void *);


// Call a function. Each function takes a table
// of arguments, and returns a single variable.
var_t fn_call(var_t, tbl_t *args);
var_t fnp_call(fn_t *, tbl_t *args);


// Returns a string representation of a function
var_t fn_repr(var_t v);


#endif
