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
#include "ProgressWindow.h"
#include "ui_progresswindow.h"

ProgressWindow::ProgressWindow(QWidget *parent) :
    QWidget(parent),
    ui_(new Ui::ProgressWindow)
{
    ui_->setupUi(this);
}

void ProgressWindow::updateProgress(const std::string& updateMessage){

    ui_->progressDisplay->append(QString::fromStdString(updateMessage));
}

void ProgressWindow::updateProgressBar(int progressNumber){
    ui_->progressBar->setValue(progressNumber);
}

void ProgressWindow::updateProgressBarDetect(int progressNumber){
    ui_->progressBar_2->setValue(progressNumber);
}

void ProgressWindow::updateProgressText(const std::string& progressText){
    ui_->progressBarText->setText(QString::fromStdString(progressText));
}

ProgressWindow::~ProgressWindow()
{
    delete ui_;
}

const Ui::ProgressWindow& ProgressWindow::ui() const
{
    return *ui_;
}

void ProgressWindow::on_cancelPushButton_clicked()
{
    emit cancelPushed();
}
