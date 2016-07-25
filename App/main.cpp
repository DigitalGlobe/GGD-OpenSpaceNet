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

#include <fstream>
#include <boost/program_options.hpp>
#include "OpenSkyNet.h"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <boost/timer/timer.hpp>
#include <DeepCore/utility/Logging.h>
#include <version.h>

using namespace std;
using namespace dg::deepcore;

using boost::algorithm::iequals;
using boost::algorithm::is_any_of;
using boost::algorithm::split;
using boost::algorithm::trim;
using boost::algorithm::to_lower;
using boost::filesystem::path;

void setupLogging(const boost::shared_ptr<log::sinks::sink>& clogSink, const string& logLevel, const string& logFile);

void outputLogo() {
    cout << "DigitalGlobe, Inc.\n";
    cout << "   ___                   ____  _          _   _      _         \n";
    cout << "  / _ \\ _ __   ___ _ __ / ___|| | ___   _| \\ | | ___| |_       \n";
    cout << " | | | | '_ \\ / _ \\ '_ \\\\___ \\| |/ / | | |  \\| |/ _ \\ __|      \n";
    cout << " | |_| | |_) |  __/ | | |___) |   <| |_| | |\\  |  __/ |_ _ _ _ \n";
    cout << "  \\___/| .__/ \\___|_| |_|____/|_|\\_\\\\__, |_| \\_|\\___|\\__(_|_|_)\n";
    cout << "       |_|                          |___/                                     " << endl;
}

