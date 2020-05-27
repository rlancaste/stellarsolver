/*  Structure Definitions for SexySolver Internal Library, developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#ifndef STRUCTUREDEFINITIONS_H
#define STRUCTUREDEFINITIONS_H

//system includes
#include "stdint.h"
#include <QString>
#include <QVector>

//Astrometry.net includes
extern "C"{
#include "astrometry/blindutils.h"
#include "astrometry/log.h"
#include "astrometry/engine.h"
#include "astrometry/sip-utils.h"
}

#ifndef struct_statistic
#define struct_statistic
/// Stats struct to hold statisical data about the FITS data
typedef struct
{
    double min[3] = {0}, max[3] = {0};
    double mean[3] = {0};
    double stddev[3] = {0};
    double median[3] = {0};
    double SNR { 0 };
    /// FITS image data type (TBYTE, TUSHORT, TULONG, TFLOAT, TLONGLONG, TDOUBLE)
    uint32_t dataType { 0 };
    int bytesPerPixel { 1 };
    int ndim { 2 };
    int64_t size { 0 };
    uint32_t samples_per_channel { 0 };
    uint16_t width { 0 };
    uint16_t height { 0 };
} Statistic;
#endif

typedef struct
{
    float x;        //The x position of the star in Pixels
    float y;        //The y position of the star in Pixels
    float mag;      //The magnitude of the star
    float flux;     //The calculated total flux
    float peak;     //The peak value of the star
    float HFR;      //The half flux radius
    float a;        //The semi-major axis of the star
    float b;        //The semi-minor axis of the star
    float theta;    //The angle of orientation of the star
    float ra;       //The right ascension of the star
    float dec;      //The declination of the star
    QString rastr;       //The right ascension of the star
    QString decstr;      //The declination of the star
} Star;

typedef struct
{
    int bw, bh;        /* single tile width, height */
    float global;      /* global mean */
    float globalrms;   /* global sigma */
} Background;

typedef struct
{
    double fieldWidth;  //The Calculated width of the field
    double fieldHeight; //The Calculated height of the field
    double ra;          //The Right Ascension of the center of the field
    double dec;         //The Declination of the center of the field
    QString rastr;      //A String for The Right Ascension of the center of the field
    QString decstr;     //A string for The Declination of the center of the field
    double orientation; //The orientation angle of the image from North in degrees
    double pixscale;    //The pixel scale of the image
    QString parity;     //The parity of the image
    double raError;     //The error between the search_ra position and the solution ra position in "
    double decError;    //The error between the search_dec position and the solution dec position in "
} Solution;

#ifndef struct_wcs_point
#define struct_wcs_point
typedef struct
{
    float ra;
    float dec;
} wcs_point;
#endif

enum Shape{
    SHAPE_AUTO,
    SHAPE_CIRCLE,
    SHAPE_ELLIPSE
};

typedef enum{DEG_WIDTH,
             ARCMIN_WIDTH,
             ARCSEC_PER_PIX,
             FOCAL_MM
}ScaleUnits;

typedef enum {INT_SEP,
              INT_SEP_HFR,
              EXT_SEXTRACTOR,
              EXT_SEXTRACTOR_HFR,
              SEXYSOLVER,
              EXT_SEXTRACTORSOLVER,
              INT_SEP_EXT_SOLVER,
              CLASSIC_ASTROMETRY,
              ASTAP,
              INT_SEP_EXT_ASTAP,
              ONLINE_ASTROMETRY_NET,
              INT_SEP_ONLINE_ASTROMETRY_NET
}ProcessType;

typedef enum {NOT_MULTI,
              MULTI_SCALES,
              MULTI_DEPTHS,
              MULTI_AUTO
}MultiAlgo;

//SEXYSOLVER PARAMETERS
//These are the parameters used by the Internal SexySolver Sextractor and Internal SexySolver Astrometry Solver
//The values here are the defaults unless they get changed.
//If you are fine with those defaults, you don't need to set any of them.

typedef enum {
    FAST_SOLVING,
    PARALLEL_SOLVING,
    PARALLEL_LARGESCALE,
    PARALLEL_SMALLSCALE,
    ALL_STARS,
    SMALL_STARS,
    MID_STARS,
    BIG_STARS
} ParametersProfile;
struct Parameters
{
    QString listName = "Default";       //This is the name of this particular profile of options for SexySolver

    //Sextractor Photometry Parameters
    Shape apertureShape = SHAPE_CIRCLE; //Whether to use the SEP_SUM_ELLIPSE method or the SEP_SUM_CIRCLE method
    double kron_fact = 2.5;             //This sets the Kron Factor for use with the kron radius for flux calculations.
    int subpix = 5;                     //The subpix setting.  The instructions say to make it 5
    double r_min = 3.5;                  //The minimum radius for stars for flux calculations.
    short inflags;                      //Note sure if we need them?

