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
#include <OpenSkyNetVersion.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_unique.hpp>
#include <boost/progress.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <classification/GbdxModelReader.h>
#include <classification/NonMaxSuppression.h>
#include <classification/SlidingWindowDetector.h>
#include <deque>
#include <imagery/CvToLog.h>
#include <imagery/GdalImage.h>
#include <imagery/MapBoxClient.h>
#include <imagery/DgcsClient.h>
#include <imagery/EvwhsClient.h>
#include <mutex>
#include <opencv2/core/mat.hpp>
#include <sstream>
#include <utility/User.h>
#include <utility/MultiProgressDisplay.h>

namespace dg { namespace osn {

using namespace dg::deepcore::classification;
using namespace dg::deepcore::imagery;
using namespace dg::deepcore::network;
using namespace dg::deepcore::vector;

using boost::format;
using boost::join;
using boost::lexical_cast;
using boost::make_unique;
using boost::posix_time::from_time_t;
using boost::posix_time::to_simple_string;
using boost::progress_display;
using boost::property_tree::json_parser::write_json;
using boost::property_tree::ptree;
using std::async;
using std::chrono::duration;
using std::chrono::high_resolution_clock;
using std::condition_variable;
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

OpenSkyNet::OpenSkyNet(const OpenSkyNetArgs &args) :
    args_(args)
{
    if(args_.source > Source::LOCAL) {
        cleanup_ = HttpCleanup::get();
    }
}

void OpenSkyNet::process()
{
    if(args_.source > Source::LOCAL) {
        initMapServiceImage();
    } else if(args_.source == Source::LOCAL) {
        initLocalImage();
    } else {
        DG_ERROR_THROW("Input source not specified");
    }

    initModel();
    printModel();
    initFeatureSet();

    if(concurrent_) {
        processConcurrent();
    } else {
        processSerial();
    }

    OSN_LOG(info) << "Saving feature set..." ;
    featureSet_.reset();
}

void OpenSkyNet::initModel()
{
    GbdxModelReader modelReader(args_.modelPath);

    OSN_LOG(info) << "Reading model package..." ;
    unique_ptr<ModelPackage> modelPackage(modelReader.readModel());
    DG_CHECK(modelPackage, "Unable to open the model package");

    OSN_LOG(info) << "Reading model..." ;

    if(args_.windowSize) {
        windowSize_ = *args_.windowSize;
    } else {
        windowSize_ = modelPackage->metadata().windowSize();
    }

    if(args_.action == Action::LANDCOVER) {
        int batchSize;
        if(blockSize_.width % windowSize_.width == 0 && blockSize_.height % windowSize_.height == 0) {
            batchSize = blockSize_.area() / windowSize_.area();
            concurrent_ = true;
        } else {
            cv::Size xySize;
            xySize.width = (int)ceilf((float)blockSize_.width / windowSize_.width);
            xySize.height = (int)ceilf((float)blockSize_.height / windowSize_.height);
            batchSize = xySize.area();
        }

        model_.reset(Model::create(*modelPackage, !args_.useCpu, { BatchSize::BATCH_SIZE, batchSize }));
        stepSize_ = windowSize_;

        auto& slidingWindowDetector = dynamic_cast<SlidingWindowDetector&>(model_->detector());
        slidingWindowDetector.setOverrideSize(windowSize_);
        slidingWindowDetector.setConfidence(0);

    } else if(args_.action == Action::DETECT) {
        model_.reset(Model::create(*modelPackage, !args_.useCpu, { BatchSize::MAX_UTILIZATION,  args_.maxUtitilization / 100 }));

        auto& slidingWindowDetector = dynamic_cast<SlidingWindowDetector&>(model_->detector());

        if(args_.action == Action::DETECT) {
            if(args_.stepSize) {
                stepSize_ = *args_.stepSize;
            } else {
                stepSize_ = slidingWindowDetector.defaultStep();
            }
        }

        slidingWindowDetector.setOverrideSize(windowSize_);
        slidingWindowDetector.setConfidence(args_.confidence / 100);
    }
}

void OpenSkyNet::initLocalImage()
{
    OSN_LOG(info) << "Opening image..." ;
    auto image = make_unique<GdalImage>(args_.image);
    DG_CHECK(!image->spatialReference().isLocal(), "Input image is not geo-registered");

    if(args_.bbox) {
        auto projBbox = image->spatialReference().fromLatLon(*args_.bbox);
        auto pixelBbox = cv::Rect { image->projToPixel(projBbox.tl()), image->projToPixel(projBbox.br()) };
        image->setBbox(pixelBbox);
        blockSize_ = image->blockSize();
    }

    image_.reset(image.release());
}

void OpenSkyNet::initMapServiceImage()
{
    DG_CHECK(args_.bbox, "Bounding box must be specified");

    bool wmts = true;
    string url;
    switch(args_.source) {
        case Source::MAPS_API:
            OSN_LOG(info) << "Connecting to MapsAPI..." ;
            client_ = make_unique<MapBoxClient>(args_.mapId, args_.token);
            wmts = false;
            break;

        case Source ::EVWHS:
            OSN_LOG(info) << "Connecting to EVWHS..." ;
            client_ = make_unique<EvwhsClient>(args_.token, args_.credentials);
            break;

        default:
            OSN_LOG(info) << "Connecting to DGCS..." ;
            client_ = make_unique<DgcsClient>(args_.token, args_.credentials);
            break;
    }

    client_->connect();

    if(wmts) {
        client_->setImageFormat("image/jpeg");
        client_->setLayer("DigitalGlobe:ImageryTileService");
        client_->setTileMatrixSet("EPSG:3857");
        client_->setTileMatrixId((format("EPSG:3857:%1d") % args_.zoom).str());
    } else {
        client_->setTileMatrixId(lexical_cast<string>(args_.zoom));
    }

    client_->setMaxConnections(args_.maxConnections);
    blockSize_ = client_->tileMatrix().tileSize;

    auto projBbox = client_->spatialReference().fromLatLon(*args_.bbox);
    image_.reset(client_->imageFromArea(projBbox, args_.action != Action::LANDCOVER));
}

void OpenSkyNet::initFeatureSet()
{
    OSN_LOG(info) << "Initializing the output feature set..." ;

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
    if(!args_.quiet) {
        progressDisplay.start();
    }

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

    producerFuture.get();
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

    skipLine();
}

void OpenSkyNet::processSerial()
{
    auto& detector = model_->detector();
    DG_CHECK(detector.detectorType() == Detector::SLIDING_WINDOW, "Unsupported model type.");

    auto& slidingWindowDetector = dynamic_cast<SlidingWindowDetector&>(detector);

    skipLine();
    OSN_LOG(info)  << "Reading image...";

    unique_ptr<boost::progress_display> openProgress;
    if(!args_.quiet) {
        openProgress = make_unique<boost::progress_display>(50);
    }

    auto startTime = high_resolution_clock::now();
    auto mat = image_->readImage([&openProgress](float progress) -> bool {
        size_t curProgress = (size_t)roundf(progress*50);
        if(openProgress && openProgress->count() < curProgress) {
            *openProgress += curProgress - openProgress->count();
        }
        return true;
    });

    duration<double> duration = high_resolution_clock::now() - startTime;
    OSN_LOG(info) << "Reading time " << duration.count() << " s";

    Pyramid pyramid;
    if(args_.pyramid) {
        pyramid = Pyramid(mat.size(), windowSize_, stepSize_, 2.0);
    } else {
        pyramid = Pyramid({ { mat.size(), stepSize_ } });
    }

    skipLine();
    OSN_LOG(info) << "Detecting features...";

    unique_ptr<boost::progress_display> detectProgress;
    if(!args_.quiet) {
        detectProgress = make_unique<boost::progress_display>(50);
    }

    startTime = high_resolution_clock::now();
    auto predictions = slidingWindowDetector.detect(mat, pyramid, [&detectProgress](float progress) -> bool {
        size_t curProgress = (size_t)roundf(progress*50);
        if(detectProgress && detectProgress->count() < curProgress) {
            *detectProgress += curProgress - detectProgress->count();
        }
        return true;
    });

    duration = high_resolution_clock::now() - startTime;
    OSN_LOG(info) << "Detection time " << duration.count() << " s" ;

    if(args_.nms) {
        skipLine();
        OSN_LOG(info) << "Performing non-maximum suppression..." ;
        auto filtered = nonMaxSuppression(predictions, args_.overlap / 100);
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

    switch (args_.geometryType) {
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

void OpenSkyNet::printModel()
{
    skipLine();

    const auto& metadata = model_->metadata();


    OSN_LOG(info) << "Model Name: " << metadata.name()
                  << "; Version: " << metadata.version()
                  << "; Created: " << to_simple_string(from_time_t(metadata.timeCreated()));
    OSN_LOG(info) << "Description: " << metadata.description();
    OSN_LOG(info) << "Dimensions (pixels): " << metadata.windowSize()
                  << "; Color Mode: " << metadata.colorMode()
                  << "; Image Type: " << metadata.imageType();
    OSN_LOG(info) << "Bounding box (lat/lon): " << metadata.boundingBox();
    OSN_LOG(info) << "Labels: " << join(metadata.labels(), ", ");

    skipLine();
}

void OpenSkyNet::skipLine() const
{
    if(!args_.quiet) {
        cout << endl;
    }
}

} } // namespace dg { namespace osn {