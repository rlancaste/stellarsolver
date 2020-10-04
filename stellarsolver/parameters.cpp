#include "parameters.h"
#include <QVariant>

bool SSolver::Parameters::operator==(const Parameters& o)
{
    //We will skip the list name, since we want to see if the contents are the same.
    return  apertureShape == o.apertureShape &&
            kron_fact == o.kron_fact &&
            subpix == o.subpix &&
            r_min == o.r_min &&
            //skip inflags??  Not sure we even need them

            magzero == o.magzero &&
            minarea == o.minarea &&
            deblend_thresh == o.deblend_thresh &&
            deblend_contrast == o.deblend_contrast &&
            clean == o.clean &&
            clean_param == o.clean_param &&
            fwhm == o.fwhm &&
            //skip conv filter?? This might be hard to compare

            maxSize == o.maxSize &&
            minSize == o.minSize &&
            maxEllipse == o.maxEllipse &&
            keepNum == o.keepNum &&
            removeBrightest == o.removeBrightest &&
            removeDimmest == o.removeDimmest &&
            saturationLimit == o.saturationLimit &&

            multiAlgorithm == o.multiAlgorithm &&
            inParallel == o.inParallel &&
            solverTimeLimit == o.solverTimeLimit &&
            minwidth == o.minwidth &&
            maxwidth == o.maxwidth &&

            resort == o.resort &&
            downsample == o.downsample &&
            search_parity == o.search_parity &&
            search_radius == o.search_radius &&
            //They need to be turned into a qstring because they are sometimes very close but not exactly the same
            QString::number(logratio_tosolve) == QString::number(o.logratio_tosolve) &&
            QString::number(logratio_tokeep) == QString::number(o.logratio_tokeep) &&
            QString::number(logratio_totune) == QString::number(o.logratio_totune);
}

QMap<QString, QVariant> SSolver::Parameters::convertToMap(Parameters params)
{
    QMap<QString, QVariant> settingsMap;
    settingsMap.insert("listName", QVariant(params.listName));

    //These are to pass the parameters to the internal sextractor
    settingsMap.insert("apertureShape", QVariant(params.apertureShape));
    settingsMap.insert("kron_fact", QVariant(params.kron_fact));
    settingsMap.insert("subpix", QVariant(params.subpix));
    settingsMap.insert("r_min", QVariant(params.r_min));
    //params.inflags
    settingsMap.insert("magzero", QVariant(params.magzero));
    settingsMap.insert("minarea", QVariant(params.minarea));
    settingsMap.insert("deblend_thresh", QVariant(params.deblend_thresh));
    settingsMap.insert("deblend_contrast", QVariant(params.deblend_contrast));
    settingsMap.insert("clean", QVariant(params.clean));
    settingsMap.insert("clean_param", QVariant(params.clean_param));

    settingsMap.insert("fwhm", QVariant(params.fwhm));
    QStringList conv;
    foreach(float num, params.convFilter)
    {
        conv << QVariant(num).toString();
    }
    settingsMap.insert("convFilter", QVariant(conv.join(",")));

    //Star Filter Settings
    settingsMap.insert("maxSize", QVariant(params.maxSize));
    settingsMap.insert("minSize", QVariant(params.minSize));
    settingsMap.insert("maxEllipse", QVariant(params.maxEllipse));
    settingsMap.insert("keepNum", QVariant(params.keepNum));
    settingsMap.insert("removeBrightest", QVariant(params.removeBrightest));
    settingsMap.insert("removeDimmest", QVariant(params.removeDimmest ));
    settingsMap.insert("saturationLimit", QVariant(params.saturationLimit));

    //Settings that usually get set by the Astrometry config file
    settingsMap.insert("maxwidth", QVariant(params.maxwidth)) ;
    settingsMap.insert("minwidth", QVariant(params.minwidth)) ;
    settingsMap.insert("inParallel", QVariant(params.inParallel)) ;
    settingsMap.insert("multiAlgo", QVariant(params.multiAlgorithm)) ;
    settingsMap.insert("solverTimeLimit", QVariant(params.solverTimeLimit));

    //Astrometry Basic Parameters
    settingsMap.insert("resort", QVariant(params.resort)) ;
    settingsMap.insert("downsample", QVariant(params.downsample)) ;
    settingsMap.insert("search_radius", QVariant(params.search_radius)) ;

    //Setting the settings to know when to stop or keep searching for solutions
    settingsMap.insert("logratio_tokeep", QVariant(params.logratio_tokeep)) ;
    settingsMap.insert("logratio_totune", QVariant(params.logratio_totune)) ;
    settingsMap.insert("logratio_tosolve", QVariant(params.logratio_tosolve)) ;

    return settingsMap;

}

