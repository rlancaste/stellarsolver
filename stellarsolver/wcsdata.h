#ifndef WCSDATA_H
#define WCSDATA_H

#include "structuredefinitions.h"
#include <QPointF>
#include <QObject>

extern "C" {
#include "astrometry/sip.h"
}

class WCSData: public QObject
{
    Q_OBJECT
public:
    struct wcsprm *m_wcs; // This is a struct used by wcslib for wcs info loaded from a file

    WCSData(sip_t internal_wcs);
    WCSData(int nwcs, wcsprm *wcs);

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

    /**
     * @brief appendStarsRAandDEC attaches the RA and DEC information to a star list
     * @param stars is the star list to process
     * @return true if it was successful
     */
    bool appendStarsRAandDEC(QList<FITSImage::Star> &stars);

private:

    bool internalWCS = true; // This determines which type of WCS Data we have

    //  This is for the Internal Solver's WCS data
    sip_t wcs;  // This is an astrometry.net internal data structure for WCS

    int m_nwcs = 0;                  // This is a number associated with wcsinfo
   // wcsprm *m_wcs {nullptr};    // This is a struct used by wcslib for wcs info loaded from a file

signals:

    /**
     * @brief logOutput signals that there is infomation that should be printed to a log file or log window
     * @param logText is the QString that should be logged
     */
    void logOutput(QString logText);
};

#endif // WCSDATA_H
