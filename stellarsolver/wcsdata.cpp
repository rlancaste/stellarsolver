#include "wcsdata.h"
#include <wcshdr.h>
#include <wcsfix.h>

//Astrometry.net includes
extern "C" {
#include "astrometry/starutil.h"
}

WCSData::WCSData(sip_t internal_wcs)
{
    internalWCS = true;
    wcs = internal_wcs;
}

WCSData::WCSData(int nwcs, wcsprm *wcs)
{
    internalWCS = false;
    m_nwcs = nwcs;
    m_wcs = wcs;
}

bool WCSData::pixelToWCS(const QPointF &pixelPoint, FITSImage::wcs_point &skyPoint)
{
    if(internalWCS)
    {
        int d = 1;//m_ActiveParameters.downsample;
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

        int status = wcsp2s(m_wcs, 1, 2, &pixcrd[0], &imgcrd[0], &phi, &theta, &world[0], &stat[0]);
        if(status != 0)
        {
            emit logOutput(QString("wcsp2s error %1: %2.").arg(status).arg(wcs_errmsg[status]));
            return false;
        }
        else
        {
            skyPoint.ra = world[0];
            skyPoint.dec = world[1];
        }
        return true;
    }
}


bool WCSData::wcsToPixel(const FITSImage::wcs_point &skyPoint, QPointF &pixelPoint)
{
    if(internalWCS)
    {
        double x;
        double y;
        anbool error = sip_radec2pixelxy(&wcs, skyPoint.ra, skyPoint.dec, &x, &y);
        if(error != 0)
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

        int status = wcss2p(m_wcs, 1, 2, &worldcrd[0], &phi[0], &theta[0], &imgcrd[0], &pixcrd[0], &stat[0]);
        if(status != 0)
        {
            emit logOutput(QString("wcss2p error %1: %2.").arg(status).arg(wcs_errmsg[status]));
            return false;
        }
        pixelPoint.setX(pixcrd[0]);
        pixelPoint.setY(pixcrd[1]);
        return true;
    }
}

bool WCSData::appendStarsRAandDEC(QList<FITSImage::Star> &stars)
{
    if(internalWCS)
    {
        int d = 1; // m_ActiveParameters.downsample;
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
            int status = 0;
            double ra = HUGE_VAL;
            double dec = HUGE_VAL;
            pixcrd[0] = oneStar.x;
            pixcrd[1] = oneStar.y;

            if ((status = wcsp2s(m_wcs, 1, 2, &pixcrd[0], &imgcrd[0], &phi, &theta, &world[0], &stat[0])) != 0)
            {
                emit logOutput(QString("wcsp2s error %1: %2.").arg(status).arg(wcs_errmsg[status]));
                return false;
            }
            else
            {
                ra  = world[0];
                dec = world[1];
            }

            oneStar.ra = ra;
            oneStar.dec = dec;
        }

        return true;
    }
}

