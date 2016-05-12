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

#ifndef OPENSKYNET_OSNRPC_H
#define OPENSKYNET_OSNRPC_H

#include <network/JsonObject.h>

#include <string>
#include <vector>
#include <json/json.h>
#include <ctime>
#include "OpenSkyNetArgs.h"

using namespace dg::deepcore::network;

/**
 * \class OsnRequest
 * \brief OpenSkyNet request payloads
 */
class OsnRequest : public JsonObject {
public:
    OsnRequest(OpenSkyNetArgs defaultArgs): args_(defaultArgs) { }

    virtual Json::Value toJson() const override;
    virtual void fromJson(const Json::Value& root) override;

    OpenSkyNetArgs getOsnArgs() const;

private:
    OpenSkyNetArgs args_;
};


#endif //OPENSKYNET_OSNRPC_H
