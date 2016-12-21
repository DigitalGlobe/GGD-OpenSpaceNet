#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <string>
#include <iostream>
#include <sstream>

#include <boost/make_unique.hpp>

#include <opencv2/core/types.hpp>

#include <QMainWindow>
#include <QDebug>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QValidator>
#include <QCloseEvent>
#include <QStatusBar>
#include <QProgressBar>

#include <OpenSpaceNetArgs.h>
#include <OpenSpaceNet.h>
#include <processthread.h>
#include <imagery/MapServiceClient.h>

#include "progresswindow.h"
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
    void on_loadConfigPushButton_clicked();

    void on_saveConfigPushButton_clicked();

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

    void on_modelpathLineEditLostFocus();

    void on_imagepathLineEditLostFocus();

    void on_mapIdLineEditCursorPositionChanged();

    void on_tokenLineEditCursorPositionChanged();

    void on_passwordLineEditCursorPositionChanged();

    void on_usernameLineEditCursorPositionChanged();

    void on_outputLocationLineEditLostFocus();

    void on_localImagePathLineEditCursorPositionChanged();

    void on_modelpathLineEditCursorPositionChanged();

    void on_outputFilenameLineEditCursorPositionChanged();

    void on_outputPathLineEditCursorPositionChanged();

    void on_closePushButton_clicked();

    void cancelThread();

private:
    void connectSignalsAndSlots();
    void setUpLogging();
    void initValidation();

    void resetProgressWindow();
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);

    bool validateUI(QString &error);
    void importConfig(QString configFile);
    void exportConfig(const QString &filepath);

    dg::osn::OpenSpaceNetArgs osnArgs;
    ProcessThread thread;
    ProgressWindow progressWindow;
    Ui::ProgressWindow *progressUi;
    Ui::MainWindow *ui;

    std::string action;

    std::string imageSource;
    std::string localImageFilePath;
    std::string modelFilePath;

    double confidence;
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

    boost::shared_ptr<::boost::log::sinks::sink> stringSink_;
    std::stringstream buffer_;
    boost::shared_ptr<std::ostream> stringStreamUI;
    std::ostream & stringStreamStdout = std::cout;
    QDebugStream qout;
    QDebugStream sout;

    int progressCount = 0;
    int whichProgress = 0;

    //web services client
    std::unique_ptr<dg::deepcore::imagery::MapServiceClient> validationClient;

    //bbox input validator
    std::unique_ptr<QRegularExpressionValidator> doubleValidator;

    //Valid input flags, OSN won't run unless these are all true
    bool hasValidModel = false;
    bool hasValidOutputFilename = false;
    bool hasValidOutputPath = false;
    bool hasValidLocalImagePath = false;
    bool hasGeoRegLocalImage = false;
    bool hasValidBbox = false;      //whether the bbox is a valid set of lat/lon coords
    bool hasValidBboxSize = false;  //whether the bbox is too large for OSN to handle

    //tracks the last-accessed directory for the file browser dialogs
    QString lastAccessedDirectory;

    QProgressBar *statusProgressBar;
    QString featuresDetected;

    cv::Size blockSize_;
    std::unique_ptr<dg::deepcore::imagery::GeoImage> geoImage;
};

#endif // MAINWINDOW_H
