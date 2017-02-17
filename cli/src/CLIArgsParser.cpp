/********************************************************************************
* Copyright 2017 DigitalGlobe, Inc.
* Author: Aleksey Vitebskiy
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
********************************************************************************/
#include "CLIArgsParser.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/tokenizer.hpp>
#include <fstream>
#include <geometry/cv_program_options.hpp>
#include <iomanip>
#include <utility/Console.h>
#include <utility/program_options.hpp>
#include <vector/FeatureSet.h>

namespace dg { namespace osn {

namespace po = boost::program_options;

using namespace deepcore;

using boost::program_options::variables_map;
using boost::program_options::name_with_default;
using boost::adaptors::reverse;
using boost::bad_lexical_cast;
using boost::filesystem::path;
using boost::iequals;
using boost::join;
using boost::lexical_cast;
using boost::make_unique;
using boost::tokenizer;
using boost::escaped_list_separator;
using boost::to_lower;
using boost::to_lower_copy;
using dg::deepcore::level_t;
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
        "  OpenSpaceNet <action> <input options> <output options> <processing options>\n"
        "  OpenSpaceNet --config <configuration file> [other options]\n\n"
        "Actions:\n"
        "  help     \t\t\t Show this help message\n"
        "  detect   \t\t\t Perform feature detection\n"
        "  landcover\t\t\t Perform land cover classification\n";

static const string OSN_DETECT_USAGE =
    "Run OpenSpaceNet in feature detection mode.\n\n"
    "Usage:\n"
        "  OpenSpaceNet detect <input options> <output options> <processing options>\n\n";

static const string OSN_LANDCOVER_USAGE =
    "Run OpenSpaceNet in landcover classification mode.\n\n"
        "Usage:\n"
        "  OpenSpaceNet landcover <input options> <output options> <processing options>\n\n";

CLIArgsParser::CLIArgsParser() :
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
         "Web service that will be the source of input. Valid values are: dgcs, evwhs, maps-api, and tile-json.")
        ("token", po::value<string>()->value_name("TOKEN"),
         "API token used for licensing. This is the connectId for WMTS services or the API key for the Web Maps API.")
        ("credentials", po::value<string>()->value_name("USERNAME[:PASSWORD]"),
         "Credentials for the map service. Not required for Web Maps API, optional for TileJSON. If password is not specified, you will be "
         "prompted to enter it. The credentials can also be set by setting the OSN_CREDENTIALS environment variable.")
        ("url", po::value<string>()->value_name("URL"),
         "TileJSON server URL. This is only required for the tile-json service.")
        ("use-tiles",
         "If set, the \"tiles\" field in TileJSON metadata will be used as the tile service address. The default behavior"
         "is to derive the service address from the provided URL.")
        ("zoom", po::value<int>()->value_name(name_with_default("ZOOM", osnArgs.zoom)), "Zoom level.")
        ("mapId", po::value<string>()->value_name(name_with_default("MAPID", osnArgs.mapId)), "MapsAPI map id to use.")
        ("num-downloads", po::value<int>()->value_name(name_with_default("NUM", osnArgs.maxConnections)),
         "Used to speed up downloads by allowing multiple concurrent downloads to happen at once.")
        ;

    string outputDescription = "Output file format for the results. Valid values are: ";
    outputDescription += join(supportedFormats_, ", ") + ".";

    auto formatNotifier = function<void(const string&)>([this](const string& format) {
        DG_CHECK(find(supportedFormats_.begin(), supportedFormats_.end(), to_lower_copy(format)) != end(supportedFormats_),
            "Invalid output format: %s.", format.c_str());
    });

    outputOptions_.add_options()
        ("format", po::value<string>()->value_name(name_with_default("FORMAT", osnArgs.outputFormat))->notifier(formatNotifier),
         outputDescription.c_str())
        ("output", po::value<string>()->value_name("PATH"),
         "Output location with file name and path or URL.")
        ("output-layer", po::value<string>()->value_name(name_with_default("NAME", "skynetdetects")),
         "The output layer name, index name, or table name.")
        ("type", po::value<string>()->value_name(name_with_default("TYPE", "polygon")),
         "Output geometry type.  Currently only point and polygon are valid.")
        ("producer-info", "Add user name, application name, and application version to the output feature set.")
        ("append", "Append to an existing vector set. If the output does not exist, it will be created.")
        ;

