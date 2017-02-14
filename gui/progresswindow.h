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
#ifndef PROGRESSWINDOW_H
#define PROGRESSWINDOW_H

#include <QWidget>

namespace Ui {
class ProgressWindow;
}

class ProgressWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ProgressWindow(QWidget *parent = 0);
    void updateProgress(std::string updateMessage);
    void updateProgressBar(int progressNumber);
    void updateProgressBarDetect(int progressNumber);
    void updateProgressText(std::string progressText);
    ~ProgressWindow();
    Ui::ProgressWindow getUI();

private slots:
    void on_cancelPushButton_clicked();

private:
    Ui::ProgressWindow *ui;
signals:
    void cancelPushed();
};

#endif // PROGRESSWINDOW_H
