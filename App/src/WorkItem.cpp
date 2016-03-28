//
// Created by joe on 12/1/15.
//

#include <exception>
#include <iostream>
#include <opencv2/imgproc.hpp>
#include <glog/logging.h>

#include "../include/WorkItem.h"


const std::string& WorkItem::url(){
    return _url;
}

void WorkItem::setUrl(const std::string& url){
    _url = url;
}

const std::pair<double, double>& WorkItem::tile(){
    return _tile;
};

void WorkItem::setTile(double x, double y){
    _tile.first = x;
    _tile.second = y;
}

const std::vector<cv::Mat>& WorkItem::images(){
    return _images;
}

void WorkItem::addImage(cv::Mat &stream){
    _images.push_back(stream);
}

cv::Mat& WorkItem::getImage(int index){
    if (_images.size() < index){
        throw "Index out of range";
    } else {
        return _images[index];
    }
}

VectorFeatureSet*& WorkItem::geometry() {
    return _geometry;
}

void WorkItem::setGeometry(VectorFeatureSet*& set){
    _geometry = set;
}

int WorkItem::zoom() {
    return _zoom;
}

void WorkItem::setZoom(long val){
    _zoom = val;
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


