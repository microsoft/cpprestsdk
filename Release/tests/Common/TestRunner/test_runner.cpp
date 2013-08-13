/***
* ==++==
*
* Copyright (c) Microsoft Corporation. All rights reserved. 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
**/
// TestRunner.cpp : Defines the entry point for the console application.
//

#include <map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <regex>

#ifdef WIN32
#include <Windows.h>
#include <conio.h>
#else
#include <boost/filesystem.hpp>
#include "unistd.h"
#endif

#include "../UnitTestpp/src/TestReporterStdout.h"
#include "../UnitTestpp/src/TimeHelpers.h"
#include "../UnitTestpp/src/GlobalSettings.h"

#include "test_module_loader.h"

static void print_help()
{
    std::cout << "Usage: testrunner.exe <test_binaries> [/list] [/listproperties] [/noignore] [/breakonerror]" <<std::endl;
    std::cout << "    [/name:<test_name>] [/select:@key=value] [/loop:<num_times>]" << std::endl;
    std::cout << std::endl;
    std::cout << "    /list              List all the names of the test_binaries and their" << std::endl;
    std::cout << "                       test cases." << std::endl;
    std::cout << std::endl;
    std::cout << "    /listproperties    List all the names of the test binaries, test cases, and" << std::endl;
    std::cout << "                       test properties." << std::endl;
    std::cout << std::endl;
    std::cout << "    /breakonerror      Break into the debugger when a failure is encountered." << std::endl;
    std::cout << std::endl;
    std::cout << "    /name:<test_name>  Run only test cases with matching name. Can contain the" << std::endl;
    std::cout << "                       wildcard '*' character." << std::endl;
    std::cout << std::endl;
    std::cout << "    /noignore          Include tests even if they have the 'Ignore' property set" << std::endl;
    std::cout << std::endl;
    std::cout << "    /select:@key=value Filter by the value of a particular test property." << std::endl;
    std::cout << std::endl;
    std::cout << "    /loop:<num_times>  Run test cases a specified number of times." << std::endl;

    std::cout << std::endl;
    std::cout << "Can also specify general global settings with the following:" << std::endl;
    std::cout << "    /global_key:global_value OR /global_key"  << std::endl << std::endl;
}

static std::string to_lower(const std::string &str)
{
    std::string lower;
    for(auto iter = str.begin(); iter != str.end(); ++iter)
    {
        lower.push_back((char)tolower(*iter));
    }
    return lower;
}

