/********************************************************************************
* Copyright 2017 DigitalGlobe, Inc.
* Author: Kevin McGee
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
********************************************************************************/
#include <fstream>
#include <limits>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include <imagery/MapBoxClient.h>
#include <imagery/DgcsClient.h>
#include <imagery/EvwhsClient.h>
#include <imagery/GdalImage.h>

#include <geometry/TransformationChain.h>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "progresswindow.h"
#include "ui_progresswindow.h"
#include "qdebugstream.h"

using std::unique_ptr;
using boost::filesystem::path;
using boost::format;
using std::string;
using boost::lexical_cast;
namespace po = boost::program_options;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() ^ Qt::WindowMaximizeButtonHint);

    connectSignalsAndSlots();
    setUpLogging();
    initValidation();

    progressWindow.getUI().cancelPushButton->setVisible(false);

    statusBar()->showMessage(tr("Ready"));
    statusBar()->installEventFilter(this);
    ui->pyramidCheckBox->hide();

    //Set the file browsers' initial location to the user's home directory
    lastAccessedDirectory = QDir::homePath();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::connectSignalsAndSlots()
{
    connect(&thread, SIGNAL(processFinished()), this, SLOT(enableRunButton()));
    connect(&qout, SIGNAL(updateProgressText(QString)), this, SLOT(updateProgressBox(QString)));
    connect(&sout, SIGNAL(updateProgressText(QString)), this, SLOT(updateProgressBox(QString)));
    connect(&progressWindow, SIGNAL(cancelPushed()), this, SLOT(cancelThread()));

    //Connections that change the color of the filepath line edits
    connect(ui->localImageFileLineEdit, SIGNAL(editingFinished()), this, SLOT(on_imagepathLineEditLostFocus()));
    connect(ui->modelFileLineEdit, SIGNAL(editingFinished()), this, SLOT(on_modelpathLineEditLostFocus()));
    connect(ui->outputLocationLineEdit, SIGNAL(editingFinished()), this, SLOT(on_outputLocationLineEditLostFocus()));

    //Connections that change the error color of widgets back to the default when the user begins editing the contents
    connect(ui->localImageFileLineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_localImagePathLineEditCursorPositionChanged()));
    connect(ui->modelFileLineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_modelpathLineEditCursorPositionChanged()));
    connect(ui->outputFilenameLineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_outputFilenameLineEditCursorPositionChanged()));
    connect(ui->outputLocationLineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_outputPathLineEditCursorPositionChanged()));

    //Connections that change the error color of the web services widgets back to the default
    connect(ui->usernameLineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_usernameLineEditCursorPositionChanged()));
    connect(ui->usernameLineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_passwordLineEditCursorPositionChanged()));
    connect(ui->passwordLineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_passwordLineEditCursorPositionChanged()));
    connect(ui->passwordLineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_usernameLineEditCursorPositionChanged()));
    connect(ui->mapIdLineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_mapIdLineEditCursorPositionChanged()));
    connect(ui->tokenLineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_tokenLineEditCursorPositionChanged()));
}

void MainWindow::setUpLogging()
{
    boost::shared_ptr<std::ostream> stringStream(&buffer_, boost::null_deleter());
    stringSink_ = dg::deepcore::log::addStreamSink(stringStream, dg::deepcore::level_t::info);

    stringStreamUI = stringStream;
    qout.setOptions(*stringStreamUI);
    sout.setOptions(stringStreamStdout);

    statusProgressBar = new QProgressBar;

    statusBar()->setStyleSheet("QStatusBar{border-top: 1px outset grey;}");
}

void MainWindow::initValidation()
{
    //bbox double validator
    QRegularExpression doubleRegExp("[+-]?\\d*\\.?\\d+");
    doubleValidator = std::unique_ptr<QRegularExpressionValidator>(new QRegularExpressionValidator(doubleRegExp, 0));

    ui->bboxWestLineEdit->setValidator(doubleValidator.get());
    ui->bboxSouthLineEdit->setValidator(doubleValidator.get());
    ui->bboxEastLineEdit->setValidator(doubleValidator.get());
    ui->bboxNorthLineEdit->setValidator(doubleValidator.get());
}

void MainWindow::on_loadConfigPushButton_clicked()
{
    QString path = QFileDialog::getOpenFileName(this,
                                                tr("Select Config File"),
                                                lastAccessedDirectory,
                                                tr("Config files (*.cfg);;All files (*.*)"));

    //Setting the last image directory to an empty string ensures that the browser will open
    //in the last directory it accessed the last time it was opened
    lastAccessedDirectory = "";

    //the user clicked cancel in the file dialog
    if(path.isEmpty()) {
        return;
    }

    importConfig(path);
}

void MainWindow::on_saveConfigPushButton_clicked()
{
    QString errorBuffer;

    //Disable the run button during validation
    ui->runPushButton->setEnabled(false);
    bool valid = validateUI(errorBuffer);
    ui->runPushButton->setEnabled(true);

    if(!valid) {
        //have to have this here to prevent run button from being disabled by validation checks
        //this is only here because the run button state is mixed into the validation logic
        ui->runPushButton->setEnabled(true);

        QMessageBox::critical(
                this,
                tr("Cannot Save Config"),
                errorBuffer);

        return;
    }

    QString filepath = QFileDialog::getSaveFileName(this,
                                                    tr("Save to Config File"),
                                                    lastAccessedDirectory,
                                                    tr("Config file (.cfg)"));
    exportConfig(filepath);
}

void MainWindow::on_localImageFileBrowseButton_clicked()
{
    QString path = QFileDialog::getOpenFileName(this,
                                                tr("Select Image File"),
                                                lastAccessedDirectory,
                                                tr("Image files (*.tif *.jpg *.JPEG *.png *.bmp);;All files (*.*)"));
    lastAccessedDirectory = "";

    if(!path.isEmpty() && !path.isNull()) {
        ui->localImageFileLineEdit->setText(path);
        //manually invoke the slot to check the new filepath
        on_imagepathLineEditLostFocus();
    }
}

void MainWindow::on_modelFileBrowseButton_clicked()
{
    QString path = QFileDialog::getOpenFileName(this,
                                                tr("Select Model File"),
                                                lastAccessedDirectory,
                                                tr("GBDXM files (*.gbdxm);;All files (*.*)"));
    //Setting the last model directory to an empty string ensures that the browser will open
    //in the last directory it accessed the last time it was opened
    lastAccessedDirectory = "";

    //The directory path string will be empty if the user presses cancel in the QFileDialog
    if(!path.isEmpty() && !path.isNull()) {
        ui->modelFileLineEdit->setText(path);
        //manually invoke the slot to check the new filepath
        on_modelpathLineEditLostFocus();
    }
}

