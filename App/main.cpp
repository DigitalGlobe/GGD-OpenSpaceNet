#include <iostream>
#include <boost/program_options.hpp>
#include "include/libopenskynet.h"
#include <boost/timer/timer.hpp>
#include <glog/logging.h>

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
    google::InitGoogleLogging(*av); // Turn off Caffe google output
    outputLogo();
    namespace po = boost::program_options;
    string allowedOutputs[] = {"shp", "kml", "elasticsearch", "postgis", "fgdb"};
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "Usage")
            ("connectId", po::value<string>(), "Connection id used for licensing")
            ("credentials", po::value<string>(), "Credentials for the map service")
            ("server", po::value<string>(), "URL to a valid WMS or WMTS service (premium users only).")
            ("image", po::value<string>(), "Local image (filetype .tif) rather than using tile service")
            ("gpu", "Use GPU for processing.")
            ("api", "Use DigitalGlobe Web API as the source of tiles")
            ("bbox", po::value<vector<double>>()->multitoken(), "Bounding box for determining tiles. This must be in longitude-latitude order.")
            ("startCol", po::value<long>(), "Starting tile column.")
            ("startRow", po::value<long>(), "Starting tile row.")
            ("columnSpan", po::value<long>(), "Number of columns.")
            ("zoom", po::value<long>(), "Zoom level to request tiles at. Defaults to zoom level 18.")
            ("rowSpan", po::value<long>(),"Number of rows.")
            ("model", po::value<vector<string>>()->multitoken(), "Folder location of the trained model.")
            ("type", po::value<string>(), "Output geometry type.  Currently only POINT and POLYGON are valid.")
            ("format", po::value<string>(), "Output file format for the results.  Valid values are shp, kml, elasticsearch, postgis, fgdb")
            ("output", po::value<string>(), "Output location with file name and path or URL.")
            ("outputLayerName", po::value<string>(), "The output layer name, index name, or table name.")
            ("confidence", po::value<double>(), "A factor to weed out weak matches for the classification process.")
            ("pyramid", "Pyramid the downloaded tile and run detection on all resultant images.\n WARNING: This will result in much longer run times, but may result in additional results from the classification process.")
            ("windowSize", po::value<long>(), "Used with stepSize to determine the height and width of the sliding window. \n WARNING: This will result in much longer run times, but may result in additional results from the classification process.")
            ("stepSize", po::value<long>(), "Used with windowSize to determine how much the sliding window moves on each step.\n WARNING: This will result in much longer run times, but may result in additional results from the classification process. ")
            ("pyramidWindowSizes", po::value<vector<long>>()->multitoken(), "Window sizes for pyramiding.")
            ("pyramidStepSizes", po::value<vector<long>>()->multitoken(), "Step sizes for pyramiding.")
            ("classThresholds", po::value<vector<pair<string, float>>>()->multitoken(), "Class specific thresholds.")
            ("threshold", po::value<double>(), "A global threshold.")
            ;

    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc, po::command_line_style::unix_style ^ po::command_line_style::allow_short), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << desc << "\n";
        return 1;
    }
    OpenSkyNetArgs args;

    const string DEFAULT_CONNECTID = "bede7af2-593a-4b9a-81ae-a06fc58b0c5b";
    if (vm.count("connectId")) {
        args.connectId = vm["connectId"].as<string>();
        cout << "ConnectId was set to "
        << args.connectId << ".\n";

    } else {
        cout << "ConnectId was not set. Using default connectId.\n";
        args.connectId = DEFAULT_CONNECTID;
    }

    if (vm.count("credentials")){
        args.credentials = vm["credentials"].as<string>();
    }

    if (vm.count("server")) {
        args.url = vm["server"].as<string>() + "?connectId=" + args.connectId;
        cout << "server was set to "
        << args.url << ".\n";
    } else {
        args.url = "https://evwhs.digitalglobe.com/earthservice/wmtsaccess";
        args.url += "?connectId=" + args.connectId;
        std::cout << "No url provided, using default.\n";
    }

    if (vm.count("image")) {
        args.image = vm["image"].as<string>();
        cout << "Input image was set to "
        << args.image << ". Not using tile server.\n";
        args.useTileServer = false;
    } else {
        cout << "Image path was not set. Using tile server.\n";
    }

    /* Bounding box */
    args.bbox.clear();
    if (vm.count("bbox")) {
        char buffer[100];
        args.bbox = vm["bbox"].as<vector<double>>();
        if (args.bbox.size() != 4) {
            cout << "Invalid  number of parameters for bounding box." << endl;
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
        else {
            if (! args.useTileServer) {
                // Local image 
                cout << "Classifying entire local image.\n";
            }
            else {
                cout << "Bounding box not set. Unable to continue.\n";
                return 1;
            }
        }
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
        args.modelPath =  vm["model"].as<vector<string>>();
        cout << "Model locations are: " << endl;
        for (auto modelIt = args.modelPath.begin(); modelIt != args.modelPath.end(); modelIt++) {
            cout << "  " << *modelIt << endl;
        }
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
        args.outputPath = "./";
    }

    if (vm.count("outputLayerName")){
        args.layerName = vm["outputLayerName"].as<string>();
        cout << "output layer name set to " << args.layerName << ".\n";
    }
    else {
        cout << "Default layer name of skynetdetects will be used." << endl;
        args.layerName = "skynetdetects";
    }

    if (vm.count("api")) {
        args.webApi = true;
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


    /* Previous method of setting windowing */
    args.multiPass = vm.count("pyramid");

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

    /* New multiresolution pyramiding */
    if (vm.count("pyramidWindowSizes")) {

        vector<long> winSizes = vm["pyramidWindowSizes"].as<vector<long>>();
        for (auto it = winSizes.begin(); it != winSizes.end(); it++) {
            args.pyramidWindowSizes.push_back(*it);
        }
    }
    else {
        args.pyramidWindowSizes.push_back(args.windowSize);
    }
    if (vm.count("pyramidStepSizes")) {
        vector<long> winSteps = vm["pyramidStepSizes"].as<vector<long>>();
        for (auto it = winSteps.begin(); it != winSteps.end(); it++) {
            args.pyramidWindowSteps.push_back(*it);
        }
    }
    else {
        args.pyramidWindowSteps.push_back(args.stepSize);
    }


    /* Class-specific thresholds */
    if (vm.count("classThresholds")) {
        cout << "Class-specific thresholds" << endl;
        cout << "Number of thresholds specified: " << vm.count("classThresholds") << endl;
        vector<pair<string, float>> cT = vm["classThresholds"].as<vector<pair<string, float>>>();
        /* From command line options into std::map for faster searching */
        for (auto it = cT.begin(); it != cT.end(); it++) {
            args.classThresholds[it->first] = it->second;
            cout << it->first << ": " << it->second << endl;
        }
    }
    
    /* Class-specific thresholds */
    args.threshold = 0.0;
    if (vm.count("threshold")){
        args.threshold = vm["threshold"].as<double>();
    }


    boost::timer::auto_cpu_timer t;

    return classifyBroadAreaMultiProcess(args);

}
