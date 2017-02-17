/********************************************************************************
* Copyright 2017 DigitalGlobe, Inc.
* Author: Kevin McGee
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
********************************************************************************/
#ifndef OSNPROCESSINGTHREAD_H
#define OSNPROCESSINGTHREAD_H
#include <OpenSpaceNetArgs.h>
#include <OpenSpaceNet.h>
#include <QThread>

class OSNProcessingThread : public QThread
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

#endif // OSNPROCESSINGTHREAD_H