void MainWindow::on_imageSourceComboBox_currentIndexChanged(const QString &source)
{
    if(source != "Local Image File") {
        //bbox
        ui->bboxOverrideCheckBox->hide();
        ui->bboxNorthLineEdit->setEnabled(true);
        ui->bboxSouthLineEdit->setEnabled(true);
        ui->bboxEastLineEdit->setEnabled(true);
        ui->bboxWestLineEdit->setEnabled(true);

        //local image selection
        ui->localImageFileLabel->setEnabled(false);
        ui->localImageFileLineEdit->setEnabled(false);
        ui->localImageFileBrowseButton->setEnabled(false);

        //set the style of the local image file field to default
        ui->localImageFileLineEdit->clear();
        ui->localImageFileLineEdit->setStyleSheet("color: default");

        //token
        ui->tokenLabel->setEnabled(true);
        ui->tokenLineEdit->setEnabled(true);

        //map id
        if(source == "MapsAPI") {
        	ui->mapIdLabel->setEnabled(true);
        	ui->mapIdLineEdit->setEnabled(true);

        	//user credentials
        	ui->usernameLabel->setEnabled(false);
        	ui->usernameLineEdit->setEnabled(false);
        	ui->passwordLabel->setEnabled(false);
        	ui->passwordLineEdit->setEnabled(false);

            if(ui->zoomSpinBox->value() > 20) {
        	    ui->zoomSpinBox->setValue(20);
            }
            ui->zoomSpinBox->setMaximum(20);
        }
        else {
        	ui->mapIdLabel->setEnabled(false);
        	ui->mapIdLineEdit->setEnabled(false);

        	//user credentials
        	ui->usernameLabel->setEnabled(true);
        	ui->usernameLineEdit->setEnabled(true);
        	ui->passwordLabel->setEnabled(true);
        	ui->passwordLineEdit->setEnabled(true);
        	ui->zoomSpinBox->setMaximum(22);
        }

        //zoom
        ui->zoomLabel->setEnabled(true);
        ui->zoomSpinBox->setEnabled(true);

        //downloads
        ui->downloadsLabel->setEnabled(true);
        ui->downloadsSpinBox->setEnabled(true);
    }
    else {
    	//bbox
        ui->bboxOverrideCheckBox->show();
        bool bboxOverridden = ui->bboxOverrideCheckBox->isChecked();
        ui->bboxNorthLineEdit->setEnabled(bboxOverridden);
        ui->bboxSouthLineEdit->setEnabled(bboxOverridden);
        ui->bboxEastLineEdit->setEnabled(bboxOverridden);
        ui->bboxWestLineEdit->setEnabled(bboxOverridden);

        //local image selection
        ui->localImageFileLabel->setEnabled(true);
        ui->localImageFileLineEdit->setEnabled(true);
        ui->localImageFileBrowseButton->setEnabled(true);

        //token
        ui->tokenLabel->setEnabled(false);
        ui->tokenLineEdit->setEnabled(false);

        //map id
        ui->mapIdLabel->setEnabled(false);
        ui->mapIdLineEdit->setEnabled(false);

        //zoom
        ui->zoomLabel->setEnabled(false);
        ui->zoomSpinBox->setEnabled(false);

        //downloads
        ui->downloadsLabel->setEnabled(false);
        ui->downloadsSpinBox->setEnabled(false);

        //user credentials
        ui->usernameLabel->setEnabled(false);
        ui->usernameLineEdit->setEnabled(false);
        ui->passwordLabel->setEnabled(false);
        ui->passwordLineEdit->setEnabled(false);

        //validate the old imagepath
        on_imagepathLineEditLostFocus();
    }
}

void MainWindow::on_viewMetadataButton_clicked()
{
    QMessageBox::information(
        this,
        tr("Metadata"),
        tr("Viewing Metadata is currently not supported."));
}

void MainWindow::on_nmsCheckBox_toggled(bool checked)
{
    if(checked) {
        ui->nmsSpinBox->setEnabled(true);
    }
    else {
        ui->nmsSpinBox->setEnabled(false);
    }
}

void MainWindow::on_bboxOverrideCheckBox_toggled(bool checked)
{
    if(checked) {
        ui->bboxNorthLineEdit->setEnabled(true);
        ui->bboxSouthLineEdit->setEnabled(true);
        ui->bboxEastLineEdit->setEnabled(true);
        ui->bboxWestLineEdit->setEnabled(true);
    }
    else {
        //set bbox values back to the local image's bounding box
        if(hasValidLocalImagePath) {
            on_imagepathLineEditLostFocus();
        }

        ui->bboxNorthLineEdit->setEnabled(false);
        ui->bboxSouthLineEdit->setEnabled(false);
        ui->bboxEastLineEdit->setEnabled(false);
        ui->bboxWestLineEdit->setEnabled(false);
    }
}

void MainWindow::on_outputLocationBrowseButton_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Select Output Location"));
    if(!path.isEmpty() && !path.isNull()) {
        ui->outputLocationLineEdit->setText(path);
        //manually invoke the slot to check the new directory path
        on_outputLocationLineEditLostFocus();
    }
}

void MainWindow::on_helpPushButton_clicked()
{
    QMessageBox::information(
        this,
        tr("Help"),
        tr("Help is currently not supported."));
}

