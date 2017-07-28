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

#ifndef OSNPROGRESSDISPLAY_H
#define OSNPROGRESSDISPLAY_H

#include "utility/ProgressDisplay.h"
#include <condition_variable>
#include <mutex>
#include <thread>

#include <QObject>


class OSNProgressDisplay : public QObject, public dg::deepcore::ProgressDisplay
{
    Q_OBJECT
public:
    OSNProgressDisplay();
    OSNProgressDisplay(const dg::deepcore::ProgressCategories& categories, int intervalMs=500);
    ~OSNProgressDisplay();

    void start();
    void stop();
    void update(size_t categoryId, float progress);
    void setCategories(const dg::deepcore::ProgressCategories& categories);
    size_t addCategory(std::string name, std::string description);

private:
    void display();

    dg::deepcore::ProgressCategories categories_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable stop_;
    std::vector<float> progress_;
    std::vector<float> oldProgress_;
    int interval_;

signals:
    void updateProgressStatus(QString, float);
};


#endif //OSNPROGRESSDISPLAY_H
