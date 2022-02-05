/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
* This file is part of SEP
*
* Copyright 1993-2011 Emmanuel Bertin -- IAP/CNRS/UPMC
* Copyright 2014 SEP developers
*
* SEP is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* SEP is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with SEP.  If not, see <http://www.gnu.org/licenses/>.
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#pragma once

#include <cstring>
#include <memory>

namespace SEP
{
#define	RETURN_OK           0  /* must be zero */
#define MEMORY_ALLOC_ERROR  1
#define PIXSTACK_FULL       2
#define ILLEGAL_DTYPE       3
#define ILLEGAL_SUBPIX      4
#define NON_ELLIPSE_PARAMS  5
#define ILLEGAL_APER_PARAMS 6
#define DEBLEND_OVERFLOW    7
#define LINE_NOT_IN_BUF     8
#define RELTHRESH_NO_NOISE  9
#define UNKNOWN_NOISE_TYPE  10

#define	BIG 1e+30f  /* a huge number (< biggest value a float can store) */
#define	PI  3.1415926535898
#define	DEG (PI/180.0)	    /* 1 deg in radians */

typedef	int	      LONG;
typedef	unsigned int  ULONG;
typedef	unsigned char BYTE;    /* a byte */

/* keep these synchronized */
typedef float         PIXTYPE;    /* type used inside of functions */
#define PIXDTYPE      SEP_TFLOAT  /* dtype code corresponding to PIXTYPE */


/* signature of converters */
typedef PIXTYPE (*converter)(void *ptr);
typedef void (*array_converter)(void *ptr, int n, PIXTYPE *target);
typedef void (*array_writer)(float *ptr, int n, void *target);

#define	QCALLOC(ptr, typ, nel, status)				     	\
  {if (!(ptr = (typ *)calloc((size_t)(nel),sizeof(typ))))		\
      {									\
    status = MEMORY_ALLOC_ERROR;					\
    goto exit;							\
      };								\
  }

#define	QMALLOC(ptr, typ, nel, status)					\
  {if (!(ptr = (typ *)malloc((size_t)(nel)*sizeof(typ))))		\
      {									\
    status = MEMORY_ALLOC_ERROR;					\
    goto exit;							\
      };								\
  }

#define	UNKNOWN	        -1    /* flag for LUTZ */
#define	CLEAN_ZONE      10.0  /* zone (in sigma) to consider for processing */
#define CLEAN_STACKSIZE 3000  /* replaces prefs.clean_stacksize  */
/* (MEMORY_OBJSTACK in sextractor inputs) */
#define CLEAN_MARGIN    0  /* replaces prefs.cleanmargin which was set based */
/* on stuff like apertures and vignet size */
#define	MARGIN_SCALE   2.0 /* Margin / object height */
#define	MARGIN_OFFSET  4.0 /* Margin offset (pixels) */
#define	MAXDEBAREA     3   /* max. area for deblending (must be >= 1)*/
#define	MAXPICSIZE     1048576 /* max. image size in any dimension */

/* plist-related macros */
#define	PLIST(ptr, elem)	(((pbliststruct *)(ptr))->elem)
#define	PLISTEXIST(elem)	(plistexist_##elem)
#define	PLISTPIX(ptr, elem)	(*((PIXTYPE *)((ptr)+plistoff_##elem)))
#define	PLISTFLAG(ptr, elem)	(*((FLAGTYPE *)((ptr)+plistoff_##elem)))

#define DETECT_MAXAREA 0             /* replaces prefs.ext_maxarea */
#define	WTHRESH_CONVFAC	1e-4         /* Factor to apply to weights when */
/* thresholding filtered weight-maps */

/* Extraction status */
typedef	enum {COMPLETE, INCOMPLETE, NONOBJECT, OBJECT} pixstatus;

/* Temporary object parameters during extraction */
typedef struct
{
    LONG	pixnb;	    /* Number of pixels included */
    LONG	firstpix;   /* Pointer to first pixel of pixlist */
    LONG	lastpix;    /* Pointer to last pixel of pixlist */
    short	flag;	    /* Extraction flag */
} infostruct;

typedef	char pliststruct;      /* Dummy type for plist */

typedef struct
{
    int     nextpix;
    int     x, y;
    PIXTYPE value;
} pbliststruct;

typedef struct
{
    int plistexist_cdvalue;
    int plistexist_thresh;
    int plistexist_var;
    int plistoff_value;
    int plistoff_cdvalue;
    int plistoff_thresh;
    int plistoff_var;
    int plistsize;
} plistvalues;

/* array buffer struct */
typedef struct
{
    BYTE *dptr;         /* pointer to original data, can be any supported type */
    int dtype;          /* data type of original data */
    int dw, dh;         /* original data width, height */
    PIXTYPE *bptr;      /* buffer pointer (self-managed memory) */
    int bw, bh;         /* buffer width, height (bufw can be larger than w due */
    /* to padding). */
    PIXTYPE *midline;   /* "middle" line in buffer (at index bh/2) */
    PIXTYPE *lastline;  /* last line in buffer */
    array_converter readline;  /* function to read a data line into buffer */
    int elsize;         /* size in bytes of one element in original data */
    int yoff;           /* line index in original data corresponding to bufptr */
} arraybuffer;

typedef struct
{
    /* thresholds */
    float	   thresh;		             /* detect threshold (ADU) */
    float	   mthresh;		             /* max. threshold (ADU) */

    /* # pixels */
    int	   fdnpix;		       	/* nb of extracted pix */
    int	   dnpix;	       		/* nb of pix above thresh  */
    int	   npix;       			/* "" in measured frame */
    int	   nzdwpix;			/* nb of zero-dweights around */
    int	   nzwpix;		       	/* nb of zero-weights inside */

    /* position */
    int	   xpeak, ypeak;                     /* pos of brightest pix */
    int	   xcpeak, ycpeak;                   /* pos of brightest pix */
    double   mx, my;        	             /* barycenter */
    int	   xmin, xmax, ymin, ymax, ycmin, ycmax; /* x,y limits */

    /* shape */
    double   mx2, my2, mxy;			   /* variances and covariance */
    float	   a, b, theta, abcor;		     /* moments and angle */
    float	   cxx, cyy, cxy;			   /* ellipse parameters */
    double   errx2, erry2, errxy;      /* Uncertainties on the variances */

    /* flux */
    float	   fdflux;	       		/* integrated ext. flux */
    float	   dflux;      			/* integrated det. flux */
    float	   flux;       			/* integrated mes. flux */
    float	   fluxerr;			/* integrated variance */
    PIXTYPE  fdpeak;	       		/* peak intensity (ADU) */
    PIXTYPE  dpeak;      			/* peak intensity (ADU) */
    PIXTYPE  peak;       			/* peak intensity (ADU) */

    /* flags */
    short	   flag;			     /* extraction flags */

    /* accessing individual pixels in plist*/
    int	   firstpix;			     /* ptr to first pixel */
    int	   lastpix;			     /* ptr to last pixel */
} objstruct;

typedef struct
{
    int           nobj;	  /* number of objects in list */
    objstruct     *obj;	  /* pointer to the object array */
    int           npix;	  /* number of pixels in pixel-list */
    pliststruct   *plist;	  /* pointer to the pixel-list */
    PIXTYPE       thresh;   /* detection threshold */
} objliststruct;

float fqmedian(float *ra, int n);
void put_errdetail(char *errtext);

int get_converter(int dtype, converter *f, int *size);
int get_array_converter(int dtype, array_converter *f, int *size);
int get_array_writer(int dtype, array_writer *f, int *size);
int get_array_subtractor(int dtype, array_writer *f, int *size);

int addobjdeep(int objnb, objliststruct *objl1, objliststruct *objl2, int plistsize);

int convolve(arraybuffer *buf, int y, float *conv, int convw, int convh, PIXTYPE *out);
int matched_filter(arraybuffer *imbuf, arraybuffer *nbuf, int y, float *conv, int convw, int convh,
                   PIXTYPE *work, PIXTYPE *out, int noise_type);

}
