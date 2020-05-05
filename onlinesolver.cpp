#include "onlinesolver.h"
#include <QTimer>
#include <QEventLoop>

OnlineSolver::OnlineSolver(ProcessType type, Statistic imagestats, uint8_t *imageBuffer, QObject *parent) : ExternalSextractorSolver(type, imagestats, imageBuffer, parent)
{
    connect(this, &OnlineSolver::timeToCheckJobs, this, &OnlineSolver::checkJobs);

    networkManager = new QNetworkAccessManager();
    connect(networkManager, &QNetworkAccessManager::finished, this, &OnlineSolver::onResult);
}

void OnlineSolver::solve()
{
    if(processType == ONLINE_ASTROMETRY_NET)
        runOnlineSolver();

    if(processType == INT_SEP_ONLINE_ASTROMETRY_NET)
    {
        delete xcol;
        delete ycol;
        xcol=strdup("X"); //This is the column for the x-coordinates, it doesn't accept X_IMAGE like the other one
        ycol=strdup("Y"); //This is the column for the y-coordinates, it doesn't accept Y_IMAGE like the other one
        if(runSEPSextractor())
        {
            int success = writeSextractorTable();
            if(success == 0)
                runOnlineSolver();
        }
        else
            emit finished(-1);
    }
}

void OnlineSolver::runOnlineSolver()
{
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logNeedsUpdating("Configuring Online Solver");

    solverTimer.start();

    authenticate(); //Go to FIRST STAGE

    start(); //Start the other thread, which will monitor everything.
}

//This method will monitor the processes of the Online solver
//Once it the job requests are sent, it will keep checking to see when they are done
//It is important that this thread keeps running as long as the online solver is doing anything
//That way it will behave like the other solvers and isRunning produces the right result
void OnlineSolver::run()
{
    bool timedOut = false;

    while(!hasSolved && !aborted && !timedOut && workflowStage != JOB_PROCESSING_STAGE)
    {
        msleep(200);
        timedOut = solverTimer.elapsed() / 1000.0 > params.solverTimeLimit;
    }

    while(!hasSolved && !aborted && !timedOut && (workflowStage == JOB_PROCESSING_STAGE || workflowStage == JOB_QUEUE_STAGE))
    {
        msleep(JOB_RETRY_DURATION);
        if (job_retries++ > JOB_RETRY_ATTEMPTS)
        {
            emit logNeedsUpdating(("Failed to retrieve job ID, it appears to be lost in the queue."));
            abort();
        }
        else
        {
            emit timeToCheckJobs();
            timedOut = solverTimer.elapsed() / 1000.0 > params.solverTimeLimit;
        }
    }

    if(!hasSolved && !aborted && !timedOut)
    {
        emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        emit logNeedsUpdating("Starting Online Solver");
    }

    while(!hasSolved && !aborted && !timedOut && workflowStage == JOB_MONITORING_STAGE)
    {
        msleep(STATUS_CHECK_INTERVAL);
        emit timeToCheckJobs();
        timedOut = solverTimer.elapsed() / 1000.0 > params.solverTimeLimit;
    }

    if(aborted)
        return;

    if(timedOut)
    {
        disconnect(networkManager, &QNetworkAccessManager::finished, this, &OnlineSolver::onResult);
        emit logNeedsUpdating("Solver timed out");
        emit finished(-1);
        return;
    }

    //Note, if it already has solved,
    //It may or may not have gotten the WCS data yet.
    //We want to wait for a little bit, but not too long.
    bool wcsTimedOut = false;
    double wcsTimeLimit = 10.0;

    if(!aborted && !hasWCS)
    {
        emit logNeedsUpdating("Waiting for WCS. . .");
        solverTimer.start();  //Restart the timer for the WCS download
    }

    //This will wait for WCS until the time limit is reached
    //If it does get the file, whether or not it can read it, the stage changes to NO_STAGE and this quits
    while(!aborted && !wcsTimedOut && workflowStage == WCS_LOADING_STAGE)
    {
        msleep(STATUS_CHECK_INTERVAL);
        wcsTimedOut = solverTimer.elapsed() / 1000.0 > wcsTimeLimit; //Wait 10 seconds for WCS, NO LONGER!
    }

    disconnect(networkManager, &QNetworkAccessManager::finished, this, &OnlineSolver::onResult);

    if(wcsTimedOut)
    {
        emit logNeedsUpdating("WCS download timed out");
        emit finished(0); //Note: It DID solve and we have results, just not WCS data, that is ok.
    }
}


