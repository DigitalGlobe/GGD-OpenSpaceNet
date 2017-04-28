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

#include "OSNProgressDisplay.h"
#include "utility/Error.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>

using std::chrono::milliseconds;
using std::cv_status;
using std::defer_lock;
using std::lock_guard;
using std::mutex;
using std::string;
using std::this_thread::sleep_for;
using std::thread;
using std::unique_lock;
using std::vector;

OSNProgressDisplay::OSNProgressDisplay(const dg::deepcore::ProgressCategories &categories, int intervalMs) :
        categories_(categories),
        progress_(categories.size(), 0.0F),
        oldProgress_(categories.size(), -1.0F),
        interval_(intervalMs)
{
    DG_CHECK(categories_.size(), "No categories specified");
}

OSNProgressDisplay::~OSNProgressDisplay()
{
    stop();
}

void OSNProgressDisplay::start()
{
    stop();

    thread_ = std::thread([this](){
        mutex stopMutex;
        unique_lock<mutex> stopLock(stopMutex);
        do {
            display();
        } while(stop_.wait_for(stopLock, milliseconds(interval_)) == cv_status::timeout);

        display();
    });
}

void OSNProgressDisplay::stop()
{
    if(thread_.joinable()) {
        stop_.notify_all();
        thread_.join();
    }
}


void OSNProgressDisplay::update(size_t categoryId, float progress)
{
    lock_guard<mutex> lock(mutex_);
    DG_CHECK(categoryId < categories_.size(), "Category ID out of range");
    progress_[categoryId] = progress;
}

void OSNProgressDisplay::display()
{
    vector<float> progress;
    {
        lock_guard<mutex> progressLock(mutex_);
        bool changed = false;
        for (size_t i = 0; i < categories_.size(); ++i) {
            if ((int) roundf(progress_[i] * 1000) != (int) roundf(oldProgress_[i] * 1000)) {
                changed = true;
                emit updateProgressStatus(QString::fromStdString(categories_[i].name), progress_[i] * 100);
                break;
            }
        }

        if (!changed) {
            return;
        }

        progress = oldProgress_ = progress_;
    }
}