static std::vector<std::string> get_files_in_directory()
{
    std::vector<std::string> files;

#ifdef WIN32

    char exe_directory_buffer[MAX_PATH]; 
    GetModuleFileNameA(NULL, exe_directory_buffer, MAX_PATH); 
    std::string exe_directory = to_lower(exe_directory_buffer);
    auto location = exe_directory.rfind("testrunner");
    exe_directory.erase(location);
    exe_directory.append("*");
    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = FindFirstFileA(exe_directory.c_str(), &findFileData);
    if(hFind != INVALID_HANDLE_VALUE && !(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        files.push_back(findFileData.cFileName);
    }
    while(FindNextFileA(hFind, &findFileData) != 0)
    {
        if(!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            files.push_back(findFileData.cFileName);
        }
    }
    FindClose(hFind);

#else
    using namespace boost::filesystem;
    
    auto exe_directory = initial_path().string();
    for (auto it = directory_iterator(path(exe_directory)); it != directory_iterator(); ++it)
    {
        if (is_regular_file(*it))
        {
            files.push_back(it->path().filename().string());
        }
    }
#endif

    return files;
}

static std::string replace_wildcard_for_regex(const std::string &str)
{
    std::string result;
    for(auto iter = str.begin(); iter != str.end(); ++iter)
    {
        if(*iter == '*')
        {
            result.push_back('.');
        }
        result.push_back(*iter);
    }
    return result;
}

static std::vector<std::string> get_matching_binaries(const std::string &dllName)
{
    std::vector<std::string> allFiles = get_files_in_directory();
    
    // If starts with .\ remove it.
    std::string expandedDllName(dllName);
    if(expandedDllName.size() > 2 && expandedDllName[0] == '.' && expandedDllName[1] == '\\')
    {
        expandedDllName = expandedDllName.substr(2);
    }

    // Escape any '.'
    size_t oldLocation = 0;
    size_t location = expandedDllName.find(".", oldLocation);
    while(location != std::string::npos)
    {
        expandedDllName.insert(expandedDllName.find(".", oldLocation), "\\");
        oldLocation = location + 2;
        location = expandedDllName.find(".", oldLocation);
    }

    // Replace all '*' in dllName with '.*'
    expandedDllName = replace_wildcard_for_regex(expandedDllName);

    // Filter out any files that don't match.
    std::regex dllRegex(expandedDllName, std::regex_constants::icase);
    std::vector<std::string> matchingFiles;
    for(auto iter = allFiles.begin(); iter != allFiles.end(); ++iter)
    {
        if(std::regex_match(*iter, dllRegex))
        {
            matchingFiles.push_back(*iter);
        }
    }
    return matchingFiles;
}

static std::multimap<std::string, std::string> g_properties;
static std::vector<std::string> g_test_binaries;
static int g_individual_test_timeout = 60000 * 3;

static int parse_command_line(int argc, char **argv)
{
    if(argc < 2)
    {
        print_help();
        return -1;
    }

    for(int i = 1; i < argc; ++i)
    {
        std::string arg(argv[i]);
        arg = to_lower(arg);

        if(arg.compare("/?") == 0)
        {
            print_help();
            return -1;
        }
        else if(arg.find("/") == 0)
        {
            if(arg.find("/select:@") == 0)
            {
                std::string prop_asgn = std::string(argv[i]).substr(std::string("/select:@").size());
                auto eqsgn = prop_asgn.find('=');
                if (eqsgn < prop_asgn.size())
                {
                    auto key = prop_asgn.substr(0,eqsgn);
                    auto value = prop_asgn.substr(eqsgn+1);
                    g_properties.insert(std::make_pair(key, value));
                }
                else
                {
                    g_properties.insert(std::make_pair(prop_asgn, "*"));
                }
            }
            else if(arg.find(":") != std::string::npos)
            {
                const size_t index = arg.find(":");
                const std::string key = std::string(argv[i]).substr(1, index - 1);
                const std::string value = std::string(argv[i]).substr(index + 1);
                UnitTest::GlobalSettings::Add(key, value);
            }
            else
            {
                UnitTest::GlobalSettings::Add(arg.substr(1), "");
            }
        }
        else if(arg.find("/debug") == 0)
        {
            printf("Attach debugger now...\n");
            int temp;
            std::cin >> temp;
        }
        else
        {
            g_test_binaries.push_back(arg);
        }
    }

    return 0;
}

static bool matched_properties(const UnitTest::TestProperties& test_props)
{
    // TestRunner can only execute either desktop or winrt tests, but not both.
	// This starts with visual studio versions after VS 2012.
#if defined (_MSC_VER) && (_MSC_VER >= 1800)
#ifdef WINRT_TEST_RUNNER
    UnitTest::GlobalSettings::Add("winrt", "");
#elif defined DESKTOP_TEST_RUNNER
    UnitTest::GlobalSettings::Add("desktop", "");
#endif
#endif

    // The 'Require' property on a test case is special.
    // It requires a certain global setting to be fulfilled to execute.
    if(test_props.Has("Requires"))
    {
        const std::string requires = test_props.Get("Requires");
        std::vector<std::string> requirements;

        // Can be multiple requirements, a semi colon seperated list
        std::string::size_type pos = requires.find_first_of(';');
        std::string::size_type last_pos = 0;
        while(pos != std::string::npos)
        {
            requirements.push_back(requires.substr(last_pos, pos - last_pos));
            last_pos = pos + 1;
            pos = requires.find_first_of(';', last_pos);
        }
        requirements.push_back(requires.substr(last_pos));
        for(auto iter = requirements.begin(); iter != requirements.end(); ++iter)
        {
            if(!UnitTest::GlobalSettings::Has(to_lower(*iter)))
            {
                return false;
            }
        }
    }

    if (g_properties.size() == 0)
        return true;

    // All the properties specified at the cmd line act as a 'filter'.
    for (auto iter = g_properties.begin(); iter != g_properties.end(); ++iter)
    {
        auto name = iter->first;
        auto value = iter->second;
        if (test_props.Has(name) && (value == "*" || test_props[name] == value) )
        {
            return true;
        }
    }
    return false;
}

// Functions to list all the test cases and their properties.
static void handle_list_option(bool listProperties, const UnitTest::TestList &tests, const std::regex &nameRegex)
{
    UnitTest::Test *pTest = tests.GetFirst();
    while(pTest != nullptr)
    {
        std::string fullTestName = pTest->m_details.suiteName;
        fullTestName.append(":");
        fullTestName.append(pTest->m_details.testName);

        if(matched_properties(pTest->m_properties) && std::regex_match(fullTestName, nameRegex))
        {
            std::cout << "    " << fullTestName << std::endl;
            if(listProperties)
            {
                std::for_each(pTest->m_properties.begin(), pTest->m_properties.end(), [&](const std::pair<std::string, std::string> key_value)
                {
                    std::cout << "        " << key_value.first << ": " << key_value.second << std::endl;
                });
            }
        }
        pTest = pTest->m_nextTest;
    }
}
static void handle_list_option(bool listProperties, const UnitTest::TestList &tests)
{
    handle_list_option(listProperties, tests, std::regex(".*"));
}

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

bool IsTestIgnored(UnitTest::Test *pTest)
{
    if(pTest->m_properties.Has("Ignore")) return true;
#ifdef WIN32
    if(pTest->m_properties.Has("Ignore:Windows")) return true;
#else
if(pTest->m_properties.Has("Ignore:Linux")) return true;
#endif
    return false;
}

//
// These are to handle cases where an exception or assert occurs on a thread
// that isn't being waited on and the process exits. These shouldn't be happening,
// but could happen if we have a bug.
//
#ifdef WIN32

int CrtReportHandler(int reportType, char *message, int *returnValue)
{
    std::cerr << "In CRT Report Handler. ReportType:" << reportType << ", message:" << message << std::endl;
    
    // Cause break into debugger.
    *returnValue = 1;
    return TRUE;
}

#endif

int main(int argc, char* argv[])
{
#ifdef WIN32
    // The test runner built with WinRT support might be used on a pre Win8 machine.
    // Obviously in that case WinRT test cases can't run, but non WinRT ones should be
    // fine. So dynamically try to call RoInitialize/RoUninitialize.
    HMODULE hComBase = LoadLibrary(L"combase.dll");
    if(hComBase != nullptr)
    {
        typedef HRESULT (WINAPI *RoInit)(int);
        RoInit roInitFunc = (RoInit)GetProcAddress(hComBase, "RoInitialize");
        if(roInitFunc != nullptr)
        {
            roInitFunc(1); // RO_INIT_MULTITHREADED
        }
    }

    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, CrtReportHandler);
    struct console_restorer {
        CONSOLE_SCREEN_BUFFER_INFO m_originalConsoleInfo;
        console_restorer()
        {
            GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &m_originalConsoleInfo);
        }
        ~console_restorer()
        {
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), m_originalConsoleInfo.wAttributes);
        }
    }local;