int main(int ac, const char* av[]) {
    auto clogSink = log::addClogSink(dg::deepcore::level_t::error, dg::deepcore::level_t::fatal,
                                     dg::deepcore::log::dg_log_format::dg_short_log);

    outputLogo();
    namespace po = boost::program_options;

    // Declare the supported options.
    po::options_description desc(
            "Allowed options\n\n"
                    "Version: " OPENSKYNET_VERSION_STRING "\n\n");
    desc.add_options()
            ("help", "Usage")
            ("image", po::value<string>(), "Local image (filetype .tif) rather than using tile service")
            ("token", po::value<string>(),
             "API token used for licensing. This is the connectId for the WMTS service or the API key for the Web Maps API.")
            ("credentials", po::value<string>(), "Credentials for the map service. Not required for Web Maps API.")
            ("gpu", "Use GPU for processing.")
            ("maxUtilization", po::value<double>(),
             "Maximum GPU utilization %. Default is 95, minimum is 5, and maximum is 100. Not used if processing on CPU")
            ("service", po::value<string>(), "The service that will be the source of tiles. Valid values are dgcs, evwhs, and web_api.  Default is dgcs.")
            ("bbox", po::value<std::vector<double>>()->multitoken(),
             "Bounding box for determining tiles. This must be in longitude-latitude order.")
            ("zoom", po::value<long>(), "Zoom level to request tiles at. Defaults to zoom level 18.")
            ("rowSpan", po::value<long>(), "Number of rows.")
            ("model", po::value<string>(), "Folder location of the trained model.")
            ("type", po::value<string>(), "Output geometry type.  Currently only POINT and POLYGON are valid.")
            ("format", po::value<string>(),
             "Output file format for the results.  Valid values are shp, kml, elasticsearch, postgis, fgdb")
            ("output", po::value<string>(), "Output location with file name and path or URL.")
            ("outputLayerName", po::value<string>(), "The output layer name, index name, or table name.")
            ("confidence", po::value<double>(), "A factor to weed out weak matches for the classification process.")
            ("pyramid",
             "Pyramid the downloaded tile and run detection on all resultant images.\n WARNING: This will result in much longer run times, but may result in additional results from the classification process.")
            ("windowSize", po::value<long>(),
             "Used with stepSize to determine the height and width of the sliding window. \n WARNING: This will result in much longer run times, but may result in additional results from the classification process.")
            ("stepSize", po::value<long>(),
             "Used with windowSize to determine how much the sliding window moves on each step.\n WARNING: This will result in much longer run times, but may result in additional results from the classification process. ")
            ("numConcurrentDownloads", po::value<long>(),
             "Used to speed up downloads by allowing multiple concurrent downloads to happen at once.")
            ("producerInfo", "Add user name, application name, and application version to the output feature set")
            ("log", po::value<string>(),
             "Log level. Permitted values are: trace, debug, info, warning, error, fatal. Default level: error.")
            ("logFile", po::value<string>(), "Name of the file to save the log to. Default logs to console.");

    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc,
                                     po::command_line_style::unix_style ^ po::command_line_style::allow_short), vm);
    po::notify(vm);

    dg::deepcore::log::init();

    if (vm.count("help")) {
        cout << desc << "\n";
        return 0;
    }
    OpenSkyNetArgs args;

    if (vm.count("service")) {
        string service = vm["service"].as<string>();
        boost::to_lower(service);
        if (service == "web_api") {
            args.service = TileSource::MAPS_API;
        } else if (service == "evwhs") {
            args.service = TileSource::EVWHS;
        } else if (service == "dgcs") {
            args.service = TileSource::DGCS;
        }
        else {
            cout << "Invalid service provided. Defaulting to dgcs.\n";
        }

    }
        string logFile;
        if (vm.count("logFile")) {
            logFile = vm["logFile"].as<string>();
        }

        if (vm.count("log")) {
            auto logLevel = vm["log"].as<string>();
            setupLogging(clogSink, logLevel, logFile);
        }


        if (vm.count("image")) {
            args.image = vm["image"].as<string>();
            args.useTileServer = false;
        }

        const string DEFAULT_CONNECTID = "bede7af2-593a-4b9a-81ae-a06fc58b0c5b";
        if (vm.count("token")) {
            args.token = vm["token"].as<string>();
        } else if (args.useTileServer) {
            if (args.service != TileSource::DGCS) {
                cout << "A token must be supplied to use the desired service.\n";
                return 1;
            }
            cout << "token was not set. Using default token.\n";
            args.token = DEFAULT_CONNECTID;
        }

        if (vm.count("credentials")) {
            args.credentials = vm["credentials"].as<string>();
        }

        if (vm.count("bbox")) {
            args.bbox = vm["bbox"].as<std::vector<double>>();
            if (args.bbox.size() != 4) {
                cout << "Invalid  number of parameters for bounding box." << endl;
            }
        } else if(args.useTileServer){
            cerr << "Bounding box not set. Unable to continue.\n";
            exit(1);
        }

        args.useGPU = false;
        if (vm.count("gpu")) {
            args.useGPU = true;
        }

        if (vm.count("maxUtilization")) {
            args.maxUtitilization = (float) vm["maxUtilization"].as<double>() / 100;
        }

        // How is this defaulted?
        //TODO: Get with Andrew about default models.
        if (vm.count("model")) {
            args.modelPath = vm["model"].as<string>();
        } else {
            cout << "Model was not set. Unable to continue.\n";
            return 1;
        }

        if (vm.count("type")) {
            args.geometryType = vm["type"].as<string>();
        }

        if (vm.count("format")) {
            args.outputFormat = vm["format"].as<string>();
        } else {
            cout << "Output format was not set. Forcing to shp.\n";
            args.outputFormat = "shp";
        }

        if (vm.count("output")) {
            args.outputPath = vm["output"].as<string>();
        }

        if(iequals(args.outputFormat, "shp")) {
            if (vm.count("outputLayerName")) {
                DG_LOG(OpenSkyNet, warning) << "outputLayerName argument is ignored for Shapefile output.";
            }

            args.layerName = path(args.outputPath).stem().filename().string();
        } else {
            if (vm.count("outputLayerName")) {
                args.layerName = vm["outputLayerName"].as<string>();
            }
            else {
                args.layerName = "skynetdetects";
            }
        }

        if (vm.count("credentials")) {
            args.credentials = vm["credentials"].as<string>();
        } else {
            if (args.service == TileSource::EVWHS) {
                cout << "You must supply credentials to use the requested tile service.\n";
                return 1;
            }

        }

        args.multiPass = vm.count("pyramid");

        args.confidence = 0.0;
        if (vm.count("confidence")) {
            args.confidence = vm["confidence"].as<double>();
        }

        if (vm.count("zoom")) {
            args.zoom = vm["zoom"].as<long>();
        }

        args.stepSize = 0;
        args.windowSize = 0;
        if (vm.count("stepSize")) {
            args.stepSize = vm["stepSize"].as<long>();
        }

        if (vm.count("windowSize")) {
            args.windowSize = vm["windowSize"].as<long>();
        }

        if (vm.count("numConcurrentDownloads")) {
            args.maxConnections = vm["numConcurrentDownloads"].as<long>();
        }

        if(vm.count("producerInfo")) {
            args.producerInfo = true;
        }

        boost::timer::auto_cpu_timer t;

        try {
            OpenSkyNet osn(args);
            osn.process();
        } catch (const Error &e) {
            cerr << e.message() << endl;
            exit(1);
        } catch (const std::exception &e) {
            cerr << e.what() << endl;
            exit(1);
        } catch (...) {
            cerr << "Unknown error" << endl;
        }
    }

void setupLogging(const boost::shared_ptr<log::sinks::sink> &clogSink, const string &logLevel, const string &logFile) {
    // Split the log level into level:channel and normalize
    std::vector<string> parts;
    split(parts, logLevel, is_any_of(":"));
    for(auto& part : parts) {
        trim(part);
        to_lower(part);
    }

    if(parts.empty()) {
        DG_LOG(OpenSkyNet, error) << "Log level must be specified";
        exit(1);
    }

    auto level = log::stringToLevel(parts[0]);
    boost::shared_ptr<log::sinks::sink> sink;
    if(logFile.empty()) {
        if(level != level_t::error) {
            log::removeSink(clogSink);
            sink = log::addClogSink(level, level_t::fatal, log::dg_short_log);
        } else {
            sink = clogSink;
        }
    } else {
        auto ofs = boost::make_shared<ofstream>(logFile);
        if(ofs->fail()) {
            DG_LOG(OpenSkyNet, error) << "Error opening log file " << logFile << " for writing.";
        }
        sink = log::addStreamSink(ofs, level, level_t::fatal, log::dg_long_log);
    }

    if(parts.size() > 1) {
        // TODO: Setup channel filter
    }
}
