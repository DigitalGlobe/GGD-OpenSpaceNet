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


#ifndef OPENSPACENET_OPENSPACENET_H
#define OPENSPACENET_OPENSPACENET_H

#include "OpenSpaceNetArgs.h"
#include <classification/Model.h>
#include <classification/Prediction.h>
#include <imagery/GeoImage.h>
#include <imagery/MapServiceClient.h>
#include <imagery/SlidingWindow.h>
#include <network/HttpCleanup.h>
#include <opencv2/core/types.hpp>
#include <vector/FeatureSet.h>
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
    void processConcurrent();
    void processSerial();
    void addFeature(const cv::Rect& window, const std::vector<deepcore::classification::Prediction>& predictions);
    deepcore::vector::Fields createFeatureFields(const std::vector<deepcore::classification::Prediction> &predictions);
    void printModel();
    void skipLine() const;
    deepcore::imagery::SizeSteps calcSizes() const;

    const OpenSpaceNetArgs& args_;
    std::shared_ptr<deepcore::network::HttpCleanup> cleanup_;
    std::unique_ptr<deepcore::classification::Model> model_;
    std::unique_ptr<deepcore::imagery::GeoImage> image_;
    std::unique_ptr<deepcore::imagery::MapServiceClient> client_;
    std::unique_ptr<deepcore::vector::FeatureSet> featureSet_;
    cv::Point stepSize_;
    cv::Size windowSize_;
    bool concurrent_ = false;
    cv::Rect bbox_;
    std::unique_ptr<deepcore::geometry::Transformation> pixelToLL_;
};

} } // namespace dg { namespace osn {

#endif //OPENSPACENET_LIBOPENSPACENET_H
