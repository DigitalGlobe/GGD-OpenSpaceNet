#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <QFileDialog>
#include <string>
#include <iostream>
#include <QTextStream>
#include <QMessageBox>
#include <OpenSkyNetArgs.h>
#include <OpenSkyNet.h>
#include <boost/make_unique.hpp>
#include <processthread.h>
#include "progresswindow.h"
#include <sstream>
#include "qdebugstream.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_runPushButton_clicked();

    void on_localImageFileBrowseButton_clicked();

    void on_modelFileBrowseButton_clicked();

    void on_outputLocationBrowseButton_clicked();

    void on_viewMetadataButton_clicked();

    void on_helpPushButton_clicked();

    void on_nmsCheckBox_toggled(bool checked);

    void on_bboxOverrideCheckBox_toggled(bool checked);

    void enableRunButton();

    void updateProgressBox(QString updateText);
    
    void on_imageSourceComboBox_currentIndexChanged(const QString &source);

private:
    dg::openskynet::OpenSkyNetArgs osnArgs;
    ProcessThread thread;
    ProgressWindow progressWindow;
    Ui::ProgressWindow *progressUi;
    Ui::MainWindow *ui;
    std::string action;

    std::string imageSource;
    std::string localImageFilePath;

    std::string modelFilePath;

    int confidence;
    int stepSize;
    bool pyramid;
    bool NMS;
    int nmsThreshold;

    std::string bboxNorth;
    std::string bboxSouth;
    std::string bboxEast;
    std::string bboxWest;

    std::string outputFilename; //Name of file, including extension
    std::string outputFilepath; //Absolute path of file, including extension
    std::string outputFormat;
    std::string geometryType;
    std::string outputLocation; //Path to output directory
    std::string outputLayer;
    bool producerInfo;

    std::string processingMode;
    int maxUtilization;
    int windowSize1;
    int windowSize2;

    void setUpLogging();

    boost::shared_ptr<::boost::log::sinks::sink> stringSink_;
    std::stringstream buffer_;
    boost::shared_ptr<std::ostream> stringStreamUI;
    QDebugStream qout;

};

#endif // MAINWINDOW_H
