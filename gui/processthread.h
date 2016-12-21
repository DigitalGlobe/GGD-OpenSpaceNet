#ifndef PROCESSTHREAD_H
#define PROCESSTHREAD_H
#include <OpenSpaceNetArgs.h>
#include <OpenSpaceNet.h>
#include <QThread>

class ProcessThread : public QThread
{
    Q_OBJECT
public:
    void setArgs(dg::osn::OpenSpaceNetArgs& osnArgsInput);
private:
    void run();
    dg::osn::OpenSpaceNetArgs *osnArgs;
signals:
    void processFinished();
};

#endif // PROCESSTHREAD_H
