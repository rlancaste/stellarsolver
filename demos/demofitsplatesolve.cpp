#include "demofitsplatesolve.h"
#include <QImageReader>
//Includes for this project
#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "externalsextractorsolver.h"
#include "onlinesolver.h"


DemoFITSPlateSolve::DemoFITSPlateSolve()
{
    QImage imageFromFile;
    FITSImage::Statistic stats;
    uint32_t imageBufferSize { 0 };
    uint8_t *imageBuffer { nullptr };

    printf("Loading image. . .\n");
fflush( stdout );
    if(!imageFromFile.load("pleiades.jpg"))
    {
        printf("Failed to open image.\n");
        exit(1);
    }

    imageFromFile = imageFromFile.convertToFormat(QImage::Format_RGB32);

    stats.dataType      = SEP_TBYTE;
    stats.bytesPerPixel = sizeof(uint8_t);
    stats.width = static_cast<uint16_t>(imageFromFile.width());
    stats.height = static_cast<uint16_t>(imageFromFile.height());
    stats.channels = 3;
    stats.samples_per_channel = stats.width * stats.height;
    imageBufferSize = stats.samples_per_channel * stats.channels * static_cast<uint16_t>(stats.bytesPerPixel);
    imageBuffer = new uint8_t[imageBufferSize];
    auto debayered_buffer = reinterpret_cast<uint8_t *>(imageBuffer);
    auto * original_bayered_buffer = reinterpret_cast<uint8_t *>(imageFromFile.bits());

    // Data in RGB32, with bytes in the order of B,G,R,A, we need to copy them into 3 layers for FITS
    uint8_t * rBuff = debayered_buffer;
    uint8_t * gBuff = debayered_buffer + (stats.width * stats.height);
    uint8_t * bBuff = debayered_buffer + (stats.width * stats.height * 2);

    int imax = stats.samples_per_channel * 4 - 4;
    for (int i = 0; i <= imax; i += 4)
    {
        *rBuff++ = original_bayered_buffer[i + 2];
        *gBuff++ = original_bayered_buffer[i + 1];
        *bBuff++ = original_bayered_buffer[i + 0];
    }
    StellarSolver *stellarSolver = new StellarSolver(stats, imageBuffer, nullptr);
    stellarSolver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
    stellarSolver->setProperty("SolverType", SSolver::SOLVER_STELLARSOLVER);
    stellarSolver->setProperty("ProcessType", SSolver::SOLVE);
    stellarSolver->setParameterProfile(SSolver::Parameters::PARALLEL_SMALLSCALE);
    stellarSolver->setIndexFolderPaths(QStringList()<<"/Users/rlancaste/Linux Share/Astrometry");

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
