#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "progresswindow.h"
#include "ui_progresswindow.h"
#include <boost/filesystem.hpp>
#include <boost/core/null_deleter.hpp>
#include "qdebugstream.h"
#include <boost/algorithm/string.hpp>
#include <QStandardPaths>

using std::unique_ptr;
using boost::filesystem::path;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow){    
    ui->setupUi(this);
    setWindowFlags(windowFlags() ^ Qt::WindowMaximizeButtonHint);
    connect(&thread, SIGNAL(processFinished()), this, SLOT(enableRunButton()));
    connect(&qout, SIGNAL(updateProgressText(QString)), this, SLOT(updateProgressBox(QString)));
    connect(&sout, SIGNAL(updateProgressText(QString)), this, SLOT(updateProgressBox(QString)));
    connect(&progressWindow, SIGNAL(cancelPushed()), this, SLOT(cancelThread()));
    setUpLogging();

    //bbox double validator
    QRegularExpression doubleRegExp("[+-]?\\d*\\.?\\d+");
    doubleValidator = std::unique_ptr<QRegularExpressionValidator>(new QRegularExpressionValidator(doubleRegExp, 0));

    ui->bboxWestLineEdit->setValidator(doubleValidator.get());
    ui->bboxSouthLineEdit->setValidator(doubleValidator.get());
    ui->bboxEastLineEdit->setValidator(doubleValidator.get());
    ui->bboxNorthLineEdit->setValidator(doubleValidator.get());

    //Connections that change the color of the filepath line edits
    QObject::connect(ui->localImageFileLineEdit, SIGNAL(editingFinished()), this, SLOT(on_imagepathLineEditLostFocus()));
    QObject::connect(ui->modelFileLineEdit, SIGNAL(editingFinished()), this, SLOT(on_modelpathLineEditLostFocus()));
    QObject::connect(ui->outputLocationLineEdit, SIGNAL(editingFinished()), this, SLOT(on_outputLocationLineEditLostFocus()));

    QObject::connect(ui->localImageFileLineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_localImagePathLineEditCursorPositionChanged()));
    QObject::connect(ui->modelFileLineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_modelpathLineEditCursorPositionChanged()));
    QObject::connect(ui->outputLocationLineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_outputPathLineEditCursorPositionChanged()));
}

MainWindow::~MainWindow(){
    delete ui;
}

void MainWindow::setUpLogging(){
    boost::shared_ptr<std::ostream> stringStream(&buffer_, boost::null_deleter());
    stringSink_ = dg::deepcore::log::addStreamSink(stringStream, dg::deepcore::level_t::info);

    stringStreamUI = stringStream;
    qout.setOptions(*stringStreamUI);
    sout.setOptions(stringStreamStdout);
}

void MainWindow::on_localImageFileBrowseButton_clicked(){
    QString path = QFileDialog::getOpenFileName(this,tr("Select Image File"));
    if(!path.isEmpty() && !path.isNull()){
        ui->localImageFileLineEdit->setText(path);
        //manually invoke the slot to check the new filepath
        on_imagepathLineEditLostFocus();
    }
}

void MainWindow::on_modelFileBrowseButton_clicked(){
    QString path = QFileDialog::getOpenFileName(
                this,
                tr("Select Model File"),
                QStandardPaths::locate(QStandardPaths::HomeLocation, QString(), QStandardPaths::LocateDirectory),
                tr("GBDXM files (*.gbdxm);;All files (*.*)") );
    //The directory path string will be empty if the user presses cancel in the QFileDialog
    if(!path.isEmpty() && !path.isNull()){
        ui->modelFileLineEdit->setText(path);
        //manually invoke the slot to check the new filepath
        on_modelpathLineEditLostFocus();
    }
}

