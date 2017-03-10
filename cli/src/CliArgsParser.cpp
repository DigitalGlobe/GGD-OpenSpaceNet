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
#include "CliArgsParser.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
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
using boost::filesystem::exists;
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
using geometry::GeometryType;
using vector::FeatureSet;

static const string OSN_USAGE =
    "Usage:\n"
        "  OpenSpaceNet <action> <input options> <output options> <processing options>\n"
        "  OpenSpaceNet --config <configuration file> [other options]\n\n"
        "Actions:\n"
        "  help                                  Show this help message\n"
        "  detect                                Perform feature detection\n";

static const string OSN_DETECT_USAGE =
    "Run OpenSpaceNet in feature detection mode.\n\n"
    "Usage:\n"
        "  OpenSpaceNet detect <input options> <output options> <processing options>\n\n";

static const string OSN_LANDCOVER_USAGE =
    "Run OpenSpaceNet in landcover classification mode.\n\n"
        "Usage:\n"
        "  OpenSpaceNet landcover <input options> <output options> <processing options>\n\n";

CliArgsParser::CliArgsParser() :
    localOptions_("Local Image Input Options"),
    webOptions_("Web Service Input Options"),
    outputOptions_("Output Options"),
    processingOptions_("Processing Options"),
    detectOptions_("Feature Detection Options"),
    filterOptions_("Filtering Options"),
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
        ("map-id", po::value<string>()->value_name(name_with_default("MAPID", osnArgs.mapId)), "MapsAPI map id to use.")
        ("max-connections", po::value<int>()->value_name(name_with_default("NUM", osnArgs.maxConnections)),
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
        ("output-layer", po::value<string>()->value_name(name_with_default("NAME", "osndetects")),
         "The output layer name, index name, or table name.")
        ("type", po::value<string>()->value_name(name_with_default("TYPE", "polygon")),
         "Output geometry type.  Currently only point and polygon are valid.")
        ("producer-info", "Add user name, application name, and application version to the output feature set.")
        ("append", "Append to an existing vector set. If the output does not exist, it will be created.")
        ;

    processingOptions_.add_options()
        ("cpu", "Use the CPU for processing, the default is to use the GPU.")
        ("max-utilization", po::value<float>()->value_name(name_with_default("PERCENT", osnArgs.maxUtilization)),
         "Maximum GPU utilization %. Minimum is 5, and maximum is 100. Not used if processing on CPU")
        ("model", po::value<string>()->value_name("PATH"), "Path to the the trained model.")
        ("window-size", po::value<std::vector<int>>()->multitoken()->value_name("SIZE [SIZE...]"),
         "Sliding window detection box sizes.  The source image is chipped with boxes of the given sizes.  "
         "If resampled-size is not specified, all windows must fit within the model."
         "Default is the model size.")
        ("window-step", po::value<std::vector<int>>()->multitoken()->value_name("STEP [STEP...]"),
         "Sliding window step.  Either a single step or a step for each window size may be given.  Default "
         "is 20% of the model size.")
        ("resampled-size", po::value<int>()->value_name("SIZE"),
         "Resample window chips to a fixed size.  This must fit within the model.")
        ("pyramid",
         "Calculate window parameters.  If this is set, only the first window size "
         "and window step are used.  A family of each are created by doubling the supplied parameters up to "
         "the area of the detection box.")
        ;

    detectOptions_.add_options()
        ("confidence", po::value<float>()->value_name(name_with_default("PERCENT", osnArgs.confidence)),
         "Minimum percent score for results to be included in the output.")
        ("nms", po::bounded_value<std::vector<float>>()->min_tokens(0)->max_tokens(1)->value_name(name_with_default("PERCENT", osnArgs.overlap)),
         "Perform non-maximum suppression on the output. You can optionally specify the overlap threshold percentage "
         "for non-maximum suppression calculation.")
        ;

    filterOptions_.add_options()
        ("include-labels", po::value<std::vector<string>>()->multitoken()->value_name("LABEL [LABEL...]"),
         "Filter results to only include specified labels.")
        ("exclude-labels", po::value<std::vector<string>>()->multitoken()->value_name("LABEL [LABEL...]"),
         "Filter results to exclude specified labels.")
        ("include-region", po::value<string>()->value_name("PATH [PATH...]"), "Path to a file prescribing regions to include when filtering.")
        ("exclude-region", po::value<string>()->value_name("PATH [PATH...]"), "Path to a file prescribing regions to exclude when filtering.")
        ("region", po::value<std::vector<string>>()->multitoken()->value_name("(include/exclude) PATH [PATH...] [(include/exclude) PATH [PATH...]...]"),
         "Paths to files including and excluding regions.")
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
    optionsDescription_.add(filterOptions_);
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
    visibleOptions_.add(filterOptions_);
    visibleOptions_.add(loggingOptions_);
    visibleOptions_.add(generalOptions_);
}