void OnlineSolver::abort()
{
    disconnect(networkManager, &QNetworkAccessManager::finished, this, &OnlineSolver::onResult);
    workflowStage  = NO_STAGE;
    emit logNeedsUpdating("Online Solver aborted.");
    emit finished(-1);
    aborted = true;
}

//This will start up the first stage, Authentication
void OnlineSolver::authenticate()
{
    QNetworkRequest request;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    // If pure IP, add http to it.
    if (!astrometryAPIURL.startsWith("http"))
        astrometryAPIURL = "http://" + astrometryAPIURL;

    QUrl url(astrometryAPIURL);
    url.setPath("/api/login");
    request.setUrl(url);

    QVariantMap apiReq;
    apiReq.insert("apikey", astrometryAPIKey);
    QJsonObject json = QJsonObject::fromVariantMap(apiReq);
    QJsonDocument json_doc(json);

    QString json_request = QString("request-json=%1").arg(QString(json_doc.toJson(QJsonDocument::Compact)));
    networkManager->post(request, json_request.toUtf8());

    workflowStage = AUTH_STAGE;
    emit logNeedsUpdating("Authenticating. . .");

}

//This will start up the second stage, uploading the file
void OnlineSolver::uploadFile()
{
    QNetworkRequest request;

    QFile *fitsFile;
    if(processType == ONLINE_ASTROMETRY_NET)
        fitsFile = new QFile(fileToProcess);
    else
        fitsFile = new QFile(sextractorFilePath);
    bool rc         = fitsFile->open(QIODevice::ReadOnly);
    if (rc == false)
    {
        emit logNeedsUpdating(QString("Failed to open the file %1: %2").arg( fileToProcess).arg( fitsFile->errorString()));
        delete (fitsFile);
        emit finished(-1);
        return;
    }

    QUrl url(astrometryAPIURL);
    url.setPath("/api/upload");
    request.setUrl(url);

    QHttpMultiPart *reqEntity = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QVariantMap uploadReq;
    uploadReq.insert("publicly_visible", "n");
    uploadReq.insert("allow_modifications", "n");
    uploadReq.insert("session", sessionKey);
    uploadReq.insert("allow_commercial_use", "n");

    if(processType == INT_SEP_ONLINE_ASTROMETRY_NET)
    {
        uploadReq.insert("image_width", stats.width);
        uploadReq.insert("image_height", stats.height);
    }

    if (use_scale)
    {
        QString onlineUnits;
        if (scaleunit == SCALE_UNITS_ARCMIN_WIDTH)
            onlineUnits = "arcminwidth";
        if (scaleunit == SCALE_UNITS_DEG_WIDTH)
            onlineUnits = "degwidth";
        if (scaleunit == SCALE_UNITS_ARCSEC_PER_PIX)
            onlineUnits = "arcsecperpix";
        uploadReq.insert("scale_type", "ul");
        uploadReq.insert("scale_units", onlineUnits);
        uploadReq.insert("scale_lower", scalelo);
        uploadReq.insert("scale_upper", scalehi);
    }

    if (use_position)
    {
        uploadReq.insert("center_ra", search_ra);
        uploadReq.insert("center_dec", search_dec);
        uploadReq.insert("radius", params.search_radius);
    }

    if (useWCSCenter)
        uploadReq.insert("crpix_center", true);

    if (params.downsample != 1)
        uploadReq.insert("downsample_factor", params.downsample);

    uploadReq.insert("parity", params.search_parity);

    QJsonObject json = QJsonObject::fromVariantMap(uploadReq);
    QJsonDocument json_doc(json);

    QHttpPart jsonPart;

    jsonPart.setHeader(QNetworkRequest::ContentTypeHeader, "application/text/plain");
    jsonPart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"request-json\"");
    jsonPart.setBody(json_doc.toJson(QJsonDocument::Compact));

    QHttpPart filePart;

    filePart.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QString("form-data; name=\"file\"; filename=\"%1\"").arg(fileToProcess));
    filePart.setBodyDevice(fitsFile);

    // Re-parent so that it get deleted later
    fitsFile->setParent(reqEntity);

    reqEntity->append(jsonPart);
    reqEntity->append(filePart);

    QNetworkReply *reply = networkManager->post(request, reqEntity);
    reqEntity->setParent(reply); //So that it can be deleted later

    workflowStage = UPLOAD_STAGE;
    emit logNeedsUpdating(("Uploading file..."));
}

