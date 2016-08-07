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
#include <opencv2/core/types.hpp>
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
    deepcore::vector::GeometryType geometryType = deepcore::vector::GeometryType::UNKNOWN;
    std::string outputFormat;
    std::string outputPath;
    std::string layerName;
    bool producerInfo = false;

    // Processing options
    bool useCpu = false;
    float maxUtitilization = 95;
    std::string modelPath;
    std::unique_ptr<cv::Size> windowSize;

    // Feature detection options
    float confidence = 95;
    std::unique_ptr<cv::Point> stepSize;
    bool pyramid = false;
    bool nms = false;
    float overlap = 30;

    OpenSkyNetArgs();
    void parseArgsAndProcess(int argc, const char* const* argv);

private:
    void setupConsoleLogging();
    template<class T>
    bool readOptional(const char* param, T& ret);
    template<class T>
    T readRequired(const char* param, const char* errorMsg = nullptr, bool showUsage=false);
    template<class T>
    bool readCoord(const char* param, T& x, T& y);
    void parseArgs(int argc, const char* const* argv);
    Action parseAction(std::string str) const;
    Source parseService(std::string service) const;
    bool maybeDisplayHelp();
    void printUsage(Action action=Action::UNKNOWN) const;
    void readArgs();
    void readWebServiceArgs();
    void promptForPassword();
    void readOutputArgs();
    void readProcessingArgs();
    void readFeatureDetectionArgs();
    void readBoundingBoxArgs();
    void readLoggingArgs();

    boost::program_options::options_description localOptions_;
    boost::program_options::options_description webOptions_;
    boost::program_options::options_description outputOptions_;
    boost::program_options::options_description processingOptions_;
    boost::program_options::options_description detectOptions_;
    boost::program_options::options_description loggingOptions_;
    boost::program_options::options_description generalOptions_;

    boost::program_options::options_description visibleOptions_;
    boost::program_options::options_description optionsDescription_;

    std::vector<std::string> supportedFormats_;

    boost::program_options::variables_map vm_;

    boost::shared_ptr<deepcore::log::sinks::sink> cerrSink_;
    boost::shared_ptr<deepcore::log::sinks::sink> coutSink_;
};

} } // namespace dg { namespace osn {

#endif //OPENSKYNET_OPENSKYNETARGS_H
