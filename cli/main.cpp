#include <iostream>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDnsLookup>
#include <QString>
#include <QHostAddress>
#include <QDebug>
#include <optional>

#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "ssolverutils/fileio.h"
#include "version.h"
#include "ssolverutils/dms.h"

struct StellarSolverCliQuery
{
    QStringList image_files;
    std::optional<dms> ra = std::nullopt;
    std::optional<dms> dec = std::nullopt;
    std::optional<double> degrees = std::nullopt;
    FITSImage::Parity parity = FITSImage::BOTH;
    std::optional<int> limit_to_brightest_stars = std::nullopt;
    std::optional<double> scale_low = std::nullopt;
    std::optional<double> scale_high = std::nullopt;
    SSolver::ScaleUnits units = SSolver::DEG_WIDTH;
    std::optional<int> cpu_limit = std::nullopt;
    // TODO what should be the default for mac or windows?
    QString index_files_path = "/usr/share/astrometry";
    SSolver::Parameters::ParametersProfile profile = SSolver::Parameters::PARALLEL_SMALLSCALE;
};

struct CommandLineParseResult
{
    enum class Status
    {
        Ok,
        Error,
        VersionRequested,
        HelpRequested
    };
    Status statusCode = Status::Ok;
    std::optional<QString> errorString = std::nullopt;
};

/**
 * @brief extracts the profile name from the output of StellarSolver::getBuiltInProfiles()
 * 
 * @param profiles 
 * @return QList<QString> the extracted profile names
 */
QList<QString> extractProfileNames(const QList<Parameters> &profiles)
{
    QList<QString> namesList;
    for (const auto &profile : profiles)
    {
        namesList.append(profile.listName);
    }
    return namesList;
}

/**
 * @brief given a list of available profile names concat them for printing the help file
 * 
 * @param list the profile names
 * @return QString concatenated string
 */
QString concatProfileNames(const QList<QString> &list)
{
    QString result;
    for (int i = 0; i < list.size(); ++i)
    {
        // enclose each profile name in single quotes, prepend spacing for better formatting in help page
        result += "    '" + list[i] + "'\n";
    }
    return result;
}

/**
 * @brief used for parsing the astrometry.cfg file line by line
 * We are only interested in the "add_path" directive in the config file, all the other options are currently ignored
 * @param line the line of the config file to parse
 * @return std::optional<QString> the content of "add_path" or std::nullopt if not existing in this line
 */
std::optional<QString> extractAddPathDirectiveFromConfigLine(QString line)
{
    QString trimmedLine = line.trimmed(); // Remove leading and trailing whitespace
    if (!trimmedLine.startsWith('#'))     // line is not a comment
    {
        // Check if the line contains 'add_path'
        if (trimmedLine.startsWith("add_path", Qt::CaseInsensitive))
        {
            QStringList parts = trimmedLine.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() > 1)
            {
                // The string after 'add_path' is what we want to extract
                QString extractedString = parts[1]; // Assuming the desired string is right after 'add_path'
                return {extractedString};
            }
        }
    }
    return std::nullopt;
}

/**
 * @brief parses the command line args and print usage notes in case of error
 * 
 * @param parser the command line parser object
 * @param query the object containing the parsed arguments
 * @return CommandLineParseResult indicates the result of the parsing
 */
