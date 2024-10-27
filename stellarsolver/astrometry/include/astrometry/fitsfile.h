/*
 # This file is part of the Astrometry.net suite.
 # Licensed under a 3-clause BSD style license - see LICENSE
 */

#ifndef FITSFILE_H
#define FITSFILE_H

#include <stdio.h>

#ifdef _MSC_VER //# Modified by Robert Lancaster for the StellarSolver Internal Library
#include "sys/types.h"
#endif

#include "astrometry/qfits_header.h"

int fitsfile_pad_with(FILE* fid, char pad);

//int fitsfile_write_primary_header(FILE* fid, qfits_header* hdr, off_t* end_offset, const char* fn); //# Modified by Robert Lancaster for the StellarSolver Internal Library

int fitsfile_fix_primary_header(FILE* fid, qfits_header* hdr,
                                off_t* end_offset, const char* fn);

// set ext = -1 if unknown.
//int fitsfile_write_header(FILE* fid, qfits_header* hdr, off_t* start_offset, off_t* end_offset, int ext, const char* fn); //# Modified by Robert Lancaster for the StellarSolver Internal Library

// set ext = -1 if unknown.
//int fitsfile_fix_header(FILE* fid, qfits_header* hdr, off_t* start_offset, off_t* end_offset, int ext, const char* fn); //# Modified by Robert Lancaster for the StellarSolver Internal Library



#endif

