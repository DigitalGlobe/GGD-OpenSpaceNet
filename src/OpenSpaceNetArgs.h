/********************************************************************************
* Copyright 2017 DigitalGlobe, Inc.
* Author: Joe White
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

#ifndef OPENSPACENET_OPENSPACENETARGS_H
#define OPENSPACENET_OPENSPACENETARGS_H

#include <boost/program_options.hpp>
#include <utility/Logging.h>
#include <vector/Feature.h>

#define OSN_LOG(sev) DG_LOG(OpenSpaceNet, sev)
#define MAPSAPI_MAPID  "digitalglobe.nal0g75k"


namespace dg { namespace osn {

enum class Source
{
    UNKNOWN,
    LOCAL,
    DGCS,
    EVWHS,
    MAPS_API,
    TILE_JSON
};

enum class Action
{
    UNKNOWN,
    HELP,
    DETECT,
    LANDCOVER
};

class OpenSpaceNetArgs
{
public:
    // Input options
    Action action = Action::UNKNOWN;
    Source source = Source::UNKNOWN;

    std::string image;
    std::unique_ptr<cv::Rect2d> bbox;

    // Web service input options
    std::string token;
    std::string credentials;
    int zoom = 18;
    int maxConnections = 10;
    std::string mapId = MAPSAPI_MAPID;
    std::string url;
    bool useTiles=false;


    // Output options
    deepcore::geometry::GeometryType geometryType = deepcore::geometry::GeometryType::POLYGON;
    std::string outputFormat = "shp";
    std::string outputPath;
    std::string layerName;
    bool producerInfo = false;
    bool append = false;

    // Processing options
    bool useCpu = false;
    float maxUtilization = 95;
    std::string modelPath;
    std::vector<int> windowSizes;
    std::vector<int> windowSteps;
    std::unique_ptr<int> resampledSize;
    bool pyramid = false;

    // Feature detection options
    float confidence = 95;
    bool nms = false;
    float overlap = 30;
    std::vector<std::string> includeLabels;
    std::vector<std::string> excludeLabels;
    std::vector<std::pair<std::string, std::vector<std::string>>> filterDefinition;

    // Logging options
    bool quiet = false;
    dg::deepcore::level_t consoleLogLevel = dg::deepcore::level_t::info;
    std::string fileLogPath;
    dg::deepcore::level_t fileLogLevel = dg::deepcore::level_t::debug;

    OpenSpaceNetArgs();
    void parseArgsAndProcess(int argc, const char* const* argv);

private:
    bool confidenceSet = false;
    bool mapIdSet = false;
    bool displayHelp = false;

    void setupInitialLogging();
    void setupLogging();
    void parseArgs(int argc, const char* const* argv);
    void maybeDisplayHelp(boost::program_options::variables_map vm);
    void printUsage(Action action=Action::UNKNOWN) const;
    void readArgs(boost::program_options::variables_map vm, bool splitArgs=false);
    void readWebServiceArgs(boost::program_options::variables_map vm, bool splitArgs=false);
    void promptForPassword();
    void readOutputArgs(boost::program_options::variables_map vm, bool splitArgs=false);
    void readProcessingArgs(boost::program_options::variables_map vm, bool splitArgs=false);
    void readFeatureDetectionArgs(boost::program_options::variables_map vm, bool splitArgs=false);
    void readLoggingArgs(boost::program_options::variables_map vm, bool splitArgs=false);
    void parseFilterArgs(const std::vector<std::string>& filterList);

    void validateArgs();

    boost::program_options::options_description localOptions_;
    boost::program_options::options_description webOptions_;
    boost::program_options::options_description outputOptions_;
    boost::program_options::options_description processingOptions_;
    boost::program_options::options_description detectOptions_;
    boost::program_options::options_description loggingOptions_;
    boost::program_options::options_description generalOptions_;

    boost::program_options::options_description allOptions_;
    boost::program_options::options_description visibleOptions_;
    boost::program_options::options_description optionsDescription_;

    std::vector<std::string> supportedFormats_;

    boost::shared_ptr<deepcore::log::sinks::sink> cerrSink_;
    boost::shared_ptr<deepcore::log::sinks::sink> coutSink_;
};

} } // namespace dg { namespace osn {

#endif //OPENSPACENET_OPENSPACENETARGS_H
