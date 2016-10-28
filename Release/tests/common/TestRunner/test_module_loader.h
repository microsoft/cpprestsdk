/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
***/

#ifndef INCLUDED_TEST_MODULE_LOADER
#define INCLUDED_TEST_MODULE_LOADER

#include <string>
#include "unittestpp.h"

// Exported function from all test dlls.
typedef UnitTest::TestList & (__cdecl *GetTestsFunc)();

// Interface to implement on each platform to be be able to load/unload and call global functions.
class test_module
{
public:
    test_module(const std::string &dllName) : m_dllName(dllName), m_loaded(false) {}
    virtual ~test_module() {}

    unsigned long load()
    {
        if(!m_loaded)
        {
            m_loaded = true;
            unsigned long error_code = load_impl();
            if(error_code != 0)
            {
                m_loaded = false;
            }
            return error_code;
        }
        return 0;
    }

    virtual GetTestsFunc get_test_list() = 0;

    unsigned long unload()
    {
        if(m_loaded)
        {
            m_loaded = false;
            return unload_impl();
        }
        return 0;
    }

protected:

    virtual unsigned long load_impl() = 0;
    virtual unsigned long unload_impl() = 0;

    bool m_loaded;
    const std::string m_dllName;

private:
	test_module(const test_module &);
	test_module & operator=(const test_module &);
};

// Handles organizing all test binaries and using the correct module loader.
class test_module_loader
{
public:
    test_module_loader();
    ~test_module_loader();

    // Does't complain if module with same name is already loaded.
    unsigned long load(const std::string &dllName);

    // Module must have already been loaded.
    UnitTest::TestList& get_test_list(const std::string &dllName);

private:
    std::map<std::string, test_module *> m_modules;
};

#endif
