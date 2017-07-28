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

#include <classification/Model.h>
#include <classification/Classification.h>
#include "OSNModelThread.h"

using std::unique_ptr;

void OSNModelThread::run()
{
    dg::deepcore::classification::init();
    dg::deepcore::classification::GbdxModelReader modelReader(modelPath_);

    unique_ptr<dg::deepcore::classification::ModelPackage> modelPackage(modelReader.readModel());
    auto model = dg::deepcore::classification::Model::create(*modelPackage, false, .50);

    windowSize_ = model->metadata().modelSize().height;
    windowStep_ = model->defaultStep().x;

    emit modelProcessFinished(windowSize_, windowStep_);
}

void OSNModelThread::setArgs(std::string modelPath)
{
    modelPath_ = modelPath;
}


