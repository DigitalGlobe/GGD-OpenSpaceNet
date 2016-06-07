#include <iostream>
#include <boost/program_options.hpp>
#include "include/libopenskynet.h"
#include <boost/timer/timer.hpp>
#include <DeepCore/utility/Logging.h>

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
    dg::deepcore::log::init();

    outputLogo();
    namespace po = boost::program_options;

    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "Usage")
            ("image", po::value<string>(), "Local image (filetype .tif) rather than using tile service")
            ("token", po::value<string>(), "API token used for licensing. This is the connectId for the WMTS service or the API key for the Web Maps API.")
            ("credentials", po::value<string>(), "Credentials for the map service. Not required for Web Maps API.")
            ("gpu", "Use GPU for processing.")
            ("api", "Use DigitalGlobe Web API as the source of tiles")
            ("bbox", po::value<vector<double>>()->multitoken(), "Bounding box for determining tiles. This must be in longitude-latitude order.")
            ("startCol", po::value<long>(), "Starting tile column.")
            ("startRow", po::value<long>(), "Starting tile row.")
            ("columnSpan", po::value<long>(), "Number of columns.")
            ("zoom", po::value<long>(), "Zoom level to request tiles at. Defaults to zoom level 18.")
            ("rowSpan", po::value<long>(),"Number of rows.")
            ("model", po::value<string>(), "Folder location of the trained model.")
            ("type", po::value<string>(), "Output geometry type.  Currently only POINT and POLYGON are valid.")
            ("format", po::value<string>(), "Output file format for the results.  Valid values are shp, kml, elasticsearch, postgis, fgdb")
            ("output", po::value<string>(), "Output location with file name and path or URL.")
            ("outputLayerName", po::value<string>(), "The output layer name, index name, or table name.")
            ("confidence", po::value<double>(), "A factor to weed out weak matches for the classification process.")
            ("pyramid", "Pyramid the downloaded tile and run detection on all resultant images.\n WARNING: This will result in much longer run times, but may result in additional results from the classification process.")
            ("windowSize", po::value<long>(), "Used with stepSize to determine the height and width of the sliding window. \n WARNING: This will result in much longer run times, but may result in additional results from the classification process.")
            ("stepSize", po::value<long>(), "Used with windowSize to determine how much the sliding window moves on each step.\n WARNING: This will result in much longer run times, but may result in additional results from the classification process. ")
            ("numConcurrentDownloads", po::value<long>(), "Used to speed up downloads by allowing multiple conccurrent downloads to happen at once.")
            ;

    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc, po::command_line_style::unix_style ^ po::command_line_style::allow_short), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << desc << "\n";
        return 0;
    }
    OpenSkyNetArgs args;

    if (vm.count("api")) {
        args.webApi = true;
    }

    if (vm.count("image")) {
        args.image = vm["image"].as<string>();
        args.useTileServer = false;
    }

    const string DEFAULT_CONNECTID = "bede7af2-593a-4b9a-81ae-a06fc58b0c5b";
    if (vm.count("token")) {
        args.token = vm["token"].as<string>();
    } else if(args.useTileServer){
        if (args.webApi){
            cout << "A token must be supplied to use the Web Maps API.\n";
            return 1;
        }
        cout << "token was not set. Using default token.\n";
        args.token = DEFAULT_CONNECTID;
    }

    if (vm.count("credentials")){
        args.credentials = vm["credentials"].as<string>();
    }

    if (vm.count("server")) {
        args.url = vm["server"].as<string>() + "?connectId=" + args.token;
    } else if(args.useTileServer){
        args.url = "https://evwhs.digitalglobe.com/earthservice/wmtsaccess";
        args.url += "?connectId=" + args.token;
    }

    if (vm.count("bbox")) {
        args.bbox = vm["bbox"].as<vector<double>>();
        if (args.bbox.size() != 4) {
            cout << "Invalid  number of parameters for bounding box." << endl;
        }
    }
    else {
        if (vm.count("rowSpan") && vm.count("columnSpan"))
        {
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
        else if(args.useTileServer) {
            cout << "Bounding box not set. Unable to continue.\n";
            return 1;
        }
    }

    args.useGPU = false;
    if (vm.count("gpu")) {
        args.useGPU = true;
    }

    // How is this defaulted?
    //TODO: Get with Andrew about default models.
    if (vm.count("model")) {
        args.modelPath =  vm["model"].as<string>();
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
    }

    //TODO: make this smarter wrt format.  If the output format is ES, then the default doesn't work.
    if (vm.count("output")) {
        args.outputPath = vm["output"].as<string>();
    }

    if (vm.count("outputLayerName")){
        args.layerName = vm["outputLayerName"].as<string>();
    }
    else {
        args.layerName = "skynetdetects";
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

    if (args.useTileServer && ((args.stepSize == 0 && args.windowSize  > 0) || (args.stepSize > 0 && args.windowSize == 0))){
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

    if (vm.count("numConcurrentDownloads")){
        args.numThreads = vm["numConcurrentDownloads"].as<long>();
    }

    boost::timer::auto_cpu_timer t;

    try {
        if(args.useTileServer) {
            return classifyBroadAreaMultiProcess(args);
        } else {
            return classifyFromFile(args);
        }
    } catch (const std::exception& e) {
        cerr << e.what() << endl;
        exit(1);
    } catch (...) {
        cerr << "Unknown error" << endl;
    }
}
