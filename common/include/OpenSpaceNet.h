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
#include <classification/node/Detector.h>
#include <geometry/SpatialReference.h>
#include <geometry/node/LabelFilter.h>
#include <geometry/node/SubsetRegionFilter.h>
#include <imagery/node/GeoBlockSource.h>
#include <imagery/node/SlidingWindow.h>
#include <network/HttpCleanup.h>
#include <opencv2/core/types.hpp>
#include <vector/node/FileFeatureSink.h>
#include <vector/node/PredictionToFeature.h>
#include <vector/node/WfsFeatureFieldExtractor.h>
#include <utility/Logging.h>
#include <utility/ProgressDisplay.h>

namespace dg { namespace osn {

class OpenSpaceNet
{
public:
    OpenSpaceNet(OpenSpaceNetArgs&& args);
    void process();
    void setProgressDisplay(boost::shared_ptr<deepcore::ProgressDisplay> display);

private:
    deepcore::imagery::node::GeoBlockSource::Ptr initLocalImage();
    deepcore::imagery::node::GeoBlockSource::Ptr initMapServiceImage();
    deepcore::geometry::node::SubsetRegionFilter::Ptr initSubsetRegionFilter();
    deepcore::classification::node::Detector::Ptr initDetector();
    void initSegmentation(deepcore::classification::Model::Ptr model);
    deepcore::imagery::node::SlidingWindow::Ptr initSlidingWindow();
    deepcore::geometry::node::LabelFilter::Ptr initLabelFilter(bool isSegmentation);
    deepcore::vector::node::PredictionToFeature::Ptr initPredictionToFeature();
    deepcore::vector::node::WfsFeatureFieldExtractor::Ptr initWfs();
    deepcore::vector::node::FileFeatureSink::Ptr initFeatureSink();

    void printModel();
    void skipLine() const;
    deepcore::imagery::SizeSteps calcWindows() const;
    cv::Size calcPrimaryWindowSize() const;
    cv::Point calcPrimaryWindowStep() const;

    OpenSpaceNetArgs args_;
    std::shared_ptr<deepcore::network::HttpCleanup> cleanup_;
    boost::shared_ptr<deepcore::ProgressDisplay> pd_;

    cv::Size imageSize_;
    cv::Rect bbox_;
    deepcore::geometry::SpatialReference imageSr_;
    deepcore::geometry::SpatialReference sr_;
    std::unique_ptr<deepcore::geometry::Transformation> pixelToProj_;
    std::unique_ptr<deepcore::geometry::Transformation> pixelToLL_;

    std::unique_ptr<deepcore::classification::ModelMetadata> metadata_;
    cv::Size modelSize_;
    cv::Point defaultStep_;
    float modelAspectRatio_;
};

} } // namespace dg { namespace osn {

#endif //OPENSPACENET_LIBOPENSPACENET_H
