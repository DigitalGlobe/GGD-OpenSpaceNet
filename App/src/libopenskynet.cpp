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
#include <boost/progress.hpp>

#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include "../include/WorkItem.h"
#include <opencv2/highgui.hpp>
#include <DeepCore/classification/Classifier.h>
#include <DeepCore/classification/GbdxModelReader.h>
#include <DeepCore/classification/ModelMetadata.h>
#include <DeepCore/classification/Model.h>
#include <DeepCore/classification/Prediction.h>
#include <DeepCore/imagery/Tile.h>
#include <DeepCore/imagery/WmtsTile.h>
#include <DeepCore/utility/coordinateHelper.h>
#include <DeepCore/network/HttpDownloader.h>
#include <curl/curl.h>
#include <curlpp/cURLpp.hpp>
#include <caffe/caffe.hpp>
#include <curlpp/Exception.hpp>

using namespace dg::deepcore::classification;
using namespace std;
boost::lockfree::queue<WorkItem *> downloadQueue(50000);
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

Model* model = nullptr;
Classifier *classifier = nullptr;
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
boost::progress_display *show_progress = nullptr;
int *current = new int(0);

static const string WEB_API_URL = "http://a.tiles.mapbox.com/v4/digitalglobe.nmmhkk79/{z}/{x}/{y}.jpg?access_token=ccc_connect_id";
static const string WMTS_URL = "https://services.digitalglobe.com/earthservice/wmtsaccess?connectId=ccc_connect_id&version=1.0.0&request=GetTile&service=WMTS&Layer=DigitalGlobe:ImageryTileService&tileMatrixSet=EPSG:3857&tileMatrix=EPSG:3857:18&format=image/jpeg&FEATUREPROFILE=Global_Currency_Profile&USECLOUDLESSGEOMETRY=false";

void convertPredictions(const std::vector<Prediction>& predictions, std::vector<std::pair<std::string, float>>& out ){
    for (int i = 0; i < predictions.size(); i++){
        out.push_back({predictions[i].label, predictions[i].confidence});
    }
}

void persistResults(std::vector<Prediction> &results, WorkItem *item, const cv::Rect *rect = nullptr) {
    if (results.size() > 0) {
        std::vector<std::pair<string, float>> predictions;
        convertPredictions(results, predictions);
        if (outputType == GeometryType::POINT) {
            if (rect == nullptr) {
                //flip the result because the converter helper takes row then column
                item->geometry()->addPoint(predictions,
                                           coordinateHelper::num2deg(item->tile().second, item->tile().first,
                                                                     item->zoom()),
                                           item->tile(), item->zoom(), 0);

            } else {
                item->geometry()->addPoint(predictions, coordinateHelper::num2deg(rect->y, rect->x, item->zoom()),
                                           item->tile(), item->zoom(), 0);
            }
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
             } else {
                geometry = coordinateHelper::detectionWindow(rect->x, rect->y, rect->height, item->tile().second,
                                                             item->tile().first,
                                                             item->zoom());

            }
            item->geometry()->addPolygon(predictions, geometry, item->tile(), item->zoom(), 0);

        }

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
                results = classifier->classify(WmtsTile(mat), *confidence);
                if (results.size() > 0) {
                    persistResults(results, item, &rects[j]);
                }
            }
        }

        results = classifier->classify(WmtsTile(data_mat), *confidence);
        persistResults(results, item);

    }

    return 0;
}

int processTile(WorkItem *item) {
    if (item == nullptr || item->url().length() == 0) {
        return -1;
    }

    HttpDownloader downloader;
    cv::Mat image;

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
                item->incrementRetryCount();
                downloadQueue.push(item);
                return -1;
            }
        } catch (curlpp::RuntimeError){
            item->incrementRetryCount();
            downloadQueue.push(item);
            return -1;

        }

    }
    else
    {
        ++failures;
        delete item;
        return -1;
    }

    return 0;
}


int process() {
    WorkItem *item;
    while (downloadQueue.pop(item)) {
        int retVal = processTile(item);
        if (retVal == 0){
            ++(*show_progress);
        }
    }
    return 0;
}

// Since this problem is a classic producer-consumer problem, the original implementation was a pair of queues, with
// WorkItems being shuffled from one to another based on successful completion of the previous stage.  Due to some issues
// with the classifier being slow to load and fast to process, the current implementation uses a pair of queues, but only
// the download downloadQueue is multi-threaded.  The classifier's Predict call mutates the state of the net, which is generally
// bad in a multi-threaded call, so it's all processed sequentially.  The speed of the classifier on GPU makes this possible.
void downloadAndProcessRow(int numThreads){
    //start working by downloading everything
    boost::thread_group downloadThreads;
    for (int i = 0; i < numThreads; ++i) {
        downloadThreads.create_thread(process);
    }
    downloadThreads.join_all();


    //classify the images once complete.

    WorkItem *item;
    while (classificationQueue.pop(item)) {
        classifyTile(item);
        ++(*show_progress);
        delete item;
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
    *confidence = args.confidence;

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


    //std::cout << "columns: " << colStart << " to " << (colStart + numCols - 1) << ".\n";
    //std::cout << "rows: " << rowStart << " to " << (rowStart + numRows - 1) << ".\n";
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

    //if (!boost::filesystem::is_directory(args.modelPath)) {
    //    std::cout << "No model file was found.  Aborting processing.";
    //    return NO_MODEL_FILE;
    //}

    //TODO: make this a little smarter!  This would be a good place to load multiple models and have it
    // create multiple classifiers, maybe based on folder structure or maybe zipped file placement.
    modelFile = args.modelPath + "model.caffemodel";
    meanFile = args.modelPath + "mean.binaryproto";
    labelFile = args.modelPath + "labels.txt";
    deployFile = args.modelPath + "deploy.prototxt";

    //Loading of the model I'm currently using (200+ MB) takes a couple of seconds.  For that reason, I'm using a single
    //model and loading it prior to any items being processed.
    //std::cout << "Initializing classifier from model.\n";
    GbdxModelReader modelReader(args.modelPath);
    ModelPackage* modelPackage = modelReader.readModel();
    if (modelPackage == nullptr){
        cout << "Unable to generate model from the provided package." << endl;
        return -1;
    }
    model = Model::create(*modelPackage, args.useGPU);
    classifier = &(model->classifer());
    //classifier = new CaffeBatchClassifier(deployFile, modelFile, meanFile, labelFile, args.useGPU);
    //std::cout << "Classifier initialization complete.\n";
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
    //std::cout << "Loading initial urls for downloading.\n";
    downloadedCount = new long(0);
    processedCount = new long(0);
    int numThreads = consumer_thread_count * args.numThreads;
    if (numCols < numThreads){
        numThreads = numCols;
    }
    std::cout << (boost::format("Num threads: %1d") % numThreads).str() << std::endl;
    show_progress = new boost::progress_display((numRows * numCols) * 2UL);

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
            downloadQueue.push(item);
        }
        downloadAndProcessRow(numThreads);
    }

    std::cout << "\nNumber of failures: " << *failures << std::endl;

    //cleanup
    //delete classifier;
    delete model;
    delete tileCount, downloadedCount, processedCount;
    delete fs;
    delete multiPass;
    delete pyramid;
    delete stepSize, windowSize;
    delete confidence;
    delete failures;

    return SUCCESS;
}

