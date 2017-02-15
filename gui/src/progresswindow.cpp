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
#include "progresswindow.h"
#include "ui_progresswindow.h"

ProgressWindow::ProgressWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ProgressWindow)
{
    ui->setupUi(this);
}

void ProgressWindow::updateProgress(std::string updateMessage){

    ui->progressDisplay->append(QString::fromStdString(updateMessage));
}

void ProgressWindow::updateProgressBar(int progressNumber){
    ui->progressBar->setValue(progressNumber);
}

void ProgressWindow::updateProgressBarDetect(int progressNumber){
    ui->progressBar_2->setValue(progressNumber);
}

void ProgressWindow::updateProgressText(std::string progressText){
    ui->progressBarText->setText(QString::fromStdString(progressText));
}

ProgressWindow::~ProgressWindow()
{
    delete ui;
}

Ui::ProgressWindow ProgressWindow::getUI()
{
    return *ui;
}

void ProgressWindow::on_cancelPushButton_clicked()
{
    emit cancelPushed();
}
