#include "onlinesolver.h"
#include <QTimer>
#include <QEventLoop>

OnlineSolver::OnlineSolver(ProcessType type, Statistic imagestats, uint8_t *imageBuffer, QObject *parent) : ExternalSextractorSolver(type, imagestats, imageBuffer, parent)
{
    connect(this, SIGNAL(authenticateFinished()), this, SLOT(uploadFile()));
    connect(this, SIGNAL(uploadFinished()), this, SLOT(getJobID()));
    connect(this, SIGNAL(timeToCheckJobs()), this, SLOT(checkJobs()));
    connect(this, SIGNAL(jobFinished()), this, SLOT(checkJobCalibration()));
}

OnlineSolver::~OnlineSolver()
{

}

void OnlineSolver::solve()
{
    if(processType == ONLINE_ASTROMETRY_NET)
        setupOnlineSolver();

    if(processType == INT_SEP_ONLINE_ASTROMETRY_NET)
    {
        delete xcol;
        delete ycol;
        xcol=strdup("X"); //This is the column for the x-coordinates
        ycol=strdup("Y"); //This is the column for the y-coordinates
        if(runSEPSextractor())
        {
            int success = writeSextractorTable();
            if(success == 0)
                setupOnlineSolver();
        }
        else
            emit finished(-1);
    }
}

void OnlineSolver::setupOnlineSolver()
{
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logNeedsUpdating("Configuring Online Solver");

    job_retries = 0;

    // Reset params
    units.clear();
    useWCSCenter = false;

    networkManager = new QNetworkAccessManager();
    connect(networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onResult(QNetworkReply*)));

    solverTimer.start();

    if (sessionKey.isEmpty())
        authenticate();
    else
        uploadFile();
}

void OnlineSolver::run()
{
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logNeedsUpdating("Starting Online Solver");

    int elapsed = 0;
    while(!hasSolved && !aborted && elapsed < params.solverTimeLimit)
    {
        msleep(SOLVER_RETRY_DURATION);
        emit timeToCheckJobs();
        elapsed = static_cast<int>(round(solverTimer.elapsed() / 1000.0));
    }

    disconnect(networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onResult(QNetworkReply*)));

    if(elapsed > params.solverTimeLimit)
    {
        emit logNeedsUpdating("Solver timed out");
        emit finished(-1);
    }

}


void OnlineSolver::abort()
{
    workflowStage  = NO_STAGE;
    emit finished(-1);
    aborted = true;
}

void OnlineSolver::authenticate()
{
    QNetworkRequest request;
    //request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    // If pure IP, add http to it.
    if (!astrometryAPIURL.startsWith("http"))
        astrometryAPIURL = "http://" + astrometryAPIURL;

    QUrl url(astrometryAPIURL);
    url.setPath("/api/login");
    request.setUrl(url);

    QVariantMap apiReq;
    apiReq.insert("apikey", astrometryAPIKey);
    //QByteArray json = serializer.serialize(apiReq, &ok);
    QJsonObject json = QJsonObject::fromVariantMap(apiReq);
    QJsonDocument json_doc(json);

    QString json_request = QString("request-json=%1").arg(QString(json_doc.toJson(QJsonDocument::Compact)));

    workflowStage = AUTH_STAGE;

    networkManager->post(request, json_request.toUtf8());

    emit logNeedsUpdating("Authenticating. . .");

}

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

    workflowStage = UPLOAD_STAGE;

    emit logNeedsUpdating(("Uploading file..."));

    QNetworkReply *reply = networkManager->post(request, reqEntity);

    // The entity should be deleted when reply is finished
    reqEntity->setParent(reply);
}

void OnlineSolver::getJobID()
{
    QNetworkRequest request;
    QUrl getJobID = QUrl(QString("%1/api/submissions/%2").arg(astrometryAPIURL).arg(subID));

    request.setUrl(getJobID);

    workflowStage = JOB_ID_STAGE;

    networkManager->get(request);
}