void MainWindow::on_runPushButton_clicked()
{
    QString errorBuffer;
    ui->runPushButton->setEnabled(false);
    bool validJob = validateUI(errorBuffer);

    if(!validJob) {
        ui->runPushButton->setEnabled(true);
        QMessageBox::critical(
            this,
            tr("Error"),
            errorBuffer);
        return;
    }

    //Parse and set the Action
    action = ui->modeComboBox->currentText().toStdString();
    if(action == "Detect") {
        osnArgs.action = dg::osn::Action::DETECT;
    }
    else if(action == "Landcover") {
        osnArgs.action = dg::osn::Action::LANDCOVER;
    }
    else {
        osnArgs.action = dg::osn::Action::UNKNOWN;
    }

    //Parse and set the image source
    imageSource = ui->imageSourceComboBox->currentText().toStdString();
    if(imageSource == "Local Image File") {
        osnArgs.source = dg::osn::Source::LOCAL;
    }
    else if(imageSource == "DGCS") {
        osnArgs.source = dg::osn::Source::DGCS;
        hasValidLocalImagePath = true;
    }
    else if(imageSource == "EVWHS") {
        osnArgs.source = dg::osn::Source::EVWHS;
        hasValidLocalImagePath = true;
    }
    else if(imageSource == "MapsAPI") {
        osnArgs.source = dg::osn::Source::MAPS_API;
        hasValidLocalImagePath = true;
    }
    else {
        osnArgs.source = dg::osn::Source::UNKNOWN;
    }

    //Parse and set web service token
    osnArgs.token = ui->tokenLineEdit->text().toStdString();

    //Parse and set the credentials, format is username:password
    osnArgs.credentials = ui->usernameLineEdit->text().toStdString()
                          + ":"
                          + ui->passwordLineEdit->text().toStdString();

    //Parse and set the zoom level
    osnArgs.zoom = ui->zoomSpinBox->value();

    //Parse and set the max downloads
    osnArgs.maxConnections = ui->downloadsSpinBox->value();

    //Parse and set the map id
    if(ui->mapIdLineEdit->text() != "") {
        osnArgs.mapId = ui->mapIdLineEdit->text().toStdString();
    }
    else {
        osnArgs.mapId = MAPSAPI_MAPID;
    }

    //Parse and set the image path
    localImageFilePath = ui->localImageFileLineEdit->text().toStdString();
    osnArgs.image = localImageFilePath;

    //Parse and set the model path
    modelFilePath = ui->modelFileLineEdit->text().toStdString();
    osnArgs.modelPath = modelFilePath;

    //Parse and set the confidence value
    confidence = ui->confidenceSpinBox->value();
    osnArgs.confidence = (float)confidence;

    //Parse and set the step size
    stepSize = ui->stepSizeSpinBox->value();
    osnArgs.stepSize = boost::make_unique<cv::Point>(stepSize, stepSize);

    //Parse and set the pyramid value
    pyramid = ui->pyramidCheckBox->isChecked();
    if(pyramid == true) {
        osnArgs.pyramid = true;
    }
    else {
        osnArgs.pyramid = false;
    }

    //Parse and set NMS
    NMS = ui->nmsCheckBox->isChecked();
    if(NMS == true) {
        osnArgs.nms = true;
        nmsThreshold = ui->nmsSpinBox->value();
        osnArgs.overlap = (float)nmsThreshold;
    }
    else {
        osnArgs.nms = false;
    }

    //bbox parsing to be set up when web services are implemented
    bboxNorth = ui->bboxNorthLineEdit->text().toStdString();
    bboxSouth = ui->bboxSouthLineEdit->text().toStdString();
    bboxEast = ui->bboxEastLineEdit->text().toStdString();
    bboxWest = ui->bboxWestLineEdit->text().toStdString();

    osnArgs.bbox = NULL;

    if(imageSource != "Local Image File") {
        osnArgs.bbox = boost::make_unique<cv::Rect2d>(cv::Point2d(stod(bboxWest), stod(bboxSouth)),
                                                      cv::Point2d(stod(bboxEast), stod(bboxNorth)));
    }

    //Output filename parsing and setting
    outputFilename = ui->outputFilenameLineEdit->text().toStdString();

    //Output path setting
    outputLocation = ui->outputLocationLineEdit->text().toStdString();

    //Output format parsing and setting
    outputFormat = ui->outputFormatComboBox->currentText().toStdString();
    if(outputFormat == "Shapefile") {
        osnArgs.outputFormat = "shp";
        //Append file extension
        outputFilename += "." + osnArgs.outputFormat;
        outputFilepath = outputLocation + "/" + outputFilename;
        osnArgs.outputPath = outputFilepath;
    }
    else if(outputFormat == "Elastic Search") {
        osnArgs.outputFormat = "elasticsearch";
        osnArgs.outputPath = outputLocation;
    }
    else if(outputFormat == "GeoJSON") {
        osnArgs.outputFormat = "geojson";
        //Append file extension
        outputFilename += "." + osnArgs.outputFormat;
        outputFilepath = outputLocation + "/" + outputFilename;
        osnArgs.outputPath = outputFilepath;
    }
    else if(outputFormat == "KML") {
        osnArgs.outputFormat = "kml";
        //Append file extension
        outputFilename += "." + osnArgs.outputFormat;
        outputFilepath = outputLocation + "/" + outputFilename;
        osnArgs.outputPath = outputFilepath;
    }
    else if(outputFormat == "PostGIS") {
        osnArgs.outputFormat = "postgis";
        osnArgs.outputPath = outputLocation;
    }

    //Geometry type parsing and setting
    geometryType = ui->geometryTypeComboBox->currentText().toStdString();
    if(geometryType == "Polygon") {
        osnArgs.geometryType = dg::deepcore::vector::GeometryType::POLYGON;
    }
    else {
        osnArgs.geometryType = dg::deepcore::vector::GeometryType::POINT;
    }

    //layer name parsing and setting
    outputLayer = ui->outputLayerLineEdit->text().toStdString();
    osnArgs.layerName = path(osnArgs.outputPath).stem().filename().string();

    //producer info parsing
    producerInfo = ui->producerInfoCheckBox->isChecked();
    if(producerInfo == true) {
        osnArgs.producerInfo = true;
    }
    else {
        osnArgs.producerInfo = false;
    }

    //Processing mode parsing and setting
    processingMode = ui->processingModeComboBox->currentText().toStdString();
    if(processingMode == "GPU") {
        osnArgs.useCpu = false;
    }
    else {
        osnArgs.useCpu = true;
    }

    //max utilization parsing and setting
    maxUtilization = ui->maxUtilizationSpinBox->value();
    osnArgs.maxUtilization = (float)maxUtilization;

    windowSize1 = ui->windowSizeSpinBox1->value();
    windowSize2 = ui->windowSizeSpinBox2->value();
    osnArgs.windowSize = unique_ptr<cv::Size>();

    std::clog << "Mode: " << action << std::endl;

    std::clog << "Image Source: " << imageSource << std::endl;
    std::clog << "Local Image File Path: " << localImageFilePath << std::endl;

    std::clog << "Web Service Token: " << osnArgs.token << std::endl;
    std::clog << "Web Service Credentials: " << osnArgs.credentials << std::endl;
    std::clog << "Zoom Level: " << osnArgs.zoom << std::endl;
    std::clog << "Number of Downloads: " << osnArgs.maxConnections << std::endl;
    std::clog << "Map Id: " << osnArgs.mapId << std::endl;

    std::clog << "Model File Path: " << modelFilePath << std::endl;

    std::clog << "Confidence: " << confidence << std::endl;
    std::clog << "Step Size: " << stepSize << std::endl;
    std::clog << "Pyramid: " << pyramid << std::endl;
    std::clog << "NMS: " << NMS << " Threshold: " << nmsThreshold << std::endl;

    std::clog << "BBOX North: " << bboxNorth << std::endl;
    std::clog << "BBOX South: " << bboxSouth << std::endl;
    std::clog << "BBOX East: " << bboxEast << std::endl;
    std::clog << "BBOX West: " << bboxWest << std::endl;

    std::clog << "Output Filename: " << outputFilename << std::endl;
    std::clog << "Output Filepath: " << outputFilepath << std::endl;
    std::clog << "Output Format: " << outputFormat << std::endl;
    std::clog << "Geometry Type: " << geometryType << std::endl;
    std::clog << "Output Location: " << outputLocation << std::endl;
    std::clog << "Output Layer: " << outputLayer << std::endl;
    std::clog << "Producer Info:  " << producerInfo << std::endl;

    std::clog << "Processing Mode: " << processingMode << std::endl;
    std::clog << "Max Utilization: " << maxUtilization << std::endl;
    std::clog << "Window Size 1: " << windowSize1 << std::endl;
    std::clog << "Window Size 2: " << windowSize2 << std::endl;

    resetProgressWindow();

    progressWindow.show();

    statusBar()->addPermanentWidget(statusProgressBar, 0);
    statusProgressBar->setValue(0);
    statusProgressBar->show();

    thread.setArgs(osnArgs);
    thread.start();
}

