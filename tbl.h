/*
 * Table variable type
 */

#ifndef V_TBL
#define V_TBL

#include "var.h"

#define tbl_maxcap ((1<<16)-1)


typedef struct tbl {
    struct tbl *tail; // tail chain of tables

    uint16_t nulls; // count of null entries
    uint16_t len;   // count of keys in use
    int32_t mask;   // size of entries - 1

    var_t *keys; // array of keys
    var_t *vals; // array of values
} tbl_t;


// Functions for managing tables
// Each table is preceeded with a reference count
// which is used as its handle in a var
tbl_t *tbl_create(void);
void tbl_destroy(tbl_t *);

// Creates preallocated table or array
tbl_t *tbl_alloc_array(uint16_t size);
tbl_t *tbl_alloc_table(uint16_t size);

// Recursively looks up a key in the table
// returns either that value or null
var_t tbl_lookup(tbl_t *, var_t key);

// Sets a value in the table with the given key
// decends down the tail chain until its found
void tbl_set(tbl_t *, var_t key, var_t val);

// Sets a value in the table with the given key
// without decending down the tail chain
void tbl_assign(tbl_t *, var_t key, var_t val);


#endif
