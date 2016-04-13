//
// Created by joe on 11/11/15.
//
#include <iostream>
#include <vector>
#include "../include/libopenskynet.h"
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include "../include/WorkItem.h"
#include <opencv2/highgui.hpp>
#include <DeepCore/classification/CaffeClassifier.h>
#include <DeepCore/utility/coordinateHelper.h>
#include <DeepCore/network/HttpDownloader.h>
#include <boost/timer/timer.hpp>
#include <curl/curl.h>
#include <curlpp/cURLpp.hpp>
#include <caffe/caffe.hpp>
#include <curlpp/Exception.hpp>

boost::lockfree::queue<WorkItem *> queue(50000);
boost::lockfree::queue<WorkItem *> classificationQueue(50000);
const int consumer_thread_count = boost::thread::hardware_concurrency() - 1;

std::string modelFile;
std::string meanFile;
std::string labelFile;
std::string deployFile;
std::string outputPath;
std::string outputFormat;
std::string credentials;
GeometryType outputType;

CaffeClassifier *classifier = nullptr;
bool *multiPass = new bool(false);
bool *pyramid = new bool(false);
long *stepSize = new long(0);
long *windowSize = new long(0);
double scale = 2.0;
double *confidence = new double(0.0);
long *failures = new long(0);
long *tileCount = new long(0);
long *downloadedCount = new long(0);
long *processedCount = new long(0);
int batchSize = 1e4;

static const string WEB_API_URL = "http://a.tiles.mapbox.com/v4/digitalglobe.nmmhkk79/{z}/{x}/{y}.jpg?access_token=ccc_connect_id";
static const string WMTS_URL = "https://services.digitalglobe.com/earthservice/wmtsaccess?connnectId=ccc_connect_id&version=1.0.0&request=GetTile&service=WMTS&Layer=DigitalGlobe:ImageryTileService&tileMatrixSet=EPSG:3857&tileMatrix=EPSG:3857:18&format=image/jpeg&FEATUREPROFILE=Global_Currency_Profile&USECLOUDLESSGEOMETRY=false";
void persistResults(std::vector<Prediction> &results, WorkItem *item, const cv::Rect *rect = nullptr) {
    if (results.size() > 0) {
        if (outputType == GeometryType::POINT) {
            if (rect == nullptr) {
                //flip the result because the converter helper takes row then column
                item->geometry()->addPoint(results,
                                           coordinateHelper::num2deg(item->tile().second, item->tile().first,
                                                                     item->zoom()),
                                           item->tile(), item->zoom(), 0);

            } else {
                item->geometry()->addPoint(results, coordinateHelper::num2deg(rect->y, rect->x, item->zoom()),
                                           item->tile(), item->zoom(), 0);
            }
            std::cout << "Creating point from tile " << item->tile().first << "," << item->tile().second <<
            ".\n";
        }
        else {
            std::vector<std::pair<double, double>> geometry;
            if (rect == nullptr) {
                geometry.push_back(
                        coordinateHelper::num2deg(item->tile().second, item->tile().first, item->zoom()));
                geometry.push_back(
                        coordinateHelper::num2deg(item->tile().second + 1, item->tile().first, item->zoom()));
                geometry.push_back(
                        coordinateHelper::num2deg(item->tile().second + 1, item->tile().first + 1, item->zoom()));
                geometry.push_back(
                        coordinateHelper::num2deg(item->tile().second, item->tile().first + 1, item->zoom()));
                geometry.push_back(
                        coordinateHelper::num2deg(item->tile().second, item->tile().first, item->zoom()));
                std::cout << "Creating polygon from tile " << item->tile().first << "," << item->tile().second <<
                ".\n";
            } else {
                geometry = coordinateHelper::detectionWindow(rect->x, rect->y, rect->height, item->tile().second,
                                                             item->tile().first,
                                                             item->zoom());
                std::cout << "Creating polygon from tile sub rect " << geometry[0].first << "," << geometry[0].second <<
                " from tile " << item->tile().first << "," << item->tile().second << ".\n";

            }
            item->geometry()->addPolygon(results, geometry, item->tile(), item->zoom(), 0);

        }

    }
    else {
        std::cout << "No results found from tile: " << item->tile().first << ", " << item->tile().second << ".\n";
    }

}

