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

#ifndef OPENSPACENET_PARSECLIARGS_H
#define OPENSPACENET_PARSECLIARGS_H

#include <OpenSpaceNetArgs.h>
#include <OpenSpaceNet.h>
#include <boost/program_options.hpp>

namespace dg { namespace osn {

class ParseCLIArgs
{
public:
    dg::deepcore::level_t consoleLogLevel = dg::deepcore::level_t::info;
    std::string fileLogPath;
    dg::deepcore::level_t fileLogLevel = dg::deepcore::level_t::debug;

    ParseCLIArgs();

    void parseArgsAndProcess(int argc, const char *const *argv);

private:
    OpenSpaceNetArgs osnArgs;
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
#endif //OPENSPACENET_PARSECLIARGS_H
