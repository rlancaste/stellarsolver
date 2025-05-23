/*
 # This file is part of the Astrometry.net suite.
 # Licensed under a 3-clause BSD style license - see LICENSE
 */

#include <stdio.h>
#include <stdlib.h>

#include "codekd.h"
#include "kdtree_fits_io.h"
#include "starutil.h"
#include "errors.h"
#include "log.h" //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging

static codetree_t* codetree_alloc() {
    codetree_t* s = calloc(1, sizeof(codetree_t));
    if (!s) {
        debug("Failed to allocate a code kdtree struct.\n"); //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging
        return NULL;
    }
    return s;
}
/** //# Modified by Robert Lancaster for the StellarSolver Internal Library
int codetree_append_to(codetree_t* s, FILE* fid) {
    return kdtree_fits_append_tree_to(s->tree, s->header, fid);
}
**/
int codetree_N(codetree_t* s) {
    return s->tree->ndata;
}

int codetree_nodes(codetree_t* s) {
    return s->tree->nnodes;
}

int codetree_D(codetree_t* s) {
    return s->tree->ndim;
}

qfits_header* codetree_header(codetree_t* s) {
    return s->header;
}

int codetree_get_permuted(codetree_t* s, int index) {
    if (s->tree->perm) return s->tree->perm[index];
    else return index;
}

static codetree_t* my_open(const char* fn, anqfits_t* fits) {
    codetree_t* s;
    kdtree_fits_t* io;
    char* treename = CODETREE_NAME;

    s = codetree_alloc();
    if (!s)
        return s;

    if (fits) {
        io = kdtree_fits_open_fits(fits);
        fn = fits->filename;
    } else
        io = kdtree_fits_open(fn);
    if (!io) {
        ERROR("Failed to open FITS file \"%s\"", fn);
        goto bailout;
    }
    if (!kdtree_fits_contains_tree(io, treename))
        treename = NULL;
    s->tree = kdtree_fits_read_tree(io, treename, &s->header);
    if (!s->tree) {
        ERROR("Failed to read code kdtree from file %s\n", fn);
        free(io); //# Modified by Robert Lancaster for the StellarSolver Internal Library, to prevent leak
        goto bailout;
    }

    // kdtree_fits_t is a typedef of fitsbin_t
    fitsbin_close_fd(io);

    return s;
 bailout:
    free(s);
    return NULL;
}

codetree_t* codetree_open_fits(anqfits_t* fits) {
    return my_open(NULL, fits);
}

codetree_t* codetree_open(const char* fn) {
    return my_open(fn, NULL);
}

int codetree_close(codetree_t* s) {
    if (!s) return 0;
    if (s->inverse_perm)
        free(s->inverse_perm);
    if (s->header)
        qfits_header_destroy(s->header);
    if (s->tree)
        kdtree_fits_close(s->tree);
    free(s);
    return 0;
}

static int Ndata(codetree_t* s) {
    return s->tree->ndata;
}

void codetree_compute_inverse_perm(codetree_t* s) {
    // compute inverse permutation vector.
    s->inverse_perm = malloc(Ndata(s) * sizeof(int));
    if (!s->inverse_perm) {
        debug("Failed to allocate code kdtree inverse permutation vector.\n"); //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging
        return;
    }
    kdtree_inverse_permutation(s->tree, s->inverse_perm);
}

int codetree_get(codetree_t* s, unsigned int codeid, double* code) {
    if (s->tree->perm && !s->inverse_perm) {
        codetree_compute_inverse_perm(s);
        if (!s->inverse_perm)
            return -1;
    }
    if (codeid >= Ndata(s)) {
        debug("Invalid code ID: %u >= %u.\n", codeid, Ndata(s)); //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging
        return -1;
    }
    if (s->inverse_perm)
        kdtree_copy_data_double(s->tree, s->inverse_perm[codeid], 1, code);
    else
        kdtree_copy_data_double(s->tree, codeid, 1, code);
    return 0;
}

codetree_t* codetree_new() {
    codetree_t* s = codetree_alloc();
    s->header = qfits_header_default();
    if (!s->header) {
        debug("Failed to create a qfits header for code kdtree.\n"); //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging
        free(s);
        return NULL;
    }
    qfits_header_add(s->header, "AN_FILE", AN_FILETYPE_CODETREE, "This file is a code kdtree.", NULL);
    return s;
}
/** //# Modified by Robert Lancaster for the StellarSolver Internal Library
int codetree_write_to_file(codetree_t* s, const char* fn) {
    return kdtree_fits_write(s->tree, fn, s->header);
}

int codetree_write_to_file_flipped(codetree_t* s, const char* fn) {
    return kdtree_fits_write_flipped(s->tree, fn, s->header);
}
**/
