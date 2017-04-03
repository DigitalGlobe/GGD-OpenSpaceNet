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
#include "OsnProcessingThread.h"

void OsnProcessingThread::run()
{
    DG_CHECK(osnArgs != nullptr, "osnArgs is a null pointer before calling process");
    dg::osn::OpenSpaceNet osn(*osnArgs);
    osn.setProgressDisplay(pd_);
    osn.process();
    emit processFinished();
}

void OsnProcessingThread::setArgs(const dg::osn::OpenSpaceNetArgs& osnArgsInput, boost::shared_ptr<OSNProgressDisplay> progressDisplay)
{
    osnArgs = &osnArgsInput;
    pd_ = progressDisplay;
}
