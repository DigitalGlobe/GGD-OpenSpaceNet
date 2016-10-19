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