void OnlineSolver::checkJobs()
{
    //qDebug() << "with jobID " << jobID << endl;

    QNetworkRequest request;
    QUrl getJobStatus = QUrl(QString("%1/api/jobs/%2").arg(astrometryAPIURL).arg(jobID));

    request.setUrl(getJobStatus);

    workflowStage = JOB_STATUS_STAGE;

    networkManager->get(request);
}

void OnlineSolver::checkJobCalibration()
{
    QNetworkRequest request;
    QUrl getCablirationResult = QUrl(QString("%1/api/jobs/%2/calibration").arg(astrometryAPIURL).arg(jobID));

    request.setUrl(getCablirationResult);

    workflowStage = JOB_CALIBRATION_STAGE;

    networkManager->get(request);
}

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

    QString json = (QString)reply->readAll();

    QJsonDocument json_doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        emit logNeedsUpdating(QString("JSON error during parsing (%1).").arg(parseError.errorString()));
        emit finished(-1);
        return;
    }

    QVariant json_result = json_doc.toVariant();
    QVariantMap result   = json_result.toMap();

    //if (Options::alignmentLogging())
    //    align->appendLogText(json_doc.toJson(QJsonDocument::Compact));
    emit logNeedsUpdating(json_doc.toJson(QJsonDocument::Compact));

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

            emit authenticateFinished();
            break;

        case UPLOAD_STAGE:
            status = result["status"].toString();
            if (status != "success")
            {
                emit logNeedsUpdating(("Upload failed."));
                emit finished(-1);
                return;
            }

            subID = result["subid"].toInt(&ok);

            if (ok == false)
            {
                emit logNeedsUpdating(("Parsing submission ID failed."));
                emit finished(-1);
                return;
            }

            emit logNeedsUpdating(("Upload complete. Waiting for astrometry.net solver to complete..."));
            emit uploadFinished();
            break;

        case JOB_ID_STAGE:
            jsonArray = result["jobs"].toList();

            if (jsonArray.isEmpty())
                jobID = 0;
            else
                jobID = jsonArray.first().toInt(&ok);

            if (jobID == 0 || !ok)
            {
                if (job_retries++ < JOB_RETRY_ATTEMPTS)
                {
                    QTimer::singleShot(JOB_RETRY_DURATION, this, SLOT(getJobID()));
                    return;
                }
                else
                {
                    emit logNeedsUpdating(("Failed to retrieve job ID."));
                    emit finished(-1);
                    return;
                }
            }

            job_retries = 0;

            //This starts the online solver monitoring thread
            start();
            break;

        case JOB_STATUS_STAGE:
            status = result["status"].toString();
            if (status == "success")
                emit jobFinished();
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
                emit finished(-1);
                return;
            }
            double fieldh = result["height_arcsec"].toDouble(&ok) / 60.0;
            if (ok == false)
            {
                emit logNeedsUpdating(("Error parsing width."));
                emit finished(-1);
                return;
            }
            int parity = result["parity"].toInt(&ok);
            if (ok == false)
            {
                emit logNeedsUpdating(("Error parsing parity."));
                emit finished(-1);
                return;
            }
            double orientation = result["orientation"].toDouble(&ok);
            if (ok == false)
            {
                emit logNeedsUpdating(("Error parsing orientation."));
                emit finished(-1);
                return;
            }
            orientation *= parity;
            double ra = result["ra"].toDouble(&ok);
            if (ok == false)
            {
                emit logNeedsUpdating(("Error parsing RA."));
                emit finished(-1);
                return;
            }
            double dec = result["dec"].toDouble(&ok);
            if (ok == false)
            {
                emit logNeedsUpdating(("Error parsing DEC."));
                emit finished(-1);
                return;
            }

            double pixscale = result["pixscale"].toDouble(&ok);
            if (ok == false)
            {
                emit logNeedsUpdating(("Error parsing DEC."));
                emit finished(-1);
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
            emit finished(0);
        }
            break;

        default:
            break;
    }
}

