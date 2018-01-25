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

#ifndef OPENSPACENET_OPENSPACENETARGS_H
#define OPENSPACENET_OPENSPACENETARGS_H

#include <imagery/RasterToPolygonDP.h>
#include <classification/ModelPackage.h>
#include <vector/Feature.h>

#define OSN_LOG(sev) DG_LOG(OpenSpaceNet, sev)
#define MAPSAPI_MAPID  "digitalglobe.nal0g75k"
#define WFS_TYPENAME "DigitalGlobe:FinishedFeature"


namespace dg { namespace osn {

enum class Source
{
    UNKNOWN,
    LOCAL,
    DGCS,
    EVWHS,
    MAPS_API,
    TILE_JSON
};

enum class Action
{
    UNKNOWN,
    HELP,
    DETECT
};

struct OpenSpaceNetArgs
{
    // Input options
    Action action = Action::UNKNOWN;
    Source source = Source::UNKNOWN;

    std::string image;
    std::unique_ptr<cv::Rect2d> bbox;

    // Web service input options
    std::string token;
    std::string credentials;
    int zoom = 18;
    int maxConnections = 10;
    std::string mapId = MAPSAPI_MAPID;
    std::string url;
    bool useTiles=false;


    // Output options
    deepcore::geometry::GeometryType geometryType = deepcore::geometry::GeometryType::POLYGON;
    std::string outputFormat = "shp";
    std::string outputPath;
    std::string layerName;
    bool producerInfo = false;
    bool dgcsCatalogID = false;
    bool evwhsCatalogID = false;
    std::string wfsCredentials;
    bool append = false;

    // Processing options
    std::string modelPath;
    std::unique_ptr<deepcore::classification::ModelPackage> modelPackage;
    bool useCpu = false;
    float maxUtilization = 95;
    std::vector<int> windowSize;
    std::vector<int> windowStep;
    std::unique_ptr<int> paddedSize;
    bool pyramid = false;
    size_t maxCacheSize = 0ULL;

    // Feature detection options
    float confidence = 95;
    bool nms = false;
    float overlap = 30;
    std::vector<std::string> includeLabels;
    std::vector<std::string> excludeLabels;
    std::vector<std::pair<std::string, std::vector<std::string>>> filterDefinition;

    // Segmentation options
    deepcore::imagery::RasterToPolygonDP::Method method = deepcore::imagery::RasterToPolygonDP::SIMPLE;
    double epsilon = 3.0;
    double minArea = 0.0;

    // Logging options
    bool quiet = false;
};

} } // namespace dg { namespace osn {

#endif //OPENSPACENET_OPENSPACENETARGS_H
