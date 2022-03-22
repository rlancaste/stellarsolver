#include "demobatchsolve.h"
#include "ui_demobatchsolve.h"
#include <QFileDialog>
#include <QTextStream>

DemoBatchSolve::DemoBatchSolve():
    QMainWindow(),
    ui(new Ui::DemoBatchSolve)
{
    ui->setupUi(this);
    connect(ui->loadB, &QPushButton::clicked, this, &DemoBatchSolve::addImages);
    connect(ui->clearImagesB, &QPushButton::clicked, this, &DemoBatchSolve::removeAllImages);
    connect(ui->deleteImageB, &QPushButton::clicked, this, &DemoBatchSolve::removeSelectedImage);
    connect(ui->resetB, &QPushButton::clicked, this, &DemoBatchSolve::resetImages);
    connect(ui->selectIndexes, &QPushButton::clicked, this, &DemoBatchSolve::addIndexDirectory);
    connect(ui->selectOutputDir, &QPushButton::clicked, this, &DemoBatchSolve::selectOutputDirectory);
    connect(ui->processB, &QPushButton::clicked, this, &DemoBatchSolve::startProcessing);
    connect(ui->abortB, &QPushButton::clicked, this, &DemoBatchSolve::abortProcessing);
    connect(ui->clearB, &QPushButton::clicked, this, &DemoBatchSolve::clearLog);
    connect(&stellarSolver, &StellarSolver::logOutput, this, &DemoBatchSolve::logOutput);
    connect(ui->imagesList,&QTableWidget::itemSelectionChanged, this, &DemoBatchSolve::displayImage);

    ui->indexDirectories->addItems(indexFileDirectories);

    QTableWidget *table = ui->imagesList;
    table->setColumnCount(4);
    table->setRowCount(0);
    table->setHorizontalHeaderItem(0, new QTableWidgetItem("Image"));
    table->setHorizontalHeaderItem(1, new QTableWidgetItem("RA (J2000)"));
    table->setHorizontalHeaderItem(2, new QTableWidgetItem("DEC (J2000)"));
    table->setHorizontalHeaderItem(3, new QTableWidgetItem("Stars"));

    QList<SSolver::Parameters> defaults = StellarSolver::getBuiltInProfiles();
    for(int i = 0; i< defaults.count(); i++)
    {
        ui->solveProfile->addItem(defaults.at(i).listName);
        ui->extractProfile->addItem(defaults.at(i).listName);
    }
    ui->solveProfile->setCurrentIndex(3);
    ui->extractProfile->setCurrentIndex(4);

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
                      "Images (*.fits *.fit *.bmp *.gif *.jpg *.jpeg *.png *.tif *.tiff)");
    if (fileURLs.isEmpty())
        return;
    for(int i = 0; i< fileURLs.count(); i++)
    {
        Image newImage;
        newImage.fileName=fileURLs.at(i);
        images.append(newImage);
        loadImage(i);
        clearCurrentImageAndBuffer();
        int row = ui->imagesList->rowCount();
        QString name = QFileInfo(newImage.fileName).baseName();
        ui->imagesList->insertRow(row);
        ui->imagesList->setItem(row, 0, new QTableWidgetItem(name));
        ui->imagesList->setItem(row, 1, new QTableWidgetItem(""));
        ui->imagesList->setItem(row, 2, new QTableWidgetItem(""));
        ui->imagesList->setItem(row, 3, new QTableWidgetItem(""));
    }
    if(ui->imagesList->selectedItems().count() == 0)
        ui->imagesList->setCurrentCell(0,0);
}

void DemoBatchSolve::resetImages()
{
    for(int i = 0; i < images.count(); i++)
    {
        Image image = images.at(i);
        image.hasExtracted = false;
        image.hasSolved = false;
        image.hasWCSData = false;
    }
    for(int row = 0; row< ui->imagesList->rowCount(); row++)
    {
        ui->imagesList->setItem(row, 1, new QTableWidgetItem(""));
        ui->imagesList->setItem(row, 2, new QTableWidgetItem(""));
        ui->imagesList->setItem(row, 3, new QTableWidgetItem(""));
        for(int col = 0; col< ui->imagesList->columnCount(); col++)
            ui->imagesList->item(row,col)->setForeground(QBrush());
    }
}

