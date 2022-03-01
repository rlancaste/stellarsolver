#include <QApplication>

//Includes for this project
#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "testerutils/fileio.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    StellarSolver stellarSolver;
    stellarSolver.setIndexFolderPaths(QStringList() << "astrometry");
    FITSImage::Solution solution;
    fileio imageLoader;
    imageLoader.logToSignal = false;


    if(!imageLoader.loadImage("pleiades.jpg"))
    {
        printf("Error in loading FITS file");
        exit(1);
    }

    stellarSolver.loadNewImageBuffer(imageLoader.getStats(), imageLoader.getImageBuffer());
    printf("Starting to blindly solve pleiades.jpg. . .\n");
    fflush( stdout );
    if(!stellarSolver.solve())
    {
        printf("Solver Failed");
        exit(0);
    }

   solution = stellarSolver.getSolution();
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    printf("Field center: (RA,Dec) = (%f, %f) deg.\n", solution.ra, solution.dec);
    printf("Field size: %f x %f arcminutes\n", solution.fieldWidth, solution.fieldHeight);
    printf("Pixel Scale: %f\"\n", solution.pixscale);
    printf("Field rotation angle: up is %f degrees E of N\n", solution.orientation);
    printf("Field parity: %s\n\n", FITSImage::getParityText(solution.parity).toUtf8().data());


    if(!imageLoader.loadImage("randomsky.fits"))
    {
        printf("Error in loading FITS file");
        exit(1);
    }

    stellarSolver.loadNewImageBuffer(imageLoader.getStats(), imageLoader.getImageBuffer());
    printf("Starting to blindly solve randomsky.fits. . .\n");
    fflush( stdout );
    if(!stellarSolver.solve())
    {
        printf("Solver Failed");
        exit(0);
    }

    solution = stellarSolver.getSolution();
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    printf("Field center: (RA,Dec) = (%f, %f) deg.\n", solution.ra, solution.dec);
    printf("Field size: %f x %f arcminutes\n", solution.fieldWidth, solution.fieldHeight);
    printf("Pixel Scale: %f\"\n", solution.pixscale);
    printf("Field rotation angle: up is %f degrees E of N\n", solution.orientation);
    printf("Field parity: %s\n\n", FITSImage::getParityText(solution.parity).toUtf8().data());


    return 0;
}
