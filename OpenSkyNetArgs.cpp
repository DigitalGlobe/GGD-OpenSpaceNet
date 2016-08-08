/********************************************************************************
* Copyright 2016 DigitalGlobe, Inc.
* Author: Aleksey Vitebskiy
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
********************************************************************************/

#include "OpenSkyNetArgs.h"

#include "OpenSkyNet.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>
#include <fstream>
#include <iomanip>
#include <utility/Console.h>
#include <utility/program_options.hpp>
#include <vector/FeatureSet.h>

namespace dg { namespace osn {

namespace po = boost::program_options;

using namespace deepcore;

using boost::filesystem::path;
using boost::format;
using boost::iequals;
using boost::join;
using boost::make_unique;
using boost::to_lower;
using boost::to_lower_copy;
using std::cout;
using std::find;
using std::function;
using std::end;
using std::endl;
using std::ofstream;
using std::string;
using vector::FeatureSet;
using vector::GeometryType;

static const string OSN_USAGE =
    "Usage:\n"
        "  OpenSkyNet <action> <input options> <output options> <processing options>\n"
        "  OpenSkyNet <configuration file>\n\n"
        "Actions:\n"
        "  help     \t\t\t Show this help message\n"
        "  detect   \t\t\t Perform feature detection\n"
        "  landcover\t\t\t Perform land cover classification\n";

static const string OSN_DETECT_USAGE =
    "Run OpenSkyNet in feature detection mode.\n\n"
    "Usage:\n"
        "  OpenSkyNet detect <input options> <output options> <processing options>\n\n";

static const string OSN_LANDCOVER_USAGE =
    "Run OpenSkyNet in landcover classification mode.\n\n"
        "Usage:\n"
        "  OpenSkyNet landcover <input options> <output options> <processing options>\n\n";


OpenSkyNetArgs::OpenSkyNetArgs() :
    localOptions_("Local Image Input Options"),
    webOptions_("Web Service Input Options"),
    outputOptions_("Output Options"),
    processingOptions_("Processing Options"),
    detectOptions_("Feature Detection Options"),
    loggingOptions_("Logging Options"),
    generalOptions_("General Options"),
    supportedFormats_(FeatureSet::supportedFormats())
{
    localOptions_.add_options()
        ("image", po::value<string>()->value_name("PATH"),
         "If this is specified, the input will be taken from a local image.")
        ;

    webOptions_.add_options()
        ("service", po::value<string>()->value_name("SERVICE"),
         "Web service that will be the source of input. Valid values are: dgcs, evwhs, and maps-api.")
        ("token", po::value<string>()->value_name("TOKEN"),
         "API token used for licensing. This is the connectId for WMTS services or the API key for the Web Maps API.")
        ("credentials", po::value<string>()->value_name("USERNAME[:PASSWORD]"),
         "Credentials for the map service. Not required for Web Maps API. If password is not specified, you will be "
         "prompted to enter it. The credentials can also be set by setting the OSN_CREDENTIALS environment variable.")
        ("zoom", po::value<int>()->default_value(zoom)->value_name("ZOOM"), "Zoom level.")
        ("mapId", po::value<string>()->default_value(MAPSAPI_MAPID), "MapsAPI map id to use.")
        ("num-downloads", po::value<int>()->default_value(maxConnections)->value_name("NUM"),
         "Used to speed up downloads by allowing multiple concurrent downloads to happen at once.")
        ;

    string outputDescription = "Output file format for the results. Valid values are: ";
    outputDescription += join(supportedFormats_, ", ") + ".";

    auto notifier = function<void(const string&)>([this](const string& format) {
        DG_CHECK(find(supportedFormats_.begin(), supportedFormats_.end(), to_lower_copy(format)) != end(supportedFormats_),
            "Invalid output format: %s.", format.c_str());
    });

    outputOptions_.add_options()
        ("format", po::value<string>()->default_value("shp")->value_name("FORMAT")->notifier(notifier),
         outputDescription.c_str())
        ("output", po::value<string>()->value_name("PATH"),
         "Output location with file name and path or URL.")
        ("output-layer", po::value<string>()->value_name("NAME (=skynetdetects)"),
         "The output layer name, index name, or table name.")
        ("type", po::value<string>()->default_value("polygon")->value_name("TYPE"),
         "Output geometry type.  Currently only point and polygon are valid.")
        ("producer-info", "Add user name, application name, and application version to the output feature set.")
        ;

    processingOptions_.add_options()
        ("cpu", "Use the CPU for processing, the default it to use the GPU.")
        ("max-utilization", po::value<float>()->default_value(maxUtitilization)->value_name("PERCENT"),
         "Maximum GPU utilization %. Minimum is 5, and maximum is 100. Not used if processing on CPU")
        ("model", po::value<string>()->value_name("PATH"), "Path to the the trained model.")
        ("window-size", po::bounded_value<std::vector<int>>()->min_tokens(1)->max_tokens(2)->value_name("WIDTH [HEIGHT]"),
         "Overrides the original model's window size. Window size can be specified in either one or two dimensions. If "
         "only one dimension is specified, the window will be square. This parameter is optional and not recommended.")
        ;

    detectOptions_.add_options()
        ("confidence", po::value<float>()->default_value(confidence)->value_name("PERCENT"),
         "Minimum percent score for results to be included in the output.")
        ("step-size", po::bounded_value<std::vector<int>>()->min_tokens(1)->max_tokens(2)->value_name("WIDTH [HEIGHT]"),
         "Sliding window step size. Default value is log2 of the model window size. Step size can be specified in "
         "either one or two dimensions. If only one dimension is specified, the step size will be the same in both directions.")
        ("pyramid",
         "Use pyramids in feature detection. WARNING: This will result in much longer run times, but may result "
             "in additional features being detected.")
        ("nms", po::bounded_value<std::vector<float>>()->min_tokens(0)->max_tokens(1)->value_name((format("[PERCENT (=%g)]") % overlap).str().c_str()),
         "Perform non-maximum suppression on the output. You can optionally specify the overlap threshold percentage "
         "for non-maximum suppression calculation.")
        ;

    loggingOptions_.add_options()
        ("log", po::bounded_value<std::vector<string>>()->min_tokens(1)->max_tokens(2)->value_name("[LEVEL (=info)] PATH"),
         "Log to a file, a file name preceded by an optional log level must be specified. Permitted values for log "
         "level are: trace, debug, info, warning, error, fatal.")
        ("quiet", "If set, no output will be sent to console, only a log file, if specified.")
        ;

    generalOptions_.add_options()
        ("config", po::value<std::vector<string>>()->value_name("PATH")->multitoken(),
         "Use options from a configuration file.")
        ("help", "Show this help message")
        ;

    // Build the options used for processing
    optionsDescription_.add(localOptions_);
    optionsDescription_.add(webOptions_);
    optionsDescription_.add(outputOptions_);
    optionsDescription_.add(processingOptions_);
    optionsDescription_.add(detectOptions_);
    optionsDescription_.add(loggingOptions_);
    optionsDescription_.add(generalOptions_);

    optionsDescription_.add_options()
        ("action", po::value<string>()->value_name("ACTION"), "Action to perform.")
        ("help-topic", po::value<string>()->value_name("TOPIC"), "Help topic.")
        ("debug", "Switch console output to \"debug\" log level.")
        ("trace", "Switch console output to \"trace\" log level.")
        // This bbox argument works for both local and web options, but we have duplicated bbox argument
        // description in the usage display
        ("bbox", po::bounded_value<std::vector<double>>()->fixed_tokens(4))
        ;

    // Add the bbox argumet to both local and web options (duplication is not allowed when parsing the arguments)
    localOptions_.add_options()
        ("bbox", po::bounded_value<std::vector<double>>()->fixed_tokens(4)->value_name("WEST SOUTH EAST NORTH"),
         "Optional bounding box for image subset, optional for local images. Coordinates are specified in the "
         "following order: west longitude, south latitude, east longitude, and north latitude.");

    webOptions_.add_options()
        ("bbox", po::bounded_value<std::vector<double>>()->fixed_tokens(4)->value_name("WEST SOUTH EAST NORTH"),
         "Bounding box for determining tiles specified in WGS84 Lat/Lon coordinate system. Coordinates are "
         "specified in the following order: west longitude, south latitude, east longitude, and north latitude.");

    visibleOptions_.add(localOptions_);
    visibleOptions_.add(webOptions_);
    visibleOptions_.add(outputOptions_);
    visibleOptions_.add(processingOptions_);
    visibleOptions_.add(detectOptions_);
    visibleOptions_.add(loggingOptions_);
    visibleOptions_.add(generalOptions_);
}

void OpenSkyNetArgs::parseArgsAndProcess(int argc, const char* const* argv)
{
    setupConsoleLogging();
    parseArgs(argc, argv);

    if(maybeDisplayHelp()) {
        return;
    }

    readArgs();
}

void OpenSkyNetArgs::setupConsoleLogging()
{
    log::init();

    cerrSink_ = log::addCerrSink(dg::deepcore::level_t::warning, dg::deepcore::level_t::fatal,
                                     dg::deepcore::log::dg_log_format::dg_short_log);

    coutSink_ = log::addCoutSink(dg::deepcore::level_t::info, dg::deepcore::level_t::info,
                                dg::deepcore::log::dg_log_format::dg_short_log);
}

template<class T>
bool OpenSkyNetArgs::readOptional(const char* param, T& ret)
{
    auto it = vm_.find(param);
    if(it != end(vm_)) {
        ret = it->second.as<T>();
        return true;
    }

    return false;
}

template<class T>
T OpenSkyNetArgs::readRequired(const char* param, const char* errorMsg, bool showUsage)
{
    auto it = vm_.find(param);
    if(it == end(vm_)) {
        if(showUsage) {
            printUsage();
        }

        if(errorMsg) {
            DG_ERROR_THROW(errorMsg);
        } else {
            DG_ERROR_THROW("Missing required parameter --%s.", param);
        }
    }

    return it->second.as<T>();
}

template<class T>
bool OpenSkyNetArgs::readCoord(const char* param, T& x, T& y)
{
    std::vector<T> vec;
    if(!readOptional(param, vec)) {
        return false;
    }

    DG_CHECK(!vec.empty(), "%s must contain at least one parameter", param);

    x = vec[0];
    y = vec.size() > 1 ? vec[1] : x;

    return true;
}

void OpenSkyNetArgs::parseArgs(int argc, const char* const* argv)
{
    if(argc < 2) {
        printUsage();
        DG_ERROR_THROW("Must have at least 1 argument.");
    }

    po::positional_options_description pd;
    pd.add("action", 1)
      .add("help-topic", 1);

    // parse regular and positional options
    po::store(po::command_line_parser(argc, argv)
                  .extra_style_parser(&po::ignore_numbers)
                  .options(optionsDescription_)
                  .positional(pd)
                  .run(), vm_);
    po::notify(vm_);

    // parse environment variable options
    po::store(po::parse_environment(optionsDescription_, "OSN_"), vm_);

    // See if we have --config option(s), parse it if we do
    std::vector<string> configFiles;
    if(readOptional("config", configFiles)) {
        for(const auto& configFile : configFiles) {
            po::store(po::parse_config_file<char>(configFile.c_str(), optionsDescription_), vm_);
            po::notify(vm_);
        }
    }

    // See what the action argument is, if we couldn't make anything of it, see if it's a config file
    auto actionStr = readRequired<string>("action", "Action or a configuration file must be specified.", true);
    action = parseAction(actionStr);
    if(action == Action::UNKNOWN) {
        po::store(po::parse_config_file<char>(actionStr.c_str(), optionsDescription_), vm_);
        po::notify(vm_);

        action = parseAction(readRequired<string>("action", "Action must be specified.", true));
    }

    if(action == Action::UNKNOWN) {
        printUsage();
        DG_ERROR_THROW("Invalid action.");
    }
}

Action OpenSkyNetArgs::parseAction(string str) const
{
    to_lower(str);
    if(str == "help") {
        return Action::HELP;
    } else if(str == "detect") {
        return Action::DETECT;
    } else if(str == "landcover") {
        return Action::LANDCOVER;
    }

    return Action::UNKNOWN;
}

Source OpenSkyNetArgs::parseService(string service) const
{
    to_lower(service);
    if(service == "dgcs") {
        return Source::DGCS;
    } else if(service == "evwhs") {
        return Source::EVWHS;
    } else if(service == "maps-api") {
        return Source::MAPS_API;
    }

    DG_ERROR_THROW("Invalid --service parameter: %s", service.c_str());
}

bool OpenSkyNetArgs::maybeDisplayHelp()
{
    // If "action" is "help", see if there's a topic. Display all help if there isn't
    string topicStr;
    if(action == Action::HELP) {
        if(readOptional("help-topic", topicStr)) {
            auto topic = parseAction(topicStr);
            printUsage(topic);
            DG_CHECK(topic != Action::UNKNOWN, "Invalid help topic specified.");
        } else {
            printUsage();
        }
        return true;
    } else if(readOptional("help-topic", topicStr)){
        // If we have "help" listed after the "action", we display help for that action
        DG_CHECK(iequals(topicStr, "help"), "Invalid argument: %s", topicStr.c_str());

        printUsage(action);
        return true;
    }

    return false;
}

void OpenSkyNetArgs::printUsage(Action action) const
{
    switch(action) {
        case Action::LANDCOVER:
        {
            po::options_description desc;
            desc.add(localOptions_);
            desc.add(webOptions_);
            desc.add(outputOptions_);
            desc.add(processingOptions_);
            desc.add(loggingOptions_);
            desc.add(generalOptions_);

            cout << OSN_LANDCOVER_USAGE << desc;
        }
            break;

        case Action::DETECT:
            cout << OSN_DETECT_USAGE << visibleOptions_;
            break;

        default:
            cout << OSN_USAGE << visibleOptions_;
            break;
    }

    cout << endl;
}

void OpenSkyNetArgs::readArgs()
{
    readBoundingBoxArgs();

    string service;
    if(readOptional("service", service)) {
        source = parseService(service);
        readWebServiceArgs();
    } else {
        image = readRequired<string>("image", "No input specified, either --service or --image argument must be present.");
        source = Source::LOCAL;
    }

    readProcessingArgs();
    readOutputArgs();
    readFeatureDetectionArgs();
    readLoggingArgs();

    // If we have all the arguments parsed correctly, now is the time to turn off console logging
    if(vm_.find("quiet") != end(vm_)) {
        log::removeSink(coutSink_);

        // Turn off error logging as well, but only if we are writing a log file
        if(vm_.find("log") != end(vm_)) {
            log::removeSink(cerrSink_);
        }
        quiet = true;
    }

    OpenSkyNet osn(*this);
    osn.process();
}

void OpenSkyNetArgs::readBoundingBoxArgs()
{
    std::vector<double> bboxVec;
    if(readOptional("bbox", bboxVec)) {
        DG_CHECK(bboxVec.size() == 4, "Bounding box must contain 4 values");
        bbox = make_unique<cv::Rect2d>(cv::Point2d { bboxVec[0], bboxVec[1] }, cv::Point2d { bboxVec[2], bboxVec[3] });
    }
}

void OpenSkyNetArgs::readWebServiceArgs()
{
    DG_CHECK(bbox, "The --bbox argument is required for web services.");

    if(source == Source::MAPS_API) {
        readOptional("mapId", mapId);
    }

    token = readRequired<string>("token");
    if(readOptional("credentials", credentials) && credentials.find(':') == string::npos) {
        promptForPassword();
    }

    readOptional("zoom", zoom);
    readOptional("num-downloads", maxConnections);
}


void OpenSkyNetArgs::promptForPassword()
{
    cout << "Enter your web service password: ";
    auto password = readMaskedInputFromConsole();
    credentials += ":";
    credentials += password;
}

void OpenSkyNetArgs::readOutputArgs()
{
    readOptional("format", outputFormat);
    to_lower(outputFormat);

    outputPath = readRequired<string>("output");

    auto layerSpecified = readOptional("output-layer", layerName);

    if(outputFormat  == "shp") {
        if(layerSpecified) {
            OSN_LOG(warning) << "output-layer argument is ignored for Shapefile output.";
        }
        layerName = path(outputPath).stem().filename().string();
    } else if(!layerSpecified) {
        layerName = "skynetdetects";
    }

    string typeStr = "polygon";
    readOptional("type", typeStr);
    to_lower(typeStr);
    if(typeStr == "polygon") {
        geometryType = GeometryType::POLYGON;
    } else if(typeStr == "point") {
        geometryType = GeometryType::POINT;
    } else {
        DG_ERROR_THROW("Invalid geometry type: %s", typeStr.c_str());
    }

    producerInfo = vm_.find("producer-info") != end(vm_);
}

void OpenSkyNetArgs::readProcessingArgs()
{
    useCpu = vm_.find("cpu") != end(vm_);
    readOptional("max-utilization", maxUtitilization);
    modelPath = readRequired<string>("model");

    cv::Size size;
    if(readCoord("window-size", size.width, size.height)) {
        windowSize = make_unique<cv::Size>(size);
    }
}

void OpenSkyNetArgs::readFeatureDetectionArgs()
{
    readOptional("confidence", confidence);

    cv::Point size;
    if(readCoord("step-size", size.x, size.y)) {
        stepSize = make_unique<cv::Point>(size);
    }

    pyramid = vm_.find("pyramid") != end(vm_);

    if(vm_.find("nms") != end(vm_)) {
        nms = true;
        std::vector<float> args;
        readOptional("nms", args);
        if(args.size()) {
            overlap = args[0];
        }
    }
}

void OpenSkyNetArgs::readLoggingArgs()
{
    if(vm_.find("trace") != end(vm_)) {
        log::removeSink(coutSink_);
        coutSink_ = log::addCoutSink(level_t::trace, level_t::info, log::dg_log_format::dg_short_log);
    } else if(vm_.find("debug") != end(vm_)) {
        log::removeSink(coutSink_);
        coutSink_ = log::addCoutSink(level_t::debug, level_t::info, dg::deepcore::log::dg_log_format::dg_short_log);
    }

    std::vector<string> logArgs;
    if(readOptional("log", logArgs)) {
        DG_CHECK(!logArgs.empty(), "Log path must be specified");
        auto level = level_t::debug;
        string path;

        if(logArgs.size() == 1) {
            path = logArgs[0];
        } else {
            level = log::stringToLevel(logArgs[0]);
            path = logArgs[1];
        }

        auto ofs = boost::make_shared<ofstream>(path);
        DG_CHECK(!ofs->fail(), "Error opening log file %s for writing.", path.c_str());

        log::addStreamSink(ofs, level, level_t::fatal, log::dg_long_log);
    }
}

} } // namespace dg { namespace osn {