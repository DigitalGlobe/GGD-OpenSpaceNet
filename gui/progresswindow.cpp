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

ProgressWindow::~ProgressWindow()
{
    delete ui;
}
