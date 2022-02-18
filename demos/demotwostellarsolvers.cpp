#include "demotwostellarsolvers.h"
#include <QImageReader>
//Includes for this project
#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "externalsextractorsolver.h"
#include "onlinesolver.h"
#include "testerutils/fileio.h"

DemoTwoStellarSolvers::DemoTwoStellarSolvers()
{
    //Setting up simultaneous solvers.  Not usually recommended, but it does work.
    solversRunning = 2;
    setupStellarSolver("randomsky.fits");
    setupStellarSolver("pleiades.jpg");
}

void DemoTwoStellarSolvers::setupStellarSolver(QString fileName)
{
    fileio imageLoader;
    bool success = false;
    if(fileName.endsWith(".fits"))
        success = imageLoader.loadFits(fileName);
    else
        success = imageLoader.loadOtherFormat(fileName);
    if(!success)
    {
        printf("Error in loading file");
        exit(1);
    }
    FITSImage::Statistic stats = imageLoader.getStats();
    uint8_t *imageBuffer = imageLoader.getImageBuffer();

    StellarSolver *stellarSolver = new StellarSolver(stats, imageBuffer, nullptr);
    stellarSolver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
    stellarSolver->setProperty("SolverType", SSolver::SOLVER_STELLARSOLVER);
    stellarSolver->setProperty("ProcessType", SSolver::SOLVE);
    stellarSolver->setParameterProfile(SSolver::Parameters::SINGLE_THREAD_SOLVING);
    stellarSolver->setIndexFolderPaths(QStringList() << "astrometry");
    stellarSolver->setLogLevel(SSolver::LOG_ALL);
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

    connect(stellarSolver, &StellarSolver::finished, this, &DemoTwoStellarSolvers::stellarSolverFinished);
    connect(stellarSolver, &StellarSolver::logOutput, this, &DemoTwoStellarSolvers::logOutput);
    stellarSolver->start();

}

void DemoTwoStellarSolvers::stellarSolverFinished()
{
    solversRunning --;
    StellarSolver *reportingSolver = qobject_cast<StellarSolver*>(sender());
    FITSImage::Solution solution = reportingSolver->getSolution();
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    printf("Field center: (RA,Dec) = (%f, %f) deg.\n", solution.ra, solution.dec);
    printf("Field size: %f x %f arcminutes\n", solution.fieldWidth, solution.fieldHeight);
    printf("Pixel Scale: %f\"\n", solution.pixscale);
    printf("Field rotation angle: up is %f degrees E of N\n", solution.orientation);
    printf("Field parity: %s\n", solution.parity.toUtf8().data());
    fflush( stdout );
    if(solversRunning == 0)
        exit(0);
}

void DemoTwoStellarSolvers::logOutput(QString text)
{
    printf("%s \n", text.toUtf8().data());
}


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
#if defined(__linux__)
    setlocale(LC_NUMERIC, "C");
#endif
    DemoTwoStellarSolvers *demo = new DemoTwoStellarSolvers();
    app.exec();

    delete demo;

    return 0;
}