void DemoBatchSolve::removeAllImages()
{
    while(images.count() > 0)
        removeImage(0);
}

void DemoBatchSolve::removeSelectedImage()
{
    removeImage(ui->imagesList->currentRow());
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
    ui->imagesList->removeRow(index);
}

void DemoBatchSolve::loadImage(int num)
{
    clearCurrentImageAndBuffer();
    fileio imageLoader;
    imageLoader.logToSignal = true;
    connect(&imageLoader, &fileio::logOutput, this, &DemoBatchSolve::logOutput);
    currentImageNum = num;
    currentImage = &images[currentImageNum];
    if(!imageLoader.loadImage(currentImage->fileName))
    {
        logOutput("Error in loading image file");
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

void DemoBatchSolve::displayImage()
{
    int num = ui->imagesList->currentRow();
    if(currentRow == num)
        return;
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
    QString dir = "";
    if(indexFileDirectories.count() > 0)
        dir = indexFileDirectories.at(0);
    QString fileURL = QFileDialog::getExistingDirectory(nullptr, "Select Index Directory", dir);
    if (fileURL.isEmpty())
        return;
    indexFileDirectories.append(fileURL);
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

void DemoBatchSolve::clearLog()
{
    ui->logDisplay->clear();
}

void DemoBatchSolve::startProcessing()
{
    if(images.count() == 0)
    {
        logOutput("No images to process");
        return;
    }
    aborted = false;
    currentImageNum = 0;
    currentProgress = 0;

    ui->processProgress->setValue(currentProgress);
    ui->processProgress->setMaximum(images.count() * 2);
    processImage(currentImageNum);
}

void DemoBatchSolve::abortProcessing()
{
    aborted = true;
    stellarSolver.abort();
    ui->processProgress->setValue(0);
}

void DemoBatchSolve::clearCurrentImageAndBuffer()
{
    if(currentImage && currentImage->m_ImageBuffer)
    {
        delete[] currentImage->m_ImageBuffer;
        currentImage->m_ImageBuffer = nullptr;
        currentImage = nullptr;
    }
}

void DemoBatchSolve::processImage(int num)
{
    clearCurrentImageAndBuffer();
    currentImageNum = num;
    currentImage = &images[currentImageNum];
    loadImage(num);
    stellarSolver.loadNewImageBuffer(currentImage->stats, currentImage->m_ImageBuffer);

    solvingBlind = false;
    solveImage();
}

void DemoBatchSolve::solveImage()
{
    if(!currentImage || !currentImage->m_ImageBuffer)
    {
        return;
    }
    if(currentImage->hasSolved)
    {
        currentProgress++;
        ui->processProgress->setValue(currentProgress);
        extractImage();
        return;
    }
    stellarSolver.setProperty("ProcessType", SSolver::SOLVE);
    stellarSolver.setParameterProfile((SSolver::Parameters::ParametersProfile) ui->solveProfile->currentIndex());
    stellarSolver.setIndexFolderPaths(indexFileDirectories);

    if(solvingBlind)
    {
        stellarSolver.clearSearchPosition();
        stellarSolver.clearSearchScale();
    }
    else
    {
        if(currentImage->searchPosition)
            stellarSolver.setSearchPositionRaDec(currentImage->searchPosition->ra, currentImage->searchPosition->dec);
        if(currentImage->searchScale)
            stellarSolver.setSearchScale(currentImage->searchScale->scale_low, currentImage->searchScale->scale_high, currentImage->searchScale->scale_units);
    }
    connect(&stellarSolver, &StellarSolver::finished, this, &DemoBatchSolve::solverComplete);
    stellarSolver.start();
}

void DemoBatchSolve::solverComplete()
{
    if(!currentImage || !currentImage->m_ImageBuffer)
    {
        return;
    }
    disconnect(&stellarSolver, &StellarSolver::finished, this, &DemoBatchSolve::solverComplete);
    if(aborted)
    {
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput("Solving was Aborted");
        clearCurrentImageAndBuffer();
        return;
    }
    if(stellarSolver.solvingDone())
    {
        currentImage->solution = stellarSolver.getSolution();
        if(stellarSolver.hasWCSData())
        {
            currentImage->wcsData = stellarSolver.getWCSData();
            currentImage->hasWCSData = true;
        }
        currentImage->hasSolved = true;
        for(int col = 0; col< ui->imagesList->columnCount(); col++)
            ui->imagesList->item(currentImageNum,col)->setForeground(QBrush(Qt::darkGreen));
        ui->imagesList->item(currentImageNum,1)->setText(StellarSolver::raString(currentImage->solution.ra));
        ui->imagesList->item(currentImageNum,2)->setText(StellarSolver::decString(currentImage->solution.dec));
        solvingBlind = false;
    }
    else
    {
        if(!solvingBlind && ( currentImage->searchScale || currentImage->searchPosition))
        {
            logOutput("Solving failed with position/scale, trying again with a blind solve.");
            solvingBlind = true;
            solveImage();
            return;
        }
        for(int col = 0; col< ui->imagesList->columnCount(); col++)
            ui->imagesList->item(currentImageNum,col)->setForeground(QBrush(Qt::darkRed));
    }
    currentProgress++;
    ui->processProgress->setValue(currentProgress);

    extractImage();
}

void DemoBatchSolve::extractImage()
{
    if(!currentImage || !currentImage->m_ImageBuffer)
    {
        return;
    }
    if(currentImage->hasExtracted)
    {
        currentProgress++;
        ui->processProgress->setValue(currentProgress);
        finishProcessing();
        return;
    }
    if(ui->getHFR->isChecked())
        stellarSolver.setProperty("ProcessType", SSolver::EXTRACT_WITH_HFR);
    else
        stellarSolver.setProperty("ProcessType", SSolver::EXTRACT);
    stellarSolver.setParameterProfile((SSolver::Parameters::ParametersProfile) ui->extractProfile->currentIndex());
    connect(&stellarSolver, &StellarSolver::finished, this, &DemoBatchSolve::extractorComplete);
    stellarSolver.start();
}

void DemoBatchSolve::extractorComplete()
{
    if(!currentImage || !currentImage->m_ImageBuffer)
    {
        return;
    }
    disconnect(&stellarSolver, &StellarSolver::finished, this, &DemoBatchSolve::extractorComplete);
    if(aborted)
    {
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput("Extraction was Aborted");
        clearCurrentImageAndBuffer();
        return;
    }
    if(stellarSolver.extractionDone())
    {
        currentImage->stars = stellarSolver.getStarList();
        ui->imagesList->item(currentImageNum,3)->setText(QString::number(currentImage->stars.count()));
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
    clearCurrentImageAndBuffer();
    processNextImage();
}

void DemoBatchSolve::saveImage()
{
    if(!currentImage || !currentImage->m_ImageBuffer)
    {
        return;
    }
    QString outputDirectory = ui->outputDirectory->text();
    QFileInfo outputDirInfo = QFileInfo(outputDirectory);
    if(outputDirInfo.exists())
    {
        QString savePath = outputDirInfo.absoluteFilePath() + QDir::separator() + QFileInfo(currentImage->fileName).baseName() + "_solved.fits";
        logOutput("Saving solved image to: " + savePath);
        fileio imageSaver;
        imageSaver.logToSignal = false;
        //connect(&imageSaver, &fileio::logOutput, this, &DemoBatchSolve::logOutput);
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
        logOutput("Saving starList to: " + savePath);
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
                outstream << " " << StellarSolver::raString(star.ra) << " " << ",";
                outstream << " " << StellarSolver::decString(star.dec) << " " << ",";
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