void MainWindow::enableRunButton()
{
    ui->runPushButton->setEnabled(true);
    progressWindow.updateProgressText("OpenSpaceNet is complete.");
    progressWindow.getUI().progressDisplay->append("Complete.");
    statusBar()->removeWidget(statusProgressBar);
    statusBar()->showMessage("Complete. " + featuresDetected);
}

void MainWindow::updateProgressBox(QString updateText)
{
    std::clog << updateText.toStdString() << std::endl;
    if(boost::contains(updateText.toStdString(), "features detected.")) {
        featuresDetected = updateText;
    }
    if(boost::contains(updateText.toStdString(), "0%")) {
        whichProgress++;
        progressCount = 0;
    }
    if(boost::contains(updateText.toStdString(), "*")) {
        progressCount += 2;
        if(whichProgress == 1) {
            progressWindow.updateProgressBar(progressCount);
            statusProgressBar->setValue(progressCount);
        }
        else if(whichProgress == 2) {
            progressWindow.updateProgressBarDetect(progressCount);
            statusProgressBar->setValue(progressCount);
        }
    }
    else if(!boost::contains(updateText.toStdString(), "0%") &&
            !boost::contains(updateText.toStdString(), "|----") &&
            !boost::contains(updateText.toStdString(), "found star")) {
        progressWindow.getUI().progressDisplay->append(updateText);
        if(!boost::contains(updateText.toStdString(), "\n")) {
            statusBar()->showMessage(updateText);
        }
    }
}

void MainWindow::on_modelpathLineEditLostFocus()
{
    std::string modelpath = ui->modelFileLineEdit->text().toStdString();
    bool exists = boost::filesystem::exists(modelpath);
    bool isDirectory = boost::filesystem::is_directory(modelpath);

    //For blank input (user erased all text, or hasn't entered any yet), set style to default,
    //but don't register the empty string as valid input
    if(modelpath == "") {
        ui->modelFileLineEdit->setStyleSheet("color: default");
        hasValidModel = false;
    }
    //Specified file either doesn't exist or is a directory instead of a file
    else if(!exists || isDirectory) {
        std::cerr << "Error: specified model file does not exist." << std::endl;
        ui->modelFileLineEdit->setStyleSheet("color: red;"
                                             "border: 1px solid red;");
        hasValidModel = false;
    }
    //Valid input
    else {
        ui->modelFileLineEdit->setStyleSheet("color: default");
        hasValidModel = true;
    }
}

void MainWindow::on_imagepathLineEditLostFocus()
{
    std::string imagepath = ui->localImageFileLineEdit->text().toStdString();
    bool exists = boost::filesystem::exists(imagepath);
    bool isDirectory = boost::filesystem::is_directory(imagepath);

    //For blank input (user erased all text, or hasn't entered any yet), set style to default,
    //but don't register the empty string as valid input
    if(imagepath == "")
    {
        ui->localImageFileLineEdit->setStyleSheet("color: default");
        hasValidLocalImagePath = false;
    }
    //Specified file either doesn't exist or is a directory instead of a file
    else if(!exists || isDirectory) {
        std::cerr << "Error: specified image file does not exist." << std::endl;
        ui->localImageFileLineEdit->setStyleSheet("color: red;"
                                                  "border: 1px solid red;");
        hasValidLocalImagePath = false;
    }
    //Valid image path
    else {
        ui->localImageFileLineEdit->setStyleSheet("color: default");
        hasValidLocalImagePath = true;
        //only geo-regeistered images are valid
        try {
            auto image = boost::make_unique<dg::deepcore::imagery::GdalImage>(imagepath);
            auto imageBbox = cv::Rect{ { 0, 0 }, image->size() };

            auto pixelToLL = dg::deepcore::geometry::TransformationChain { image->pixelToProj().clone(), image->spatialReference().toLatLon() };
            auto bbox = pixelToLL.transform(imageBbox);

            ui->bboxWestLineEdit->setText(QString::number(bbox.x));
            ui->bboxSouthLineEdit->setText(QString::number(bbox.y));
            ui->bboxEastLineEdit->setText(QString::number(bbox.x + bbox.width));
            ui->bboxNorthLineEdit->setText(QString::number(bbox.y + bbox.height));
            hasGeoRegLocalImage = true;
            std::clog << "Local Pixel is: " << (uint64_t)image->size().width*image->size().height << std::endl;
            if((uint64_t)image->size().width*image->size().height > std::numeric_limits<int>::max()){

                std::clog << "Local Pixel size is too large" << std::endl;
                std::clog << "Local Pixel is: " << (uint64_t)image->size().width*image->size().height << std::endl;
                hasValidBboxSize = false;
            }
            else{
                hasValidBboxSize = true;
            }

        }
        catch(dg::deepcore::Error e) {
            std::cerr << "Image \'" << imagepath << "\' is not geo-registered" << std::endl;
            ui->localImageFileLineEdit->setStyleSheet("color: red;"
                                                      "border: 1px solid red;");
            hasGeoRegLocalImage = false;
            QMessageBox::critical(
                this,
                tr("Error"),
                "Selected image is not geo-registered and cannot be processed");
        }
    }
}