    processingOptions_.add_options()
        ("cpu", "Use the CPU for processing, the default it to use the GPU.")
        ("max-utilization", po::value<float>()->value_name(name_with_default("PERCENT", osnArgs.maxUtilization)),
         "Maximum GPU utilization %. Minimum is 5, and maximum is 100. Not used if processing on CPU")
        ("model", po::value<string>()->value_name("PATH"), "Path to the the trained model.")
        ("window-size", po::cvSize_value()->min_tokens(1)->value_name("WIDTH [HEIGHT]"),
         "Overrides the original model's window size. Window size can be specified in either one or two dimensions. If "
         "only one dimension is specified, the window will be square. This parameter is optional and not recommended.")
        ;

    detectOptions_.add_options()
        ("confidence", po::value<float>()->value_name(name_with_default("PERCENT", osnArgs.confidence)),
         "Minimum percent score for results to be included in the output.")
        ("step-size", po::cvPoint_value()->min_tokens(1)->value_name("WIDTH [HEIGHT]"),
         "Sliding window step size. Default value is log2 of the model window size. Step size can be specified in "
         "either one or two dimensions. If only one dimension is specified, the step size will be the same in both directions.")
        ("pyramid",
         "Use pyramids in feature detection. WARNING: This will result in much longer run times, but may result "
             "in additional features being detected.")
        ("nms", po::bounded_value<std::vector<float>>()->min_tokens(0)->max_tokens(1)->value_name(name_with_default("PERCENT", osnArgs.overlap)),
         "Perform non-maximum suppression on the output. You can optionally specify the overlap threshold percentage "
         "for non-maximum suppression calculation.")
        ("include-labels", po::value<std::vector<string>>()->multitoken()->value_name("LABEL [LABEL...]"),
         "Filter results to only include specified labels.")
        ("exclude-labels", po::value<std::vector<string>>()->multitoken()->value_name("LABEL [LABEL...]"),
         "Filter results to exclude specified labels.")
        ("pyramid-window-sizes", po::value<std::vector<std::string>>()->multitoken()->value_name("SIZE [SIZE...]"),
         "Sliding window sizes to match to pyramid levels. --pyramid-step-sizes argument must be present and have the same number of values.")
        ("pyramid-step-sizes", po::value<std::vector<std::string>>()->multitoken()->value_name("SIZE [SIZE...]"),
         "Sliding window step sizes to match to pyramid levels. --pyramid-window-sizes argument must be present and have the same number of values.")
        ;

    loggingOptions_.add_options()
        ("log", po::bounded_value<std::vector<string>>()->min_tokens(1)->max_tokens(2)->value_name("[LEVEL (=debug)] PATH"),
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

void CLIArgsParser::parseArgsAndProcess(int argc, const char* const* argv)
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
        if (displayHelp) {
            printUsage(osnArgs.action);
        }
        throw;
    }

    if (displayHelp) {
        printUsage(osnArgs.action);
        return;
    }

    setupLogging();

    OpenSpaceNet osn(osnArgs);
    osn.process();
}

void CLIArgsParser::setupInitialLogging()
{
    log::init();

    cerrSink_ = log::addCerrSink(dg::deepcore::level_t::warning, dg::deepcore::level_t::fatal,
                                     dg::deepcore::log::dg_log_format::dg_short_log);

    coutSink_ = log::addCoutSink(dg::deepcore::level_t::info, dg::deepcore::level_t::info,
                                dg::deepcore::log::dg_log_format::dg_short_log);
}

void CLIArgsParser::setupLogging() {
    osnArgs.quiet = consoleLogLevel > level_t::info;

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
static bool readVariable(const char* param, const variables_map& vm, T& ret)
{
    auto it = vm.find(param);
    if(it != end(vm)) {
        ret = it->second.as<T>();
        return true;
    }

    return false;
}

template <typename T>
static unique_ptr<T> readVariable(const char* param, const variables_map& vm)
{
    auto it = vm.find(param);
    if(it != end(vm)) {
        return make_unique<T>(it->second.as<T>());
    } else {
        return unique_ptr<T>();
    }
}

template<typename T>
static bool readVariable(const char* param, variables_map& vm, std::vector<T>& ret, bool splitArgs)
{
    auto it = vm.find(param);
    if(it != end(vm)) {
        if(splitArgs) {
            auto args = it->second.as<std::vector<string>>();
            ret.clear();
            for(const auto& arg : args) {
                using so_tokenizer = tokenizer<escaped_list_separator<char> >;
                so_tokenizer tok(arg, escaped_list_separator<char>('\\', ' ', '\"'));
                for(const auto& a : tok) {
                    try {
                        ret.push_back(lexical_cast<T>(a));
                    } catch(bad_lexical_cast&) {
                        DG_ERROR_THROW("Invalid --%s parameter: %s", param, arg.c_str());
                    }
                }
            }
        } else {
            ret = it->second.as<std::vector<T>>();
        }

        return true;
    }

    return false;
}

// Order of precedence: config files from env, env, config files from cli, cli.
void CLIArgsParser::parseArgs(int argc, const char* const* argv)
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
    } else if(service == "tile-json") {
        return Source::TILE_JSON;
    }

    DG_ERROR_THROW("Invalid --service parameter: %s", service.c_str());
}

