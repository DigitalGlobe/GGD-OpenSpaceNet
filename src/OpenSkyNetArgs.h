/********************************************************************************
* Copyright 2015 DigitalGlobe, Inc.
* Author: Joe White
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

#ifndef OPENSKYNET_OPENSKYNETARGS_H
#define OPENSKYNET_OPENSKYNETARGS_H

#include <boost/program_options.hpp>
#include <utility/Logging.h>
#include <vector/Feature.h>

#define OSN_LOG(sev) DG_LOG(OpenSkyNet, sev)
#define MAPSAPI_MAPID  "digitalglobe.nal0g75k"


namespace dg { namespace osn {

enum class Source
{
    UNKNOWN,
    LOCAL,
    DGCS,
    EVWHS,
    MAPS_API
};

enum class Action
{
    UNKNOWN,
    HELP,
    DETECT,
    LANDCOVER
};

class OpenSkyNetArgs
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

    // Output options
    deepcore::vector::GeometryType geometryType = deepcore::vector::GeometryType::POLYGON;
    std::string outputFormat = "shp";
    std::string outputPath;
    std::string layerName;
    bool producerInfo = false;

    // Processing options
    bool useCpu = false;
    float maxUtilization = 95;
    std::string modelPath;
    std::unique_ptr<cv::Size> windowSize;

    // Feature detection options
    float confidence = 95;
    std::unique_ptr<cv::Point> stepSize;
    bool pyramid = false;
    bool nms = false;
    float overlap = 30;
    std::vector<std::string> includeLabels;
    std::vector<std::string> excludeLabels;
    std::vector<int> pyramidWindowSizes;
    std::vector<int> pyramidStepSizes;

    // Logging options
    bool quiet = false;
    dg::deepcore::level_t consoleLogLevel = dg::deepcore::level_t::info;
    std::string fileLogPath;
    dg::deepcore::level_t fileLogLevel = dg::deepcore::level_t::debug;

    OpenSkyNetArgs();
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

#endif //OPENSKYNET_OPENSKYNETARGS_H
