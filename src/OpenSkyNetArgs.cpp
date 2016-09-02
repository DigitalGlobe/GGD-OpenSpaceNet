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
#include <boost/range/adaptor/reversed.hpp>
#include <boost/tokenizer.hpp>
#include <fstream>
#include <imagery/cv_program_options.hpp>
#include <iomanip>
#include <utility/Console.h>
#include <vector/FeatureSet.h>

namespace dg { namespace osn {

namespace po = boost::program_options;

using namespace deepcore;

using boost::adaptors::reverse;
using boost::filesystem::path;
using boost::format;
using boost::iequals;
using boost::join;
using boost::make_unique;
using boost::tokenizer;
using boost::escaped_list_separator;
using boost::to_lower;
using boost::to_lower_copy;
using std::cout;
using std::find;
using std::function;
using std::end;
using std::endl;
using std::move;
using std::ofstream;
using std::string;
using std::unique_ptr;
using vector::FeatureSet;
using vector::GeometryType;

static const string OSN_USAGE =
    "Usage:\n"
        "  OpenSkyNet <action> <input options> <output options> <processing options>\n"
        "  OpenSkyNet --config <configuration file> [other options]\n\n"
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

    auto formatNotifier = function<void(const string&)>([this](const string& format) {
        DG_CHECK(find(supportedFormats_.begin(), supportedFormats_.end(), to_lower_copy(format)) != end(supportedFormats_),
            "Invalid output format: %s.", format.c_str());
    });