void MainWindow::on_outputLocationLineEditLostFocus()
{
    std::string outputPath = ui->outputLocationLineEdit->text().toStdString();
    bool exists = boost::filesystem::exists(outputPath);
    bool isDirectory = boost::filesystem::is_directory(outputPath);

    if(outputPath == "") {
        ui->outputLocationLineEdit->setStyleSheet("color: red;"
                                                  "border: 1px solid red;");
        hasValidOutputPath = false;
    }
    //Specified file either doesn't exist or isn't a directory
    else if(!exists || !isDirectory) {
        std::cerr << "Error: specified output directory does not exist." << std::endl;
        ui->outputLocationLineEdit->setStyleSheet("color: red;"
                                                  "border: 1px solid red;");
        hasValidOutputPath = false;
    }
    else {
        ui->outputLocationLineEdit->setStyleSheet("color: default");
        hasValidOutputPath = true;
    }
}

void MainWindow::on_localImagePathLineEditCursorPositionChanged()
{
    ui->localImageFileLineEdit->setStyleSheet("color: default");
}

void MainWindow::on_modelpathLineEditCursorPositionChanged()
{
    ui->modelFileLineEdit->setStyleSheet("color: default");
}

void MainWindow::on_outputFilenameLineEditCursorPositionChanged()
{
    ui->outputFilenameLineEdit->setStyleSheet("color: default");
}

void MainWindow::on_outputPathLineEditCursorPositionChanged()
{
    ui->outputLocationLineEdit->setStyleSheet("color: default");
}

void MainWindow::on_mapIdLineEditCursorPositionChanged()
{
    ui->mapIdLineEdit->setStyleSheet("color: default");
}

void MainWindow::on_tokenLineEditCursorPositionChanged()
{
    ui->tokenLineEdit->setStyleSheet("color: default");
}

void MainWindow::on_passwordLineEditCursorPositionChanged()
{
    ui->passwordLineEdit->setStyleSheet("color: default");
}

void MainWindow::on_usernameLineEditCursorPositionChanged()
{
    ui->usernameLineEdit->setStyleSheet("color: default");
}

void MainWindow::resetProgressWindow()
{
    progressCount = 0;
    whichProgress = 0;
    progressWindow.updateProgressText("Running OpenSpaceNet...");
    progressWindow.getUI().progressDisplay->clear();
    progressWindow.updateProgressBar(0);
    progressWindow.updateProgressBarDetect(0);
}

void MainWindow::cancelThread()
{
    ui->runPushButton->setEnabled(true);
    progressWindow.close();
    qout.eraseString();
    sout.eraseString();

    if(!thread.isFinished()) {
        thread.quit();
    }
}

void MainWindow::on_closePushButton_clicked()
{
    if(!thread.isFinished()) {
        thread.quit();
    }
    qout.eraseString();
    sout.eraseString();

    progressWindow.close();

    exit(1);
}

void MainWindow::closeEvent (QCloseEvent *event)
{
    if(!thread.isFinished()) {
        thread.quit();
    }
    qout.eraseString();
    sout.eraseString();

    progressWindow.close();
    exit(1);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if(event->type() == QEvent::MouseButtonPress) {
        progressWindow.show();
    }
    return false;
}

