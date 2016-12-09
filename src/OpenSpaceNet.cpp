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

#include "OpenSpaceNet.h"
#include <OpenSpaceNetVersion.h>

#include <atomic>
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
#include <classification/FilterLabels.h>
#include <deque>
#include <future>
#include <imagery/AffineTransformation.h>
#include <imagery/CvToLog.h>
#include <imagery/GdalImage.h>
#include <imagery/MapBoxClient.h>
#include <imagery/DgcsClient.h>
#include <imagery/EvwhsClient.h>
#include <imagery/TransformationChain.h>
#include <mutex>
#include <opencv2/core/mat.hpp>
#include <sstream>
#include <utility/DcMath.h>
#include <utility/MultiProgressDisplay.h>
#include <utility/Semaphore.h>
#include <utility/User.h>

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
using std::atomic;
using std::async;
using std::chrono::duration;
using std::chrono::high_resolution_clock;
using std::cout;
using std::deque;
using std::endl;
using std::launch;
using std::lock_guard;
using std::make_pair;
using std::map;
using std::move;
using std::recursive_mutex;
using std::ostringstream;
using std::pair;
using std::string;
using std::vector;
using std::unique_ptr;
using dg::deepcore::almostEq;
using dg::deepcore::loginUser;
using dg::deepcore::Semaphore;
using dg::deepcore::MultiProgressDisplay;

OpenSpaceNet::OpenSpaceNet(const OpenSpaceNetArgs &args) :
    args_(args)
{
    if(args_.source > Source::LOCAL) {
        cleanup_ = HttpCleanup::get();
    }
}

void OpenSpaceNet::process()
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

void OpenSpaceNet::initModel()
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

    model_.reset(Model::create(*modelPackage, !args_.useCpu, args_.maxUtilization / 100));

    float confidence = 0;
    if(args_.action == Action::LANDCOVER) {
        const auto& blockSize = image_->blockSize();
        if(blockSize.width % windowSize_.width == 0 && blockSize.height % windowSize_.height == 0) {
            bbox_ = { cv::Point {0, 0} , image_->size() };
            concurrent_ = true;
        } else {
            cv::Size xySize;
            xySize.width = (int)ceilf((float)blockSize.width / windowSize_.width);
            xySize.height = (int)ceilf((float)blockSize.height / windowSize_.height);
        }
        stepSize_ = windowSize_;
    } else if(args_.action == Action::DETECT) {
        auto& slidingWindowDetector = dynamic_cast<SlidingWindowDetector&>(model_->detector());

        if(args_.action == Action::DETECT) {
            if(args_.stepSize) {
                stepSize_ = *args_.stepSize;
            } else {
                stepSize_ = slidingWindowDetector.defaultStep();
            }
        }

        confidence = args_.confidence / 100;
    }

    auto& slidingWindowDetector = dynamic_cast<SlidingWindowDetector&>(model_->detector());
    slidingWindowDetector.setOverrideSize(windowSize_);
    slidingWindowDetector.setConfidence(confidence);
}

void OpenSpaceNet::initLocalImage()
{
    OSN_LOG(info) << "Opening image..." ;
    image_ = make_unique<GdalImage>(args_.image);
    DG_CHECK(!image_->spatialReference().isLocal(), "Input image is not geo-registered");

    bbox_ = cv::Rect{ { 0, 0 }, image_->size() };

    TransformationChain llToPixel {
        image_->spatialReference().fromLatLon(),
        image_->pixelToProj().inverse()
    };

    pixelToLL_ = llToPixel.inverse();

    if(args_.bbox) {
        auto bbox = llToPixel.transformToInt(*args_.bbox);

        auto intersect = bbox_ & (cv::Rect)bbox;
        DG_CHECK(intersect.width && intersect.height, "Input image and the provided bounding box do not intersect");

        if(bbox != intersect) {
            auto llIntersect = pixelToLL_->transform(intersect);
            OSN_LOG(info) << "Bounding box adjusted to " << llIntersect.tl() << " : " << llIntersect.br();
        }

        bbox_ = intersect;
    }
}

void OpenSpaceNet::initMapServiceImage()
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

        case Source::TILE_JSON:
            OSN_LOG(info) << "Connecting to TileJSON...";
            client_ = make_unique<TileJsonClient>(args_.url, args_.credentials, args_.useTiles);
            wmts = false;
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

    unique_ptr<Transformation> llToProj(client_->spatialReference().fromLatLon());
    auto projBbox = llToProj->transform(*args_.bbox);
    image_.reset(client_->imageFromArea(projBbox));

    unique_ptr<Transformation> projToPixel(image_->pixelToProj().inverse());
    bbox_ = projToPixel->transformToInt(projBbox);
    pixelToLL_ = TransformationChain { std::move(llToProj), std::move(projToPixel) }.inverse();

    auto msImage = dynamic_cast<MapServiceImage*>(image_.get());
    msImage->setMaxConnections(args_.maxConnections);
}

