#include "testmultiplesyncsolvers.h"
#include <unistd.h>

TestMultipleSyncSolvers::TestMultipleSyncSolvers()
{
    //Setting up simultaneous solvers.  Not usually recommended, but it does work.
    extractorsRunning = 0;
    solversRunning = 0;
    int pairsToRun = 100;
    FITSImage::Statistic stats1;
    FITSImage::Statistic stats2;
    uint8_t *imageBuffer[pairsToRun * 2];
    for(int i=0; i<pairsToRun; i++)
    {
        QString fileName1 = "randomsky.fits";
        QString fileName2 = "pleiades.jpg";
        imageBuffer[i*2] = loadImageBuffer(stats1, fileName1);
        imageBuffer[i*2 + 1] = loadImageBuffer(stats2, fileName2);
    }
    for(int i=0; i<pairsToRun; i++)
    {
        extractorsRunning += 2;
        solversRunning += 2;
        QtConcurrent::run(this, &TestMultipleSyncSolvers::runSynchronousSEP, stats1, imageBuffer[i*2]);
        QtConcurrent::run(this, &TestMultipleSyncSolvers::runSynchronousSEP, stats2, imageBuffer[i*2 + 1]);
        QtConcurrent::run(this, &TestMultipleSyncSolvers::runSynchronousSolve, stats1, imageBuffer[i*2]);
        QtConcurrent::run(this, &TestMultipleSyncSolvers::runSynchronousSolve, stats2, imageBuffer[i*2 + 1]);
    }
    while(extractorsRunning > 0 || solversRunning > 0)
    {
        usleep(1000);
    }
    exit(0);
}

uint8_t* TestMultipleSyncSolvers::loadImageBuffer(FITSImage::Statistic &stats, QString fileName)
{
    fileio imageLoader;
    if(!imageLoader.loadImage(fileName))
    {
        printf("Error in loading file");
        exit(1);
    }
    stats = imageLoader.getStats();
    return imageLoader.getImageBuffer();
}

void TestMultipleSyncSolvers::runSynchronousSolve(FITSImage::Statistic &stats, uint8_t *imageBuffer)
{

    StellarSolver *stellarSolver = new StellarSolver(stats, imageBuffer, nullptr);
    stellarSolver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
    stellarSolver->setProperty("SolverType", SSolver::SOLVER_STELLARSOLVER);
    stellarSolver->setProperty("ProcessType", SSolver::SOLVE);
    stellarSolver->setParameterProfile(SSolver::Parameters::SINGLE_THREAD_SOLVING);
    stellarSolver->setIndexFolderPaths(QStringList() << "astrometry");

    printf("Starting to solve. . .\n");
    fflush( stdout );

    stellarSolver->solve();

    FITSImage::Solution solution = stellarSolver->getSolution();
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    printf("Field center: (RA,Dec) = (%f, %f) deg.\n", solution.ra, solution.dec);
    printf("Field size: %f x %f arcminutes\n", solution.fieldWidth, solution.fieldHeight);
    printf("Pixel Scale: %f\"\n", solution.pixscale);
    printf("Field rotation angle: up is %f degrees E of N\n", solution.orientation);
    printf("Field parity: %s\n", FITSImage::getParityText(solution.parity).toUtf8().data());
    fflush( stdout );
    solversRunning--;
    delete stellarSolver;
    stellarSolver = nullptr;
}

void TestMultipleSyncSolvers::runSynchronousSEP(FITSImage::Statistic &stats, uint8_t *imageBuffer)
{
    StellarSolver *stellarSolver = new StellarSolver(stats, imageBuffer, nullptr);

    printf("Starting to extract. . .\n");
    fflush( stdout );

    if(stellarSolver->extract(true))
    {
        QList<FITSImage::Star> starList = stellarSolver->getStarList();
        printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
        printf("Stars found: %u\n", starList.count());
        for(int i=0; i < starList.count(); i++)
        {
            FITSImage::Star star = starList.at(i);
            printf("Star #%u: (%f x, %f y), mag: %f, peak: %f, hfr: %f \n", i, star.x, star.y, star.mag, star.peak, star.HFR);
        }
    }
    else
    {
         printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
         printf("Star Extraction Failed");
    }
    fflush( stdout );
    extractorsRunning--;
    delete stellarSolver;
    stellarSolver = nullptr;
}

void TestMultipleSyncSolvers::logOutput(QString text)
{
    printf("%s \n", text.toUtf8().data());
}


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
#if defined(__linux__)
    setlocale(LC_NUMERIC, "C");
#endif
    TestMultipleSyncSolvers *demo = new TestMultipleSyncSolvers();
    app.exec();

    delete demo;

    return 0;
}