int classifyTile(WorkItem *item) {
    for (size_t i = 0; i < item->images().size(); ++i) {
        const cv::Mat& data_mat = item->images().at(i); //convert string into a vector
        std::vector<Prediction> results;
        if (*multiPass) {
            //Sliding window implementation
            std::vector<cv::Rect> rects = item->get_sliding_windows(data_mat, *windowSize, *windowSize, *stepSize);
            for (int j = 0; j < rects.size(); ++j) {
                cv::Mat mat(data_mat, rects[j]);
                results = classifier->Classify(mat, 5, *confidence);
                if (results.size() > 0) {
                    persistResults(results, item, &rects[j]);
                }
            }
        }

        results = classifier->Classify(data_mat, 5, *confidence);
        persistResults(results, item);
        double percentage = (((double) ++*processedCount) / *tileCount) * 100.0;
        std::cout << "Classification " << percentage << "% completed.";
    }

    return 0;
}

int processTile(WorkItem *item) {
    if (item == nullptr || item->url().length() == 0) {
        return -1;
    }

    HttpDownloader downloader;
    cv::Mat image;

    //cv::Mat *image = downloader.download(item->url(), credentials);
    //if (image != nullptr) {
    if (item->retryCount() < 5){

        bool retVal = false;
        try {
            retVal = downloader.download(item->url(), image, credentials);
            if (retVal){
                item->addImage(image);
                if (*pyramid) { //downscale the images
                    item->pyramid(scale, 30);
                }
                classificationQueue.push(item);
            }
            else {
                std::cout << "Download failed for tile " << item->tile().first << " " << item->tile().second << ".\n";
                std::cout << "Adding the tile back to the queue for redownload.\n";
                item->incrementRetryCount();
                queue.push(item);
                return -1;
            }
        } catch (curlpp::RuntimeError){
            std::cout << "Download failed for tile " << item->tile().first << " " << item->tile().second << ".\n";
            std::cout << "Adding the tile back to the queue for redownload.\n";
            item->incrementRetryCount();
            queue.push(item);
            return -1;

        }

    }
    else
    {
        std::cout << "Maximum retries failed for tile " << item->tile().first << " " << item->tile().second << ".\n";
        ++failures;
        delete item;
        return -1;
    }

    return 0;
}


int process() {
    WorkItem *item;
    while (queue.pop(item)) {
        int retVal = processTile(item);
        if (retVal == 0){
            double percentage = (((double) ++*downloadedCount) / *tileCount) * 100.0;
            std::cout << "Downloads: " << percentage << "% completed.\n";
        }
    }
    return 0;
}

// Since this problem is a classic producer-consumer problem, the original implementation was a pair of queues, with
// WorkItems being shuffled from one to another based on successful completion of the previous stage.  Due to some issues
// with the classifier being slow to load and fast to process, the current implementation uses a pair of queues, but only
// the download queue is multi-threaded.  The classifier's Predict call mutates the state of the net, which is generally
// bad in a multi-threaded call, so it's all processed sequentially.  The speed of the classifier on GPU makes this possible.
void downloadAndProcessRow(){
    std::cout << "Downloading:\n";
    //start working by downloading everything
    boost::thread_group downloadThreads;
    for (int i = 0; i < consumer_thread_count; ++i) {
        downloadThreads.create_thread(process);
    }
    downloadThreads.join_all();

    std::cout << "Download complete.\n";
    std::cout << "Processing:\n";

    //classify the images once complete.

    WorkItem *item;
    while (classificationQueue.pop(item)) {
        classifyTile(item);
    }
}