void MainWindow::importConfig(QString configPath)
{
    po::variables_map config_vm;
    po::options_description desc;
    desc.add_options()
            ("action", po::value<std::string>())

            ("image", po::value<std::string>())
            ("service", po::value<std::string>())
            ("token", po::value<std::string>())
            ("credentials", po::value<std::string>())
            ("zoom", po::value<int>())
            ("num-downloads", po::value<int>())
            ("mapId", po::value<std::string>())

            ("model", po::value<std::string>())

            ("confidence", po::value<float>())
            ("step-size", po::value<float>())
            ("pyramid", po::value<bool>())
            ("nms", po::value<float>())

            ("bbox", po::value<std::string>())

            ("format", po::value<std::string>())
            ("type", po::value<std::string>())
            ("output-name", po::value<std::string>())
            ("output-location", po::value<std::string>())
            ("producer-info", po::value<bool>())

            ("cpu", po::value<bool>())
            ("max-utilization", po::value<float>())
            ("window-size", po::value<std::string>());

    std::ifstream configFile(configPath.toStdString());

    po::store(po::parse_config_file<char>(configFile, desc, true), config_vm);
    po::notify(config_vm);

    //action
    if(config_vm.find("action") != end(config_vm)) {
        std::string action = config_vm["action"].as<std::string>();
        int actionIndex;
        if(action == "detect"){
            actionIndex = ui->modeComboBox->findText("Detect");
            ui->modeComboBox->setCurrentIndex(actionIndex);
        }
        else if(action == "landcover") {
            actionIndex = ui->imageSourceComboBox->findText("Landcover");
            ui->modeComboBox->setCurrentIndex(actionIndex);
        }
    }

    //image
    if(config_vm.find("image") != end(config_vm)) {
        int sourceIndex = ui->imageSourceComboBox->findText("Local Image File");
        ui->imageSourceComboBox->setCurrentIndex(sourceIndex);

        ui->localImageFileLineEdit->setText(QString::fromStdString(config_vm["image"].as<std::string>()));
        //run validation to ensure that the image is still there
        on_imagepathLineEditLostFocus();
    }

    //service
    std::string service;
    if(config_vm.find("service") != end(config_vm)) {
        int sourceIndex;
        service = config_vm["service"].as<std::string>();
        if(service == "dgcs") {
            sourceIndex = ui->imageSourceComboBox->findText("DGCS");
            ui->imageSourceComboBox->setCurrentIndex(sourceIndex);
        }
        else if(service == "evwhs") {
            sourceIndex = ui->imageSourceComboBox->findText("EVWHS");
            ui->imageSourceComboBox->setCurrentIndex(sourceIndex);
        }
        else if(service == "maps-api") {
            sourceIndex = ui->imageSourceComboBox->findText("MapsAPI");
            ui->imageSourceComboBox->setCurrentIndex(sourceIndex);
        }
        ui->imageSourceComboBox->setCurrentIndex(sourceIndex);
    }

    //token
    QString token;
    if(config_vm.find("token") != end(config_vm)) {
        token = QString::fromStdString(config_vm["token"].as<std::string>());
        ui->tokenLineEdit->setText(token);
    }

    //credentials
    //only check for credentials if the service key is present in the config file, and doesn't have maps-api as its value
    if(config_vm.find("service") != end(config_vm) && service != "maps-api") {
        if(config_vm.find("credentials") != end(config_vm)) {
            std::string storedCredentials = config_vm["credentials"].as<std::string>();
            std::vector<std::string> credentials;
            boost::split(credentials, storedCredentials, boost::is_any_of(":"));
            ui->usernameLineEdit->setText(QString::fromStdString(credentials[0]));
            ui->passwordLineEdit->setText(QString::fromStdString(credentials[1]));
        }
    }

    //zoom
    if(config_vm.find("zoom") != end(config_vm)) {
        ui->zoomSpinBox->setValue(config_vm["zoom"].as<int>());
    }

    //downloads
    if(config_vm.find("num-downloads") != end(config_vm)) {
        ui->downloadsSpinBox->setValue(config_vm["num-downloads"].as<int>());
    }

    //map id
    if(service == "maps-api") {
        //don't show the default map id in the UI
        if (config_vm["mapId"].as<std::string>() != MAPSAPI_MAPID) {
            ui->mapIdLineEdit->setText(QString::fromStdString(config_vm["mapId"].as<std::string>()));
        }
    }

    //model
    if(config_vm.find("model") != end(config_vm)) {
        ui->modelFileLineEdit->setText(QString::fromStdString(config_vm["model"].as<std::string>()));
        //run validation to ensure that the model file is still there
        on_modelpathLineEditLostFocus();
    }

    //confidence
    if(config_vm.find("confidence") != end(config_vm)) {
        ui->confidenceSpinBox->setValue(config_vm["confidence"].as<float>());
    }

    //step size
    if(config_vm.find("step-size") != end(config_vm)) {
        ui->stepSizeSpinBox->setValue(config_vm["step-size"].as<float>());
    }

    //pyramid
    if(config_vm.find("pyramid") != end(config_vm)) {
        ui->pyramidCheckBox->setChecked(config_vm["pyramid"].as<bool>());
    }

    //non-maximum suppression
    if(config_vm.find("nms") != end(config_vm)) {
        float nmsValue = config_vm["nms"].as<float>();
        ui->nmsCheckBox->setChecked(nmsValue > 0);
        ui->nmsSpinBox->setValue(nmsValue);
    }

    //bounding box
    if(config_vm.find("bbox") != end(config_vm)) {
        std::string storedCoords = config_vm["bbox"].as<std::string>();
        std::vector<std::string> bbox;
        boost::split(bbox, storedCoords, boost::is_any_of(" "));
        ui->bboxWestLineEdit->setText(QString::fromStdString(bbox[0]));
        ui->bboxSouthLineEdit->setText(QString::fromStdString(bbox[1]));
        ui->bboxEastLineEdit->setText(QString::fromStdString(bbox[2]));
        ui->bboxNorthLineEdit->setText(QString::fromStdString(bbox[3]));
    }

    //output format
    if(config_vm.find("format") != end(config_vm)) {
        int formatIndex;
        std::string format = config_vm["format"].as<std::string>();
        if (format == "shp") {
            formatIndex = ui->outputFormatComboBox->findText("Shapefile");
        }
        else if(format == "geojson") {
            formatIndex = ui->outputFormatComboBox->findText("GeoJSON");
        }
        else if(format == "kml") {
            formatIndex = ui->outputFormatComboBox->findText("KML");
        }
        ui->outputFormatComboBox->setCurrentIndex(formatIndex);
    }

    //output type
    if(config_vm.find("type") != end(config_vm)) {
        int typeIndex;
        std::string type = config_vm["type"].as<std::string>();
        if (type == "polygon") {
            typeIndex = ui->geometryTypeComboBox->findText("Polygon");
        }
        else if (type == "point"){
            typeIndex = ui->geometryTypeComboBox->findText("Point");
        }
        ui->geometryTypeComboBox->setCurrentIndex(typeIndex);
    }

    //output name
    if(config_vm.find("output-name") != end(config_vm)) {
        std::string outputName = config_vm["output-name"].as<std::string>();
        ui->outputFilenameLineEdit->setText(QString::fromStdString(outputName));
    }

    //output location
    if(config_vm.find("output-location") != end(config_vm)) {
        std::string outputLocation = config_vm["output-location"].as<std::string>();
        ui->outputLocationLineEdit->setText(QString::fromStdString(outputLocation));
        //ensure that the path from the config file is still valid
        on_outputLocationLineEditLostFocus();
    }

    //output layer
    if(config_vm.find("output-layer") != end(config_vm)) {
        std::string outputLayer = config_vm["output-layer"].as<std::string>();
        ui->outputLayerLineEdit->setText(QString::fromStdString(outputLayer));
    }

    //producer info
    if(config_vm.find("producer-info") != end(config_vm)) {
        ui->producerInfoCheckBox->setChecked(config_vm["producer-info"].as<bool>());
    }

    //cpu
    if(config_vm.find("cpu") != end(config_vm) && config_vm["cpu"].as<bool>() == true) {
        int cpuIndex = ui->processingModeComboBox->findText("CPU");
        ui->processingModeComboBox->setCurrentIndex(cpuIndex);
    }

    //max utilization
    if(config_vm.find("max-utilization") != end(config_vm)) {
        ui->maxUtilizationSpinBox->setValue(config_vm["max-utilization"].as<float>());
    }

    //window size
    if(config_vm.find("window-size") != end(config_vm)) {
        std::string windowSize = config_vm["window-size"].as<std::string>();
        std::vector<std::string> dimensions;

        boost::split(dimensions, windowSize, boost::is_any_of(" "));

        ui->windowSizeSpinBox1->setValue(boost::lexical_cast<int>(dimensions[0]));
        ui->windowSizeSpinBox2->setValue(boost::lexical_cast<int>(dimensions[1]));
    }

    configFile.close();
}