void MainWindow::on_imageSourceComboBox_currentIndexChanged(const QString &source){
    if(source != "Local Image File"){
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

        //token
        ui->tokenLabel->setEnabled(true);
        ui->tokenLineEdit->setEnabled(true);

        //map id
        if(source == "MapsAPI"){
        	ui->mapIdLabel->setEnabled(true);
        	ui->mapIdLineEdit->setEnabled(true);

        	//user credentials
        	ui->usernameLabel->setEnabled(false);
        	ui->usernameLineEdit->setEnabled(false);
        	ui->passwordLabel->setEnabled(false);
        	ui->passwordLineEdit->setEnabled(false);
        }
        else{
        	ui->mapIdLabel->setEnabled(false);
        	ui->mapIdLineEdit->setEnabled(false);

        	//user credentials
        	ui->usernameLabel->setEnabled(true);
        	ui->usernameLineEdit->setEnabled(true);
        	ui->passwordLabel->setEnabled(true);
        	ui->passwordLineEdit->setEnabled(true);
        }

        //zoom
        ui->zoomLabel->setEnabled(true);
        ui->zoomSpinBox->setEnabled(true);

        //downloads
        ui->downloadsLabel->setEnabled(true);
        ui->downloadsSpinBox->setEnabled(true);       
    }
    else{
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
    }
}

void MainWindow::on_viewMetadataButton_clicked(){
    QMessageBox::information(
        this,
        tr("Metadata"),
        tr("Viewing Metadata is currently not supported."));
}

void MainWindow::on_nmsCheckBox_toggled(bool checked){
    if(checked){
        ui->nmsSpinBox->setEnabled(true);
    }
    else{
        ui->nmsSpinBox->setEnabled(false);
    }
}

void MainWindow::on_bboxOverrideCheckBox_toggled(bool checked){
    if(checked){
        ui->bboxNorthLineEdit->setEnabled(true);
        ui->bboxSouthLineEdit->setEnabled(true);
        ui->bboxEastLineEdit->setEnabled(true);
        ui->bboxWestLineEdit->setEnabled(true);
    }
    else{
        ui->bboxNorthLineEdit->setEnabled(false);
        ui->bboxSouthLineEdit->setEnabled(false);
        ui->bboxEastLineEdit->setEnabled(false);
        ui->bboxWestLineEdit->setEnabled(false);
    }
}

void MainWindow::on_outputLocationBrowseButton_clicked(){
    QString path = QFileDialog::getExistingDirectory(this, tr("Select Output Location"));
    if(!path.isEmpty() && !path.isNull()){
        ui->outputLocationLineEdit->setText(path);
        //manually invoke the slot to check the new directory path
        on_outputLocationLineEditLostFocus();
    }
}

void MainWindow::on_helpPushButton_clicked(){
    QMessageBox::information(
        this,
        tr("Help"),
        tr("Help is currently not supported."));
}

