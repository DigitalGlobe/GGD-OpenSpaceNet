#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{    
    ui->setupUi(this);
    setWindowFlags(windowFlags() ^ Qt::WindowMaximizeButtonHint);
}

MainWindow::~MainWindow()
{
    delete ui;
}
