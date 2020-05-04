#ifndef ONLINESOLVER_H
#define ONLINESOLVER_H

#include "sexysolver.h"

#include <QFile>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QVariantMap>
#include <QTime>

#define JOB_RETRY_DURATION    2000 /* 2000 ms */
#define JOB_RETRY_ATTEMPTS    90
#define SOLVER_RETRY_DURATION 2000 /* 2000 ms */

class OnlineSolver : public SexySolver
{
    Q_OBJECT
public:
    explicit OnlineSolver(ProcessType type, Statistic imagestats, uint8_t *imageBuffer, QObject *parent);
    ~OnlineSolver();

    QString IP;
    QString astrometryAPIKey;
    QString astrometryAPIURL;
    QString fileToProcess;

    void solve() override;
    void abort() override;

    //virtual bool init();
    //virtual void verifyIndexFiles(double fov_x, double fov_y);
    //virtual bool startSovler(const QString &filename, const QStringList &args, bool generated = true);
    //virtual bool stopSolver();

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

    //void run() override;
    bool aborted = false;

    const int INVALID_VALUE = -1e6;

    WorkflowStage workflowStage { NO_STAGE };
    QNetworkAccessManager *networkManager { nullptr };
    QString sessionKey;
    int subID { 0 };
    int jobID { 0 };
    int job_retries { 0 };
    int solver_retries { 0 };
    QTime solverTimer;

    double ra{0};
    double dec{0};
    double pixscale { 0 };
    bool useWCSCenter { false };
    int parity { 0 };

    double orientation { 0 };

    bool isGenerated { true };
    //Align *align { nullptr };

signals:
    void authenticateFinished();
    void uploadFinished();
    void jobIDFinished();
    void jobFinished();
    void timeToCheckJobs();

};

#endif // ONLINESOLVER_H
