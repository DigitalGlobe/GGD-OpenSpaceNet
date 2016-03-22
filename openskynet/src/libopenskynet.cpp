//
// Created by joe on 11/11/15.
//
#include <iostream>
#include <string>
#include <vector>
#include <libopenskynet.h>
#include <coordinateHelper.h>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <TileRequest.h>
#include <Tile.h>
#include <OpenSkyNetArgs.h>
#include <opencv2/imgcodecs/imgcodecs_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/highgui.hpp>
#include <Classifier.h>
#include <CaffeBatchClassifier.h>
#include <boost/timer/timer.hpp>
#include <curl/curl.h>
#include <curlpp/cURLpp.hpp>
#include <glob.h>
#include <GdalTileProducer.h>
#include <WmtsTileProducer.h>
#include <Threshold.h>

boost::lockfree::queue<TileRequest *> tileRequestQueue(50000);
boost::lockfree::queue<Tile *> classificationQueue(50000);
const int consumer_thread_count = boost::thread::hardware_concurrency() - 1;

std::string outputPath;
std::string outputFormat;
std::string credentials;
GeometryType outputType;
CaffeBatchClassifier *classifier = nullptr;
double scale = 2.0;

long *tileCount = new long(0);
long *processedCount = new long(0);

/* Function used to glob model names directories for ensemble models */
inline std::vector<std::string> glob(const std::string& pat){
    glob_t glob_result;
    glob(pat.c_str(),GLOB_TILDE,NULL,&glob_result);
    std::vector<string> ret;
    for(unsigned int i=0;i<glob_result.gl_pathc;++i){
        ret.push_back(string(glob_result.gl_pathv[i]));
    }
    globfree(&glob_result);
    return ret;
}

void persistResultsWindows(std::vector<WindowPrediction>& results, Tile *tile, 
                           VectorFeatureSet* geom, const Threshold& thresholds, 
                           const cv::Rect *rect = nullptr) {
    for (auto result = results.begin(); result != results.end(); result++) {
        if (result->predictions.size() > 0) {
            for (auto prediction = result->predictions.begin(); 
                 prediction != result->predictions.end(); prediction++) {
                
                if (prediction->second >= thresholds.getThreshold(prediction->first)) {
                    std::cout << "Region above threshold - adding to results." << std::endl;
                    std::cout << "  " << prediction->first << " " << prediction->second << std::endl;
            
                    std::vector<std::pair<double, double>> geometry;
                    tile->getWindowCoords(result->window, geometry);
                    std::cout << "Creating polygon from tile sub rect " << geometry[0].first << "," << geometry[0].second <<
                    " from tile " << tile->tile().first << "," << tile->tile().second << ".\n";
                    /* How to abstract away zoom ? */
                    //geom->addPolygon(result->predictions, geometry, tile->tile(), tile->zoom(), 0);
                    geom->addPolygon(result->predictions, geometry, tile->tile(), -1, 0);
                    // break if any are satisfied - all predictions get written
                    break;
                }
            }
        }
    }
}


int classifyBroadAreaMultiProcess(OpenSkyNetArgs &args) {
    curlpp::Cleanup cleaner;

    /* geometryType is originally set to an empty string to GeometryType::POLYGON is the default */
    outputType = args.geometryType == "POINT" ? GeometryType::POINT : GeometryType::POLYGON;

    /* Create pyramid object */
    /* 
     */
    /* Process the pyramid options */
    //Pyramid* pyramid = new Pyramid(150, 30, 30, 1.5);
    std::cout << "Pyramid window sizes: " << std::endl;
    for (auto it = args.pyramidWindowSizes.begin(); it != args.pyramidWindowSizes.end(); it++) {

    std::cout << "  " << *it << std::endl;
    }
    std::cout << "Pyramid step sizes: " << std::endl;
    for (auto it = args.pyramidWindowSteps.begin(); it != args.pyramidWindowSteps.end(); it++) {

    std::cout << "  " << *it << std::endl;
    }

    Pyramid* pyramid = new Pyramid(args.pyramidWindowSizes, args.pyramidWindowSteps);
 
    std::cout << "Pyramid:" << std::endl;
    for (pyramid->begin(); ! pyramid->end(); ++(*pyramid)) {
        long winSize, stepSize;
        pyramid->current(winSize, stepSize);
        std::cout << winSize << ", " << stepSize << std::endl;
    }

    /* Retrieve the caffe model paths */
    if (!boost::filesystem::is_directory(args.modelPath)) {
        std::cout << "Model directory not found.  Aborting processing.";
        return NO_MODEL_FILE;
    }

    //TODO: Support multiple directories for ensemble model
    std::vector<std::string> modelGlob;
    modelGlob = glob(args.modelPath + "/*.caffemodel");
    if (modelGlob.size() == 0) {
        std::cout << "No model file .caffemodel was found.  Aborting processing.";
        return NO_MODEL_FILE;
    }

    std::string modelFile = modelGlob[0];
    std::string meanFile = args.modelPath + "mean.binaryproto";
    std::string labelFile = args.modelPath + "labels.txt";
    std::string deployFile = args.modelPath + "deploy.prototxt";

    std::cout << "Initializing classifier from model.\n";
    classifier = new CaffeBatchClassifier(deployFile, modelFile, meanFile, labelFile, *pyramid, args.useGPU);
    std::cout << "Classifier initialization complete.\n";

    /* Class specific and global thresholding */
    Threshold thresholds(args.classThresholds, args.threshold);
    thresholds.printThresholds();

    VectorFeatureSet *fs;
    try {
        fs = new VectorFeatureSet(args.outputPath, args.outputFormat, args.layerName);
    }
    catch (std::invalid_argument) {
        std::cout << "An invalid format was provided for output." << std::endl;
        curl_global_cleanup();
        return BAD_OUTPUT_FORMAT;
    }

    TileProducer* tiler;
    if (args.useTileServer) {
        std::cout << "Classification WMTS tiles" << std::endl;
        /* Download and put Tiles on classification queue */
        tiler = new WmtsTileProducer(args.url, args.credentials, fs,
                                     args.bbox, args.zoom,
                                     args.rowSpan, args.columnSpan,
                                     args.startRow, args.startColumn);
    }
    else {
        std::cout << "Classifcation on local image" << std::endl;
        /* Tile image and put Tiles on classification queue */
        /* TODO: Tile size (and offset) from command line?
         * Can we calculate the offset from the pyramid? 
         * **/
        //tiler = new GdalTileProducer(args.image, 280, 280, 0, 0);
        tiler = new GdalTileProducer(args.image, 1400, 1400, 30, 30);
        fs->setProjection(tiler->getSpr());
    }
    tiler->PrintTiling();
    (*tileCount) = tiler->getNumTiles();

    while (! tiler->empty()) {
        Tile* tilePtr;
        tiler->pop(tilePtr);

        std::vector<WindowPrediction > results;
        results = classifier->Classify(*tilePtr, 5, thresholds.getLowestThreshold());
        /* Now process results */
        persistResultsWindows(results, tilePtr, fs, thresholds);
        tilePtr->printTileOffsets();

        delete tilePtr;

        double percentage = (((double) ++*processedCount) / *tileCount) * 100.0;
        std::cout << "Classification " << percentage << "% completed." << std::endl;
    }


    delete tiler;

    delete tileCount;
    delete processedCount;

    delete fs;

    delete classifier;

    delete pyramid;

    return SUCCESS;
}
