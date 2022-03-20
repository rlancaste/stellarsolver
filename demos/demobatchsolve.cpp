#include "demobatchsolve.h"
#include "ui_demobatchsolve.h"
#include <QFileDialog>

DemoBatchSolve::DemoBatchSolve():
    QMainWindow(),
    ui(new Ui::DemoBatchSolve)
{
    ui->setupUi(this);
    connect(ui->loadB, &QPushButton::clicked, this, &DemoBatchSolve::addImages);
    connect(ui->clearImagesB, &QPushButton::clicked, this, &DemoBatchSolve::removeAllImages);
    connect(ui->deleteImageB, &QPushButton::clicked, this, &DemoBatchSolve::removeSelectedImage);
    connect(ui->selectIndexes, &QPushButton::clicked, this, &DemoBatchSolve::addIndexDirectory);
    connect(ui->selectOutputDir, &QPushButton::clicked, this, &DemoBatchSolve::selectOutputDirectory);
    connect(ui->solveB, &QPushButton::clicked, this, &DemoBatchSolve::startProcessing);
    connect(ui->pauseB, &QPushButton::clicked, this, &DemoBatchSolve::pauseProcessing);
    connect(ui->abortB, &QPushButton::clicked, this, &DemoBatchSolve::abortProcessing);
    connect(&stellarSolver, &StellarSolver::logOutput, this, &DemoBatchSolve::logOutput);
    connect(ui->imagesList,&QListWidget::currentRowChanged, this, &DemoBatchSolve::displayImage);

    ui->indexDirectories->addItems(indexFileDirectories);
    this->setWindowTitle("StellarSolver Batch Solver Demo");
    this->show();
    ui->horSplitter->setSizes(QList<int>() << 100 << ui->horSplitter->width() / 2  << 0 );
    ui->verSplitter->setSizes(QList<int>() << ui->verSplitter->height() / 2  << ui->verSplitter->height() / 2 );


}

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);
#if defined(__linux__)
    setlocale(LC_NUMERIC, "C");
#endif
    DemoBatchSolve *win = new DemoBatchSolve();
    app.exec();

    delete win;

    return 0;
}

void DemoBatchSolve::addImages()
{
    QStringList fileURLs = QFileDialog::getOpenFileNames(nullptr, "Load Image", dirPath,
                      "Images (*.fits *.fit *.bmp *.gif *.jpg *.jpeg *.tif *.tiff)");
    if (fileURLs.isEmpty())
        return;
    for(int i = 0; i< fileURLs.count(); i++)
    {
        Image newImage;
        newImage.fileName=fileURLs.at(i);
        images.append(newImage);
        loadImage(i);
    }
    ui->imagesList->addItems(fileURLs);
}

void DemoBatchSolve::removeAllImages()
{
    while(images.count() > 0)
        removeImage(0);
}

void DemoBatchSolve::removeSelectedImage()
{
    QList<QListWidgetItem *> items = ui->imagesList->selectedItems();
    for(int i = 0; i< items.count(); i++)
        removeImage(ui->imagesList->row(items.at(i)));
}

void DemoBatchSolve::removeImage(int index)
{
    Image image = images.at(index);
    if(image.m_ImageBuffer)
        delete[] image.m_ImageBuffer;
    if(image.searchPosition)
        delete image.searchPosition;
    if(image.searchScale)
        delete image.searchScale;
    images.removeAt(index);
    QListWidgetItem *widget = ui->imagesList->takeItem(index);
    delete widget;
}

void DemoBatchSolve::loadImage(int num)
{
    if(currentImage && currentImage->m_ImageBuffer)
    {
        delete[] currentImage->m_ImageBuffer;
        currentImage->m_ImageBuffer = nullptr;
    }
    fileio imageLoader;
    imageLoader.logToSignal = true;
    connect(&imageLoader, &fileio::logOutput, this, &DemoBatchSolve::logOutput);
    currentImageNum = num;
    currentImage = &images[currentImageNum];
    if(!imageLoader.loadImage(currentImage->fileName))
    {
        logOutput("Error in loading FITS file");
        return;
    }
    currentImage->stats = imageLoader.getStats();
    currentImage->m_ImageBuffer = imageLoader.getImageBuffer();
    currentImage->rawImage = imageLoader.getRawQImage();
    if(imageLoader.position_given)
    {
        FITSImage::wcs_point *position = new FITSImage::wcs_point;
        position->ra = imageLoader.ra;
        position->dec = imageLoader.dec;
        currentImage->searchPosition = position;
    }
    if(imageLoader.scale_given)
    {
        ImageScale *scale = new ImageScale;
        scale->scale_low = imageLoader.scale_low;
        scale->scale_high = imageLoader.scale_high;
        scale->scale_units = imageLoader.scale_units;
        currentImage->searchScale = scale;
    }
    currentImage->m_HeaderRecords = imageLoader.getRecords();

}

