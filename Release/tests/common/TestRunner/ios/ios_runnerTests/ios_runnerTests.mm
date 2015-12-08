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
*
* Creates an Xcode test to bride with our UnitTestpp tests. Can be used to run
* iOS tests in the simulator or a connected physically device.
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "unittestpp.h"
#include "src/TestReporterStdout.h"

@interface ios_runnerTests : XCTestCase

@end

@implementation ios_runnerTests

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)testCppRestSdk {
    UnitTest::TestReporterStdout testReporter;
    UnitTest::TestRunner testRunner(testReporter);
    
    UnitTest::TestList& tests = UnitTest::GetTestList();
    testRunner.RunTestsIf(tests,
                          [&](UnitTest::Test *pTest)
                          {
                              if (pTest->m_properties.Has("Ignore")) return false;
                              if (pTest->m_properties.Has("Ignore:Apple")) return false;
                              if (pTest->m_properties.Has("Ignore:IOS")) return false;
                              if (pTest->m_properties.Has("Requires")) return false;
                              return true;
                          },
                          60000 * 3);
    
    int totalTestCount = testRunner.GetTestResults()->GetTotalTestCount();
    int failedTestCount = testRunner.GetTestResults()->GetFailedTestCount();
    if(failedTestCount > 0)
    {
        printf("%i tests FAILED\n", failedTestCount);
        XCTAssert(false);
    }
    else
    {
        printf("All %i tests PASSED\n", totalTestCount);
        XCTAssert(YES, @"Pass");
    }
}

@end