void OpenSpaceNet::initFeatureSet()
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

    VectorOpenMode openMode = args_.append ? APPEND : OVERWRITE;

    featureSet_ = make_unique<FeatureSet>(args_.outputPath, args_.outputFormat, args_.layerName, definitions, openMode);
}

void OpenSpaceNet::processConcurrent()
{
    auto& detector = model_->detector();
    DG_CHECK(detector.detectorType() == Detector::SLIDING_WINDOW, "Unsupported model type.");

    auto& slidingWindowDetector = dynamic_cast<SlidingWindowDetector&>(detector);

    recursive_mutex queueMutex;
    deque<pair<cv::Point, cv::Mat>> blockQueue;
    Semaphore haveWork;
    atomic<bool> cancelled = ATOMIC_VAR_INIT(false);

    MultiProgressDisplay progressDisplay({ "Loading", "Classifying" });
    if(!args_.quiet) {
        progressDisplay.start();
    }

    auto numBlocks = image_->numBlocks().area();
    size_t curBlockRead = 0;
    image_->setReadFunc([&blockQueue, &queueMutex, &haveWork, &curBlockRead, numBlocks, &progressDisplay](const cv::Point& origin, cv::Mat&& block) -> bool {
        {
            lock_guard<recursive_mutex> lock(queueMutex);
            blockQueue.push_front(make_pair(origin, std::move(block)));
        }

        progressDisplay.update(0, (float)++curBlockRead / numBlocks);
        haveWork.notify();

        return true;
    });

    image_->setOnError([&cancelled, &haveWork](std::exception_ptr) {
        cancelled.store(true);
        haveWork.notify();
    });

    image_->readBlocksInAoi();

    size_t curBlockClass = 0;
    auto consumerFuture = async(launch::async, [this, &blockQueue, &queueMutex, &haveWork, &cancelled, &slidingWindowDetector, &progressDisplay, numBlocks, &curBlockRead, &curBlockClass]() {
        while(curBlockClass < numBlocks && !cancelled.load()) {
            pair<cv::Point, cv::Mat> item;
            {
                lock_guard<recursive_mutex> lock(queueMutex);
                if(!blockQueue.empty()) {
                    item = blockQueue.back();
                    blockQueue.pop_back();
                } else {
                    queueMutex.unlock();
                    haveWork.wait();
                    continue;
                }
            }

            Pyramid pyramid({ { item.second.size(), stepSize_ } });
            auto predictions = slidingWindowDetector.detect(item.second, pyramid);

            if(!args_.excludeLabels.empty()) {
                std::set<string> excludeLabels(args_.excludeLabels.begin(), args_.excludeLabels.end());
                auto filtered = filterLabels(predictions, FilterType::Exclude, excludeLabels);
                predictions = move(filtered);
            }

            if(!args_.includeLabels.empty()) {
                std::set<string> includeLabels(args_.includeLabels.begin(), args_.includeLabels.end());
                auto filtered = filterLabels(predictions, FilterType::Include, includeLabels);
                predictions = move(filtered);
            }

            for(auto& prediction : predictions) {
                prediction.window.x += item.first.x;
                prediction.window.y += item.first.y;
                addFeature(prediction.window, prediction.predictions);
            }

            progressDisplay.update(1, (float)++curBlockClass / numBlocks);
        }
    });

    consumerFuture.wait();
    progressDisplay.stop();

    image_->rethrowIfError();

    skipLine();
}

