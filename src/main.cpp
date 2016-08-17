/********************************************************************************
* Copyright 2016 DigitalGlobe, Inc.
* Author: Aleksey Vitebskiy
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
********************************************************************************/

#include "OpenSkyNetArgs.h"
#include <OpenSkyNetVersion.h>

using namespace dg::osn;
using namespace dg::deepcore;

using std::cout;
using std::exception;
using std::string;

static const string OSN_LOGO =
    "DigitalGlobe, Inc.\n"
        "   ___                   ____  _          _   _      _         \n"
        "  / _ \\ _ __   ___ _ __ / ___|| | ___   _| \\ | | ___| |_       \n"
        " | | | | '_ \\ / _ \\ '_ \\\\___ \\| |/ / | | |  \\| |/ _ \\ __|      \n"
        " | |_| | |_) |  __/ | | |___) |   <| |_| | |\\  |  __/ |_ _ _ _ \n"
        "  \\___/| .__/ \\___|_| |_|____/|_|\\_\\\\__, |_| \\_|\\___|\\__(_|_|_)\n"
        "       |_|                          |___/                                     \n\n"
        "Version: " OPENSKYNET_VERSION_STRING "\n\n";

int main (int argc, const char* const* argv)
{
    cout << OSN_LOGO;
    try {
        OpenSkyNetArgs osnArgs;
        osnArgs.parseArgsAndProcess(argc, argv);
    } catch (const Error& e) {
        DG_ERROR_LOG(OpenSkyNet, e);
        return 1;
    } catch (const exception &e) {
        OSN_LOG(error) << e.what();
        return 1;
    } catch (...) {
        OSN_LOG(error) << "Unknown error.";
        return 1;
    }
}
