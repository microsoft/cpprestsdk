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
*/
#ifdef WIN32
#include <Windows.h>
#endif

#include "test_module_loader.h"
#include <iostream>

#ifdef WIN32

// Windows module
class windows_module : public test_module
{
public:
    windows_module(const std::string &dllName) : test_module(dllName), m_hModule(nullptr) {}

    GetTestsFunc get_test_list()
    {
        return (GetTestsFunc)GetProcAddress(m_hModule, "GetTestList");
    }

protected:

    virtual unsigned long load_impl()
    {
        // Make sure ends in .dll
        if(*(m_dllName.end() - 1) != 'l' 
            || *(m_dllName.end() - 2) != 'l' 
            || *(m_dllName.end() - 3) != 'd' 
            || *(m_dllName.end() - 4) != '.')
        {
            return (unsigned long)-1;
        }
        m_hModule = LoadLibraryA(m_dllName.c_str());
        if(m_hModule == nullptr)
        {
            return GetLastError();
        }
        return 0;
    }

    virtual unsigned long unload_impl()
    {
        if(!FreeLibrary(m_hModule))
        {
            return GetLastError();
        }
        return 0;
    }

    HMODULE m_hModule;

private:
	windows_module(const windows_module &);
	windows_module & operator=(const windows_module &);

};

#else
#include "dlfcn.h"
#include <boost/filesystem.hpp>

class linux_module : public test_module
{
public:
    linux_module(const std::string &soName) : test_module(soName), m_handle(nullptr) {}

    GetTestsFunc get_test_list()
    {
        auto ptr = dlsym(m_handle, "GetTestList");
        if (ptr == nullptr)
        {
            std::cerr << "couldn't find GetTestList" <<
#ifdef __APPLE__
                " " << dlerror() <<
#endif
                std::endl;
        }
        return (GetTestsFunc)ptr;
    }

protected:

    virtual unsigned long load_impl()
    {
#ifdef __APPLE__
        auto exe_directory = getcwd(nullptr, 0);
        auto path = std::string(exe_directory) + "/" + m_dllName;
        free(exe_directory);
#else
        auto path = boost::filesystem::initial_path().string() + "/" + m_dllName;
#endif

        m_handle = dlopen(path.c_str(), RTLD_LAZY|RTLD_GLOBAL);
        if (m_handle == nullptr)
        {
            std::cerr << std::string(dlerror()) << std::endl;
            return -1;
        }
        return 0;
    }

    virtual unsigned long unload_impl()
    {
        if (dlclose(m_handle) != 0)
        {
            std::cerr << std::string(dlerror()) << std::endl;
            return -1;
        }
        return 0;
    }

    void* m_handle;
};
#endif

test_module_loader::test_module_loader()
{
}

test_module_loader::~test_module_loader()
{
    for(auto iter = m_modules.begin(); iter != m_modules.end(); ++iter)
    {
        iter->second->unload();
        delete iter->second;
    }
}

unsigned long test_module_loader::load(const std::string &dllName)
{
    // Check if the module is already loaded.
    if(m_modules.find(dllName) != m_modules.end())
    {
        return 0;
    }

    test_module *pModule;
#ifdef WIN32
    pModule = new windows_module(dllName);
#else
    pModule = new linux_module(dllName);
#endif

    // Load dll.
    const unsigned long error_code = pModule->load();
    if(error_code != 0)
    {
        delete pModule;
        return error_code;
    }
    else
    {
        m_modules[dllName] = pModule;
    }
    return 0;
}

UnitTest::TestList g_list;

UnitTest::TestList& test_module_loader::get_test_list(const std::string &dllName)
{
    GetTestsFunc getTestsFunc = m_modules[dllName]->get_test_list();

    // If there is no GetTestList function then it must be a dll without any tests.
    // Simply return an empty TestList.
    if(getTestsFunc == nullptr)
    {
        return g_list;
    }

    return getTestsFunc();
}