CommandLineParseResult parseCommandLine(QCommandLineParser &parser, StellarSolverCliQuery *query)
{
    using Status = CommandLineParseResult::Status;

    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.addPositionalArgument("image-file", "Input image file(s) to solve", "image-file...");
    auto builtInProfileNames = extractProfileNames(StellarSolver::getBuiltInProfiles());
    QString profileNamesConcat = concatProfileNames(builtInProfileNames);
    parser.addOptions({{{"3", "ra"},
                        "RA of field center for search, format: degrees or hh:mm:ss",
                        "RA"},
                       {{"4", "dec"},
                        "DEC of field center for search, format: degrees or hh:mm:ss.",
                        "DEC"},
                       {{"5", "degrees"},
                        "Only search in indexes within 'radius' of the field center given by --ra and --dec",
                        "DEG"},
                       {{"8", "parity"},
                        "Only check for matches with positive/negative parity (default: try both)",
                        "pos/neg"},
                       {{"d", "depth"},
                        "Number of field objects to look at, or range of numbers; 1 is the brightest star, so \"-d 10\" or \"-d 1-10\" mean look at the top ten brightest stars only.",
                        "number or range"},
                       {{"L", "scale-low"},
                        "Lower bound of image scale estimate",
                        "scale"},
                       {{"H", "scale-high"},
                        "Upper bound of image scale estimate",
                        "scale"},
                       {{"u", "scale-units"},
                        "In what units are the lower and upper bounds? Choices:\n"
                        "\"degwidth\", \"degw\", \"dw\"\n"
                        "    width of the image, in degrees (default)\n"
                        "\"arcminwidth\", \"amw\", \"aw\"\n"
                        "    width of the image, in arcminutes\n"
                        "\"arcsecperpix\", \"app\"\n"
                        "    arcseconds per pixel\n"
                        "\"focalmm\"\n"
                        "    35-mm (width-based) equivalent focal length",
                        "units"},
                       {{"l", "cpulimit"},
                        "Give up solving after the specified number of seconds of CPU time",
                        "seconds"},
                       {"config",
                        "Use this 'solve-field' compatible 'astrometry.cfg' to specify the index file location",
                        "path"},
                       {"index-files",
                        "Specifies the directory where the index files are located, has priority over --config (default: xy)",
                        "path"},
                       {"built-in-profile",
                        "One of:\n" + profileNamesConcat + "\n(default: 4-SmallScaleSolving)",
                        "profilename"}});

    const QCommandLineOption helpOption = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();

    if (!parser.parse(QCoreApplication::arguments()))
    {
        return {Status::Error, parser.errorText()};
    }

    if (parser.isSet(versionOption))
    {
        return {Status::VersionRequested};
    }

    if (parser.isSet(helpOption))
    {
        return {Status::HelpRequested};
    }

    const QStringList positionalArguments = parser.positionalArguments();
    if (positionalArguments.isEmpty())
    {
        return {Status::Error, "Argument 'image-file' missing."};
    }
    else
    {
        query->image_files = positionalArguments;
    }

    if (parser.isSet("ra"))
    {
        QString valueStr = parser.value("ra");
        bool ok = false;
        double value = valueStr.toDouble(&ok);
        if (ok) // is degrees
        {
            query->ra = {dms(value)};
        }
        else
        {
            dms dms;
            bool isHMS = dms.setFromString(valueStr, false);
            if (isHMS)
            {
                query->ra = {dms};
            }
            return {Status::Error, "Argument '-3, --ra' is not in a accepted format"};
        }
    }
    if (parser.isSet("dec"))
    {
        QString valueStr = parser.value("dec");
        bool ok = false;
        double value = valueStr.toDouble(&ok);
        if (ok)
        {
            query->dec = {dms(value)};
        }
        else
        {
            dms dms;
            bool isHMS = dms.setFromString(valueStr, false);
            if (isHMS)
            {
                query->dec = {dms};
            }
            return {Status::Error, "Argument '-4, --dec' is not a accepted format"};
        }
    }
    if (parser.isSet("degrees"))
    {
        QString valueStr = parser.value("degrees");
        bool ok = false;
        double value = valueStr.toDouble(&ok);
        if (ok)
        {
            query->degrees = {value};
        }
        else
        {
            return {Status::Error, "Argument '-5, --degrees' is not a valid floating point number"};
        }
    }

    if (parser.isSet("parity"))
    {
        QString valueStr = parser.value("parity");
        if (valueStr == "pos")
        {
            query->parity = FITSImage::POSITIVE;
        }
        else if (valueStr == "neg")
        {
            query->parity = FITSImage::NEGATIVE;
        }
        else
        {
            std::cerr << "Error: -8, --parity has to be one of [pos, neg]" << std::endl;
            return {Status::Error, "Argument: '-8, --parity' has to be one of ['pos', 'neg']"};
        }
    }
    // TODO solve-field supports also a range, this is currently not possible
    if (parser.isSet("depth"))
    {
        QString valueStr = parser.value("depth");
        bool ok = false;
        int value = valueStr.toInt(&ok);
        if (ok)
        {
            query->limit_to_brightest_stars = {value};
        }
        else
        {
            return {Status::Error, "Argument '-d, --depth' is not a integer"};
        }
    }
    if (parser.isSet("scale-low"))
    {
        QString valueStr = parser.value("scale-low");
        bool ok = false;
        double value = valueStr.toDouble(&ok);
        if (ok)
        {
            query->scale_low = {value};
        }
        else
        {
            return {Status::Error, "Argument '-L, --scale-low' is not a valid floating point number"};
        }
    }

    if (parser.isSet("scale-high"))
    {
        QString valueStr = parser.value("scale-high");
        bool ok = false;
        double value = valueStr.toDouble(&ok);
        if (ok)
        {
            query->scale_high = {value};
        }
        else
        {
            return {Status::Error, "Argument '-H, --scale-high' is not a valid floating point number"};
        }
    }

    if (parser.isSet("scale-units"))
    {
        QString valueStr = parser.value("scale-units");
        if (valueStr == "dw" || valueStr == "degw" || valueStr == "degwidth")
        {
            query->units = SSolver::DEG_WIDTH;
        }
        else if (valueStr == "app" || valueStr == "arcsecperpix")
        {
            query->units = SSolver::ARCSEC_PER_PIX;
        }
        else if (valueStr == "aw" || valueStr == "amw" || valueStr == "arcminwidth")
        {
            query->units = SSolver::ARCMIN_WIDTH;
        }
        else if (valueStr == "focalmm")
        {
            query->units = SSolver::FOCAL_MM;
        }
        else
        {
            return {Status::Error, "Argument '-u, --scale-units' has to be one of the accepted values"};
        }
    }

    if (parser.isSet("cpulimit"))
    {
        QString valueStr = parser.value("cpulimit");
        bool ok = false;
        int value = valueStr.toInt(&ok);
        if (ok)
        {
            query->cpu_limit = {value};
        }
        else
        {
            return {Status::Error, "Argument: '-l, --cpulimit' is not a valid integer"};
        }
    }
    if (parser.isSet("config"))
    {
        std::optional<QString> addPathDirective = std::nullopt;
        QFile configFile(parser.value("config"));
        if (configFile.open(QIODevice::ReadOnly))
        {
            QTextStream in(&configFile);
            while (!in.atEnd())
            {
                QString line = in.readLine();
                auto result = extractAddPathDirectiveFromConfigLine(line);
                if (result)
                {
                    addPathDirective = {result.value()};
                }
            }
            configFile.close();
            if (addPathDirective)
            {
                query->index_files_path = addPathDirective.value();
            }
            else
            {
                return {Status::Error, "Config file does not contain a 'add_path' directive "};
            }
        }
        else
        {
            return {Status::Error, "Argument: '--config' does not specify a valid path: " + configFile.errorString()};
        }
    }

    if (parser.isSet("index-files"))
    {
        query->index_files_path = parser.value("index-files");
    }

    if (parser.isSet("built-in-profile"))
    {
        int profileIndex = builtInProfileNames.indexOf(parser.value("built-in-profile"));
        // when the list in argument is actually a built in profile
        if (profileIndex >= 0)
        {
            query->profile = (SSolver::Parameters::ParametersProfile)profileIndex;
        }
    }

    return {Status::Ok};
}

