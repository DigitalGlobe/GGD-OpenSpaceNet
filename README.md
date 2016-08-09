# OPENSKYNET

[![Build Status](http://52.1.7.235/buildStatus/icon?job=OpenSkyNet&style=plastic)](http://52.1.7.235/job/OpenSkyNet) Jenkins Build Status

An application to perform object or terrain detection against orthorectified imagery using the DeepCore libraries.
This application includes and is based on CUDA 7.5 and requires NVIDIA driver version 352 or higher to run using the GPU.

## OpenSkyNet Actions

_OpenSkyNet_ can be called in 3 different ways:

```
OpenSkyNet help [topic]
OpenSkyNet detect <addtional options>
OpenSkyNet landcover <addtional options>
```

### help
The `help` mode shows the _OpenSkyNet_ usage. The amount of information can be reduced by specifying the action for
which to show the help.

The following are the ways you can call help:
```
OpenSkyNet help
OpenSkyNet help landcover
OpenSkyNet help detect
OpenSkyNet <other arguments> --help
```

### detect
The `detect` mode performs feature detection. The arguments to configure feature detection are:

#### Command Line arguments for the `detect` Action

##### --confidence
This option sets the minimum percent score for results to be included in the output. This should be a value between 0 and 100. 
The default value for this argument is 95%.

i.e. `--confidence 99` sets the confidence to 99%.

##### --step-size
This option sets the sliding window step size. Default value is _log<sub>2</sub>_ of the model's 
window size. Step size can be specified in either one or two dimensions. If only one dimension is specified, the step 
size will be the same in both directions.

i.e. `--step-size 30` will result in the sliding window step size being 30 in the _x_ direction and 30 in the _y_ direction.

##### --pyramid
This option will enable using pyramids in feature detection. This means that the image will be repeatedly resampled down
by a factor of 2 with sliding window detection run on each reduced image. This option will result in much longer run time
and is not recommended. Most _OpenSkyNet_ models are designed to work at a certain resolution and do not require pyramidding.

##### --nms
This option will cause _OpenSkyNet_ to perform non-maximum suppression on the output. This that adjacent detection boxes
will be removed for each feature detected and only one detecton box per object will be output. This option results in much
better quality output. You can optionally specify the overlap threshold percentage for non-maximum suppression calculation.
The default overlap is 30%.

i.e. `--nms` will result in non-maximum suppression with 30% overlap, while `--nms 20` will result in non-maximum 
suppression with 20% overlap.

### landcover

The landcover action is used to perform CNN-based landcover classification. This mode differs from the `detect` mode
as follows:

* If the image source is a web service, the requested bounding box will be expanded to include whole server tiles. 
* `--step-size` argument is ignored. The step size is set to the model's window size or to the `--window-size` argument's value.
* `--nms` argument is ignored.
* `--confidence` argument is ignored, confidence is set to 0%.
* `--pyramid` argument is ignored.

## Image Input

_OpenSkyNet_ is able use either a geo-registered local image, or a MapBox or WMTS web server as its input. Depending on
the input source, different command line arguments apply. To select which source to use, one of two options must be present:

* `--service <service name>` To load imagery from a web service.
* `--image <path>` To load a local image file.

### Web Service Input

Depending on which service you use, different arguments apply. This is a summary:

| service  | token    | credentials | mapId    | zoom     | bbox     | num-downloads |
|----------|----------|-------------|----------|----------|----------|---------------|
| dgcs     | Required | Required    |          | Optional | Required | Optional      |
| evwhs    | Required | Required    |          | Optional | Required | Optional      |
| maps-api | Required |             | Optional | Optional | Required | Optional      |

#### Command Line Options for Web Services

##### --service

This argument is required for a web service input. If this argument is specified, the `--image` argument cannot be 
present. The following services are available:

* 'dgcs' is the DigitalGlobe Cloud Service WMTS data source. This service requires both '--token' and '--credentials' to 
be set. The service's Web URL is http://services.digitalglobe.com/.
 
* 'evwhs' is the DigitalGlobe's Enhanced View Web Hosting Service WMTS data source. This service requires both '--token' 
and '--credentials' to be set. The service's web URL is http://evwhs.digitalglobe.com/.

* 'maps-api' is the DigitalGlobes MapsAPI service hosted by MapBox. This service only requires the `--token` to be set.
In addition, the user can specify the `--mapId` argument to set the image source map ID. The service's web URL is 
http://mapsapi.digitalglobe.com/.

##### --bbox

The bounding box argument is required for all web service input sources. The coordinates are specified in the WGS84 
Lat/Lon coordinate system. The order of coordinates is west longitude, south latitude, east longitude, and north latitude.
Note that for the landcover action the bounding box may be expanded so that the tiles downloaded from the service will
be processed completely.

i.e. `--bbox 125.541 38.866 125.562 38.881` specifies a bounding box for the Atlanta Hartsfield-Jackson Airport area.

##### --token

This argument specifies the API token for the desired web service. This is required for all three supported services.

##### --credentials

This argument specifies the user name and password for the WMTS services, that is `dgcs` and `evwhs`. The format is
`--credentials username:password`. If only the user name is specified like `--credentials username`, the user fill be
prompted for the password before _OpenSkyNet_ will proceed. In addition, credentials can be specified by setting the
`OSN_CREDENTIALS` environment variable.

##### --mapId

This argument is only valid for the MapsAPI, or `maps-api` service. The DigitalGlobe map IDs are listed on this web page:
http://mapsapidocs.digitalglobe.com/docs/imagery-and-basemaps.

##### --zoom

This argument specifies the zoom level for the web service. For MapsAPI the zoom level is 0 to 22, while both DGCS and
EVWHS zoom levels range from 0 to 20. The default zoom level is 18.

##### --num-downloads

This argument specifies the number of image tiles that will be downloaded simultaneously. Increasing the value of this
argument can dramatically speed up downloads, but it can cause the service to crash or deny you access. The default value
is 10.

## Local Image Input

Local image input means that _OpenSkyNet_ will load a single image a local drive or a network drive accessible through
the file system.

### Command Line Options for Local Image Input

##### --image

This argument is required for local image input.

i.e. `--image /home/user/Pictures/my_image.tif`

##### --bbox

The bounding box argument is optional for local image input. If the specified bounding box is not fully within the input
image an intersection of the image bounding box and the specified bounding box will be used. If the specified bounding
box does not intersect the image, an error will be displayed.

### Files on S3 (a variant of local files)

Since _OpenSkyNet_ uses GDAL to load local imagery, VSI sources are supported out of the box.  Of 
particular interest is imagery stored on S-3.

i.e.

```
AWS_ACCESS_KEY_ID=[AWS_ACCESS_KEY_ID] AWS_SECRET_ACCESS_KEY=[AWS_SECRET_ACCESS_KEY] \
   ./OpenSkyNet --image /vsis3/bucket/path/to/image.tif --model /path/to/model.gbdxm \
   --output foo.shp --confidence 99 --step-size 30
```

#### VSI S3 Parameters
 * `AWS_SECRET_ACCESS_KEY`, `AWS_ACCESS_KEY_ID` (Required) define the access credentials
 * `AWS_SESSION_TOKEN` (Required for temporary credentials)
 * `AWS_REGION` (defaults to 'us-east-1')
 * `AWS_S3_ENDPOINT` (defaults to 's3.amazonaws.com')

#### Additional HTTP Parameters
 * `GDAL_HTTP_PROXY`, `GDAL_HTTP_PROXYUSERPWD`, `GDAL_PROXY_AUTH` configuration options can be used to define a proxy server.

## Output Options

## Processing Options

## Logging Options

## Using Configuration Files

## Complete Usage Statement

```
Usage:
  OpenSkyNet <action> <input options> <output options> <processing options>
  OpenSkyNet <configuration file>

Actions:
  help     			 Show this help message
  detect   			 Perform feature detection
  landcover			 Perform land cover classification

Local Image Input Options:
  --image PATH                          If this is specified, the input will be
                                        taken from a local image.
  --bbox WEST SOUTH EAST NORTH          Optional bounding box for image subset,
                                        optional for local images. Coordinates 
                                        are specified in the following order: 
                                        west longitude, south latitude, east 
                                        longitude, and north latitude.

Web Service Input Options:
  --service SERVICE                     Web service that will be the source of 
                                        input. Valid values are: dgcs, evwhs, 
                                        and maps-api.
  --token TOKEN                         API token used for licensing. This is 
                                        the connectId for WMTS services or the 
                                        API key for the Web Maps API.
  --credentials USERNAME[:PASSWORD]     Credentials for the map service. Not 
                                        required for Web Maps API. If password 
                                        is not specified, you will be prompted 
                                        to enter it. The credentials can also 
                                        be set by setting the OSN_CREDENTIALS 
                                        environment variable.
  --zoom ZOOM (=18)                     Zoom level.
  --mapId arg (=digitalglobe.nal0g75k)  MapsAPI map id to use.
  --num-downloads NUM (=10)             Used to speed up downloads by allowing 
                                        multiple concurrent downloads to happen
                                        at once.
  --bbox WEST SOUTH EAST NORTH          Bounding box for determining tiles 
                                        specified in WGS84 Lat/Lon coordinate 
                                        system. Coordinates are specified in 
                                        the following order: west longitude, 
                                        south latitude, east longitude, and 
                                        north latitude.

Output Options:
  --format FORMAT (=shp)                Output file format for the results. 
                                        Valid values are: elasticsearch, 
                                        geojson, kml, postgis, shp.
  --output PATH                         Output location with file name and path
                                        or URL.
  --output-layer NAME (=skynetdetects)  The output layer name, index name, or 
                                        table name.
  --type TYPE (=polygon)                Output geometry type.  Currently only 
                                        point and polygon are valid.
  --producer-info                       Add user name, application name, and 
                                        application version to the output 
                                        feature set.

Processing Options:
  --cpu                                 Use the CPU for processing, the default
                                        it to use the GPU.
  --max-utilization PERCENT (=95)       Maximum GPU utilization %. Minimum is 
                                        5, and maximum is 100. Not used if 
                                        processing on CPU
  --model PATH                          Path to the the trained model.
  --window-size WIDTH [HEIGHT]          Overrides the original model's window 
                                        size. Window size can be specified in 
                                        either one or two dimensions. If only 
                                        one dimension is specified, the window 
                                        will be square. This parameter is 
                                        optional and not recommended.

Feature Detection Options:
  --confidence PERCENT (=95)            Minimum percent score for results to be
                                        included in the output.
  --step-size WIDTH [HEIGHT]            Sliding window step size. Default value
                                        is log2 of the model window size. Step 
                                        size can be specified in either one or 
                                        two dimensions. If only one dimension 
                                        is specified, the step size will be the
                                        same in both directions.
  --pyramid                             Use pyramids in feature detection. 
                                        WARNING: This will result in much 
                                        longer run times, but may result in 
                                        additional features being detected.
  --nms [PERCENT (=30)]                 Perform non-maximum suppression on the 
                                        output. You can optionally specify the 
                                        overlap threshold percentage for 
                                        non-maximum suppression calculation.

Logging Options:
  --log [LEVEL (=info)] PATH            Log to a file, a file name preceded by 
                                        an optional log level must be 
                                        specified. Permitted values for log 
                                        level are: trace, debug, info, warning,
                                        error, fatal.
  --quiet                               If set, no output will be sent to 
                                        console, only a log file, if specified.

General Options:
  --config PATH                         Use options from a configuration file.
  --help                                Show this help message
```