void OpenSpaceNet::processSerial()
{
    auto& detector = model_->detector();
    DG_CHECK(detector.detectorType() == Detector::SLIDING_WINDOW, "Unsupported model type.");

    auto& slidingWindowDetector = dynamic_cast<SlidingWindowDetector&>(detector);

    // Adjust the transformation to shift to the bounding box
    auto& pixelToLL = dynamic_cast<TransformationChain&>(*pixelToLL_);
    pixelToLL.chain.push_front(new AffineTransformation {
        (double) bbox_.x, 1.0, 0.0,
        (double) bbox_.y, 0.0, 1.0
    });
    pixelToLL.compact();

    skipLine();
    OSN_LOG(info)  << "Reading image...";

    unique_ptr<boost::progress_display> openProgress;
    if(!args_.quiet) {
        openProgress = make_unique<boost::progress_display>(50);
    }

    auto startTime = high_resolution_clock::now();
    auto mat = GeoImage::readImage(*image_, bbox_, [&openProgress](float progress) -> bool {
        size_t curProgress = (size_t)roundf(progress*50);
        if(openProgress && openProgress->count() < curProgress) {
            *openProgress += curProgress - openProgress->count();
        }
        return true;
    });

    duration<double> duration = high_resolution_clock::now() - startTime;
    OSN_LOG(info) << "Reading time " << duration.count() << " s";

    skipLine();
    OSN_LOG(info) << "Detecting features...";

    unique_ptr<boost::progress_display> detectProgress;
    if(!args_.quiet) {
        detectProgress = make_unique<boost::progress_display>(50);
    }

    startTime = high_resolution_clock::now();
    auto predictions = slidingWindowDetector.detect(mat, calcPyramid(), [&detectProgress](float progress) -> bool {
        size_t curProgress = (size_t)roundf(progress*50);
        if(detectProgress && detectProgress->count() < curProgress) {
            *detectProgress += curProgress - detectProgress->count();
        }
        return true;
    });

    duration = high_resolution_clock::now() - startTime;
    OSN_LOG(info) << "Detection time " << duration.count() << " s" ;

    if(!args_.excludeLabels.empty()) {
        skipLine();
        OSN_LOG(info) << "Performing category filtering..." ;
        std::set<string> excludeLabels(args_.excludeLabels.begin(), args_.excludeLabels.end());
        auto filtered = filterLabels(predictions, FilterType::Exclude, excludeLabels);
        predictions = move(filtered);
    }

    if(!args_.includeLabels.empty()) {
        skipLine();
        OSN_LOG(info) << "Performing category filtering..." ;
        std::set<string> includeLabels(args_.includeLabels.begin(), args_.includeLabels.end());
        auto filtered = filterLabels(predictions, FilterType::Include, includeLabels);
        predictions = move(filtered);
    }

    if(args_.nms) {
        skipLine();
        OSN_LOG(info) << "Performing non-maximum suppression..." ;
        auto filtered = nonMaxSuppression(predictions, args_.overlap / 100);
        predictions = move(filtered);
    }

    OSN_LOG(info) << predictions.size() << " features detected.";

    for(const auto& prediction : predictions) {
        addFeature(prediction.window, prediction.predictions);
    }
}

void OpenSpaceNet::addFeature(const cv::Rect &window, const vector<Prediction> &predictions)
{
    if(predictions.empty()) {
        return;
    }

    auto fields = createFeatureFields(predictions);

    switch (args_.geometryType) {
        case GeometryType::POINT:
        {
            cv::Point center(window.x + window.width / 2, window.y + window.height / 2);
            auto point = pixelToLL_->transform(center);
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
                auto llPoint = pixelToLL_->transform(point);
                llPoints.push_back(llPoint);
            }

            featureSet_->addFeature(PolygonFeature(llPoints, fields));
        }
            break;

        default:
            DG_ERROR_THROW("Invalid output type");
    }
}

Fields OpenSpaceNet::createFeatureFields(const vector<Prediction> &predictions) {
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
        fields["app"] = { FieldType::STRING, "OpenSpaceNet"};
        fields["app_ver"] =  { FieldType::STRING, OPENSPACENET_VERSION_STRING };
    }

    return std::move(fields);
}

void OpenSpaceNet::printModel()
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

void OpenSpaceNet::skipLine() const
{
    if(!args_.quiet) {
        cout << endl;
    }
}

Pyramid OpenSpaceNet::calcPyramid() const
{
    Pyramid pyramid;
    if(args_.pyramidWindowSizes.empty()) {
        if(args_.pyramid) {
            pyramid = Pyramid(bbox_.size(), windowSize_, stepSize_, 2.0);
        } else {
            pyramid = Pyramid({ { bbox_.size(), stepSize_ } });
        }
    } else {
        // Sanity check, should've been caught before
        DG_CHECK(args_.pyramidWindowSizes.size() == args_.pyramidStepSizes.size(), "Pyramid window sizes don't match step sizes.");

        auto imageSize = bbox_.size();
        const auto& metadata = model_->metadata();

        for(size_t i = 0; i < args_.pyramidWindowSizes.size(); ++i) {
            const auto& windowSize = args_.pyramidWindowSizes[i];
            const auto& modelSize = std::max(metadata.windowSize().width, metadata.windowSize().height);
            auto scale = (double) modelSize / windowSize;
            auto stepSize = (int)round(args_.pyramidStepSizes[i] * scale);
            auto width = (int)round(imageSize.width * scale);
            auto height = (int)round(imageSize.height * scale);

            pyramid.levels().push_back({width, height, stepSize, stepSize });
        }
    }

    return std::move(pyramid);
}

} } // namespace dg { namespace osn {