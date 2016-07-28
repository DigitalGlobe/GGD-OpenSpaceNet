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

#include "OpenSkyNet.h"

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_unique.hpp>
#include <boost/progress.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <classification/GbdxModelReader.h>
#include <classification/NonMaxSuppression.h>
#include <classification/SlidingWindowDetector.h>
#include <deque>
#include <imagery/GdalImage.h>
#include <imagery/MapBoxClient.h>
#include <imagery/WmtsClient.h>
#include <mutex>
#include <opencv2/core/mat.hpp>
#include <sstream>
#include <utility/User.h>
#include <utility/MultiProgressDisplay.h>
#include <version.h>

using namespace dg::deepcore::classification;
using namespace dg::deepcore::imagery;
using namespace dg::deepcore::network;
using namespace dg::deepcore::vector;

using boost::format;
using boost::lexical_cast;
using boost::make_unique;
using boost::progress_display;
using boost::property_tree::json_parser::write_json;
using boost::property_tree::ptree;
using std::async;
using std::chrono::duration;
using std::chrono::high_resolution_clock;
using std::condition_variable;
using std::cerr;
using std::cout;
using std::deque;
using std::endl;
using std::launch;
using std::make_pair;
using std::map;
using std::move;
using std::mutex;
using std::ostringstream;
using std::pair;
using std::string;
using std::vector;
using std::unique_lock;
using std::unique_ptr;
using dg::deepcore::loginUser;
using dg::deepcore::MultiProgressDisplay;

static const string MAPSAPI_MAPID = "digitalglobe.nal0g75k";

static const string DGCS_URL = "https://services.digitalglobe.com/earthservice/wmtsaccess";
static const string EVWHS_URL = "https://evwhs.digitalglobe.com/earthservice/wmtsaccess";

static const map<string, string> DG_WMTS_ARGS = {
        { "FEATUREPROFILE", "Global_Currency_Profile" },
        { "USECLOUDLESSGEOMETRY", "false" }
};

OpenSkyNet::OpenSkyNet(const OpenSkyNetArgs &args) :
    args_(args)
{
    if(args.useTileServer) {
        cleanup_ = HttpCleanup::get();
    }
}

void OpenSkyNet::process()
{
    if(args_.useTileServer) {
        initMapService();
        initModel();
        initMapServiceImage();
    } else {
        initLocalImage();
        initModel();
    }

    initFeatureSet();

    if(concurrent_) {
        processConcurrent();
    } else {
        processSerial();
    }

    cout << "Saving feature set..." << endl;
    featureSet_.reset();
}

void OpenSkyNet::initModel()
{
    GbdxModelReader modelReader(args_.modelPath);

    cout << "Reading model package..." << endl;
    unique_ptr<ModelPackage> modelPackage(modelReader.readModel());
    DG_CHECK(modelPackage, "Unable to open the model package");

    cout << "Reading model..." << endl;

    if(args_.windowSize > 0) {
        windowSize_ = { args_.windowSize, args_.windowSize };
    } else {
        windowSize_ = modelPackage->metadata().windowSize();
    }

    concurrent_ = !args_.multiPass &&
             windowSize_.width == windowSize_.height &&
             windowSize_.width == args_.stepSize &&
             blockSize_.width == blockSize_.height &&
             blockSize_.width % args_.stepSize == 0;

    if(concurrent_) {
        model_.reset(Model::create(*modelPackage, args_.useGPU, { BatchSize::BATCH_SIZE, blockSize_.area() / windowSize_.area() }));
        stepSize_ = { args_.stepSize, args_.stepSize };
    } else {
        model_.reset(Model::create(*modelPackage, args_.useGPU, { BatchSize::MAX_UTILIZATION,  args_.maxUtitilization }));
    }

    auto& slidingWindowDetector = dynamic_cast<SlidingWindowDetector&>(model_->detector());

    if(args_.stepSize) {
        stepSize_ = { args_.stepSize, args_.stepSize };
    } else {
        stepSize_ = slidingWindowDetector.defaultStep();
    }

    slidingWindowDetector.setOverrideSize(windowSize_);
    slidingWindowDetector.setConfidence(args_.confidence);
    slidingWindowDetector.setMaxResults(5);
}

void OpenSkyNet::initLocalImage()
{
    cout << "Opening image..." << endl;
    auto image = make_unique<GdalImage>(args_.image);
    DG_CHECK(!image->spatialReference().isLocal(), "Input image is not geo-registered");

    if(haveBbox()) {
        auto projBbox = image->spatialReference().fromLatLon(bbox());
        auto pixelBbox = cv::Rect { image->projToPixel(projBbox.tl()), image->projToPixel(projBbox.br()) };
        image->setBbox(pixelBbox);
        blockSize_ = image->blockSize();
    }

    image_.reset(image.release());
}