void MainWindow::on_runPushButton_clicked(){

    //Parse and set the Action
    action = ui->modeComboBox->currentText().toStdString();
    if(action == "Detect"){
        osnArgs.action = dg::openskynet::Action::DETECT;
    }
    else if(action == "Landcover"){
        osnArgs.action = dg::openskynet::Action::LANDCOVER;
    }
    else{
        osnArgs.action = dg::openskynet::Action::UNKNOWN;
    }

    //Parse and set the image source
    imageSource = ui->imageSourceComboBox->currentText().toStdString();
    if(imageSource == "Local Image File"){
        osnArgs.source = dg::openskynet::Source::LOCAL;
    }
    else if(imageSource == "DGCS"){
    	osnArgs.source = dg::openskynet::Source::DGCS;
    	hasValidLocalImagePath = true;
    }
    else if(imageSource == "EVWHS"){
    	osnArgs.source = dg::openskynet::Source::EVWHS;
        hasValidLocalImagePath = true;
    }
    else if(imageSource == "MapsAPI"){
    	osnArgs.source = dg::openskynet::Source::MAPS_API;
        hasValidLocalImagePath = true;
    }
    else{
    	osnArgs.source = dg::openskynet::Source::UNKNOWN;
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
    if(ui->mapIdLineEdit->text() != ""){
        osnArgs.mapId = ui->mapIdLineEdit->text().toStdString();
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
    if(pyramid == true){
        osnArgs.pyramid = true;
    }
    else{
        osnArgs.pyramid = false;
    }

    //Parse and set NMS
    NMS = ui->nmsCheckBox->isChecked();
    if(NMS == true){
        osnArgs.nms = true;
        nmsThreshold = ui->nmsSpinBox->value();
        osnArgs.overlap = (float)nmsThreshold;
    }
    else{
        osnArgs.nms = false;
    }

    //bbox parsing to be set up when web services are implemented
    bboxNorth = ui->bboxNorthLineEdit->text().toStdString();
    bboxSouth = ui->bboxSouthLineEdit->text().toStdString();
    bboxEast = ui->bboxEastLineEdit->text().toStdString();
    bboxWest = ui->bboxWestLineEdit->text().toStdString();

    if(imageSource != "Local Image File"){
        osnArgs.bbox = boost::make_unique<cv::Rect2d>(cv::Point2d(stod(bboxWest), stod(bboxSouth)),
                                                      cv::Point2d(stod(bboxEast), stod(bboxNorth)));
    }

    //Output filename parsing and setting
    outputFilename = ui->outputFilenameLineEdit->text().toStdString();

    //Output path setting
    outputLocation = ui->outputLocationLineEdit->text().toStdString();

    //Output format parsing and setting
    outputFormat = ui->outputFormatComboBox->currentText().toStdString();
    if(outputFormat == "Shapefile"){
        osnArgs.outputFormat = "shp";
        //Append file extension
        outputFilename += "." + osnArgs.outputFormat;
        outputFilepath = outputLocation + "/" + outputFilename;
        osnArgs.outputPath = outputFilepath;
    }
    else if(outputFormat == "Elastic Search"){
        osnArgs.outputFormat = "elasticsearch";
        osnArgs.outputPath = outputLocation;
    }
    else if(outputFormat == "GeoJSON"){
        osnArgs.outputFormat = "geojson";
        //Append file extension
        outputFilename += "." + osnArgs.outputFormat;
        outputFilepath = outputLocation + "/" + outputFilename;
        osnArgs.outputPath = outputFilepath;
    }
    else if(outputFormat == "KML"){
        osnArgs.outputFormat = "kml";
        //Append file extension
        outputFilename += "." + osnArgs.outputFormat;
        outputFilepath = outputLocation + "/" + outputFilename;
        osnArgs.outputPath = outputFilepath;
    }
    else if(outputFormat == "PostGIS"){
        osnArgs.outputFormat = "postgis";
        osnArgs.outputPath = outputLocation;
    }

    //Geometry type parsing and setting
    geometryType = ui->geometryTypeComboBox->currentText().toStdString();
    if(geometryType == "Polygon"){
        osnArgs.geometryType = dg::deepcore::vector::GeometryType::POLYGON;
    }
    else{
        osnArgs.geometryType = dg::deepcore::vector::GeometryType::POINT;
    }

    //layer name parsing and setting
    outputLayer = ui->outputLayerLineEdit->text().toStdString();
    osnArgs.layerName = path(osnArgs.outputPath).stem().filename().string();

    //producer info parsing
    producerInfo = ui->producerInfoCheckBox->isChecked();
    if(producerInfo == true){
        osnArgs.producerInfo = true;
    }
    else{
        osnArgs.producerInfo = false;
    }

    //Processing mode parsing and setting
    processingMode = ui->processingModeComboBox->currentText().toStdString();
    if(processingMode == "GPU"){
        osnArgs.useCpu = false;
    }
    else{
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

    //Validation checks
    bool validJob = true;

    hasValidOutputFilename = ui->outputFilenameLineEdit->text().trimmed() != "";
    QString error("Cannot run process:\n\n");

    //Local image specific validation
    if(osnArgs.source == dg::openskynet::Source::LOCAL && !hasValidLocalImagePath){
        validJob = false;
        std::clog << "valid local " << hasValidLocalImagePath << std::endl;

    }

    //Image source agnostic validation
    if(!hasValidOutputFilename || !hasValidOutputPath || !hasValidModel){
        validJob = false;
        std::clog << "valid model " << hasValidModel << std::endl;
        std::clog << "valid output " << hasValidOutputPath << std::endl;
        if(osnArgs.source == dg::openskynet::Source::LOCAL && !hasValidLocalImagePath){
            error += "Invalid local image filepath: \'" + ui->localImageFileLineEdit->text() + "\'\n\n";
        }
        if(!hasValidModel){
            error += "Invalid model filepath: \'" + ui->modelFileLineEdit->text() + "\'\n\n";
        }
        if(ui->outputFilenameLineEdit->text() == "") {
            error += "Missing output filename\n\n";
        }
        if(!hasValidOutputPath) {
            error += "Invalid output directory path: \'" + ui->outputLocationLineEdit->text() + "\'\n";
        }
    }

    if(!validJob){
        QMessageBox::critical(
            this,
            tr("Error"),
            error);
        return;
    }

    ui->runPushButton->setEnabled(false);

    resetProgressWindow();

    progressWindow.show();

    thread.setArgs(osnArgs);
    thread.start();
}

void MainWindow::enableRunButton(){
    ui->runPushButton->setEnabled(true);
    progressWindow.updateProgressText("OpenSkyNet is complete.");
}

void MainWindow::updateProgressBox(QString updateText){
    if(boost::contains(updateText.toStdString(), "0%")){
        whichProgress++;
        progressCount = 0;
    }
    if(boost::contains(updateText.toStdString(), "*")){
        progressCount += 2;
        if(whichProgress == 1) {
            progressWindow.updateProgressBar(progressCount);
        }
        else if(whichProgress == 2){
            progressWindow.updateProgressBarDetect(progressCount);
        }
    }
    else if(!boost::contains(updateText.toStdString(), "0%") && !boost::contains(updateText.toStdString(), "|----")) {
        progressWindow.getUI().progressDisplay->append(updateText);
    }
}

void MainWindow::on_modelpathLineEditLostFocus(){
    std::string modelpath = ui->modelFileLineEdit->text().toStdString();
    bool exists = boost::filesystem::exists(modelpath);
    bool isDirectory = boost::filesystem::is_directory(modelpath);

    //For blank input (user erased all text, or hasn't entered any yet), set style to default,
    //but don't register the empty string as valid input
    if (modelpath == "")
    {
        ui->modelFileLineEdit->setStyleSheet("color: default");
        hasValidModel = false;
    }
    //Specified file either doesn't exist or is a directory instead of a file
    else if(!exists || isDirectory) {
        std::cerr << "Error: specified model file does not exist." << std::endl;
        ui->modelFileLineEdit->setStyleSheet("color: red");
        hasValidModel = false;
    }
    //Valid input
    else {
        ui->modelFileLineEdit->setStyleSheet("color: default");
        hasValidModel = true;
    }
}

void MainWindow::on_imagepathLineEditLostFocus(){
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
        ui->localImageFileLineEdit->setStyleSheet("color: red");
        hasValidLocalImagePath = false;
    }
    //Valid input
    else {
        ui->localImageFileLineEdit->setStyleSheet("color: default");
        hasValidLocalImagePath = true;
    }
}

void MainWindow::on_outputLocationLineEditLostFocus(){
    std::string outputPath = ui->outputLocationLineEdit->text().toStdString();
    bool exists = boost::filesystem::exists(outputPath);
    bool isDirectory = boost::filesystem::is_directory(outputPath);

    //For blank input (user erased all text, or hasn't entered any yet), set style to default,
    //but don't register the empty string as valid input
    if(outputPath == "")
    {
        ui->outputLocationLineEdit->setStyleSheet("color: default");
        hasValidOutputPath = false;
    }
    //Specified file either doesn't exist or isn't a directory
    else if(!exists || !isDirectory){
        std::cerr << "Error: specified output directory does not exist." << std::endl;
        ui->outputLocationLineEdit->setStyleSheet("color: red");
        hasValidOutputPath = false;
    }
    else {
        ui->outputLocationLineEdit->setStyleSheet("color: default");
        hasValidOutputPath = true;
    }
}

void MainWindow::on_localImagePathLineEditCursorPositionChanged(){
    ui->localImageFileLineEdit->setStyleSheet("color: default");
}

void MainWindow::on_modelpathLineEditCursorPositionChanged(){
    ui->modelFileLineEdit->setStyleSheet("color: default");
}

void MainWindow::on_outputPathLineEditCursorPositionChanged(){
    ui->outputLocationLineEdit->setStyleSheet("color: default");
}

void MainWindow::resetProgressWindow(){
    progressCount = 0;
    whichProgress = 0;
    progressWindow.setWindowTitle("OpenSkyNet Progress");
    progressWindow.updateProgressText("Running OpenSkyNet...");
    progressWindow.getUI().progressDisplay->clear();
    progressWindow.updateProgressBar(0);
    progressWindow.updateProgressBarDetect(0);
    progressWindow.getUI().cancelPushButton->setVisible(false);

}

void MainWindow::cancelThread() {
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
    progressWindow.close();
    QApplication::quit();
}
