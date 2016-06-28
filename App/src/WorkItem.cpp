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

#include <exception>
#include <iostream>
#include <opencv2/imgproc.hpp>
#include <glog/logging.h>

#include "../include/WorkItem.h"

using namespace dg::deepcore::vector;

const std::string& WorkItem::url(){
    return _url;
}

void WorkItem::setUrl(const std::string& url){
    _url = url;
}

const cv::Point3i& WorkItem::tile(){
    return _tile;
};

void WorkItem::setTile(const cv::Point3i& tile){
    _tile = tile;
}

const std::vector<cv::Mat>& WorkItem::images(){
    return _images;
}

void WorkItem::addImage(cv::Mat &stream){
    _images.push_back(stream);
}

cv::Mat& WorkItem::getImage(int index){
    if (_images.size() < (size_t)index){
        throw "Index out of range";
    } else {
        return _images[index];
    }
}

FeatureSet*& WorkItem::geometry() {
    return _geometry;
}

void WorkItem::setGeometry(FeatureSet*& set){
    _geometry = set;
}

WorkItem::~WorkItem() {
    _images.clear();
}

void WorkItem::pyramid(double scale, long minSize) {
    if (_images.size() == 0){
        LOG(WARNING) << "No images to pyramid!\n";
        return;
    }
    cv::Mat src = _images[0];
    //make a temp copy so that nothing happens to the original
    cv::Mat tmp = src;
    cv::Mat dst = tmp;
    while (true){
        long size = tmp.cols;
        //check to see if this iteration will result in an image that's too small
        if ((size / scale) < minSize){
            break;
        }
        cv::pyrDown(tmp, dst, cv::Size((int)(tmp.cols / scale), (int)(tmp.rows / scale)));
        _images.push_back(cv::Mat(dst));

        tmp = dst;
    };
}

std::vector<cv::Rect> WorkItem::get_sliding_windows(const cv::Mat &image, long winWidth, long winHeight, long step)
{
    std::vector<cv::Rect> rects;
    for(int i=0;i<image.rows;i+=step)
    {
        if((i+winHeight)>image.rows){break;}
        for(int j=0;j< image.cols;j+=step)
        {
            if((j+winWidth)>image.cols){break;}
            cv::Rect rect(j,i,winWidth,winHeight);
            rects.push_back(rect);
        }
    }
    return rects;
}


int WorkItem::retryCount() { return _downloadAttempt; }
void WorkItem::incrementRetryCount() { ++_downloadAttempt; }


