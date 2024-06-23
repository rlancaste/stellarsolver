/*  ExtractorSolver, StellarSolver Internal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

//Project Includes
#include "extractorsolver.h"

//Astrometry.net includes
extern "C" {
#include "astrometry/starutil.h"
}

using namespace SSolver;

ExtractorSolver::ExtractorSolver(ProcessType pType, ExtractorType eType, SolverType sType,
                                   const FITSImage::Statistic &statistics, uint8_t const *imageBuffer, QObject *parent) :
    QThread(parent), m_ProcessType(pType), m_ExtractorType(eType), m_SolverType(sType), m_Statistics(statistics),
    m_ImageBuffer(imageBuffer)
{
}

ExtractorSolver::~ExtractorSolver()
{

}

//This is a convenience function used to set all the scale parameters based on the FOV high and low values wit their units.
void ExtractorSolver::setSearchScale(double fov_low, double fov_high, ScaleUnits units)
{
    m_UseScale = true;
    //L
    scalelo = fov_low;
    //H
    scalehi = fov_high;
    //u
    scaleunit = units;
}

//This is a convenience function used to set all the search position parameters based on the ra, dec, and radius
//Warning!!  This method accepts the RA in degrees just like the DEC
void ExtractorSolver::setSearchPositionInDegrees(double ra, double dec)
{
    m_UsePosition = true;
    //3
    search_ra = ra;
    //4
    search_dec = dec;
}

double ExtractorSolver::convertToDegreeHeight(double scale)
{
    switch(scaleunit)
    {
        case DEG_WIDTH:
            return scale;
            break;
        case ARCMIN_WIDTH:
            return arcmin2deg(scale);
            break;
        case ARCSEC_PER_PIX:
            return arcsec2deg(scale) * (double) m_Statistics.height;
            break;
        case FOCAL_MM:
            return rad2deg(atan(36. / (2. * scale)));
            break;
        default:
            return scale;
            break;
    }
}

void ExtractorSolver::execute()
{
    run();
}
