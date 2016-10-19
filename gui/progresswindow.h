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

private:
    Ui::ProgressWindow *ui;
};

#endif // PROGRESSWINDOW_H
