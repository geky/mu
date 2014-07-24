#include "tbl.h"

#include "mem.h"

#include <assert.h>
#include <string.h>


// finds capactiy based on load factor of 1.5
static inline int32_t tbl_ncap(int32_t s) {
    return s + (s >> 1);
}

// finds next power of two
static inline int32_t tbl_npw2(int32_t s) {
    if (s)
        return 1 << (32-__builtin_clz(s - 1));
    else
        return 0;
}

// iterates through hash entries using
// i = i*5 + 1 to avoid degenerative cases
static inline hash_t tbl_next(hash_t i) {
    return (i<<2) + i + 1;
}


// Functions for managing tables
// Each table is preceeded with a reference count
// which is used as its handle in a var
tbl_t *tbl_create(uint16_t size) {
    tbl_t *tbl = vref_alloc(sizeof(tbl_t));

    tbl->mask = tbl_npw2(tbl_ncap(size)) - 1;

    tbl->tail = 0;
    tbl->nils = 0;
    tbl->len = 0;

    tbl->koff = 0;
    tbl->voff = 0;
    tbl->stride = 0;

    return tbl;
}


// Called by garbage collector to clean up
void tbl_destroy(void *m) {
    tbl_t *tbl = m;
    int i;

    if (tbl->stride > 0) {
        if (tbl->stride > 1) {
            for (i=0; i <= tbl->mask; i++) {
                if (!var_isnil(tbl->array[2*i])) {
                    var_dec(tbl->array[2*i    ]);
                    var_dec(tbl->array[2*i + 1]);
                }
            }
        } else {
            for (i=0; i < tbl->nils+tbl->len; i++) {
                var_dec(tbl->array[i]);
            } 
        }

        vdealloc(tbl->array);
    }

    if (tbl->tail)
        tbl_dec(tbl->tail);
}


// Recursively looks up a key in the table
// returns either that value or nil
var_t tbl_lookup(tbl_t *tbl, var_t key) {
    if (var_isnil(key))
        return vnil;

    hash_t i, hash = var_hash(key);

    for (tbl = tbl_readp(tbl); tbl; tbl = tbl_readp(tbl->tail)) {
        if (tbl->stride < 2) {
            i = hash - tbl->koff;

            if (var_equals(key, vnum(hash)) && i < tbl->nils+tbl->len) {
                if (tbl->stride == 0)
                    return vnum(i + tbl->voff);
                else if (!var_isnil(tbl->array[i]))
                    return tbl->array[i];
            }
        } else {
            for (i = hash;; i = tbl_next(i)) {
                hash_t mi = i & tbl->mask;
                var_t *v = &tbl->array[2*mi];

                if (var_isnil(v[0]))
                    break;

                if (var_equals(key, v[0]) && !var_isnil(v[1]))
                    return v[1];
            }
        }
    }

    return vnil;
}


// converts implicit range to actual array of nums on heap
static void tbl_realizevars(tbl_t *tbl) {
    int32_t cap = tbl->mask + 1;
    var_t *w = valloc(cap * sizeof(var_t));
    int i;

    for (i=0; i < tbl->len; i++) {
        w[i] = vnum(i + tbl->voff);
    }

    tbl->array = w;
    tbl->stride = 1;
}

static void tbl_realizekeys(tbl_t *tbl) {
    int32_t cap = tbl->mask + 1;
    var_t *w = valloc(2*cap * sizeof(var_t));
    int i;

    if (tbl->stride == 0) {
        for (i=0; i < tbl->len; i++) {
            w[2*i  ] = vnum(i + tbl->koff);
            w[2*i+1] = vnum(i + tbl->voff);
        }
    } else {
        for (i=0; i < tbl->len; i++) {
            w[2*i  ] = vnum(i + tbl->koff);
            w[2*i+1] = tbl->array[i];
        }

        vdealloc(tbl->array);
    }

    tbl->array = w;
    tbl->stride = 2;
}


// reallocates and rehashes a table
static inline void tbl_resize(tbl_t * tbl, uint16_t size) {
    int32_t cap = tbl_npw2(tbl_ncap(size));
    int32_t j, mask = cap - 1;

    if (tbl->stride < 2) {
        tbl->mask = mask;

        if (tbl->stride == 0)
            return;

        if (tbl->nils + tbl->len <= cap) {
            var_t *w = valloc(cap * sizeof(var_t));
            memcpy(w, tbl->array, (tbl->nils+tbl->len) * sizeof(var_t));

            vdealloc(tbl->array);
            tbl->array = w;
            return;
        }

        tbl_realizekeys(tbl);
    }

    var_t *w = valloc(2*cap * sizeof(var_t));
    memset(w, 0, 2*cap * sizeof(var_t));

    for (j=0; j <= tbl->mask; j++) {
        var_t *u = &tbl->array[2*j];

        if (var_isnil(u[0]) || var_isnil(u[1]))
            continue;

        hash_t i = var_hash(u[0]);

        for (;; i = tbl_next(i)) {
            hash_t mi = i & mask;
            var_t *v = &w[2*mi];

            if (var_isnil(v[0])) {
                v[0] = u[0];
                v[1] = u[1];
                break;
            }
        }
    }

    vdealloc(tbl->array);
    tbl->array = w;
    tbl->nils = 0;
    tbl->mask = mask;
}
    

