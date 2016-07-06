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
#include <thread>
#include <condition_variable>

#include <iostream>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/progress.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>


#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <DeepCore/classification/Classifier.h>
#include <DeepCore/classification/GbdxModelReader.h>
#include <DeepCore/classification/Model.h>
#include <DeepCore/classification/SlidingWindowDetector.h>
#include <DeepCore/imagery/GeoImage.h>
#include <DeepCore/imagery/ImageDownloader.h>
#include <DeepCore/imagery/MapBoxClient.h>
#include <DeepCore/imagery/XyzCoords.h>
#include <DeepCore/utility/Math.h>
#include <curlpp/cURLpp.hpp>
#include <caffe/caffe.hpp>
#include <curlpp/Exception.hpp>
#include <utility/Error.h>
#include <utility/User.h>
#include <version.h>
#include <future>

using namespace dg::deepcore;
using namespace dg::deepcore::classification;
using namespace dg::deepcore::imagery;
using namespace dg::deepcore::vector;

using namespace std;

using boost::property_tree::json_parser::write_json;
using boost::property_tree::ptree;
using chrono::high_resolution_clock;
using chrono::duration;
using dg::deepcore::loginUser;

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
bool producerInfo = true;

unique_ptr<boost::progress_display> show_progress;

static const string WEB_API_URL = "http://a.tiles.mapbox.com/v4/digitalglobe.nmmhkk79/{z}/{x}/{y}.jpg?access_token=ccc_connect_id";
static const string DGCS_URL = "https://services.digitalglobe.com/earthservice/wmtsaccess?connectId=ccc_connect_id&version=1.0.0&request=GetTile&service=WMTS&Layer=DigitalGlobe:ImageryTileService&tileMatrixSet=EPSG:3857&tileMatrix=EPSG:3857:18&format=image/jpeg&FEATUREPROFILE=Global_Currency_Profile&USECLOUDLESSGEOMETRY=false";
static const string EVWHS_URL = "https://evwhs.digitalglobe.com/earthservice/wmtsaccess?connectId=ccc_connect_id&version=1.0.0&request=GetTile&service=WMTS&Layer=DigitalGlobe:ImageryTileService&tileMatrixSet=EPSG:3857&tileMatrix=EPSG:3857:18&format=image/jpeg&FEATUREPROFILE=Global_Currency_Profile&USECLOUDLESSGEOMETRY=false";

Fields createFeatureFields(const std::vector<Prediction> &predictions, const cv::Point3i& tile, int modelId)
{
    Fields fields = {
            { "top_cat", { FieldType::STRING, predictions[0].label.c_str() } },
            { "top_score", { FieldType ::REAL, predictions[0].confidence } },
            { "x_tile", { FieldType ::INTEGER, tile.x } },
            { "y_tile", { FieldType ::INTEGER, tile.y } },
            { "zoom_level", { FieldType ::INTEGER, tile.z } },
            { "date", { FieldType ::DATE, time(nullptr) } },
            { "model_id", { FieldType ::INTEGER, modelId } }
    };

    ptree top5;
    for(const auto& prediction : predictions) {
        top5.put(prediction.label, prediction.confidence);
    }

    ostringstream oss;
    write_json(oss, top5);

    fields["top_five"] = Field(FieldType::STRING, oss.str());

    if(producerInfo) {
        fields["username"] = { FieldType ::STRING, loginUser() };
        fields["app"] = { FieldType::STRING, "OpenSkyNet"};
        fields["app_ver"] =  { FieldType::STRING, OPENSKYNET_VERSION_STRING };
    }

    return std::move(fields);
}

