//
// Created by Joe on 12/1/15.
// This class holds the work items that will be processed as a part of the queue.  It contains members for the url,
// tile coordinates, images, and result geometry.
//
#include <string>
#include <vector>
#include <opencv2/core/mat.hpp>
#include <DeepCore/vector/VectorFeatureSet.h>

#ifndef OPENSKYNET_WORKITEM_H
#define OPENSKYNET_WORKITEM_H


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

    dg::deepcore::vector::VectorFeatureSet*& geometry();
    void setGeometry(dg::deepcore::vector::VectorFeatureSet*& geometry);

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
    dg::deepcore::vector::VectorFeatureSet* _geometry;
    long _zoom = 0;
    int _downloadAttempt = 0;
};


#endif //OPENSKYNET_WORKITEM_H
