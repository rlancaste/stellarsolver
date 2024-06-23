//Qt includes
#include <QFileDialog>
#include <QTextStream>

//includes from this project
#include "stellarbatchsolver.h"
#include "ui_stellarbatchsolver.h"


StellarBatchSolver::StellarBatchSolver():
    QMainWindow(),
    ui(new Ui::StellarBatchSolver)
{
    ui->setupUi(this);
    connect(ui->loadB, &QPushButton::clicked, this, &StellarBatchSolver::addImages);
    connect(ui->clearImagesB, &QPushButton::clicked, this, &StellarBatchSolver::removeAllImages);
    connect(ui->deleteImageB, &QPushButton::clicked, this, &StellarBatchSolver::removeSelectedImage);
    connect(ui->resetB, &QPushButton::clicked, this, &StellarBatchSolver::resetImages);
    connect(ui->selectIndexes, &QPushButton::clicked, this, &StellarBatchSolver::addIndexDirectory);
    connect(ui->selectOutputDir, &QPushButton::clicked, this, &StellarBatchSolver::selectOutputDirectory);
    connect(ui->processB, &QPushButton::clicked, this, &StellarBatchSolver::startProcessing);
    connect(ui->abortB, &QPushButton::clicked, this, &StellarBatchSolver::abortProcessing);
    connect(ui->clearB, &QPushButton::clicked, this, &StellarBatchSolver::clearLog);
    connect(&stellarSolver, &StellarSolver::logOutput, this, &StellarBatchSolver::logOutput);
    connect(ui->imagesList,&QTableWidget::itemSelectionChanged, this, &StellarBatchSolver::displayImage);

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

    this->setWindowTitle("StellarSolver Batch Solver Program");
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
    StellarBatchSolver *win = new StellarBatchSolver();
    app.exec();

    delete win;

    return 0;
}

void StellarBatchSolver::addImages()
{
    QStringList fileURLs = QFileDialog::getOpenFileNames(nullptr, "Load Image", dirPath,
                      "Images (*.fits *.fit *.bmp *.gif *.jpg *.jpeg *.png *.tif *.tiff *.pbm *.pgm *.ppm *.xbm *.xpm)");
    if (fileURLs.isEmpty())
        return;
    for(int i = 0; i< fileURLs.count(); i++)
    {
        Image newImage;
        newImage.fileName=fileURLs.at(i);
        images.append(newImage);
        loadImage(i);
        int row = ui->imagesList->rowCount();
        QString name = QFileInfo(newImage.fileName).fileName();
        ui->imagesList->insertRow(row);
        ui->imagesList->setItem(row, 0, new QTableWidgetItem(name));
        ui->imagesList->item(row, 0)->setToolTip(newImage.fileName);
        for(int col = 1; col < 4; col++)
        {
            ui->imagesList->setItem(row, col, new QTableWidgetItem(""));
            ui->imagesList->item(row, col)->setToolTip(newImage.fileName);
        }
    }
    if(ui->imagesList->selectedItems().count() == 0)
        ui->imagesList->setCurrentCell(0,0);
}

void StellarBatchSolver::resetImages()
{
    for(int i = 0; i < images.count(); i++)
    {
        Image &image = images[i];
        image.hasExtracted = false;
        image.hasSolved = false;
        image.hasWCSData = false;
    }
    for(int row = 0; row< ui->imagesList->rowCount(); row++)
    {
        ui->imagesList->item(row, 1)->setText("");
        ui->imagesList->item(row, 2)->setText("");
        ui->imagesList->item(row, 3)->setText("");
        for(int col = 0; col< ui->imagesList->columnCount(); col++)
            ui->imagesList->item(row,col)->setForeground(QBrush());
    }
}

void StellarBatchSolver::removeAllImages()
{
    while(images.count() > 0)
        removeImage(0);
    ui->imageDisplay->clear();
}

void StellarBatchSolver::removeSelectedImage()
{
    removeImage(ui->imagesList->currentRow());
}

