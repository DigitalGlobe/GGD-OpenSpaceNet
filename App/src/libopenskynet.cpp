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

#include "../include/libopenskynet.h"
#include "../include/WorkItem.h"

#include <chrono>
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/progress.hpp>

#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <DeepCore/classification/Classifier.h>
#include <DeepCore/classification/GbdxModelReader.h>
#include <DeepCore/classification/Model.h>
#include <DeepCore/classification/SlidingWindowDetector.h>
#include <DeepCore/imagery/GdalImage.h>
#include <DeepCore/utility/coordinateHelper.h>
#include <DeepCore/network/HttpDownloader.h>
#include <curlpp/cURLpp.hpp>
#include <caffe/caffe.hpp>
#include <curlpp/Exception.hpp>

using namespace dg::deepcore::classification;
using namespace dg::deepcore::imagery;
using namespace dg::deepcore::vector;

using namespace std;

using chrono::high_resolution_clock;
using chrono::duration;

boost::lockfree::queue<WorkItem *> downloadQueue(50000);
boost::lockfree::queue<WorkItem *> classificationQueue(50000);
const int consumer_thread_count = boost::thread::hardware_concurrency() - 1;

std::string credentials;
GeometryType outputType;

Model* model = nullptr;
Classifier *classifier = nullptr;
bool multiPass = false;
bool pyramid = false;
int stepSize = 0;
int windowSize = 0;
double scale = 2.0;
double confidence = 0.0;
long failures = 0;

unique_ptr<boost::progress_display> show_progress;

static const string WEB_API_URL = "http://a.tiles.mapbox.com/v4/digitalglobe.nmmhkk79/{z}/{x}/{y}.jpg?access_token=ccc_connect_id";
static const string DGCS_URL = "https://services.digitalglobe.com/earthservice/wmtsaccess?connectId=ccc_connect_id&version=1.0.0&request=GetTile&service=WMTS&Layer=DigitalGlobe:ImageryTileService&tileMatrixSet=EPSG:3857&tileMatrix=EPSG:3857:18&format=image/jpeg&FEATUREPROFILE=Global_Currency_Profile&USECLOUDLESSGEOMETRY=false";
static const string EVWHS_URL = "https://evwhs.digitalglobe.com/earthservice/wmtsaccess?connectId=ccc_connect_id&version=1.0.0&request=GetTile&service=WMTS&Layer=DigitalGlobe:ImageryTileService&tileMatrixSet=EPSG:3857&tileMatrix=EPSG:3857:18&format=image/jpeg&FEATUREPROFILE=Global_Currency_Profile&USECLOUDLESSGEOMETRY=false";

