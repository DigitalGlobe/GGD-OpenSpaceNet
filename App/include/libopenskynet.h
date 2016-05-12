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

inline const char * osnReturnStatus(int code) {
    switch(code) {
        case SUCCESS: return "SUCCESS";
        case INVALID_URL: return "INVALID_URL";
        case BAD_CURL_HANDLE: return "BAD_CURL_HANDLE";
        case NO_MODEL_FILE: return "NO_MODEL_FILE";
        case INVALID_MULTIPASS: return "INVALID_MULTIPASS";
        case BAD_OUTPUT_FORMAT: return "BAD_OUTPUT_FORMAT";
    }
    return "UNKNOWN";
}

enum GeometryType {
    POINT,
    POLYGON
};


int classifyBroadAreaMultiProcess(OpenSkyNetArgs& args);
#endif //OPENSKYNET_LIBOPENSKYNET_H
