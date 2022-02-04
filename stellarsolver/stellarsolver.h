/*  StellarSolver, StellarSolver Intenal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#pragma once

//Includes for this project
#include "structuredefinitions.h"
#include "sextractorsolver.h"
#include "parameters.h"
#include "version.h"

//QT Includes
#include <QDir>
#include <QThread>
#include <QMap>
#include <QVariant>
#include <QVector>
#include <QRect>
#include <QPointer>

using namespace SSolver;

class StellarSolver : public QObject
{
        Q_OBJECT
        Q_PROPERTY(QString BasePath MEMBER m_BasePath)
        Q_PROPERTY(QString FileToProcess MEMBER m_FileToProcess)
        Q_PROPERTY(QString ASTAPBinaryPath MEMBER m_ASTAPBinaryPath)
        Q_PROPERTY(QString SextractorBinaryPath MEMBER m_SextractorBinaryPath)
        Q_PROPERTY(QString ConfPath MEMBER m_ConfPath)
        Q_PROPERTY(QString SolverPath MEMBER m_SolverPath)
        Q_PROPERTY(QString WCSPath MEMBER m_WCSPath)
        Q_PROPERTY(QString AstrometryAPIKey MEMBER m_AstrometryAPIKey)
        Q_PROPERTY(QString AstrometryAPIURL MEMBER m_AstrometryAPIURL)
        Q_PROPERTY(QString LogFileName MEMBER m_LogFileName)
        Q_PROPERTY(bool UsePosition MEMBER m_UsePosition)
        Q_PROPERTY(bool UseScale MEMBER m_UseScale)
        Q_PROPERTY(bool AutoGenerateAstroConfig MEMBER m_AutoGenerateAstroConfig)
        Q_PROPERTY(bool CleanupTemporaryFiles MEMBER m_CleanupTemporaryFiles)
        Q_PROPERTY(bool OnlySendFITSFiles MEMBER m_OnlySendFITSFiles)
        Q_PROPERTY(bool LogToFile MEMBER m_LogToFile)
        Q_PROPERTY(SolverType SolverType MEMBER m_SolverType)
        Q_PROPERTY(ProcessType ProcessType MEMBER m_ProcessType)
        Q_PROPERTY(ExtractorType ExtractorType MEMBER m_SextractorType)

    public:
        //The constructor and destructor foe the StellarSolver Object
        explicit StellarSolver(ProcessType type, const FITSImage::Statistic &imagestats, uint8_t const *imageBuffer,
                               QObject *parent = nullptr);
        explicit StellarSolver(const FITSImage::Statistic &imagestats,  uint8_t const *imageBuffer, QObject *parent = nullptr);
        ~StellarSolver();

        //Methods to get default file paths
        static ExternalProgramPaths getLinuxDefaultPaths();
        static ExternalProgramPaths getLinuxInternalPaths();
        static ExternalProgramPaths getMacHomebrewPaths();
        static ExternalProgramPaths getMacInternalPaths();
        static ExternalProgramPaths getWinANSVRPaths();
        static ExternalProgramPaths getWinCygwinPaths();

        //This gets the processType as a string explaining the command StellarSolver is Running
        QString getCommandString()
        {
            return SSolver::getCommandString(m_ProcessType, m_SextractorType, m_SolverType);
        }

        //This gets the scale unit string for astrometry.net input
        //This should NOT be translated
        QString getScaleUnitString()
        {
            return SSolver::getScaleUnitString(m_ScaleUnit);
        }

        //This gets a string for the Sextractor setting for calculating Flux using ellipses or circles
        QString getShapeString()
        {
            return SSolver::getShapeString(params.apertureShape);
        }

        //This gets a string for which Parallel Solving Algorithm we are using
        QString getMultiAlgoString()
        {
            return SSolver::getMultiAlgoString(params.multiAlgorithm);
        }

        QString getLogLevelString()
        {
            return SSolver::getLogLevelString(m_AstrometryLogLevel);
        }

        static QString getVersion()
        {
            return QString("StellarSolver Library Version: %1, build: %2").arg(StellarSolver_VERSION).arg(StellarSolver_BUILD_TS);
        }

        static QString getVersionNumber()
        {
            return StellarSolver_VERSION;
        }

        //Logging Settings for Astrometry
        bool m_LogToFile {false};             //This determines whether or not to save the output from Astrometry.net to a file
        QString m_LogFileName;                //This is the path to the log file that it will save.
        logging_level m_AstrometryLogLevel {LOG_NONE};   //This is the level of astrometry logging.  Beware, setting this too high can severely affect performance
        SSolverLogLevel m_SSLogLevel {LOG_NORMAL};   //This is the level for the StellarSolver Logging

        //These are for creating temporary files
        //This is the base name used for all temporary files.  It uses a random name based on the type of solver/sextractor.
        QString m_BaseName;
        //This is the path used for saving any temporary files.  They are by default saved to the default temp directory, you can change it if you want to.
        QString m_BasePath {QDir::tempPath()};

        //STELLARSOLVER METHODS
        //The public methods here are for you to start, stop, setup, and get results from the StellarSolver

        //These are the most important methods that you can use for the StellarSolver

        /**
         * @brief sextract Extracts stars from the image.
         * @param calculateHFR If true, it will also calculated Half-Flux Radius for each detected star. HFR calculations can be very CPU-intensive.
         * @param frame If set, it will only extract stars within this rectangular region of the image.
         */
        void extract(bool calculateHFR = false, QRect frame = QRect());

        void solve();
        void start();
        void releaseSextractorSolver(SextractorSolver *solver);
        void abort();

        //These set the settings for the StellarSolver
        void setParameters(Parameters parameters)
        {
            params = parameters;
        };
        void setParameterProfile(SSolver::Parameters::ParametersProfile profile);
        void setIndexFolderPaths(QStringList indexPaths)
        {
            indexFolderPaths = indexPaths;
        };
        void setSearchScale(double fov_low, double fov_high, const QString &scaleUnits);
        //This sets the scale range for the image to speed up the solver
        void setSearchScale(double fov_low, double fov_high, ScaleUnits units);
        void setSearchPositionRaDec(double ra,
                                    double dec);                                                    //This sets the search RA/DEC/Radius to speed up the solver
        void setSearchPositionInDegrees(double ra, double dec);
        void setLogLevel(logging_level level)
        {
            m_AstrometryLogLevel = level;
        };
        void setSSLogLevel(SSolverLogLevel level)
        {
            m_SSLogLevel = level;
        };

        //These static methods can be used by classes to configure parameters or paths
        //This creates the conv filter from a fwhm
        static void createConvFilterFromFWHM(Parameters *params, double fwhm);
        static QList<Parameters> getBuiltInProfiles();
        static QList<SSolver::Parameters> loadSavedOptionsProfiles(QString savedOptionsProfiles);
        static QStringList getDefaultIndexFolderPaths();


        //Accessor Method for external classes
        int getNumStarsFound() const
        {
            return numStars;
        }
        const QList<FITSImage::Star> &getStarList() const
        {
            return m_ExtractorStars;
        }
        const FITSImage::Background &getBackground() const
        {
            return background;
        }
        const QList<FITSImage::Star> &getStarListFromSolve() const
        {
            return m_SolverStars;
        }
        const FITSImage::Solution &getSolution() const
        {
            return solution;
        }

        bool sextractionDone() const
        {
            return m_HasExtracted;
        }
        bool solvingDone() const
        {
            return m_HasSolved;
        }

        bool failed() const
        {
            return m_HasFailed;
        }
        bool hasWCSData() const
        {
            return hasWCS;
        };
        int getNumThreads() const
        {
            if(parallelSolvers.size() == 0) return 1;
            else return parallelSolvers.size();
        }

        const Parameters &getCurrentParameters()
        {
            return params;
        }
        bool isCalculatingHFR()
        {
            return m_CalculateHFR;
        }

        void setUseSubframe(QRect frame);
        void clearSubFrame()
        {
            useSubframe = false;
            m_Subframe = QRect(0, 0, m_Statistics.width, m_Statistics.height);
        };

        bool isRunning() const;

        inline static QString raString(double ra)
        {
            char rastr[32];
            ra2hmsstring(ra, rastr);
            return rastr;
        }

        inline static QString decString(double dec)
        {
            char decstr[32];
            dec2dmsstring(dec, decstr);
            return decstr;
        }

        bool pixelToWCS(const QPointF &pixelPoint, FITSImage::wcs_point &skyPoint);
        bool wcsToPixel(const FITSImage::wcs_point &skyPoint, QPointF &pixelPoint);


    public slots:
        void processFinished(int code);
        void parallelSolve();
        void finishParallelSolve(int success);

    private:
        int whichSolver(SextractorSolver *solver);
        //Static Utility
        static double snr(const FITSImage::Background &background,
                          const FITSImage::Star &star, double gain = 0.5);


        bool appendStarsRAandDEC(QList<FITSImage::Star> &stars);

        bool checkParameters();
        SextractorSolver* createSextractorSolver();

        //This finds out the amount of available RAM on the system
        bool getAvailableRAM(double &availableRAM, double &totalRAM);
        //This determines if there is enough RAM for the selected index files so that we don't try to load indexes inParallel unless it can handle it.
        bool enoughRAMisAvailableFor(QStringList indexFolders);

        //This defines the type of process to perform.
        ProcessType m_ProcessType { EXTRACT };
        ExtractorType m_SextractorType { EXTRACTOR_INTERNAL };
        SolverType m_SolverType {SOLVER_STELLARSOLVER};

        //External Options
        QString m_FileToProcess;
        bool m_CleanupTemporaryFiles {true};
        bool m_AutoGenerateAstroConfig {true};
        bool m_OnlySendFITSFiles {true};

        //System File Paths
        QStringList m_IndexFilePaths;
        QString m_ASTAPBinaryPath;
        QString m_SextractorBinaryPath;
        QString m_ConfPath;
        QString m_SolverPath;
        QString m_WCSPath;

        //Online Options
        QString m_AstrometryAPIKey;
        QString m_AstrometryAPIURL;

        bool useSubframe {false};
        QRect m_Subframe;
        bool m_isRunning {false};
        QList<SextractorSolver*> parallelSolvers;
        QPointer<SextractorSolver> m_SextractorSolver;
        QPointer<SextractorSolver> solverWithWCS;
        int m_ParallelSolversFinishedCount {0};
        bool parallelSolversAreRunning() const;

        //The currently set parameters for StellarSolver
        Parameters params;
        //This is the list of folder paths that the solver will use to search for index files
        QStringList indexFolderPaths {getDefaultIndexFolderPaths()};

        //Astrometry Scale Parameters, These are not saved parameters and change for each image, use the methods to set them
        bool m_UseScale {false};               //Whether or not to use the image scale parameters
        double m_ScaleLow {0};                 //Lower bound of image scale estimate
        double m_ScaleHigh {0};                //Upper bound of image scale estimate
        ScaleUnits m_ScaleUnit {ARCMIN_WIDTH}; //In what units are the lower and upper bounds?

        //Astrometry Position Parameters, These are not saved parameters and change for each image, use the methods to set them
        bool m_UsePosition = false;          //Whether or not to use initial information about the position
        double m_SearchRA = HUGE_VAL;        //RA of field center for search, format: decimal degrees
        double m_SearchDE = HUGE_VAL;       //DEC of field center for search, format: decimal degrees

        //StellarSolver Internal settings that are needed by ExternalSextractorSolver as well
        bool m_CalculateHFR {false};          //Whether or not the HFR of the image should be calculated using sep_flux_radius.  Don't do it unless you need HFR
        bool m_HasExtracted {false};         //This boolean is set when the sextraction is done
        bool m_HasSolved {false};             //This boolean is set when the solving is done
        bool m_HasFailed {false};
        FITSImage::Statistic m_Statistics;                    //This is information about the image

        const uint8_t *m_ImageBuffer { nullptr }; //The generic data buffer containing the image data

        //The Results
        FITSImage::Background background;      //This is a report on the background levels found during sextraction
        //This is the list of stars that get extracted from the image, saved to the file, and then solved by astrometry.net
        QList<FITSImage::Star> m_ExtractorStars;
        //This is the list of stars that were extracted for the last successful solve
        QList<FITSImage::Star> m_SolverStars;
        //The number of stars found in the last operation
        int numStars;
        FITSImage::Solution solution;          //This is the solution that comes back from the Solver
        bool hasWCS {false};        //This boolean gets set if the StellarSolver has WCS data to retrieve

        bool wasAborted {false};
        // This is the cancel file path that astrometry.net monitors.  If it detects this file, it aborts the solve
        QString cancelfn;           //Filename whose creation signals the process to stop
        QString solvedfn;           //Filename whose creation tells astrometry.net it already solved the field.

        bool m_isBlocking = false;              //This boolean determines whether to run in a blocking way.

    signals:
        //This signals that there is infomation that should be printed to a log file or log window
        void logOutput(QString logText);

        // Extraction and/or solving complete.
        // If can be completed successfully or in failure, but it's done.
        void ready();

        void finished();

};

