//Qt Includes
#include <QVariant>

//Project Includes
#include "parameters.h"


bool SSolver::Parameters::operator==(const Parameters& o)
{
    //We will skip the list name, since we want to see if the contents are the same.
    return  apertureShape == o.apertureShape &&
            kron_fact == o.kron_fact &&
            subpix == o.subpix &&
            r_min == o.r_min &&
            magzero == o.magzero &&
            minarea == o.minarea &&
            deblend_thresh == o.deblend_thresh &&
            deblend_contrast == o.deblend_contrast &&
            clean == o.clean &&
            clean_param == o.clean_param &&

            //These are StellarSolver parameters used for the creation of the convolution filter
            convFilterType == o.convFilterType &&
            fwhm == o.fwhm &&

            //Option to partition star extraction in separate threads or not
            partition == o.partition &&

            threshold_offset == o.threshold_offset &&
            threshold_bg_multiple == o.threshold_bg_multiple &&

            //StellarSolver Star Filter Settings
            maxSize == o.maxSize &&
            minSize == o.minSize &&
            maxEllipse == o.maxEllipse &&
            initialKeep == o.initialKeep &&
            keepNum == o.keepNum &&
            removeBrightest == o.removeBrightest &&
            removeDimmest == o.removeDimmest &&
            saturationLimit == o.saturationLimit &&

            //The setting for parallel thread solving
            multiAlgorithm == o.multiAlgorithm &&

            //Settings from the Astrometry Config file
            inParallel == o.inParallel &&
            solverTimeLimit == o.solverTimeLimit &&
            minwidth == o.minwidth &&
            maxwidth == o.maxwidth &&

            //Basic Astrometry settings
            resort == o.resort &&
            autoDownsample == o.autoDownsample &&
            downsample == o.downsample &&
            search_parity == o.search_parity &&
            search_radius == o.search_radius &&

            //Astrometry settings that determine when to keep solutions or keep searching for better solutions
            //They need to be turned into a qstring because they are sometimes very close but not exactly the same
            QString::number(logratio_tosolve) == QString::number(o.logratio_tosolve) &&
            QString::number(logratio_tokeep) == QString::number(o.logratio_tokeep) &&
            QString::number(logratio_totune) == QString::number(o.logratio_totune);
}

QMap<QString, QVariant> SSolver::Parameters::convertToMap(const Parameters &params)
{
    QMap<QString, QVariant> settingsMap;
    settingsMap.insert("listName", QVariant(params.listName));
    settingsMap.insert("description", QVariant(params.description));

    //These are to pass the parameters to the internal star extractor
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

    //This is a StellarSolver parameter used for the creation of the convolution filter
    settingsMap.insert("convFilterType", QVariant(params.convFilterType));
    settingsMap.insert("fwhm", QVariant(params.fwhm));

    //Option to partition star extraction in separate threads or not
    settingsMap.insert("partition", QVariant(params.partition));

    settingsMap.insert("threshold_offset", QVariant(params.threshold_offset));
    settingsMap.insert("threshold_bg_multiple", QVariant(params.threshold_bg_multiple));
    
    //StellarSolver Star Filter Settings
    settingsMap.insert("maxSize", QVariant(params.maxSize));
    settingsMap.insert("minSize", QVariant(params.minSize));
    settingsMap.insert("maxEllipse", QVariant(params.maxEllipse));
    settingsMap.insert("initialKeep", QVariant(params.initialKeep));
    settingsMap.insert("keepNum", QVariant(params.keepNum));
    settingsMap.insert("removeBrightest", QVariant(params.removeBrightest));
    settingsMap.insert("removeDimmest", QVariant(params.removeDimmest ));
    settingsMap.insert("saturationLimit", QVariant(params.saturationLimit));

    //A setting specifig to StellarSovler for choosing the algorithm to use to solve with parallel threads.
    settingsMap.insert("multiAlgo", QVariant(params.multiAlgorithm)) ;

    //Settings that usually get set by the Astrometry config file
    settingsMap.insert("maxwidth", QVariant(params.maxwidth)) ;
    settingsMap.insert("minwidth", QVariant(params.minwidth)) ;
    settingsMap.insert("inParallel", QVariant(params.inParallel)) ;
    settingsMap.insert("solverTimeLimit", QVariant(params.solverTimeLimit));

    //Astrometry Basic Parameters
    settingsMap.insert("resort", QVariant(params.resort)) ;
    settingsMap.insert("autoDownsample", QVariant(params.autoDownsample)) ;
    settingsMap.insert("downsample", QVariant(params.downsample)) ;
    settingsMap.insert("search_radius", QVariant(params.search_radius)) ;

    //Astrometry settings that determine when to keep solutions or keep searching for better solutions
    settingsMap.insert("logratio_tokeep", QVariant(params.logratio_tokeep)) ;
    settingsMap.insert("logratio_totune", QVariant(params.logratio_totune)) ;
    settingsMap.insert("logratio_tosolve", QVariant(params.logratio_tosolve)) ;

    return settingsMap;

}

