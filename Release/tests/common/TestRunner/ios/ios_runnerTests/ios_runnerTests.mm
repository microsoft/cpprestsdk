/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
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
