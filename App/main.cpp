#include <iostream>
#include <boost/program_options.hpp>
#include "include/libopenskynet.h"
#include <boost/timer/timer.hpp>
#include <glog/logging.h>
#include <server_http.hpp>
#include <OsnServer.h>

using namespace std;

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

    outputLogo();
    namespace po = boost::program_options;
    string allowedOutputs[] = {"shp", "kml", "elasticsearch", "postgis", "fgdb"};
    // Declare the supported options.
    po::options_description general("General options");
    general.add_options()
            ("token", po::value<string>(), "API token used for licensing. This is the connectId for the WMTS service or the API key for the Web Maps API.")
            ("credentials", po::value<string>(), "Credentials for the map service. Not required for Web Maps API.")
            ("gpu", "Use GPU for processing.")
            ("api", "Use DigitalGlobe Web API as the source of tiles")
            ("model", po::value<string>(), "A trained classifier packaged in a gbdxm file.")
            ;

    po::options_description immediate("Immediate processing options");
    immediate.add_options()
            ("bbox", po::value<vector<double>>()->multitoken(), "Bounding box for determining tiles. This must be in longitude-latitude order.")
            ("startCol", po::value<long>(), "Starting tile column.")
            ("startRow", po::value<long>(), "Starting tile row.")
            ("columnSpan", po::value<long>(), "Number of columns.")
            ("zoom", po::value<long>(), "Zoom level to request tiles at. Defaults to zoom level 18.")
            ("rowSpan", po::value<long>(),"Number of rows.")
            ("type", po::value<string>(), "Output geometry type.  Currently only POINT and POLYGON are valid.")
            ("format", po::value<string>(), "Output file format for the results.  Valid values are shp, kml, elasticsearch, postgis, fgdb")
            ("output", po::value<string>(), "Output location with file name and path or URL.")
            ("outputLayerName", po::value<string>(), "The output layer name, index name, or table name.")
            ("confidence", po::value<double>(), "A factor to weed out weak matches for the classification process.")
            ("pyramid", "Pyramid the downloaded tile and run detection on all resultant images.\n WARNING: This will result in much longer run times, but may result in additional results from the classification process.")
            ("windowSize", po::value<long>(), "Used with stepSize to determine the height and width of the sliding window. \n WARNING: This will result in much longer run times, but may result in additional results from the classification process.")
            ("stepSize", po::value<long>(), "Used with windowSize to determine how much the sliding window moves on each step.\n WARNING: This will result in much longer run times, but may result in additional results from the classification process. ")
            ;

    po::options_description server("Server options");
    server.add_options()
            ("port", po::value<unsigned short>(), "the port on which to server")
            ("threads", po::value<size_t>(), "the number of threads to create to serve clients")
            ;

    po::options_description desc("Allowed options");
    desc.add(general).add(immediate).add(server).add_options()
            ("help", "Usage");

    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc, po::command_line_style::unix_style ^ po::command_line_style::allow_short), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << desc << "\n";
        return 1;
    }



    OpenSkyNetArgs args;

    if (vm.count("api")) {
        args.webApi = true;
    }

    const string DEFAULT_CONNECTID = "bede7af2-593a-4b9a-81ae-a06fc58b0c5b";
    if (vm.count("token")) {
        args.token = vm["token"].as<string>();
        cout << "token was set to "
        << args.token << ".\n";

    } else {
        if (args.webApi){
            cout << "A token must be supplied to use the Web Maps API.\n";
            return 1;
        }
        cout << "token was not set. Using default token.\n";
        args.token = DEFAULT_CONNECTID;
    }

    args.useGPU = false;
    if (vm.count("gpu")){
        cout << "Processing with GPU.\n";
        args.useGPU = true;
    } else {
        cout << "Processing with main CPU.\n";
    }

    // How is this defaulted?
    //TODO: Get with Andrew about default models.
    if (vm.count("model")) {
        args.modelPath =  vm["model"].as<string>();
        cout << "Model location is "
        << args.modelPath << ".\n";
    } else {
        cout << "Model was not set. Unable to continue.\n";
        return 1;
    }

    if (vm.count("type")) {
        args.geometryType = vm["type"].as<string>();
        cout << "type was set to "
        << args.geometryType << ".\n";
    } else {
        cout << "Output geometry type was not set. Forcing to point.\n";
    }

    if (vm.count("format")) {
        args.outputFormat = vm["format"].as<string>();
        cout << "format was set to "
        << args.outputFormat << ".\n";
    } else {
        cout << "Output format was not set. Forcing to shp.\n";
        args.outputFormat = "shp";
    }

    //TODO: make this smarter wrt format.  If the output format is ES, then the default doesn't work.
    if (vm.count("output")) {
        args.outputPath = vm["output"].as<string>();
        cout << "output was set to "
        << args.outputPath << ".\n";
    } else {
        cout << "Output path was not set. Output to current working directory.\n";
    }

    if (vm.count("outputLayerName")){
        args.layerName = vm["outputLayerName"].as<string>();
        cout << "output layer name set to " << args.layerName << ".\n";
    }
    else {
        cout << "Default layer name of skynetdetects will be used.\n";
        args.layerName = "skynetdetects";
    }


    if (vm.count("bbox")) {
        char buffer[100];
        args.bbox = vm["bbox"].as<vector<double>>();
        if (args.bbox.size() != 4) {
            cout << "Invalid  number of parameters for bounding box.\n" << endl;
        } else {
            sprintf(buffer, "LL: %.16g, %.16g, UR: %.16g. %.16g", args.bbox[0], args.bbox[1], args.bbox[2], args.bbox[3]);
            cout << buffer << "\n";
        }
    }
    else {
        if (vm.count("rowSpan") && vm.count("columnSpan"))
        {
            cout << "Specifying tiles instead of bounding rectangle." << "\n";
            if (vm.count("startRow")){
                args.startRow = vm["startRow"].as<long>();
            }
            if (vm.count("startCol")){
                args.startColumn = vm["startCol"].as<long>();
            }
            if (vm.count("rowSpan")){
                args.rowSpan = vm["rowSpan"].as<long>();
            }
            if (vm.count("columnSpan")){
                args.columnSpan = vm["columnSpan"].as<long>();
            }
        }
        else if (!vm.count("port")){
            cout << "Bounding box not set. Unable to continue.\n";
            return 1;
        }
    }

    if (vm.count("credentials")){
        args.credentials = vm["credentials"].as<string>();
    }

    args.multiPass = vm.count("pyramid");

    args.confidence = 0.0;
    if (vm.count("confidence")){
        args.confidence = vm["confidence"].as<double>();
    }



    args.zoom = 18;
    if (vm.count("zoom")){
        args.zoom = vm["zoom"].as<long>();
    }

    args.stepSize = 0;
    args.windowSize = 0;
    if (vm.count("stepSize")){
        args.stepSize = vm["stepSize"].as<long>();
    }

    if (vm.count("windowSize")){
        args.windowSize = vm["windowSize"].as<long>();
    }

    if ((args.stepSize == 0 && args.windowSize  > 0) || (args.stepSize > 0 && args.windowSize == 0)){
        cout << "Unable to continue as configured.  Sliding window processing must have a step size and window size.\n";
        return INVALID_MULTIPASS;
    }

    // If a port is specified, switch into server mode.  Many options thusfar may be overwritten by the request.
    if (vm.count("port")) {
        typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;
        unsigned short port = vm["port"].as<unsigned short>();

        size_t threads = 4;
        if (vm.count("threads")) {
            threads = vm["threads"].as<size_t>();
        }

        try {
            cout << "Starting server with " << threads << " threads on port " << port << "." << endl;
            HttpServer server(port, threads);
            OsnServer rpcServer(args);

            server.resource["^/osn$"]["POST"] = rpcServer.makeHttpHook();

            server.default_resource["GET"]=[](HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
                string content="Could not get path "+request->path+"\n";
                response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
            };

            server.default_resource["POST"]=[](HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
                string content="Could not post to path "+request->path+"\n";
                response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
            };

            server.start();
        } catch (const std::exception & e) {
            cout << "Failure stating server: " << e.what();
            exit(2);
        } catch (const boost::exception & e ) {
            std::cerr << "Failure stating server: " << boost::diagnostic_information(e);
        } catch (...) {
            cout << "Unknown failure starting server";
            exit(3);
        }

    } else {
        boost::timer::auto_cpu_timer t;
        return classifyBroadAreaMultiProcess(args);
    }




}