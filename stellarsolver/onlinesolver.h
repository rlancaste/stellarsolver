/*  OnlineSolver, StellarSolver Intenal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#pragma once

//Qt Includes
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

//Project Includes
#include "externalextractorsolver.h"

#define JOB_RETRY_DURATION    2000 /* 2000 ms */
#define JOB_RETRY_ATTEMPTS    90
#define STATUS_CHECK_INTERVAL 2000 /* 2000 ms */

using namespace SSolver;

class OnlineSolver : public ExternalExtractorSolver
{
        Q_OBJECT
    public:
        explicit OnlineSolver(ProcessType type, ExtractorType sexType, SolverType solType, const FITSImage::Statistic &imagestats,
                              uint8_t const *imageBuffer, QObject *parent);

        QString astrometryAPIKey;   // The API key used by the online solver to identify the user solving the image
        QString astrometryAPIURL;   // The URL of the online solver
        QString fileToProcess;      // The file path of the image to solve

        // An enum to keep track of which stage in the solving we are on
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

        /**
         * @brief execute is called to start up the online solver
         */
        void execute() override;

        /**
         * @brief abort will stop the extractorsolver by using the quit method and disconnecting from the server
         */
        void abort() override;

    public slots:

        /**
         * @brief onResult is the slot that gets called when the server replies to a request
         * @param reply is the reply that was received
         */
        void onResult(QNetworkReply *reply);

        /**
         * @brief checkJobs will check on the job status periodically while solving is happening
         */
        void checkJobs();

    private:

        // This keeps track of which stage in the solve we are currently on
        WorkflowStage workflowStage { NO_STAGE };
        // This is the network manager that handles all the online communication with the solver
        QNetworkAccessManager *networkManager { nullptr };
        QString sessionKey;         // This is the session key reported by the online solver
        int subID { 0 };            // This is the submission id reported by the online solver
        int jobID { 0 };            // This is the job id issued by the online solver
        int job_retries { 0 };      // Keeps track of how many times it retried to start the solving task
        QElapsedTimer solverTimer;  // This logs how long the online solver has been running

        /**
         * @brief runOnlineSolver Starts up the other thread to run and monitor the online solver
         */
        void runOnlineSolver();

        /**
         * @brief run is the method running in a separate thread to monitor and respond to the online solver
         */
        void run() override;

        /**
         * @brief authenticate Starts Stage 1, authenticating with the online server
         */
        void authenticate();

        /**
         * @brief uploadFile Starts Stage 2, uploading the file to the online server
         */
        void uploadFile();

        /**
         * @brief waitForProcessing Starts Stage 3, waiting for the processing to start
         */
        void waitForProcessing();

        /**
         * @brief getJobID Starts Stage 4, getting the JobID from the online server
         */
        void getJobID();

        /**
         * @brief startMonitoring Starts Stage 5, monitoring for the results from the online server
         */
        void startMonitoring();

        /**
         * @brief checkJobCalibration Starts Stage 6, checking the job calibration
         */
        void checkJobCalibration();

        /**
         * @brief getJobLogFile Starts Stage 7, getting the log file from the online server
         */
        void getJobLogFile();

        /**
         * @brief getJobWCSFile Starts Stage 8, getting the WCS information file from the server
         */
        void getJobWCSFile();

    signals:
        /**
         * @brief timeToCheckJobs indicates that it is time to check on the jobs running on the server
         */
        void timeToCheckJobs();

        /**
         * @brief startupOnlineSolver signals that it is time to start up the first stage
         */
        void startupOnlineSolver();

};

