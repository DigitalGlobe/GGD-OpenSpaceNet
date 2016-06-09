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

#ifndef OPENSKYNET_OPENSKYNETARGS_H
#define OPENSKYNET_OPENSKYNETARGS_H

#include <map>
#include <sstream>
#include <string>
#include <vector>

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
    float maxUtitilization = 0.95;

    int windowSize = 0;
    int stepSize = 0;
    std::vector<long> pyramidWindowSizes;
    std::vector<long> pyramidWindowSteps;
};

#endif //OPENSKYNET_OPENSKYNETARGS_H