    outputOptions_.add_options()
        ("format", po::value<string>()->default_value("shp")->value_name("FORMAT")->notifier(formatNotifier),
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
        ("window-size", po::cvSize_value()->min_tokens(1)->value_name("WIDTH [HEIGHT]"),
         "Overrides the original model's window size. Window size can be specified in either one or two dimensions. If "
         "only one dimension is specified, the window will be square. This parameter is optional and not recommended.")
        ;

    detectOptions_.add_options()
        ("confidence", po::value<float>()->default_value(confidence)->value_name("PERCENT"),
         "Minimum percent score for results to be included in the output.")
        ("step-size", po::cvPoint_value()->min_tokens(1)->value_name("WIDTH [HEIGHT]"),
         "Sliding window step size. Default value is log2 of the model window size. Step size can be specified in "
         "either one or two dimensions. If only one dimension is specified, the step size will be the same in both directions.")
        ("pyramid",
         "Use pyramids in feature detection. WARNING: This will result in much longer run times, but may result "
             "in additional features being detected.")
        ("nms", po::bounded_value<std::vector<float>>()->min_tokens(0)->max_tokens(1)->value_name((format("[PERCENT (=%g)]") % overlap).str().c_str()),
         "Perform non-maximum suppression on the output. You can optionally specify the overlap threshold percentage "
         "for non-maximum suppression calculation.")
         ("include-labels", po::value<std::vector<string>>()->multitoken()->value_name("LABEL [LABEL...]"),
          "Filter results to only include specified labels.")
         ("exclude-labels", po::value<std::vector<string>>()->multitoken()->value_name("LABEL [LABEL...]"),
          "Filter results to exclude specified labels.")
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
        ("bbox", po::cvRect2d_value())
        ;

    // Add the bbox argumet to both local and web options (duplication is not allowed when parsing the arguments)
    localOptions_.add_options()
        ("bbox", po::cvRect2d_value()->value_name("WEST SOUTH EAST NORTH"),
         "Optional bounding box for image subset, optional for local images. Coordinates are specified in the "
         "following order: west longitude, south latitude, east longitude, and north latitude.");

    webOptions_.add_options()
        ("bbox", po::cvRect2d_value()->value_name("WEST SOUTH EAST NORTH"),
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
    setupInitialLogging();

    for (int i = 1; i < argc;  ++i) {
        if(strcmp(argv[i], "--help") == 0) {
            displayHelp = true;
        }
    }

    try {
        parseArgs(argc, argv);
        validateArgs();
    } catch (...) {
        if (!displayHelp) {
            throw;
        }
    }

    if (displayHelp) {
        printUsage(action);
        return;
    }

    setupLogging();

    OpenSkyNet osn(*this);
    osn.process();
}

void OpenSkyNetArgs::setupInitialLogging()
{
    log::init();

    cerrSink_ = log::addCerrSink(dg::deepcore::level_t::warning, dg::deepcore::level_t::fatal,
                                     dg::deepcore::log::dg_log_format::dg_short_log);

    coutSink_ = log::addCoutSink(dg::deepcore::level_t::info, dg::deepcore::level_t::info,
                                dg::deepcore::log::dg_log_format::dg_short_log);
}

void OpenSkyNetArgs::setupLogging() {
    quiet = consoleLogLevel < level_t::warning;

    // If no file is specified, assert that warning and above goes to the console
    if (fileLogPath.empty() && consoleLogLevel > level_t::warning) {
        consoleLogLevel = level_t::warning;
    }

    // If we have all the arguments parsed correctly, now is the time to turn off console logging
    if(consoleLogLevel > level_t::info) {
        log::removeSink(coutSink_);
        log::removeSink(cerrSink_);
        cerrSink_ = log::addCerrSink(consoleLogLevel, level_t::fatal, log::dg_log_format::dg_short_log);
    } else if(consoleLogLevel < level_t::info) {
        log::removeSink(coutSink_);
        coutSink_ = log::addCoutSink(consoleLogLevel, level_t::info, log::dg_log_format::dg_short_log);
    }

    // Setup a file logger
    if (!fileLogPath.empty()) {
        auto ofs = boost::make_shared<ofstream>(fileLogPath);
        DG_CHECK(!ofs->fail(), "Error opening log file %s for writing.", fileLogPath.c_str());
        log::addStreamSink(ofs, fileLogLevel, level_t::fatal, log::dg_log_format::dg_long_log);
    }
}

template<class T>
static bool readVariable(const char* param, variables_map vm, T& ret)
{
    auto it = vm.find(param);
    if(it != end(vm)) {
        ret = it->second.as<T>();
        return true;
    }

    return false;
}

template <typename T>
static unique_ptr<T> readVariable(const char* param, variables_map vm)
{
    auto it = vm.find(param);
    if(it != end(vm)) {
        return make_unique<T>(it->second.as<T>());
    } else {
        return unique_ptr<T>();
    }
}

static bool readVariable(const char* param, variables_map vm, std::vector<std::string>& ret, bool splitArgs)
{
    auto it = vm.find(param);
    if(it != end(vm)) {
        auto args = it->second.as<std::vector<string>>();
        if(splitArgs) {
            ret.clear();
            for(const auto& arg : args) {
                using so_tokenizer = tokenizer<escaped_list_separator<char> >;
                so_tokenizer tok(arg, escaped_list_separator<char>('\\', ' ', '\"'));
                ret.insert(end(ret), begin(tok), end(tok));
            }
        } else {
            ret = move(args);
        }

        return true;
    }

    return false;
}

// Order of precedence: config files from env, env, config files from cli, cli.
void OpenSkyNetArgs::parseArgs(int argc, const char* const* argv)
{
    po::positional_options_description pd;
    pd.add("action", 1)
      .add("help-topic", 1);

    // parse environment variable options
    variables_map environment_vm;
    po::store(po::parse_environment(optionsDescription_, "OSN_"), environment_vm);
    po::notify(environment_vm);
    readArgs(environment_vm, true);

    // parse regular and positional options
    variables_map command_line_vm;
    po::store(po::command_line_parser(argc, argv)
                  .extra_style_parser(&po::ignore_numbers)
                  .options(optionsDescription_)
                  .positional(pd)
                  .run(), command_line_vm);
    po::notify(command_line_vm);
    readArgs(command_line_vm);
}

static Action parseAction(string str)
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

static Source parseService(string service)
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


void OpenSkyNetArgs::validateArgs()
{
    // Validate action args.  "Required" results in an error if unspecified. "Unused" results in a warning if specified.
    bool unusedStepSize = false;
    bool unusedNms= false;
    bool unusedPyramid = false;
    bool unusedConfidence = false;
    switch (action) {
        case Action::DETECT:
            break;

        case Action::LANDCOVER:
            unusedStepSize = true;
            unusedNms= true;
            unusedPyramid = true;
            unusedConfidence = true;
            break;

        case Action::HELP:
            return;

        default:
            DG_ERROR_THROW("Invalid action.");
    }

    if (unusedStepSize && (stepSize.get() != nullptr)) {
        OSN_LOG(warning) << "Argument --step-size is unused for LANDCOVER.";
    }

    if (unusedNms && nms) {
        OSN_LOG(warning) << "Argument --nms is unused for LANDCOVER.";
    }

    if (unusedPyramid && pyramid) {
        OSN_LOG(warning) << "Argument --pyramid is unused for LANDCOVER.";
    }

    if (unusedConfidence && confidenceSet) {
        OSN_LOG(warning) << "Argument --confidence is unused for LANDCOVER.";
    }


    // Validate source args.  "Required" results in an error if unspecified. "Unused" results in a warning if specified.
    bool unusedMapId = false;
    bool requireBbox = false;
    bool unusedToken = false;
    bool requireToken = false;
    bool unusedCredentials = false;
    bool requireCredentials = false;
    string sourceName;

    switch (source) {
        case Source::LOCAL:
            unusedMapId = true;
            unusedToken = true;
            unusedCredentials = true;
            sourceName = "a local image";
            break;

        case Source::MAPS_API:
            requireBbox = true;
            requireToken = true;
            unusedCredentials = true;
            sourceName = "maps-api";
            break;

        case Source::DGCS:
        case Source::EVWHS:
            requireBbox = true;
            requireToken = true;
            requireCredentials = true;
            unusedMapId = true;
            sourceName = "dgcs or evwhs";
            break;

        default:
            DG_ERROR_THROW("Source is unknown or unspecified");
    }

    if (requireToken && token.empty()) {
        DG_ERROR_THROW("Argument --token is required for %s.", sourceName.c_str());
    } else if (unusedToken && !token.empty()) {
        OSN_LOG(warning) << "Argument --token is unused for " << sourceName << '.';
    }

    if (requireCredentials && credentials.empty()) {
        DG_ERROR_THROW("Argument --credentials argument is required for %s.", sourceName.c_str());
    } else if (unusedCredentials && !credentials.empty()) {
        OSN_LOG(warning) << "Argument --credentials is unused for " << sourceName << '.';
    }

    if (unusedMapId && mapIdSet) {
        OSN_LOG(warning) << "Argument --mapId is unused for " << sourceName << '.';
    }

    if (requireBbox && (bbox.get() == nullptr)) {
        DG_ERROR_THROW("Argument --bbox is required for %s.", sourceName.c_str());
    }


    // validate model and detection
    if (modelPath.empty()) {
        DG_ERROR_THROW("Argument --model is required.");
    }

    if (!includeLabels.empty() && !excludeLabels.empty()) {
        DG_ERROR_THROW("Arguments --include-labels and --exclude-labels may not be specified at the same time.");
    }


    // validate output
    if (outputPath.empty()) {
        DG_ERROR_THROW("Argument --output is required.");
    }

    if(outputFormat  == "shp") {
        if(!layerName.empty()) {
            OSN_LOG(warning) << "Argument --output-layer is ignored for Shapefile output.";
        }
        layerName = path(outputPath).stem().filename().string();
    } else if(layerName.empty()) {
        layerName = "skynetdetects";
    }


    // Ask for password, if not specified
    if (requireCredentials && !displayHelp && credentials.find(':') == string::npos) {
        promptForPassword();
    }
}

void OpenSkyNetArgs::promptForPassword()
{
    cout << "Enter your web service password: ";
    auto password = readMaskedInputFromConsole();
    credentials += ":";
    credentials += password;
}


void OpenSkyNetArgs::readArgs(variables_map vm, bool splitArgs) {
    // See if we have --config option(s), parse it if we do
    std::vector<string> configFiles;
    if (readVariable("config", vm, configFiles, splitArgs)) {
        // Parse config files in reverse order because parse_config_file will not override existing options. This way
        // options apply as "last one wins". The arguments specified on the command line always win.
        for (const auto &configFile : reverse(configFiles)) {
            variables_map config_vm;
            po::store(po::parse_config_file<char>(configFile.c_str(), optionsDescription_), config_vm);
            po::notify(config_vm);
            readArgs(config_vm, true);
        }
    }

    bbox = readVariable<cv::Rect2d>("bbox", vm);

    string service;
    if (readVariable("service", vm, service)) {
        source = parseService(service);
        readWebServiceArgs(vm);
    } else if (readVariable("image", vm, image)) {
        source = Source::LOCAL;
    }

    string actionString;
    readVariable("action", vm, actionString);
    action = parseAction(actionString);
    maybeDisplayHelp(vm);

    readWebServiceArgs(vm);
    readProcessingArgs(vm);
    readOutputArgs(vm);
    readFeatureDetectionArgs(vm);
    readLoggingArgs(vm);
}

void OpenSkyNetArgs::maybeDisplayHelp(variables_map vm)
{
    // If "action" is "help", see if there's a topic. Display all help if there isn't
    string topicStr;
    if(action == Action::HELP) {
        if(readVariable("help-topic", vm, topicStr)) {
            action = parseAction(topicStr);
            displayHelp = true;
        } else {
            printUsage();
        }
        displayHelp = true;
    } else if(readVariable("help-topic", vm, topicStr)) {
        // If we have "help" listed after the "action", we display help for that action
        DG_CHECK(iequals(topicStr, "help"), "Invalid argument: %s", topicStr.c_str());
        displayHelp = true;
    }
}

void OpenSkyNetArgs::readWebServiceArgs(variables_map vm, bool splitArgs)
{
    mapIdSet |= readVariable("mapId", vm, mapId);
    readVariable("token", vm, token);
    readVariable("credentials", vm, credentials);
    readVariable("zoom", vm, zoom);
    readVariable("num-downloads", vm, maxConnections);
}


void OpenSkyNetArgs::readOutputArgs(variables_map vm, bool splitArgs)
{
    readVariable("format", vm, outputFormat);
    to_lower(outputFormat);

    readVariable("output", vm, outputPath);
    readVariable("output-layer", vm, layerName);

    string typeStr = "polygon";
    readVariable("type", vm, typeStr);
    to_lower(typeStr);
    if(typeStr == "polygon") {
        geometryType = GeometryType::POLYGON;
    } else if(typeStr == "point") {
        geometryType = GeometryType::POINT;
    } else {
        DG_ERROR_THROW("Invalid geometry type: %s", typeStr.c_str());
    }

    producerInfo = vm.find("producer-info") != end(vm);
}

void OpenSkyNetArgs::readProcessingArgs(variables_map vm, bool splitArgs)
{
    useCpu = vm.find("cpu") != end(vm);
    readVariable("max-utilization", vm, maxUtitilization);
    readVariable("model", vm, modelPath);
    windowSize = readVariable<cv::Size>("window-size", vm);
}

void OpenSkyNetArgs::readFeatureDetectionArgs(variables_map vm, bool splitArgs)
{
    readVariable("include-labels", vm, includeLabels, splitArgs);
    readVariable("exclude-labels", vm, excludeLabels, splitArgs);

    confidenceSet |= readVariable("confidence", vm, confidence);
    stepSize = readVariable<cv::Point>("step-size", vm);
    pyramid = vm.find("pyramid") != end(vm);

    if(vm.find("nms") != end(vm)) {
        nms = true;
        std::vector<float> args;
        readVariable("nms", vm, args);
        if(args.size()) {
            overlap = args[0];
        }
    }
}

void OpenSkyNetArgs::readLoggingArgs(variables_map vm, bool splitArgs)
{
    if(vm.find("quiet") != end(vm)) {
        consoleLogLevel = level_t::fatal;
    } if(vm.find("trace") != end(vm)) {
        consoleLogLevel = level_t::trace;
    } else if(vm.find("debug") != end(vm)) {
        consoleLogLevel = level_t::debug;
    }

    std::vector<string> logArgs;
    if(readVariable("log", vm, logArgs, splitArgs)) {
        DG_CHECK(!logArgs.empty(), "Log path must be specified");
        fileLogLevel = level_t::debug;

        if(logArgs.size() == 1) {
            fileLogPath = logArgs[0];
        } else {
            fileLogLevel = log::stringToLevel(logArgs[0]);
            fileLogPath = logArgs[1];
        }
    }
}

} } // namespace dg { namespace osn {