// Inserts a value in the table with the given key
// without decending down the tail chain
void tbl_insert(tbl_t *tbl, var_t key, var_t val) {
    tbl = tbl_writep(tbl);

    if (var_isnil(key))
        return;

    if (!var_isnil(val) && tbl_ncap(tbl->nils+tbl->len + 1) > tbl->mask + 1)
        tbl_resize(tbl, tbl->len + 1);

   
    hash_t i, hash = var_hash(key);

    if (tbl->stride < 2) {
        i = hash - tbl->koff;

        if (!var_isnil(key) && tbl->nils+tbl->len == 0 &&
            var_isnum(key) && !(hash & ~0xffff)) {
            tbl->koff = hash;

            if (tbl->stride == 0) {
                if (var_isnum(val) && !(num_hash(val) & ~0xffff)) {
                    tbl->voff = num_hash(val);
                    tbl->len++;
                    return;
                }

                tbl_realizevars(tbl);
            }

            tbl->array[0] = val;
            tbl->len++;
            return;
        }

        if (var_equals(key, vnum(hash))) {
            if (var_isnil(val)) {
                if (i >= tbl->nils+tbl->len)
                    return;

                if (i == tbl->nils+tbl->len - 1) {
                    if (tbl->stride != 0)
                        var_dec(tbl->array[i]);

                    tbl->len--;
                    return;
                }

                if (tbl->stride == 0)
                    tbl_realizevars(tbl);

                if (var_isnil(tbl->array[i]))
                    return;

                var_dec(tbl->array[i]);
                tbl->array[i] = vnil;
                tbl->nils++; tbl->len--;
                return;
            } else {
               if (i == tbl->nils+tbl->len) {
                    if (tbl->stride == 0) {
                        if (var_equals(val, vnum(i+tbl->voff))) {
                            tbl->len++;
                            return;
                        }

                        tbl_realizevars(tbl);
                    }

                    tbl->array[i] = val;
                    tbl->len++;
                    return;
                } else if (i < tbl->nils+tbl->len) {
                    if (tbl->stride == 0) {
                        if (var_equals(val, vnum(i+tbl->voff)))
                            return;

                        tbl_realizevars(tbl);
                    }

                    if (var_isnil(tbl->array[i])) {
                        tbl->array[i] = val;
                        tbl->nils--; tbl->len++;
                        return;
                    }

                    var_dec(tbl->array[i]);
                    tbl->array[i] = val;
                    return;
                }
            }
        }

        tbl_realizekeys(tbl);
    }


    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & tbl->mask;
        var_t *v = &tbl->array[2*mi];

        if (var_isnil(v[0])) {
            if (var_isnil(val))
                return;

            var_dec(v[0]); v[0] = key;
            var_dec(v[1]); v[1] = val;
            tbl->len++;
            return;
        }

        if (var_equals(key, v[0])) {
            if (var_isnil(val)) {
                if (var_isnil(v[1]))
                    return;

                var_dec(v[1]); v[1] = vnil;
                tbl->nils++; tbl->len--;
                return;
            }

            if (var_isnil(v[1])) {
                v[1] = val;
                tbl->nils--; tbl->len++;
                return;
            }
            
            var_dec(v[1]); v[1] = val;
            return;
        }
    }
}


// Sets the next index in the table with the value
void tbl_add(tbl_t *tbl, var_t val) {
    tbl_insert(tbl, vnum(tbl->len), val);
}


// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
void tbl_assign(tbl_t *head, var_t key, var_t val) {
    if (var_isnil(key))
        return;

    hash_t i, hash = var_hash(key);
    tbl_t *tbl;

    for (tbl = head; tbl; tbl = tbl->tail) {
        if (tbl_isro(tbl))
            break;
        else
            tbl = tbl_writep(tbl);

        if (tbl->stride < 2) {
            i = hash - tbl->koff;

            if (var_equals(key, vnum(hash)) && i < tbl->nils+tbl->len) {
                if (var_isnil(val)) {
                    if (i == tbl->nils+tbl->len - 1) {
                        if (tbl->stride != 0)
                            var_dec(tbl->array[i]);

                        tbl->len--;
                        return;
                    }

                    if (tbl->stride == 0)
                        tbl_realizevars(tbl);

                    if (!var_isnil(tbl->array[i])) {
                        var_dec(tbl->array[i]);
                        tbl->array[i] = vnil;
                        tbl->nils++; tbl->len--;
                        return;
                    }
                } else {
                    if (tbl->stride == 0) {
                        if (var_equals(val, vnum(i+tbl->voff)))
                            return;

                        tbl_realizevars(tbl);
                    }

                    if (!var_isnil(tbl->array[i])) {
                        var_dec(tbl->array[i]);
                        tbl->array[i] = val;
                        return;
                    }
                }
            }
        } else {
            for (i = hash;; i = tbl_next(i)) {
                hash_t mi = i & tbl->mask;
                var_t *v = &tbl->array[2*mi];

                if (var_isnil(v[0])) 
                    break;

                if (var_equals(key, v[0])) {
                    if (var_isnil(v[1]))
                        break;

                    if (var_isnil(val)) {
                        var_dec(v[1]); v[1] = vnil;
                        tbl->nils++; tbl->len--;
                        return;
                    }

                    var_dec(v[1]); v[1] = val;
                    return;
                }
            }
        }
    }

    if (var_isnil(val))
        return;


    tbl = tbl_writep(head);

    if (tbl_ncap(tbl->len + tbl->nils + 1) > tbl->mask + 1)
        tbl_resize(tbl, tbl->len+1);


    if (tbl->stride < 2) {
        i = hash - tbl->koff;

        if (tbl->nils+tbl->len == 0 && var_isnum(val) && !(hash & ~0xffff)) {
            tbl->koff = hash;

            if (tbl->stride == 0) {
                if (var_isnum(val) && !(num_hash(val) & ~0xffff)) {
                    tbl->voff = num_hash(val);
                    tbl->len++;
                    return;
                }

                tbl_realizevars(tbl);
            }

            tbl->array[0] = val;
            tbl->len++;
            return;
        }

        if (var_equals(key, vnum(hash))) {
            if (i == tbl->nils+tbl->len) {
                if (tbl->stride == 0) {
                    if (var_equals(val, vnum(i+tbl->voff))) {
                        tbl->len++;
                        return;
                    }

                    tbl_realizevars(tbl);
                }

                tbl->array[i] = val;
                tbl->len++;
                return;
            } else if (i < tbl->nils+tbl->len && var_isnil(tbl->array[i])) {
                tbl->array[i] = val;
                tbl->nils--; tbl->len++;
                return;
            }
        }

        tbl_realizekeys(tbl);
    }


    for (i = hash;; i = tbl_next(i)) {
        hash_t mi = i & tbl->mask;
        var_t *v = &tbl->array[2*mi];

        if (var_isnil(v[0])) {
            var_dec(v[0]); v[0] = key;
            var_dec(v[1]); v[1] = val;
            tbl->len++;
            return;
        }

        if (var_equals(key, v[0]) && var_isnil(v[1])) {
            v[1] = val;
            tbl->nils--; tbl->len++;
            return;
        }
    }
}



// Returns a string representation of the table
var_t tbl_repr(var_t v) {
    tbl_t *tbl = tbl_readp(v.tbl);
    unsigned int size = 2;

    uint8_t *out, *s;

    var_t *key_repr = valloc(tbl->len * sizeof(var_t));
    var_t *val_repr = valloc(tbl->len * sizeof(var_t));

    int i = 0;
    tbl_for (k, v, tbl, {
        key_repr[i] = var_repr(k);
        val_repr[i] = var_repr(v);
        size += key_repr[i].len + 2;
        size += val_repr[i].len;

        if (i != tbl->len-1)
            size += 2;

        i++;
    })

    out = vref_alloc(size);
    s = out;

    *s++ = '[';

    for (i=0; i < tbl->len; i++) {
        memcpy(s, var_str(key_repr[i]), key_repr[i].len);
        s += key_repr[i].len;
        var_dec(key_repr[i]);

        *s++ = ':';
        *s++ = ' ';

        memcpy(s, var_str(val_repr[i]), val_repr[i].len);
        s += val_repr[i].len;
        var_dec(val_repr[i]);

        if (i != tbl->len-1) {
            *s++ = ',';
            *s++ = ' ';
        }
    }

    *s++ = ']';

    vdealloc(key_repr);
    vdealloc(val_repr);

    return vstr(out, 0, size);
}
