#include "processthread.h"

void ProcessThread::run()
{
    dg::osn::OpenSpaceNet osn(*osnArgs);
    osn.process();
    emit processFinished();
}

void ProcessThread::setArgs(dg::osn::OpenSpaceNetArgs& osnArgsInput)
{
    osnArgs = &osnArgsInput;
}