void CLIArgsParser::printUsage(Action action) const
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


void CLIArgsParser::validateArgs()
{
    // Validate action args.  "Required" results in an error if unspecified. "Unused" results in a warning if specified.
    bool unusedStepSize = false;
    bool unusedNms= false;
    bool unusedPyramid = false;
    bool unusedConfidence = false;
    switch (osnArgs.action) {
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

    if (unusedStepSize && (osnArgs.stepSize.get() != nullptr)) {
        OSN_LOG(warning) << "Argument --step-size is unused for LANDCOVER.";
    }

    if (unusedNms && osnArgs.nms) {
        OSN_LOG(warning) << "Argument --nms is unused for LANDCOVER.";
    }

    if (unusedPyramid && osnArgs.pyramid) {
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
    bool requireUrl = false;
    bool unusedUseTiles = true;
    string sourceName;

    switch (osnArgs.source) {
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

        case Source::TILE_JSON:
            requireBbox = true;
            requireToken = false;
            unusedCredentials = false;
            requireUrl = true;
            unusedUseTiles = false;
            sourceName = "tile-json";
            break;

        default:
            DG_ERROR_THROW("Source is unknown or unspecified");
    }

    if (requireToken && osnArgs.token.empty()) {
        DG_ERROR_THROW("Argument --token is required for %s.", sourceName.c_str());
    } else if (unusedToken && !osnArgs.token.empty()) {
        OSN_LOG(warning) << "Argument --token is unused for " << sourceName << '.';
    }

    if (requireCredentials && osnArgs.credentials.empty()) {
        DG_ERROR_THROW("Argument --credentials argument is required for %s.", sourceName.c_str());
    } else if (unusedCredentials && !osnArgs.credentials.empty()) {
        OSN_LOG(warning) << "Argument --credentials is unused for " << sourceName << '.';
    }

    if (unusedMapId && mapIdSet) {
        OSN_LOG(warning) << "Argument --mapId is unused for " << sourceName << '.';
    }

    if (requireBbox && (osnArgs.bbox.get() == nullptr)) {
        DG_ERROR_THROW("Argument --bbox is required for %s.", sourceName.c_str());
    }

    if (unusedUseTiles && osnArgs.useTiles) {
        OSN_LOG(warning) << "Argument --use-tiles is unused for " << sourceName << '.';
    }


    if(requireUrl && osnArgs.url.empty()) {
        DG_ERROR_THROW("Argument --url is required for %s.", sourceName.c_str());
    } else if(!requireUrl && !osnArgs.url.empty()) {
        OSN_LOG(warning) << "Argument --url is unused for " << sourceName << '.';
    }

    // validate model and detection
    if (osnArgs.modelPath.empty()) {
        DG_ERROR_THROW("Argument --model is required.");
    }

    if (!osnArgs.includeLabels.empty() && !osnArgs.excludeLabels.empty()) {
        DG_ERROR_THROW("Arguments --include-labels and --exclude-labels may not be specified at the same time.");
    }

    // validate output
    if (osnArgs.outputPath.empty()) {
        DG_ERROR_THROW("Argument --output is required.");
    }

    if(osnArgs.outputFormat  == "shp") {
        if(!osnArgs.layerName.empty()) {
            OSN_LOG(warning) << "Argument --output-layer is ignored for Shapefile output.";
        }
        osnArgs.layerName = path(osnArgs.outputPath).stem().filename().string();
    } else if(osnArgs.layerName.empty()) {
        osnArgs.layerName = "skynetdetects";
    }

    DG_CHECK(osnArgs.pyramidWindowSizes.size() == osnArgs.pyramidStepSizes.size(),
             "Number of arguments in --pyramid-window-sizes and --pyramid-step-sizes must match.");

    if(osnArgs.pyramidWindowSizes.size() && osnArgs.pyramid) {
        OSN_LOG(warning) << "Argument --pyramid is ignored because pyramid levels are specified manually.";
    }

    if(osnArgs.pyramidWindowSizes.size() && osnArgs.stepSize) {
        OSN_LOG(warning) << "Argument --step-size is ignored because pyramid levels are specified manually.";
    }

    // Ask for password, if not specified
    if (requireCredentials && !displayHelp && osnArgs.credentials.find(':') == string::npos) {
        promptForPassword();
    }
}

void CLIArgsParser::promptForPassword()
{
    cout << "Enter your web service password: ";
    auto password = readMaskedInputFromConsole();
    osnArgs.credentials += ":";
    osnArgs.credentials += password;
}


void CLIArgsParser::readArgs(variables_map vm, bool splitArgs) {
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

    osnArgs.bbox = readVariable<cv::Rect2d>("bbox", vm);

    string service;
    if (readVariable("service", vm, service)) {
        osnArgs.source = parseService(service);
        readWebServiceArgs(vm, splitArgs);
    } else if (readVariable("image", vm, osnArgs.image)) {
        osnArgs.source = Source::LOCAL;
    }

    string actionString;
    readVariable("action", vm, actionString);
    osnArgs.action = parseAction(actionString);
    maybeDisplayHelp(vm);

    readWebServiceArgs(vm, splitArgs);
    readProcessingArgs(vm, splitArgs);
    readOutputArgs(vm, splitArgs);
    readFeatureDetectionArgs(vm, splitArgs);
    readLoggingArgs(vm, splitArgs);
}

void CLIArgsParser::maybeDisplayHelp(variables_map vm)
{
    // If "action" is "help", see if there's a topic. Display all help if there isn't
    string topicStr;
    if(osnArgs.action == Action::HELP) {
        if(readVariable("help-topic", vm, topicStr)) {
            osnArgs.action = parseAction(topicStr);
        }
        displayHelp = true;
    } else if(readVariable("help-topic", vm, topicStr)) {
        // If we have "help" listed after the "action", we display help for that action
        DG_CHECK(iequals(topicStr, "help"), "Invalid argument: %s", topicStr.c_str());
        displayHelp = true;
    }
}

void CLIArgsParser::readWebServiceArgs(variables_map vm, bool splitArgs)
{
    mapIdSet |= readVariable("mapId", vm, osnArgs.mapId);
    readVariable("token", vm, osnArgs.token);
    readVariable("credentials", vm, osnArgs.credentials);
    readVariable("url", vm, osnArgs.url);
    osnArgs.useTiles = vm.find("use-tiles") != vm.end();
    readVariable("zoom", vm, osnArgs.zoom);
    readVariable("num-downloads", vm, osnArgs.maxConnections);
}


void CLIArgsParser::readOutputArgs(variables_map vm, bool splitArgs)
{
    readVariable("format", vm, osnArgs.outputFormat);
    to_lower(osnArgs.outputFormat);

    readVariable("output", vm, osnArgs.outputPath);
    readVariable("output-layer", vm, osnArgs.layerName);

    string typeStr = "polygon";
    readVariable("type", vm, typeStr);
    to_lower(typeStr);
    if(typeStr == "polygon") {
        osnArgs.geometryType = GeometryType::POLYGON;
    } else if(typeStr == "point") {
        osnArgs.geometryType = GeometryType::POINT;
    } else {
        DG_ERROR_THROW("Invalid geometry type: %s", typeStr.c_str());
    }

    osnArgs.append = vm.find("append") != end(vm);
    osnArgs.producerInfo = vm.find("producer-info") != end(vm);
}

void CLIArgsParser::readProcessingArgs(variables_map vm, bool splitArgs)
{
    osnArgs.useCpu = vm.find("cpu") != end(vm);
    readVariable("max-utilization", vm, osnArgs.maxUtilization);
    readVariable("model", vm, osnArgs.modelPath);
    osnArgs.windowSize = readVariable<cv::Size>("window-size", vm);
}

void CLIArgsParser::readFeatureDetectionArgs(variables_map vm, bool splitArgs)
{
    readVariable("include-labels", vm, osnArgs.includeLabels, splitArgs);
    readVariable("exclude-labels", vm, osnArgs.excludeLabels, splitArgs);

    confidenceSet |= readVariable("confidence", vm, osnArgs.confidence);

    readVariable("pyramid-window-sizes", vm, osnArgs.pyramidWindowSizes, true);
    readVariable("pyramid-step-sizes", vm, osnArgs.pyramidStepSizes, true);

    osnArgs.stepSize = readVariable<cv::Point>("step-size", vm);
    osnArgs.pyramid = vm.find("pyramid") != end(vm);

    if(vm.find("nms") != end(vm)) {
        osnArgs.nms = true;
        std::vector<float> args;
        readVariable("nms", vm, args);
        if(args.size()) {
            osnArgs.overlap = args[0];
        }
    }
}

void CLIArgsParser::readLoggingArgs(variables_map vm, bool splitArgs)
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