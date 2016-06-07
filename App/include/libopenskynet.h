//
// Created by joe on 11/11/15.
//
#include "OpenSkyNetArgs.h"

#ifndef OPENSKYNET_LIBOPENSKYNET_H
#define OPENSKYNET_LIBOPENSKYNET_H
enum ReturnCodes {
    SUCCESS,
    INVALID_URL,
    BAD_CURL_HANDLE,
    NO_MODEL_FILE,
    INVALID_MULTIPASS,
    BAD_OUTPUT_FORMAT
};

enum GeometryType {
    POINT,
    POLYGON
};


int classifyFromFile(OpenSkyNetArgs &args);
int classifyBroadAreaMultiProcess(OpenSkyNetArgs& args);
#endif //OPENSKYNET_LIBOPENSKYNET_H
