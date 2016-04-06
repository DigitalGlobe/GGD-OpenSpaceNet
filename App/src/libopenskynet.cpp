//
// Created by joe on 11/11/15.
//
#include <iostream>
#include <string>
#include <vector>
#include "../include/libopenskynet.h"
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <imagery/TileRequest.h>
#include <imagery/Tile.h>
#include <OpenSkyNetArgs.h>
#include <opencv2/imgcodecs/imgcodecs_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/highgui.hpp>
#include <Classifier.h>
#include <CaffeBatchClassifier.h>
#include <CaffeEnsembleBatchClassifier.h>
#include <boost/timer/timer.hpp>
#include <curl/curl.h>
#include <curlpp/cURLpp.hpp>
#include <glob.h>
#include <imagery/GdalTileProducer.h>
#include <imagery/WmtsTileProducer.h>
#include <Threshold.h>

/* Function used to glob model names directories for ensemble models */
bool modelGlob(const std::string& inPath, const std::vector<std::string>& suffixes, std::vector<std::string>& modelPaths) {
    bool foundAll = true;
    glob_t glob_result;
    modelPaths.clear();
    
    for (auto it = suffixes.begin(); it != suffixes.end(); it++) {
        std::string fullPath = inPath + (*it);
        glob(fullPath.c_str(), GLOB_TILDE, NULL, &glob_result);
        if (glob_result.gl_pathc < 1) {
            std::cout << "Error: no path matches " << fullPath << std::endl;
            modelPaths.push_back("");
            foundAll = false;
        }
        else if (glob_result.gl_pathc > 1) {
            std::cout << "Error: more than one path matches " << fullPath << std::endl;
            modelPaths.push_back("");
            foundAll = false;
        }
        else {
            modelPaths.push_back(glob_result.gl_pathv[0]);
        }
    }
    globfree(&glob_result);
    return foundAll;
}

void persistResultsWindows(std::vector<WindowPrediction>& results, Tile *tile, 
                           VectorFeatureSet* geom, const Threshold& thresholds, 
                           const cv::Rect *rect = nullptr) {
    for (auto result = results.begin(); result != results.end(); result++) {
        if (result->predictions.size() > 0) {
            for (auto prediction = result->predictions.begin(); 
                 prediction != result->predictions.end(); prediction++) {
                
                if (prediction->second >= thresholds.getThreshold(prediction->first)) {
                    std::cout << "Region above threshold - adding to results." << std::endl;
                    std::cout << "  " << prediction->first << " " << prediction->second << std::endl;
            
                    std::vector<std::pair<double, double>> geometry;
                    tile->getWindowCoords(result->window, geometry);
                    std::cout << "Creating polygon from tile sub rect " << geometry[0].first << "," << geometry[0].second <<
                    " from tile " << tile->tile().first << "," << tile->tile().second << ".\n";
                    
                    geom->addPolygon(result->predictions, geometry, tile->tile(), tile->zoom(), 0);

                    // break if any are satisfied - all predictions get written
                    break;
                }
            }
        }
    }
}


