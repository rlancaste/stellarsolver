#include <QApplication>

//Includes for this project
#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "testerutils/fileio.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
#if defined(__linux__)
    setlocale(LC_NUMERIC, "C");
#endif
    fileio fileLoader;
    fileLoader.logToSignal = false;
    if(!fileLoader.loadImage("randomsky.fits"))
    {
        printf("Error in loading FITS file");
        exit(1);
    }
    FITSImage::Statistic stats = fileLoader.getStats();
    uint8_t *imageBuffer = fileLoader.getImageBuffer();

    StellarSolver *stellarSolver = new StellarSolver(stats, imageBuffer, nullptr);
    stellarSolver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
    stellarSolver->setProperty("ProcessType", SSolver::EXTRACT);
    stellarSolver->setParameterProfile(SSolver::Parameters::ALL_STARS);

    if(!stellarSolver->extract(false))
    {
        printf("Solver Failed");
        exit(0);
    }
    QList<FITSImage::Star> starList = stellarSolver->getStarList();
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    printf("Stars found: %u\n", starList.count());
    for(int i=0; i < starList.count(); i++)
    {
        FITSImage::Star star = starList.at(i);
        printf("Star #%u: (%f x, %f y), (%f a, %f b, %f theta), mag: %f, flux: %f, peak: %f \n ", i, star.x, star.y, star.a, star.b, star.theta, star.mag, star.flux, star.peak);
    }

    return 0;
}