void OpenSkyNet::initMapService()
{
    DG_CHECK(haveBbox(), "Bounding box must be specified");

    bool wmts = true;
    string url;
    switch(args_.service) {
        case TileSource::MAPS_API:
            cout << "Connecting to MapsAPI..." << endl;
            wmts = false;
            break;

        case TileSource ::EVWHS:
            cout << "Connecting to EVWHS..." << endl;
            url = EVWHS_URL;
            break;

        default:
            cout << "Connecting to DGCS..." << endl;
            url = DGCS_URL;
            break;
    }

    if(wmts) {
        client_ = make_unique<WmtsClient>(url, args_.credentials);
        auto args = DG_WMTS_ARGS;
        args["connectId"] = args_.token;
        client_->setAdditionalArguments(args);
        client_->connect();

        client_->setImageFormat("image/jpeg");
        client_->setLayer("DigitalGlobe:ImageryTileService");
        client_->setTileMatrixSet("EPSG:3857");
        client_->setTileMatrixId((format("EPSG:3857:%1d") % args_.zoom).str());
    } else {
        client_ = make_unique<MapBoxClient>(MAPSAPI_MAPID, args_.token);
        client_->connect();

        client_->setTileMatrixId(lexical_cast<string>(args_.zoom));
    }

    client_->setMaxConnections(args_.maxConnections);
    blockSize_ = client_->tileMatrix().tileSize;
}

void OpenSkyNet::initFeatureSet()
{
    cout << "Initializing the output feature set..." << endl;

    FieldDefinitions definitions = {
            { FieldType::STRING, "top_cat", 50 },
            { FieldType::REAL, "top_score" },
            { FieldType::DATE, "date" },
            { FieldType::STRING, "top_five", 254 }
    };

    if(args_.producerInfo) {
        definitions.push_back({ FieldType::STRING, "username", 50 });
        definitions.push_back({ FieldType::STRING, "app", 50 });
        definitions.push_back({ FieldType::STRING, "app_ver", 50 });
    }

    featureSet_ = make_unique<FeatureSet>(args_.outputPath, args_.outputFormat, args_.layerName, definitions);
    outputType_ = args_.geometryType == "POINT" ? GeometryType::POINT : GeometryType::POLYGON;
}

void OpenSkyNet::initMapServiceImage()
{
    auto projBbox = client_->spatialReference().fromLatLon(bbox());
    image_.reset(client_->imageFromArea(projBbox, !concurrent_));
}

void OpenSkyNet::processConcurrent()
{
    auto& detector = model_->detector();
    DG_CHECK(detector.detectorType() == Detector::SLIDING_WINDOW, "Unsupported model type.");

    auto& slidingWindowDetector = dynamic_cast<SlidingWindowDetector&>(detector);

    mutex queueMutex;
    condition_variable cv;
    deque<pair<cv::Point, cv::Mat>> blockQueue;
    bool done = false;

    MultiProgressDisplay progressDisplay({ "Loading", "Classifying" });
    progressDisplay.start();

    auto producerFuture = image_->readImageAsync([&blockQueue, &queueMutex, &cv](const cv::Point& origin, cv::Mat&& block) -> bool {
        unique_lock<mutex> lock(queueMutex);
        blockQueue.push_front(make_pair(origin, std::move(block)));
        queueMutex.unlock();
        cv.notify_all();
        return true;
    }, [&progressDisplay](float progress) -> bool {
        progressDisplay.update(0, progress);
        return true;
    });

    auto numBlocks = (size_t)ceilf((float)image_->size().width / image_->blockSize().width) *
                     (size_t)ceilf((float)image_->size().height / image_->blockSize().height);
    size_t curBlock = 0;

    auto consumerFuture = async(launch::async, [this, &blockQueue, &queueMutex, &cv, &done, &slidingWindowDetector, &progressDisplay, numBlocks, &curBlock]() {
        while(true) {
            pair<cv::Point, cv::Mat> item;
            {
                unique_lock<mutex> lock(queueMutex);
                if(!blockQueue.empty()) {
                    item = blockQueue.back();
                    blockQueue.pop_back();
                } else if(!done) {
                    queueMutex.unlock();
                    cv.wait(lock);
                    continue;
                } else {
                    break;
                }
            }

            Pyramid pyramid({ { item.second.size(), stepSize_ } });
            auto predictions = slidingWindowDetector.detect(item.second, pyramid);
            for(auto& prediction : predictions) {
                prediction.window.x += item.first.x;
                prediction.window.y += item.first.y;
                addFeature(prediction.window, prediction.predictions);
            }

            progressDisplay.update(1, (float)curBlock / numBlocks);
            curBlock++;
        }
    });

    auto status = producerFuture.get();
    if(!status.success) {
        throw status.error;
    }
    progressDisplay.update(0, 1.0F);

    {
        unique_lock<mutex> lock(queueMutex);
        done = true;
        queueMutex.unlock();
        cv.notify_all();
    }

    consumerFuture.wait();
    progressDisplay.update(1, 1.0F);
    progressDisplay.stop();
}

