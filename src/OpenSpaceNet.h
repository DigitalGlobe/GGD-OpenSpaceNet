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


#ifndef OPENSPACENET_OPENSPACENET_H
#define OPENSPACENET_OPENSPACENET_H

#include "OpenSpaceNetArgs.h"
#include <classification/Model.h>
#include <geometry/Prediction.h>
#include <geometry/SpatialReference.h>
#include <geometry/MaskedRegionFilter.h>
#include <imagery/GeoImage.h>
#include <imagery/MapServiceClient.h>
#include <imagery/SlidingWindow.h>
#include <network/HttpCleanup.h>
#include <opencv2/core/types.hpp>
#include <vector/OgrFeatureSet.h>
#include <vector/WfsFeatureSet.h>
#include <vector/Layer.h>
#include <utility/Logging.h>

namespace dg { namespace osn {

class OpenSpaceNet
{
public:
    OpenSpaceNet(const OpenSpaceNetArgs& args);
    void process();

private:
    void initModel();
    void initLocalImage();
    void initMapServiceImage();
    void initFeatureSet();
    void initFilter();
    void initWfs();
    void processConcurrent();
    void processSerial();
    void addFeature(const cv::Rect& window, const std::vector<deepcore::geometry::Prediction>& predictions);
    deepcore::vector::Fields createFeatureFields(const std::vector<deepcore::geometry::Prediction> &predictions);
    void printModel();
    void skipLine() const;
    deepcore::imagery::SizeSteps calcWindows() const;
    cv::Size calcPrimaryWindowSize() const;
    cv::Point calcPrimaryWindowStep() const;
    std::string determineCatID(const cv::Point& llPoint);
    std::string determineCatID(const std::vector<cv::Point2d>& llPoints);

    const OpenSpaceNetArgs& args_;
    std::shared_ptr<deepcore::network::HttpCleanup> cleanup_;
    deepcore::classification::Model::Ptr model_;
    std::unique_ptr<deepcore::imagery::GeoImage> image_;
    std::unique_ptr<deepcore::imagery::MapServiceClient> client_;
    std::unique_ptr<deepcore::vector::OgrFeatureSet> featureSet_;
    std::unique_ptr<deepcore::vector::WfsFeatureSet> wfsFeatureSet_ = nullptr;
    std::unique_ptr<deepcore::geometry::RegionFilter> regionFilter_ = nullptr;
    bool concurrent_ = false;
    cv::Rect bbox_;
    float modelAspectRatio_;
    std::unique_ptr<deepcore::geometry::Transformation> pixelToLL_;
    deepcore::vector::Layer layer_;
    deepcore::geometry::SpatialReference sr_;
};

} } // namespace dg { namespace osn {

#endif //OPENSPACENET_LIBOPENSPACENET_H
