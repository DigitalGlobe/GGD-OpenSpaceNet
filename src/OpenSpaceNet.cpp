/********************************************************************************
* Copyright 2017 DigitalGlobe, Inc.
* Author: Joe White
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
********************************************************************************/

#include "OpenSpaceNet.h"
#include <OpenSpaceNetVersion.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/range/combine.hpp>
#include <boost/date_time.hpp>
#include <boost/format.hpp>
#include <boost/make_unique.hpp>
#include <boost/progress.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <classification/GbdxModelReader.h>
#include <classification/CaffeModelIdentifier.h>
#include <classification/CaffeModelPackage.h>
#include <future>
#include <geometry/AffineTransformation.h>
#include <geometry/CvToLog.h>
#include <geometry/FilterLabels.h>
#include <geometry/NonMaxSuppression.h>
#include <geometry/PassthroughRegionFilter.h>
#include <geometry/TransformationChain.h>
#include <imagery/GdalImage.h>
#include <imagery/MapBoxClient.h>
#include <imagery/SlidingWindowChipper.h>
#include <imagery/DgcsClient.h>
#include <imagery/EvwhsClient.h>
#include <network/Url.h>
#include <utility/MultiProgressDisplay.h>
#include <utility/Semaphore.h>
#include <utility/User.h>
#include <vector/FileFeatureSet.h>
#include <vector/WfsFeatureSet.h>

#include <geometry/GeosCompat_define.h>
#include <geos/geom/GeometryFactory.h>
#include <geometry/GeosCompat_undef.h>

namespace dg { namespace osn {

using namespace dg::deepcore::classification;
using namespace dg::deepcore::geometry;
using namespace dg::deepcore::imagery;
using namespace dg::deepcore::network;
using namespace dg::deepcore::vector;

using boost::copy;
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
using std::back_inserter;
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
using std::to_string;
using std::vector;
using std::unique_ptr;
using dg::deepcore::loginUser;
using dg::deepcore::Semaphore;
using dg::deepcore::MultiProgressDisplay;

namespace gg = geos::geom;
auto deleter_ = [](gg::Geometry* geometry) {
                    gg::GeometryFactory::getDefaultInstance()->destroyGeometry(geometry);
                };

using GGGeometryPtr = unique_ptr<gg::Geometry, decltype(deleter_)>;

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

    // Adjust the transformation to shift to the bounding box
    auto& pixelToLL = dynamic_cast<TransformationChain&>(*pixelToLL_);
    pixelToLL.chain.push_front(new AffineTransformation {
            (double) bbox_.x, 1.0, 0.0,
            (double) bbox_.y, 0.0, 1.0
    });

    initModel();
    printModel();
    initFilter();
    initFeatureSet();
    if (args_.dgcsCatalogID || args_.evwhsCatalogID) {
        initWfs();
    }

    pixelToLL.compact();

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
    ModelPackage::registerFactory<CaffeModelPackage, CaffeModelIdentifier>();
    GbdxModelReader modelReader(args_.modelPath);

    OSN_LOG(info) << "Reading model package..." ;
    unique_ptr<ModelPackage> modelPackage(modelReader.readModel());
    DG_CHECK(modelPackage, "Unable to open the model package");

    OSN_LOG(info) << "Reading model..." ;

    model_ = Model::create(*modelPackage, !args_.useCpu, args_.maxUtilization / 100);
    modelAspectRatio_ = (float) model_->metadata().modelSize().height / model_->metadata().modelSize().width;

    float confidence = 0;
    if(args_.action == Action::LANDCOVER) {
        const auto& blockSize = image_->blockSize();
        auto windowSize = calcPrimaryWindowSize();
        if (args_.windowSize.size() < 2) {
            if(blockSize.width % windowSize.width == 0 &&
               blockSize.height % windowSize.height == 0) {
                bbox_ = {cv::Point {0, 0}, image_->size()};
                concurrent_ = true;
            }
        }
    } else if(args_.action == Action::DETECT) {
        confidence = args_.confidence / 100;
    }

    model_->setConfidence(confidence);

    DG_CHECK(!args_.resampledSize || *args_.resampledSize <= model_->metadata().modelSize().width,
             "Argument --resample-size (size: %d) does not fit within the model (width: %d).",
             *args_.resampledSize, model_->metadata().modelSize().width)

