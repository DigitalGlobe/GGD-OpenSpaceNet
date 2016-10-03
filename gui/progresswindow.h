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
    ~ProgressWindow();

private:
    Ui::ProgressWindow *ui;
};

#endif // PROGRESSWINDOW_H
