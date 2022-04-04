#include "testdeletesolver.h"

//Includes for this project
#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "ssolverutils/fileio.h"

TestDeleteSolver::TestDeleteSolver()
{
    runCrashTest("pleiades.jpg");
    printf("No Crash test 1!\n");
    fflush( stdout );
    runCrashTestStarExtract("pleiades.jpg");
    printf("No Crash test 2!\n");
    fflush( stdout );
    runCrashTest("randomsky.fits");
    printf("No Crash test 3!\n");
    fflush( stdout );
    runCrashTestStarExtract("randomsky.fits");
    printf("No Crash test 4!\n");
    fflush( stdout );
    exit(0);
}

void TestDeleteSolver::runCrashTest(QString fileName)
{
    fileio imageLoader;
    if(!imageLoader.loadImage(fileName))
    {
        printf("Error in loading file");
        exit(1);
    }

    StellarSolver stellarSolver;
    stellarSolver.loadNewImageBuffer(imageLoader.getStats(), imageLoader.getImageBuffer());
    stellarSolver.setProperty("ProcessType", SSolver::SOLVE);
    stellarSolver.setParameterProfile(SSolver::Parameters::PARALLEL_SMALLSCALE);
    stellarSolver.setIndexFolderPaths(QStringList() << "astrometry");
    printf("Starting to solve. . .\n");
    fflush( stdout );

    stellarSolver.start();
    //Note: This could cause a crash because it will delete StellarSolver while the threads are running.
}

void TestDeleteSolver::runCrashTestStarExtract(QString fileName)
{
    fileio imageLoader;
    if(!imageLoader.loadImage(fileName))
    {
        printf("Error in loading file");
        exit(1);
    }

    StellarSolver stellarSolver;
    stellarSolver.loadNewImageBuffer(imageLoader.getStats(), imageLoader.getImageBuffer());
    stellarSolver.setProperty("ProcessType", SSolver::EXTRACT_WITH_HFR);
    stellarSolver.setParameterProfile(SSolver::Parameters::ALL_STARS);
    printf("Starting to extract. . .\n");
    fflush( stdout );

    stellarSolver.start();
    //Note: This could cause a crash because it will delete StellarSolver while the threads are running.
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
#if defined(__linux__)
    setlocale(LC_NUMERIC, "C");
#endif
    TestDeleteSolver *demo = new TestDeleteSolver();
    app.exec();

    delete demo;

    return 0;
}