    if (!args_.resampledSize) {
        for (auto c : args_.windowSize) {
            DG_CHECK(c <= model_->metadata().modelSize().width,
                     "Argument --window-size contains a size that does not fit within the model (width: %d).",
                      model_->metadata().modelSize().width)
        }
    }
}

void OpenSpaceNet::initLocalImage()
{
    OSN_LOG(info) << "Opening image..." ;
    image_ = make_unique<GdalImage>(args_.image);

    bbox_ = cv::Rect{ { 0, 0 }, image_->size() };
    bool ignoreArgsBbox = false;

    TransformationChain llToPixel;
    if (!image_->spatialReference().isLocal()) {
        llToPixel = {
                image_->spatialReference().fromLatLon(),
                image_->pixelToProj().inverse()
        };
        sr_ = SpatialReference::WGS84;
    } else {
        OSN_LOG(warning) << "Image has geometric metadata which cannot be converted to WGS84.  "
                            "Output will be in native space, and some output formats will fail.";

        if (args_.bbox) {
            OSN_LOG(warning) << "Supplying the --bbox option implicitly requests a conversion from "
                                "WGS84 to pixel space however there is no conversion from WGS84 to "
                                "pixel space.";
            OSN_LOG(warning) << "Ignoring user-supplied bounding box";

            ignoreArgsBbox = true;
        }

        llToPixel = { image_->pixelToProj().inverse() };
    }

    pixelToLL_ = llToPixel.inverse();

    if(args_.bbox && !ignoreArgsBbox) {
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
    sr_ = SpatialReference::WGS84;

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

    if(args_.dgcsCatalogID || args_.evwhsCatalogID) {
        definitions.push_back({FieldType::STRING, "catalog_id"});
    }

    VectorOpenMode openMode = args_.append ? APPEND : OVERWRITE;

    featureSet_ = make_unique<FileFeatureSet>(args_.outputPath, args_.outputFormat, openMode);

    if (openMode == OVERWRITE) {
        layer_ = featureSet_->createLayer(args_.layerName, sr_, args_.geometryType, definitions);
    } else if (openMode == APPEND) {
        if (featureSet_->hasLayer(args_.layerName)) {
            layer_ = featureSet_->layer(args_.layerName);
        } else {
            layer_ = featureSet_->createLayer(args_.layerName, sr_, args_.geometryType, definitions);
        }
    }
}

void OpenSpaceNet::initFilter()
{
    if (args_.filterDefinition.size()) {
        OSN_LOG(info) << "Initializing the region filter..." ;

        regionFilter_ = make_unique<MaskedRegionFilter>(cv::Rect(0, 0, bbox_.width, bbox_.height),
                                                        calcPrimaryWindowStep(),
                                                        MaskedRegionFilter::FilterMethod::ANY);
        bool firstAction = true;
        for (const auto& filterAction : args_.filterDefinition) {
            string action = filterAction.first;
            std::vector<Polygon> filterPolys;
            for (const auto& filterFile : filterAction.second) {
                FileFeatureSet filter(filterFile);
                for (auto& layer : filter) {
                    auto pixelToProj = dynamic_cast<const TransformationChain&>(*pixelToLL_);

                    if(layer.spatialReference().isLocal() != sr_.isLocal()) {
                        DG_CHECK(layer.spatialReference().isLocal(), "Error applying region filter: %d doesn't have a spatial reference, but the input image does", filterFile.c_str());
                        DG_CHECK(sr_.isLocal(), "Error applying region filter: Input image doesn't have a spatial reference, but the %d does", filterFile.c_str());
                    } else if(!sr_.isLocal()) {
                        pixelToProj.append(*layer.spatialReference().from(SpatialReference::WGS84));
                    }

                    auto transform = pixelToProj.inverse();
                    transform->compact();

                    for (const auto& feature: layer) {
                        if (feature.type() != GeometryType::POLYGON) {
                            DG_ERROR_THROW("Filter from file \"%s\" contains a geometry that is not a POLYGON", filterFile.c_str());
                        }
                        auto poly = dynamic_cast<Polygon*>(feature.geometry->transform(*transform).release());
                        filterPolys.emplace_back(std::move(*poly));
                    }
                }
            }
            if (action == "include") {
                regionFilter_->add(filterPolys);
                firstAction = false;
            } else if (action == "exclude") {
                if (firstAction) {
                    OSN_LOG(info) << "User excluded regions first...automatically including the bounding box...";
                    regionFilter_->add(Polygon(LinearRing(cv::Rect(0, 0, bbox_.width, bbox_.height))));
                }
                regionFilter_->subtract(filterPolys);
                firstAction = false;
            } else {
                DG_ERROR_THROW("Unknown filtering action \"%s\"", action.c_str());
            }
        }
    } else {
        regionFilter_ = make_unique<PassthroughRegionFilter>();
    }
}

void OpenSpaceNet::initWfs()
{
    string baseUrl;
    if(args_.dgcsCatalogID) {
        OSN_LOG(info) << "Connecting to DGCS web feature service...";
        baseUrl = "https://services.digitalglobe.com/catalogservice/wfsaccess";
    } else if (args_.evwhsCatalogID) {
        OSN_LOG(info) << "Connecting to EVWHS web feature service...";
        baseUrl = Url("https://evwhs.digitalglobe.com/catalogservice/wfsaccess");
    }

    auto wfsCreds = args_.wfsCredentials;
    if (wfsCreds.empty()) {
        DG_CHECK(!args_.credentials.empty(), "No credentials specified for WFS service");
        wfsCreds = args_.credentials;
    }

    DG_CHECK(!args_.token.empty(), "No token specified for WFS service");

    vector<string> splitCreds;
    boost::split(splitCreds, wfsCreds, boost::is_any_of(":"));

    map<string, string> query;
    query["service"] = "wfs";
    query["version"] = "1.1.0";
    query["connectid"] = args_.token; 
    query["request"] = "getFeature";
    query["typeName"] = WFS_TYPENAME;

    if (args_.bbox) {
        auto topLeft = args_.bbox->tl();
        auto bottomRight = args_.bbox->br();
        auto stringBbox = to_string(bottomRight.x) + "," + to_string(bottomRight.y) + "," + 
                          to_string(topLeft.x) + "," + to_string(topLeft.y);

        query["bbox"] = stringBbox;
    }

    auto url = Url(baseUrl);
    url.user = splitCreds[0];
    url.password = splitCreds[1];
    url.query = std::move(query);
    wfsFeatureSet_ = make_unique<WfsFeatureSet>(url);

    if (!wfsFeatureSet_->isOpen()) {
       OSN_LOG(info) << "Failed to connect to Web Feature Service";
       wfsFeatureSet_ = nullptr;
    }
}


void OpenSpaceNet::processConcurrent()
{
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

    image_->readBlocksInAoi({}, regionFilter_.get());

    size_t curBlockClass = 0;
    auto filter = regionFilter_.get();
    auto windowSizes = calcWindows();
    auto resampledSize = args_.resampledSize ?  cv::Size {*args_.resampledSize, (int) roundf(modelAspectRatio_ * (*args_.resampledSize))} : cv::Size {};
    auto consumerFuture = async(launch::async, [this, &blockQueue, &queueMutex, &haveWork, &cancelled, &progressDisplay, numBlocks, &curBlockRead, &curBlockClass, &filter, &windowSizes, &resampledSize]() {
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

            if (filter->contains({item.first, item.second.size()}))
            {
                SlidingWindowChipper chipper(item.second,
                                             windowSizes,
                                             resampledSize,
                                             model_->metadata().modelSize());

                Subsets subsets;
                copy(chipper, back_inserter(subsets));

                auto predictions = model_->detectBoxes(subsets);

                if(!args_.excludeLabels.empty()) {
                    std::set<string> excludeLabels(args_.excludeLabels.begin(), args_.excludeLabels.end());
                    auto filtered = filterLabels(predictions, LabelFilterType::EXCLUDE, excludeLabels);
                    predictions = move(filtered);
                }

                if(!args_.includeLabels.empty()) {
                    std::set<string> includeLabels(args_.includeLabels.begin(), args_.includeLabels.end());
                    auto filtered = filterLabels(predictions, LabelFilterType::INCLUDE, includeLabels);
                    predictions = move(filtered);
                }

                for(auto& prediction : predictions) {
                    prediction.window.x += item.first.x;
                    prediction.window.y += item.first.y;
                    addFeature(prediction.window, prediction.predictions);
                }
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
    skipLine();
    OSN_LOG(info) << "Reading image...";

    unique_ptr<boost::progress_display> openProgress;
    if(!args_.quiet) {
        openProgress = make_unique<boost::progress_display>(50);
    }


    auto startTime = high_resolution_clock::now();
    auto mat = GeoImage::readImage(*image_, bbox_, regionFilter_.get(), [&openProgress](float progress) -> bool {
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

    SlidingWindowChipper chipper;
    auto resampledSize = args_.resampledSize ?  cv::Size {*args_.resampledSize, (int) roundf(modelAspectRatio_ * (*args_.resampledSize))} : cv::Size {};
    if (args_.action != Action::LANDCOVER && args_.pyramid) {
        chipper = SlidingWindowChipper(mat, 2.0,
                                       calcPrimaryWindowSize(),
                                       resampledSize,
                                       model_->metadata().modelSize());
    } else {
        chipper = SlidingWindowChipper(mat, calcWindows(), resampledSize,
                                       model_->metadata().modelSize());
    }
    chipper.setFilter(std::move(regionFilter_->clone()));
    auto it = chipper.begin();
    std::vector<PredictionBox> predictions;
    int progress = 0;

    startTime = high_resolution_clock::now();

    while(it != chipper.end()) {
        Subsets subsets;
        for(int i = 0; i < model_->batchSize() && it != chipper.end(); ++i, ++it) {
            subsets.push_back(*it);
        }

        auto predictionBatch = model_->detectBoxes(subsets);
        predictions.insert(predictions.end(), predictionBatch.begin(), predictionBatch.end());

        if(detectProgress) {
            progress += subsets.size();
            auto curProgress = (size_t)round((double)(progress + it.skipped()) / chipper.slidingWindow().totalWindows() * 50);
            if(detectProgress && detectProgress->count() < curProgress) {
                *detectProgress += curProgress - detectProgress->count();
            }
        }
    }

    duration = high_resolution_clock::now() - startTime;
    OSN_LOG(info) << "Detection time " << duration.count() << " s" ;

    if(!args_.excludeLabels.empty()) {
        skipLine();
        OSN_LOG(info) << "Performing category filtering..." ;
        std::set<string> excludeLabels(args_.excludeLabels.begin(), args_.excludeLabels.end());
        auto filtered = filterLabels(predictions, LabelFilterType::EXCLUDE, excludeLabels);
        predictions = move(filtered);
    }

    if(!args_.includeLabels.empty()) {
        skipLine();
        OSN_LOG(info) << "Performing category filtering..." ;
        std::set<string> includeLabels(args_.includeLabels.begin(), args_.includeLabels.end());
        auto filtered = filterLabels(predictions, LabelFilterType::INCLUDE, includeLabels);
        predictions = move(filtered);
    }

    if(args_.action != Action::LANDCOVER && args_.nms) {
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

    Fields fields = move(createFeatureFields(predictions));

    switch (args_.geometryType) {
        case GeometryType::POINT:
        {
            cv::Point center(window.x + window.width / 2, window.y + window.height / 2);
            auto point = pixelToLL_->transform(center);

            if(args_.dgcsCatalogID || args_.evwhsCatalogID) {
                string legacyId;
                if (wfsFeatureSet_ && !wfsFeatureSet_->isOpen()) {
                   OSN_LOG(info) << "Web Feature Service has disconnected";
                   legacyId = "uncataloged";
                } else {
                    legacyId = determineLegacyID(point);
                }
                
                fields["catalog_id"] = { FieldType::STRING, legacyId };
            }

            layer_.addFeature(Feature(new Point(point),
                              move(fields)));
            break;
        }

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

            if(args_.dgcsCatalogID || args_.evwhsCatalogID) {
                string legacyId;
                if (wfsFeatureSet_ && !wfsFeatureSet_->isOpen()) {
                   OSN_LOG(info) << "Web Feature Service has disconnected";
                   legacyId = "uncataloged";
                } else {
                    legacyId = determineLegacyID(llPoints);
                }

                fields["catalog_id"] = { FieldType::STRING, legacyId };
            }

            layer_.addFeature(Feature(new Polygon(LinearRing(llPoints)),
                                      move(fields)));
            break;
        }

        default:
            DG_ERROR_THROW("Invalid output type");
    }
}

Fields OpenSpaceNet::createFeatureFields(const vector<Prediction> &predictions) {
    Fields fields = {
            { "top_cat", { FieldType::STRING, predictions[0].label.c_str() } },
            { "top_score", { FieldType::REAL, predictions[0].confidence } },
            { "date", { FieldType::DATE, time(nullptr) } }
    };

    ptree top5;
    for(const auto& prediction : predictions) {
        top5.put(prediction.label, prediction.confidence);
    }

    ostringstream oss;
    write_json(oss, top5);

    fields["top_five"] = Field(FieldType::STRING, oss.str());

    if(args_.producerInfo) {
        fields["username"] = { FieldType::STRING, loginUser() };
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
    OSN_LOG(info) << "Dimensions (pixels): " << metadata.modelSize()
                  << "; Color Mode: " << metadata.colorMode();
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

cv::Size OpenSpaceNet::calcPrimaryWindowSize() const
{
    auto windowSize = model_->metadata().modelSize();
    if(!args_.windowSize.empty()) {
        windowSize = {args_.windowSize[0], (int) roundf(modelAspectRatio_ * args_.windowSize[0])};
    }
    return windowSize;
}

cv::Point OpenSpaceNet::calcPrimaryWindowStep() const
{
    auto windowStep = model_->defaultStep();
    if(!args_.windowStep.empty()) {
        windowStep = {args_.windowStep[0], (int) roundf(modelAspectRatio_ * args_.windowStep[0])};
    }
    return windowStep;
}

SizeSteps OpenSpaceNet::calcWindows() const
{
    if (args_.action == Action::LANDCOVER) {
        auto primaryWindowSize = calcPrimaryWindowSize();
        return { { primaryWindowSize, primaryWindowSize } };
    }

    DG_CHECK(args_.windowSize.size() < 2 || args_.windowStep.size() < 2 ||
             args_.windowSize.size() == args_.windowStep.size(),
             "Number of window sizes and window steps must match.");

    if(args_.windowSize.size() == args_.windowStep.size() &&
       args_.windowStep.size() > 0) {
        SizeSteps ret;
        for(const auto& c : boost::combine(args_.windowSize, args_.windowStep)) {
            int windowSize, windowStep;
            boost::tie(windowSize, windowStep) = c;
            ret.emplace_back(cv::Size {windowSize, (int) roundf(modelAspectRatio_ * windowSize)},
                             cv::Point {windowStep, (int) roundf(modelAspectRatio_ * windowStep)});
        }
        return ret;
    } else if (args_.windowSize.size() > 1) {
        auto windowStep = calcPrimaryWindowStep();

        SizeSteps ret;
        for(const auto& c : args_.windowSize) {
            ret.emplace_back(cv::Size { c, (int) roundf(modelAspectRatio_ * c) }, windowStep);
        }
        return ret;
    } else if (args_.windowStep.size() > 1) {
        auto windowSize = calcPrimaryWindowSize();

        SizeSteps ret;
        for(const auto& c : args_.windowStep) {
            ret.emplace_back(windowSize, cv::Point { c, (int) roundf(modelAspectRatio_ * c) });
        }
        return ret;
    } else {
        return { { calcPrimaryWindowSize(), calcPrimaryWindowStep() } };
    }
}

std::string OpenSpaceNet::determineLegacyID(const cv::Point& llPoint)
{
    auto filterPoint = Point(llPoint);
    for (auto& layer: *wfsFeatureSet_) {
        layer.setSpatialFilter(filterPoint);
        for (const auto& feature : layer) {
            if (feature.geometry) {
                if (!feature.fields.count("legacyId")) {
                    continue;
                }

                return boost::get<string>(feature.fields.at("legacyId").value);
            }
        }
    }
    
    return "uncataloged";
}

std::string OpenSpaceNet::determineLegacyID(const vector<cv::Point2d>& llPoints)
{
    const auto geosFactory = gg::GeometryFactory::getDefaultInstance();

    auto filterPoly = Polygon(LinearRing(llPoints));
    auto geosFilter = filterPoly.toGeos(*geosFactory);
    auto maxIntersection = -1.0;
    string legacyId = "uncataloged";
    for (auto& layer: *wfsFeatureSet_) {
        layer.setSpatialFilter(filterPoly);
        for (const auto& feature : layer) {
            if (feature.geometry) {
                if (!feature.fields.count("legacyId")) {
                    continue;
                }

                GGGeometryPtr extractGeom(feature.geometry->toGeos(*geosFactory), deleter_);
                GGGeometryPtr intersectGeom(geosFilter->intersection(extractGeom.get()), deleter_);
                auto intersectArea = intersectGeom->getArea();
                if (intersectArea > maxIntersection) {
                    maxIntersection = intersectArea;
                    legacyId = boost::get<string>(feature.fields.at("legacyId").value);
                }
            }
        }
    }
    
    return legacyId;
}

} } // namespace dg { namespace osn {