void StellarBatchSolver::removeImage(int index)
{
    Image &image = images[index];
    if(image.m_ImageBuffer)
        delete[] image.m_ImageBuffer;
    if(image.searchPosition)
        delete image.searchPosition;
    if(image.searchScale)
        delete image.searchScale;
    images.removeAt(index);
    ui->imagesList->removeRow(index);
}

void StellarBatchSolver::loadImage(int num)
{
    fileio imageLoader;
    imageLoader.logToSignal = true;
    connect(&imageLoader, &fileio::logOutput, this, &StellarBatchSolver::logOutput);
    Image &image = images[num];
    if(!imageLoader.loadImage(image.fileName))
    {
        logOutput("Error in loading image file");
        return;
    }
    image.stats = imageLoader.getStats();
    //No need to get the imageBuffer now.
    image.rawImage = imageLoader.getRawQImage();
    if(imageLoader.position_given)
    {
        FITSImage::wcs_point *position = new FITSImage::wcs_point;
        position->ra = imageLoader.ra;
        position->dec = imageLoader.dec;
        image.searchPosition = position;
    }
    if(imageLoader.scale_given)
    {
        ImageScale *scale = new ImageScale;
        scale->scale_low = imageLoader.scale_low;
        scale->scale_high = imageLoader.scale_high;
        scale->scale_units = imageLoader.scale_units;
        image.searchScale = scale;
    }
    image.m_HeaderRecords = imageLoader.getRecords();
}

