# OpenSpaceNet

[![Build Status](http://52.1.7.235/buildStatus/icon?job=OpenSpaceNet_CUDA8&style=plastic)](http://52.1.7.235/job/OpenSpaceNet_CUDA8) Jenkins Build Status

OpenSpaceNet is an open-source application to perform object or terrain detection against orthorectified imagery using the DeepCore libraries. This application includes and is based on CUDA 7.5 and requires NVIDIA driver version 352 or higher to run using the GPU.

OpenSpaceNet takes a pre-trained neural network and applies it to imagery.  It can use local files from your computer, or connect to DigitalGlobe servers to download maps.  OpenSpaceNet uses your DigitalGlobe account to download the maps, so it can only use maps that you have already purchased.  This guide will explain the basic usage of OpenSpaceNet.

## Train a model
Train a model with Caffe.  OpenSpaceNet only supports Caffe models for the time being.  Once you have a trained model, you will need to package it as a gbdxm file, which is an archive format used by OpenSpaceNet.

## Convert model to GBDXM
The GBDXM format stores a model in a compressed file which OSN uses.  The `gbdxm` module acts as a link between caffe and OSN.  The executable is available on the [DeepCore webpage](https://digitalglobe.github.io/DeepCore/).  You will need to make it executable before you can run it `chmod u+x gbdxm`.  Then you can convert your Caffe model to gbdxm format:

```bash 
$ ./gbdxm pack -f out.gbdxm --caffe-model deploy.prototxt --caffe-trained model.caffemodel \
  --caffe-mean mean.binaryproto -t caffe -n "Airliner" -v 0 -d "Model Description" -l labels.txt \
  --image-type jpg -b -84.06163 37.22197 -84.038803 37.240162
```

For detailed documentation of the command line arguments, run `./gbdxm help` or just `./gbdxm` at the terminal.

## Run with OpenSpaceNet
1. Make sure CUDA 7.5 is installed ([instructions](http://www.r-tutor.com/gpu-computing/cuda-installation/cuda7.5-ubuntu)). We are working on OSN for CUDA 8, but it will only run with 7.5 for now.  

2. Download OpenSpaceNet executable [by clicking the link in the right column](https://digitalglobe.github.io/DeepCore/).  You do not need to install DeepCore if you use the pre-compiled executable.  

3. Use OpenSpaceNet to apply models to map data

For descriptions of the command line arguments, consult the [documentation](https://github.com/DigitalGlobe/OpenSpaceNet/blob/master/doc/REFERENCE.md) for OpenSpaceNet.  For basic usage, you can follow these examples.

If you have the images that you want to search in an image file, then you can use the `--image` option.  This does not require any access keys.

### Using a local file
Linux:
```
$ ./OpenSpaceNet detect --model MODEL.gbdxm --type POLYGON --format shp --output OUTPUT_FILENAME.shp \
  --nms --window-step 30 --image INPUT_IMAGE_FILENAME
```  
Windows:
```
C:\> OpenSpaceNet.exe detect --model MODEL.gbdxm --type POLYGON --format shp --output OUTPUT_FILENAME.shp --nms --window-step 30 --image INPUT_IMAGE_FILENAME
```
### Using the API  
If you access DigitalGlobe API, then you can use your token to download maps.  DeepCore will handle the maps for you, so all you need to provide is the API token, and the bounding box that you want to search in.

Linux:
```
$ ./OpenSpaceNet detect --bbox -84.44579 33.63404 -84.40601 33.64583  --model MODEL.gbdxm \
  --type POLYGON --format shp --output OUTPUT_FILENAME.shp --confidence 99.9 --nms \
  --window-step 25 --num-downloads 200 --token API_TOKEN  --service maps-api
```

Windows:
```
C:\> OpenSpaceNet.exe detect --bbox -84.44579 33.63404 -84.40601 33.64583  --model MODEL.gbdxm --type POLYGON --format shp --output OUTPUT_FILENAME.shp --confidence 99.9 --nms --window-step 25 --num-downloads 200 --token API_TOKEN  --service maps-api
```
### Using DigitalGlobe Cloud Services (DGCS)
If you have access to DigitalGlobe Cloud Services, then OpenSpaceNet can access maps using your account.  You need to provide your username, password, and the appropriate token for the type of maps that you want to use.  You can find information about the kinds of maps offered by DigitalGlobe of is available on [our webpage](https://www.digitalglobe.com/products/basemap-suite).

Linux:
```
$ ./OpenSpaceNet detect --bbox -84.44579 33.63404 -84.40601 33.64583 --model MODEL.gbdxm \
  --type POLYGON --format shp --output OUTPUT_FILENAME.shp --confidence 99.9 --nms \
  --window-step 15 --num-downloads 200 --token MAP_TOKEN --credentials USER:PASS --service dgcs
```

Windows:
```
C:\> ./OpenSpaceNet.exe detect --bbox -84.44579 33.63404 -84.40601 33.64583 --model MODEL.gbdxm --type POLYGON --format shp --output OUTPUT_FILENAME.shp --confidence 99.9 --nms --window-step 15 --num-downloads 200 --token MAP_TOKEN --credentials USER:PASS --service dgcs
```

## Visualizing results
You can visualize your results using GIS software like ArcGIS or QGIS.  You can configure QGIS to use DigitalGlobe maps by following [these instructions](https://developer.digitalglobe.com/using-maps-api-with-qgis/).  Then you can import the shapefile as a vector layer to see the model results.

## Links

### [User Reference Guide](doc/REFERENCE.md)
### [Building OpenSpaceNet](doc/BUILDING.md)