SSolver::Parameters SSolver::Parameters::convertFromMap(QMap<QString, QVariant> settingsMap)
{
    Parameters params;
    params.listName = settingsMap.value("listName", params.listName).toString();

    //These are to pass the parameters to the internal sextractor

    params.apertureShape = (Shape)settingsMap.value("apertureShape", params.listName).toInt();
    params.kron_fact = settingsMap.value("kron_fact", params.listName).toDouble();
    params.subpix = settingsMap.value("subpix", params.listName).toInt();
    params.r_min= settingsMap.value("r_min", params.listName).toDouble();
    //params.inflags
    params.magzero = settingsMap.value("magzero", params.magzero).toDouble();
    params.minarea = settingsMap.value("minarea", params.minarea).toDouble();
    params.deblend_thresh = settingsMap.value("deblend_thresh", params.deblend_thresh).toInt();
    params.deblend_contrast = settingsMap.value("deblend_contrast", params.deblend_contrast).toDouble();
    params.clean = settingsMap.value("clean", params.clean).toInt();
    params.clean_param = settingsMap.value("clean_param", params.clean_param).toDouble();

    //The Conv Filter
    params.fwhm = settingsMap.value("fwhm",params.fwhm).toDouble();
    if(settingsMap.contains("convFilter"))
    {
        QStringList conv = settingsMap.value("convFilter", "").toString().split(",");
        QVector<float> filter;
        foreach(QString item, conv)
            filter.append(QVariant(item).toFloat());
        params.convFilter = filter;
    }

    //Star Filter Settings
    params.maxSize = settingsMap.value("maxSize", params.maxSize).toDouble();
    params.minSize = settingsMap.value("minSize", params.minSize).toDouble();
    params.maxEllipse = settingsMap.value("maxEllipse", params.maxEllipse).toDouble();
    params.keepNum = settingsMap.value("keepNum", params.keepNum).toDouble();
    params.removeBrightest = settingsMap.value("removeBrightest", params.removeBrightest).toDouble();
    params.removeDimmest = settingsMap.value("removeDimmest", params.removeDimmest ).toDouble();
    params.saturationLimit = settingsMap.value("saturationLimit", params.saturationLimit).toDouble();

    //Settings that usually get set by the Astrometry config file
    params.maxwidth = settingsMap.value("maxwidth", params.maxwidth).toDouble() ;
    params.minwidth = settingsMap.value("minwidth", params.minwidth).toDouble() ;
    params.inParallel = settingsMap.value("inParallel", params.inParallel).toBool() ;
    params.multiAlgorithm = (MultiAlgo)(settingsMap.value("multiAlgo", params.multiAlgorithm)).toInt();
    params.solverTimeLimit = settingsMap.value("solverTimeLimit", params.solverTimeLimit).toInt();

    //Astrometry Basic Parameters
    params.resort = settingsMap.value("resort", params.resort).toBool();
    params.downsample = settingsMap.value("downsample", params.downsample).toBool() ;
    params.search_radius = settingsMap.value("search_radius", params.search_radius).toDouble() ;

    //Setting the settings to know when to stop or keep searching for solutions
    params.logratio_tokeep = settingsMap.value("logratio_tokeep", params.logratio_tokeep).toDouble() ;
    params.logratio_totune = settingsMap.value("logratio_totune", params.logratio_totune).toDouble() ;
    params.logratio_tosolve = settingsMap.value("logratio_tosolve", params.logratio_tosolve).toDouble();

    return params;

}






