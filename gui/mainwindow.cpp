#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "progresswindow.h"
#include "ui_progresswindow.h"
#include <boost/filesystem/path.hpp>
#include <boost/core/null_deleter.hpp>
#include "qdebugstream.h"

using std::unique_ptr;
using boost::filesystem::path;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow){    
    ui->setupUi(this);
    setWindowFlags(windowFlags() ^ Qt::WindowMaximizeButtonHint);
    connect(&thread, SIGNAL(processFinished()), this, SLOT(enableRunButton()));
    connect(&qout, SIGNAL(updateProgressText(QString)), this, SLOT(updateProgressBox(QString)));
    setUpLogging();
}

MainWindow::~MainWindow(){
    delete ui;
}

void MainWindow::setUpLogging(){
    boost::shared_ptr<std::ostream> stringStream(&buffer_, boost::null_deleter());
    stringSink_ = dg::deepcore::log::addStreamSink(stringStream, dg::deepcore::level_t::info);

    stringStreamUI = stringStream;
    qout.setOptions(*stringStreamUI, progressWindow.getUI().progressDisplay);

}

void MainWindow::on_localImageFileBrowseButton_clicked(){
    QString directory = QFileDialog::getOpenFileName(this,tr("Select Image File"));
    ui->localImageFileLineEdit->setText(directory);
}

void MainWindow::on_modelFileBrowseButton_clicked(){
    QString directory = QFileDialog::getOpenFileName(this,tr("Select Model File"));
    ui->modelFileLineEdit->setText(directory);
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
    QString directory = QFileDialog::getExistingDirectory(this, tr("Select Output Location"));
    ui->outputLocationLineEdit->setText(directory);
}

void MainWindow::on_helpPushButton_clicked(){
    QMessageBox::information(
        this,
        tr("Help"),
        tr("Help is currently not supported."));
}

void MainWindow::on_runPushButton_clicked(){
    ui->runPushButton->setEnabled(false);
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

    osnArgs.bbox = unique_ptr<cv::Rect2d>();

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

    std::cout << "Mode: " << action << std::endl;

    std::cout << "Image Source: " << imageSource << std::endl;
    std::cout << "Local Image File Path: " << localImageFilePath << std::endl;

    std::cout << "Model File Path: " << modelFilePath << std::endl;

    std::cout << "Confidence: " << confidence << std::endl;
    std::cout << "Step Size: " << stepSize << std::endl;
    std::cout << "Pyramid: " << pyramid << std::endl;
    std::cout << "NMS: " << NMS << " Threshold: " << nmsThreshold << std::endl;

    std::cout << "BBOX North: " << bboxNorth << std::endl;
    std::cout << "BBOX South: " << bboxSouth << std::endl;
    std::cout << "BBOX East: " << bboxEast << std::endl;
    std::cout << "BBOX West: " << bboxWest << std::endl;

    std::cout << "Output Filename: " << outputFilename << std::endl;
    std::cout << "Output Filepath: " << outputFilepath << std::endl;
    std::cout << "Output Format: " << outputFormat << std::endl;
    std::cout << "Geometry Type: " << geometryType << std::endl;
    std::cout << "Output Location: " << outputLocation << std::endl;
    std::cout << "Output Layer: " << outputLayer << std::endl;
    std::cout << "Producer Info:  " << producerInfo << std::endl;

    std::cout << "Processing Mode: " << processingMode << std::endl;
    std::cout << "Max Utilization: " << maxUtilization << std::endl;
    std::cout << "Window Size 1: " << windowSize1 << std::endl;
    std::cout << "Window Size 2: " << windowSize2 << std::endl;

    progressWindow.setWindowTitle("OpenSkyNet Progress");
    progressWindow.show();
    progressWindow.updateProgressBar(33);
    progressWindow.updateProgressText("Running OpenSkyNet...");

    thread.setArgs(osnArgs);
    thread.start();
}

void MainWindow::enableRunButton() {
    ui->runPushButton->setEnabled(true);
    progressWindow.updateProgressText("OpenSkyNet is complete.");
    progressWindow.updateProgressBar(100);
}

void MainWindow::updateProgressBox(QString updateText){
    progressWindow.getUI().progressDisplay->append(updateText);
}