void StellarBatchSolver::displayImage()
{
    if(images.count() == 0)
    {
         ui->imageDisplay->clear();
         return;
    }
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

void StellarBatchSolver::addIndexDirectory()
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

void StellarBatchSolver::selectOutputDirectory()
{
    QString fileURL = QFileDialog::getExistingDirectory(nullptr, "Select Output Directory", outputDirectory);
    if (fileURL.isEmpty())
        return;
    ui->outputDirectory->setText(fileURL);
}

void StellarBatchSolver::logOutput(QString text)
{
    ui->logDisplay->appendPlainText(text);
}

void StellarBatchSolver::clearLog()
{
    ui->logDisplay->clear();
}

void StellarBatchSolver::startProcessing()
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

void StellarBatchSolver::abortProcessing()
{
    aborted = true;
    stellarSolver.abort();
    ui->processProgress->setValue(0);
}

void StellarBatchSolver::clearCurrentImageBuffer()
{
    if(currentImage && currentImage->m_ImageBuffer)
    {
        delete[] currentImage->m_ImageBuffer;
        currentImage->m_ImageBuffer = nullptr;
    }
}

void StellarBatchSolver::processImage(int num)
{
    currentImageNum = num;
    currentImage = &images[currentImageNum];
    if(currentImage->hasSolved && currentImage->hasExtracted)
    {
        currentProgress += 2;
        ui->processProgress->setValue(currentProgress);
        finishProcessing();
        return;
    }
    clearCurrentImageBuffer();
    fileio imageLoader;
    imageLoader.logToSignal = true;
    connect(&imageLoader, &fileio::logOutput, this, &StellarBatchSolver::logOutput);

    if(!imageLoader.loadImageBufferOnly(currentImage->fileName))
    {
        logOutput("Error in loading image file");
        return;
    }
    currentImage->m_ImageBuffer = imageLoader.getImageBuffer();
    stellarSolver.loadNewImageBuffer(currentImage->stats, currentImage->m_ImageBuffer);

    solvingBlind = false;
    solveImage();
}

void StellarBatchSolver::solveImage()
{
    if(!currentImage || !currentImage->m_ImageBuffer)
        return;

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
    stellarSolver.setColorChannel(ui->colorChannel->currentIndex());
    connect(&stellarSolver, &StellarSolver::finished, this, &StellarBatchSolver::solverComplete);
    stellarSolver.start();
}

void StellarBatchSolver::solverComplete()
{
    if(!currentImage || !currentImage->m_ImageBuffer)
        return;
    disconnect(&stellarSolver, &StellarSolver::finished, this, &StellarBatchSolver::solverComplete);
    if(aborted)
    {
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput("Solving was Aborted");
        clearCurrentImageBuffer();
        currentImage = nullptr;
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
    }
    else
    {
        if(!solvingBlind && ( currentImage->searchScale || currentImage->searchPosition))
        {
            logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
            logOutput("Solving failed with position/scale, trying again with a blind solve.");
            logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
            solvingBlind = true;
            solveImage();
            return;
        }
        for(int col = 0; col< ui->imagesList->columnCount(); col++)
            ui->imagesList->item(currentImageNum,col)->setForeground(QBrush(Qt::darkRed));
    }
    currentProgress++;
    ui->processProgress->setValue(currentProgress);
    solvingBlind = false;
    extractImage();
}

void StellarBatchSolver::extractImage()
{
    if(!currentImage || !currentImage->m_ImageBuffer)
        return;
    if(ui->getHFR->isChecked())
        stellarSolver.setProperty("ProcessType", SSolver::EXTRACT_WITH_HFR);
    else
        stellarSolver.setProperty("ProcessType", SSolver::EXTRACT);
    stellarSolver.setColorChannel(ui->colorChannel->currentIndex());
    stellarSolver.setParameterProfile((SSolver::Parameters::ParametersProfile) ui->extractProfile->currentIndex());
    connect(&stellarSolver, &StellarSolver::finished, this, &StellarBatchSolver::extractorComplete);
    stellarSolver.start();
}

void StellarBatchSolver::extractorComplete()
{
    if(!currentImage || !currentImage->m_ImageBuffer)
        return;
    disconnect(&stellarSolver, &StellarSolver::finished, this, &StellarBatchSolver::extractorComplete);
    if(aborted)
    {
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput("Extraction was Aborted");
        clearCurrentImageBuffer();
        currentImage = nullptr;
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

void StellarBatchSolver::finishProcessing()
{
    if(ui->saveImages->isChecked() && currentImage->hasSolved)
        saveImage();
    if(ui->saveImages->isChecked() && currentImage->hasExtracted)
        saveStarList();
    clearCurrentImageBuffer();
    currentImage = nullptr;
    processNextImage();
}

void StellarBatchSolver::saveImage()
{
    if(!currentImage || !currentImage->m_ImageBuffer)
        return;
    QString outputDirectory = ui->outputDirectory->text();
    QFileInfo outputDirInfo = QFileInfo(outputDirectory);
    if(outputDirInfo.exists())
    {
        QString savePath = outputDirInfo.absoluteFilePath() + QDir::separator() + QFileInfo(currentImage->fileName).baseName() + "_solved.fits";
        logOutput("Saving solved image to: " + savePath);
        fileio imageSaver;
        imageSaver.logToSignal = false;
        //connect(&imageSaver, &fileio::logOutput, this, &StellarBatchSolver::logOutput);
        if(currentImage->hasWCSData)
            imageSaver.saveAsFITS(savePath, currentImage->stats, currentImage->m_ImageBuffer, stellarSolver.getSolution(), currentImage->m_HeaderRecords, true);
        else
            imageSaver.saveAsFITS(savePath, currentImage->stats, currentImage->m_ImageBuffer, stellarSolver.getSolution(), currentImage->m_HeaderRecords, false);

    }
    else
        logOutput("File output directory does not exist, output files were not written.");
}

void StellarBatchSolver::saveStarList()
{
    QString outputDirectory = ui->outputDirectory->text();
    QFileInfo outputDirInfo = QFileInfo(outputDirectory);
    if(outputDirInfo.exists())
    {
        QString savePath = outputDirInfo.absoluteFilePath() + QDir::separator() + QFileInfo(currentImage->fileName).baseName() + "_extracted.csv";
        logOutput("Saving starList to: " + savePath);
        fileio imageSaver;
        imageSaver.logToSignal = true;
        connect(&imageSaver, &fileio::logOutput, this, &StellarBatchSolver::logOutput);

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

void StellarBatchSolver::processNextImage()
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

