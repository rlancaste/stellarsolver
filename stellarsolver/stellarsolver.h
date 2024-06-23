/*  StellarSolver, StellarSolver Intenal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#pragma once

//Includes for this project
#include "extractorsolver.h"
#include "structuredefinitions.h"
#include "wcsdata.h"

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
        Q_PROPERTY(ExtractorType ExtractorType MEMBER m_ExtractorType)

    public:
        /**
         * @brief StellarSolver This constructor makes a StellarSolver without any image or information
         * @param parent The parent of this StellarSolver, allows it to be deleted with its parent, defaults to nullptr
         */
        explicit StellarSolver(QObject *parent = nullptr);

         /**
         * @brief StellarSolver This constructor makes a StellarSolver with just the image information
         * @param imagestats Information about the imagebuffer provided
         * @param imageBuffer The imagebuffer to be processed
         * @param parent The parent of this StellarSolver, allows it to be deleted with its parent, defaults to nullptr
         */
        explicit StellarSolver(const FITSImage::Statistic &imagestats,  uint8_t const *imageBuffer, QObject *parent = nullptr);

        /**
         * @brief StellarSolver This constructor makes a StellarSolver with the ProcessType included
         * @param type The type of process you are planning to perform on the image
         * @param imagestats Information about the imagebuffer provided
         * @param imageBuffer The imagebuffer to be processed
         * @param parent The parent of this StellarSolver, allows it to be deleted with its parent, defaults to nullptr
         */
        explicit StellarSolver(ProcessType type, const FITSImage::Statistic &imagestats, uint8_t const *imageBuffer, QObject *parent = nullptr);

        //This deletes the StellarSolver
        ~StellarSolver();

        /**
         * @brief loadNewImageBuffer loads a new image buffer for StellarSolver to process
         * @param imagestats Information about the imagebuffer provided
         * @param imageBuffer The imagebuffer to be processed
         * @return whether or not it succesfully loaded a new image.  It will not be successful if you try to load a null image buffer or if a process is running.
         */
        bool loadNewImageBuffer(const FITSImage::Statistic &imagestats,  uint8_t const *imageBuffer);

        /**
         * @brief getDefaultExternalPaths gets the default external program paths appropriate for the selected Computer System
         * @param system is the selected system setup
         * @return The appropriate ExernalProgramPaths Object
         */
        static ExternalProgramPaths getDefaultExternalPaths(ComputerSystemType system);

        /**
         * @brief getDefaultExternalPaths gets the default external program paths appropriate for the current Computer System
         * @param system is the selected system setup
         * @return The appropriate ExernalProgramPaths Object
         */
        static ExternalProgramPaths getDefaultExternalPaths();


        // Notes for the function below:
        // Return the full path to index files to use when solving.
        // The input is a list of directory names, and index and healpix constraints.
        // If indexToUse and healpixToUse are -1, then return all the .fit or .fits
        // files in the directories. If indexToUse >= 0, then constrain the list to
        // just those of the correct index. If healpixToUse is also >= 0 then
        // further constrain the list to correct healpix. It is assumed the index
        // filename format is index-$INDEX.fit or .fits, or index-$INDEX-$HH.fit or
        // .fits where $HH is a 2-character healpix number, e.g. index-4205-03.fits.

        /**
         * @brief getIndexFiles This lets you get a list of paths to index files to pass to astrometry instead of letting it automatically search.
         * @param directoryList This is the list of directory names to search for index files
         * @param indexToUse If you know which index series should solve it, this lets you constrain the list to that index series.
         * @param healpixToUse If you further know which healpix to use, this lets you constrain to just that index file
         * @return The list of index files to use
         */
        static QStringList getIndexFiles(const QStringList &directoryList, int indexToUse = -1, int healpixToUse = -1);
  
        /**
         * @brief getCommandString gets the processType as a string explaining the command StellarSolver is Running
         * @return The details of what StellarSolver is doing.
         */
        QString getCommandString()
        {
            return SSolver::getCommandString(m_ProcessType, m_ExtractorType, m_SolverType);
        }

        /**
         * @brief getScaleUnitString gets a string for the scale units used in the scale for plate solving
         * @return The string for the scale units used.
         */
        QString getScaleUnitString()
        {
            return SSolver::getScaleUnitString(m_ScaleUnit);
        }

        /**
         * @brief getShapeString gets a string for the Star Extractor setting for calculating Flux using ellipses or circles
         * @return The name of the shape
         */
        QString getShapeString()
        {
            return SSolver::getShapeString(params.apertureShape);
        }

        /**
         * @brief getConvFilterString gets a string for the name of the Convolution Filter used in Star Extraction
         * @return The name of the Convolution Filter
         */
        QString getConvFilterString()
        {
            return SSolver::getConvFilterString(params.convFilterType);
        }

        /**
         * @brief getMultiAlgoString gets a string for which algorithm we are using to solve in parallel threads
         * @return The name of the parallel thread solving algorithm
         */
        QString getMultiAlgoString()
        {
            return SSolver::getMultiAlgoString(params.multiAlgorithm);
        }

        /**
         * @brief getLogLevelString gets a string for the current Astrometry Logging Level we are using
         * @return The name of the current logging level
         */
        QString getLogLevelString()
        {
            return SSolver::getLogLevelString(m_AstrometryLogLevel);
        }

        /**
         * @brief getColorChannel Returns which color channel is being used for Star Extraction and Solving in an RGB image
         * @return The Color Channel in use
         */
        FITSImage::ColorChannel getColorChannel()
        {
            return (FITSImage::ColorChannel) m_ColorChannel;
        }

        /**
         * @brief getVersion gets the StellarSolver Version String
         * @return The version string
         */
        static QString getVersion()
        {
            return QString("StellarSolver Library Version: %1, build: %2").arg(StellarSolver_VERSION).arg(StellarSolver_BUILD_TS);
        }

        /**
         * @brief getVersionNumber gets the StellarSolver Version Number
         * @return The version number
         */
        static QString getVersionNumber()
        {
            return StellarSolver_VERSION;
        }

        //STELLARSOLVER METHODS
        //The public methods here are for you to start, stop, setup, and get results from the StellarSolver

        //These are the most important methods that you can use for the StellarSolver

        /**
         * @brief extract Performs Star Extraction on the image.  This is performed synchronously and blocks the calling thread until the finished signal is emitted.
         * @param calculateHFR If true, it will also calculated Half-Flux Radius for each detected star. HFR calculations can be very CPU-intensive.
         * @param frame If set, it will only extract stars within this rectangular region of the image.
         * @return A boolean that reports whether it was successful, true means success.
         */
        bool extract(bool calculateHFR = false, QRect frame = QRect());

        /**
         * @brief solve Plate Solves the image.  This is performed synchronously and blocks the calling thread until the finished signal is emitted.
         * @return A boolean that reports whether it was successful, true means success.
         */
        bool solve();

        /**
         * @brief start Starts a Star Extraction or Plate Solving proccess.  The process is performed asynchronously.  The calling program should then wait for the ready or finished signal.
         */
        void start();

        /**
         * @brief abort will abort the Star Extraction or Plate Solving process.  For external programs it runs the kill command.
         */
        void abort();

        /**
         * @brief abort will abort the Star Extraction or Plate Solving process and wait synchronously till that is done.
         */
        void abortAndWait();

        /**
         * @brief setParameters sets the Parameters for the StellarSolver based on a Parameters object you set up.
         * @param parameters The Parameters object
         */
        void setParameters(const Parameters &parameters)
        {
            params = parameters;
        };

        /**
         * @brief setParameterProfile sets the Parameters for the StellarSolver based on a selection of a built in parameters profile.
         * @param profile The selected built in profile
         */
        void setParameterProfile(SSolver::Parameters::ParametersProfile profile);

        /**
         * @brief setExternalFilePaths sets the external file paths for the external programs
         * @param paths are the paths to set
         */
        void setExternalFilePaths(const ExternalProgramPaths &paths)
        {
            m_ExternalPaths = paths;
        }

        /**
         * @brief setIndexFolderPaths Sets the IndexFolderPaths to automatically search for index files.
         * @param indexPaths A QStringList containing the list of folders to search.
         */
        void setIndexFolderPaths(const QStringList &indexPaths)
        {
            indexFolderPaths = indexPaths;
        };

        /**
         * @brief setIndexFilePaths Sets the IndexFilePaths variable to a list of index file paths to use.
         * @param indexFilePaths A QStringList containing the list of index file paths.
         */
        void setIndexFilePaths(QStringList indexFilePaths)
        {
            m_IndexFilePaths = indexFilePaths;
        };

        /**
         * @brief clearIndexFileAndFolderPaths Clears both the Index File paths and Index Folder paths in case they were set before.
         */
        void clearIndexFileAndFolderPaths()
        {
            m_IndexFilePaths.clear();
            indexFolderPaths.clear();
        }

        /**
         * @brief setSearchScale will set the Search Scale range to speed up the solver based on the ScaleUnits QString
         * @param fov_low The low end of the search range in the specified units
         * @param fov_high The high end of the search range in the specified units
         * @param scaleUnits The specified units for the range.
         */
        void setSearchScale(double fov_low, double fov_high, const QString &scaleUnits);

        /**
         * @brief setSearchScale will set the Search Scale range to speed up the solver based on the ScaleUnits enum
         * @param fov_low The low end of the search range in the specified units
         * @param fov_high The high end of the search range in the specified units
         * @param units The specified units for the range.
         */
        void setSearchScale(double fov_low, double fov_high, ScaleUnits units);

        /**
         * @brief setSearchPositionRaDec sets the search RA/DEC/Radius to speed up the solver when plate solving
         * @param ra The Right Ascension in decimal hours
         * @param dec The Declination in decimal degrees
         */
        void setSearchPositionRaDec(double ra, double dec);

        /**
         * @brief setSearchPositionInDegrees sets the search RA/DEC/Radius to speed up the solver when plate solving
         * @param ra The Right Ascension in decimal degrees
         * @param dec The Declination in decimal degrees
         */
        void setSearchPositionInDegrees(double ra, double dec);

        /**
         * @brief clearSearchPosition turns off the usage of the Search Position if it was set previously
         */
        void clearSearchPosition()
        {
            m_UsePosition = false;
        }

        /**
         * @brief clearSearchScale turns off the usage of the Search Scale if it was set previously
         */
        void clearSearchScale()
        {
            m_UseScale = false;
        }

        /**
         * @brief setLogLevel sets the astrometry logging level
         * @param level The level of logging
         */
        void setLogLevel(logging_level level)
        {
            m_AstrometryLogLevel = level;
        };

        /**
         * @brief setSSLogLevel sets the logging level for StellarSolver
         * @param level The level of logging
         */
        void setSSLogLevel(SSolverLogLevel level)
        {
            m_SSLogLevel = level;
        };

        /**
         * @brief setConvolutionFilter lets you set up your own custom QVector convolution filter
         * @param filter the QVector of float values that is the convolution filter
         */
        void setConvolutionFilter(const QVector<float> &filter)
        {
            params.convFilterType = SSolver::CONV_CUSTOM;
            convFilter = filter;
        }

        /**
         * @brief generateConvFilter can be used to generate a Convolution Filter based on the selected parameters
         * @param filter The selected Convolution Filter type
         * @param fwhm The fwhm of the filter in pixels
         * @return The QVector of float values that is the Convolution Filter
         */
        static QVector<float> generateConvFilter(SSolver::ConvFilterType filter, double fwhm);

        /**
         * @brief getBuiltInProfiles gets the built in Parameter options profiles
         * @return The QList of Parameter Options Profiles
         */
        static QList<Parameters> getBuiltInProfiles();

        /**
         * @brief loadSavedOptionsProfiles loads saved Parameter Options Profiles from a file on the computer
         * @param savedOptionsProfiles The file path where the profiles are saved
         * @return The QList full of Parameter Options Profiles
         */
        static QList<SSolver::Parameters> loadSavedOptionsProfiles(const QString &savedOptionsProfiles);

        /**
         * @brief getDefaultIndexFolderPaths gets the default astrometry Index folder paths for the Operating System
         * @return A QStringList full of folder paths to search for index files
         */
        static QStringList getDefaultIndexFolderPaths();


        //Accessor Method for external classes
        /**
         * @brief getNumStarsFound gets the number of stars found in the star extraction
         * @return The number of stars found
         */
        int getNumStarsFound() const
        {
            return numStars;
        }
        /**
         * @brief getStarList gets the list of stars found during star extraction
         * @return A QList full of stars and their properties
         */
        const QList<FITSImage::Star> &getStarList() const
        {
            return m_ExtractorStars;
        }

        /**
         * @brief getStarListFromSolve gets the list of stars used to plate solve the image
         * @return A QList full of stars and their properties
         */
        const QList<FITSImage::Star> &getStarListFromSolve() const
        {
            return m_SolverStars;
        }

        /**
         * @brief getBackground gets information about the image background found during star exraction
         * @return The background information
         */
        const FITSImage::Background &getBackground() const
        {
            return background;
        }

        /**
         * @brief getSolution gets the Solution information from the latest plate solve
         * @return The Solution information
         */
        const FITSImage::Solution &getSolution() const
        {
            return solution;
        }

        /**
         * @brief getSolutionIndexNumber gets the astrometry index file number used to solve the latest plate solve
         * @return The index number
         */
        short getSolutionIndexNumber()
        {
            return solutionIndexNumber;
        };

        /**
         * @brief getSolutionHealpix gets the healpix identifying which image in the index series solved the image in the latest plate solve
         * @return The healpix number
         */
        short getSolutionHealpix()
        {
            return solutionHealpix;
        };

        /**
         * @brief extractionDone Whether or not star extraction has been completed
         * @return true means the star extraction is done
         */
        bool extractionDone() const
        {
            return m_HasExtracted;
        }
        /**
         * @brief solvingDone Whether or not plate solving has been completed
         * @return true means the plate solving is done
         */
        bool solvingDone() const
        {
            return m_HasSolved;
        }

        /**
         * @brief failed Whether or not the Star Extraction or Plate Solving has failed
         * @return true means that it failed
         */
        bool failed() const
        {
            return m_HasFailed;
        }

        /**
         * @brief hasWCSData gets whether or not WCS Data has been retrieved for the image after plate solving
         * @return true means we have WCS data
         */
        bool hasWCSData() const
        {
            return hasWCS;
        };

        /**
         * @brief getWCSData returns a copy of the WCS Data Object that contains either internal or external WCS data and can perform WCS calculations on Stars and Images
         * @return the WCSData Object
         */
        WCSData getWCSData()
        {
            return wcsData;
        }

        /**
         * @brief getNumThreads gets the number of ExtractorSolvers used to plate solve the image
         * @return the number of solvers
         */
        int getNumThreads() const
        {
            if(parallelSolvers.size() == 0) return 1;
            else return parallelSolvers.size();
        }

        /**
         * @brief getCurrentParameters gets a Parameters object containing the current Options Profile
         * @return The current parameters
         */
        const Parameters &getCurrentParameters()
        {
            return params;
        }

        /**
         * @brief isCalculatingHFR returns whether or not the star extraction is also doing HFR
         * @return true means HFR is being done too
         */
        bool isCalculatingHFR()
        {
            return m_CalculateHFR;
        }

        /**
         * @brief setUseSubframe sets up a subframe for star extraction
         * @param frame The QRect that represents the subframe in the image
         */
        void setUseSubframe(QRect frame);

        /**
         * @brief clearSubFrame removes the subframe so that star extraction does the whole image.
         */
        void clearSubFrame()
        {
            useSubframe = false;
            m_Subframe = QRect(0, 0, m_Statistics.width, m_Statistics.height);
        };

        /**
         * @brief setColorChannel allows you to choose which color channel to use for Star Extraction and Solving in an RGB Image
         * @param channel The ColorChannel to use
         */
        void setColorChannel(FITSImage::ColorChannel channel)
        {
            m_ColorChannel = channel;
        };

        /**
         * @brief setColorChannel allows you to choose which color channel to use for Star Extraction and Solving in an RGB Image
         * @param channel The int of the ColorChannel to use
         */
        void setColorChannel(int channel)
        {
            m_ColorChannel = (FITSImage::ColorChannel) channel;
        };

        /**
         * @brief isRunning returns whether or not a process is currently running
         * @return true means it is running
         */
        bool isRunning() const;

        /**
         * @brief raString will generate a nicely formatted QString for the Right Ascension
         * @param ra The ra in decimal hours
         * @return A QString for the Right Ascension
         */
        static QString raString(double ra);

        /**
         * @brief decString will generate a nicely formatted QString for the Declination
         * @param dec The dec in decimal degrees
         * @return A QString for the Declination
         */
        static QString decString(double dec);

        /**
         * @brief pixelToWCS converts the image X, Y Pixel coordinates to RA, DEC sky coordinates using the WCS data
         * @param pixelPoint The X, Y coordinate in pixels
         * @param skyPoint The RA, DEC coordinates
         * @return A boolean to say whether it succeeded, true means it did
         */
        bool pixelToWCS(const QPointF &pixelPoint, FITSImage::wcs_point &skyPoint);

        /**
         * @brief wcsToPixel converts the RA, DEC sky coordinates to image X, Y Pixel coordinates using the WCS data
         * @param skyPoint The RA, DEC coordinates
         * @param pixelPoint The X, Y coordinate in pixels
         * @return A boolean to say whether it succeeded, true means it did
         */
        bool wcsToPixel(const FITSImage::wcs_point &skyPoint, QPointF &pixelPoint);


    public slots:
        /**
         * @brief processFinished slot gets called when a Star Extraction or a Plate Solve finishes.
         * @param code Whether the process has failed or not.  0 means success.
         */
        void processFinished(int code);

        /**
         * @brief finishParallelSolve slot gets called when a Parallel Plate Solve finishes.
         * @param code Whether the process has failed or not.  0 means success.
         */
        void finishParallelSolve(int success);

    private:

   // Useful state information for the StellarSolver
        bool m_HasExtracted {false};        // This boolean is set when the star extraction is done
        bool m_HasSolved {false};           // This boolean is set when the solving is done
        bool m_HasFailed {false};           // This boolean is set when a process has failed
        bool hasWCS {false};                // This boolean gets set if the StellarSolver has WCS data to retrieve
        bool m_isRunning {false};           // Whether or not the StellarSolver is currently running

   //StellarSolver Options

        // Primary Options for StellarSolver
        ProcessType m_ProcessType { EXTRACT };                  // This defines the type of process to perform.
        ExtractorType m_ExtractorType { EXTRACTOR_INTERNAL };   // This is the type of star extractor to use.
        SolverType m_SolverType {SOLVER_STELLARSOLVER};         // This is the type of solver to use.

        // External Process Options
        QString m_FileToProcess;                // The file that is being processed.
        bool m_CleanupTemporaryFiles {true};    // Whether or not to delete the temp files when finished
        bool m_AutoGenerateAstroConfig {true};  // Whether or not to generate the astrometry.cfg file. This is preferred so that it sends all the options requested.
        bool m_OnlySendFITSFiles {true};        // This is sometimes needed if the external solvers can't handle other file types
        ExternalProgramPaths m_ExternalPaths;   // System File Paths to external programs and files

        // Index File Options
        QStringList indexFolderPaths;           // This is the list of folder paths that the solver will use to search for index files
        QStringList m_IndexFilePaths;           // This is an alternative to the indexFolderPaths variable.  We can just load individual index files instead of searching for them

        // Online Options
        QString m_AstrometryAPIKey;
        QString m_AstrometryAPIURL;

        // HFR Options
        bool m_CalculateHFR {false};          // Whether or not the HFR of the image should be calculated using sep_flux_radius.  Don't do it unless you need HFR

        // Subframing Options
        bool useSubframe {false};
        QRect m_Subframe;

        // This determines which color channel in an RGB image should be used for SEP
        // By Default we should use green since most telescopes are best color corrected for Green
        int m_ColorChannel = FITSImage::GREEN;

        // The currently set parameters for StellarSolver
        Parameters params;

        // This is the Convolution Filter used by the Source Extractor
        QVector<float> convFilter = {1, 2, 1,
                                     2, 4, 2,
                                     1, 2, 1
                                    };

        // Astrometry Scale Parameters, These are not saved parameters and change for each image, use the methods to set them
        bool m_UseScale {false};                // Whether or not to use the image scale parameters
        double m_ScaleLow {0};                  // Lower bound of image scale estimate
        double m_ScaleHigh {0};                 // Upper bound of image scale estimate
        ScaleUnits m_ScaleUnit {ARCMIN_WIDTH};  // In what units are the lower and upper bounds?

        // Astrometry Position Parameters, These are not saved parameters and change for each image, use the methods to set them
        bool m_UsePosition = false;             // Whether or not to use initial information about the position
        double m_SearchRA = HUGE_VAL;           // RA of field center for search, format: decimal degrees
        double m_SearchDE = HUGE_VAL;           // DEC of field center for search, format: decimal degrees

    // StellarSolver Variables

        FITSImage::Statistic m_Statistics;                  // This is information about the image
        const uint8_t *m_ImageBuffer { nullptr };           // The generic data buffer containing the image data
        QList<ExtractorSolver*> parallelSolvers;            // This is the list of parallel ExtractorSolvers when solving in parallel
        QScopedPointer<ExtractorSolver> m_ExtractorSolver;  // This is the single ExtractorSolver used when not working in parallel
        WCSData wcsData;                    // This is the WCS information from the last solve.
        int m_ParallelSolversFinishedCount {0};             // This is the number of parallel solvers that are done.

    // StellarSolver Results Information

        FITSImage::Background background;           // This is a report on the background levels found during star extraction
        QList<FITSImage::Star> m_ExtractorStars;    // This is the list of stars that get extracted from the image
        QList<FITSImage::Star> m_SolverStars;       // This is the list of stars that were extracted for the last successful solve
        int numStars = 0;                           // The number of stars found in the last operation
        FITSImage::Solution solution;               // This is the solution that comes back from the Solver
        short solutionIndexNumber = -1;             // This is the index number of the index used to solve the image.
        short solutionHealpix = -1;                 // This is the healpix of the index used to solve the image.

    // Logging Settings for Astrometry
        bool m_LogToFile {false};                       //This determines whether or not to save the output from Astrometry.net to a file
        QString m_LogFileName;                          //This is the path to the log file that it will save.
        logging_level m_AstrometryLogLevel {LOG_NONE};  //This is the level of astrometry logging.  Beware, setting this too high can severely affect performance
        SSolverLogLevel m_SSLogLevel {LOG_NORMAL};      //This is the level for the StellarSolver Logging

    // These are for creating temporary files
        //This is the base name used for all temporary files.  It uses a random name based on the type of solver/star extractor.
        QString m_BaseName;
        //This is the path used for saving any temporary files.  They are by default saved to the default temp directory, you can change it if you want to.
        QString m_BasePath {QDir::tempPath()};

    // StellarSolver Private Methods
        /**
         * @brief registerMetaTypes registers the meta types so they can be used in functions
         */
        void registerMetaTypes();

        /**
         * @brief parallelSolversAreRunning returns whether the parallel solvers are currently running
         * @return true if they are running
         */
        bool parallelSolversAreRunning() const;

        /**
         * @brief whichSolver gets the index of this particular solver in the Parallel Solvers list
         * @param solver is which solver to check the index of
         * @return an int representing its position
         */
        int whichSolver(ExtractorSolver *solver);

        /**
         * @brief snr gets the signal to noise ratio for a star with the specified background
         * @param background The specified background object which may have come from star extraction
         * @param star The specified star
         * @param gain The gain used in the calculation
         * @return The snr as a double
         */
        static double snr(const FITSImage::Background &background,
                          const FITSImage::Star &star, double gain = 0.5);

        /**
         * @brief parallelSolve method gets called to start up a parallel solve.
         */
        void parallelSolve();

        /**
         * @brief updateConvolutionFilter This will update the convolution filter when the StellarSolver gets set up
         */
        void updateConvolutionFilter();

        /**
         * @brief appendStarsRAandDEC attaches the RA and DEC information to a star list
         * @param stars is the star list to process
         * @return true if it was successful
         */
        bool appendStarsRAandDEC(QList<FITSImage::Star> &stars);

        /**
         * @brief checkParameters checks the current Parameters before starting an operation to make sure they are sound.
         * @return true if the parameters are good to continue.
         */
        bool checkParameters();

        /**
         * @brief createExtractorSolver is an internal StellarSolver method that creates the ExtractorSolvers that will be used in the operation.
         * @return The newly created ExtractorSolver.
         */
        ExtractorSolver* createExtractorSolver();

        /**
         * @brief getAvailableRAM finds out the amount of available RAM on the system
         * @param availableRAM is the variable that will be set to the available RAM found
         * @param totalRAM is the variable that will be set to the total RAM on the system
         * @return true if it is successful
         */
        bool getAvailableRAM(double &availableRAM, double &totalRAM);

        /**
         * @brief enoughRAMisAvailableFor determines if there is enough RAM for the selected index files so that we don't try to load indexes inParallel unless it can handle it.
         * @param indexFolders is the list of index folders we will be searching for index files
         * @return true if it is successful
         */
        bool enoughRAMisAvailableFor(const QStringList &indexFolders);

    signals:
        /**
         * @brief logOutput signals that there is infomation that should be printed to a log file or log window
         * @param logText is the QString that should be logged
         */
        void logOutput(QString logText);

        // Ready signal note: StellarSolver might not be shut down yet, especially if doing a parallel solve.
        // You can certainly use the results, but it is not recommended to delete a StellarSolver until the parallel threads are all finished.
        /**
         * @brief ready Extraction and/or solving complete, whether successful or not.  Warning, do not delete StellarSolver yet!
         */
        void ready();

        // Finished Signal note: It should be safe to delete StellarSolver at this time since no parallel threads are running.
        /**
         * @brief finished Extraction and/or solving complete, whether successful or not, and StellarSolver has shut down.
         */
        void finished();

};