//This will start up the third stage, waiting till processing is done
void OnlineSolver::waitForProcessing()
{
    workflowStage = JOB_PROCESSING_STAGE;
    emit logNeedsUpdating(("Waiting for Processing to complete..."));
}

//This will start up the fourth stage, getting the Job ID, essentially waiting in the Job Queue
void OnlineSolver::getJobID()
{ 
    workflowStage = JOB_QUEUE_STAGE;
    emit logNeedsUpdating(("Waiting for the Job to Start..."));
}

//This will start the fifth stage, monitoring the job to see when it's done
void OnlineSolver::startMonitoring()
{
    workflowStage = JOB_MONITORING_STAGE;
    emit logNeedsUpdating(("Starting Job Monitoring..."));
}

//This will start the sixth stage, checking the results
void OnlineSolver::checkJobCalibration()
{
    QNetworkRequest request;
    QUrl getCablirationResult = QUrl(QString("%1/api/jobs/%2/calibration").arg(astrometryAPIURL).arg(jobID));
    request.setUrl(getCablirationResult);
    networkManager->get(request);

    workflowStage = JOB_CALIBRATION_STAGE;
    emit logNeedsUpdating(("Requesting the results..."));
}

//This will start the seventh stage, getting the WCS File and loading it (optional).
void OnlineSolver::getJobWCSFile()
{

    QString URL = QString("http://nova.astrometry.net/wcs_file/%1").arg(jobID);
    networkManager->get(QNetworkRequest(QUrl(URL)));

    workflowStage = WCS_LOADING_STAGE;
    emit logNeedsUpdating(("Downloading the WCS file..."));
}

//This will check on the job status during the fourth stage, as it is solving
//It gets called by the other thread, which is monitoring what is happening.
void OnlineSolver::checkJobs()
{
    if(workflowStage == JOB_PROCESSING_STAGE || workflowStage == JOB_QUEUE_STAGE)
    {
        QNetworkRequest request;
        QUrl getJobID = QUrl(QString("%1/api/submissions/%2").arg(astrometryAPIURL).arg(subID));
        request.setUrl(getJobID);
        networkManager->get(request);
    }
    if(workflowStage == JOB_MONITORING_STAGE)
    {
        QNetworkRequest request;
        QUrl getJobStatus = QUrl(QString("%1/api/jobs/%2").arg(astrometryAPIURL).arg(jobID));
        request.setUrl(getJobStatus);
        networkManager->get(request);
    }
}