void CliArgsParser::setupArgParsing(int argc, const char* const* argv)
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


}

void CliArgsParser::startOSNProcessing()
{
    OpenSpaceNet osn(osnArgs);
    osn.process();
}

void CliArgsParser::setupInitialLogging()
{
    log::init();

    cerrSink_ = log::addCerrSink(dg::deepcore::level_t::warning, dg::deepcore::level_t::fatal,
                                 dg::deepcore::log::dg_log_format::dg_short_log);

    coutSink_ = log::addCoutSink(dg::deepcore::level_t::info, dg::deepcore::level_t::info,
                                 dg::deepcore::log::dg_log_format::dg_short_log);
}

void CliArgsParser::setupLogging() {
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
        DG_CHECK(!ofs->fail(), "Error opening log file %s for writing", fileLogPath.c_str());
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
void CliArgsParser::parseArgs(int argc, const char* const* argv)
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
                  .extra_style_parser(po::postfix_argument("region"))
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

void CliArgsParser::printUsage(Action action) const
{
    switch(action) {
        case Action::LANDCOVER:
        {
            po::options_description desc;
            desc.add(localOptions_);
            desc.add(webOptions_);
            desc.add(outputOptions_);
            desc.add(processingOptions_);
            desc.add(filterOptions_);
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

enum ArgUse {
    IGNORED = 1,
    MAY_USE_ONE = 2,
    OPTIONAL = 3,
    REQUIRED = 4
};

inline void checkArgument(const char* argumentName, ArgUse expectedUse, bool set, const char* cause = "processing")
{
    if (expectedUse == REQUIRED && !set) {
        DG_ERROR_THROW("Argument --%s is required when %s", argumentName, cause);
    } else if (expectedUse == IGNORED && set) {
        OSN_LOG(warning) << "Argument --" << argumentName << " is ignored when " << cause;
    }
}

inline void checkArgument(const char* argumentName, ArgUse expectedUse, const std::string& set, const char* cause = "processing")
{
    checkArgument(argumentName, expectedUse, !set.empty(), cause);
}

template<class T>
inline void checkArgument(const char* argumentName, ArgUse expectedUse, const std::vector<T>& set, const char* cause = "processing")
{
    if (expectedUse == REQUIRED && set.empty()) {
        DG_ERROR_THROW("Argument --%s is required for %s", argumentName, cause);
    } else if (expectedUse == MAY_USE_ONE && set.size() > 1) {
        OSN_LOG(warning) << "Argument --" << argumentName << " has ignored additional parameters when " << cause;
    } else if (expectedUse == IGNORED && !set.empty()) {
        OSN_LOG(warning) << "Argument --" << argumentName << " is ignored when " << cause;
    }
}

void CliArgsParser::validateArgs()
{
    if (osnArgs.action == Action::HELP) {
        return;
    }

    //
    // Validate action args.
    //
    ArgUse windowStepUse(OPTIONAL);
    ArgUse windowSizeUse(OPTIONAL);
    ArgUse nmsUse(OPTIONAL);
    ArgUse pyramidUse(OPTIONAL);
    ArgUse confidenceUse(OPTIONAL);
    const char * actionDescription;

    switch (osnArgs.action) {
        case Action::DETECT:
            actionDescription = "the action is detect";
            break;

        case Action::LANDCOVER:
            windowStepUse = IGNORED;
            windowSizeUse = MAY_USE_ONE;
            nmsUse = IGNORED;
            pyramidUse = IGNORED;
            confidenceUse = IGNORED;
            actionDescription = "the action is landcover";
            break;

        default:
            DG_ERROR_THROW("Invalid action");
    }

    checkArgument("window-step", windowStepUse, osnArgs.windowStep, actionDescription);
    checkArgument("window-size", windowSizeUse, osnArgs.windowSize, actionDescription);
    checkArgument("nms", nmsUse, osnArgs.nms, actionDescription);
    checkArgument("pyramid", pyramidUse, osnArgs.pyramid, actionDescription);
    checkArgument("confidence", confidenceUse, confidenceSet, actionDescription);


    //
    // Validate source args.
    //
    ArgUse tokenUse(IGNORED);
    ArgUse credentialsUse(IGNORED);
    ArgUse mapIdUse(IGNORED);
    ArgUse zoomUse(IGNORED);
    ArgUse bboxUse(IGNORED);
    ArgUse maxConnectionsUse(IGNORED);
    ArgUse urlUse(IGNORED);
    ArgUse useTilesUse(IGNORED);
    const char * sourceDescription;

    switch (osnArgs.source) {
        case Source::LOCAL:
            bboxUse = OPTIONAL;
            sourceDescription = "using a local image";
            break;

        case Source::MAPS_API:
            tokenUse = REQUIRED;
            mapIdUse = OPTIONAL;
            zoomUse = OPTIONAL;
            bboxUse = REQUIRED;
            maxConnectionsUse = OPTIONAL;
            sourceDescription = "using maps-api";
            break;

        case Source::DGCS:
        case Source::EVWHS:
            tokenUse = REQUIRED;
            credentialsUse = REQUIRED;
            mapIdUse = OPTIONAL;
            zoomUse = OPTIONAL;
            bboxUse = REQUIRED;
            maxConnectionsUse = OPTIONAL;
            sourceDescription = "using dgcs or evwhs";
            break;

        case Source::TILE_JSON:
            credentialsUse = OPTIONAL;
            zoomUse = OPTIONAL;
            bboxUse = REQUIRED;
            maxConnectionsUse = OPTIONAL;
            urlUse = REQUIRED;
            useTilesUse = OPTIONAL;
            sourceDescription = "using tile-json";
            break;

        default:
            DG_ERROR_THROW("Source is unknown or unspecified");
    }

    checkArgument("token", tokenUse, osnArgs.token, sourceDescription);
    checkArgument("credentials", credentialsUse, osnArgs.credentials, sourceDescription);
    checkArgument("map-id", mapIdUse, mapIdSet, sourceDescription);
    checkArgument("zoom", zoomUse, zoomSet, sourceDescription);
    checkArgument("bbox", bboxUse, (bool) osnArgs.bbox, sourceDescription);
    checkArgument("max-connections", maxConnectionsUse, maxConnectionsSet, sourceDescription);
    checkArgument("url", urlUse, osnArgs.url, sourceDescription);
    checkArgument("use-tiles", useTilesUse, osnArgs.useTiles, sourceDescription);


    //
    // Validate model and detection
    //
    checkArgument("model", REQUIRED, osnArgs.modelPath);
    DG_CHECK(osnArgs.includeLabels.empty() || osnArgs.excludeLabels.empty(),
             "Arguments --include-labels and --exclude-labels may not be specified at the same time");

    if (pyramidUse > IGNORED && osnArgs.pyramid) {
        checkArgument("window-size", MAY_USE_ONE, osnArgs.windowSize, "--pyramid is specified");
        checkArgument("window-step", MAY_USE_ONE, osnArgs.windowStep, "--pyramid is specified");
    } else if(windowSizeUse > MAY_USE_ONE || windowStepUse > MAY_USE_ONE) {
        DG_CHECK(osnArgs.windowSize.size() < 2 || osnArgs.windowStep.size() < 2 ||
                 osnArgs.windowSize.size() == osnArgs.windowStep.size(),
                 "Arguments --window-size and --window-step must match in length");
    }


    //
    // Validate output
    //
    checkArgument("output", REQUIRED, osnArgs.outputPath);
    if(osnArgs.outputFormat  == "shp") {
        checkArgument("output-layer", IGNORED, osnArgs.layerName, "the output format is a shapefile");
        osnArgs.layerName = path(osnArgs.outputPath).stem().filename().string();
    } else if(osnArgs.layerName.empty()) {
        osnArgs.layerName = "osndetects";
    }


    //
    // Validate filtering
    //
    if (osnArgs.filterDefinition.size()) {
        for (const auto& action : osnArgs.filterDefinition) {
            for (const auto& file : action.second) {
                path filePath = path(file);
                if (!exists(filePath)) {
                    DG_ERROR_THROW("Argument to %s region using file \"%s\" invalid, file does not exist",
                                   action.first.c_str(), file.c_str());
                } else {
                    string fileExtension = filePath.extension().string();
                    fileExtension.erase(0,1);
                    if (find(supportedFormats_.begin(), supportedFormats_.end(), fileExtension) == supportedFormats_.end()) {
                        DG_ERROR_THROW("Argument to %s region using file \"%s\" invalid, format \"%s\" is unsupported",
                                       action.first.c_str(), file.c_str(), fileExtension.c_str());
                    }
                }
            }
        }

        if (pyramidUse > IGNORED && osnArgs.pyramid) {
            checkArgument("window-size", MAY_USE_ONE, osnArgs.windowSize, "creating the spatial filter");
        }
    }

    // Ask for password, if not specified
    if (credentialsUse > IGNORED && !displayHelp && !osnArgs.credentials.empty() && osnArgs.credentials.find(':') == string::npos) {
        promptForPassword();
    }
}

void CliArgsParser::promptForPassword()
{
    cout << "Enter your web service password: ";
    auto password = readMaskedInputFromConsole();
    osnArgs.credentials += ":";
    osnArgs.credentials += password;
}


void CliArgsParser::readArgs(variables_map vm, bool splitArgs) {
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
    bool serviceSet  = readVariable("service", vm, service);
    if (serviceSet) {
        osnArgs.source = parseService(service);
        readWebServiceArgs(vm, splitArgs);
    }

    bool imageSet = readVariable("image", vm, osnArgs.image);
    if (imageSet) {
        osnArgs.source = Source::LOCAL;
    }

    DG_CHECK(!imageSet || !serviceSet, "Arguments --image and --service may not be specified at the same time");


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

void CliArgsParser::maybeDisplayHelp(variables_map vm)
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

void CliArgsParser::readWebServiceArgs(variables_map vm, bool splitArgs)
{
    mapIdSet |= readVariable("map-id", vm, osnArgs.mapId);
    readVariable("token", vm, osnArgs.token);
    readVariable("credentials", vm, osnArgs.credentials);
    readVariable("url", vm, osnArgs.url);
    osnArgs.useTiles = vm.find("use-tiles") != vm.end();
    zoomSet |= readVariable("zoom", vm, osnArgs.zoom);
    maxConnectionsSet |= readVariable("max-connections", vm, osnArgs.maxConnections);
}


void CliArgsParser::readOutputArgs(variables_map vm, bool splitArgs)
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

void CliArgsParser::readProcessingArgs(variables_map vm, bool splitArgs)
{
    osnArgs.useCpu = vm.find("cpu") != end(vm);
    readVariable("max-utilization", vm, osnArgs.maxUtilization);
    readVariable("model", vm, osnArgs.modelPath);

    readVariable("window-size", vm, osnArgs.windowSize, splitArgs);
    readVariable("window-step", vm, osnArgs.windowStep, splitArgs);
    osnArgs.resampledSize = readVariable<int>("resampled-size", vm);
    osnArgs.pyramid = vm.find("pyramid") != end(vm);

    readVariable("include-labels", vm, osnArgs.includeLabels, splitArgs);
    readVariable("exclude-labels", vm, osnArgs.excludeLabels, splitArgs);
    if (vm.find("region") != end(vm)) {
        parseFilterArgs(vm["region"].as<std::vector<std::string>>());
    }
}

void CliArgsParser::readFeatureDetectionArgs(variables_map vm, bool splitArgs)
{
    confidenceSet |= readVariable("confidence", vm, osnArgs.confidence);


    if(vm.find("nms") != end(vm)) {
        osnArgs.nms = true;
        std::vector<float> args;
        readVariable("nms", vm, args);
        if(args.size()) {
            osnArgs.overlap = args[0];
        }
    }
}

void CliArgsParser::readLoggingArgs(variables_map vm, bool splitArgs)
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

void CliArgsParser::parseFilterArgs(const std::vector<string>& filterList)
{
    string filterAction = "";
    string finalEntry = "";
    std::vector<string> filterActionFileSet;
    for (auto entry : filterList) {
        if (entry == filterAction) {
            finalEntry = entry;
            continue;
        }
        else if (entry == "include" ||
                 entry == "exclude") {
            if (filterAction != "") {
                if (filterActionFileSet.empty()) {
                    DG_ERROR_THROW("Argument to %s region without file input", filterAction.c_str());
                }
                osnArgs.filterDefinition.push_back(std::make_pair(filterAction, move(filterActionFileSet)));
            }

            filterActionFileSet.clear();
            filterAction = entry.c_str();
        } else {
            filterActionFileSet.push_back(entry);
        }

        finalEntry = entry;
    }
    if (finalEntry == "include" || finalEntry == "exclude") {
        DG_ERROR_THROW("Argument to %s region without file input", finalEntry.c_str());
    }
    if (!filterActionFileSet.empty()) {
        osnArgs.filterDefinition.push_back(std::make_pair(filterAction, move(filterActionFileSet)));
    }
}

} } // namespace dg { namespace osn {