SSolver::Parameters SSolver::Parameters::convertFromMap(const QMap<QString, QVariant> &settingsMap)
{
    Parameters params;
    params.listName = settingsMap.value("listName", params.listName).toString();
    params.description = settingsMap.value("description", params.description).toString();

    //These are to pass the parameters to the internal star extractor

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

    params.threshold_offset = settingsMap.value("threshold_offset",params.threshold_offset).toDouble();
    params.threshold_bg_multiple = settingsMap.value("threshold_bg_multiple",params.threshold_bg_multiple).toDouble();
    
    //These are StellarSolver parameters used for the creation of the convolution filter
    params.fwhm = settingsMap.value("fwhm",params.fwhm).toDouble();
    params.convFilterType = (ConvFilterType)settingsMap.value("convFilterType",params.convFilterType).toInt();

    //Option to partition star extraction in separate threads or not
    params.partition = settingsMap.value("partition", params.partition).toBool();

    //StellarSolver Star Filter Settings
    params.maxSize = settingsMap.value("maxSize", params.maxSize).toDouble();
    params.minSize = settingsMap.value("minSize", params.minSize).toDouble();
    params.maxEllipse = settingsMap.value("maxEllipse", params.maxEllipse).toDouble();
    params.initialKeep = settingsMap.value("initialKeep", params.initialKeep).toInt();
    params.keepNum = settingsMap.value("keepNum", params.keepNum).toInt();
    params.removeBrightest = settingsMap.value("removeBrightest", params.removeBrightest).toDouble();
    params.removeDimmest = settingsMap.value("removeDimmest", params.removeDimmest ).toDouble();
    params.saturationLimit = settingsMap.value("saturationLimit", params.saturationLimit).toDouble();

    //This is a parameter specific to StellarSolver.  It determines the algorithm to use to run parallel threads for solving
    params.multiAlgorithm = (MultiAlgo)(settingsMap.value("multiAlgo", params.multiAlgorithm)).toInt();

    //Settings that usually get set by the Astrometry config file
    params.maxwidth = settingsMap.value("maxwidth", params.maxwidth).toDouble() ;
    params.minwidth = settingsMap.value("minwidth", params.minwidth).toDouble() ;
    params.inParallel = settingsMap.value("inParallel", params.inParallel).toBool() ;
    params.solverTimeLimit = settingsMap.value("solverTimeLimit", params.solverTimeLimit).toInt();

    //Astrometry Basic Parameters
    params.resort = settingsMap.value("resort", params.resort).toBool();
    params.autoDownsample = settingsMap.value("autoDownsample", params.autoDownsample).toBool();
    params.downsample = settingsMap.value("downsample", params.downsample).toInt();
    params.search_radius = settingsMap.value("search_radius", params.search_radius).toDouble() ;

    //Astrometry settings that determine when to keep solutions or keep searching for better solutions
    params.logratio_tokeep = settingsMap.value("logratio_tokeep", params.logratio_tokeep).toDouble() ;
    params.logratio_totune = settingsMap.value("logratio_totune", params.logratio_totune).toDouble() ;
    params.logratio_tosolve = settingsMap.value("logratio_tosolve", params.logratio_tosolve).toDouble();

    return params;

}






