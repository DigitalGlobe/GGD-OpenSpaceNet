/********************************************************************************
* Copyright 2016 DigitalGlobe, Inc.
* Author: Ryan Desmond
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

#include "OsnRequest.h"
#include <network/JsonRpcError.h>


Json::Value OsnRequest::toJson() const {
    Json::Value request(Json::objectValue);
    request["bbox"] = Json::Value(Json::arrayValue);
    for (auto itr: args_.bbox) {
        request["bbox"].append(itr);
    }
    request["geometryType"] = args_.geometryType;
    request["outputFormat"] = args_.outputFormat;
    request["layerName"] = args_.layerName;
    request["zoom"] = Json::Int(args_.zoom);
    request["startColumn"] = Json::Int64(args_.startColumn);
    request["startRow"] = Json::Int64(args_.startRow);
    request["columnSpan"] = Json::Int64(args_.columnSpan);
    request["rowSpan"] = Json::Int64(args_.rowSpan);
    request["multiPass"] = args_.multiPass;
    request["confidence"] = args_.confidence;
    request["windowSize"] = Json::Int64(args_.windowSize);
    request["stepSize"] = Json::Int64(args_.stepSize);

    return request;
}

void OsnRequest::fromJson(const Json::Value& root) {
    DG_JSONRPC_CHECK(root.isObject(), INVALID_PARAMS, "%s", "The OsnRequest parameter element must be an object.");

    if (root.isMember("bbox")) {
        Json::Value bbox = root["bbox"];
        DG_JSONRPC_CHECK(bbox.isArray(), INVALID_PARAMS, "%s", "Parameter 'bbox' must be an array.");
        for (auto itr: bbox) {
            DG_JSONRPC_CHECK(itr.isDouble(), INVALID_PARAMS, "%s", "Each 'bbox' element must be a double.");
            args_.bbox.push_back(itr.asDouble());
        }
    } else if (root.isMember("rowSpan") && root.isMember("columnSpan")) {
        Json::Value startColumn = root["startColumn"];
        DG_JSONRPC_CHECK(startColumn.isIntegral(), INVALID_PARAMS, "%s", "Parameter 'startColumn' must be an integer.");
        args_.startColumn = startColumn.asLargestInt();

        Json::Value startRow = root["startRow"];
        DG_JSONRPC_CHECK(startRow.isIntegral(), INVALID_PARAMS, "%s", "Parameter 'startRow' must be an integer.");
        args_.startRow = startRow.asLargestInt();

        Json::Value columnSpan = root["columnSpan"];
        DG_JSONRPC_CHECK(columnSpan.isIntegral(), INVALID_PARAMS, "%s", "Parameter 'columnSpan' must be an integer.");
        args_.columnSpan = columnSpan.asLargestInt();

        Json::Value rowSpan = root["rowSpan"];
        DG_JSONRPC_CHECK(rowSpan.isIntegral(), INVALID_PARAMS, "%s", "Parameter 'columnSpan' must be an integer.");
        args_.rowSpan = rowSpan.asLargestInt();
    } else {
        DG_JSONRPC_ERROR_THROW(INVALID_PARAMS, "Either a geometric box or a row-column box is required");
    }

    if (root.isMember("geometryType")) {
        Json::Value geometryType = root["geometryType"];
        DG_JSONRPC_CHECK(geometryType.isString(), INVALID_PARAMS, "%s", "Parameter 'geometryType' must be a string.");
        args_.geometryType = geometryType.asString();
    }

    if (root.isMember("outputFormat")) {
        Json::Value outputFormat = root["outputFormat"];
        DG_JSONRPC_CHECK(outputFormat.isString(), INVALID_PARAMS, "%s", "Parameter 'outputFormat' must be a string.");
        args_.outputFormat = outputFormat.asString();
    }

    if (root.isMember("zoom")) {
        Json::Value zoom = root["zoom"];
        DG_JSONRPC_CHECK(zoom.isInt(), INVALID_PARAMS, "%s", "Parameter 'zoom' must be an integer.");
        args_.zoom = zoom.asInt();
    }


    if (root.isMember("multiPass")) {
        Json::Value multiPass = root["multiPass"];
        DG_JSONRPC_CHECK(multiPass.isBool(), INVALID_PARAMS, "%s", "Parameter 'multiPass' must be a boolean.");
        args_.multiPass = multiPass.asBool();
    }


    if (root.isMember("confidence")) {
        Json::Value confidence = root["confidence"];
        DG_JSONRPC_CHECK(confidence.isDouble(), INVALID_PARAMS, "%s", "Parameter 'confidence' must be a double.");
        args_.confidence = confidence.asDouble();
    }


    if (root.isMember("windowSize")) {
        Json::Value windowSize = root["windowSize"];
        DG_JSONRPC_CHECK(windowSize.isIntegral(), INVALID_PARAMS, "%s", "Parameter 'windowSize' must be an integer.");
        args_.windowSize = windowSize.asLargestInt();
    }


    if (root.isMember("stepSize")) {
        Json::Value stepSize = root["stepSize"];
        DG_JSONRPC_CHECK(stepSize.isIntegral(), INVALID_PARAMS, "%s", "Parameter 'stepSize' must be an integer.");
        args_.stepSize = stepSize.asLargestInt();
    }
}


OpenSkyNetArgs OsnRequest::getOsnArgs() const {
    return args_;
}