void persistResults(std::vector<Prediction> &predictions, WorkItem *item, const cv::Rect *rect = nullptr) {
    if (predictions.size() > 0) {
        if (outputType == GeometryType::POINT) {
            if (rect == nullptr) {
                //flip the result because the converter helper takes row then column
                item->geometry()->addPoint(predictions,
                                           coordinateHelper::num2deg(item->tile().y, item->tile().x,
                                                                     item->zoom()),
                                           item->tile(), item->zoom(), 0);

            } else {
                item->geometry()->addPoint(predictions, coordinateHelper::num2deg(rect->y, rect->x, item->zoom()),
                                           item->tile(), item->zoom(), 0);
            }
         }
        else {
            std::vector<cv::Point2d> geometry;
            if (rect == nullptr) {
                geometry.push_back(
                        coordinateHelper::num2deg(item->tile().y, item->tile().x, item->zoom()));
                geometry.push_back(
                        coordinateHelper::num2deg(item->tile().y + 1, item->tile().x, item->zoom()));
                geometry.push_back(
                        coordinateHelper::num2deg(item->tile().y + 1, item->tile().x + 1, item->zoom()));
                geometry.push_back(
                        coordinateHelper::num2deg(item->tile().y, item->tile().x + 1, item->zoom()));
                geometry.push_back(
                        coordinateHelper::num2deg(item->tile().y, item->tile().x, item->zoom()));
             } else {
                geometry = coordinateHelper::detectionWindow(rect->x, rect->y, rect->height, item->tile().y,
                                                             item->tile().x,
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
        if (multiPass) {
            //Sliding window implementation
            std::vector<cv::Rect> rects = item->get_sliding_windows(data_mat, windowSize, windowSize, stepSize);
            for (const auto& rect : rects) {
                 cv::Mat mat(data_mat, rect);
                results = classifier->classify(mat, confidence);
                if (results.size() > 0) {
                    persistResults(results, item, &rect);
                }
            }
        } else {
            results = classifier->classify(data_mat, confidence);
            persistResults(results, item);
        }
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
                if (pyramid) { //downscale the images
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

VectorFeatureSet* createFeatureSet(const OpenSkyNetArgs& args) {
    unique_ptr<VectorFeatureSet> fs(new VectorFeatureSet(args.outputPath, args.outputFormat, args.layerName));
    outputType = args.geometryType == "POINT" ? GeometryType::POINT : GeometryType::POLYGON;

    return fs.release();
}

int loadModel(const OpenSkyNetArgs& args) {
    GbdxModelReader modelReader(args.modelPath);

    cout << "Reading model package..." << endl;
    unique_ptr<ModelPackage> modelPackage(modelReader.readModel());
    if (modelPackage == nullptr){
        cerr << "Unable to generate model from the provided package." << endl;
        return -1;
    }

    cout << "Reading model..." << endl;
    if(args.useTileServer) {
        model = Model::create(*modelPackage, args.useGPU, { BatchSize::BATCH_SIZE, 1 });
    } else {
        model = Model::create(*modelPackage, args.useGPU, { BatchSize::MAX_UTILIZATION,  args.maxUtitilization });
    }

    return 0;
}

int addFeature(VectorFeatureSet& fs, const GdalImage& image, const cv::Rect& window, const vector<Prediction>& predictions)
{
    if(predictions.empty()) {
        return SUCCESS;
    }

    if(outputType == GeometryType::POINT) {
        cv::Point center(window.x + window.width / 2, window.y + window.height / 2);
        auto point = image.imageToLL(center);
        fs.addPoint(predictions, point, {-1, -1}, 0, 0);
    } else if(outputType == GeometryType::POLYGON){
        vector<cv::Point> points = {
                window.tl(),
                { window.x + window.width, window.y },
                window.br(),
                { window.x, window.y + window.height },
                window.tl()
        };

        vector<cv::Point2d> llPoints;
        for(const auto& point : points) {
            auto llPoint = image.imageToLL(point);
            llPoints.push_back(llPoint);
        }

        fs.addPolygon(predictions, llPoints, {0, 0}, 0, 0);
    } else {
        cerr << "Invalid output type." << endl;
        return -1;
    }

    return SUCCESS;
}

int classifyFromFile(OpenSkyNetArgs &args) {
    cout << endl << "Reading image...";
    GdalImage gdalImage(args.image);

    cv::Rect2d bbox;
    if(args.bbox.size() == 4) {
        bbox = cv::Rect(cv::Point2d(args.bbox[1], args.bbox[0]), cv::Point2d(args.bbox[4], args.bbox[3]));
    }

    if(!gdalImage.projection().empty()) {
        auto ul = gdalImage.imageToLL({0, 0});
        auto lr = gdalImage.imageToLL(gdalImage.size());
        cv::Rect2d imageBbox(ul, lr);

        if(bbox.area() == 0) {
            bbox = imageBbox;
        } else {
            bbox &= imageBbox;

            if(bbox.area() == 0) {
                cerr << "Bounding box and the image don't intersect." << endl;
                return BAD_OUTPUT_FORMAT;
            }
        }
    } else if(bbox.area() == 0) {
        cerr << "No bounding box specified and image is not geo-registered." << endl;
        return BAD_OUTPUT_FORMAT;
    }

    boost::progress_display openProgress(50);
    auto image = gdalImage.readImage([&openProgress](float progress, const char*) -> bool {
        size_t curProgress = (size_t)roundf(progress*50);
        if(openProgress.count() < curProgress) {
            openProgress += curProgress - openProgress.count();
        }
        return true;
    });
    cout << endl;

    if(loadModel(args)) {
        return -1;
    }

    cv::Point step;
    if(!args.stepSize) {
        classifier = &model->classifer();

        cout << "Classifying..." << endl;
        auto predictions = classifier->classify(image, args.confidence);

        unique_ptr<VectorFeatureSet> fs(createFeatureSet(args));
        addFeature(*fs, gdalImage, cv::Rect({ 0, 0 } , image.size()), predictions);
    } else {
        step = { args.stepSize, args.stepSize };

        auto& detector = model->detector();
        if(detector.detectorType() != Detector::SLIDING_WINDOW) {
            cerr << "Unsupported model type." << endl;
            return -1;
        }

        auto& slidingWindowDetector = dynamic_cast<SlidingWindowDetector&>(detector);

        Pyramid pyramid;
        if(args.multiPass) {
            pyramid = Pyramid(image.size(), model->metadata().windowSize(), step, 2.0);
        } else {
            pyramid = Pyramid({ { image.size(), step } });
        }

        cout << endl << "Detecting features...";

        boost::progress_display detectProgress(50);
        auto startTime = high_resolution_clock::now();
        auto predictions = slidingWindowDetector.detect(image, pyramid, args.confidence, 5, [&detectProgress](float progress) {
            size_t curProgress = (size_t)roundf(progress*50);
            if(detectProgress.count() < curProgress) {
                detectProgress += curProgress - detectProgress.count();
            }
        });
        duration<double> duration = high_resolution_clock::now() - startTime;
        cout << "Total detection time " << duration.count() << " s" << endl;

        unique_ptr<VectorFeatureSet> fs(createFeatureSet(args));

        for(const auto& prediction : predictions) {
            addFeature(*fs, gdalImage, prediction.window, prediction.predictions);
        }
    }

    return SUCCESS;
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
    multiPass = (args.stepSize >= 1 && args.windowSize >= 1);
    pyramid = args.multiPass;
    stepSize = args.stepSize;
    windowSize = args.windowSize;
    confidence = args.confidence;

    //if the bbox is specified, it takes precedence!
    if (args.bbox.size() != 0) {
        std::vector<double> aoi = args.bbox;
        int zoomLevel = args.zoom;

        auto llCorner = coordinateHelper::deg2num(aoi[1], aoi[0], zoomLevel);
        auto urCorner = coordinateHelper::deg2num(aoi[3], aoi[2], zoomLevel);

        numCols = (int) (urCorner.x - llCorner.x);
        numRows = (int) (llCorner.y - urCorner.y);

        rowStart = (int) (llCorner.y - numRows);
        colStart = (int) (urCorner.x - numCols);
    }

    std::string completedUrl = "";
    switch (args.service){
        case TileSource::MAPS_API:
            completedUrl = WEB_API_URL;
            break;
        case TileSource::EVWHS:
            completedUrl = EVWHS_URL;
            break;
        default:
            completedUrl = DGCS_URL;
    }

    boost::replace_all(completedUrl, "ccc_connect_id", args.token);

    //Loading of the model I'm currently using (200+ MB) takes a couple of seconds.  For that reason, I'm using a single
    //model and loading it prior to any items being processed.
    if(loadModel(args)) {
        return -1;
    }

    classifier = &(model->classifer());

    unique_ptr<VectorFeatureSet> fs(createFeatureSet(args));
    credentials = args.credentials;

    int numThreads = consumer_thread_count * args.numThreads;
    if (numCols < numThreads){
        numThreads = numCols;
    }
    std::cout << (boost::format("Num threads: %1d") % numThreads).str() << std::endl;
    show_progress.reset(new boost::progress_display((numRows * numCols) * 2UL));

    for (long i = rowStart; i < rowStart + numRows; i++) {
        for (long j = colStart; j < colStart + numCols; j++) {
            WorkItem *item = new WorkItem;
            std::string finalUrl = completedUrl;
            if (args.service == TileSource::MAPS_API){
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
            item->geometry() = fs.get();
            downloadQueue.push(item);
        }
        downloadAndProcessRow(numThreads);
    }

    std::cout << "\nNumber of failures: " << failures << std::endl;

    return SUCCESS;
}

