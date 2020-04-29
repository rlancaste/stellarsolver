/*  Structure Definitions for SexySolver Library, developed by Robert Lancaster, 2020

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

/// Stats struct to hold statisical data about the FITS data
typedef struct
{
    double min[3] = {0}, max[3] = {0};
    double mean[3] = {0};
    double stddev[3] = {0};
    double median[3] = {0};
    double SNR { 0 };
    /// FITS image data type (SEP_TBYTE, TUSHORT, TULONG, TFLOAT, TLONGLONG, TDOUBLE)
    uint32_t dataType { 0 };
    int bytesPerPixel { 1 };
    int ndim { 2 };
    int64_t size { 0 };
    uint32_t samples_per_channel { 0 };
    uint16_t width { 0 };
    uint16_t height { 0 };
} Statistic;

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
} Star;

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

enum Shape{
    SHAPE_AUTO,
    SHAPE_CIRCLE,
    SHAPE_ELLIPSE
};

#endif // STRUCTUREDEFINITIONS_H
