#OPENSKYNET

An application to perform object or terrain detection against orthorectified imagery using the DeepCore libraries.

[![Build Status](http://52.1.7.235/buildStatus/icon?job=OpenSkyNet&style=plastic)](http://52.1.7.235/job/OpenSkyNet) Jenkins Build Status

OpenSkyNet is built on top of the DeepCore libraries and requries that it is build and installed before building this project.

This application includes and is based on CUDA 7.5 and requires NVIDIA driver version 352 or higher to run using the GPU.

## Networked Imagery

Networked imagery is available to obtain orthorectified imagery directly from DigitalGlobe sources.  At this time the following datasets are supported:

| type                  | url                               | required options              |
|-----------------------|-----------------------------------|-------------------------------|
| `dgcs` (wmts layers)  | http://services.digitalglobe.com/ | `--token` and `--credentials` |
| `evwhs` (wmts layers) | http://evwhs.digitalglobe.com/    | `--token` and `--credentials` |
| `web_api`             | http://mapsapi.digitalglobe.com/  | `--token`                     |


For example, to use a GPU to create a geojson file which locates airplanes over the Hong Kong airport, use the following.  A valid airliner model file and `MAPS_API_TOKEN` is required.

```./OpenSkyNet --service web_api --token "$MAPS_API_TOKEN" --stepSize 30 --bbox 113.9235 22.3086 113.9350 22.3158 --model airliner.gbdxm --gpu --format geojson --output airliner.geojson --confidence 0.99```

### Command Line Arguments Relevant for Local Image

* `stepSize` Sliding window step size. If this argument is not specified, the image will be classified as a whole.
* `pyramid` Pyramid down to the model window size scaling down by a factor of 2 for each level. Note that this doesn't necessarily increase recall and makes detection significantly slow. Only relevant if `stepSize` argument is present.
* `bbox`​ If present, OSN will try to accommodate it by intersecting `bbox` with the image extents. If they don't intersect, and error will be shown. If the input image is not geo-registered, the ​`bbox`​ argument is required and becomes the stated image bounding box.
* `api`, `token`, `credentials` as specified for the type in the table above
* `startCol, startRow, columnSpan, rowSpan`  An alternative to the bounding box to select the region of interest
* `zoom` Zoom level to request tiles at. Defaults to zoom level 18.``
* `numConcurrentDownloads` Used to speed up downloads by allowing multiple concurrent downloads to happen at once.


### Ignored Arguments

* `image` must not be supplied, if it is, the local file will be used

## Local Files

OSN also supports processing local orthorectified imagery.

```./OpenSkyNet --image /path/to/image.tif --gpu --model /path/to/model.gbdxm --format shp --output foo.shp --outputLayerName foo --confidence .99 --stepSize 30 --pyramid```

### Command Line Arguments Relevant for Local Image

* `image` Path to the image on which to perform feature detection. The image should be geo-registered. If the image is not geo-registered, the `bbox` argument can be used to specify the image extents.
* `stepSize` Sliding window step size. If this argument is not specified, the image will be classified as a whole.
* `pyramid` Pyramid down to the model window size scaling down by a factor of 2 for each level. Note that this doesn't necessarily increase recall and makes detection significantly slow. Only relevant if `stepSize` argument is present.
* `bbox`​ If present, OSN will try to accommodate it by intersecting `bbox` with the image extents. If they don't intersect, and error will be shown. If the input image is not geo-registered, the ​`bbox`​ argument is required and becomes the stated image bounding box.

### Ignored Arguments

* `windowSize`​ argument is ignored because we can get that from the GBDXM file
* `api, startCol, startRow, columnSpan, zoom, rowSpan,`​ and ​`numConcurrentDownloads` arguments are ignored as well because they're not relevant to local file.

## Files on S3 (a variant of local files)

Since OSN uses GDAL to load local orthorectified imagery, VSI sources are supported out of the box.  Of particular interest is

```
AWS_ACCESS_KEY_ID=[AWS_ACCESS_KEY_ID] AWS_SECRET_ACCESS_KEY=[AWS_SECRET_ACCESS_KEY] \
   ./OpenSkyNet --image /vsis3/bucket/path/to/image.tif --gpu --model /path/to/model.gbdxm \
   --format shp --output foo.shp --outputLayerName foo --confidence .99 --stepSize 30
```

### Command Line Arguments Relevant for S3 Images

This command line arguments that are shown in [[Using-Local-Files-with-OpenSkyNet]] are also relevant here.

### Command line variables

2 or more command line variables must be set and additional may be set to control S3 access (see: http://www.gdal.org/cpl__vsi_8h.html)

#### VSI S3 Parameters
 - `AWS_SECRET_ACCESS_KEY`, `AWS_ACCESS_KEY_ID` (Required) define the access credentials
 - `AWS_SESSION_TOKEN` (Required for temporary credentials)
 - `AWS_REGION` (defaults to 'us-east-1')
 - `AWS_S3_ENDPOINT` (defaults to 's3.amazonaws.com')

#### Additional HTTP Parameters
 - `GDAL_HTTP_PROXY`, `GDAL_HTTP_PROXYUSERPWD`, `GDAL_PROXY_AUTH` configuration options can be used to define a proxy server.

## Complete Usage Statement

```
DigitalGlobe, Inc.
   ___                   ____  _          _   _      _
  / _ \ _ __   ___ _ __ / ___|| | ___   _| \ | | ___| |_
 | | | | '_ \ / _ \ '_ \\___ \| |/ / | | |  \| |/ _ \ __|
 | |_| | |_) |  __/ | | |___) |   <| |_| | |\  |  __/ |_ _ _ _
  \___/| .__/ \___|_| |_|____/|_|\_\\__, |_| \_|\___|\__(_|_|_)
       |_|                          |___/
Allowed options

Version: 0.1.0

:
  --help                       Usage
  --image arg                  Local image (filetype .tif) rather than using
                               tile service
  --token arg                  API token used for licensing. This is the
                               connectId for the WMTS service or the API key
                               for the Web Maps API.
  --credentials arg            Credentials for the map service. Not required
                               for Web Maps API.
  --gpu                        Use GPU for processing.
  --maxUtilization arg         Maximum GPU utilization %. Default is 95,
                               minimum is 5, and maximum is 100. Not used if
                               processing on CPU
  --service arg                The service that will be the source of tiles.
                               Valid values are dgcs, evwhs, and web_api.
                               Default is dgcs.
  --bbox arg                   Bounding box for determining tiles. This must be
                               in longitude-latitude order.
  --startCol arg               Starting tile column.
  --startRow arg               Starting tile row.
  --columnSpan arg             Number of columns.
  --zoom arg                   Zoom level to request tiles at. Defaults to zoom
                               level 18.
  --rowSpan arg                Number of rows.
  --model arg                  Folder location of the trained model.
  --type arg                   Output geometry type.  Currently only POINT and
                               POLYGON are valid.
  --format arg                 Output file format for the results.  Valid
                               values are shp, kml, elasticsearch, postgis,
                               fgdb
  --output arg                 Output location with file name and path or URL.
  --outputLayerName arg        The output layer name, index name, or table
                               name.
  --confidence arg             A factor to weed out weak matches for the
                               classification process.
  --pyramid                    Pyramid the downloaded tile and run detection on
                               all resultant images.
                                WARNING: This will result in much longer run
                               times, but may result in additional results from
                               the classification process.
  --windowSize arg             Used with stepSize to determine the height and
                               width of the sliding window.
                                WARNING: This will result in much longer run
                               times, but may result in additional results from
                               the classification process.
  --stepSize arg               Used with windowSize to determine how much the
                               sliding window moves on each step.
                                WARNING: This will result in much longer run
                               times, but may result in additional results from
                               the classification process.
  --numConcurrentDownloads arg Used to speed up downloads by allowing multiple
                               concurrent downloads to happen at once.
  --producerInfo               Add user name, application name, and application
                               version to the output feature set
  --log arg                    Log level. Permitted values are: trace, debug,
                               info, warning, error, fatal. Default level:
                               error.
  --logFile arg                Name of the file to save the log to. Default
                               logs to console.
```
