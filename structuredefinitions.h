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
    int bitpix { 8 };
    int bytesPerPixel { 1 };
    int ndim { 2 };
    int64_t size { 0 };
    uint32_t samples_per_channel { 0 };
    uint16_t width { 0 };
    uint16_t height { 0 };
} Statistic;

typedef struct
{
    float x;
    float y;
    float mag;
    float flux;
} Star;

typedef struct
{
    double fieldWidth;
    double fieldHeight;
    double ra;
    double dec;
    QString rastr;
    QString decstr;
    double orientation;

} Solution;

#endif // STRUCTUREDEFINITIONS_H
