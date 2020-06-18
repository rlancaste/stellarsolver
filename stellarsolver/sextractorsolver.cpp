/*  SextractorSolver, StellarSolver Internal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#include "sextractorsolver.h"

using namespace SSolver;


SextractorSolver::SextractorSolver(ProcessType type, FITSImage::Statistic imagestats, uint8_t const *imageBuffer, QObject *parent) : QThread(parent)
{
    processType = type;
    stats=imagestats;
    m_ImageBuffer=imageBuffer;
}

SextractorSolver::~SextractorSolver()
{

}

//This is a convenience function used to set all the scale parameters based on the FOV high and low values wit their units.
void SextractorSolver::setSearchScale(double fov_low, double fov_high, ScaleUnits units)
{
    use_scale = true;
    //L
    scalelo = fov_low;
    //H
    scalehi = fov_high;
    //u
    scaleunit = units;
}

//This is a convenience function used to set all the search position parameters based on the ra, dec, and radius
//Warning!!  This method accepts the RA in degrees just like the DEC
void SextractorSolver::setSearchPositionInDegrees(double ra, double dec)
{
    use_position = true;
    //3
    search_ra = ra;
    //4
    search_dec = dec;
}

void SextractorSolver::startProcess()
{
    start();
}

void SextractorSolver::executeProcess()
{
    run();
}