void OpenSkyNet::processSerial()
{
    auto& detector = model_->detector();
    DG_CHECK(detector.detectorType() == Detector::SLIDING_WINDOW, "Unsupported model type.");

    auto& slidingWindowDetector = dynamic_cast<SlidingWindowDetector&>(detector);

    cout << endl << "Reading image...";

    boost::progress_display openProgress(50);
    auto startTime = high_resolution_clock::now();
    auto mat = image_->readImage([&openProgress](float progress) -> bool {
        size_t curProgress = (size_t)roundf(progress*50);
        if(openProgress.count() < curProgress) {
            openProgress += curProgress - openProgress.count();
        }
        return true;
    });

    duration<double> duration = high_resolution_clock::now() - startTime;
    cout << "Reading time " << duration.count() << " s" << endl << endl;

    Pyramid pyramid;
    if(args_.multiPass) {
        pyramid = Pyramid(mat.size(), windowSize_, stepSize_, 2.0);
    } else {
        pyramid = Pyramid({ { mat.size(), stepSize_ } });
    }

    cout << endl << "Detecting features...";

    boost::progress_display detectProgress(50);
    startTime = high_resolution_clock::now();
    auto predictions = slidingWindowDetector.detect(mat, pyramid, [&detectProgress](float progress) -> bool {
        size_t curProgress = (size_t)roundf(progress*50);
        if(detectProgress.count() < curProgress) {
            detectProgress += curProgress - detectProgress.count();
        }
        return true;
    });

    duration = high_resolution_clock::now() - startTime;
    cout << "Detection time " << duration.count() << " s" << endl;

    if(args_.nms) {
        cout << "Performing non-maximum suppression..." << endl;
        auto filtered = nonMaxSuppression(predictions, args_.overlap);
        predictions = move(filtered);
    }

    for(const auto& prediction : predictions) {
        addFeature(prediction.window, prediction.predictions);
    }
}

void OpenSkyNet::addFeature(const cv::Rect &window, const vector<Prediction> &predictions)
{
    if(predictions.empty()) {
        return;
    }

    auto fields = createFeatureFields(predictions);

    switch (outputType_) {
        case GeometryType::POINT:
        {
            cv::Point center(window.x + window.width / 2, window.y + window.height / 2);
            auto point = image_->pixelToLL(center);
            featureSet_->addFeature(PointFeature(point, fields));
        }
            break;

        case GeometryType::POLYGON:
        {
            std::vector<cv::Point> points = {
                    window.tl(),
                    { window.x + window.width, window.y },
                    window.br(),
                    { window.x, window.y + window.height },
                    window.tl()
            };

            std::vector<cv::Point2d> llPoints;
            for(const auto& point : points) {
                auto llPoint = image_->pixelToLL(point);
                llPoints.push_back(llPoint);
            }

            featureSet_->addFeature(PolygonFeature(llPoints, fields));
        }
            break;

        default:
            DG_ERROR_THROW("Invalid output type");
    }
}

Fields OpenSkyNet::createFeatureFields(const vector<Prediction> &predictions) {
    Fields fields = {
            { "top_cat", { FieldType::STRING, predictions[0].label.c_str() } },
            { "top_score", { FieldType ::REAL, predictions[0].confidence } },
            { "date", { FieldType ::DATE, time(nullptr) } }
    };

    ptree top5;
    for(const auto& prediction : predictions) {
        top5.put(prediction.label, prediction.confidence);
    }

    ostringstream oss;
    write_json(oss, top5);

    fields["top_five"] = Field(FieldType::STRING, oss.str());

    if(args_.producerInfo) {
        fields["username"] = { FieldType ::STRING, loginUser() };
        fields["app"] = { FieldType::STRING, "OpenSkyNet"};
        fields["app_ver"] =  { FieldType::STRING, OPENSKYNET_VERSION_STRING };
    }

    return std::move(fields);
}

bool OpenSkyNet::haveBbox()
{
    return !args_.bbox.empty();
}

cv::Rect2d OpenSkyNet::bbox() {
    DG_CHECK(haveBbox(), "Bounding box was not specified");
    return { cv::Point2d { args_.bbox[0], args_.bbox[1] }, cv::Point2d { args_.bbox[2], args_.bbox[3] } };
}
