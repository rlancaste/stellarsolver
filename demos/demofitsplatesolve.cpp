#include "demofitsplatesolve.h"
#include <QImageReader>
//Includes for this project
#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "externalsextractorsolver.h"
#include "onlinesolver.h"
#include "testerutils/fileio.h"

DemoFITSPlateSolve::DemoFITSPlateSolve()
{
    fileio imageLoader;
    imageLoader.logToSignal = false;
    if(!imageLoader.loadFits("randomsky.fits"))
    {
        printf("Error in loading FITS file");
        exit(1);
    }
    FITSImage::Statistic stats = imageLoader.getStats();
    uint8_t *imageBuffer = imageLoader.getImageBuffer();

    StellarSolver *stellarSolver = new StellarSolver(stats, imageBuffer, nullptr);
    stellarSolver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
    stellarSolver->setProperty("SolverType", SSolver::SOLVER_STELLARSOLVER);
    stellarSolver->setProperty("ProcessType", SSolver::SOLVE);
    stellarSolver->setParameterProfile(SSolver::Parameters::PARALLEL_SMALLSCALE);
    stellarSolver->setIndexFolderPaths(QStringList()<<"/Users/rlancaste/Linux Share/Astrometry");

    if(imageLoader.position_given)
    {
        printf("Using Position: %f hours, %f degrees\n", imageLoader.ra, imageLoader.dec);
        stellarSolver->setSearchPositionRaDec(imageLoader.ra, imageLoader.dec);
    }
    if(imageLoader.scale_given)
    {
        stellarSolver->setSearchScale(imageLoader.scale_low, imageLoader.scale_high, imageLoader.scale_units);
        printf("Using Scale: %f to %f, %s\n", imageLoader.scale_low, imageLoader.scale_high, SSolver::getScaleUnitString(imageLoader.scale_units).toUtf8().data());
    }

    printf("Starting to solve. . .\n");

    fflush( stdout );
    QEventLoop loop;
    connect(stellarSolver, &StellarSolver::finished, &loop, &QEventLoop::quit);
    stellarSolver->start();
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    FITSImage::Solution solution = stellarSolver->getSolution();
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    printf("Field center: (RA,Dec) = (%f, %f) deg.\n", solution.ra, solution.dec);
    printf("Field size: %f x %f arcminutes\n", solution.fieldWidth, solution.fieldHeight);
    printf("Pixel Scale: %f\"\n", solution.pixscale);
    printf("Field rotation angle: up is %f degrees E of N\n", solution.orientation);
    printf("Field parity: %s\n\n", solution.parity.toUtf8().data());
    exit(0);
}


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
#if defined(__linux__)
    setlocale(LC_NUMERIC, "C");
#endif
    DemoFITSPlateSolve *demo = new DemoFITSPlateSolve();
    app.exec();

    delete demo;

    return 0;
}