void persistResults(std::vector<Prediction> &predictions, WorkItem *item, const cv::Rect *rect = nullptr) {
    if (predictions.size() > 0) {
        Fields fields = createFeatureFields(predictions, item->tile(), 0);

        if (outputType == GeometryType::POINT) {
            cv::Point2d pointLL;
            cv::Point center;
            if (rect == nullptr) {
                center = { 128, 128 };
            } else {
                center = { rect->x + rect->width / 2, rect->y + rect->height / 2 };
            }

            pointLL = degrees(xyz::tileToLL(item->tile(), center));
            item->geometry()->addFeature(PointFeature(pointLL, fields));
         }
        else {
            std::vector<cv::Point2d> geometry;
            if (rect == nullptr) {
                geometry.push_back(degrees(xyz::tileToLL(item->tile(), {0, 0})));
                geometry.push_back(degrees(xyz::tileToLL(item->tile(), {255, 0})));
                geometry.push_back(degrees(xyz::tileToLL(item->tile(), {255, 255})));
                geometry.push_back(degrees(xyz::tileToLL(item->tile(), {0, 255})));
                geometry.push_back(degrees(xyz::tileToLL(item->tile(), {0, 0})));
             } else {
                auto tl = rect->tl();
                auto br = rect->br();
                geometry.push_back(degrees(xyz::tileToLL(item->tile(), tl)));
                geometry.push_back(degrees(xyz::tileToLL(item->tile(), { br.x, tl.y })));
                geometry.push_back(degrees(xyz::tileToLL(item->tile(), br)));
                geometry.push_back(degrees(xyz::tileToLL(item->tile(), { tl.x, br.y })));
                geometry.push_back(degrees(xyz::tileToLL(item->tile(), tl)));
            }
            item->geometry()->addFeature(PolygonFeature(geometry, fields));
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

    if (item->retryCount() < 5){

        try {
            auto image = ImageDownloader::download(item->url(), credentials);
            item->addImage(image);
            if (pyramid) { //downscale the images
                item->pyramid(scale, 30);
            }
            classificationQueue.push(item);
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

FeatureSet* createFeatureSet(const OpenSkyNetArgs& args) {
    FieldDefinitions definitions = {
        { FieldType::STRING, "top_cat", 50 },
        { FieldType::REAL, "top_score" },
        { FieldType::INTEGER, "zoom_level" },
        { FieldType::INTEGER, "x_tile" },
        { FieldType::INTEGER, "y_tile" },
        { FieldType::INTEGER, "model_id" },
        { FieldType::DATE, "date" },
        { FieldType::STRING, "top_five", 254 }
    };

    producerInfo = args.producerInfo;
    if(producerInfo) {
        definitions.push_back({ FieldType::STRING, "username", 50 });
        definitions.push_back({ FieldType::STRING, "app", 50 });
        definitions.push_back({ FieldType::STRING, "app_ver", 50 });
    }

    unique_ptr<FeatureSet> fs(new FeatureSet(args.outputPath, args.outputFormat, args.layerName, definitions));
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

int addFeature(FeatureSet& fs, const GeoImage& image, const cv::Rect& window, const std::vector<Prediction>& predictions)
{
    if(predictions.empty()) {
        return SUCCESS;
    }

    auto fields = createFeatureFields(predictions, {-1, -1, 0}, 0);

    if(outputType == GeometryType::POINT) {
        cv::Point center(window.x + window.width / 2, window.y + window.height / 2);
        auto point = image.imageToLL(center);
        fs.addFeature(PointFeature(point, fields));
    } else if(outputType == GeometryType::POLYGON){
        std::vector<cv::Point> points = {
                window.tl(),
                { window.x + window.width, window.y },
                window.br(),
                { window.x, window.y + window.height },
                window.tl()
        };

        std::vector<cv::Point2d> llPoints;
        for(const auto& point : points) {
            auto llPoint = image.imageToLL(point);
            llPoints.push_back(llPoint);
        }

        fs.addFeature(PolygonFeature(llPoints, fields));
    } else {
        cerr << "Invalid output type." << endl;
        return -1;
    }

    return SUCCESS;
}

int classifyFromFile(OpenSkyNetArgs &args) {
    cout << endl << "Reading image...";
    GeoImage geoImage(args.image);

    cv::Rect2d bbox;
    if(args.bbox.size() == 4) {
        bbox = cv::Rect(cv::Point2d(args.bbox[0], args.bbox[1]), cv::Point2d(args.bbox[2], args.bbox[3]));
    }

    if(!geoImage.projection().empty()) {
        auto ul = geoImage.imageToLL({0, 0});
        auto lr = geoImage.imageToLL(geoImage.size());
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
    auto image = geoImage.readImage([&openProgress](float progress, const char*) -> bool {
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

        unique_ptr<FeatureSet> fs(createFeatureSet(args));
        addFeature(*fs, geoImage, cv::Rect({ 0, 0 } , image.size()), predictions);
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

        unique_ptr<FeatureSet> fs(createFeatureSet(args));

        for(const auto& prediction : predictions) {
            addFeature(*fs, geoImage, prediction.window, prediction.predictions);
        }
    }

    return SUCCESS;
}

int classifyFromMapsApi(OpenSkyNetArgs &args) {
    cout << "Connecting to MapsAPI..." << endl;
    MapBoxClient client("digitalglobe.nmmhkk79", args.token);
    client.connect();

    GbdxModelReader modelReader(args.modelPath);

    cout << "Reading model package..." << endl;
    unique_ptr<ModelPackage> modelPackage(modelReader.readModel());
    if (modelPackage == nullptr){
        cerr << "Unable to generate model from the provided package." << endl;
        return -1;
    }

    cout << "Reading model..." << endl;
    const auto& metadata = modelPackage->metadata();

    unique_ptr<FeatureSet> fs(createFeatureSet(args));

    if(args.stepSize && !args.multiPass &&
        metadata.windowSize().width == metadata.windowSize().height &&
        metadata.windowSize().width == args.stepSize && 256 % args.stepSize == 0)
    {

        double prog1 = 0, prog2 = 0;
        cout << "Downloaded Processed" << endl;
        cout << fixed << setprecision(1);
        cout << '\r' << prog1*100 << "%     " << prog2*100 << "%     ";

        // Landcover
        model = Model::create(*modelPackage, args.useGPU, { BatchSize::BATCH_SIZE, 256 * 256 / metadata.windowSize().area() });
        auto& slidingWindowDetector = dynamic_cast<SlidingWindowDetector&>(model->detector());
        Pyramid pyramid({ { cv::Size {256, 256}, cv::Point { args.stepSize, args.stepSize } } });

        cv::Rect tiles;
        if(args.bbox.size()) {
            std::vector<double> aoi = args.bbox;
            auto bl = xyz::lltoTile(radians(cv::Point2d { aoi[0], aoi[1] }), args.zoom);
            auto tr = xyz::lltoTile(radians(cv::Point2d { aoi[2], aoi[3] }), args.zoom);
            tiles = { cv::Point { bl.x, bl.y }, cv::Point { tr.x, tr.y } };
        } else if (args.columnSpan != 0 && args.rowSpan != 0) {
            tiles = { (int)args.startColumn, (int)args.startRow, (int)args.columnSpan, (int)args.rowSpan };
        } else {
            cerr << "Bounding box was not specified" << endl;
            return -1;
        }

        auto downloader = client.downloadAsync(tiles, args.zoom, 0);

        mutex queueMutex;
        condition_variable cv;
        deque<pair<cv::Point3i, cv::Mat>> tileQueue;
        auto maxConnections = (int)args.numThreads;
        auto producer = thread([&downloader, &tileQueue, &maxConnections, &cv, &queueMutex, &prog1, &prog2]() {
            downloader.downloadAsync([&tileQueue, &queueMutex, &cv, &prog1, &prog2](cv::Mat&& image, const cv::Point3i& tile) {
                unique_lock<mutex> lock(queueMutex);
                tileQueue.push_front(make_pair(tile, std::move(image)));
                queueMutex.unlock();
                cv.notify_all();
            }, [&prog1, &prog2](double progress) {
                prog1 = progress;
                cout << fixed << setprecision(1);
                cout << '\r' << prog1*100 << "%     " << prog2*100 << "%     ";
            }, maxConnections);
        });

        producer.detach();

        int cur = 0, of = (tiles.width + 1) * (tiles.height + 1);
        while(cur < of) {
            pair<cv::Point3i, cv::Mat> item;

            {
                unique_lock<mutex> lock(queueMutex);
                if(!tileQueue.empty()) {
                    item = tileQueue.back();
                    tileQueue.pop_back();
                } else {
                    queueMutex.unlock();
                    cv.wait(lock);
                    continue;
                }
            }

            auto predictions = slidingWindowDetector.detect(item.second, pyramid, args.confidence);
            for(const auto& prediction : predictions) {
                auto fields = createFeatureFields(prediction.predictions, {-1, -1, 0}, 0);

                const auto& window = prediction.window;
                if(outputType == GeometryType::POINT) {
                    cv::Point center(window.x + window.width / 2, window.y + window.height / 2);
                    auto point = xyz::tileToLL(item.first, center, {256, 256});
                    fs->addFeature(PointFeature(point, fields));
                } else if(outputType == GeometryType::POLYGON) {
                    std::vector<cv::Point> points = {
                            window.tl(),
                            {window.x + window.width, window.y},
                            window.br(),
                            {window.x, window.y + window.height},
                            window.tl()
                    };

                    std::vector<cv::Point2d> llPoints;
                    for (const auto &point : points) {
                        auto llPoint = xyz::tileToLL(item.first, point, {256, 256});
                        llPoints.push_back(degrees(llPoint));
                    }

                    fs->addFeature(PolygonFeature(llPoints, fields));
                }
            }

            cur++;
            prog2 = (double)cur / of;
            cout << fixed << setprecision(1);
            cout << '\r' << prog1*100 << "%     " << prog2*100 << "%     ";
        }
        cout << endl;
    } else {
        // Detection
        model = Model::create(*modelPackage, args.useGPU, { BatchSize::MAX_UTILIZATION,  args.maxUtitilization });

        auto& slidingWindowDetector = dynamic_cast<SlidingWindowDetector&>(model->detector());
        cv::Point step;
        if(args.stepSize) {
            step = { args.stepSize, args.stepSize };
        } else {
            step = slidingWindowDetector.defaultStep();
        }

        cv::Rect2d area;
        if(args.bbox.size()) {
            std::vector<double> aoi = args.bbox;
            area = {cv::Point2d(aoi[0], aoi[1]), cv::Point2d {aoi[2], aoi[3]}};
        }

        cout << endl << "Downloading image...";
        boost::progress_display openProgress(50);
        auto image = client.downloadArea(area, args.zoom, 0, true, [&openProgress](float progress) {
            size_t curProgress = (size_t)roundf(progress*50);
            if(openProgress.count() < curProgress) {
                openProgress += curProgress - openProgress.count();
            }
        }, (int)args.numThreads);
        cout << endl;

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

        auto tl = radians( cv::Point2d {area.x, area.br().y} );
        auto tlTile = xyz::lltoTile(tl, args.zoom);
        auto tlOffset = xyz::llToPixel(tl, tlTile, {256, 256});

        for(const auto& prediction : predictions) {
            auto fields = createFeatureFields(prediction.predictions, {-1, -1, 0}, 0);

            const auto& window = prediction.window;
            if(outputType == GeometryType::POINT) {
                cv::Point center(window.x + window.width / 2, window.y + window.height / 2);
                auto point = xyz::pixelToLL(center, tlTile, tlOffset, { 256, 256 });
                fs->addFeature(PointFeature(point, fields));
            } else if(outputType == GeometryType::POLYGON){
                std::vector<cv::Point> points = {
                        window.tl(),
                        { window.x + window.width, window.y },
                        window.br(),
                        { window.x, window.y + window.height },
                        window.tl()
                };

                std::vector<cv::Point2d> llPoints;
                for(const auto& point : points) {
                    auto llPoint = xyz::pixelToLL(point, tlTile, tlOffset, { 256, 256 });
                    llPoints.push_back(degrees(llPoint));
                }

                fs->addFeature(PolygonFeature(llPoints, fields));
            } else {
                cerr << "Invalid output type." << endl;
                return -1;
            }
        }
    }

    return SUCCESS;
}

int classifyBroadAreaMultiProcess(OpenSkyNetArgs &args) {
    curlpp::Cleanup cleaner;
    long numCols = 0, numRows = 0, rowStart = 0, colStart = 0;

    if(args.service == TileSource::MAPS_API && !args.old) {
        return classifyFromMapsApi(args);
    }

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
    producerInfo = args.producerInfo;

    //if the bbox is specified, it takes precedence!
    if (args.bbox.size() != 0) {
        std::vector<double> aoi = args.bbox;
        int zoomLevel = args.zoom;

        auto llCorner = xyz::lltoTile(radians(cv::Point2d { aoi[0], aoi[1] }), zoomLevel);
        auto urCorner = xyz::lltoTile(radians(cv::Point2d { aoi[2], aoi[3] }), zoomLevel);

        numCols = urCorner.x - llCorner.x;
        numRows = llCorner.y - urCorner.y;

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

    unique_ptr<FeatureSet> fs(createFeatureSet(args));
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
            item->setTile({ (int)j, (int)i, args.zoom});
            item->geometry() = fs.get();
            downloadQueue.push(item);
        }
        downloadAndProcessRow(numThreads);
    }

    std::cout << "\nNumber of failures: " << failures << std::endl;

    return SUCCESS;
}

