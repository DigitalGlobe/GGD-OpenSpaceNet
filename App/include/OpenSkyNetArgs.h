//
// Created by joe on 12/5/15.
//
#include <string>
#include <vector>

#ifndef OPENSKYNET_OPENSKYNETARGS_H
#define OPENSKYNET_OPENSKYNETARGS_H


struct OpenSkyNetArgs {
    const static int CURRENT_VERSION = 1;
    std::string token = "";
    std::string credentials = "";
    bool useGPU = false;
    bool webApi = false;
    std::vector<double> bbox;
    std::string modelPath = "";
    std::string geometryType = "";
    std::string outputFormat = "";
    std::string outputPath = "";
    std::string layerName = "";
    std::string authToken = "";
    int zoom = 0;
    long startColumn = 0;
    long startRow = 0;
    long columnSpan = 0;
    long rowSpan = 0;
    bool multiPass = false;
    double confidence = 0.0;
    const int version = CURRENT_VERSION;
    long windowSize = 0;
    long stepSize = 0;
};


#endif //OPENSKYNET_OPENSKYNETARGS_H
