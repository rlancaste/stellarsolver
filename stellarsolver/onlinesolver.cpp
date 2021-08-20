/*  OnlineSolver, StellarSolver Internal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#include "onlinesolver.h"
#include <QTimer>
#include <QEventLoop>

OnlineSolver::OnlineSolver(ProcessType type, ExtractorType sexType, SolverType solType, FITSImage::Statistic imagestats,
                           uint8_t const *imageBuffer, QObject *parent) : ExternalSextractorSolver(type, sexType, solType, imagestats, imageBuffer,
                                       parent)
{
    connect(this, &OnlineSolver::timeToCheckJobs, this, &OnlineSolver::checkJobs);
    connect(this, &OnlineSolver::startupOnlineSolver, this, &OnlineSolver::authenticate);

    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished, this, &OnlineSolver::onResult);
}

void OnlineSolver::execute()
{
    if(m_ActiveParameters.multiAlgorithm != NOT_MULTI)
        emit logOutput("The Online solver option does not support multithreading, since the server already does this internally, ignoring this option");

    if(m_ExtractorType == EXTRACTOR_BUILTIN)
        runOnlineSolver();
    else
    {
        delete xcol;
        delete ycol;
        xcol = strdup("X"); //This is the column for the x-coordinates, it doesn't accept X_IMAGE like the other one
        ycol = strdup("Y"); //This is the column for the y-coordinates, it doesn't accept Y_IMAGE like the other one
        int fail = 0;
        if(m_ExtractorType == EXTRACTOR_INTERNAL)
            fail = runSEPSextractor();
        else if(m_ExtractorType == EXTRACTOR_EXTERNAL)
            fail = runExternalSextractor();
        if(fail != 0)
        {
            emit finished(fail);
            return;
        }
        if(m_ExtractedStars.size() == 0)
        {
            emit logOutput("No stars were found, so the image cannot be solved");
            emit finished(-1);
            return;
        }
        if((fail = writeSextractorTable()) != 0)
        {
            emit finished(fail);
            return;
        }
        runOnlineSolver();
    }
}

void OnlineSolver::runOnlineSolver()
{
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Configuring Online Solver");

    if(m_LogToFile && m_AstrometryLogLevel != LOG_NONE)
    {
        if(m_LogFileName == "")
            m_LogFileName = m_BasePath + "/" + m_BaseName + ".log.txt";
        if(QFile(m_LogFileName).exists())
            QFile(m_LogFileName).remove();
    }

    solverTimer.start();

    emit startupOnlineSolver(); //Go to FIRST STAGE
    start(); //Start the other thread, which will monitor everything.
}

//This method will monitor the processes of the Online solver
//Once it the job requests are sent, it will keep checking to see when they are done
//It is important that this thread keeps running as long as the online solver is doing anything
//That way it will behave like the other solvers and isRunning produces the right result
void OnlineSolver::run()
{
    bool timedOut = false;

    while(!m_HasSolved && !aborted && !timedOut && workflowStage != JOB_PROCESSING_STAGE)
    {
        msleep(200);
        timedOut = solverTimer.elapsed() / 1000.0 > m_ActiveParameters.solverTimeLimit;
    }

    while(!m_HasSolved && !aborted && !timedOut && (workflowStage == JOB_PROCESSING_STAGE || workflowStage == JOB_QUEUE_STAGE))
    {
        msleep(JOB_RETRY_DURATION);
        if (job_retries++ > JOB_RETRY_ATTEMPTS)
        {
            emit logOutput(("Failed to retrieve job ID, it appears to be lost in the queue."));
            abort();
        }
        else
        {
            emit timeToCheckJobs();
            timedOut = solverTimer.elapsed() / 1000.0 > m_ActiveParameters.solverTimeLimit;
        }
    }

    if(!m_HasSolved && !aborted && !timedOut)
    {
        emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        emit logOutput("Starting Online Solver with the " + m_ActiveParameters.listName + " profile . . .");

    }

    while(!m_HasSolved && !aborted && !timedOut && workflowStage == JOB_MONITORING_STAGE)
    {
        msleep(STATUS_CHECK_INTERVAL);
        emit timeToCheckJobs();
        timedOut = solverTimer.elapsed() / 1000.0 > m_ActiveParameters.solverTimeLimit;
    }

    if(aborted)
        return;

    if(timedOut)
    {
        disconnect(networkManager, &QNetworkAccessManager::finished, this, &OnlineSolver::onResult);
        emit logOutput("Solver timed out");
        emit finished(-1);
        return;
    }

    //Note, if it already has solved,
    //It may or may not have gotten the Stars, Log, and WCS data yet.
    //We want to wait for a little bit, but not too long.
    bool starsAndWCSTimedOut = false;
    double starsAndWCSTimeLimit = 10.0;

    if(!aborted && !m_HasWCS)
    {
        emit logOutput("Waiting for Stars and WCS. . .");
        solverTimer.start();  //Restart the timer for the Stars and WCS download
    }

    //This will wait for stars and WCS until the time limit is reached
    //If it does get the file, whether or not it can read it, the stage changes to NO_STAGE and this quits
    while(!aborted && !starsAndWCSTimedOut && (workflowStage == LOG_LOADING_STAGE || workflowStage == WCS_LOADING_STAGE))
    {
        msleep(STATUS_CHECK_INTERVAL);
        starsAndWCSTimedOut = solverTimer.elapsed() / 1000.0 > starsAndWCSTimeLimit; //Wait 10 seconds for STARS and WCS, NO LONGER!
    }

    disconnect(networkManager, &QNetworkAccessManager::finished, this, &OnlineSolver::onResult);

    if(starsAndWCSTimedOut)
    {
        emit logOutput("WCS download timed out");
        emit finished(0); //Note: It DID solve and we have results, just not WCS data, that is ok.
    }
}


void OnlineSolver::abort()
{
    disconnect(networkManager, &QNetworkAccessManager::finished, this, &OnlineSolver::onResult);
    workflowStage  = NO_STAGE;
    emit logOutput("Online Solver aborted.");
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
    emit logOutput("Authenticating. . .");

}

//This will start up the second stage, uploading the file
void OnlineSolver::uploadFile()
{
    QNetworkRequest request;

    QFile *fitsFile;
    if(m_ExtractorType == EXTRACTOR_BUILTIN)
        fitsFile = new QFile(fileToProcess);
    else
        fitsFile = new QFile(sextractorFilePath);
    bool rc = fitsFile->open(QIODevice::ReadOnly);
    if (rc == false)
    {
        emit logOutput(QString("Failed to open the file %1: %2").arg( fileToProcess).arg( fitsFile->errorString()));
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

    if(m_ExtractorType != EXTRACTOR_BUILTIN)
    {
        uploadReq.insert("image_width", m_Statistics.width);
        uploadReq.insert("image_height", m_Statistics.height);
    }

    if (m_UseScale)
    {
        uploadReq.insert("scale_type", "ul");
        uploadReq.insert("scale_units", getScaleUnitString());
        uploadReq.insert("scale_lower", scalelo);
        uploadReq.insert("scale_upper", scalehi);
    }

    if (m_UsePosition)
    {
        uploadReq.insert("center_ra", search_ra);
        uploadReq.insert("center_dec", search_dec);
        uploadReq.insert("radius", m_ActiveParameters.search_radius);
    }

    //We would like the Coordinates found to be the center of the image
    uploadReq.insert("crpix_center", true);

    if (m_ActiveParameters.downsample != 1)
        uploadReq.insert("downsample_factor", m_ActiveParameters.downsample);

    uploadReq.insert("parity", m_ActiveParameters.search_parity);

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
    emit logOutput(("Uploading file..."));
}

//This will start up the third stage, waiting till processing is done
void OnlineSolver::waitForProcessing()
{
    workflowStage = JOB_PROCESSING_STAGE;
    emit logOutput(("Waiting for Processing to complete..."));
}

//This will start up the fourth stage, getting the Job ID, essentially waiting in the Job Queue
void OnlineSolver::getJobID()
{
    workflowStage = JOB_QUEUE_STAGE;
    emit logOutput(("Waiting for the Job to Start..."));
}

//This will start the fifth stage, monitoring the job to see when it's done
void OnlineSolver::startMonitoring()
{
    workflowStage = JOB_MONITORING_STAGE;
    emit logOutput(("Starting Job Monitoring..."));
}

//This will start the sixth stage, checking the results
void OnlineSolver::checkJobCalibration()
{
    QNetworkRequest request;
    QUrl getCablirationResult = QUrl(QString("%1/api/jobs/%2/calibration").arg(astrometryAPIURL).arg(jobID));
    request.setUrl(getCablirationResult);
    networkManager->get(request);

    workflowStage = JOB_CALIBRATION_STAGE;
    emit logOutput(("Requesting the results..."));
}

//This will start the seventh stage, getting the Job LOG file and loading it (optional).
void OnlineSolver::getJobLogFile()
{
    QString URL = QString("%1/joblog/%2").arg(astrometryAPIURL).arg(jobID);
    networkManager->get(QNetworkRequest(QUrl(URL)));

    workflowStage = LOG_LOADING_STAGE;
    emit logOutput(("Downloading the Log file..."));
}

//This will start the eighth stage, getting the WCS File and loading it (optional).
void OnlineSolver::getJobWCSFile()
{
    QString URL = QString("%1/wcs_file/%2").arg(astrometryAPIURL).arg(jobID);
    networkManager->get(QNetworkRequest(QUrl(URL)));

    workflowStage = WCS_LOADING_STAGE;
    emit logOutput(("Downloading the WCS file..."));
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

    if(m_SSLogLevel != LOG_OFF)
        emit logOutput("Reply Received");

    if (workflowStage == NO_STAGE)
    {
        reply->abort();
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        emit logOutput(reply->errorString());
        emit finished(-1);
        return;
    }
    QString json;
    QJsonDocument json_doc;
    QVariant json_result;
    QVariantMap result;
    if(workflowStage != LOG_LOADING_STAGE && workflowStage != WCS_LOADING_STAGE)
    {
        json = (QString)reply->readAll();

        json_doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);

        if (parseError.error != QJsonParseError::NoError)
        {
            emit logOutput(QString("JSON error during parsing (%1).").arg(parseError.errorString()));
            emit finished(-1);
            return;
        }

        json_result = json_doc.toVariant();
        result   = json_result.toMap();

        if(m_SSLogLevel != LOG_OFF)
            emit logOutput(json_doc.toJson(QJsonDocument::Compact));
    }
    switch (workflowStage)
    {
        case AUTH_STAGE:
            status = result["status"].toString();
            if (status != "success")
            {
                emit logOutput(QString("%1 authentication failed. Check the validity of the API Key.").arg(astrometryAPIURL));
                abort();
                return;
            }

            sessionKey = result["session"].toString();

            if(m_SSLogLevel != LOG_OFF)
                emit logOutput(QString("Authentication to %1 is successful. Session: %2").arg(astrometryAPIURL).arg(sessionKey));

            uploadFile(); //Go to NEXT STAGE
            break;

        case UPLOAD_STAGE:
            status = result["status"].toString();
            if (status != "success")
            {
                emit logOutput(("Upload failed."));
                abort();
                return;
            }

            subID = result["subid"].toInt(&ok);

            if (ok == false)
            {
                emit logOutput(("Parsing submission ID failed."));
                abort();
                return;
            }

            emit logOutput(QString("Upload complete. Waiting for %1 solver to complete...").arg(astrometryAPIURL));
            waitForProcessing();  //Go to the NEXT STAGE
            break;

        case JOB_PROCESSING_STAGE:
        {
            QString finished;
            finished = result["processing_finished"].toString();

            if (finished == "None" || finished == "")
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
                emit logOutput("Solver Failed");
                abort();
                return;
            }
            break;

        case JOB_CALIBRATION_STAGE:
        {
            double fieldw = result["width_arcsec"].toDouble(&ok) / 60.0;
            if (ok == false)
            {
                emit logOutput(("Error parsing width."));
                abort();
                return;
            }
            double fieldh = result["height_arcsec"].toDouble(&ok) / 60.0;
            if (ok == false)
            {
                emit logOutput(("Error parsing width."));
                abort();
                return;
            }
            int parity = result["parity"].toInt(&ok);
            if (ok == false)
            {
                emit logOutput(("Error parsing parity."));
                abort();
                return;
            }
            double orientation = result["orientation"].toDouble(&ok);
            if (ok == false)
            {
                emit logOutput(("Error parsing orientation."));
                abort();
                return;
            }
            orientation *= parity;
            double ra = result["ra"].toDouble(&ok);
            if (ok == false)
            {
                emit logOutput(("Error parsing RA."));
                abort();
                return;
            }
            double dec = result["dec"].toDouble(&ok);
            if (ok == false)
            {
                emit logOutput(("Error parsing DEC."));
                abort();
                return;
            }

            double pixscale = result["pixscale"].toDouble(&ok);
            if (ok == false)
            {
                emit logOutput(("Error parsing DEC."));
                abort();
                return;
            }

            float raErr = 0;
            float decErr = 0;
            if(m_UsePosition)
            {
                raErr = (search_ra - ra) * 3600;
                decErr = (search_dec - dec) * 3600;
            }
            QString par = (parity > 0) ? "neg" : "pos";
            m_Solution = {fieldw, fieldh, ra, dec, orientation, pixscale, par, raErr, decErr};
            m_HasSolved = true;

            if(m_AstrometryLogLevel != LOG_NONE || m_LogToFile)
                getJobLogFile(); //Go to next stage
            else
                getJobWCSFile(); //Go to Last Stage
        }
        break;

        case LOG_LOADING_STAGE:
        {
            QByteArray responseData = reply->readAll();

            if(m_AstrometryLogLevel != LOG_NONE && !m_LogToFile)
                emit logOutput(responseData);

            if(m_LogToFile)
            {
                QFile file(m_LogFileName);
                if (!file.open(QIODevice::WriteOnly))
                {
                    emit logOutput(("Log File Write Error"));
                }
                file.write(responseData.data(), responseData.size());
                file.close();
            }
            getJobWCSFile(); //Go to Last Stage
        }
        break;

        case WCS_LOADING_STAGE:
        {
            QByteArray responseData = reply->readAll();
            QString solutionFile = m_BasePath + "/" + m_BaseName + ".wcs";
            QFile file(solutionFile);
            if (!file.open(QIODevice::WriteOnly))
            {
                emit logOutput(("WCS File Write Error"));
                emit finished(0); //We still have the solution, this is not a failure!
                return;
            }
            file.write(responseData.data(), responseData.size());
            file.close();
            loadWCS(); //Attempt to load WCS from the file
            emit finished(0); //Success! We are completely done, whether or not the WCS loading was successful
            workflowStage = NO_STAGE;
        }
        break;

        default:
            break;
    }
}