int classifyBroadAreaMultiProcess(OpenSkyNetArgs &args) {
    curlpp::Cleanup cleaner;

    /* Create pyramid object */
    std::cout << "Pyramid window sizes: " << std::endl;
    for (auto it = args.pyramidWindowSizes.begin(); it != args.pyramidWindowSizes.end(); it++) {

    std::cout << "  " << *it << std::endl;
    }
    std::cout << "Pyramid step sizes: " << std::endl;
    for (auto it = args.pyramidWindowSteps.begin(); it != args.pyramidWindowSteps.end(); it++) {

    std::cout << "  " << *it << std::endl;
    }

    Pyramid* pyramid = new Pyramid(args.pyramidWindowSizes, args.pyramidWindowSteps);
 
    std::cout << "Pyramid:" << std::endl;
    for (pyramid->begin(); ! pyramid->end(); ++(*pyramid)) {
        long winSize, stepSize;
        pyramid->current(winSize, stepSize);
        std::cout << "  Window size " << winSize << " step " << stepSize << std::endl;
    }

    /* Multiple directories for ensemble model */
    /* TODO: Should this be a struct? One for each model? */
    std::vector<std::string> modelFiles;
    std::vector<std::string> meanFiles;
    std::vector<std::string> labelFiles;
    std::vector<std::string> deployFiles;

    /* Model suffixes required for caffe models */
    std::vector<std::string> modelSuffixes;
    modelSuffixes.push_back("*.caffemodel");
    modelSuffixes.push_back("mean.binaryproto");
    modelSuffixes.push_back("labels.txt");
    modelSuffixes.push_back("deploy.prototxt");

    for (auto pathIt = args.modelPath.begin(); pathIt != args.modelPath.end(); pathIt++) {
        std::vector<std::string> paths;
        bool modelFound = modelGlob((*pathIt), modelSuffixes, paths);
        if (modelFound) {
            modelFiles.push_back(paths[0]);
            meanFiles.push_back(paths[1]);
            labelFiles.push_back(paths[2]);
            deployFiles.push_back(paths[3]);
        }
        else {
            std::cout << "Error: Unable to determine model from path " << *pathIt << std::endl;
            return NO_MODEL_FILE;
        }
    }

    std::cout << "Using models: " << std::endl;
    int numModels = modelFiles.size();
    for (int i = 0; i < numModels; i++) {
        std::cout << "Model " << i << std::endl;
        std::cout << "  model " << modelFiles[i] << std::endl;
        std::cout << "  mean " << meanFiles[i] << std::endl;
        std::cout << "  labels " << labelFiles[i] << std::endl;
        std::cout << "  deploy " << deployFiles[i] << std::endl;
    }

    std::cout << "Initializing classifier from model.\n";
    CaffeEnsembleBatchClassifier* classifier = new CaffeEnsembleBatchClassifier(deployFiles, modelFiles, meanFiles, labelFiles, *pyramid, args.useGPU);
    std::cout << "Classifier initialization complete.\n";

    /* Class specific and global thresholding */
    Threshold thresholds(args.classThresholds, args.threshold);
    thresholds.printThresholds();

    VectorFeatureSet *fs;
    try {
        fs = new VectorFeatureSet(args.outputPath, args.outputFormat, args.layerName);
    }
    catch (std::invalid_argument) {
        std::cout << "An invalid format was provided for output." << std::endl;
        curl_global_cleanup();
        return BAD_OUTPUT_FORMAT;
    }

    TileProducer* tiler;
    if (args.useTileServer) {
        std::cout << "Classification WMTS tiles" << std::endl;
        /* Download and put Tiles on classification queue */
        tiler = new WmtsTileProducer(args.url, args.credentials, fs,
                                     args.bbox, args.zoom,
                                     args.rowSpan, args.columnSpan,
                                     args.startRow, args.startColumn);
    }
    else {
        std::cout << "Classifcation on local image" << std::endl;
        /* Tile image and put Tiles on classification queue */
        tiler = new GdalTileProducer(args.image, 1000, 1000, 30, 30);

        /* The following line outputs a projection file .prj corresponding to the GeoTif for the vector layer */
        //fs->setProjection(tiler->getSpr());
    }

    tiler->PrintTiling();

    /* Tile counting */
    long processedCount = 0;
    const long tileCount = tiler->getNumTiles();

    while (not tiler->empty()) {
        Tile* tilePtr;
        tiler->pop(tilePtr);

        std::vector<WindowPrediction > results;
        results = classifier->Classify(*tilePtr, 5, thresholds.getLowestThreshold());
        /* Now process results */
        persistResultsWindows(results, tilePtr, fs, thresholds);
        tilePtr->printTileOffsets();

        delete tilePtr;

        double percentage = (((double) ++processedCount) / tileCount) * 100.0;
        std::cout << "Classification " << percentage << "% completed." << std::endl;
    }


    delete tiler;

    delete fs;

    delete classifier;

    delete pyramid;

    return SUCCESS;
}