void DemoBatchSolve::displayImage(int num)
{
    Image image = images.at(num);
    int sampling = 2;
    double currentZoom = 1;

    double width = ui->scrollArea->rect().width();
    double height = ui->scrollArea->rect().height();

    int w = (image.stats.width + sampling - 1) / sampling;
    int h = (image.stats.height + sampling - 1) / sampling;

    // Find the zoom level which will enclose the current FITS in the current window size
    double zoomX                  = ( width / w);
    double zoomY                  = ( height / h);
    (zoomX < zoomY) ? currentZoom = zoomX : currentZoom = zoomY;

    int currentWidth = static_cast<int> (w * (currentZoom));
    int currentHeight = static_cast<int> (h * (currentZoom));

    QImage scaledImage = image.rawImage.scaled(currentWidth, currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap renderedImage = QPixmap::fromImage(scaledImage);
    ui->imageDisplay->setPixmap(renderedImage);
    ui->imageDisplay->setFixedSize(renderedImage.size());
}

void DemoBatchSolve::addIndexDirectory()
{
    QString fileURL = QFileDialog::getExistingDirectory(nullptr, "Select Index Directory", indexFileDirectories.at(0));
    if (fileURL.isEmpty())
        return;
    ui->indexDirectories->addItem(fileURL);
}

void DemoBatchSolve::selectOutputDirectory()
{
    QString fileURL = QFileDialog::getExistingDirectory(nullptr, "Select Output Directory", outputDirectory);
    if (fileURL.isEmpty())
        return;
    ui->outputDirectory->setText(fileURL);
}

void DemoBatchSolve::logOutput(QString text)
{
    ui->logDisplay->appendPlainText(text);
}

void DemoBatchSolve::startProcessing()
{
    if(images.count() == 0)
    {
        logOutput("No images to process");
        return;
    }
    stoppedBeforeFinished = false;
    if(!paused)
    {
        for(int i = 0; i< ui->imagesList->count(); i++)
        ui->imagesList->item(i)->setBackground(QBrush(QColor("")));
        currentImageNum = 0;
        currentProgress = 0;
    }

    ui->processProgress->setValue(currentProgress);
    ui->processProgress->setMaximum(images.count() * 2);
    processImage(currentImageNum);
}

void DemoBatchSolve::pauseProcessing()
{
    paused = true;
    stoppedBeforeFinished = true;
    stellarSolver.abort();
}

void DemoBatchSolve::abortProcessing()
{
    paused = false;
    stoppedBeforeFinished = true;
    stellarSolver.abort();
    ui->processProgress->setValue(0);
}

void DemoBatchSolve::processImage(int num)
{
    if(currentImage && currentImage->m_ImageBuffer)
    {
        delete[] currentImage->m_ImageBuffer;
        currentImage->m_ImageBuffer = nullptr;
    }
    currentImageNum = num;
    currentImage = &images[currentImageNum];
    loadImage(num);
    stellarSolver.loadNewImageBuffer(currentImage->stats, currentImage->m_ImageBuffer);

    solveImage();
}

void DemoBatchSolve::solveImage()
{
    stellarSolver.setProperty("ProcessType", SSolver::SOLVE);
    stellarSolver.setParameterProfile(SSolver::Parameters::PARALLEL_SMALLSCALE);
    stellarSolver.setIndexFolderPaths(indexFileDirectories);

    if(currentImage->searchPosition)
        stellarSolver.setSearchPositionRaDec(currentImage->searchPosition->ra, currentImage->searchPosition->dec);
    if(currentImage->searchScale)
        stellarSolver.setSearchScale(currentImage->searchScale->scale_low, currentImage->searchScale->scale_high, currentImage->searchScale->scale_units);
    connect(&stellarSolver, &StellarSolver::finished, this, &DemoBatchSolve::solverComplete);
    stellarSolver.start();
}

void DemoBatchSolve::solverComplete()
{
    disconnect(&stellarSolver, &StellarSolver::finished, this, &DemoBatchSolve::solverComplete);
    if(stoppedBeforeFinished)
        return;
    if(stellarSolver.solvingDone())
    {
        currentImage->solution = stellarSolver.getSolution();
        if(stellarSolver.hasWCSData())
        {
            currentImage->wcsData = stellarSolver.getWCSData();
            currentImage->hasWCSData = true;
        }
        currentImage->hasSolved = true;
        ui->imagesList->item(currentImageNum)->setBackground(QBrush(QColor("Green")));
    }
    else
    {
         ui->imagesList->item(currentImageNum)->setBackground(QBrush(QColor("Red")));
    }
    currentProgress++;
    ui->processProgress->setValue(currentProgress);

    extractImage();
}

void DemoBatchSolve::extractImage()
{
    stellarSolver.setProperty("ProcessType", SSolver::EXTRACT);
    stellarSolver.setParameterProfile(SSolver::Parameters::ALL_STARS);
    connect(&stellarSolver, &StellarSolver::finished, this, &DemoBatchSolve::extractorComplete);
    stellarSolver.start();
}

void DemoBatchSolve::extractorComplete()
{
    disconnect(&stellarSolver, &StellarSolver::finished, this, &DemoBatchSolve::extractorComplete);
    if(stoppedBeforeFinished)
        return;
    if(stellarSolver.extractionDone())
    {
        currentImage->stars = stellarSolver.getStarList();
        currentImage->hasExtracted = true;
    }
    currentProgress++;
    ui->processProgress->setValue(currentProgress);

    finishProcessing();
}

void DemoBatchSolve::finishProcessing()
{
    if(ui->saveImages->isChecked() && currentImage->hasSolved)
        saveImage();
    if(ui->saveImages->isChecked() && currentImage->hasExtracted)
        saveStarList();
    processNextImage();
}

void DemoBatchSolve::saveImage()
{
    QString outputDirectory = ui->outputDirectory->text();
    QFileInfo outputDirInfo = QFileInfo(outputDirectory);
    if(outputDirInfo.exists())
    {
        QString savePath = outputDirInfo.absoluteFilePath() + QDir::separator() + QFileInfo(currentImage->fileName).baseName() + "_solved.fits";
        fileio imageSaver;
        imageSaver.logToSignal = true;
        connect(&imageSaver, &fileio::logOutput, this, &DemoBatchSolve::logOutput);
        if(currentImage->hasWCSData)
            imageSaver.saveAsFITS(savePath, currentImage->stats, currentImage->m_ImageBuffer, stellarSolver.getSolution(), currentImage->m_HeaderRecords, true);
        else
            imageSaver.saveAsFITS(savePath, currentImage->stats, currentImage->m_ImageBuffer, stellarSolver.getSolution(), currentImage->m_HeaderRecords, false);

    }
    else
        logOutput("File output directory does not exist, output files were not written.");
}

void DemoBatchSolve::saveStarList()
{
    QString outputDirectory = ui->outputDirectory->text();
    QFileInfo outputDirInfo = QFileInfo(outputDirectory);
    if(outputDirInfo.exists())
    {
        QString savePath = outputDirInfo.absoluteFilePath() + QDir::separator() + QFileInfo(currentImage->fileName).baseName() + "_extracted.csv";
        fileio imageSaver;
        imageSaver.logToSignal = true;
        connect(&imageSaver, &fileio::logOutput, this, &DemoBatchSolve::logOutput);

        QFile file;
        file.setFileName(savePath);
        if (!file.open(QIODevice::WriteOnly))
        {
            logOutput("Unable to write to file" + savePath);
            return;
        }

        QTextStream outstream(&file);
        outstream << "MAG_AUTO" << ",";
        if(currentImage->hasSolved)
        {
            outstream << "RA (J2000)" << ",";
            outstream << "DEC (J2000)" << ",";
        }
        outstream << "X_IMAGE" << ",";
        outstream << "Y_IMAGE" << ",";
        outstream << "FLUX_AUTO" << ",";
        outstream << "PEAK" << ",";
        if(currentImage->hasHFRData)
             outstream << "HFR" << ",";
        outstream << "a" << ",";
        outstream << "b" << ",";
        outstream << "theta";
        outstream << "\n";

        for(int i = 0; i < currentImage->stars.size(); i ++)
        {
            FITSImage::Star star = currentImage->stars.at(i);
            outstream << QString::number(star.mag) << ",";
            if(currentImage->hasSolved)
            {
                outstream << StellarSolver::raString(star.ra) << ",";
                outstream << StellarSolver::decString(star.dec) << ",";
            }
            outstream << QString::number(star.x) << ",";
            outstream << QString::number(star.y) << ",";
            outstream << QString::number(star.flux) << ",";
            outstream << QString::number(star.peak) << ",";
            if(currentImage->hasHFRData)
                 outstream << QString::number(star.HFR) << ",";
            outstream << QString::number(star.a) << ",";
            outstream << QString::number(star.b) << ",";
            outstream << QString::number(star.theta);
            outstream << "\n";
        }

        #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            outstream << Qt::endl;
        #else
            outstream << endl;
        #endif

    }
}

void DemoBatchSolve::processNextImage()
{
    if(currentImageNum < images.count() - 1)
        processImage(currentImageNum + 1);
    else
    {
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput("Processing Complete!");
        ui->processProgress->setValue(0);
    }

}