//This handles the replies from the server
void OnlineSolver::onResult(QNetworkReply *reply)
{
    bool ok = false;
    QJsonParseError parseError;
    QString status;
    QList<QVariant> jsonArray;

    if (workflowStage == NO_STAGE)
    {
        reply->abort();
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        emit logNeedsUpdating(reply->errorString());
        emit finished(-1);
        return;
    }
    QString json;
    QJsonDocument json_doc;
    QVariant json_result;
    QVariantMap result;
    if(workflowStage != WCS_LOADING_STAGE)
    {
        json = (QString)reply->readAll();

        json_doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);

        if (parseError.error != QJsonParseError::NoError)
        {
            emit logNeedsUpdating(QString("JSON error during parsing (%1).").arg(parseError.errorString()));
            emit finished(-1);
            return;
        }

        json_result = json_doc.toVariant();
        result   = json_result.toMap();

        //if (Options::alignmentLogging())
        //    align->appendLogText(json_doc.toJson(QJsonDocument::Compact));
        emit logNeedsUpdating(json_doc.toJson(QJsonDocument::Compact));
    }
    switch (workflowStage)
    {
        case AUTH_STAGE:
            status = result["status"].toString();
            if (status != "success")
            {
                emit logNeedsUpdating("Astrometry.net authentication failed. Check the validity of the Astrometry.net API Key.");
                abort();
                return;
            }

            sessionKey = result["session"].toString();

            //if (Options::alignmentLogging())
            //    emit logNeedsUpdating(("Authentication to astrometry.net is successful. Session: %1", sessionKey));
            emit logNeedsUpdating(QString("Authentication to astrometry.net is successful. Session: %1").arg(sessionKey));

            emit uploadFile(); //Go to NEXT STAGE
            break;

        case UPLOAD_STAGE:
            status = result["status"].toString();
            if (status != "success")
            {
                emit logNeedsUpdating(("Upload failed."));
                abort();
                return;
            }

            subID = result["subid"].toInt(&ok);

            if (ok == false)
            {
                emit logNeedsUpdating(("Parsing submission ID failed."));
                abort();
                return;
            }

            emit logNeedsUpdating(("Upload complete. Waiting for astrometry.net solver to complete..."));
            waitForProcessing();  //Go to the NEXT STAGE
            break;

        case JOB_PROCESSING_STAGE:
        {
            QString finished;
            finished = result["processing_finished"].toString();

            if (finished == "None" || finished =="")
                return;

            getJobID(); //Go to the NEXT STAGE
        }
            break;

        case JOB_QUEUE_STAGE:
            jsonArray = result["jobs"].toList();

            if (jsonArray.isEmpty())
                jobID = 0;
            else
                jobID = jsonArray.first().toInt(&ok);

            if (jobID == 0 || !ok)
                return;

            startMonitoring(); //Go to the NEXT STAGE
            break;

        case JOB_MONITORING_STAGE:
            status = result["status"].toString();
            if (status == "success")
                checkJobCalibration(); // Go to the NEXT STAGE
            else if (status == "solving" || status == "processing")
            {
                return;
            }
            else if (status == "failure")
            {
                emit logNeedsUpdating("Solver Failed");
                abort();
                return;
            }
            break;

        case JOB_CALIBRATION_STAGE:
        {
            double fieldw = result["width_arcsec"].toDouble(&ok) / 60.0;
            if (ok == false)
            {
                emit logNeedsUpdating(("Error parsing width."));
                abort();
                return;
            }
            double fieldh = result["height_arcsec"].toDouble(&ok) / 60.0;
            if (ok == false)
            {
                emit logNeedsUpdating(("Error parsing width."));
                abort();
                return;
            }
            int parity = result["parity"].toInt(&ok);
            if (ok == false)
            {
                emit logNeedsUpdating(("Error parsing parity."));
                abort();
                return;
            }
            double orientation = result["orientation"].toDouble(&ok);
            if (ok == false)
            {
                emit logNeedsUpdating(("Error parsing orientation."));
                abort();
                return;
            }
            orientation *= parity;
            double ra = result["ra"].toDouble(&ok);
            if (ok == false)
            {
                emit logNeedsUpdating(("Error parsing RA."));
                abort();
                return;
            }
            double dec = result["dec"].toDouble(&ok);
            if (ok == false)
            {
                emit logNeedsUpdating(("Error parsing DEC."));
                abort();
                return;
            }

            double pixscale = result["pixscale"].toDouble(&ok);
            if (ok == false)
            {
                emit logNeedsUpdating(("Error parsing DEC."));
                abort();
                return;
            }

            char rastr[32], decstr[32];
            ra2hmsstring(ra, rastr);
            dec2dmsstring(dec, decstr);
            float raErr = 0;
            float decErr = 0;
            if(use_position)
            {
                raErr = (search_ra - ra) * 3600;
                decErr = (search_dec - dec) * 3600;
            }
            QString par = (parity > 0) ? "neg" : "pos";
            solution = {fieldw,fieldh,ra,dec,rastr,decstr,orientation, pixscale, par, raErr, decErr};
            hasSolved = true;

            getJobWCSFile(); //Go to FINAL STAGE
        }
            break;

        case WCS_LOADING_STAGE:
        {
            QByteArray responseData = reply->readAll();
            QString solutionFile = basePath + "/" + baseName + ".wcs";
            QFile file(solutionFile);
            if (!file.open(QIODevice::WriteOnly))
            {
                emit logNeedsUpdating(("WCS File Write Error"));
                emit finished(0); //We still have the solution, this is not a failure!
                return;
            }
            file.write(responseData.data(), responseData.size());
            file.close();
            loadWCS(); //Attempt to load WCS from the file
            emit finished(0); //Success! We are completely done, whether or not the WCS loading was successful
            workflowStage = NO_STAGE;
            return;
        }
            break;

        default:
            break;
    }
}

