/***
* This file is based on or incorporates material from the UnitTest++ r30 open source project.
* Microsoft is not the original author of this code but has modified it and is licensing the code under 
* the MIT License. Microsoft reserves all other rights not expressly granted under the MIT License, 
* whether by implication, estoppel or otherwise. 
*
* UnitTest++ r30 
*
* Copyright (c) 2006 Noel Llopis and Charles Nicholson
* Portions Copyright (c) Microsoft Corporation
*
* All Rights Reserved.
*
* MIT License
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
* and associated documentation files (the "Software"), to deal in the Software without restriction, 
* including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
* and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, 
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or 
* substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE 
* AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
* DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
***/

#include "stdafx.h"

// cstdio doesn't pull in namespace std on VC6, so we do it here.
#if defined(UNITTEST_WIN32) && (_MSC_VER == 1200)
	namespace std {}
#endif

namespace UnitTest {

static void ChangeConsoleTextColorToRed()
{
#ifdef WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x0004 | 0x0008);
#else
    std::cout << "\033[1;31m";
#endif
}

static void ChangeConsoleTextColorToGreen()
{
#ifdef WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x0002 | 0x0008);
#else
    std::cout << "\033[1;32m";
#endif
}

static void ChangeConsoleTextColorToGrey()
{
#ifdef WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);
#else
    std::cout << "\033[0m";
#endif
}

void TestReporterStdout::ReportFailure(TestDetails const& details, char const* failure)
{
#if defined(__APPLE__) || defined(__GNUG__)
    char const* const errorFormat = "%s:%d: error: Failure in %s: %s\n";
#else
    char const* const errorFormat = "%s(%d): error: Failure in %s: %s\n";
#endif

    ChangeConsoleTextColorToRed();
    printf(errorFormat, details.filename, details.lineNumber, details.testName, failure);
    ChangeConsoleTextColorToGrey();
}

void TestReporterStdout::ReportTestStart(TestDetails const& test)
{
    printf("Starting test case %s:%s...\n", test.suiteName, test.testName);
}

void TestReporterStdout::ReportTestFinish(TestDetails const& test, bool passed, float)
{
    if(passed)
    {
        printf("Test case %s:%s ", test.suiteName, test.testName);
        ChangeConsoleTextColorToGreen();
        printf("PASSED\n");
        ChangeConsoleTextColorToGrey();
    }
    else
    {
        ChangeConsoleTextColorToRed();
        printf("Test case %s:%s FAILED\n", test.suiteName, test.testName);
        ChangeConsoleTextColorToGrey();
    }
}

void TestReporterStdout::ReportSummary(int const, int const,
                                       int const, float)
{
}

}
