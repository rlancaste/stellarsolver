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
#define SOLVER_RETRY_DURATION 2000 /* 2000 ms */

class OnlineSolver : public ExternalSextractorSolver
{
    Q_OBJECT
public:
    explicit OnlineSolver(ProcessType type, Statistic imagestats, uint8_t *imageBuffer, QObject *parent);
    ~OnlineSolver();

    QString astrometryAPIKey;
    QString astrometryAPIURL;
    QString fileToProcess;

    void solve() override;
    void abort() override;

    typedef enum
    {
        NO_STAGE,
        AUTH_STAGE,
        UPLOAD_STAGE,
        JOB_ID_STAGE,
        JOB_STATUS_STAGE,
        JOB_CALIBRATION_STAGE
    } WorkflowStage;

public slots:
    void onResult(QNetworkReply *reply);
    void uploadFile();
    void getJobID();
    void checkJobs();
    void checkJobCalibration();
    //void resetSolver();

private:
    void authenticate();

    void setupOnlineSolver();
    void run() override;
    bool aborted = false;

    WorkflowStage workflowStage { NO_STAGE };
    QNetworkAccessManager *networkManager { nullptr };
    QString sessionKey;
    int subID { 0 };
    int jobID { 0 };
    int job_retries { 0 };
    QElapsedTimer solverTimer;

    bool useWCSCenter { false };

signals:
    void authenticateFinished();
    void uploadFinished();
    void jobIDFinished();
    void jobFinished();
    void timeToCheckJobs();

};

#endif // ONLINESOLVER_H
