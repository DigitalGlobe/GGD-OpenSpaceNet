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
#include <OSNProcessingThread.h>
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

    void on_zoomSpinBoxCursorPositionChanged();

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
    OSNProcessingThread thread;
    ProgressWindow progressWindow;
    Ui::MainWindow *ui;

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
