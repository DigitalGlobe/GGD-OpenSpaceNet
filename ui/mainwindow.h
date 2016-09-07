#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <QFileDialog>
#include <string>
#include <iostream>
#include <QTextStream>
#include <QMessageBox>

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

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