#endif

    if(parse_command_line(argc, argv) != 0)
    {
        return -1;
    }

    if(g_test_binaries.empty())
    {
        std::cout << "Error: no test binaries were specified" << std::endl;
        print_help();
        return -1;
    }

    test_module_loader module_loader;
    int totalTestCount = 0, failedTestCount = 0;
    std::vector<std::string> failedTests;
    UnitTest::TestReporterStdout testReporter;
    bool breakOnError = false;
    if(UnitTest::GlobalSettings::Has("breakonerror"))
    {
        breakOnError = true;
    }

    // Determine if list or listProperties.
    bool listOption = false, listPropertiesOption = false;
    if(UnitTest::GlobalSettings::Has("list"))
    {
        listOption = true;
    }
    if(UnitTest::GlobalSettings::Has("listproperties"))
    {
        listOption = true;
        listPropertiesOption = true;
    }

    // Start timer.
    UnitTest::Timer timer;
    timer.Start();

    // Cycle through all the test binaries, load them, and perform the specified action.
    for(auto iter = g_test_binaries.begin(); iter != g_test_binaries.end(); ++iter)
    {
        std::vector<std::string> matchingBinaries = get_matching_binaries(*iter);
        if(matchingBinaries.empty())
        {
            std::cout << "File '" << *iter << "' not found." << std::endl;
        }
        for(auto binary = matchingBinaries.begin(); binary != matchingBinaries.end(); ++binary)
        {
            unsigned long error_code = module_loader.load(*binary);
            if(error_code != 0)
            {
                // Only omit an error if a wildcard wasn't used.
                if(iter->find('*') == std::string::npos)
                {
                    std::cout << "Error loading " << *binary << ": " << error_code << std::endl;
                    return error_code;
                }
                else
                {
                    continue;
                }
            }
            std::cout << "Loaded " << *binary << "..." << std::endl;
            UnitTest::TestList& tests = module_loader.get_test_list(*binary);

            // Skip if binary contains no tests.
            if(tests.IsEmpty())
            {
                std::cout << "Skipping " << *binary << " because it contains no test cases" << std::endl;
                continue;
            }

            // Check to see if running tests multiple times
            int numTimesToRun = 1;
            if(UnitTest::GlobalSettings::Has("loop"))
            {
                std::istringstream strstream(UnitTest::GlobalSettings::Get("loop"));
                strstream >> numTimesToRun;
            }

            const bool include_ignored_tests = UnitTest::GlobalSettings::Has("noignore");

            // Run test cases
            for(int i = 0; i < numTimesToRun; ++i)
            {
                if(!listOption)
                {
                    std::cout << "Running test cases in " << *binary << "..." << std::endl;
                    std::fflush(stdout);
                }

                UnitTest::TestRunner testRunner(testReporter, breakOnError);
                if(UnitTest::GlobalSettings::Has("name"))
                {
                    std::regex nameRegex(replace_wildcard_for_regex(UnitTest::GlobalSettings::Get("name")));

                    if(listOption)
                    {
                        handle_list_option(listPropertiesOption, tests, nameRegex);
                    }
                    else
                    {
                        testRunner.RunTestsIf(
                            tests, 
                            [&](UnitTest::Test *pTest) -> bool
                        {
                            // Combine suite and test name
                            std::string fullTestName = pTest->m_details.suiteName;
                            fullTestName.append(":");
                            fullTestName.append(pTest->m_details.testName);

                            if(IsTestIgnored(pTest) && !include_ignored_tests)
                                return false;
                            else
                                return matched_properties(pTest->m_properties) && 
                                std::regex_match(fullTestName, nameRegex);
                        },
                            g_individual_test_timeout);
                    }
                }
                else
                {
                    if(listOption)
                    {
                        handle_list_option(listPropertiesOption, tests);
                    }
                    else
                    {
                        testRunner.RunTestsIf(
                            tests,
                            [&](UnitTest::Test *pTest) -> bool
                        {
                            if(IsTestIgnored(pTest) && !include_ignored_tests)
                                return false;
                            else
                                return matched_properties(pTest->m_properties);
                        },
                            g_individual_test_timeout);
                    }
                }

                if(!listOption)
                {
                    totalTestCount += testRunner.GetTestResults()->GetTotalTestCount();
                    failedTestCount += testRunner.GetTestResults()->GetFailedTestCount();
                    if( totalTestCount == 0 )
                    {
                        std::cout << "No tests were run. Check the command line syntax (must be 'TestRunner.exe SomeTestDll.dll /name:suite_name:test_name')" << std::endl;
                    }
                    else
                    {
                        if(testRunner.GetTestResults()->GetFailedTestCount() > 0)
                        {
                            ChangeConsoleTextColorToRed();
                            const std::vector<std::string> & failed = testRunner.GetTestResults()->GetFailedTests();
                            std::for_each(failed.begin(), failed.end(), [](const std::string &failedTest)
                            {
                                std::cout << "**** " << failedTest << " FAILED ****" << std::endl << std::endl;
                                std::fflush(stdout);
                            });
                            ChangeConsoleTextColorToGrey();
                        }
                        else
                        {
                            ChangeConsoleTextColorToGreen();
                            std::cout << "All test cases in " << *binary << " PASSED" << std::endl << std::endl;
                            ChangeConsoleTextColorToGrey();
                        }
                        const std::vector<std::string> &newFailedTests = testRunner.GetTestResults()->GetFailedTests();
                        failedTests.insert(failedTests.end(), newFailedTests.begin(), newFailedTests.end());
                    }
                }
            }

            tests.Clear();
        }
    }

    if( totalTestCount > 0 )
    {
        // Print out all failed test cases at the end for easy viewing.
        const double elapsedTime = timer.GetTimeInMs();
        std::cout << "Finished running all tests. Took " << elapsedTime << "ms" << std::endl;
        if(failedTestCount > 0)
        {
            ChangeConsoleTextColorToRed();
            std::for_each(failedTests.begin(), failedTests.end(), [](const std::string &failedTest)
            {
                std::cout << "**** " << failedTest << " FAILED ****" << std::endl;
            });
        }
        else
        {
            ChangeConsoleTextColorToGreen();
            std::cout << "****SUCCESS all " << totalTestCount << " test cases PASSED****" << std::endl;
        }
        ChangeConsoleTextColorToGrey();
    }

#ifdef WIN32
    if(hComBase != nullptr)
    {
        typedef void (WINAPI *RoUnInit)();
        RoUnInit roUnInitFunc = (RoUnInit)GetProcAddress(hComBase, "RoUninitialize");
        if(roUnInitFunc != nullptr)
        {
            roUnInitFunc();
        }
        FreeLibrary(hComBase);
    }
#endif

    return failedTestCount;
}
