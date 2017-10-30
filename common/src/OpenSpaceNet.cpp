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

#include <include/OpenSpaceNetArgs.h>

#include <boost/algorithm/string.hpp>
#include <boost/range/combine.hpp>
#include <boost/date_time.hpp>
#include <boost/format.hpp>
#include <boost/make_unique.hpp>
#include <classification/Classification.h>
#include <classification/CaffeSegmentation.h>
#include <classification/Nodes.h>
#include <geometry/AffineTransformation.h>
#include <geometry/MaskedRegionFilter.h>
#include <geometry/Nodes.h>
#include <geometry/PassthroughRegionFilter.h>
#include <geometry/TransformationChain.h>
#include <imagery/DgcsClient.h>
#include <imagery/EvwhsClient.h>
#include <imagery/GdalImage.h>
#include <imagery/Imagery.h>
#include <imagery/MapBoxClient.h>
#include <imagery/Nodes.h>
#include <imagery/RasterToPolygonDP.h>
#include <process/Metrics.h>
#include <utility/User.h>
#include <vector/FileFeatureSet.h>
#include <vector/Nodes.h>
#include <vector/Vector.h>

namespace dg { namespace osn {

using namespace dg::deepcore::classification;
using namespace dg::deepcore::classification::node;
using namespace dg::deepcore::geometry;
using namespace dg::deepcore::geometry::node;
using namespace dg::deepcore::imagery;
using namespace dg::deepcore::imagery::node;
using namespace dg::deepcore::network;
using namespace dg::deepcore::vector;
using namespace dg::deepcore::vector::node;

using boost::format;
using boost::is_any_of;
using boost::join;
using boost::lexical_cast;
using boost::make_unique;
using boost::posix_time::from_time_t;
using boost::posix_time::to_simple_string;
using boost::split;
using std::chrono::duration;
using std::chrono::high_resolution_clock;
using std::map;
using std::move;
using std::string;
using std::thread;
using std::vector;
using std::unique_ptr;
using dg::deepcore::loginUser;
using dg::deepcore::ProgressCategories;

OpenSpaceNet::OpenSpaceNet(OpenSpaceNetArgs&& args) :
    args_(std::move(args))
{
    if(args_.source > Source::LOCAL) {
        cleanup_ = HttpCleanup::get();
    }
}

void OpenSpaceNet::process()
{
    deepcore::classification::init(); 
    deepcore::vector::init();

    GeoBlockSource::Ptr blockSource;
    if(args_.source > Source::LOCAL) {
        OSN_LOG(info) << "Opening map service image..." ;
        blockSource = initMapServiceImage();
    } else if(args_.source == Source::LOCAL) {
        OSN_LOG(info) << "Opening local image..." ;
        blockSource = initLocalImage();
    } else {
        DG_ERROR_THROW("Input source not specified");
    }

    auto blockCache = BlockCache::create("cache");
    blockCache->connectAttrs(*blockSource);

    auto subsetWithBorder = SubsetWithBorder::create("subsetWithBorder");
    subsetWithBorder->connectAttrs(*blockSource);

    auto subsetFilter = initSubsetRegionFilter();

    //Note: Model must be initialized before sliding window for model size and stepping
    OSN_LOG(info) << "Reading model..." ;
    auto model = initModel();
    auto slidingWindow = initSlidingWindow();
    slidingWindow->connectAttrs(*blockSource);
    model->connectAttrs(*blockSource);

    auto labelFilter = initLabelFilter();
    NonMaxSuppression::Ptr nmsNode;
    if(args_.action != Action::LANDCOVER && args_.nms) {
        nmsNode = NonMaxSuppression::create("NonMaxSuppression");
        nmsNode->attr("overlapThreshold") = (float) args_.overlap / 100;
    }

    auto predictionToFeature = initPredictionToFeature();
    auto wfsExtractor = initWfs();
    auto featureSink = initFeatureSink();

    blockCache->input("blocks") = blockSource->output("blocks");
    subsetWithBorder->input("subsets") = blockCache->output("subsets");
    if (subsetFilter) {
        subsetFilter->input("subsets") = subsetWithBorder->output("subsets");
        slidingWindow->input("subsets") = subsetFilter->output("subsets");
    } else {
        slidingWindow->input("subsets") = subsetWithBorder->output("subsets");
    }

    model->input("subsets") = slidingWindow->output("subsets");
    if (labelFilter) {
        labelFilter->input("predictions") = model->output("predictions");
        if (nmsNode) {
            nmsNode->input("predictions") = labelFilter->output("predictions");
        }
    } else if (nmsNode) {
        nmsNode->input("predictions") = model->output("predictions");
    }

    if (nmsNode) {
        predictionToFeature->input("predictions") = nmsNode->output("predictions");
    } else if (labelFilter) {
        predictionToFeature->input("predictions") = labelFilter->output("predictions");
    } else {
        predictionToFeature->input("predictions") = model->output("predictions");
    }

    if (wfsExtractor) {
        wfsExtractor->input("features") = predictionToFeature->output("features");
        featureSink->input("features") = wfsExtractor->output("features");
    } else {
        featureSink->input("features") = predictionToFeature->output("features");
    }

    startProgressDisplay();
    auto startTime = high_resolution_clock::now();

    if (!args_.quiet && pd_) {
        thread([&slidingWindow, this]() {
            while(!slidingWindow->canceled()) {
                slidingWindow->metric("processed").changed().wait();
                auto processed = slidingWindow->metric("processed").convert<int>();
                auto requested = slidingWindow->metric("requested").convert<int>();
                if (processed) {
                    auto progress = (float) processed / requested;
                    this->pd_->update("Reading", progress);
                }
            }
        }).detach();

        thread([&model, this]() {
            while(!model->canceled()) {
                model->metric("processed").changed().wait();
                auto processed = model->metric("processed").convert<int>();
                if (processed) {
                    auto progress = (float) processed / model->metric("requested").convert<int>();
                    this->pd_->update(classifyCategory_, progress);
                }
            }
        }).detach();

        thread([&featureSink, this]() {
            while(!featureSink->canceled()) {
                featureSink->metric("processed").changed().wait();
                auto processed = featureSink->metric("processed").convert<int>();
                if (processed) {
                    auto progress = (float) processed / featureSink->metric("requested").convert<int>();
                    this->pd_->update("Writing", progress);
                }
            }
        }).detach();

        featureSink->run();
        featureSink->wait();
        pd_->stop();
    } else {
        featureSink->run();
        featureSink->wait();
    }

    if (!args_.quiet) {
        skipLine();
        duration<double> duration = high_resolution_clock::now() - startTime;
        OSN_LOG(info) << featureSink->metric("processed").convert<int>() << " features detected.";
        OSN_LOG(info) << "Processing time " << duration.count() << " s";
    }
}

void OpenSpaceNet::setProgressDisplay(boost::shared_ptr<deepcore::ProgressDisplay> display)
{
    pd_ = display;
}

GeoBlockSource::Ptr OpenSpaceNet::initLocalImage()
{
    auto image = make_unique<GdalImage>(args_.image);
    blockSize_ = image->blockSize();
    imageSize_ = image->size();
    pixelToProj_ = image->pixelToProj().clone();
    imageSr_ = image->spatialReference();

    bbox_ = cv::Rect{ { 0, 0 }, imageSize_ };
    bool ignoreArgsBbox = false;

    TransformationChain llToPixel;
    if (!imageSr_.isLocal()) {
        llToPixel = {
                imageSr_.fromLatLon(),
                pixelToProj_->inverse()
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

        llToPixel = { pixelToProj_->inverse() };
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

    GeoBlockSource::Ptr blockSource = GdalBlockSource::create("source");
    blockSource->attr("path") = args_.image;
    return blockSource;
}

GeoBlockSource::Ptr OpenSpaceNet::initMapServiceImage()
{
    DG_CHECK(args_.bbox, "Bounding box must be specified");

    std::unique_ptr<deepcore::imagery::MapServiceClient> client;
    bool wmts = true;
    string url;
    switch(args_.source) {
        case Source::MAPS_API:
            OSN_LOG(info) << "    Connecting to MapsAPI..." ;
            client = make_unique<MapBoxClient>(args_.mapId, args_.token);
            wmts = false;
            break;

        case Source ::EVWHS:
            OSN_LOG(info) << "    Connecting to EVWHS..." ;
            client = make_unique<EvwhsClient>(args_.token, args_.credentials);
            break;

        case Source::TILE_JSON:
            OSN_LOG(info) << "    Connecting to TileJSON...";
            client = make_unique<TileJsonClient>(args_.url, args_.credentials, args_.useTiles);
            wmts = false;
            break;

        default:
            OSN_LOG(info) << "    Connecting to DGCS..." ;
            client = make_unique<DgcsClient>(args_.token, args_.credentials);
            break;
    }

    client->connect();

    if(wmts) {
        client->setImageFormat("image/jpeg");
        client->setLayer("DigitalGlobe:ImageryTileService");
        client->setTileMatrixSet("EPSG:3857");
        client->setTileMatrixId((format("EPSG:3857:%1d") % args_.zoom).str());
    } else {
        client->setTileMatrixId(lexical_cast<string>(args_.zoom));
    }

    unique_ptr<Transformation> llToProj(client->spatialReference().fromLatLon());
    auto projBbox = llToProj->transform(*args_.bbox);
    auto image = client->imageFromArea(projBbox);
    blockSize_ = image->blockSize();
    imageSize_ = image->size();
    pixelToProj_ = image->pixelToProj().clone();
    imageSr_ = image->spatialReference();

    unique_ptr<Transformation> projToPixel(image->pixelToProj().inverse());
    bbox_ = projToPixel->transformToInt(projBbox);
    pixelToLL_ = TransformationChain { std::move(llToProj), std::move(projToPixel) }.inverse();
    sr_ = SpatialReference::WGS84;

    auto blockSource = MapServiceBlockSource::create("source");
    blockSource->attr("config") = client->configFromArea(*args_.bbox);
    blockSource->attr("maxConnections") = args_.maxConnections;
    return blockSource;
}

SubsetRegionFilter::Ptr OpenSpaceNet::initSubsetRegionFilter()
{
    if (args_.filterDefinition.size()) {
        OSN_LOG(info) << "Initializing the subset filter..." ;

        RegionFilter::Ptr regionFilter = make_unique<MaskedRegionFilter>(cv::Rect(0, 0, bbox_.width, bbox_.height),
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
                regionFilter->add(filterPolys);
                firstAction = false;
            } else if (action == "exclude") {
                if (firstAction) {
                    OSN_LOG(info) << "User excluded regions first...automatically including the bounding box...";
                    regionFilter->add(Polygon(LinearRing(cv::Rect(0, 0, bbox_.width, bbox_.height))));
                }
                regionFilter->subtract(filterPolys);
                firstAction = false;
            } else {
                DG_ERROR_THROW("Unknown filtering action \"%s\"", action.c_str());
            }
        }

        
        auto subsetFilter = SubsetRegionFilter::create("SubsetRegionFilter");
        subsetFilter->attr("regionFilter") = regionFilter;

        return subsetFilter;
    }

    return nullptr;
}

Detector::Ptr OpenSpaceNet::initModel()
{
    auto model = Model::create(*args_.modelPackage, !args_.useCpu, args_.maxUtilization / 100);
    args_.modelPackage.reset();

    metadata_ = model->metadata().clone();
    modelSize_ = metadata_->modelSize();
    defaultStep_ = model->defaultStep();
    modelAspectRatio_ = (float) modelSize_.height / modelSize_.width;

    float confidence = 0;
    if(args_.action == Action::LANDCOVER && model->modelOutput() == ModelOutput::BOX) {
        auto windowSize = calcPrimaryWindowSize();
        if (args_.windowSize.size() < 2) {
            if(blockSize_.width % windowSize.width == 0 &&
               blockSize_.height % windowSize.height == 0) {
                bbox_ = {cv::Point {0, 0}, imageSize_};
                concurrent_ = true;
            }
        }
    } else if(args_.action == Action::DETECT) {
        confidence = args_.confidence / 100;
    }

    DG_CHECK(!args_.resampledSize || *args_.resampledSize <= modelSize_.width,
             "Argument --resample-size (size: %d) does not fit within the model (width: %d).",
             *args_.resampledSize, modelSize_.width)

    if (!args_.resampledSize) {
        for (auto c : args_.windowSize) {
            DG_CHECK(c <= modelSize_.width,
                     "Argument --window-size contains a size that does not fit within the model (width: %d).",
                      modelSize_.width)
        }
    }

    if(metadata_->category() == "segmentation") {
        initSegmentation(model);
    }

    auto modelNode = Detector::create("Model");
    modelNode->attr("model") = model;
    modelNode->attr("confidence") = confidence;
    return modelNode;
}

void OpenSpaceNet::initSegmentation(Model::Ptr model)
{
    auto castModel = dynamic_cast<CaffeSegmentation*>(model.get());
    DG_CHECK(castModel, "Unsupported model type: only Caffe segmentation models are currently supported");

    castModel->setRasterToPolygon(make_unique<RasterToPolygonDP>(args_.method, args_.epsilon, args_.minArea));
}

dg::deepcore::imagery::node::SlidingWindow::Ptr OpenSpaceNet::initSlidingWindow()
{
    auto slidingWindow = dg::deepcore::imagery::node::SlidingWindow::create("SlidingWindow");
    auto windowSizes = calcWindows();
    auto resampledSize = args_.resampledSize ?  
                         cv::Size {*args_.resampledSize, (int) roundf(modelAspectRatio_ * (*args_.resampledSize))} : 
                         cv::Size {};
    slidingWindow->attr("windowSizes") = windowSizes;
    slidingWindow->attr("resampledSize") = resampledSize;

    return slidingWindow;
}

LabelFilter::Ptr OpenSpaceNet::initLabelFilter()
{
    LabelFilter::Ptr labelFilter;
    if(!args_.excludeLabels.empty()) {
        labelFilter = ExcludeLabels::create("ExcludeLabelFilter");
        labelFilter->attr("labels") = vector<string>(args_.excludeLabels.begin(), args_.excludeLabels.end());
    } else if(!args_.includeLabels.empty()) {
        labelFilter = IncludeLabels::create("IncludeLabelFilter");
        labelFilter->attr("labels") = vector<string>(args_.includeLabels.begin(), args_.includeLabels.end());
    }

    return labelFilter;
}

PredictionToFeature::Ptr OpenSpaceNet::initPredictionToFeature()
{
    auto predictionToFeature = PredictionToFeature::create("PredictionToFeature");
    predictionToFeature->attr("geometryType") = args_.geometryType;
    predictionToFeature->attr("pixelToProj") = pixelToProj_;
    predictionToFeature->attr("topNName") = "top_five";
    predictionToFeature->attr("topNCategories") = 5;

    return predictionToFeature;
}

WfsFeatureFieldExtractor::Ptr OpenSpaceNet::initWfs()
{
    if (args_.dgcsCatalogID || args_.evwhsCatalogID) {
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

        map<string, string> query;
        query["service"] = "wfs";
        query["version"] = "1.1.0";
        query["connectid"] = args_.token; 
        query["request"] = "getFeature";
        query["typeName"] = WFS_TYPENAME;
        query["srsName"] = "EPSG:3857";

        vector<string> splitCreds;
        split(splitCreds, wfsCreds, is_any_of(":"));
        auto url = Url(baseUrl);
        url.user = splitCreds[0];
        url.password = splitCreds[1];
        url.query = std::move(query);

        vector<string> fieldNames = {"legacyId"};
        Fields defaultFields = { {"legacyId", Field(FieldType::STRING, "uncataloged")}};

        auto featureFieldExtractor = WfsFeatureFieldExtractor::create("WFSFeatureFieldExtractor");
        featureFieldExtractor->attr("inputSpatialReference") = imageSr_;
        featureFieldExtractor->attr("fieldNames") = fieldNames;
        featureFieldExtractor->attr("defaultFields") = defaultFields;
        featureFieldExtractor->attr("url") = url;
        return featureFieldExtractor;
    }

    return nullptr;
}

FileFeatureSink::Ptr OpenSpaceNet::initFeatureSink()
{
    time_t currentTime = time(nullptr);
    struct tm* timeInfo = gmtime(&currentTime);
    time_t gmTimet = timegm(timeInfo);
    Fields extraFields = { {"date", Field(FieldType::DATE,  gmTimet)} };

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

        extraFields["username"] = { FieldType ::STRING, loginUser() };
        extraFields["app"] = { FieldType::STRING, "OpenSpaceNet"};
        extraFields["app_ver"] =  { FieldType::STRING, OPENSPACENET_VERSION_STRING };
    }

    if(args_.dgcsCatalogID || args_.evwhsCatalogID) {
        definitions.push_back({FieldType::STRING, "catalog_id"});
    }

    VectorOpenMode openMode = args_.append ? APPEND : OVERWRITE;

    auto featureSink = FileFeatureSink::create("FeatureSink");
    featureSink->attr("spatialReference") = imageSr_;
    featureSink->attr("outputSpatialReference") = sr_;
    featureSink->attr("geometryType") = args_.geometryType;
    featureSink->attr("path") = args_.outputPath;
    featureSink->attr("layerName") = args_.layerName;
    featureSink->attr("outputFormat") = args_.outputFormat;
    featureSink->attr("openMode") = openMode;
    featureSink->attr("fieldDefinitions") = definitions;

    return featureSink;
}

void OpenSpaceNet::startProgressDisplay()
{
    if(!pd_ || args_.quiet) {
        return;
    }

    ProgressCategories categories = {{"Reading", "Reading the image" }};

    if (args_.action == Action::LANDCOVER) {
        classifyCategory_ = "Classifying";
        categories.emplace_back(classifyCategory_, "Classifying the image");
    } else if(args_.action == Action::DETECT) {
        classifyCategory_ = "Detecting";
        categories.emplace_back(classifyCategory_, "Detecting the object(s)");
    } else {
        DG_ERROR_THROW("Invalid action called during process()");
    }

    categories.emplace_back("Writing", "Writing the features");

    pd_->setCategories(move(categories));
    pd_->start();
}

bool OpenSpaceNet::isCancelled()
{
    return !pd_ || pd_->isCancelled();
}

void OpenSpaceNet::printModel()
{
    skipLine();

    OSN_LOG(info) << "Model Name: " << metadata_->name()
                  << "; Version: " << metadata_->version()
                  << "; Created: " << to_simple_string(from_time_t(metadata_->timeCreated()));
    OSN_LOG(info) << "Description: " << metadata_->description();
    OSN_LOG(info) << "Dimensions (pixels): " << modelSize_
                  << "; Color Mode: " << metadata_->colorMode();
    OSN_LOG(info) << "Bounding box (lat/lon): " << metadata_->boundingBox();
    OSN_LOG(info) << "Labels: " << join(metadata_->labels(), ", ");

    skipLine();
}

void OpenSpaceNet::skipLine() const
{
    if(!args_.quiet) {
        std::cout << std::endl;
    }
}

cv::Size OpenSpaceNet::calcPrimaryWindowSize() const
{
    auto windowSize = modelSize_;
    if(!args_.windowSize.empty()) {
        windowSize = {args_.windowSize[0], (int) roundf(modelAspectRatio_ * args_.windowSize[0])};
    }
    return windowSize;
}

cv::Point OpenSpaceNet::calcPrimaryWindowStep() const
{
    auto windowStep = defaultStep_;
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

} } // namespace dg { namespace osn {