    //Sextractor Extraction Parameters
    double magzero = 20;                 //This is the 'zero' magnitude used for settting the magnitude scale for the stars in the image during sextraction.
    double minarea = 5;                  //This is the minimum area in pixels for a star detection, smaller stars are ignored.
    int deblend_thresh = 32;            //The number of thresholds the intensity range is divided up into.
    double deblend_contrast = 0.005;         //The percentage of flux a separate peak must # have to be considered a separate object.
    int clean = 1;                      //Attempts to 'clean' the image to remove artifacts caused by bright objects
    double clean_param = 1;             //The cleaning parameter, not sure what it does.
    double fwhm = 2;                    //A variable to store the fwhm used to generate the conv filter, changing this WILL NOT change the conv filter, you can use the method below to create the conv filter based on the fwhm

    //This is the filter used for convolution. You can create this directly or use the convenience method below.
    QVector<float> convFilter= {0.260856, 0.483068, 0.260856,
                                0.483068, 0.894573, 0.483068,
                                0.260856, 0.483068, 0.260856};

    //Star Filter Parameters
    double maxSize = 0;                 //The maximum size of stars to include in the final list in pixels based on semi-major and semi-minor axes
    double minSize = 0;                 //The minimum size of stars to include in the final list in pixels based on semi-major and semi-minor axes
    double maxEllipse = 0;              //The maximum ratio between the semi-major and semi-minor axes for stars to include (a/b)
    double keepNum = 0;                 //The number of brightest stars to keep in the list
    double removeBrightest = 0;         //The percentage of brightest stars to remove from the list
    double removeDimmest = 0;           //The percentage of dimmest stars to remove from the list
    double saturationLimit = 0;         //Remove all stars above a certain threshhold percentage of saturation

    //Astrometry Config/Engine Parameters
    MultiAlgo multiAlgorithm = NOT_MULTI;//Algorithm for running multiple threads on possibly multiple cores to solve faster
    bool inParallel = true;             //Check the indices in parallel? if the indices you are using take less than 2 GB of space, and you have at least as much physical memory as indices, you want this enabled,
    int solverTimeLimit = 600;          //Give up solving after the specified number of seconds of CPU time
    double minwidth = 0.1;              //If no scale estimate is given, this is the limit on the minimum field width in degrees.
    double maxwidth = 180;              //If no scale estimate is given, this is the limit on the maximum field width in degrees.

    //Astrometry Basic Parameters
    bool resort = true;                 //Whether to resort the stars based on magnitude NOTE: This is REQUIRED to be true for the filters above
    int downsample = 1;                 //Factor to use for downsampling the image before SEP for plate solving.  Can speed it up.  Note: This should ONLY be used for SEP used for solving, not for Sextraction
    int search_parity = PARITY_BOTH;    //Only check for matches with positive/negative parity (default: try both)
    double search_radius = 15;          //Only search in indexes within 'radius' of the field center given by RA and DEC

    //LogOdds Settings
    double logratio_tosolve = log(1e9); //Odds ratio at which to consider a field solved (default: 1e9)
    double logratio_tokeep  = log(1e9); //Odds ratio at which to keep a solution (default: 1e9)
    double logratio_totune  = log(1e6); //Odds ratio at which to try tuning up a match that isn't good enough to solve (default: 1e6)

    //This allows you to compare the Parameters to see if they match
    bool operator==(const Parameters& o)
    {
        //We will skip the list name, since we want to see if the contents are the same.
        return  apertureShape == o.apertureShape &&
                kron_fact == o.kron_fact &&
                subpix == o.subpix &&
                r_min == o.r_min &&
                //skip inflags??  Not sure we even need them

                magzero == o.magzero &&
                deblend_thresh == o.deblend_thresh &&
                deblend_contrast == o.deblend_contrast &&
                clean == o.clean &&
                clean_param == o.clean_param &&
                fwhm == o.fwhm &&
                //skip conv filter?? This might be hard to compare

                maxSize == o.maxSize &&
                minSize == o.minSize &&
                maxEllipse == o.maxEllipse &&
                keepNum == o.keepNum &&
                removeBrightest == o.removeBrightest &&
                removeDimmest == o.removeDimmest &&
                saturationLimit == o.saturationLimit &&

                multiAlgorithm == o.multiAlgorithm &&
                inParallel == o.inParallel &&
                solverTimeLimit == o.solverTimeLimit &&
                minwidth == o.minwidth &&
                maxwidth == o.maxwidth &&

                resort == o.resort &&
                downsample == o.downsample &&
                search_parity == o.search_parity &&
                search_radius == o.search_radius &&
                //They need to be turned into a qstring because they are sometimes very close but not exactly the same
                QString::number(logratio_tosolve) == QString::number(o.logratio_tosolve) &&
                QString::number(logratio_tokeep) == QString::number(o.logratio_tokeep) &&
                QString::number(logratio_totune) == QString::number(o.logratio_totune);
    }
} ;



#endif // STRUCTUREDEFINITIONS_H
