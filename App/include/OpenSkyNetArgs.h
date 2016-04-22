//
// Created by joe on 12/5/15.
//
#include <string>
#include <vector>
#include <map>

#ifndef OPENSKYNET_OPENSKYNETARGS_H
#define OPENSKYNET_OPENSKYNETARGS_H

typedef std::vector<std::pair<std::string, float>> ClassThresholds;

/*****************************************************
 * For reading in arguments of type "Class name":value 
 * ***************************************************/
namespace std {
    template <typename V> static inline std::istream& operator>>(std::istream& is, std::pair<std::string, V>& into) {
        char ch;
        while (is >> ch && ch!='=') into.first += ch;
        return is >> into.second;
    }
}

struct OpenSkyNetArgs {
    const static int CURRENT_VERSION = 1;
    std::string token = "";
    std::string credentials = "";
    std::string url = "";
    std::string image = "";
    bool useTileServer = true;
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
    std::map<std::string, float> classThresholds;
    double threshold = 0.0;
    long numThreads = 1;

    long windowSize = 0;
    long stepSize = 0;
    std::vector<long> pyramidWindowSizes;
    std::vector<long> pyramidWindowSteps;
};

#endif //OPENSKYNET_OPENSKYNETARGS_H
