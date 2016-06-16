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

#ifndef OPENSKYNET_WORKITEM_H
#define OPENSKYNET_WORKITEM_H

#include <string>
#include <vector>
#include <opencv2/core/mat.hpp>
#include <DeepCore/vector/FeatureSet.h>

// This class holds the work items that will be processed as a part of the queue.  It contains members for the url,
// tile coordinates, images, and result geometry.
class WorkItem {
public:
     ~WorkItem();
    const std::string& url();
    void setUrl(const std::string& url);

    const cv::Point2d& tile();
    void setTile(const cv::Point2d& tile);
    void setTile(double x, double y);

    const std::vector<cv::Mat>& images();
    void addImage(cv::Mat &stream);
    cv::Mat& getImage(int index);

    dg::deepcore::vector::FeatureSet*& geometry();
    void setGeometry(dg::deepcore::vector::FeatureSet*& geometry);

    int retryCount();
    void incrementRetryCount();

    int zoom();
    void setZoom(long val);


    void pyramid(double scale, long minSize = 30);
    std::vector<cv::Rect> get_sliding_windows(const cv::Mat &image, long winWidth, long winHeight, long step);
private:
    std::string _url;
    cv::Point2d _tile;
    std::vector<cv::Mat> _images;

    //Do not delete this!
    dg::deepcore::vector::FeatureSet* _geometry;
    long _zoom = 0;
    int _downloadAttempt = 0;
};


#endif //OPENSKYNET_WORKITEM_H
