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

#ifndef OPENSPACENET_CLIPROCESSOR_H
#define OPENSPACENET_CLIPROCESSOR_H

#include <OpenSpaceNetArgs.h>
#include <OpenSpaceNet.h>
#include <boost/program_options.hpp>

namespace dg { namespace osn {

class CliProcessor
{
public:
    dg::deepcore::level_t consoleLogLevel = dg::deepcore::level_t::info;
    std::string fileLogPath;
    dg::deepcore::level_t fileLogLevel = dg::deepcore::level_t::debug;

    CliProcessor();
    void setupArgParsing(int argc, const char* const* argv);
    void startOSNProcessing();
    bool showHelp();
    OpenSpaceNetArgs osnArgs;

private:
    bool confidenceSet = false;
    bool mapIdSet = false;
    bool displayHelp = false;
    bool maxConnectionsSet = false;
    bool zoomSet = false;

    void setupInitialLogging();
    void setupLogging();
    void parseArgs(int argc, const char* const* argv);
    void printUsage() const;
    void readArgs(boost::program_options::variables_map vm, bool splitArgs=false);
    void readWebServiceArgs(boost::program_options::variables_map vm, bool splitArgs=false);
    void promptForPassword();
    void readOutputArgs(boost::program_options::variables_map vm, bool splitArgs=false);
    void readProcessingArgs(boost::program_options::variables_map vm, bool splitArgs=false);
    void readFeatureDetectionArgs(boost::program_options::variables_map vm, bool splitArgs=false);
    void readSegmentationArgs(boost::program_options::variables_map vm, bool splitArgs=false);
    void readLoggingArgs(boost::program_options::variables_map vm, bool splitArgs=false);
    void parseFilterArgs(const std::vector<std::string>& filterList);

    void readModelPackage();
    void validateArgs();

    boost::program_options::options_description localOptions_;
    boost::program_options::options_description webOptions_;
    boost::program_options::options_description outputOptions_;
    boost::program_options::options_description processingOptions_;
    boost::program_options::options_description detectOptions_;
    boost::program_options::options_description segmentationOptions_;
    boost::program_options::options_description filterOptions_;
    boost::program_options::options_description loggingOptions_;
    boost::program_options::options_description generalOptions_;

    boost::program_options::options_description allOptions_;
    boost::program_options::options_description visibleOptions_;
    boost::program_options::options_description optionsDescription_;

    std::vector<std::string> supportedFormats_;

    boost::shared_ptr<deepcore::log::sinks::sink> cerrSink_;
    boost::shared_ptr<deepcore::log::sinks::sink> coutSink_;
    boost::shared_ptr<deepcore::ProgressDisplay> pd_;
};

} } // namespace dg { namespace osn {
#endif //OPENSPACENET_CLIPROCESSOR_H
