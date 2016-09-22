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


#ifndef OPENSKYNET_PARSECLIARGS_H
#define OPENSKYNET_PARSECLIARGS_H

#include "OpenSkyNetArgs.h"
#include "OpenSkyNet.h"
#include <boost/program_options.hpp>

namespace dg { namespace osn {

class ParseCLIArgs
{
public:
    ParseCLIArgs(const OpenSkyNetArgs& args);
    ParseCLIArgs();
    void parseArgsAndProcess(int argc, const char* const* argv);
    dg::deepcore::level_t consoleLogLevel = dg::deepcore::level_t::info;
    std::string fileLogPath;
    dg::deepcore::level_t fileLogLevel = dg::deepcore::level_t::debug;

private:
    OpenSkyNetArgs osnArgs;
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

    boost::program_options::variables_map vm_;

    boost::shared_ptr<deepcore::log::sinks::sink> cerrSink_;
    boost::shared_ptr<deepcore::log::sinks::sink> coutSink_;

};

} } // namespace dg { namespace osn {

#endif //OPENSKYNET_PARSECLIARGS_H