int classifyBroadAreaMultiProcess(OpenSkyNetArgs &args) {
    curlpp::Cleanup cleaner;
    long numCols = 0, numRows = 0, rowStart = 0, colStart = 0;

    if (args.columnSpan != 0 && args.rowSpan != 0) {
        rowStart = args.startRow;
        colStart = args.startColumn;
        numCols = args.columnSpan;
        numRows = args.rowSpan;
    }
    *multiPass = (args.stepSize >= 1 && args.windowSize >= 1);
    *pyramid = args.multiPass;
    *stepSize = args.stepSize;
    *windowSize = args.windowSize;


    //if the bbox is specified, it takes precedence!
    if (args.bbox.size() != 0) {
        std::vector<double> aoi = args.bbox;
        int zoomLevel = args.zoom;

        std::pair<double, double> llCorner = coordinateHelper::deg2num(aoi[1], aoi[0], zoomLevel);
        std::pair<double, double> urCorner = coordinateHelper::deg2num(aoi[3], aoi[2], zoomLevel);

        numCols = (int) (urCorner.first - llCorner.first);
        numRows = (int) (llCorner.second - urCorner.second);

        rowStart = (int) (llCorner.second - numRows);
        colStart = (int) (urCorner.first - numCols);
    }


    std::cout << "columns: " << colStart << " to " << (colStart + numCols - 1) << ".\n";
    std::cout << "rows: " << rowStart << " to " << (rowStart + numRows - 1) << ".\n";
    tileCount = new long(0);
    *tileCount = numCols * numRows;

    std::string completedUrl = "";
    if (args.webApi){
        completedUrl = WEB_API_URL;
    }
    else {
        completedUrl = WMTS_URL;
    }

    boost::replace_all(completedUrl, "ccc_connect_id", args.token);

    if (!boost::filesystem::is_directory(args.modelPath)) {
        std::cout << "No model file was found.  Aborting processing.";
        return NO_MODEL_FILE;
    }

    //TODO: make this a little smarter!  This would be a good place to load multiple models and have it
    // create multiple classifiers, maybe based on folder structure or maybe zipped file placement.
    modelFile = args.modelPath + "model.caffemodel";
    meanFile = args.modelPath + "mean.binaryproto";
    labelFile = args.modelPath + "labels.txt";
    deployFile = args.modelPath + "deploy.prototxt";

    //Loading of the model I'm currently using (200+ MB) takes a couple of seconds.  For that reason, I'm using a single
    //model and loading it prior to any items being processed.
    std::cout << "Initializing classifier from model.\n";
    classifier = new CaffeClassifier(deployFile, modelFile, meanFile, labelFile, args.useGPU);
    std::cout << "Classifier initialization complete.\n";
    VectorFeatureSet *fs;
    try {
        fs = new VectorFeatureSet(args.outputPath, args.outputFormat, args.layerName);
    }
    catch (std::invalid_argument) {
        std::cout << "An invalid format was provided for output.";
        curl_global_cleanup();
        return BAD_OUTPUT_FORMAT;
    }

    outputType = args.geometryType == "POINT" ? GeometryType::POINT : GeometryType::POLYGON;
    credentials = args.credentials;
    std::cout << "Loading initial urls for downloading.\n";
    downloadedCount = new long(0);
    processedCount = new long(0);

    for (long i = rowStart; i < rowStart + numRows; i++) {
        for (long j = colStart; j < colStart + numCols; j++) {
            WorkItem *item = new WorkItem;
            std::string finalUrl = completedUrl;
            if (args.webApi){
                boost::replace_all(finalUrl, "{z}", (boost::format("%1d") % args.zoom).str());
                boost::replace_all(finalUrl, "{x}", (boost::format("%1d") % j).str());
                boost::replace_all(finalUrl, "{y}", (boost::format("%1d") % i).str());
            }
            else {
                finalUrl += (boost::format("&tilerow=%1d") % i).str();
                finalUrl += (boost::format("&tilecol=%ld") % j).str();
            }
            item->setUrl(finalUrl);
            item->setTile(j, i);
            item->setZoom(args.zoom);
            item->geometry() = fs;
            queue.push(item);
        }
        downloadAndProcessRow();
    }

    std::cout << "Number of failures: " << *failures << std::endl;

    //cleanup
    delete classifier;
    delete tileCount, downloadedCount, processedCount;
    delete fs;
    delete multiPass;
    delete pyramid;
    delete stepSize, windowSize;
    delete confidence;
    delete failures;

    return SUCCESS;
}

