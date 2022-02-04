/*  SextractorSolver, StellarSolver Intenal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#pragma once

//Includes for this project
#include <QThread>
#include <QRect>
#include <QDir>
#include "structuredefinitions.h"
#include "parameters.h"

//CFitsio Includes
#include <fitsio.h>

//Astrometry.net includes
extern "C" {
#include "astrometry/blindutils.h"
#include "astrometry/engine.h"
#include "astrometry/sip-utils.h"
}

using namespace SSolver;

class SextractorSolver : public QThread
{
        Q_OBJECT
    public:
        SextractorSolver(ProcessType type, ExtractorType sexType, SolverType solType, const FITSImage::Statistic &statistics,
                         const uint8_t *imageBuffer, QObject *parent = nullptr);
        ~SextractorSolver();

        ProcessType m_ProcessType;
        ExtractorType m_ExtractorType;
        SolverType m_SolverType;

        virtual int extract() = 0;
        //virtual void solve()=0;
        //These are the most important methods that you can use for the StellarSolver
        //This starts the process in a separate thread
        virtual void execute();
        virtual void abort() = 0;
        virtual SextractorSolver* spawnChildSolver(int n) = 0;
        virtual void cleanupTempFiles() = 0;
        //This will abort the solver

        virtual bool appendStarsRAandDEC(QList<FITSImage::Star> &stars) = 0;

        //Logging Settings for Astrometry
        bool m_LogToFile = false;             //This determines whether or not to save the output from Astrometry.net to a file
        QString m_LogFileName;                //This is the path to the log file that it will save.
        logging_level m_AstrometryLogLevel = LOG_NONE;       //This is the level of logging getting saved to the log file
        SSolverLogLevel m_SSLogLevel {LOG_NORMAL};   //This is the level for the StellarSolver Logging
        //These are for creating temporary files
        QString m_BaseName;                   //This is the base name used for all temporary files.  If it is not set, it will use a number based on the order of solvers created.
        QString m_BasePath;                   //This is the path used for saving any temporary files.  They are by default saved to the default temp directory, you can change it if you want to.

        Parameters m_ActiveParameters;                  //The currently set parameters for StellarSolver
        QStringList indexFolderPaths;       //This is the list of folder paths that the solver will use to search for index files

        //Astrometry Scale Parameters, These are not saved parameters and change for each image, use the methods to set them
        bool m_UseScale = false;             //Whether or not to use the image scale parameters
        double scalelo = 0;                 //Lower bound of image scale estimate
        double scalehi = 0;                 //Upper bound of image scale estimate
        ScaleUnits scaleunit;               //In what units are the lower and upper bounds?

        //Astrometry Position Parameters, These are not saved parameters and change for each image, use the methods to set them
        bool m_UsePosition = false;          //Whether or not to use initial information about the position
        double search_ra = HUGE_VAL;        //RA of field center for search, format: decimal degrees
        double search_dec = HUGE_VAL;       //DEC of field center for search, format: decimal degrees

        inline QString getScaleUnitString()
        {
            switch(scaleunit)
            {
                case DEG_WIDTH:
                    return "degwidth";
                    break;
                case ARCMIN_WIDTH:
                    return "arcminwidth";
                    break;
                case ARCSEC_PER_PIX:
                    return "arcsecperpix";
                    break;
                case FOCAL_MM:
                    return "focalmm";
                    break;
                default:
                    return "";
                    break;
            }
        }

        void setSearchScale(double fov_low, double fov_high,
                            ScaleUnits
                            units); //This sets the scale range for the image to speed up the solver                                                    //This sets the search RA/DEC/Radius to speed up the solver
        void setSearchPositionInDegrees(double ra, double dec);
        int depthlo = -1;                       //This is the low depth of this child solver
        int depthhi = -1;                       //This is the high depth of this child solver


        const FITSImage::Background &getBackground() const
        {
            return m_Background;
        }
        int getNumStarsFound() const
        {
            return m_ExtractedStars.size();
        };
        const QList<FITSImage::Star> &getStarList() const
        {
            return m_ExtractedStars;
        }
        void setStarList(QList<FITSImage::Star> starList)
        {
            m_ExtractedStars = starList;
        }
        FITSImage::Solution getSolution()
        {
            return m_Solution;
        };
        bool hasWCSData()
        {
            return m_HasWCS;
        };
        bool solvingDone()
        {
            return m_HasSolved;
        };
        bool sextractionDone()
        {
            return m_HasExtracted;
        };
        bool isCalculatingHFR()
        {
            return m_ProcessType == EXTRACT_WITH_HFR;
        };
        void setUseSubframe(QRect frame)
        {
            m_UseSubframe = true;
            m_SubFrameRect = frame;
        };

        virtual bool pixelToWCS(const QPointF &pixelPoint, FITSImage::wcs_point &skyPoint) = 0;
        virtual bool wcsToPixel(const FITSImage::wcs_point &skyPoint, QPointF &pixelPoint) = 0;


    protected:  //Note: These items are not private because they are needed by ExternalSextractorSolver

        bool m_UseSubframe = false;
        QRect m_SubFrameRect;
        //StellarSolver Internal settings that are needed by ExternalSextractorSolver as well
        //This boolean is set when the sextraction is done
        bool m_HasExtracted = false;
        //This boolean is set when the solving is done
        bool m_HasSolved = false;
        //This is information about the image
        FITSImage::Statistic m_Statistics;
        //The generic data buffer containing the image data
        const uint8_t *m_ImageBuffer { nullptr };
        //This boolean gets set internally if we are using a downsampled image buffer for SEP
        bool usingDownsampledImage = false;

        //The Results
        //This is a report on the background levels found during sextraction
        FITSImage::Background m_Background;
        //This is the list of stars that get sextracted from the image, saved to the file, and then solved by astrometry.net
        QList<FITSImage::Star> m_ExtractedStars;
        FITSImage::Solution m_Solution;          //This is the solution that comes back from the Solver

        bool runSEPSextractor();    //This is the method that actually runs the internal sextractor
        bool m_HasWCS = false;        //This boolean gets set if the StellarSolver has WCS data to retrieve
        bool m_WasAborted = false;
        // This is the cancel file path that astrometry.net monitors.  If it detects this file, it aborts the solve
        QString cancelfn;           //Filename whose creation signals the process to stop
        QString solvedfn;           //Filename whose creation tells astrometry.net it already solved the field.

        bool isChildSolver = false;              //This identifies that this solver is in fact a child solver.

        inline double convertToDegreeHeight(double scale)
        {
            switch(scaleunit)
            {
                case DEG_WIDTH:
                    return scale;
                    break;
                case ARCMIN_WIDTH:
                    return arcmin2deg(scale);
                    break;
                case ARCSEC_PER_PIX:
                    return arcsec2deg(scale) * (double)m_Statistics.height;
                    break;
                case FOCAL_MM:
                    return rad2deg(atan(36. / (2. * scale)));
                    break;
                default:
                    return scale;
                    break;
            }
        }

    signals:

        //This signals that there is infomation that should be printed to a log file or log window
        void logOutput(QString logText);

        //This signals that the sextraction or image solving is complete, whether they were successful or not
        //A -1 or some positive value should signify failure, where a 0 should signify success.
        void finished(int exit_code);

};

