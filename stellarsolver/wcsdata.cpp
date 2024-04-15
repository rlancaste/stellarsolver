//Project Includes
#include "wcsdata.h"

//WCSLib Includes
#include <wcshdr.h>
#include <wcsfix.h>

//Astrometry.net includes
extern "C" {
#include "astrometry/starutil.h"
}

WCSData::WCSData()
{
    hasWCS = false;
}

WCSData::WCSData(sip_t internal_wcs, int downsample)
{
    internalWCS = true;
    hasWCS = true;
    wcs = internal_wcs;
    d = downsample;
}

WCSData::WCSData(int nwcs, wcsprm *wcs, int downsample)
{
    internalWCS = false;
    hasWCS = true;
    m_nwcs = nwcs;
    m_wcs = wcs;
    d = downsample;
}

bool WCSData::pixelToWCS(const QPointF &pixelPoint, FITSImage::wcs_point &skyPoint)
{
    if(!hasWCS)
        return false;
    if(internalWCS)
    {
        double ra;
        double dec;
        sip_pixelxy2radec(&wcs, pixelPoint.x() / d, pixelPoint.y() / d, &ra, &dec);
        skyPoint.ra = ra;
        skyPoint.dec = dec;
        return true;
    }

    else{
        double imgcrd[2], phi, pixcrd[2], theta, world[2];
        int stat[2];
        pixcrd[0] = pixelPoint.x();
        pixcrd[1] = pixelPoint.y();
        if(wcsp2s(m_wcs, 1, 2, &pixcrd[0], &imgcrd[0], &phi, &theta, &world[0], &stat[0]) != 0)
            return false;
        skyPoint.ra = world[0];
        skyPoint.dec = world[1];
        return true;
    }
}


bool WCSData::wcsToPixel(const FITSImage::wcs_point &skyPoint, QPointF &pixelPoint)
{
    if(!hasWCS)
        return false;
    if(internalWCS)
    {
        double x;
        double y;
        if(sip_radec2pixelxy(&wcs, skyPoint.ra, skyPoint.dec, &x, &y) != TRUE)
            return false;
        pixelPoint.setX(x);
        pixelPoint.setY(y);
        return true;
    }
    else
    {
        double imgcrd[2], worldcrd[2], pixcrd[2], phi[2], theta[2];
        int stat[2];
        worldcrd[0] = skyPoint.ra;
        worldcrd[1] = skyPoint.dec;
        if(wcss2p(m_wcs, 1, 2, &worldcrd[0], &phi[0], &theta[0], &imgcrd[0], &pixcrd[0], &stat[0]) != 0)
            return false;
        pixelPoint.setX(pixcrd[0]);
        pixelPoint.setY(pixcrd[1]);
        return true;
    }
}

bool WCSData::appendStarsRAandDEC(QList<FITSImage::Star> &stars)
{
    if(!hasWCS)
        return false;
    if(internalWCS)
    {
        for(auto &oneStar : stars)
        {
            double ra = HUGE_VAL;
            double dec = HUGE_VAL;
            sip_pixelxy2radec(&wcs, oneStar.x / d, oneStar.y / d, &ra, &dec);
            char rastr[32], decstr[32];
            ra2hmsstring(ra, rastr);
            dec2dmsstring(dec, decstr);
            oneStar.ra = ra;
            oneStar.dec = dec;
        }
        return true;
    }
    else
    {
        double imgcrd[2], phi = 0, pixcrd[2], theta = 0, world[2];
        int stat[2];

        for(auto &oneStar : stars)
        {
            pixcrd[0] = oneStar.x;
            pixcrd[1] = oneStar.y;
            if ((wcsp2s(m_wcs, 1, 2, &pixcrd[0], &imgcrd[0], &phi, &theta, &world[0], &stat[0])) != 0)
                return false;
            oneStar.ra = world[0];
            oneStar.dec = world[1];
        }

        return true;
    }
}

