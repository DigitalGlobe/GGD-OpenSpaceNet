/********************************************************************************
* Copyright 2017 DigitalGlobe, Inc.
* Author: Aleksey Vitebskiy
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

#include "ParseCLIArgs.h"
#include <OpenSpaceNetVersion.h>

using namespace dg::osn;
using namespace dg::deepcore;

using std::cout;
using std::exception;
using std::string;

static const string OSN_LOGO =
    "DigitalGlobe, Inc.\n"
        "   ____                    _____                      _   _      _          \n"
        "  / __ \\                  / ____|                    | \\ | |    | |         \n"
        " | |  | |_ __   ___ _ __ | (___  _ __   __ _  ___ ___|  \\| | ___| |_        \n"
        " | |  | | '_ \\ / _ \\ '_ \\ \\___ \\| '_ \\ / _` |/ __/ _ \\ . ` |/ _ \\ __|       \n"
        " | |__| | |_) |  __/ | | |____) | |_) | (_| | (_|  __/ |\\  |  __/ |_ _ _ _  \n"
        "  \\____/| .__/ \\___|_| |_|_____/| .__/ \\__,_|\\___\\___|_| \\_|\\___|\\__(_|_|_) \n"
        "        | |                     | |                                         \n"
        "        |_|                     |_|                                         \n\n"
        "Version: " OPENSPACENET_VERSION_STRING "\n\n";

int main (int argc, const char* const* argv)
{
    cout << OSN_LOGO;
    try {
        ParseCLIArgs osnCLIArgs;
        osnCLIArgs.parseArgsAndProcess(argc, argv);
    } catch (const Error& e) {
        DG_ERROR_LOG(OpenSpaceNet, e);
        return 1;
    } catch (const exception &e) {
        OSN_LOG(error) << e.what();
        return 1;
    } catch (...) {
        OSN_LOG(error) << "Unknown error.";
        return 1;
    }
}
