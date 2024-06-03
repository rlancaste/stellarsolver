/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
* This file is part of SEP
*
* All content except array comparison functions fqcmp() and fqmedian() is
* distributed under an MIT license.
*
* Copyright 2014 SEP developers
*
* Array comparison functions fqcmp() and fqmedian() are distributed under an
* LGPL license:
*
* Copyright 1993-2011 Emmanuel Bertin -- IAP/CNRS/UPMC
* Copyright 2014 SEP developers
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sep.h"
#include "sepcore.h"

#define DETAILSIZE 512

//char *sep_version_string = "0.6.0";
//static char _errdetail_buffer[DETAILSIZE] = "";

/****************************************************************************/
/* data type conversion mechanics for runtime type conversion */

namespace SEP
{
PIXTYPE convert_dbl(void *ptr)
{
    return *(double *)ptr;
}

PIXTYPE convert_flt(void *ptr)
{
    return *(float *)ptr;
}

PIXTYPE convert_int(void *ptr)
{
    return *(int *)ptr;
}

PIXTYPE convert_byt(void *ptr)
{
    return *(BYTE *)ptr;
}

/* return the correct converter depending on the datatype code */
int get_converter(int dtype, converter *f, int *size)
{
    int status = RETURN_OK;

    if (dtype == SEP_TFLOAT)
    {
        *f = convert_flt;
        *size = sizeof(float);
    }
    else if (dtype == SEP_TINT)
    {
        *f = convert_int;
        *size = sizeof(int);
    }
    else if (dtype == SEP_TDOUBLE)
    {
        *f = convert_dbl;
        *size = sizeof(double);
    }
    else if (dtype == SEP_TBYTE)
    {
        *f = convert_byt;
        *size = sizeof(BYTE);
    }
    else
    {
        *f = NULL;
        *size = 0;
        status = ILLEGAL_DTYPE;
    }
    return status;
}

/* array conversions */
void convert_array_flt(void *ptr, int n, PIXTYPE *target)
{
    float *source = (float *)ptr;
    int i;
    for (i = 0; i < n; i++, source++)
        target[i] = *source;
}

void convert_array_dbl(void *ptr, int n, PIXTYPE *target)
{
    double *source = (double *)ptr;
    int i;
    for (i = 0; i < n; i++, source++)
        target[i] = *source;
}

void convert_array_int(void *ptr, int n, PIXTYPE *target)
{
    int *source = (int *)ptr;
    int i;
    for (i = 0; i < n; i++, source++)
        target[i] = *source;
}

void convert_array_byt(void *ptr, int n, PIXTYPE *target)
{
    BYTE *source = (BYTE *)ptr;
    int i;
    for (i = 0; i < n; i++, source++)
        target[i] = *source;
}

int get_array_converter(int dtype, array_converter *f, int *size)
{
    int status = RETURN_OK;

    if (dtype == SEP_TFLOAT)
    {
        *f = convert_array_flt;
        *size = sizeof(float);
    }
    else if (dtype == SEP_TBYTE)
    {
        *f = convert_array_byt;
        *size = sizeof(BYTE);
    }
    else if (dtype == SEP_TINT)
    {
        *f = convert_array_int;
        *size = sizeof(int);
    }
    else if (dtype == SEP_TDOUBLE)
    {
        *f = convert_array_dbl;
        *size = sizeof(double);
    }
    else
    {
        *f = NULL;
        *size = 0;
        status = ILLEGAL_DTYPE;
    }
    return status;
}


/****************************************************************************/
/* Copy a float array to various sorts of arrays */

void write_array_dbl(float *ptr, int n, void *target)
{
    double *t = (double *)target;
    int i;
    for (i = 0; i < n; i++, ptr++)
        t[i] = (double)(*ptr);
}

void write_array_int(float *ptr, int n, void *target)
{
    int *t = (int *)target;
    int i;
    for (i = 0; i < n; i++, ptr++)
        t[i] = (int)(*ptr + 0.5);
}

/* return the correct writer depending on the datatype code */
int get_array_writer(int dtype, array_writer *f, int *size)
{
    int status = RETURN_OK;

    if (dtype == SEP_TINT)
    {
        *f = write_array_int;
        *size = sizeof(int);
    }
    else if (dtype == SEP_TDOUBLE)
    {
        *f = write_array_dbl;
        *size = sizeof(double);
    }
    else
    {
        *f = NULL;
        *size = 0;
        status = ILLEGAL_DTYPE;
    }
    return status;
}

/* subtract a float array from arrays of various types */

void subtract_array_dbl(float *ptr, int n, void *target)
{
    double *t = (double *)target;
    int i;
    for (i = 0; i < n; i++, ptr++)
        t[i] -= (double)(*ptr);
}

void subtract_array_flt(float *ptr, int n, void *target)
{
    float *t = (float *)target;
    int i;
    for (i = 0; i < n; i++, ptr++)
        t[i] -= *ptr;
}

void subtract_array_int(float *ptr, int n, void *target)
{
    int *t = (int *)target;
    int i;
    for (i = 0; i < n; i++, ptr++)
        t[i] -= (int)(*ptr + 0.5);
}

/* return the correct subtractor depending on the datatype code */
int get_array_subtractor(int dtype, array_writer *f, int *size)
{
    int status = RETURN_OK;
    char errtext[80];

    if (dtype == SEP_TFLOAT)
    {
        *f = subtract_array_flt;
        *size = sizeof(float);
    }
    else if (dtype == SEP_TINT)
    {
        *f = subtract_array_int;
        *size = sizeof(int);
    }
    else if (dtype == SEP_TDOUBLE)
    {
        *f = subtract_array_dbl;
        *size = sizeof(double);
    }
    else
    {
        *f = NULL;
        *size = 0;
        status = ILLEGAL_DTYPE;
        snprintf(errtext, 40, "in get_array_subtractor(): %d", dtype); //# Modified by Robert Lancaster for the StellarSolver Internal Library to resolve warning
        // TODO
        //put_errdetail(errtext);
    }
    return status;
}

/*****************************************************************************/
/* Error messaging */

void sep_get_errmsg(int status, char *errtext)
/* Return a short descriptive error message that corresponds to the input
 * error status value.  The message may be up to 60 characters long, plus
 * the terminating null character. */
{
    errtext[0] = '\0';
    switch (status)
    {
        case RETURN_OK:
            strcpy(errtext, "OK - no error");
            break;
        case MEMORY_ALLOC_ERROR:
            strcpy(errtext, "memory allocation");
            break;
        case PIXSTACK_FULL:
            strcpy(errtext, "internal pixel buffer full");
            break;
        case DEBLEND_OVERFLOW:
            strcpy(errtext, "object deblending overflow");
            break;
        case ILLEGAL_DTYPE:
            strcpy(errtext, "dtype not recognized/unsupported");
            break;
        case ILLEGAL_SUBPIX:
            strcpy(errtext, "subpix value must be nonnegative");
            break;
        case NON_ELLIPSE_PARAMS:
            strcpy(errtext, "parameters do not describe ellipse");
            break;
        case ILLEGAL_APER_PARAMS:
            strcpy(errtext, "invalid aperture parameters");
            break;
        case LINE_NOT_IN_BUF:
            strcpy(errtext, "array line out of buffer");
            break;
        case RELTHRESH_NO_NOISE:
            strcpy(errtext, "relative threshold but image has noise_type of NONE");
            break;
        case UNKNOWN_NOISE_TYPE:
            strcpy(errtext, "image has unknown noise_type");
            break;
        default:
            strcpy(errtext, "unknown error status");
            break;
    }
}

//void sep_get_errdetail(char *errtext)
//{
//  strcpy(errtext, _errdetail_buffer);
//  memset(_errdetail_buffer, 0, DETAILSIZE);
//}

//void put_errdetail(char *errtext)
//{
//   strcpy(_errdetail_buffer, errtext);

//}

/*****************************************************************************/
/* Array median */

static int fqcmp(const void *p1, const void *p2)
/* Sorting function for floats, used in fqmedian() below.
 * Return value is 1 if *p1>*p2, 0 if *p1==*p2, -1 otherwise */
{
    double f1 = *((float *)p1);
    double f2 = *((float *)p2);
    return f1 > f2 ? 1 : (f1 < f2 ? -1 : 0);
}

float fqmedian(float *ra, int n)
/* Compute median of an array of floats.
 *
 * WARNING: input data are reordered! */
{
    int dqcmp(const void *p1, const void *p2);

    qsort(ra, n, sizeof(float), fqcmp);
    if (n < 2)
        return *ra;
    else
        return n & 1 ? ra[n / 2] : (ra[n / 2 - 1] + ra[n / 2]) / 2.0;
}

/********** addobjdeep (originally in manobjlist.c) **************************/
/*
Add object number `objnb` from list `objl1` to list `objl2`.
Unlike `addobjshallow` this also copies plist pixels to the second list.
*/

int addobjdeep(int objnb, objliststruct *objl1, objliststruct *objl2, int plistsize)
{
    objstruct	*objl2obj;
    pliststruct	*plist1 = objl1->plist, *plist2 = objl2->plist;
    int		fp, i, j, npx, objnb2;

    fp = objl2->npix;      /* 2nd list's plist size in pixels */
    j = fp * plistsize;    /* 2nd list's plist size in bytes */
    objnb2 = objl2->nobj;  /* # of objects currently in 2nd list*/

    /* Allocate space in `objl2` for the new object */
    if (objnb2)
        objl2obj = (objstruct *)realloc(objl2->obj,
                                        (++objl2->nobj) * sizeof(objstruct));
    else
        objl2obj = (objstruct *)malloc((++objl2->nobj) * sizeof(objstruct));

    if (!objl2obj)
        goto earlyexit;
    objl2->obj = objl2obj;

    /* Allocate space for the new object's pixels in 2nd list's plist */
    npx = objl1->obj[objnb].fdnpix;
    if (fp)
        plist2 = (pliststruct *)realloc(plist2, (objl2->npix += npx) * plistsize);
    else
        plist2 = (pliststruct *)malloc((objl2->npix = npx) * plistsize);

    if (!plist2)
        goto earlyexit;
    objl2->plist = plist2;

    /* copy the plist */
    plist2 += j;
    for(i = objl1->obj[objnb].firstpix; i != -1; i = PLIST(plist1 + i, nextpix))
    {
        memcpy(plist2, plist1 + i, (size_t)plistsize);
        PLIST(plist2, nextpix) = (j += plistsize);
        plist2 += plistsize;
    }
    PLIST(plist2 -= plistsize, nextpix) = -1;

    /* copy the object itself */
    objl2->obj[objnb2] = objl1->obj[objnb];
    objl2->obj[objnb2].firstpix = fp * plistsize;
    objl2->obj[objnb2].lastpix = j - plistsize;

    return RETURN_OK;

    /* if early exit, reset 2nd list */
earlyexit:
    objl2->nobj--;
    objl2->npix = fp;
    return MEMORY_ALLOC_ERROR;
}

}
