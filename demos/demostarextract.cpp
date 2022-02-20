#include "demostarextract.h"
#include <QImageReader>
//Includes for this project
#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "externalsextractorsolver.h"
#include "onlinesolver.h"
#include "testerutils/fileio.h"

DemoStarExtract::DemoStarExtract()
{
    fileio imageLoader;
    imageLoader.logToSignal = false;
    if(!imageLoader.loadImage("pleiades.jpg"))
    {
        printf("Error in loading FITS file");
        exit(1);
    }
    FITSImage::Statistic stats = imageLoader.getStats();
    uint8_t *imageBuffer = imageLoader.getImageBuffer();

    StellarSolver *stellarSolver = new StellarSolver(stats, imageBuffer, nullptr);
    stellarSolver->setParameterProfile(SSolver::Parameters::ALL_STARS);

    if(!stellarSolver->extract(false))
    {
        printf("Solver Failed");
        exit(0);
    }
    QList<FITSImage::Star> starList = stellarSolver->getStarList();
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    printf("Stars found: %u", starList.count());
    for(int i=0; i < starList.count(); i++)
    {
        FITSImage::Star star = starList.at(i);
        printf("Star #%u: (%f x, %f y), (%f a, %f b, %f theta), mag: %f, flux: %f, peak: %f \n ", i, star.x, star.y, star.a, star.b, star.theta, star.mag, star.flux, star.peak);
    }
    exit(0);
}


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
#if defined(__linux__)
    setlocale(LC_NUMERIC, "C");
#endif
    DemoStarExtract *demo = new DemoStarExtract();
    app.exec();

    delete demo;

    return 0;
}
