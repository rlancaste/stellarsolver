/*  OnlineSovler, StellarSolver Intenal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#ifndef ONLINESOLVER_H
#define ONLINESOLVER_H

#include "externalsextractorsolver.h"

#include <QFile>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QVariantMap>
#include <QTime>
#include <QElapsedTimer>

#define JOB_RETRY_DURATION    2000 /* 2000 ms */
#define JOB_RETRY_ATTEMPTS    90
#define STATUS_CHECK_INTERVAL 2000 /* 2000 ms */

using namespace SSolver;

class OnlineSolver : public ExternalSextractorSolver
{
    Q_OBJECT
public:
    explicit OnlineSolver(ProcessType type, FITSImage::Statistic imagestats, uint8_t const *imageBuffer, QObject *parent);

    QString astrometryAPIKey;
    QString astrometryAPIURL;
    QString fileToProcess;

    void startProcess() override;
    //void executeProcess() override;
    void abort() override;

    typedef enum
    {
        NO_STAGE,
        AUTH_STAGE,
        UPLOAD_STAGE,
        JOB_PROCESSING_STAGE,
        JOB_QUEUE_STAGE,
        JOB_MONITORING_STAGE,
        JOB_CALIBRATION_STAGE,
        LOG_LOADING_STAGE,
        WCS_LOADING_STAGE
    } WorkflowStage;

public slots:

    void onResult(QNetworkReply *reply);
    void checkJobs();

private:

    void runOnlineSolver();
    void run() override;
    bool aborted = false;

    void authenticate();        //Starts Stage 1
    void uploadFile();          //Starts Stage 2
    void waitForProcessing();   //Starts Stage 3
    void getJobID();            //Starts Stage 4
    void startMonitoring();     //Starts Stage 5
    void checkJobCalibration(); //Starts Stage 6
    void getJobLogFile();       //Starts Stage 7
    void getJobWCSFile();       //Starts Stage 8

    WorkflowStage workflowStage { NO_STAGE };
    QNetworkAccessManager *networkManager { nullptr };
    QString sessionKey;
    int subID { 0 };
    int jobID { 0 };
    int job_retries { 0 };
    QElapsedTimer solverTimer;

signals:
    void timeToCheckJobs();
    void startupOnlineSolver();

};

#endif // ONLINESOLVER_H
