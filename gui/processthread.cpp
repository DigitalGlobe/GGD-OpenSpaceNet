#include "processthread.h"

void ProcessThread::run()
{
    dg::openskynet::OpenSkyNet osn(*osnArgs);
    osn.process();
    emit processFinished();
}

void ProcessThread::setArgs(dg::openskynet::OpenSkyNetArgs& osnArgsInput)
{
    osnArgs = &osnArgsInput;
}
