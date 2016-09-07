#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow){    
    ui->setupUi(this);
    setWindowFlags(windowFlags() ^ Qt::WindowMaximizeButtonHint);
}

MainWindow::~MainWindow(){
    delete ui;
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
    std::string mode = ui->modeComboBox->currentText().toStdString();

    std::string imageSource = ui->imageSourceComboBox->currentText().toStdString();
    std::string localImageFilePath = ui->localImageFileLineEdit->text().toStdString();

    std::string modelFilePath = ui->modelFileLineEdit->text().toStdString();

    int confidence = ui->confidenceSpinBox->value();
    int stepSize = ui->stepSizeSpinBox->value();
    bool pyramid = ui->pyramidCheckBox->isChecked();
    bool NMS = ui->nmsCheckBox->isChecked();
    int nmsThreshold = ui->nmsSpinBox->value();

    std::string  bboxNorth = ui->bboxNorthLineEdit->text().toStdString();
    std::string  bboxSouth = ui->bboxSouthLineEdit->text().toStdString();
    std::string  bboxEast = ui->bboxEastLineEdit->text().toStdString();
    std::string  bboxWest = ui->bboxWestLineEdit->text().toStdString();

    std::string outputFormat = ui->outputFormatComboBox->currentText().toStdString();
    std::string geometryType = ui->geometryTypeComboBox->currentText().toStdString();
    std::string outputLocation = ui->outputLocationLineEdit->text().toStdString();
    std::string outputLayer = ui->outputLayerLineEdit->text().toStdString();
    bool producerInfo = ui->producerInfoCheckBox->isChecked();

    std::string processingMode = ui->processingModeComboBox->currentText().toStdString();
    int maxUtilization = ui->maxUtilizationSpinBox->value();
    int windowSize1 = ui->windowSizeSpinBox1->value();
    int windowSize2 = ui->windowSizeSpinBox2->value();

    std::cout << "Mode: " << mode << std::endl;

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

    std::cout << "Output Format: " << outputFormat << std::endl;
    std::cout << "Geometry Type: " << geometryType << std::endl;
    std::cout << "Output Location: " << outputLocation << std::endl;
    std::cout << "Output Layer: " << outputLayer << std::endl;
    std::cout << "Producer Info:  " << producerInfo << std::endl;

    std::cout << "Processing Mode: " << processingMode << std::endl;
    std::cout << "Max Utilization: " << maxUtilization << std::endl;
    std::cout << "Window Size 1: " << windowSize1 << std::endl;
    std::cout << "Window Size 2: " << windowSize2 << std::endl;
}