bool MainWindow::validateUI(QString &error)
{
    //assume job is valid until a validation check fails
    bool validJob = true;

    hasValidOutputFilename = ui->outputFilenameLineEdit->text().trimmed() != "";

    //Local image specific validation
    if(ui->imageSourceComboBox->currentText() == "Local Image File") {
        if(!hasValidLocalImagePath){
            error += "Invalid local image filepath: \'" + ui->localImageFileLineEdit->text() + "\'\n\n";
            ui->localImageFileLineEdit->setStyleSheet("color: red;"
                                                      "border: 1px solid red;");
            validJob = false;
        }
        else {
            //The 'hasGeoRegLocalImage' flag is automatically updated every time the user selects a new image
            if (!hasGeoRegLocalImage){
                std::clog << "valid geo-registered image " << hasGeoRegLocalImage << std::endl;
                error += "Selected image is not geo-registered: \'" + ui->localImageFileLineEdit->text() + "\'\n\n";
                ui->localImageFileLineEdit->setStyleSheet("color: red;"
                                                          "border: 1px solid red;");
                validJob = false;
            }


            //only validate the bbox if the override checkbox is checked
            if(ui->bboxOverrideCheckBox->isChecked()) {
                bboxNorth = ui->bboxNorthLineEdit->text().toStdString();
                bboxSouth = ui->bboxSouthLineEdit->text().toStdString();
                bboxEast = ui->bboxEastLineEdit->text().toStdString();
                bboxWest = ui->bboxWestLineEdit->text().toStdString();

                unique_ptr<dg::deepcore::imagery::GdalImage> image;
                image = boost::make_unique<dg::deepcore::imagery::GdalImage>(ui->localImageFileLineEdit->text().toStdString());

                cv::Rect imageBbox;
                imageBbox = cv::Rect{ { 0, 0 }, image->size() };

                std::unique_ptr<cv::Rect2d> bbox_entered = nullptr;
                bbox_entered = boost::make_unique<cv::Rect2d>(cv::Point2d(stod(bboxWest), stod(bboxSouth)),
                                                              cv::Point2d(stod(bboxEast), stod(bboxNorth)));

                dg::deepcore::geometry::TransformationChain llToPixel {
                    image->spatialReference().fromLatLon(),
                    image->pixelToProj().inverse()
                };

                std::unique_ptr<dg::deepcore::geometry::Transformation> pixelToLL_ = llToPixel.inverse();

                auto bbox = llToPixel.transformToInt(*bbox_entered);
                auto intersect = imageBbox & (cv::Rect)bbox;
                try {
                    DG_CHECK(intersect.width && intersect.height, "Input image and the provided bounding box do not intersect");
                    hasValidBbox = true;
                }
                catch(dg::deepcore::Error e) {
                    std::clog << e.what() << std::endl;
                    error += "Input image and the provided bounding box do not intersect\n\n";
                    validJob = false;
                }

                if(hasValidBbox && bbox != intersect) {
                    auto llIntersect = pixelToLL_->transform(intersect);
                    std::cout << "Bounding box adjusted" << std::endl;
                }
            }
        }

    }

    //validate that bbox coordinates are in valid range
    if(ui->bboxOverrideCheckBox->isChecked() || ui->imageSourceComboBox->currentText() != "Local Image File") {
        double north = ui->bboxNorthLineEdit->text().toDouble();
        double south = ui->bboxSouthLineEdit->text().toDouble();
        double east = ui->bboxEastLineEdit->text().toDouble();
        double west = ui->bboxWestLineEdit->text().toDouble();

        if(abs(north) >= 90.0) {
            error += "North bounding box coordinate is invalid: must be in range (-90,90)\n\n";
            validJob = false;
        }
        if(abs(south) >= 90.0) {
            error += "South bounding box coordinate is invalid: must be in range (-90,90)\n\n";
            validJob = false;
        }
        if(abs(east) >= 180.0) {
            error += "East bounding box coordinate is invalid: must be in range (-180,180)\n\n";
            validJob = false;
        }
        if(abs(west) >= 180.0) {
            error += "West bounding box coordinate is invalid: must be in range (-180,180(])\n\n";
            validJob = false;
        }
    }

    //Image source agnostic validation
    if(!hasValidOutputFilename || !hasValidOutputPath || !hasValidModel) {
        validJob = false;
        std::clog << "valid model " << hasValidModel << std::endl;
        std::clog << "valid output " << hasValidOutputPath << std::endl;
        if(!hasValidModel) {
            error += "Invalid model filepath: \'" + ui->modelFileLineEdit->text() + "\'\n\n";
            ui->modelFileLineEdit->setStyleSheet("color: red;"
                                                 "border: 1px solid red;");
        }
        if(ui->outputFilenameLineEdit->text().trimmed() == "") {
            error += "Missing output filename\n\n";
            ui->outputFilenameLineEdit->setStyleSheet("color: red;"
                                                      "border: 1px solid red;");
        }
        if(!hasValidOutputPath) {
            error += "Invalid output directory path: \'" + ui->outputLocationLineEdit->text() + "\'\n\n";
            ui->outputLocationLineEdit->setStyleSheet("color: red;"
                                                      "border: 1px solid red;");
        }
    }


    statusBar()->showMessage("Checking credentials...");

    if(ui->imageSourceComboBox->currentText() != "Local Image File") {
        QString webservice = ui->imageSourceComboBox->currentText();
        bool wmts = true;
        hasValidBboxSize = true;
        std::string token = ui->tokenLineEdit->text().toStdString();
        std::string credentials = ui->usernameLineEdit->text().toStdString() + ":" + ui->passwordLineEdit->text().toStdString();
        std::string mapId;
        //Parse and set the map id
        if(ui->mapIdLineEdit->text() != "") {
            mapId = ui->mapIdLineEdit->text().toStdString();
        }
        else {
            mapId = MAPSAPI_MAPID;
        }
        if(webservice == "DGCS") {
            validationClient = boost::make_unique<dg::deepcore::imagery::DgcsClient>(token, credentials);
        }
        else if(webservice == "EVWHS") {
            validationClient = boost::make_unique<dg::deepcore::imagery::EvwhsClient>(token, credentials);
        }
        else if(webservice == "MapsAPI") {
            validationClient = boost::make_unique<dg::deepcore::imagery::MapBoxClient>(mapId, token);
            wmts = false;
        }

        try {
            validationClient->connect();
            statusBar()->showMessage("Validating Bounding Box...");
            if(wmts) {
                validationClient->setImageFormat("image/jpeg");
                validationClient->setLayer("DigitalGlobe:ImageryTileService");
                validationClient->setTileMatrixSet("EPSG:3857");
                validationClient->setTileMatrixId((format("EPSG:3857:%1d") % ui->zoomSpinBox->value()).str());
            }
            else {
                validationClient->setTileMatrixId(lexical_cast<string>(ui->zoomSpinBox->value()));
            }

            bboxNorth = ui->bboxNorthLineEdit->text().toStdString();
            bboxSouth = ui->bboxSouthLineEdit->text().toStdString();
            bboxEast = ui->bboxEastLineEdit->text().toStdString();
            bboxWest = ui->bboxWestLineEdit->text().toStdString();

            std::unique_ptr<cv::Rect2d> bbox = nullptr;
            bbox = boost::make_unique<cv::Rect2d>(cv::Point2d(stod(bboxWest), stod(bboxSouth)),
                                                  cv::Point2d(stod(bboxEast), stod(bboxNorth)));

            unique_ptr<dg::deepcore::geometry::Transformation> llToProj(validationClient->spatialReference().fromLatLon());
            auto projBbox = llToProj->transform(*bbox);
            geoImage.reset(validationClient->imageFromArea(projBbox));

            unique_ptr<dg::deepcore::geometry::Transformation> projToPixel(geoImage->pixelToProj().inverse());
            *bbox = projToPixel->transformToInt(projBbox);
            std::unique_ptr<dg::deepcore::geometry::Transformation> pixelToLL_ = dg::deepcore::geometry::TransformationChain { std::move(llToProj), std::move(projToPixel) }.inverse();

            auto msImage = dynamic_cast<dg::deepcore::imagery::MapServiceImage*>(geoImage.get());
            msImage->setMaxConnections(ui->downloadsSpinBox->value());

            std::clog << "Pixel Size width: " << geoImage->size().width << std::endl;
            std::clog << "Pixel Size height: " << geoImage->size().height << std::endl;
            std::clog << "Total size: " << (uint64_t)geoImage->size().width*geoImage->size().height << std::endl;

            if((uint64_t)geoImage->size().width*geoImage->size().height > std::numeric_limits<int>::max()) {
                std::clog << "Pixel size is too large" << std::endl;
                hasValidBboxSize = false;
            }
            else {
                hasValidBboxSize = true;
            }
        }
        catch(dg::deepcore::Error e) {
            std::string serverMessage(e.what());
            std::clog << serverMessage << std::endl;
            //check for invalid token message, first from DGCS, then from MapsAPI
            if (serverMessage.find("INVALID CONNECT ID") != std::string::npos ||
                serverMessage.find("Not Authorized - Invalid Token") != std::string::npos ||
                serverMessage.find("Not Authorized - No Token") != std::string::npos) {
                error += "Invalid web service token\n\n";
                ui->tokenLineEdit->setStyleSheet("color: red;"
                                                 "border: 1px solid red;");
            }
            //check for invalid username/password message
            else if(serverMessage.find("This request requires HTTP authentication") != std::string::npos) {
                error += "Invalid web service username and/or password\n\n";
                ui->passwordLineEdit->setStyleSheet("color: red;"
                                                    "border: 1px solid red;");
                ui->usernameLineEdit->setStyleSheet("color: red;"
                                                    "border: 1px solid red;");
            }
            //check for invalid map id
            else if(serverMessage.find("Not Found") != std::string::npos) {
                 error += "Invalid Map Id\n\n";
                 ui->mapIdLineEdit->setStyleSheet("color: red;"
                                                           "border: 1px solid red;");
            }
            else {
                error += "Unknown web service authentication error occurred\n\n";
            }
            validJob = false;
        }
    }
    statusBar()->clearMessage();

    if(hasValidBboxSize == false){
        if(ui->imageSourceComboBox->currentText() != "Local Image File") {
            error += "The entered bounding box is too large\n\n";
        }
        else if(ui->imageSourceComboBox->currentText() == "Local Image File" && hasValidLocalImagePath) {
            error += "The entered image is too large\n\n";
        }
        validJob = false;
    }

    return validJob;
}