/**
 * @brief starts the solving for a given image file based on the parsed parameters and print the results to console
 * 
 * @param image_file the path to the image file to solve
 * @param query the object containing the parsed command line parameters
 */
void solve(const QString &image_file, StellarSolverCliQuery *query)
{

    fileio imageLoader;
    imageLoader.logToSignal = false;
    if (!imageLoader.loadImage(image_file))
    {
        exit(1);
    }
    FITSImage::Statistic stats = imageLoader.getStats();
    uint8_t *imageBuffer = imageLoader.getImageBuffer();
    printf("Solving...\n");
    printf("Field: %s\n", image_file.toUtf8().data());
    StellarSolver stellarSolver(stats, imageBuffer);
    stellarSolver.setIndexFolderPaths(QStringList() << query->index_files_path);
    stellarSolver.setParameterProfile(query->profile);
    SSolver::Parameters params = stellarSolver.getCurrentParameters();
    if (query->degrees)
    {
        params.search_radius = query->degrees.value();
    }
    params.search_parity = query->parity;
    if (query->limit_to_brightest_stars)
    {
        params.keepNum = query->limit_to_brightest_stars.value();
    }
    if (query->scale_low && query->scale_high)
    {
        stellarSolver.setSearchScale(query->scale_low.value(), query->scale_high.value(), query->units);
    }
    if (query->ra && query->dec)
    {
        stellarSolver.setSearchPositionInDegrees(query->ra.value().Degrees(), query->dec.value().Degrees());
    }
    if (query->cpu_limit)
    {
        params.solverTimeLimit = query->cpu_limit.value();
    }
    stellarSolver.setParameters(params);
    if (!stellarSolver.solve()) // TODO Error Message CPU timeout or no index file found
    {
        printf("Did not solve (or no WCS file was written).\n");
        exit(1);
    }

    FITSImage::Solution solution = stellarSolver.getSolution();

    printf("Field center: (RA,Dec) = (%f, %f) deg.\n", solution.ra, solution.dec);
    dms ra_dms(solution.ra);
    dms dec_dms(solution.dec);
    printf("Field center: (RA H:M:S, Dec D:M:S) = (%s, %s)\n", ra_dms.toHMSString(true).toUtf8().data(), dec_dms.toDMSString(true, true).toUtf8().data());
    // TODO how is the unit of this selected in solve-field?
    printf("Field size: %f x %f arcminutes\n", solution.fieldWidth, solution.fieldHeight);
    printf("Field rotation angle: up is %f degrees E of N\n", solution.orientation);
    printf("Field parity: %s\n\n", FITSImage::getShortParityText(solution.parity).toUtf8().data());
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(StellarSolver::getVersion());
    QCoreApplication::setApplicationName("StellarSolver CLI");
    QCommandLineParser parser;
    parser.setApplicationDescription("Commandline interface for the StellarSolver library");
    StellarSolverCliQuery query;
    CommandLineParseResult parseResult = parseCommandLine(parser, &query);
    using Status = CommandLineParseResult::Status;
    switch (parseResult.statusCode)
    {
    case Status::Ok:
        for (int i = 0; i < query.image_files.size(); ++i)
        {
            printf("Reading input file %d out of %d: \"%s\"\n", i + 1, query.image_files.size(), query.image_files[i].toUtf8().constData());
            fflush(stdout);
            solve(query.image_files[i], &query);
        }
        break;
    case Status::Error:
        std::fputs(qPrintable(parseResult.errorString.value_or("Unknown error occurred")),
                   stderr);
        std::fputs("\n\n", stderr);
        return 1;
    case Status::VersionRequested:
        parser.showVersion();
        return 0;
    case Status::HelpRequested:
        parser.showHelp();
        return 0;
    }

    return 0;
}
