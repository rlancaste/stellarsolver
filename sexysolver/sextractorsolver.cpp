/*  SextractorSolver, SexySolver Internal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#include "sextractorsolver.h"
#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/stat.h>
#elif defined(_WIN32)
#include "windows.h"
#else //Linux
#include <QProcess>
#include <sys/stat.h>
#endif

SextractorSolver::SextractorSolver(ProcessType type, Statistic imagestats, uint8_t *imageBuffer, QObject *parent) : QThread(parent)
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

//This function should get the system RAM in bytes.  I may revise it later to get the currently available RAM
//But from what I read, getting the Available RAM is inconsistent and buggy on many systems.
uint64_t SextractorSolver::getAvailableRAM()
{
    uint64_t RAM = 0;

#if defined(Q_OS_OSX)
    int mib[2];
    size_t length;
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    length = sizeof(int64_t);
    if(sysctl(mib, 2, &RAM, &length, NULL, 0))
        return 0; // On Error
#elif defined(Q_OS_LINUX)
    QProcess p;
    p.start("awk", QStringList() << "/MemTotal/ { print $2 }" << "/proc/meminfo");
    p.waitForFinished();
    QString memory = p.readAllStandardOutput();
    RAM = memory.toLong() * 1024; //It is in kB on this system
    p.close();
#else
    MEMORYSTATUSEX memory_status;
    ZeroMemory(&memory_status, sizeof(MEMORYSTATUSEX));
    memory_status.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memory_status)) {
      RAM = memory_status.ullTotalPhys;
    } else {
      RAM = 0;
    }
#endif
    return RAM;
}

//This should determine if enough RAM is available to load all the index files in parallel
bool SextractorSolver::enoughRAMisAvailableFor(QStringList indexFolders)
{
    uint64_t totalSize = 0;

    foreach(QString folder, indexFolders)
    {
        QDir dir(folder);
        if(dir.exists())
        {
            dir.setNameFilters(QStringList()<<"*.fits"<<"*.fit");
            QFileInfoList indexInfoList = dir.entryInfoList();
            foreach(QFileInfo indexInfo, indexInfoList)
                totalSize += indexInfo.size();
        }

    }
    uint64_t availableRAM = getAvailableRAM();
    if(availableRAM == 0)
    {
        emit logOutput("Unable to determine system RAM for inParallel Option");
        return false;
    }
    float bytesInGB = 1024 * 1024 * 1024; // B -> KB -> MB -> GB , float to make sure it reports the answer with any decimals
    emit logOutput(QString("Evaluating Installed RAM for inParallel Option.  Total Size of Index files: %1 GB, Installed RAM: %2 GB").arg(totalSize / bytesInGB).arg(availableRAM / bytesInGB));
    return availableRAM > totalSize;
}
