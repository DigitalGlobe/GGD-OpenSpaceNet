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

#include "OsnServer.h"
#include "OsnRequest.h"
#include "OpenSkyNetArgs.h"
#include "libopenskynet.h"

using namespace std;

OsnServer::OsnServer(OpenSkyNetArgs defaultArgs) : defaultArgs_(defaultArgs) { }

void OsnServer::handleRequest(const string &method, const Json::Value &params,
                                 JsonRpcResponse &response) {
    if (method == "classify") {
        OsnRequest osnRequest(defaultArgs_);
        osnRequest.fromJson(params);
        OpenSkyNetArgs args = osnRequest.getOsnArgs();

        int code = classifyBroadAreaMultiProcess(args);
        if (code != ReturnCodes::SUCCESS)
            response.setError(JsonRpcError::fromData(code, osnReturnStatus(code), Json::Value()));


    } else if (method == "defaults") {
        if (!params.isNull()) {
            DG_JSONRPC_ERROR_THROW(INVALID_PARAMS, "'%s' must not have any parameters", method.c_str());
        }

        OsnRequest osnRequest(defaultArgs_);
        response.setResult(osnRequest.toJson());

    } else {
        DG_JSONRPC_ERROR_THROW(METHOD_NOT_FOUND, "'%s' not found", method.c_str());
    }
}

void OsnServer::handleNotification(const string &method, const Json::Value &params) {
    cout << "Notification: " << method;
}