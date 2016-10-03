#ifndef PROCESSTHREAD_H
#define PROCESSTHREAD_H
#include <OpenSkyNetArgs.h>
#include <OpenSkyNet.h>
#include <QThread>

class ProcessThread : public QThread
{
    Q_OBJECT
public:
    void setArgs(dg::openskynet::OpenSkyNetArgs& osnArgsInput);
private:
    void run();
    dg::openskynet::OpenSkyNetArgs *osnArgs;
signals:
    void processFinished();
};

#endif // PROCESSTHREAD_H