void MainWindow::exportConfig(const QString &filepath)
{
    std::stringstream configContents;
    std::ofstream configFile;
    configFile.open(filepath.toStdString() + ".cfg");

    //Mode
    std::string key;
    QString index = ui->modeComboBox->currentText();
    if(index == "Detect") {
        key = "detect";
    }
    else {
        key = "landcover";
    }
    configContents << "action=" << key << "\n";

    //Image Source
    index = ui->imageSourceComboBox->currentText();
    if(index == "Local Image File") {
        configContents << "image=" << ui->localImageFileLineEdit->text().toStdString() << "\n";
    }
    else if(index == "DGCS") {
        configContents << "service=dgcs\n";
    }
    else if(index == "EVWHS") {
        configContents << "service=evwhs\n";
    }
    else if(index == "MapsAPI") {
        configContents << "service=maps-api\n";
    }

    //Credentials
    if(ui->usernameLineEdit->isEnabled() && ui->passwordLineEdit->isEnabled()) {
        configContents << "credentials=" << ui->usernameLineEdit->text().toStdString() << ":" << ui->passwordLineEdit->text().toStdString() << "\n";
    }

    //zoom level
    if(ui->zoomSpinBox->isEnabled()) {
        configContents << "zoom=" << ui->zoomSpinBox->value() << "\n";
    }

    //token
    if(ui->tokenLineEdit->isEnabled()) {
        configContents << "token=" << ui->tokenLineEdit->text().toStdString() << "\n";
    }

    //map id
    if(ui->mapIdLineEdit->isEnabled()) {
        configContents << "map-id=" << ui->mapIdLineEdit->text().toStdString() << "\n";
    }

    //model file
    configContents << "model=" << ui->modelFileLineEdit->text().toStdString() << "\n";

    //confidence
    configContents << "confidence=" << ui->confidenceSpinBox->value() << "\n";

    //step size
    configContents << "step-size=" << ui->stepSizeSpinBox->value() << "\n";

    //pyramid
    if (ui->pyramidCheckBox->isChecked()) {
        configContents << "pyramid=true\n";
    }

    //nms
    if(ui->nmsCheckBox->isChecked()) {
        configContents << "nms=" << ui->nmsSpinBox->value() << "\n";
    }

    //bounding box
    configContents << "bbox=" << ui->bboxWestLineEdit->text().toStdString() << " " <<
                                 ui->bboxSouthLineEdit->text().toStdString() << " " <<
                                 ui->bboxEastLineEdit->text().toStdString() << " " <<
                                 ui->bboxNorthLineEdit->text().toStdString() << "\n";

    //output format
    configContents << "format=";
    index = ui->outputFormatComboBox->currentText();
    if(index == "Shapefile") {
        configContents << "shp\n";
    }
    else if(index == "GeoJSON") {
        configContents << "geojson\n";
    }
    else if(index == "KML") {
        configContents << "kml\n";
    }

    //output filename
    configContents << "output-name=" << ui->outputFilenameLineEdit->text().toStdString() << "\n";

    //output location
    configContents << "output-location=" << ui->outputLocationLineEdit->text().toStdString() << "\n";

    //output layer
    configContents << "output-layer=" << ui->outputLayerLineEdit->text().toStdString() << "\n";

    //geometry type
    configContents << "type=";
    index = ui->geometryTypeComboBox->currentText();
    if(index == "Polygon") {
        configContents << "polygon\n";
    }
    else if(index == "Point") {
        configContents << "point\n";
    }

    //producer infor
    if(ui->producerInfoCheckBox->isChecked()) {
        configContents << "producer-info=true\n";
    }

    //cpu
    if(ui->processingModeComboBox->currentText() == "CPU") {
        configContents << "cpu=true\n";
    }

    //max utilization
    configContents << "max-utilization=" << ui->maxUtilizationSpinBox->value() << "\n";

    //window size
    configContents << "window-size=" << ui->windowSizeSpinBox1->value() << " " << ui->windowSizeSpinBox2->value() << "\n";

    //write the stringstream contents to the file
    configFile << configContents.rdbuf();

    configFile.close();
}
