//
// Created by Joe on 12/1/15.
// This class holds the work items that will be processed as a part of the queue.  It contains members for the url,
// tile coordinates, images, and result geometry.
//
#include <string>
#include <vector>
#include <opencv2/core/mat.hpp>
#include "../../vector/include/VectorFeatureSet.h"

#ifndef OPENSKYNET_WORKITEM_H
#define OPENSKYNET_WORKITEM_H


class WorkItem {
public:
     ~WorkItem();
    const std::string& url();
    void setUrl(const std::string& url);

    const std::pair<double, double>& tile();
    void setTile(double x, double y);

    const std::vector<cv::Mat>& images();
    void addImage(cv::Mat &stream);
    cv::Mat& getImage(int index);

    VectorFeatureSet*& geometry();
    void setGeometry(VectorFeatureSet*& geometry);

    int retryCount();
    void incrementRetryCount();

    int zoom();
    void setZoom(long val);


    void pyramid(double scale, long minSize = 30);
    std::vector<cv::Rect> get_sliding_windows(const cv::Mat &image, long winWidth, long winHeight, long step);
private:
    std::string _url;
    std::pair<double, double> _tile;
    std::vector<cv::Mat> _images;

    //Do not delete this!
    VectorFeatureSet* _geometry;
    long _zoom = 0;
    int _downloadAttempt = 0;
};


#endif //OPENSKYNET_WORKITEM_H
