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

#ifndef OPENSKYNET_OSNSERVER_H
#define OPENSKYNET_OSNSERVER_H

#include "OpenSkyNetArgs.h"

#include <network/JsonRpcServer.h>
#include <network/MilNetRequest.h>
#include <network/MilNetResponse.h>
#include <classification/Model.h>

#include <memory>
#include <string>

using namespace dg::deepcore::network;
using namespace dg::deepcore::classification;

typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;

class OsnServer : public JsonRpcServer {
public:
    OsnServer(OpenSkyNetArgs defaultArgs);

protected:

    virtual void handleRequest(const std::string &method, const Json::Value &params,
                               JsonRpcResponse &response) override;

    virtual void handleNotification(const std::string &method, const Json::Value &params) override;

private:
    OpenSkyNetArgs defaultArgs_;
};


#endif //OPENSKYNET_OSNSERVER_H
